/**
 * \file pcm/pcm_direct.c
 * \ingroup PCM_Plugins
 * \brief PCM Direct Stream Mixing (dmix) Plugin Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 */
/*
 *  PCM - Direct Stream Mixing
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
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
#include "pcm_direct.h"

/*
 *
 */
 
 
/*
 * FIXME:
 *  add possibility to use futexes here
 */

int snd_pcm_direct_semaphore_create_or_connect(snd_pcm_direct_t *dmix)
{
	dmix->semid = semget(dmix->ipc_key, DIRECT_IPC_SEMS, IPC_CREAT | 0666);
	if (dmix->semid < 0)
		return -errno;
	return 0;
}

int snd_pcm_direct_semaphore_discard(snd_pcm_direct_t *dmix)
{
	if (dmix->semid < 0)
		return -EINVAL;
	if (semctl(dmix->semid, 0, IPC_RMID, NULL) < 0)
		return -errno;
	dmix->semid = -1;
	return 0;
}

int snd_pcm_direct_semaphore_down(snd_pcm_direct_t *dmix, int sem_num)
{
	struct sembuf op[2] = { { 0, 0, SEM_UNDO }, { 0, 1, SEM_UNDO | IPC_NOWAIT } };
	assert(dmix->semid >= 0);
	op[0].sem_num = sem_num;
	op[1].sem_num = sem_num;
	if (semop(dmix->semid, op, 2) < 0)
		return -errno;
	return 0;
}

int snd_pcm_direct_semaphore_up(snd_pcm_direct_t *dmix, int sem_num)
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

int snd_pcm_direct_shm_create_or_connect(snd_pcm_direct_t *dmix)
{
	static int snd_pcm_direct_shm_discard(snd_pcm_direct_t *dmix);
	struct shmid_ds buf;
	int ret = 0;
	
	dmix->shmid = shmget(dmix->ipc_key, sizeof(snd_pcm_direct_share_t), IPC_CREAT | 0666);
	if (dmix->shmid < 0)
		return -errno;
	dmix->shmptr = shmat(dmix->shmid, 0, 0);
	if (dmix->shmptr == (void *) -1) {
		snd_pcm_direct_shm_discard(dmix);
		return -errno;
	}
	mlock(dmix->shmptr, sizeof(snd_pcm_direct_share_t));
	if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0) {
		snd_pcm_direct_shm_discard(dmix);
		return -errno;
	}
	if (buf.shm_nattch == 1) {	/* we're the first user, clear the segment */
		memset(dmix->shmptr, 0, sizeof(snd_pcm_direct_share_t));
		ret = 1;
	}
	return ret;
}

int snd_pcm_direct_shm_discard(snd_pcm_direct_t *dmix)
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

#if 0
#define server_printf(fmt, args...) printf(fmt, ##args)
#else
#define server_printf(fmt, args...) /* nothing */
#endif

static void server_job(snd_pcm_direct_t *dmix)
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

	server_printf("DIRECT SERVER STARTED\n");
	while (1) {
		ret = poll(pfds, current + 1, 500);
		server_printf("DIRECT SERVER: poll ret = %i, revents[0] = 0x%x\n", ret, pfds[0].revents);
		if (ret < 0)	/* some error */
			break;
		if (ret == 0 || pfds[0].revents & (POLLERR | POLLHUP)) {	/* timeout or error? */
			struct shmid_ds buf;
			snd_pcm_direct_semaphore_down(dmix, DIRECT_IPC_SEM_CLIENT);
			if (shmctl(dmix->shmid, IPC_STAT, &buf) < 0) {
				snd_pcm_direct_shm_discard(dmix);
				continue;
			}
			server_printf("DIRECT SERVER: nattch = %i\n", (int)buf.shm_nattch);
			if (buf.shm_nattch == 1)	/* server is the last user, exit */
				break;
			snd_pcm_direct_semaphore_up(dmix, DIRECT_IPC_SEM_CLIENT);
			continue;
		}
		if (pfds[0].revents & POLLIN) {
			ret--;
			sck = accept(dmix->server_fd, 0, 0);
			if (sck >= 0) {
				server_printf("DIRECT SERVER: new connection %i\n", sck);
				if (current == max) {
					close(sck);
				} else {
					unsigned char buf = 'A';
					pfds[current+1].fd = sck;
					pfds[current+1].events = POLLIN | POLLERR | POLLHUP;
					snd_send_fd(sck, &buf, 1, dmix->hw_fd);
					server_printf("DIRECT SERVER: fd sent ok\n");
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
	snd_pcm_direct_shm_discard(dmix);
	snd_pcm_direct_semaphore_discard(dmix);
	server_printf("DIRECT SERVER EXIT\n");
	exit(EXIT_SUCCESS);
}

int snd_pcm_direct_server_create(snd_pcm_direct_t *dmix)
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

int snd_pcm_direct_server_discard(snd_pcm_direct_t *dmix)
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

int snd_pcm_direct_client_connect(snd_pcm_direct_t *dmix)
{
	int ret;
	unsigned char buf;

	ret = make_local_socket(dmix->shmptr->socket_name, 0);
	if (ret < 0)
		return ret;
	dmix->comm_fd = ret;

	ret = snd_receive_fd(dmix->comm_fd, &buf, 1, &dmix->hw_fd);
	if (ret < 1 || buf != 'A') {
		close(dmix->comm_fd);
		dmix->comm_fd = -1;
		return ret;
	}

	dmix->client = 1;
	return 0;
}

int snd_pcm_direct_client_discard(snd_pcm_direct_t *dmix)
{
	if (dmix->client) {
		close(dmix->comm_fd);
		dmix->comm_fd = -1;
	}
	return 0;
}

/*
 * this function initializes hardware and starts playback operation with
 * no stop threshold (it operates all time without xrun checking)
 * also, the driver silences the unused ring buffer areas for us
 */
int snd_pcm_direct_initialize_slave(snd_pcm_direct_t *dmix, snd_pcm_t *spcm, struct slave_params *params)
{
	snd_pcm_hw_params_t *hw_params;
	snd_pcm_sw_params_t *sw_params;
	int ret, buffer_is_not_initialized;
	snd_pcm_uframes_t boundary;
	struct pollfd fd;
	int loops = 10;

	hw_params = &dmix->shmptr->hw_params;
	sw_params = &dmix->shmptr->sw_params;

      __again:
      	if (loops-- <= 0) {
      		SNDERR("unable to find a valid configuration for slave");
      		return -EINVAL;
      	}
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
		snd_pcm_format_t format;
		switch (params->format) {
		case SND_PCM_FORMAT_S32: format = SND_PCM_FORMAT_S16; break;
		case SND_PCM_FORMAT_S16: format = SND_PCM_FORMAT_S32; break;
		default:
			SNDERR("invalid format");
			return -EINVAL;
		}
		ret = snd_pcm_hw_params_set_format(spcm, hw_params, format);
		if (ret < 0) {
			SNDERR("requested or auto-format is not available");
			return ret;
		}
		params->format = format;
	}
	ret = snd_pcm_hw_params_set_channels(spcm, hw_params, params->channels);
	if (ret < 0) {
		unsigned int min, max;
		ret = INTERNAL(snd_pcm_hw_params_get_channels_min)(hw_params, &min);
		if (ret < 0) {
			SNDERR("cannot obtain minimal count of channels");
			return ret;
		}
		ret = INTERNAL(snd_pcm_hw_params_get_channels_min)(hw_params, &max);
		if (ret < 0) {
			SNDERR("cannot obtain maximal count of channels");
			return ret;
		}
		if (min == max) {
			ret = snd_pcm_hw_params_set_channels(spcm, hw_params, min);
			if (ret >= 0)
				params->channels = min;
		}
		if (ret < 0) {
			SNDERR("requested count of channels is not available");
			return ret;
		}
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
		int periods = params->periods;
		ret = INTERNAL(snd_pcm_hw_params_set_periods_near)(spcm, hw_params, &params->periods, 0);
		if (ret < 0) {
			SNDERR("unable to set requested periods");
			return ret;
		}
		if (params->periods == 1) {
			params->periods = periods;
			if (params->period_time > 0) {
				params->period_time /= 2;
				goto __again;
			} else if (params->period_size > 0) {
				params->period_size /= 2;
				goto __again;
			}
			SNDERR("unable to use stream with periods == 1");
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
		SNDERR("unable to get boundary");
		return ret;
	}
	ret = snd_pcm_sw_params_set_stop_threshold(spcm, sw_params, boundary);
	if (ret < 0) {
		SNDERR("unable to set stop threshold");
		return ret;
	}
	ret = snd_pcm_sw_params_set_silence_threshold(spcm, sw_params, 0);
	if (ret < 0) {
		SNDERR("unable to set silence threshold");
		return ret;
	}
	ret = snd_pcm_sw_params_set_silence_size(spcm, sw_params, boundary);
	if (ret < 0) {
		SNDERR("unable to set silence threshold (please upgrade to 0.9.0rc8+ driver)");
		return ret;
	}

	ret = snd_pcm_sw_params(spcm, sw_params);
	if (ret < 0) {
		SNDERR("unable to install sw params (please upgrade to 0.9.0rc8+ driver)");
		return ret;
	}
	
	ret = snd_pcm_start(spcm);
	if (ret < 0) {
		SNDERR("unable to start PCM stream");
		return ret;
	}

	if (snd_pcm_poll_descriptors_count(spcm) != 1) {
		SNDERR("unable to use hardware pcm with fd more than one!!!");
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
int snd_pcm_direct_initialize_poll_fd(snd_pcm_direct_t *dmix)
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
		SNDERR("unable to info for slave pcm");
		return ret;
	}
	sprintf(name, "hw:CLASS=%i,SCLASS=0,CARD=%i,DEV=%i,SUBDEV=%i",
				(int)SND_TIMER_CLASS_PCM, 
				snd_pcm_info_get_card(info),
				snd_pcm_info_get_device(info),
				snd_pcm_info_get_subdevice(info) * 2);	/* it's a bit trick to distict playback and capture */
	ret = snd_timer_open(&dmix->timer, name, SND_TIMER_OPEN_NONBLOCK);
	if (ret < 0) {
		SNDERR("unable to open timer '%s'", name);
		return ret;
	}
	snd_timer_params_set_auto_start(params, 1);
	snd_timer_params_set_ticks(params, 1);
	ret = snd_timer_params(dmix->timer, params);
	if (ret < 0) {
		SNDERR("unable to set timer parameters", name);
                return ret;
	}
	if (snd_timer_poll_descriptors_count(dmix->timer) != 1) {
		SNDERR("unable to use timer with fd more than one!!!", name);
		return ret;
	}
	snd_timer_poll_descriptors(dmix->timer, &fd, 1);
	dmix->poll_fd = fd.fd;
	return 0;
}

/*
 *  ring buffer operation
 */
int snd_pcm_direct_check_interleave(snd_pcm_direct_t *dmix, snd_pcm_t *pcm)
{
	unsigned int chn, channels;
	int interleaved = 1;
	const snd_pcm_channel_area_t *dst_areas;
	const snd_pcm_channel_area_t *src_areas;

	channels = dmix->u.dmix.channels;
	dst_areas = snd_pcm_mmap_areas(dmix->spcm);
	src_areas = snd_pcm_mmap_areas(pcm);
	for (chn = 1; chn < channels; chn++) {
		if (dst_areas[chn-1].addr != dst_areas[chn].addr) {
			interleaved = 0;
			break;
		}
		if (src_areas[chn-1].addr != src_areas[chn].addr) {
			interleaved = 0;
			break;
		}
	}
	for (chn = 0; chn < channels; chn++) {
		if (dmix->u.dmix.bindings && dmix->u.dmix.bindings[chn] != chn) {
			interleaved = 0;
			break;
		}
		if (dst_areas[chn].first != sizeof(signed short) * chn * 8 ||
		    dst_areas[chn].step != channels * sizeof(signed short) * 8) {
			interleaved = 0;
			break;
		}
		if (src_areas[chn].first != sizeof(signed short) * chn * 8 ||
		    src_areas[chn].step != channels * sizeof(signed short) * 8) {
			interleaved = 0;
			break;
		}
	}
	return dmix->interleaved = interleaved;
}
