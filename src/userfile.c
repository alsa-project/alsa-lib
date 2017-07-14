/*
 *  Get full filename
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@perex.cz>
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
  
#include <config.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

/**
 * \brief Get the full file name
 * \param file The file name string to parse
 * \param result The pointer to store the resultant file name
 * \return 0 if successful, or a negative error code
 *
 * Parses the given file name with POSIX-Shell-like expansion and
 * stores the first matchine one.  The returned string is strdup'ed.
 */

#ifdef HAVE_WORDEXP
#include <wordexp.h>
int snd_user_file(const char *file, char **result)
{
	wordexp_t we;
	int err;

	assert(file && result);
	err = wordexp(file, &we, WRDE_NOCMD);
	switch (err) {
	case WRDE_NOSPACE:
		wordfree(&we);
		return -ENOMEM;
	case 0:
		if (we.we_wordc == 1)
			break;
		wordfree(&we);
		/* fall thru */
	default:
		return -EINVAL;
	}
	*result = strdup(we.we_wordv[0]);
	wordfree(&we);
	if (*result == NULL)
		return -ENOMEM;
	return 0;
}

#else /* !HAVE_WORDEX */

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>

int snd_user_file(const char *file, char **result)
{
	int err;
	size_t len;
	char *buf = NULL;

	assert(file && result);
	*result = NULL;

	/* expand ~/ if needed */
	if (file[0] == '~' && file[1] == '/') {
		const char *home = getenv("HOME");
		if (home == NULL) {
			struct passwd pwent, *p = NULL;
			uid_t id = getuid();
			size_t bufsize = 1024;

			buf = malloc(bufsize);
			if (buf == NULL)
				goto out;

			while ((err = getpwuid_r(id, &pwent, buf, bufsize, &p)) == ERANGE) {
				char *newbuf;
				bufsize += 1024;
				if (bufsize < 1024)
					break;
				newbuf = realloc(buf, bufsize);
				if (newbuf == NULL)
					goto out;
				buf = newbuf;
			}
			home = err ? "" : pwent.pw_dir;
		}
		len = strlen(home) + strlen(&file[2]) + 2;
		*result = malloc(len);
		if (*result)
			snprintf(*result, len, "%s/%s", home, &file[2]);
	} else {
		*result = strdup(file);
	}

out:
	if (buf)
		free(buf);

	if (*result == NULL)
		return -ENOMEM;
	return 0;
}

#endif /* HAVE_WORDEXP */
