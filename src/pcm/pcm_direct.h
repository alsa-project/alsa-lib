/**
 * \file pcm/pcm_dmix.c
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

#include "pcm_local.h"  

#define DIRECT_IPC_SEMS         1
#define DIRECT_IPC_SEM_CLIENT   0

typedef void (mix_areas1_t)(unsigned int size,
			volatile signed short *dst, signed short *src,
			volatile signed int *sum, size_t dst_step,
			size_t src_step, size_t sum_step);

typedef void (mix_areas2_t)(unsigned int size,
			volatile signed int *dst, signed int *src,
			volatile signed int *sum, size_t dst_step,
			size_t src_step, size_t sum_step);

struct slave_params {
	snd_pcm_format_t format;
	int rate;
	int channels;
	int period_time;
	int buffer_time;
	snd_pcm_sframes_t period_size;
	snd_pcm_sframes_t buffer_size;
	int periods;
};

typedef struct {
	char socket_name[256];			/* name of communication socket */
	snd_pcm_type_t type;			/* PCM type (currently only hw) */
	snd_pcm_hw_params_t hw_params;
	snd_pcm_sw_params_t sw_params;
	struct {
		snd_pcm_uframes_t buffer_size;
		snd_pcm_uframes_t boundary;
		snd_pcm_uframes_t channels;
		unsigned int sample_bits;
		unsigned int rate;
		snd_pcm_format_t format;
		unsigned int info;
		unsigned int msbits;
	} s;
	union {
		struct {
			unsigned long long chn_mask;
		} dshare;
	} u;
} snd_pcm_direct_share_t;

typedef struct snd_pcm_direct snd_pcm_direct_t;

struct snd_pcm_direct {
	snd_pcm_type_t type;		/* type (dmix, dsnoop, dshare) */
	key_t ipc_key;			/* IPC key for semaphore and memory */
	mode_t ipc_perm;		/* IPC socket permissions */
	int semid;			/* IPC global semaphore identification */
	int shmid;			/* IPC global shared memory identification */
	snd_pcm_direct_share_t *shmptr;	/* pointer to shared memory area */
	snd_pcm_t *spcm; 		/* slave PCM handle */
	snd_pcm_uframes_t appl_ptr;
	snd_pcm_uframes_t hw_ptr;
	snd_pcm_uframes_t avail_max;
	snd_pcm_uframes_t slave_appl_ptr;
	snd_pcm_uframes_t slave_hw_ptr;
	int (*sync_ptr)(snd_pcm_t *pcm);
	snd_pcm_state_t state;
	snd_htimestamp_t trigger_tstamp;
	int server, client;
	int comm_fd;			/* communication file descriptor (socket) */
	int hw_fd;			/* hardware file descriptor */
	int poll_fd;
	int server_fd;
	pid_t server_pid;
	snd_timer_t *timer; 		/* timer used as poll_fd */
	int interleaved;	 	/* we have interleaved buffer */
	int slowptr;			/* use slow but more precise ptr updates */
	unsigned int channels;		/* client's channels */
	unsigned int *bindings;
	union {
		struct {
			int shmid_sum;			/* IPC global sum ring buffer memory identification */
			signed int *sum_buffer;		/* shared sum buffer */
			mix_areas1_t *mix_areas1;
			mix_areas2_t *mix_areas2;
		} dmix;
		struct {
		} dsnoop;
		struct {
			unsigned long long chn_mask;
		} dshare;
	} u;
	void (*server_free)(snd_pcm_direct_t *direct);
};

int snd_pcm_direct_semaphore_create_or_connect(snd_pcm_direct_t *dmix);
int snd_pcm_direct_semaphore_discard(snd_pcm_direct_t *dmix);
int snd_pcm_direct_semaphore_down(snd_pcm_direct_t *dmix, int sem_num);
int snd_pcm_direct_semaphore_up(snd_pcm_direct_t *dmix, int sem_num);
int snd_pcm_direct_shm_create_or_connect(snd_pcm_direct_t *dmix);
int snd_pcm_direct_shm_discard(snd_pcm_direct_t *dmix);
int snd_pcm_direct_server_create(snd_pcm_direct_t *dmix);
int snd_pcm_direct_server_discard(snd_pcm_direct_t *dmix);
int snd_pcm_direct_client_connect(snd_pcm_direct_t *dmix);
int snd_pcm_direct_client_discard(snd_pcm_direct_t *dmix);
int snd_pcm_direct_initialize_slave(snd_pcm_direct_t *dmix, snd_pcm_t *spcm, struct slave_params *params);
int snd_pcm_direct_initialize_poll_fd(snd_pcm_direct_t *dmix);
int snd_pcm_direct_check_interleave(snd_pcm_direct_t *dmix, snd_pcm_t *pcm);
int snd_pcm_direct_parse_bindings(snd_pcm_direct_t *dmix, snd_config_t *cfg);
int snd_pcm_direct_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_pcm_direct_async(snd_pcm_t *pcm, int sig, pid_t pid);
int snd_pcm_direct_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents);
int snd_pcm_direct_info(snd_pcm_t *pcm, snd_pcm_info_t * info);
int snd_pcm_direct_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_direct_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params);
int snd_pcm_direct_hw_free(snd_pcm_t *pcm);
int snd_pcm_direct_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params);
int snd_pcm_direct_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info);
int snd_pcm_direct_mmap(snd_pcm_t *pcm);
int snd_pcm_direct_munmap(snd_pcm_t *pcm);

int snd_timer_async(snd_timer_t *timer, int sig, pid_t pid);
struct timespec snd_pcm_hw_fast_tstamp(snd_pcm_t *pcm);
