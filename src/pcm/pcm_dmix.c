/**
 * \file pcm/pcm_dmix.c
 * \ingroup PCM_Plugins
 * \brief PCM Direct Stream Mixing (dmix) Plugin Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2002
 */
/*
 *  PCM - Direct Stream Mixing
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *
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
  
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "pcm_local.h"
#include "../control/control_local.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_dmix = "";
#endif

/*
 *
 */

struct slave_params {
	snd_pcm_format_t format;
	unsigned int rate;
	unsigned int channels;
	unsigned int period_time;
	unsigned int buffer_time;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size;
	unsigned int periods;
};

typedef struct {
	int owners;		/* count of all owners */
	char socket_name[256];	/* name of communication socket */
	snd_pcm_type_t type;	/* PCM type (currently only hw) */
	snd_pcm_hw_params_t hw_params;
	snd_pcm_sw_params_t sw_params;
} snd_pcm_dmix_share_t;

typedef struct {
	key_t ipc_key;	/* IPC key for semaphore and memory */
	int semid;	/* IPC global semaphore identification */
	int semlock;	/* locked with our process? */
	int shmid;	/* IPC global shared memory identification */
	snd_pcm_dmix_share_t *shmptr;	/* pointer to shared memory area */
	snd_pcm_t *spcm; /* slave PCM handle */
	snd_pcm_uframes_t appl_ptr;
	snd_pcm_uframes_t hw_ptr;
	int server, client;
	int comm_fd;	/* communication file descriptor (socket) */
	int hw_fd;	/* hardware file descriptor */
	int poll_fd;
	int server_fd;
	pid_t server_pid;
	snd_timer_t *timer; /* timer used as poll_fd */
} snd_pcm_dmix_t;

/*
 *  global semaphore and shared memory area
 */

#if 0
struct sembuf {
	unsigned short sem_num;
	short sem_op;
	short sem_flg;
};
#endif

static int semaphore_create_or_connect(snd_pcm_dmix_t *dmix)
{
	dmix->semid = semget(dmix->ipc_key, 1, O_CREAT | 0666);
	if (dmix->semid < 0)
		return -errno;
	return 0;
}

static int semaphore_discard(snd_pcm_dmix_t *dmix)
{
	if (dmix->semid < 0)
		return -EINVAL;
	if (semctl(dmix->semid, 0, IPC_RMID, NULL) < 0)
		return -errno;
	dmix->semlock = 0;
	dmix->semid = -1;
	return 0;
}

static int semaphore_down(snd_pcm_dmix_t *dmix)
{
	static struct sembuf op[2] = { { 0, 0, SEM_UNDO }, { 0, 1, SEM_UNDO | IPC_NOWAIT } };
	assert(dmix->semid >= 0);
	if (semop(dmix->semid, op, 1) < 0)
		return -errno;
	dmix->semlock = 1;
	return 0;
}

static int semaphore_up(snd_pcm_dmix_t *dmix)
{
	static struct sembuf op = { 0, -1, SEM_UNDO | IPC_NOWAIT };
	assert(dmix->semid >= 0);
	if (semop(dmix->semid, &op, 1) < 0)
		return -errno;
	dmix->semlock = 0;
	return 0;
}

/*
 *  global shared memory area 
 */

static int shm_create_or_connect(snd_pcm_dmix_t *dmix)
{
	static int shm_discard(snd_pcm_dmix_t *dmix);
	struct shmid_ds buf;
	int ret = 0;
	
	dmix->shmid = shmget(dmix->ipc_key, sizeof(snd_pcm_dmix_share_t), O_CREAT | 0666);
	if (dmix->shmid < 0)
		return -errno;
	dmix->shmptr = shmat(dmix->shmid, 0, 0);
	if (dmix->shmptr == (void *) -1) {
		shm_discard(dmix);
		return -errno;
	}
	if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0) {
		shm_discard(dmix);
		return -errno;
	}
	if (buf.shm_nattch == 1) {	/* we're the first user, clear the segment */
		memset(dmix->shmptr, 0, sizeof(snd_pcm_dmix_share_t));
		ret = 1;
	}
	return 0;
}

static int shm_discard(snd_pcm_dmix_t *dmix)
{
	struct shmid_ds buf;
	int ret = 0;

	if (dmix->shmid < 0)
		return -EINVAL;
	if (dmix->shmptr != (void *) -1 && shmdt(dmix->shmptr) < 0)
		return -errno;
	dmix->shmptr = (void *) -1;
	if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0) {
		shm_discard(dmix);
		return -errno;
	}
	if (buf.shm_nattch == 0) {	/* we're the last user, destroy the segment */
		if (shmctl(dmix->shmid, IPC_RMID, NULL) < 0)
			return -errno;
		ret = 1;
	}
	dmix->shmid = -1;
	return ret;
}

/*
 *  server side
 */

static int get_tmp_name(char *filename, size_t size)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	snprintf(filename, size, "/tmp/alsa-dmix-%i-%li-%li", getpid(), tv.tv_sec, tv.tv_usec);
	filename[size-1] = '\0';
	return 0;
}

static int make_local_socket(const char *filename, int server)
{
	size_t l = strlen(filename);
	size_t size = offsetof(struct sockaddr_un, sun_path) + l;
	struct sockaddr_un *addr = alloca(size);
	int sock;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0) {
		int result = -errno;
		SYSERR("socket failed");
		return result;
	}

	if (server)
		unlink(filename);
	addr->sun_family = AF_LOCAL;
	memcpy(addr->sun_path, filename, l);
	
	if (server) {
		if (bind(sock, (struct sockaddr *) addr, size) < 0) {
			int result = -errno;
			SYSERR("bind failed");
			return result;
		}
	} else {
		if (connect(sock, (struct sockaddr *) addr, size) < 0) {
			SYSERR("connect failed");
			return -errno;
		}
	}
	return sock;
}

static int send_fd(int sock, void *data, size_t len, int fd)
{
	int ret;
	size_t cmsg_len = CMSG_LEN(sizeof(int));
	struct cmsghdr *cmsg = alloca(cmsg_len);
	int *fds = (int *) CMSG_DATA(cmsg);
	struct msghdr msghdr;
	struct iovec vec;

	vec.iov_base = (void *)&data;
	vec.iov_len = len;

	cmsg->cmsg_len = cmsg_len;
	cmsg->cmsg_level = SOL_SOCKET;
	cmsg->cmsg_type = SCM_RIGHTS;
	*fds = fd;

	msghdr.msg_name = NULL;
	msghdr.msg_namelen = 0;
	msghdr.msg_iov = &vec;
 	msghdr.msg_iovlen = 1;
	msghdr.msg_control = cmsg;
	msghdr.msg_controllen = cmsg_len;
	msghdr.msg_flags = 0;

	ret = sendmsg(sock, &msghdr, 0 );
	if (ret < 0) {
		SYSERR("sendmsg failed");
		return -errno;
	}
	return ret;
}

static void server_job(snd_pcm_dmix_t *dmix)
{
	int ret, sck, i;
	int max = 128, current = 0;
	struct pollfd pfds[max + 1];
	
	/* close all files to free resources */
	i = sysconf(_SC_OPEN_MAX);
	while (--i >= 0) {
		if (i != dmix->server_fd && i != dmix->hw_fd)
			close(i);
	}
	
	/* detach from parent */
	setsid();
	
	pfds[0].fd = dmix->server_fd;
	pfds[0].events = POLLIN | POLLERR | POLLHUP;

	while (1) {
		ret = poll(pfds, current + 1, 500);
		if (ret < 0)	/* some error */
			break;
		if (ret == 0) {	/* timeout */
			struct shmid_ds buf;
			if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0) {
				shm_discard(dmix);
				continue;
			}
			if (buf.shm_nattch == 0)	/* server is the last user, exit */
				break;
		}
		if (pfds[0].revents & (POLLERR | POLLHUP))
			break;
		if (pfds[0].revents & POLLIN) {
			sck = accept(dmix->server_fd, 0, 0);
			if (sck >= 0) {
				if (current == max) {
					close(sck);
				} else {
					unsigned char buf = 'A';
					pfds[current].fd = sck;
					pfds[current].events = POLLIN | POLLERR | POLLHUP;
					send_fd(sck, &buf, 1, dmix->hw_fd);
					current++;
				}
			}
		}
		for (i = 0; i < max; i++) {
			struct pollfd *pfd = &pfds[i+1];
			unsigned char cmd;
			if (pfd->revents & (POLLERR | POLLHUP)) {
				close(pfd->fd);
				pfd->fd = -1;
				continue;
			}
			if (!(pfd->revents & POLLIN))
				continue;
			if (read(pfd->fd, &cmd, 1) == 1)
				cmd = 0 /*process command */;
		}
		for (i = 0; i < max; i++) {
			if (pfds[i+1].fd < 0) {
				if (i + 1 != max)
					memcpy(&pfds[i+1], &pfds[i+2], sizeof(struct pollfd) * (max - i - 1));
				current--;
			}
		}
	}
	close(dmix->server_fd);
	close(dmix->poll_fd);
	shm_discard(dmix);
	exit(EXIT_SUCCESS);
}

static int server_create(snd_pcm_dmix_t *dmix)
{
	int ret;

	dmix->server_fd = -1;

	ret = get_tmp_name(dmix->shmptr->socket_name, sizeof(dmix->shmptr->socket_name));
	if (ret < 0)
		return ret;
	
	ret = make_local_socket(dmix->shmptr->socket_name, 1);
	if (ret < 0)
		return ret;
	dmix->server_fd = ret;

	ret = listen(dmix->server_fd, 4);
	if (ret < 0) {
		close(dmix->server_fd);
		return ret;
	}
	
	ret = fork();
	if (ret < 0) {
		close(dmix->server_fd);
		return ret;
	} else if (ret == 0) {
		server_job(dmix);
	}
	dmix->server_pid = ret;
	dmix->server = 1;
	return 0;
}

static int server_discard(snd_pcm_dmix_t *dmix)
{
	if (dmix->server) {
		kill(dmix->server_pid, SIGTERM);
		waitpid(dmix->server_pid, NULL, 0);
		dmix->server_pid = (pid_t)-1;
	}
	if (dmix->server_fd > 0) {
		close(dmix->server_fd);
		dmix->server_fd = -1;
	}
	dmix->server = 0;
	return 0;
}

/*
 *  client side
 */

int snd_receive_fd(int sock, void *data, size_t len, int *fd);

static int client_connect(snd_pcm_dmix_t *dmix)
{
	int ret;
	unsigned char buf;

	ret = make_local_socket(dmix->shmptr->socket_name, 0);
	if (ret < 0)
		return ret;
	dmix->comm_fd = ret;

	ret = snd_receive_fd(dmix->comm_fd, &buf, 1, &dmix->hw_fd);
	if (ret < 0 || buf != 'A') {
		close(dmix->comm_fd);
		dmix->comm_fd = -1;
		return ret;
	}

	dmix->client = 1;
	return 0;
}

static int client_discard(snd_pcm_dmix_t *dmix)
{
	if (dmix->client) {
		close(dmix->comm_fd);
		dmix->comm_fd = -1;
	}
	return 0;
}

/*
 *  plugin implementation
 */

static int snd_pcm_dmix_nonblock(snd_pcm_t *pcm, int nonblock)
{
	return 0;
}

static int snd_pcm_dmix_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static snd_pcm_state_t snd_pcm_dmix_state(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_prepare(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_reset(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_start(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_drop(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_drain(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return frames;
}

static int snd_pcm_dmix_resume(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	return -ENODEV;
}

static snd_pcm_sframes_t snd_pcm_dmix_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	return -ENODEV;
}

static snd_pcm_sframes_t snd_pcm_dmix_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	return -ENODEV;
}

static snd_pcm_sframes_t snd_pcm_dmix_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	return -ENODEV;
}

static int snd_pcm_dmix_mmap(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_munmap(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return 0;
}

static int snd_pcm_dmix_close(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;

	if (dmix->timer)
		snd_timer_close(dmix->timer);
	semaphore_down(dmix);
	snd_pcm_close(dmix->spcm);
 	if (dmix->server)
 		server_discard(dmix);
 	if (dmix->client)
 		client_discard(dmix);
 	if (shm_discard(dmix) > 0) {
 		if (semaphore_discard(dmix) < 0)
 			semaphore_up(dmix);
 	}
	semaphore_up(dmix);
	pcm->private_data = NULL;
	free(dmix);
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_mmap_commit(snd_pcm_t *pcm,
						snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						snd_pcm_uframes_t size)
{
	snd_pcm_dmix_t *hw = pcm->private_data;
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *hw = pcm->private_data;
	return 0;
}

static void snd_pcm_dmix_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	// snd_pcm_dmix_t *dmix = pcm->private_data;
	snd_output_printf(out, "Direct Stream Mixing PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
}

static snd_pcm_ops_t snd_pcm_dmix_ops = {
	close: snd_pcm_dmix_close,
	info: snd_pcm_dmix_info,
	hw_refine: snd_pcm_dmix_hw_refine,
	hw_params: snd_pcm_dmix_hw_params,
	hw_free: snd_pcm_dmix_hw_free,
	sw_params: snd_pcm_dmix_sw_params,
	channel_info: snd_pcm_dmix_channel_info,
	dump: snd_pcm_dmix_dump,
	nonblock: snd_pcm_dmix_nonblock,
	async: snd_pcm_dmix_async,
	mmap: snd_pcm_dmix_mmap,
	munmap: snd_pcm_dmix_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_dmix_fast_ops = {
	status: snd_pcm_dmix_status,
	state: snd_pcm_dmix_state,
	hwsync: snd_pcm_dmix_hwsync,
	delay: snd_pcm_dmix_delay,
	prepare: snd_pcm_dmix_prepare,
	reset: snd_pcm_dmix_reset,
	start: snd_pcm_dmix_start,
	drop: snd_pcm_dmix_drop,
	drain: snd_pcm_dmix_drain,
	pause: snd_pcm_dmix_pause,
	rewind: snd_pcm_dmix_rewind,
	resume: snd_pcm_dmix_resume,
	writei: snd_pcm_dmix_writei,
	writen: snd_pcm_dmix_writen,
	readi: snd_pcm_dmix_readi,
	readn: snd_pcm_dmix_readn,
	avail_update: snd_pcm_dmix_avail_update,
	mmap_commit: snd_pcm_dmix_mmap_commit,
};

/*
 * this function initializes hardware and starts playback operation with
 * no stop threshold (it operates all time without xrun checking)
 * also, the driver silences the unused ring buffer areas for us
 */
static int snd_pcm_dmix_initialize_slave(snd_pcm_dmix_t *dmix, snd_pcm_t *spcm, struct slave_params *params)
{
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	int ret, buffer_is_not_initialized;
	snd_pcm_uframes_t buffer_size;

	hw_params = &dmix->shmptr->hw_params;
	sw_params = &dmix->shmptr->sw_params;

	ret = snd_pcm_hw_params_any(spcm, hw_params);
	if (ret < 0) {
		SNDERR("snd_pcm_hw_params_any failed");
		return ret;
	}
	ret = snd_pcm_hw_params_set_access(spcm, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (ret < 0) {
		ret = snd_pcm_hw_params_set_access(spcm, hw_params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
		if (ret < 0) {
			SNDERR("slave plugin does not support interleaved or noninterleaved access");
			return ret;
		}
	}
	ret = snd_pcm_hw_params_set_format(spcm, hw_params, params->format);
	if (ret < 0) {
		SNDERR("requested format is not available");
		return ret;
	}
	ret = snd_pcm_hw_params_set_channels(spcm, hw_params, params->channels);
	if (ret < 0) {
		SNDERR("requested count of channels is not available");
		return ret;
	}
	ret = INTERNAL(snd_pcm_hw_params_set_rate_near)(spcm, hw_params, &params->rate, 0);
	if (ret < 0) {
		SNDERR("requested rate is not available");
		return ret;
	}

	buffer_is_not_initialized = 0;
	if (params->buffer_time > 0) {
		ret = INTERNAL(snd_pcm_hw_params_set_buffer_time_near)(spcm, hw_params, &params->buffer_time, 0);
		if (ret < 0) {
			SNDERR("unable to set buffer time");
			return ret;
		}
	} else if (params->buffer_size > 0) {
		ret = INTERNAL(snd_pcm_hw_params_set_buffer_size_near)(spcm, hw_params, &params->buffer_size);
		if (ret < 0) {
			SNDERR("unable to set buffer size");
			return ret;
		}
	} else {
		buffer_is_not_initialized = 1;
	}

	if (params->period_time > 0) {
		ret = INTERNAL(snd_pcm_hw_params_set_period_time_near)(spcm, hw_params, &params->period_time, 0);
		if (ret < 0) {
			SNDERR("unable to set period_time");
			return ret;
		}
	} else if (params->period_size > 0) {
		ret = INTERNAL(snd_pcm_hw_params_set_period_size_near)(spcm, hw_params, &params->period_size, 0);
		if (ret < 0) {
			SNDERR("unable to set period_size");
			return ret;
		}
	}		
	
	if (buffer_is_not_initialized && params->periods > 0) {
		ret = INTERNAL(snd_pcm_hw_params_set_periods_near)(spcm, hw_params, &params->periods, 0);
		if (ret < 0) {
			SNDERR("unable to set requested periods");
			return ret;
		}
	}
	
	ret = snd_pcm_hw_params(spcm, hw_params);
	if (ret < 0) {
		SNDERR("unable to install hw params");
		return ret;
	}

	ret = snd_pcm_sw_params_current(spcm, sw_params);
	if (ret < 0) {
		SNDERR("unable to get current sw_params");
		return ret;
	}

	ret = INTERNAL(snd_pcm_hw_params_get_buffer_size)(hw_params, &buffer_size);
	if (ret < 0) {
		SNDERR("unable to get buffer size\n");
		return ret;
	}
	ret = snd_pcm_sw_params_set_stop_threshold(spcm, sw_params, buffer_size);
	if (ret < 0) {
		SNDERR("unable to set stop threshold\n");
		return ret;
	}
	ret = snd_pcm_sw_params_set_silence_threshold(spcm, sw_params, buffer_size);
	if (ret < 0) {
		SNDERR("unable to set silence threshold\n");
		return ret;
	}

	ret = snd_pcm_sw_params(spcm, sw_params);
	if (ret < 0) {
		SNDERR("unable to install sw params");
		return ret;
	}
	
	ret = snd_pcm_start(spcm);
	if (ret < 0) {
		SNDERR("unable to start PCM stream\n");
		return ret;
	}

	return 0;
}

/*
 * the trick is used here; we cannot use effectively the hardware handle because
 * we cannot drive multiple accesses to appl_ptr; so we use slave timer of given
 * PCM hardware handle; it's not this easy and cheap?
 */
static int snd_pcm_dmix_initialize_poll_fd(snd_pcm_dmix_t *dmix)
{
	int ret;
	snd_pcm_info_t *info;
	snd_timer_params_t *params;
	char name[128];
	
	snd_pcm_info_alloca(&info);
	snd_timer_params_alloca(&params);
	ret = snd_pcm_info(dmix->spcm, info);
	if (ret < 0) {
		SNDERR("unable to info for slave pcm\n");
		return ret;
	}
	sprintf(name, "hw:CLASS=%i,SCLASS=0,CARD=%i,DEV=%i,SUBDEV=%i",
				(int)SND_TIMER_CLASS_PCM, 
				snd_pcm_info_get_card(info),
				snd_pcm_info_get_device(info),
				snd_pcm_info_get_subdevice(info) * 2);	/* it's a bit trick to distict playback and capture */
	ret = snd_timer_open(&dmix->timer, name, SND_TIMER_OPEN_NONBLOCK);
	if (ret < 0) {
		SNDERR("unable to open timer '%s'\n", name);
		return ret;
	}
	snd_timer_params_set_auto_start(params, 1);
	snd_timer_params_set_ticks(params, 1);
	ret = snd_timer_params(dmix->timer, params);
	if (ret < 0) {
		SNDERR("unable to set timer parameters\n", name);
                return ret;
	}
	return 0;
}

/**
 * \brief Creates a new dmix PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param ipc_key IPC key for semaphore and shared memory
 * \param params Parameters for slave
 * \param root Configuration root
 * \param sconf Slave configuration
 * \param stream PCM Direction (stream)
 * \param mode PCM Mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_dmix_open(snd_pcm_t **pcmp, const char *name,
		      key_t ipc_key, struct slave_params *params,
		      snd_config_t *root, snd_config_t *sconf,
		      snd_pcm_stream_t stream, int mode)
{
	snd_pcm_t *pcm = NULL, *spcm = NULL;
	snd_pcm_dmix_t *dmix = NULL;
	int ret, first_instance;

	assert(pcmp);

	if (stream != SND_PCM_STREAM_PLAYBACK) {
		SNDERR("The dmix plugin supports only playback stream");
		return -EINVAL;
	}

	dmix = calloc(1, sizeof(snd_pcm_dmix_t));
	if (!dmix) {
		ret = -ENOMEM;
		goto _err;
	}
	dmix->ipc_key = ipc_key;
	dmix->semid = -1;
	dmix->shmid = -1;

	ret = snd_pcm_new(&pcm, SND_PCM_TYPE_DMIX, name, stream, mode);
	if (ret < 0)
		goto _err;

	ret = semaphore_create_or_connect(dmix);
	if (ret < 0)
		goto _err;
	
	ret = semaphore_down(dmix);
	if (ret < 0) {
		semaphore_discard(dmix);
		goto _err;
	}
		
	first_instance = ret = shm_create_or_connect(dmix);
	if (ret < 0)
		goto _err;
		
	pcm->ops = &snd_pcm_dmix_ops;
	pcm->fast_ops = &snd_pcm_dmix_fast_ops;
	pcm->private_data = dmix;

	if (first_instance) {
		ret = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
		if (ret < 0)
			goto _err;
	
		if (snd_pcm_type(spcm) != SND_PCM_TYPE_HW) {
			SNDERR("dmix plugin can be only connected to hw plugin");
			goto _err;
		}
		
		ret = snd_pcm_dmix_initialize_slave(dmix, spcm, params);
		if (ret < 0)
			goto _err;

		dmix->spcm = spcm;
		
		ret = server_create(dmix);
		if (ret < 0)
			goto _err;
	} else {
		ret = client_connect(dmix);
		if (ret < 0)
			return ret;
			
		ret = snd_pcm_hw_open_fd(&spcm, "dmix_client", dmix->hw_fd, 0);
		if (ret < 0)
			goto _err;
	}

	ret = snd_pcm_dmix_initialize_poll_fd(dmix);
	if (ret < 0)
		goto _err;
		
	pcm->poll_fd = dmix->poll_fd;
		
	dmix->shmptr->type = spcm->type;

	snd_pcm_set_hw_ptr(pcm, &dmix->hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &dmix->appl_ptr, -1, 0);

	*pcmp = pcm;
	return 0;
	
 _err:
 	if (dmix->timer)
 		snd_timer_close(dmix->timer);
 	if (dmix->server)
 		server_discard(dmix);
 	if (dmix->client)
 		client_discard(dmix);
 	if (spcm)
 		snd_pcm_close(spcm);
 	if (dmix->shmid >= 0) {
 		if (shm_discard(dmix) > 0) {
		 	if (dmix->semid >= 0) {
 				if (semaphore_discard(dmix) < 0)
 					semaphore_up(dmix);
 			}
 		}
 	}
	if (dmix)
		free(dmix);
	if (pcm)
		snd_pcm_free(pcm);
	return ret;
}

/*! \page pcm_plugins

\section pcm_plugins_dmix Plugin: dmix

This plugin provides direct mixing of multiple streams.

\code
pcm.name {
	type dmix		# Direct mix
	ipc_key INT		# unique IPC key
	slave STR
	# or
	slave {			# Slave definition
		pcm STR		# slave PCM name
		# or
		pcm { }		# slave PCM definition
		format STR	# format definition
		rate INT	# rate definition
		channels INT
		period_time INT	# in usec
		# or
		period_size INT	# in bytes
		buffer_time INT	# in usec
		# or
		buffer_size INT # in bytes
		periods INT	# when buffer_size or buffer_time is not specified
	}
}
\endcode

\subsection pcm_plugins_hw_funcref Function reference

<UL>
  <LI>snd_pcm_dmix_open()
  <LI>_snd_pcm_dmix_open()
</UL>

*/

/**
 * \brief Creates a new dmix PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with dmix PCM description
 * \param stream PCM Stream
 * \param mode PCM Mode
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_dmix_open(snd_pcm_t **pcmp, const char *name,
		       snd_config_t *root, snd_config_t *conf,
		       snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	snd_config_t *slave = NULL, *sconf;
	struct slave_params params;
	int bsize, psize;
	key_t ipc_key = 0;
	int err;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "ipc_key") == 0) {
			long key;
			err = snd_config_get_integer(n, &key);
			if (err < 0) {
				SNDERR("The field ipc_key must be an integer type");
				return err;
			}
			ipc_key = key;
		}
		if (strcmp(id, "slave") == 0) {
			slave = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	if (!ipc_key) {
		SNDERR("Unique IPC key is not defined");
		return -EINVAL;
	}
	/* the default settings, it might be invalid for some hardware */
	params.format = SND_PCM_FORMAT_S16;
	params.rate = 48000;
	params.channels = 2;
	params.period_time = 125000;	/* 0.125 seconds */
	params.buffer_time = -1;
	bsize = psize = -1;
	params.periods = 3;
	err = snd_pcm_slave_conf(root, slave, &sconf, 8,
				 SND_PCM_HW_PARAM_FORMAT, 0, &params.format,
				 SND_PCM_HW_PARAM_RATE, 0, &params.rate,
				 SND_PCM_HW_PARAM_CHANNELS, 0, &params.channels,
				 SND_PCM_HW_PARAM_PERIOD_TIME, 0, &params.period_time,
				 SND_PCM_HW_PARAM_BUFFER_TIME, 0, &params.buffer_time,
				 SND_PCM_HW_PARAM_PERIOD_SIZE, 0, &bsize,
				 SND_PCM_HW_PARAM_BUFFER_SIZE, 0, &psize,
				 SND_PCM_HW_PARAM_PERIODS, 0, &params.periods);
	if (err < 0)
		return err;
	params.period_size = psize;
	params.buffer_size = bsize;
	err = snd_pcm_dmix_open(pcmp, name, ipc_key, &params, root, sconf, stream, mode);
	if (err < 0)
		snd_config_delete(sconf);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_dmix_open, SND_PCM_DLSYM_VERSION);
#endif
