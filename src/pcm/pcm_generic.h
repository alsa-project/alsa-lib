/*
 *  PCM - Common generic plugin code
 *  Copyright (c) 2004 by Jaroslav Kysela <perex@suse.cz>
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
  
typedef struct {
	snd_pcm_t *slave;
	int close_slave;
} snd_pcm_generic_t;	

int snd_pcm_generic_close(snd_pcm_t *pcm);
int snd_pcm_generic_nonblock(snd_pcm_t *pcm, int nonblock);
int snd_pcm_generic_async(snd_pcm_t *pcm, int sig, pid_t pid);
int snd_pcm_generic_poll_descriptors_count(snd_pcm_t *pcm);
int snd_pcm_generic_poll_descriptors(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int space);
int snd_pcm_generic_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents);
int snd_pcm_generic_info(snd_pcm_t *pcm, snd_pcm_info_t * info);
int snd_pcm_generic_hw_free(snd_pcm_t *pcm);
int snd_pcm_generic_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params);
int snd_pcm_generic_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_generic_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
int snd_pcm_generic_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info);
int snd_pcm_generic_channel_info_no_buffer(snd_pcm_t *pcm, snd_pcm_channel_info_t * info);
int snd_pcm_generic_status(snd_pcm_t *pcm, snd_pcm_status_t * status);
snd_pcm_state_t snd_pcm_generic_state(snd_pcm_t *pcm);
int snd_pcm_generic_prepare(snd_pcm_t *pcm);
int snd_pcm_generic_hwsync(snd_pcm_t *pcm);
int snd_pcm_generic_reset(snd_pcm_t *pcm);
int snd_pcm_generic_start(snd_pcm_t *pcm);
int snd_pcm_generic_drop(snd_pcm_t *pcm);
int snd_pcm_generic_drain(snd_pcm_t *pcm);
int snd_pcm_generic_pause(snd_pcm_t *pcm, int enable);
int snd_pcm_generic_resume(snd_pcm_t *pcm);
int snd_pcm_generic_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp);
snd_pcm_sframes_t snd_pcm_generic_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
snd_pcm_sframes_t snd_pcm_generic_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames);
int snd_pcm_generic_link_fd(snd_pcm_t *pcm, int *fds, int count, int (**failed)(snd_pcm_t *, int)); 
int snd_pcm_generic_link(snd_pcm_t *pcm1, snd_pcm_t *pcm2);
int snd_pcm_generic_link2(snd_pcm_t *pcm1, snd_pcm_t *pcm2);
int snd_pcm_generic_unlink(snd_pcm_t *pcm);
snd_pcm_sframes_t snd_pcm_generic_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_generic_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_generic_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_generic_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_generic_mmap_commit(snd_pcm_t *pcm,
					      snd_pcm_uframes_t offset,
					      snd_pcm_uframes_t size);
snd_pcm_sframes_t snd_pcm_generic_avail_update(snd_pcm_t *pcm);
int snd_pcm_generic_mmap(snd_pcm_t *pcm);
int snd_pcm_generic_munmap(snd_pcm_t *pcm);
