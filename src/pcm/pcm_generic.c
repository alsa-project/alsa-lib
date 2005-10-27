/**
 * \file pcm/pcm_generic.c
 * \ingroup PCM
 * \brief PCM Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2004
 */
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

#include <sys/shm.h>
#include <sys/ioctl.h>
#include <limits.h>
#include "pcm_local.h"
#include "pcm_generic.h"

#ifndef DOC_HIDDEN

int snd_pcm_generic_close(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	int err = 0;
	if (generic->close_slave)
		err = snd_pcm_close(generic->slave);
	free(generic);
	return 0;
}

int snd_pcm_generic_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_nonblock(generic->slave, nonblock);
}

int snd_pcm_generic_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_async(generic->slave, sig, pid);
}

int snd_pcm_generic_poll_descriptors_count(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_poll_descriptors_count(generic->slave);
}

int snd_pcm_generic_poll_descriptors(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int space)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_poll_descriptors(generic->slave, pfds, space);
}

int snd_pcm_generic_poll_revents(snd_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_poll_descriptors_revents(generic->slave, pfds, nfds, revents);
}

int snd_pcm_generic_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_info(generic->slave, info);
}

int snd_pcm_generic_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_hw_free(generic->slave);
}

int snd_pcm_generic_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_sw_params(generic->slave, params);
}

int snd_pcm_generic_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_hw_refine(generic->slave, params);
}

int snd_pcm_generic_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return _snd_pcm_hw_params(generic->slave, params);
}

int snd_pcm_generic_prepare(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_prepare(generic->slave);
}

int snd_pcm_generic_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t *info)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	if (pcm->mmap_shadow) {
		/* No own buffer is required - the plugin won't change
		 * the data on the buffer, or do safely on-the-place
		 * conversion
		 */
		return snd_pcm_channel_info(generic->slave, info);
	} else {
		/* Allocate own buffer */
		return snd_pcm_channel_info_shm(pcm, info, -1);
	}
}

int snd_pcm_generic_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{ 
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_status(generic->slave, status);
}

snd_pcm_state_t snd_pcm_generic_state(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_state(generic->slave);
}

int snd_pcm_generic_hwsync(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_hwsync(generic->slave);
}

int snd_pcm_generic_reset(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_reset(generic->slave);
}

int snd_pcm_generic_start(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_start(generic->slave);
}

int snd_pcm_generic_drop(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_drop(generic->slave);
}

int snd_pcm_generic_drain(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_drain(generic->slave);
}

int snd_pcm_generic_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_pause(generic->slave, enable);
}

int snd_pcm_generic_resume(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_resume(generic->slave);
}

int snd_pcm_generic_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_delay(generic->slave, delayp);
}

snd_pcm_sframes_t snd_pcm_generic_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return INTERNAL(snd_pcm_forward)(generic->slave, frames);
}

snd_pcm_sframes_t snd_pcm_generic_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_rewind(generic->slave, frames);
}

int snd_pcm_generic_link_fd(snd_pcm_t *pcm, int *fds, int count, int (**failed)(snd_pcm_t *, int))
{
	snd_pcm_generic_t *generic = pcm->private_data;
	if (generic->slave->fast_ops->link_fd)
		return generic->slave->fast_ops->link_fd(generic->slave->fast_op_arg, fds, count, failed);
	return -ENOSYS;
}

int snd_pcm_generic_link(snd_pcm_t *pcm1, snd_pcm_t *pcm2)
{
	snd_pcm_generic_t *generic = pcm1->private_data;
	if (generic->slave->fast_ops->link)
		return generic->slave->fast_ops->link(generic->slave->fast_op_arg, pcm2);
	return -ENOSYS;
}

int snd_pcm_generic_link2(snd_pcm_t *pcm1, snd_pcm_t *pcm2)
{
	int fds1[16], fds2[16];
	int (*failed1)(snd_pcm_t *, int) = NULL;
	int (*failed2)(snd_pcm_t *, int) = NULL;
	int count1 = _snd_pcm_link_descriptors(pcm1, fds1, 16, &failed1);
	int count2 = _snd_pcm_link_descriptors(pcm2, fds2, 16, &failed2);
	int i, err = 0;

	if (count1 < 0)
		return count1;
	if (count2 < 0)
		return count2;
	for (i = 1; i < count1; i++) {
		if (fds1[i] < 0)
			return 0;
		if (ioctl(fds1[0], SNDRV_PCM_IOCTL_LINK, fds1[i]) < 0) {
			if (failed1 != NULL) {
				err = failed1(pcm2, fds1[i]);
			} else {
				SYSMSG("SNDRV_PCM_IOCTL_LINK failed");
				err = -errno;
			}
		}
	}
	for (i = 0; i < count2; i++) {
		if (fds2[i] < 0)
			return 0;
		if (ioctl(fds1[0], SNDRV_PCM_IOCTL_LINK, fds2[i]) < 0) {
			if (failed1 != NULL) {
				err = failed2(pcm2, fds2[i]);
			} else {
				SYSMSG("SNDRV_PCM_IOCTL_LINK failed");
				err = -errno;
			}
		}
	}
	return err;
}

int snd_pcm_generic_unlink(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	if (generic->slave->fast_ops->unlink)
		return generic->slave->fast_ops->unlink(generic->slave->fast_op_arg);
	return -ENOSYS;
}

snd_pcm_sframes_t snd_pcm_generic_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_writei(generic->slave, buffer, size);
}

snd_pcm_sframes_t snd_pcm_generic_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_writen(generic->slave, bufs, size);
}

snd_pcm_sframes_t snd_pcm_generic_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_readi(generic->slave, buffer, size);
}

snd_pcm_sframes_t snd_pcm_generic_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_readn(generic->slave, bufs, size);
}

snd_pcm_sframes_t snd_pcm_generic_mmap_commit(snd_pcm_t *pcm, 
					      snd_pcm_uframes_t offset,
					      snd_pcm_uframes_t size)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_mmap_commit(generic->slave, offset, size);
}

snd_pcm_sframes_t snd_pcm_generic_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_generic_t *generic = pcm->private_data;
	return snd_pcm_avail_update(generic->slave);
}

int snd_pcm_generic_mmap(snd_pcm_t *pcm)
{
	if (pcm->mmap_shadow) {
		/* Copy the slave mmapped buffer data */
		snd_pcm_generic_t *generic = pcm->private_data;
		pcm->mmap_channels = generic->slave->mmap_channels;
		pcm->running_areas = generic->slave->running_areas;
		pcm->stopped_areas = generic->slave->stopped_areas;
	}
	return 0;
}

int snd_pcm_generic_munmap(snd_pcm_t *pcm)
{
	if (pcm->mmap_shadow) {
		/* Clean up */
		pcm->mmap_channels = NULL;
		pcm->running_areas = NULL;
		pcm->stopped_areas = NULL;
	}
	return 0;
}

#endif /* DOC_HIDDEN */
