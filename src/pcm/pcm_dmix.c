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
#include <sys/mman.h>
#include "pcm_local.h"
#include "../control/control_local.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_dmix = "";
#endif

/*
 *
 */
 
 
/*
 * FIXME:
 *  add possibility to use futexes here
 */
#define DMIX_IPC_SEMS		1
#define DMIX_IPC_SEM_CLIENT	0

/*
 *
 */
 
int snd_timer_async(snd_timer_t *timer, int sig, pid_t pid);

typedef void (mix_areas1_t)(unsigned int size,
			volatile signed short *dst, signed short *src,
			volatile signed int *sum, unsigned int dst_step,
			unsigned int src_step, unsigned int sum_step);

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
	struct {
		snd_pcm_uframes_t buffer_size;
		snd_pcm_uframes_t boundary;
		snd_pcm_uframes_t channels;
		unsigned int sample_bits;
		unsigned int rate;
		snd_pcm_format_t format;
	} s;
} snd_pcm_dmix_share_t;

typedef struct {
	key_t ipc_key;	/* IPC key for semaphore and memory */
	int semid;	/* IPC global semaphore identification */
	int shmid;	/* IPC global shared memory identification */
	int shmid_sum;	/* IPC global sum ring buffer memory identification */
	snd_pcm_dmix_share_t *shmptr;	/* pointer to shared memory area */
	signed int *sum_buffer;		/* shared sum buffer */
	snd_pcm_t *spcm; /* slave PCM handle */
	snd_pcm_uframes_t appl_ptr;
	snd_pcm_uframes_t hw_ptr;
	snd_pcm_uframes_t avail_max;
	snd_pcm_uframes_t slave_appl_ptr;
	snd_pcm_uframes_t slave_hw_ptr;
	snd_pcm_state_t state;
	snd_timestamp_t trigger_tstamp;
	int server, client;
	int comm_fd;	/* communication file descriptor (socket) */
	int hw_fd;	/* hardware file descriptor */
	int poll_fd;
	int server_fd;
	pid_t server_pid;
	snd_timer_t *timer; /* timer used as poll_fd */
	int interleaved; /* we have interleaved buffer */
	mix_areas1_t *mix_areas1;
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
	dmix->semid = semget(dmix->ipc_key, DMIX_IPC_SEMS, IPC_CREAT | 0666);
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
	dmix->semid = -1;
	return 0;
}

static int semaphore_down(snd_pcm_dmix_t *dmix, int sem_num)
{
	struct sembuf op[2] = { { 0, 0, SEM_UNDO }, { 0, 1, SEM_UNDO | IPC_NOWAIT } };
	assert(dmix->semid >= 0);
	op[0].sem_num = sem_num;
	op[1].sem_num = sem_num;
	if (semop(dmix->semid, op, 2) < 0)
		return -errno;
	return 0;
}

static int semaphore_up(snd_pcm_dmix_t *dmix, int sem_num)
{
	struct sembuf op = { 0, -1, SEM_UNDO | IPC_NOWAIT };
	assert(dmix->semid >= 0);
	op.sem_num = sem_num;
	if (semop(dmix->semid, &op, 1) < 0)
		return -errno;
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
	
	dmix->shmid = shmget(dmix->ipc_key, sizeof(snd_pcm_dmix_share_t), IPC_CREAT | 0666);
	if (dmix->shmid < 0)
		return -errno;
	dmix->shmptr = shmat(dmix->shmid, 0, 0);
	if (dmix->shmptr == (void *) -1) {
		shm_discard(dmix);
		return -errno;
	}
	mlock(dmix->shmptr, sizeof(snd_pcm_dmix_share_t));
	if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0) {
		shm_discard(dmix);
		return -errno;
	}
	if (buf.shm_nattch == 1) {	/* we're the first user, clear the segment */
		memset(dmix->shmptr, 0, sizeof(snd_pcm_dmix_share_t));
		ret = 1;
	}
	return ret;
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
	if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0)
		return -errno;
	if (buf.shm_nattch == 0) {	/* we're the last user, destroy the segment */
		if (shmctl(dmix->shmid, IPC_RMID, NULL) < 0)
			return -errno;
		ret = 1;
	}
	dmix->shmid = -1;
	return ret;
}

/*
 *  sum ring buffer shared memory area 
 */

static int shm_sum_create_or_connect(snd_pcm_dmix_t *dmix)
{
	static int shm_sum_discard(snd_pcm_dmix_t *dmix);
	size_t size;

	size = dmix->shmptr->s.channels *
	       dmix->shmptr->s.buffer_size *
	       sizeof(signed int);	
	dmix->shmid_sum = shmget(dmix->ipc_key + 1, size, IPC_CREAT | 0666);
	if (dmix->shmid_sum < 0)
		return -errno;
	dmix->sum_buffer = shmat(dmix->shmid_sum, 0, 0);
	if (dmix->sum_buffer == (void *) -1) {
		shm_sum_discard(dmix);
		return -errno;
	}
	mlock(dmix->sum_buffer, size);
	return 0;
}

static int shm_sum_discard(snd_pcm_dmix_t *dmix)
{
	struct shmid_ds buf;
	int ret = 0;

	if (dmix->shmid_sum < 0)
		return -EINVAL;
	if (dmix->sum_buffer != (void *) -1 && shmdt(dmix->sum_buffer) < 0)
		return -errno;
	dmix->sum_buffer = (void *) -1;
	if (shmctl(dmix->shmid_sum, IPC_STAT, &buf) < 0)
		return -errno;
	if (buf.shm_nattch == 0) {	/* we're the last user, destroy the segment */
		if (shmctl(dmix->shmid_sum, IPC_RMID, NULL) < 0)
			return -errno;
		ret = 1;
	}
	dmix->shmid_sum = -1;
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

#if 0
#define server_printf(fmt, args...) printf(fmt, ##args)
#else
#define server_printf(fmt, args...) /* nothing */
#endif

static void server_job(snd_pcm_dmix_t *dmix)
{
	int ret, sck, i;
	int max = 128, current = 0;
	struct pollfd pfds[max + 1];

	/* close all files to free resources */
	i = sysconf(_SC_OPEN_MAX);
	while (--i >= 3) {
		if (i != dmix->server_fd && i != dmix->hw_fd)
			close(i);
	}
	
	/* detach from parent */
	setsid();

	pfds[0].fd = dmix->server_fd;
	pfds[0].events = POLLIN | POLLERR | POLLHUP;

	server_printf("DMIX SERVER STARTED\n");
	while (1) {
		ret = poll(pfds, current + 1, 500);
		server_printf("DMIX SERVER: poll ret = %i, revents[0] = 0x%x\n", ret, pfds[0].revents);
		if (ret < 0)	/* some error */
			break;
		if (ret == 0 || pfds[0].revents & (POLLERR | POLLHUP)) {	/* timeout or error? */
			struct shmid_ds buf;
			semaphore_down(dmix, DMIX_IPC_SEM_CLIENT);
			if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0) {
				shm_discard(dmix);
				continue;
			}
			server_printf("DMIX SERVER: nattch = %i\n", (int)buf.shm_nattch);
			if (buf.shm_nattch == 1)	/* server is the last user, exit */
				break;
			semaphore_up(dmix, DMIX_IPC_SEM_CLIENT);
			continue;
		}
		if (pfds[0].revents & POLLIN) {
			ret--;
			sck = accept(dmix->server_fd, 0, 0);
			if (sck >= 0) {
				server_printf("DMIX SERVER: new connection %i\n", sck);
				if (current == max) {
					close(sck);
				} else {
					unsigned char buf = 'A';
					pfds[current+1].fd = sck;
					pfds[current+1].events = POLLIN | POLLERR | POLLHUP;
					send_fd(sck, &buf, 1, dmix->hw_fd);
					server_printf("DMIX SERVER: fd sent ok\n");
					current++;
				}
			}
		}
		for (i = 0; i < current && ret > 0; i++) {
			struct pollfd *pfd = &pfds[i+1];
			unsigned char cmd;
			server_printf("client %i revents = 0x%x\n", pfd->fd, pfd->revents);
			if (pfd->revents & (POLLERR | POLLHUP)) {
				ret--;
				close(pfd->fd);
				pfd->fd = -1;
				continue;
			}
			if (!(pfd->revents & POLLIN))
				continue;
			ret--;
			if (read(pfd->fd, &cmd, 1) == 1)
				cmd = 0 /*process command */;
		}
		for (i = 0; i < current; i++) {
			if (pfds[i+1].fd < 0) {
				if (i + 1 != max)
					memcpy(&pfds[i+1], &pfds[i+2], sizeof(struct pollfd) * (max - i - 1));
				current--;
			}
		}
	}
	close(dmix->server_fd);
	close(dmix->hw_fd);
	shm_discard(dmix);
	semaphore_discard(dmix);
	server_printf("DMIX SERVER EXIT\n");
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
		//kill(dmix->server_pid, SIGTERM);
		//waitpid(dmix->server_pid, NULL, 0);
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
 *  ring buffer operation
 */

static int check_interleave(snd_pcm_dmix_t *dmix)
{
	unsigned int chn, channels;
	int interleaved = 1;
	const snd_pcm_channel_area_t *dst_areas;

	channels = dmix->shmptr->s.channels;
	dst_areas = snd_pcm_mmap_areas(dmix->spcm);
	for (chn = 1; chn < channels; chn++) {
		if (dst_areas[chn-1].addr != dst_areas[chn].addr) {
			interleaved = 0;
			break;
		}
	}
	for (chn = 0; chn < channels; chn++) {
		if (dst_areas[chn].first != sizeof(signed short) * chn * 8 ||
		    dst_areas[chn].step != channels * sizeof(signed short) * 8) {
			interleaved = 0;
			break;
		}
	}
	return dmix->interleaved = interleaved;
}

/*
 *  the main function of this plugin: mixing
 *  FIXME: optimize it for different architectures
 */

#ifdef __i386__

#define ADD_AND_SATURATE

#define MIX_AREAS1 mix_areas1
#define MIX_AREAS1_MMX mix_areas1_mmx
#define LOCK_PREFIX ""
#include "pcm_dmix_i386.h"
#undef MIX_AREAS1
#undef MIX_AREAS1_MMX
#undef LOCK_PREFIX

#define MIX_AREAS1 mix_areas1_smp
#define MIX_AREAS1_MMX mix_areas1_smp_mmx
#define LOCK_PREFIX "lock ; "
#include "pcm_dmix_i386.h"
#undef MIX_AREAS1
#undef MIX_AREAS1_MMX
#undef LOCK_PREFIX
 
static void mix_select_callbacks(snd_pcm_dmix_t *dmix)
{
	FILE *in;
	char line[255];
	int smp = 0, mmx = 0;
	
	/* safe settings for all i386 CPUs */
	dmix->mix_areas1 = mix_areas1_smp;
	/* try to determine, if we have a MMX capable CPU */
	in = fopen("/proc/cpuinfo", "r");
	if (in == NULL)
		return;
	while (!feof(in)) {
		fgets(line, sizeof(line), in);
		if (!strncmp(line, "processor", 9))
			smp++;
		else if (!strncmp(line, "flags", 5)) {
			if (strstr(line, " mmx"))
				mmx = 1;
		}
	}
	fclose(in);
	printf("MMX: %i, SMP: %i\n", mmx, smp);
	if (mmx) {
		dmix->mix_areas1 = smp > 1 ? mix_areas1_smp_mmx : mix_areas1_mmx;
	} else {
		dmix->mix_areas1 = smp > 1 ? mix_areas1_smp : mix_areas1;
	}
}
#endif

#ifndef ADD_AND_SATURATE
#warning Please, recode mix_areas1() routine to your architecture...
static void mix_areas1(unsigned int size,
		       volatile signed short *dst, signed short *src,
		       volatile signed int *sum, unsigned int dst_step,
		       unsigned int src_step, unsigned int sum_step)
{
	register signed int sample, old_sample;

	while (size-- > 0) {
		sample = *src;
		if (*dst == 0)
			sample -= *sum;
		*sum += sample;
		do {
			old_sample = *sum;
			if (old_sample > 0x7fff)
				sample = 0x7fff;
			else if (old_sample < -0x8000)
				sample = -0x8000;
			else
				sample = old_sample;
			*dst = sample;
		} while (*sum != old_sample);
		((char *)src) += dst_step;
		((char *)dst) += src_step;
		((char *)sum) += sum_step;		
	}
}

static void mix_select_callbacks(snd_pcm_dmix_t *dmix)
{
	dmix->mix_areas1 = mix_areas1;
}
#endif

static void mix_areas(snd_pcm_dmix_t *dmix,
		      const snd_pcm_channel_area_t *src_areas,
		      const snd_pcm_channel_area_t *dst_areas,
		      snd_pcm_uframes_t src_ofs,
		      snd_pcm_uframes_t dst_ofs,
		      snd_pcm_uframes_t size)
{
	signed short *src;
	volatile signed short *dst;
	volatile signed int *sum;
	unsigned int src_step, dst_step;
	unsigned int chn, channels;
	
	channels = dmix->shmptr->s.channels;
	if (dmix->interleaved) {
		/*
		 * process the all areas in one loop
		 * it optimizes the memory accesses for this case
		 */
		dmix->mix_areas1(size * channels,
				 ((signed short *)dst_areas[0].addr) + (dst_ofs * channels),
				 ((signed short *)src_areas[0].addr) + (src_ofs * channels),
				 dmix->sum_buffer + (dst_ofs * channels),
				 sizeof(signed short),
				 sizeof(signed short),
				 sizeof(signed int));
		return;
	}
	for (chn = 0; chn < channels; chn++) {
		src_step = src_areas[chn].step / 8;
		dst_step = dst_areas[chn].step / 8;
		src = (signed short *)(((char *)src_areas[chn].addr + src_areas[chn].first / 8) + (src_ofs * src_step));
		dst = (signed short *)(((char *)dst_areas[chn].addr + dst_areas[chn].first / 8) + (dst_ofs * dst_step));
		sum = dmix->sum_buffer + channels * dst_ofs + chn;
		dmix->mix_areas1(size, dst, src, sum, dst_step, src_step, channels * sizeof(signed int));
	}
}

/*
 *  synchronize shm ring buffer with hardware
 */
static void snd_pcm_dmix_sync_area(snd_pcm_t *pcm, snd_pcm_uframes_t size)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	snd_pcm_uframes_t appl_ptr, slave_appl_ptr, transfer;
	const snd_pcm_channel_area_t *src_areas, *dst_areas;
	
	/* get the start of update area */
	appl_ptr = dmix->appl_ptr - size;
	if (appl_ptr > pcm->boundary)
		appl_ptr += pcm->boundary;
	appl_ptr %= pcm->buffer_size;
	/* add sample areas here */
	src_areas = snd_pcm_mmap_areas(pcm);
	dst_areas = snd_pcm_mmap_areas(dmix->spcm);
	slave_appl_ptr = dmix->slave_appl_ptr % dmix->shmptr->s.buffer_size;
	dmix->slave_appl_ptr += size;
	dmix->slave_appl_ptr %= dmix->shmptr->s.boundary;
	while (size > 0) {
		transfer = appl_ptr + size > pcm->buffer_size ? pcm->buffer_size - appl_ptr : size;
		transfer = slave_appl_ptr + transfer > dmix->shmptr->s.buffer_size ? dmix->shmptr->s.buffer_size - slave_appl_ptr : transfer;
		size -= transfer;
		mix_areas(dmix, src_areas, dst_areas, appl_ptr, slave_appl_ptr, transfer);
		slave_appl_ptr += transfer;
		slave_appl_ptr %= dmix->shmptr->s.buffer_size;
		appl_ptr += transfer;
		appl_ptr %= pcm->buffer_size;
	}
}

/*
 *  synchronize hardware pointer (hw_ptr) with ours
 */
static int snd_pcm_dmix_sync_ptr(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	snd_pcm_uframes_t slave_hw_ptr, old_slave_hw_ptr, avail;
	snd_pcm_sframes_t diff;
	
	old_slave_hw_ptr = dmix->slave_hw_ptr;
	slave_hw_ptr = dmix->slave_hw_ptr = *dmix->spcm->hw.ptr;
	diff = slave_hw_ptr - old_slave_hw_ptr;
	if (diff == 0)		/* fast path */
		return 0;
	if (diff < 0) {
		slave_hw_ptr += dmix->shmptr->s.boundary;
		diff = slave_hw_ptr - old_slave_hw_ptr;
	}
	dmix->hw_ptr += diff;
	dmix->hw_ptr %= pcm->boundary;
	// printf("sync ptr diff = %li\n", diff);
	if (pcm->stop_threshold >= pcm->boundary)	/* don't care */
		return 0;
	if ((avail = snd_pcm_mmap_playback_avail(pcm)) >= pcm->stop_threshold) {
		gettimeofday(&dmix->trigger_tstamp, 0);
		dmix->state = SND_PCM_STATE_XRUN;
		dmix->avail_max = avail;
		return -EPIPE;
	}
	if (avail > dmix->avail_max)
		dmix->avail_max = avail;
	return 0;
}

/*
 *  plugin implementation
 */

static int snd_pcm_dmix_nonblock(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int nonblock ATTRIBUTE_UNUSED)
{
	/* value is cached for us in pcm->mode (SND_PCM_NONBLOCK flag) */
	return 0;
}

static int snd_pcm_dmix_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return snd_timer_async(dmix->timer, sig, pid);
}

static int snd_pcm_dmix_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	unsigned short events;
	static snd_timer_read_t rbuf[5];	/* can be overwriten by multiple plugins, we don't need the value */

	assert(pfds && nfds == 1 && revents);
	events = pfds[0].revents;
	if (events & POLLIN) {
		events |= POLLOUT;
		events &= ~POLLIN;
		/* empty the timer read queue */
		while (snd_timer_read(dmix->timer, &rbuf, sizeof(rbuf)) == sizeof(rbuf)) ;
	}
	*revents = events;
	return 0;
}

static int snd_pcm_dmix_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	// snd_pcm_dmix_t *dmix = pcm->private_data;

	memset(info, 0, sizeof(*info));
	info->stream = pcm->stream;
	info->card = -1;
	/* FIXME: fill this with something more useful: we know the hardware name */
	strncpy(info->id, pcm->name, sizeof(info->id));
	strncpy(info->name, pcm->name, sizeof(info->name));
	strncpy(info->subname, pcm->name, sizeof(info->subname));
	info->subdevices_count = 1;
	return 0;
}

static inline snd_mask_t *hw_param_mask(snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var)
{
	return &params->masks[var - SND_PCM_HW_PARAM_FIRST_MASK];
}

static inline snd_interval_t *hw_param_interval(snd_pcm_hw_params_t *params,
						snd_pcm_hw_param_t var)
{
	return &params->intervals[var - SND_PCM_HW_PARAM_FIRST_INTERVAL];
}

static int hw_param_interval_refine_one(snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var,
					snd_pcm_hw_params_t *src)
{
	snd_interval_t *i;

	if (!(params->rmask & (1<<var)))	/* nothing to do? */
		return 0;
	i = hw_param_interval(params, var);
	if (snd_interval_empty(i)) {
		SNDERR("dmix interval %i empty?\n", (int)var);
		return -EINVAL;
	}
	if (snd_interval_refine(i, hw_param_interval(src, var)))
		params->cmask |= 1<<var;
	return 0;
}

#undef REFINE_DEBUG

static int snd_pcm_dmix_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	snd_pcm_hw_params_t *hw_params = &dmix->shmptr->hw_params;
	static snd_mask_t access = { .bits = { 
					(1<<SNDRV_PCM_ACCESS_MMAP_INTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_RW_INTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_RW_NONINTERLEAVED),
					0, 0, 0 } };
	int err;

#ifdef REFINE_DEBUG
	snd_output_t *log;
	snd_output_stdio_attach(&log, stderr, 0);
	snd_output_puts(log, "DMIX REFINE (begin):\n");
	snd_pcm_hw_params_dump(params, log);
#endif

	if (params->rmask & (1<<SND_PCM_HW_PARAM_ACCESS)) {
		if (snd_mask_empty(hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS))) {
			SNDERR("dmix access mask empty?\n");
			return -EINVAL;
		}
		if (snd_mask_refine(hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS), &access))
			params->cmask |= 1<<SND_PCM_HW_PARAM_ACCESS;
	}
	if (params->rmask & (1<<SND_PCM_HW_PARAM_FORMAT)) {
		if (snd_mask_empty(hw_param_mask(params, SND_PCM_HW_PARAM_FORMAT))) {
			SNDERR("dmix format mask empty?\n");
			return -EINVAL;
		}
		if (snd_mask_refine_set(hw_param_mask(params, SND_PCM_HW_PARAM_FORMAT),
				        snd_mask_value(hw_param_mask(hw_params, SND_PCM_HW_PARAM_FORMAT))))
			params->cmask |= 1<<SND_PCM_HW_PARAM_FORMAT;
	}
	//snd_mask_none(hw_param_mask(params, SND_PCM_HW_PARAM_SUBFORMAT));
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_CHANNELS, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_RATE, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_BUFFER_SIZE, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_BUFFER_TIME, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_PERIOD_SIZE, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_PERIOD_TIME, hw_params);
	if (err < 0)
		return err;
	err = hw_param_interval_refine_one(params, SND_PCM_HW_PARAM_PERIODS, hw_params);
	if (err < 0)
		return err;
#ifdef REFINE_DEBUG
	snd_output_puts(log, "DMIX REFINE (end):\n");
	snd_pcm_hw_params_dump(params, log);
	snd_output_close(log);
#endif
	return 0;
}

static int snd_pcm_dmix_hw_params(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t * params ATTRIBUTE_UNUSED)
{
	/* values are cached in the pcm structure */
	
	return 0;
}

static int snd_pcm_dmix_hw_free(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	/* values are cached in the pcm structure */
	return 0;
}

static int snd_pcm_dmix_sw_params(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t * params ATTRIBUTE_UNUSED)
{
	/* values are cached in the pcm structure */
	return 0;
}

static int snd_pcm_dmix_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
        return snd_pcm_channel_info_shm(pcm, info, -1);
}

static int snd_pcm_dmix_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;

	memset(status, 0, sizeof(*status));
	status->state = dmix->state;
	status->trigger_tstamp = dmix->trigger_tstamp;
	gettimeofday(&status->tstamp, 0);
	status->avail = snd_pcm_mmap_playback_avail(pcm);
	status->avail_max = status->avail > dmix->avail_max ? status->avail : dmix->avail_max;
	dmix->avail_max = 0;
	return 0;
}

static snd_pcm_state_t snd_pcm_dmix_state(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	return dmix->state;
}

static int snd_pcm_dmix_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	int err;
	
	switch(dmix->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		err = snd_pcm_dmix_sync_ptr(pcm);
		if (err < 0)
			return err;
	case SNDRV_PCM_STATE_PREPARED:
	case SNDRV_PCM_STATE_SUSPENDED:
		*delayp = snd_pcm_mmap_playback_hw_avail(pcm);
		return 0;
	case SNDRV_PCM_STATE_XRUN:
		return -EPIPE;
	default:
		return -EBADFD;
	}
}

static int snd_pcm_dmix_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;

	switch(dmix->state) {
	case SNDRV_PCM_STATE_DRAINING:
	case SNDRV_PCM_STATE_RUNNING:
		return snd_pcm_dmix_sync_ptr(pcm);
	case SNDRV_PCM_STATE_PREPARED:
	case SNDRV_PCM_STATE_SUSPENDED:
		return 0;
	case SNDRV_PCM_STATE_XRUN:
		return -EPIPE;
	default:
		return -EBADFD;
	}
}

static int snd_pcm_dmix_prepare(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;

	// assert(pcm->boundary == dmix->shmptr->s.boundary);	/* for sure */
	dmix->state = SND_PCM_STATE_PREPARED;
	dmix->appl_ptr = 0;
	dmix->hw_ptr = 0;
	return 0;
}

static int snd_pcm_dmix_reset(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	dmix->hw_ptr %= pcm->period_size;
	dmix->appl_ptr = dmix->hw_ptr;
	dmix->slave_appl_ptr = dmix->slave_hw_ptr = *dmix->spcm->hw.ptr;
	return 0;
}

static int snd_pcm_dmix_start(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	snd_pcm_sframes_t avail;
	int err;
	
	if (dmix->state != SND_PCM_STATE_PREPARED)
		return -EBADFD;
	err = snd_timer_start(dmix->timer);
	if (err < 0)
		return err;
	dmix->state = SND_PCM_STATE_RUNNING;
	dmix->slave_appl_ptr = dmix->slave_hw_ptr = *dmix->spcm->hw.ptr;
	avail = snd_pcm_mmap_playback_hw_avail(pcm);
	if (avail < 0)
		return 0;
	if (avail > (snd_pcm_sframes_t)pcm->buffer_size)
		avail = pcm->buffer_size;
	snd_pcm_dmix_sync_area(pcm, avail);
	gettimeofday(&dmix->trigger_tstamp, 0);
	return 0;
}

static int snd_pcm_dmix_drop(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	if (dmix->state == SND_PCM_STATE_OPEN)
		return -EBADFD;
	snd_timer_stop(dmix->timer);
	dmix->state = SND_PCM_STATE_SETUP;
	return 0;
}

static int snd_pcm_dmix_drain(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	snd_pcm_uframes_t stop_threshold;
	int err;

	if (dmix->state == SND_PCM_STATE_OPEN)
		return -EBADFD;
	stop_threshold = pcm->stop_threshold;
	if (pcm->stop_threshold > pcm->buffer_size)
		pcm->stop_threshold = pcm->buffer_size;
	while (dmix->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dmix_sync_ptr(pcm);
		if (err < 0)
			break;
		if (pcm->mode & SND_PCM_NONBLOCK)
			return -EAGAIN;
		snd_pcm_wait(pcm, -1);
	}
	pcm->stop_threshold = stop_threshold;
	return snd_pcm_dmix_drop(pcm);
}

static int snd_pcm_dmix_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
        if (enable) {
		if (dmix->state != SND_PCM_STATE_RUNNING)
			return -EBADFD;
		dmix->state = SND_PCM_STATE_PAUSED;
		snd_timer_stop(dmix->timer);
	} else {
		if (dmix->state != SND_PCM_STATE_PAUSED)
			return -EBADFD;
                dmix->state = SND_PCM_STATE_RUNNING;
                snd_timer_start(dmix->timer);
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	/* FIXME: substract samples from the mix ring buffer, too? */
	snd_pcm_mmap_appl_backward(pcm, frames);
	return frames;
}

static snd_pcm_sframes_t snd_pcm_dmix_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_sframes_t avail;

	avail = snd_pcm_mmap_avail(pcm);
	if (avail < 0)
		return 0;
	if (frames > avail)
		frames = avail;
	snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

static int snd_pcm_dmix_resume(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	// snd_pcm_dmix_t *dmix = pcm->private_data;
	// FIXME
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_readi(snd_pcm_t *pcm ATTRIBUTE_UNUSED, void *buffer ATTRIBUTE_UNUSED, snd_pcm_uframes_t size ATTRIBUTE_UNUSED)
{
	return -ENODEV;
}

static snd_pcm_sframes_t snd_pcm_dmix_readn(snd_pcm_t *pcm ATTRIBUTE_UNUSED, void **bufs ATTRIBUTE_UNUSED, snd_pcm_uframes_t size ATTRIBUTE_UNUSED)
{
	return -ENODEV;
}

static int snd_pcm_dmix_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_dmix_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_dmix_close(snd_pcm_t *pcm)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;

	if (dmix->timer)
		snd_timer_close(dmix->timer);
	semaphore_down(dmix, DMIX_IPC_SEM_CLIENT);
	snd_pcm_close(dmix->spcm);
 	if (dmix->server)
 		server_discard(dmix);
 	if (dmix->client)
 		client_discard(dmix);
 	shm_sum_discard(dmix);
 	if (shm_discard(dmix) > 0) {
 		if (semaphore_discard(dmix) < 0)
 			semaphore_up(dmix, DMIX_IPC_SEM_CLIENT);
 	} else {
		semaphore_up(dmix, DMIX_IPC_SEM_CLIENT);
	}
	pcm->private_data = NULL;
	free(dmix);
	return 0;
}

static snd_pcm_sframes_t snd_pcm_dmix_mmap_commit(snd_pcm_t *pcm,
						  snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						  snd_pcm_uframes_t size)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	int err;

	snd_pcm_mmap_appl_forward(pcm, size);
	if (dmix->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dmix_sync_ptr(pcm);
		if (err < 0)
			return err;
		/* ok, we commit the changes after the validation of area */
		/* it's intended, although the result might be crappy */
		snd_pcm_dmix_sync_area(pcm, size);
	}
	return size;
}

static snd_pcm_sframes_t snd_pcm_dmix_avail_update(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;
	int err;
	
	if (dmix->state == SND_PCM_STATE_RUNNING) {
		err = snd_pcm_dmix_sync_ptr(pcm);
		if (err < 0)
			return err;
	}
	return snd_pcm_mmap_playback_avail(pcm);
}

static void snd_pcm_dmix_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_dmix_t *dmix = pcm->private_data;

	snd_output_printf(out, "Direct Stream Mixing PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	if (dmix->spcm)
		snd_pcm_dump(dmix->spcm, out);
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
	poll_revents: snd_pcm_dmix_poll_revents,
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
	forward: snd_pcm_dmix_forward,
	resume: snd_pcm_dmix_resume,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
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
	snd_pcm_uframes_t boundary;
	struct pollfd fd;

	hw_params = &dmix->shmptr->hw_params;
	sw_params = &dmix->shmptr->sw_params;

	ret = snd_pcm_hw_params_any(spcm, hw_params);
	if (ret < 0) {
		SNDERR("snd_pcm_hw_params_any failed");
		return ret;
	}
	ret = snd_pcm_hw_params_set_access(spcm, hw_params, SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (ret < 0) {
		ret = snd_pcm_hw_params_set_access(spcm, hw_params, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
		if (ret < 0) {
			SNDERR("slave plugin does not support mmap interleaved or mmap noninterleaved access");
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

	ret = snd_pcm_sw_params_get_boundary(sw_params, &boundary);
	if (ret < 0) {
		SNDERR("unable to get boundary\n");
		return ret;
	}
	ret = snd_pcm_sw_params_set_stop_threshold(spcm, sw_params, boundary);
	if (ret < 0) {
		SNDERR("unable to set stop threshold\n");
		return ret;
	}
	ret = snd_pcm_sw_params_set_silence_threshold(spcm, sw_params, 0);
	if (ret < 0) {
		SNDERR("unable to set silence threshold\n");
		return ret;
	}
	ret = snd_pcm_sw_params_set_silence_size(spcm, sw_params, boundary);
	if (ret < 0) {
		SNDERR("unable to set silence threshold (please upgrade to 0.9.0rc8+ driver)\n");
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

	if (snd_pcm_poll_descriptors_count(spcm) != 1) {
		SNDERR("unable to use hardware pcm with fd more than one!!!\n");
		return ret;
	}
	snd_pcm_poll_descriptors(spcm, &fd, 1);
	dmix->hw_fd = fd.fd;
	
	dmix->shmptr->s.boundary = spcm->boundary;
	dmix->shmptr->s.buffer_size = spcm->buffer_size;
	dmix->shmptr->s.sample_bits = spcm->sample_bits;
	dmix->shmptr->s.channels = spcm->channels;
	dmix->shmptr->s.rate = spcm->rate;
	dmix->shmptr->s.format = spcm->format;
	dmix->shmptr->s.boundary = spcm->boundary;

	spcm->donot_close = 1;
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
	struct pollfd fd;
	
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
	if (snd_timer_poll_descriptors_count(dmix->timer) != 1) {
		SNDERR("unable to use timer with fd more than one!!!\n", name);
		return ret;
	}
	snd_timer_poll_descriptors(dmix->timer, &fd, 1);
	dmix->poll_fd = fd.fd;
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
	if (ret < 0) {
		SNDERR("unable to create IPC semaphore\n");
		goto _err;
	}
	
	ret = semaphore_down(dmix, DMIX_IPC_SEM_CLIENT);
	if (ret < 0) {
		semaphore_discard(dmix);
		goto _err;
	}
		
	first_instance = ret = shm_create_or_connect(dmix);
	if (ret < 0) {
		SNDERR("unable to create IPC shm instance\n");
		goto _err;
	}
		
	pcm->ops = &snd_pcm_dmix_ops;
	pcm->fast_ops = &snd_pcm_dmix_fast_ops;
	pcm->private_data = dmix;
	dmix->state = SND_PCM_STATE_OPEN;

	if (first_instance) {
		ret = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
		if (ret < 0) {
			SNDERR("unable to open slave\n");
			goto _err;
		}
	
		if (snd_pcm_type(spcm) != SND_PCM_TYPE_HW) {
			SNDERR("dmix plugin can be only connected to hw plugin");
			goto _err;
		}
		
		ret = snd_pcm_dmix_initialize_slave(dmix, spcm, params);
		if (ret < 0) {
			SNDERR("unable to initialize slave\n");
			goto _err;
		}

		dmix->spcm = spcm;
		
		ret = server_create(dmix);
		if (ret < 0) {
			SNDERR("unable to create server\n");
			goto _err;
		}

		dmix->shmptr->type = spcm->type;
	} else {
		ret = client_connect(dmix);
		if (ret < 0) {
			SNDERR("unable to connect client\n");
			return ret;
		}
			
		ret = snd_pcm_hw_open_fd(&spcm, "dmix_client", dmix->hw_fd, 0);
		if (ret < 0) {
			SNDERR("unable to open hardware\n");
			goto _err;
		}
		
		spcm->donot_close = 1;
		spcm->setup = 1;
		spcm->buffer_size = dmix->shmptr->s.buffer_size;
		spcm->sample_bits = dmix->shmptr->s.sample_bits;
		spcm->channels = dmix->shmptr->s.channels;
		spcm->format = dmix->shmptr->s.format;
		spcm->boundary = dmix->shmptr->s.boundary;
		ret = snd_pcm_mmap(spcm);
		if (ret < 0) {
			SNDERR("unable to mmap channels\n");
			goto _err;
		}
		dmix->spcm = spcm;
	}

	ret = shm_sum_create_or_connect(dmix);
	if (ret < 0) {
		SNDERR("unabel to initialize sum ring buffer\n");
		goto _err;
	}

	ret = snd_pcm_dmix_initialize_poll_fd(dmix);
	if (ret < 0) {
		SNDERR("unable to initialize poll_fd\n");
		goto _err;
	}

	check_interleave(dmix);
	mix_select_callbacks(dmix);
		
	pcm->poll_fd = dmix->poll_fd;
	pcm->poll_events = POLLIN;	/* it's different than other plugins */
		
	pcm->mmap_rw = 1;
	snd_pcm_set_hw_ptr(pcm, &dmix->hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &dmix->appl_ptr, -1, 0);

	semaphore_up(dmix, DMIX_IPC_SEM_CLIENT);

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
 	if (dmix->shmid_sum >= 0)
 		shm_sum_discard(dmix);
 	if (dmix->shmid >= 0) {
 		if (shm_discard(dmix) > 0) {
		 	if (dmix->semid >= 0) {
 				if (semaphore_discard(dmix) < 0)
 					semaphore_up(dmix, DMIX_IPC_SEM_CLIENT);
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
	ipc_key_add_uid BOOL	# add current uid to unique IPC key
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
	int bsize, psize, ipc_key_add_uid = 0;
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
			continue;
		}
		if (strcmp(id, "ipc_key_add_uid") == 0) {
			char *tmp;
			err = snd_config_get_ascii(n, &tmp);
			if (err < 0) {
				SNDERR("The field ipc_key_add_uid must be a boolean type");
				return err;
			}
			err = snd_config_get_bool_ascii(tmp);
			free(tmp);
			if (err < 0) {
				SNDERR("The field ipc_key_add_uid must be a boolean type");
				return err;
			}
			ipc_key_add_uid = err;
			continue;
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
	if (ipc_key_add_uid)
		ipc_key += getuid();
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

	assert(params.format == SND_PCM_FORMAT_S16); /* sorry, limited features */

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
