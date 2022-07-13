/*
 *  Exec an external program
 *  Copyright (C) 2021 Jaroslav Kysela
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Support for the verb/device/modifier core logic and API,
 *  command line tool and file parser was kindly sponsored by
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices,
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 *
 *  Copyright (C) 2021 Red Hat Inc.
 *  Authors: Jaroslav Kysela <perex@perex.cz>
 */

#include "ucm_local.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <limits.h>
#include <dirent.h>

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__DragonFly__)
#include <signal.h>
#if defined(__DragonFly__)
#define environ NULL /* XXX */
#else
extern char **environ;
#endif
#endif

static pthread_mutex_t fork_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Search PATH for executable
 */
static int find_exec(const char *name, char *out, size_t len)
{
	int ret = 0;
	char bin[PATH_MAX];
	char *path, *tmp, *tmp2 = NULL;
	DIR *dir;
	struct dirent64 *de;
	struct stat64 st;
	if (name[0] == '/') {
		if (lstat64(name, &st))
			return 0;
		if (!S_ISREG(st.st_mode) || !(st.st_mode & S_IEXEC))
			return 0;
		snd_strlcpy(out, name, len);
		return 1;
	}
	if (!(tmp = getenv("PATH")))
		return 0;
	path = alloca(strlen(tmp) + 1);
	if (!path)
		return 0;
	strcpy(path, tmp);
	tmp = strtok_r(path, ":", &tmp2);
	while (tmp && !ret) {
		if ((dir = opendir(tmp))) {
			while ((de = readdir64(dir))) {
				if (strstr(de->d_name, name) != de->d_name)
					continue;
				snprintf(bin, sizeof(bin), "%s/%s", tmp,
					 de->d_name);
				if (lstat64(bin, &st))
					continue;
				if (!S_ISREG(st.st_mode)
				    || !(st.st_mode & S_IEXEC))
					continue;
				snd_strlcpy(out, bin, len);
				closedir(dir);
				return 1;
			}
			closedir(dir);
		}
		tmp = strtok_r(NULL, ":", &tmp2);
	}
	return ret;
}

static void free_args(char **argv)
{
	char **a;

	for (a = argv; *a; a++)
		free(*a);
	free(argv);
}

static int parse_args(char ***argv, int argc, const char *cmd)
{
	char *s, *f;
	int i = 0, l, eow;

	if (!argv || !cmd)
		return -1;

	s = alloca(strlen(cmd) + 1);
	if (!s)
		return -1;
	strcpy(s, cmd);
	*argv = calloc(argc, sizeof(char *));

	while (*s && i < argc - 1) {
		while (*s == ' ')
			s++;
		f = s;
		eow = 0;
		while (*s) {
			if (*s == '\\') {
				l = *(s + 1);
				if (l == 'b')
					l = '\b';
				else if (l == 'f')
					l = '\f';
				else if (l == 'n')
					l = '\n';
				else if (l == 'r')
					l = '\r';
				else if (l == 't')
					l = '\t';
				else
					l = 0;
				if (l) {
					*s++ = l;
					memmove(s, s + 1, strlen(s));
				} else {
					memmove(s, s + 1, strlen(s));
					if (*s)
						s++;
				}
			} else if (eow) {
				if (*s == eow) {
					memmove(s, s + 1, strlen(s));
					eow = 0;
				} else {
					s++;
				}
			} else if (*s == '\'' || *s == '"') {
				eow = *s;
				memmove(s, s + 1, strlen(s));
			} else if (*s == ' ') {
				break;
			} else {
				s++;
			}
		}
		if (f != s) {
			if (*s) {
				*(char *)s = '\0';
				s++;
			}
			(*argv)[i] = strdup(f);
			if ((*argv)[i] == NULL) {
				free_args(*argv);
				return -ENOMEM;
			}
			i++;
		}
	}
	(*argv)[i] = NULL;
	return 0;
}

/*
 * execute a binary file
 *
 */
int uc_mgr_exec(const char *prog)
{
	pid_t p, f, maxfd;
	int err = 0, status;
	char bin[PATH_MAX];
	struct sigaction sa;
	struct sigaction intr, quit;
	sigset_t omask;
	char **argv;

	if (parse_args(&argv, 32, prog))
		return -EINVAL;

	prog = argv[0];
	if (prog == NULL) {
		err = -EINVAL;
		goto __error;
	}
	if (prog[0] != '/' && prog[0] != '.') {
		if (!find_exec(argv[0], bin, sizeof(bin))) {
			err = -ENOEXEC;
			goto __error;
		}
		prog = bin;
	}

	maxfd = sysconf(_SC_OPEN_MAX);

	/*
	 * block SIGCHLD signal
	 * ignore SIGINT and SIGQUIT in parent
	 */

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGCHLD);

	pthread_mutex_lock(&fork_lock);

	sigprocmask(SIG_BLOCK, &sa.sa_mask, &omask);

	sigaction(SIGINT, &sa, &intr);
	sigaction(SIGQUIT, &sa, &quit);

	p = fork();

	if (p == -1) {
		err = -errno;
		pthread_mutex_unlock(&fork_lock);
		uc_error("Unable to fork() for \"%s\" -- %s", prog,
			 strerror(errno));
		goto __error;
	}

	if (p == 0) {
		f = open("/dev/null", O_RDWR);
		if (f == -1) {
			uc_error("pid %d cannot open /dev/null for redirect %s -- %s",
				 getpid(), prog, strerror(errno));
			exit(1);
		}

		close(0);
		close(1);
		close(2);

		dup2(f, 0);
		dup2(f, 1);
		dup2(f, 2);

		close(f);

		for (f = 3; f < maxfd; f++)
			close(f);

		/* install default handlers for the forked process */
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);

		execve(prog, argv, environ);
		exit(1);
	}

	sigaction(SIGINT, &intr, NULL);
	sigaction(SIGQUIT, &quit, NULL);
	sigprocmask(SIG_SETMASK, &omask, NULL);

	pthread_mutex_unlock(&fork_lock);

	/* make the spawned process a session leader so killing the
	   process group recursively kills any child process that
	   might have been spawned */
	setpgid(p, p);

	while (1) {
		f = waitpid(p, &status, 0);
		if (f == -1) {
			if (errno == EAGAIN)
				continue;
			err = -errno;
			goto __error;
		}
		if (WIFSIGNALED(status)) {
			err = -EINTR;
			break;
		}
		if (WIFEXITED(status)) {
			err = WEXITSTATUS(status);
			break;
		}
	}

 __error:
	free_args(argv);
	return err;
}
