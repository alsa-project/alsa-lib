/**
 * \file ordinary_pcm/ordinary_pcm.c
 * \ingroup PCM_ordinary
 * \brief Ordinary PCM interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003,2004
 *
 * Ordinary PCM interface is a high level abtraction for
 * digital audio streaming.
 *
 * See the \ref PCM_ordinary page for more details.
 */
/*
 *  Ordinary PCM Interface - main file
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
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

/*! \page PCM_ordinary Ordinary PCM interface

<P>Write something here</P>

\section PCM_ordinary_overview

Write something here

*/
/**
 * \example ../test/ordinary_pcm.c
 * \anchor example_ordinary_pcm
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdarg.h>
#include <signal.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <limits.h>
#include "asoundlib.h"
#include "alisp.h"
#include "pcm_ordinary.h"

struct sndo_pcm {
	snd_pcm_t *playback;
	snd_pcm_t *capture;
	snd_pcm_hw_params_t *p_hw_params;
	snd_pcm_hw_params_t *c_hw_params;
	snd_pcm_sw_params_t *p_sw_params;
	snd_pcm_sw_params_t *c_sw_params;
	snd_pcm_t *master;
	unsigned int channels;
	unsigned int samplebytes;
	snd_pcm_uframes_t p_offset;
	snd_pcm_uframes_t c_offset;
	snd_pcm_uframes_t p_period_size;
	snd_pcm_uframes_t c_period_size;
	snd_pcm_uframes_t transfer_block;
	snd_pcm_uframes_t ring_size;
	enum sndo_pcm_latency_type latency;
	enum sndo_pcm_xrun_type xrun;
	int setting_up;
	int initialized;
};

static int sndo_pcm_setup(sndo_pcm_t *pcm);
static int sndo_pcm_initialize(sndo_pcm_t *pcm);

static inline int sndo_pcm_check_setup(sndo_pcm_t *pcm)
{
	if (!pcm->initialized)
		return sndo_pcm_initialize(pcm);
	return 0;
}

/**
 * \brief Opens a ordinary pcm instance
 * \param ppcm Returned ordinary pcm handle
 * \param playback_name ASCII identifier of the ordinary pcm handle (playback)
 * \param capture_name ASCII identifier of the ordinary pcm handle (capture)
 * \param lconf Local configuration (might be NULL - use global configuration)
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_open(sndo_pcm_t **ppcm,
		  const char *playback_name,
		  const char *capture_name,
		  struct alisp_cfg *lconf)
{
	int err = 0;
	sndo_pcm_t *pcm;
	
	assert(ppcm);
	assert(playback_name || capture_name);
	*ppcm = NULL;
	pcm = calloc(1, sizeof(sndo_pcm_t));
	if (pcm == NULL)
		return -ENOMEM;
	if (playback_name) {
		err = snd_pcm_hw_params_malloc(&pcm->p_hw_params);
		if (err < 0)
			goto __end;
		err = snd_pcm_sw_params_malloc(&pcm->p_sw_params);
	}
	if (capture_name) {
		err = snd_pcm_hw_params_malloc(&pcm->c_hw_params);
		if (err < 0)
			goto __end;
		err = snd_pcm_sw_params_malloc(&pcm->p_sw_params);
	}
	if (err < 0)
		goto __end;
	if (lconf) {
		if (playback_name) {
			err = snd_pcm_open_lconf(&pcm->playback, playback_name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK, NULL);
			if (err < 0)
				goto __end;
		}
		if (capture_name) {
			err = snd_pcm_open_lconf(&pcm->capture, playback_name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK, NULL);
			if (err < 0)
				goto __end;
		}
	} else {
		if (playback_name) {
			err = snd_pcm_open(&pcm->playback, playback_name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
			if (err < 0)
				goto __end;
		}
		if (capture_name) {
			err = snd_pcm_open(&pcm->capture, playback_name, SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
			if (err < 0)
				goto __end;
		}
	}
	if (pcm->playback && pcm->capture) {
		err = snd_pcm_link(pcm->playback, pcm->capture);
		if (err < 0)
			goto __end;
		pcm->master = pcm->playback;
	}
      __end:
	if (err < 0)
		sndo_pcm_close(pcm);
	return err;
}

/**
 * \brief Closes a ordinary pcm instance
 * \param pcm Ordinary pcm handle to close
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_close(sndo_pcm_t *pcm)
{
	int err;

	if (pcm->playback)
		err = snd_pcm_close(pcm->playback);
	if (pcm->capture)
		err = snd_pcm_close(pcm->capture);
	if (pcm->p_hw_params)
		snd_pcm_hw_params_free(pcm->p_hw_params);
	if (pcm->p_sw_params)
		snd_pcm_sw_params_free(pcm->p_sw_params);
	if (pcm->c_hw_params)
		snd_pcm_hw_params_free(pcm->c_hw_params);
	if (pcm->c_sw_params)
		snd_pcm_sw_params_free(pcm->c_sw_params);
	free(pcm);
	return 0;
}

/**
 * \brief get count of poll descriptors for ordinary pcm handle
 * \param pcm ordinary pcm handle
 * \return count of poll descriptors
 */
int sndo_pcm_poll_descriptors_count(sndo_pcm_t *pcm)
{
	int err, res = 0;
	
	err = snd_pcm_poll_descriptors_count(pcm->playback);
	if (err > 0)
		res += err;
	err = snd_pcm_poll_descriptors_count(pcm->capture);
	if (err > 0)
		res += err;
	return err < 0 ? err : res;
}

/**
 * \brief get poll descriptors
 * \param pcm ordinary pcm handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */     
int sndo_pcm_poll_descriptors(sndo_pcm_t *pcm, struct pollfd *pfds, unsigned int space)
{
	int playback, err, res = 0;

	playback = snd_pcm_poll_descriptors_count(pcm->playback);
	if (playback < 0)
		return playback;
	err = snd_pcm_poll_descriptors(pcm->playback, pfds, (unsigned)playback < space ? (unsigned)playback : space);
	if (err < 0)
		return err;
	res += err;
	if ((unsigned)res < space) {
		err = snd_pcm_poll_descriptors(pcm->capture, pfds + res, space - res);
		if (err < 0)
			return err;
		res += err;
	}
	return res;
}

/**
 * \brief get returned events from poll descriptors
 * \param pcm ordinary pcm handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents returned events
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_pcm_poll_descriptors_revents(sndo_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	int playback, err;
	unsigned short _revents;

	playback = snd_pcm_poll_descriptors_count(pcm->playback);
	if (playback < 0)
		return playback;
	err = snd_pcm_poll_descriptors_revents(pcm->playback, pfds, nfds < (unsigned)playback ? nfds : (unsigned)playback, revents);
	if (err < 0)
		return err;
	if (nfds > (unsigned)playback) {
		err = snd_pcm_poll_descriptors_revents(pcm->capture, pfds + playback, nfds - playback, &_revents);
		if (err < 0)
			return err;
		*revents |= _revents;
	}
	return 0;
}

/**
 * \brief Start a PCM
 * \param pcm ordinary PCM handle
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_start(sndo_pcm_t *pcm)
{
	int err;

	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	/* the streams are linked, so use only one stream */
	return snd_pcm_start(pcm->master);
}

/**
 * \brief Stop a PCM dropping pending frames
 * \param pcm ordinary PCM handle
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_drop(sndo_pcm_t *pcm)
{
	/* the streams are linked, so use only one stream */
	return snd_pcm_drop(pcm->master);
}

/**
 * \brief Stop a PCM preserving pending frames
 * \param pcm PCM handle
 * \return 0 on success otherwise a negative error code
 * \retval -ESTRPIPE a suspend event occurred
 *
 * For playback wait for all pending frames to be played and then stop
 * the PCM.
 * For capture stop PCM permitting to retrieve residual frames.
 */
int sndo_pcm_drain(sndo_pcm_t *pcm)
{
	/* the streams are linked, so use only one stream */
	return snd_pcm_drain(pcm->master);
}

/**
 * \brief Obtain delay for a running PCM handle
 * \param pcm ordinary PCM handle
 * \param delayp Returned delay in frames
 * \return 0 on success otherwise a negative error code
 *
 * Delay is distance between current application frame position and
 * sound frame position.
 * It's positive and less than buffer size in normal situation,
 * negative on playback underrun and greater than buffer size on
 * capture overrun.
 */
int sndo_pcm_delay(sndo_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	int err;
	snd_pcm_sframes_t pdelay, cdelay;

	assert(pcm);
	assert(delayp);
	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	if (pcm->playback)
		err = snd_pcm_avail_update(pcm->playback);
	if (err >= 0 && pcm->capture)
		err = snd_pcm_avail_update(pcm->capture);
	if (err >= 0 && pcm->playback)
		err = snd_pcm_delay(pcm->playback, &pdelay);
	if (err >= 0 && pcm->capture)
		err = snd_pcm_delay(pcm->capture, &cdelay);
	if (pdelay > cdelay)
		pdelay = cdelay;
	*delayp = pdelay;
	return err;
}

/**
 * \brief Obtain transfer block size (aka period size)
 * \param pcm ordinary PCM handle
 * \param tblock Returned transfer block size in frames
 * \return 0 on success otherwise a negative error code
 *
 * All read/write operations must use this transfer block.
 */
int sndo_pcm_transfer_block(sndo_pcm_t *pcm, snd_pcm_uframes_t *tblock)
{
	int err;

	assert(pcm);
	assert(tblock);
	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	*tblock = pcm->transfer_block;
	return 0;
}

/**
 * \brief Resume from suspend, no samples are lost
 * \param pcm ordinary PCM handle
 * \return 0 on success otherwise a negative error code
 * \retval -EAGAIN resume can't be proceed immediately (audio hardware is probably still suspended)
 * \retval -ENOSYS hardware doesn't support this feature
 *
 * This function can be used when the stream is in the suspend state
 * to do the fine resume from this state. Not all hardware supports
 * this feature, when an -ENOSYS error is returned, use the snd_pcm_prepare
 * function to recovery.
 */
int sndo_pcm_resume(sndo_pcm_t *pcm)
{
	return snd_pcm_resume(pcm->master);
}

/**
 * \brief Wait for a PCM to become ready
 * \param pcm ordinary PCM handle
 * \param timeout maximum time in milliseconds to wait
 * \return a positive value on success otherwise a negative error code
 * \retval 0 timeout occurred
 * \retval 1 PCM stream is ready for I/O
 */
int sndo_pcm_wait(sndo_pcm_t *pcm, int timeout)
{
        struct pollfd pfd[2];
        unsigned short p_revents, c_revents;
        int err;
        err = snd_pcm_poll_descriptors(pcm->playback, &pfd[0], 1);
        assert(err == 1);
        err = snd_pcm_poll_descriptors(pcm->capture, &pfd[1], 1);
        assert(err == 1);
        err = poll(pfd, 2, timeout);
        if (err < 0)
                return -errno;
	if (err == 0)
		return 0;
	do {
	        err = snd_pcm_poll_descriptors_revents(pcm->playback, &pfd[0], 1, &p_revents);
		if (err < 0)
			return err;
		if (p_revents & (POLLERR | POLLNVAL))
			return -EIO;
		err = snd_pcm_poll_descriptors_revents(pcm->capture, &pfd[1], 1, &c_revents);
		if (err < 0)
			return err;
		if (c_revents & (POLLERR | POLLNVAL))
			return -EIO;
		if ((p_revents & POLLOUT) && (c_revents & POLLIN))
			return 1;
		err = poll(&pfd[(p_revents & POLLOUT) ? 1 : 0], 1, 1);
		if (err < 0)
			return err;
	} while (1);
}

/**
 * \brief Get raw (lowlevel) playback PCM handle
 * \param pcm ordinary PCM handle
 * \return raw (lowlevel) capture PCM handle or NULL
 */
snd_pcm_t *sndo_pcm_raw_playback(sndo_pcm_t *pcm)
{
	return pcm->playback;
}

/**
 * \brief Get raw (lowlevel) capture PCM handle
 * \param pcm ordinary PCM handle
 * \return raw (lowlevel) capture PCM handle or NULL
 */
snd_pcm_t *sndo_pcm_raw_capture(sndo_pcm_t *pcm)
{
	return pcm->capture;
}

/**
 * \brief Reset all parameters
 * \param pcm ordinary PCM handle
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_param_reset(sndo_pcm_t *pcm)
{
	int err;

	err = sndo_pcm_drain(pcm);
	if (err < 0)
		return err;
	pcm->initialized = 0;
	if (pcm->playback) {
		err = snd_pcm_hw_params_any(pcm->playback, pcm->p_hw_params);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_current(pcm->playback, pcm->p_sw_params);
		if (err < 0)
			return err;
	}
	if (pcm->capture) {
		err = snd_pcm_hw_params_any(pcm->capture, pcm->c_hw_params);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_current(pcm->capture, pcm->c_sw_params);
		if (err < 0)
			return err;
	}
	return 0;
}

/**
 * \brief Set sample access type
 * \param pcm ordinary PCM handle
 * \param access access type (interleaved or noninterleaved)
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_param_access(sndo_pcm_t *pcm, enum sndo_pcm_access_type access)
{
	int err;
	snd_pcm_access_t native_access = SND_PCM_ACCESS_MMAP_INTERLEAVED;

	switch (access) {
	case SNDO_PCM_ACCESS_INTERLEAVED:	native_access = SND_PCM_ACCESS_MMAP_INTERLEAVED;	break;
	case SNDO_PCM_ACCESS_NONINTERLEAVED:	native_access = SND_PCM_ACCESS_MMAP_NONINTERLEAVED;	break;
	default:
		return -EINVAL;
	}
	err = sndo_pcm_setup(pcm);
	if (err < 0)
		return err;
	if (pcm->playback) {
		err = snd_pcm_hw_params_set_access(pcm->playback, pcm->p_hw_params, native_access);
		if (err < 0) {
			SNDERR("cannot set requested access for the playback direction");
			return err;
		}
	}
	if (pcm->capture) {
		err = snd_pcm_hw_params_set_access(pcm->capture, pcm->c_hw_params, native_access);
		if (err < 0) {
			SNDERR("cannot set requested access for the capture direction");
			return err;
		}
	}
	return 0;
}

/**
 * \brief Set stream rate
 * \param pcm ordinary PCM handle
 * \param rate requested rate
 * \param used_rate returned real rate
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_param_rate(sndo_pcm_t *pcm, unsigned int rate, unsigned int *used_rate)
{
	int err;
	unsigned int prate = rate, crate = rate;

	err = sndo_pcm_setup(pcm);
	if (err < 0)
		return err;
	if (pcm->playback) {
		err = snd_pcm_hw_params_set_rate_near(pcm->playback, pcm->p_hw_params, &prate, 0);
		if (err < 0) {
			SNDERR("cannot set requested rate for the playback direction");
			return err;
		}
	}
	if (pcm->capture) {
		err = snd_pcm_hw_params_set_rate_near(pcm->capture, pcm->c_hw_params, &crate, 0);
		if (err < 0) {
			SNDERR("cannot set requested rate for the capture direction");
			return err;
		}
	}
	if (used_rate)
		*used_rate = pcm->capture ? crate : prate;
	return 0;
}

/**
 * \brief Set channels in stream
 * \param pcm ordinary PCM handle
 * \param channels requested channels
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_param_channels(sndo_pcm_t *pcm, unsigned int channels)
{
	int err;

	err = sndo_pcm_setup(pcm);
	if (err < 0)
		return err;
	if (pcm->playback) {
		err = snd_pcm_hw_params_set_channels(pcm->capture, pcm->p_hw_params, channels);
		if (err < 0) {
			SNDERR("cannot set requested channels for the playback direction");
			return err;
		}
	}
	if (pcm->capture) {
		err = snd_pcm_hw_params_set_channels(pcm->capture, pcm->c_hw_params, channels);
		if (err < 0) {
			SNDERR("cannot set requested channels for the capture direction");
			return err;
		}
	}
	return 0;
}

/**
 * \brief Set stream format
 * \param pcm ordinary PCM handle
 * \param rate requested channels
 * \param used_rate returned real channels
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_param_format(sndo_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_subformat_t subformat)
{
	int err;
	
	if (subformat != SND_PCM_SUBFORMAT_STD)
		return -EINVAL;
	err = snd_pcm_format_physical_width(format);
	if (err < 0)
		return err;
	if (err % 8)
		return -EINVAL;
	err = sndo_pcm_setup(pcm);
	if (err < 0)
		return err;
	if (pcm->playback) {
		err = snd_pcm_hw_params_set_format(pcm->capture, pcm->p_hw_params, format);
		if (err < 0) {
			SNDERR("cannot set requested format for the playback direction");
			return err;
		}
	}
	if (pcm->capture) {
		err = snd_pcm_hw_params_set_format(pcm->capture, pcm->c_hw_params, format);
		if (err < 0) {
			SNDERR("cannot set requested format for the capture direction");
			return err;
		}
	}
	return 0;
}

/**
 * \brief Set stream latency
 * \param pcm ordinary PCM handle
 * \param latency requested latency
 * \param used_latency returned real latency in frames
 * \return 0 on success otherwise a negative error code
 *
 * Note that the result value is only approximate and for one direction.
 * For example, hardware FIFOs are not counted etc.
 */
int sndo_pcm_param_latency(sndo_pcm_t *pcm, enum sndo_pcm_latency_type latency, snd_pcm_uframes_t *used_latency)
{
	int err;

	err = sndo_pcm_setup(pcm);
	if (err < 0)
		return err;
	pcm->latency = latency;
	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	if (used_latency)
		*used_latency = pcm->ring_size;
	return 0;
}

/**
 * \brief Set xrun behaviour
 * \param pcm ordinary PCM handle
 * \param xrun requested behaviour
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_param_xrun(sndo_pcm_t *pcm, enum sndo_pcm_xrun_type xrun)
{
	int err;

	err = sndo_pcm_setup(pcm);
	if (err < 0)
		return err;
	pcm->xrun = xrun;
	return 0;
}

/**
 * \brief Begin the playback interleaved frame update
 * \param pcm ordinary PCM handle
 * \param ring_buffer returned pointer to actual destination area
 * \param frames returned maximum count of updated frames
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_pio_ibegin(sndo_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames)
{
	int err;
	const snd_pcm_channel_area_t *areas;

	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_begin(pcm->playback, &areas, &pcm->p_offset, frames);
	if (err < 0)
		return err;
	if (*frames < pcm->transfer_block) {
		frames = 0;
	} else {
		*frames -= *frames % pcm->transfer_block;
		*ring_buffer = (char *)areas->addr + (areas->first / 8) + pcm->p_offset * pcm->samplebytes;
	}
	return 0;
}

/**
 * \brief Finish the playback interleave frame update (commit data to hardware)
 * \param pcm ordinary PCM handle
 * \param frames count of updated frames
 * \return count of transferred frames on success otherwise a negative error code
 */
snd_pcm_sframes_t sndo_pcm_pio_iend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	if (frames <= 0)
		return -EINVAL;
	if (frames % pcm->transfer_block)
		return -EINVAL;
	return snd_pcm_mmap_commit(pcm->playback, pcm->p_offset, frames);
}

/**
 * \brief Begin the playback noninterleaved frame update
 * \param pcm ordinary PCM handle
 * \param ring_buffer returned pointer to actual destination area
 * \param frames returned maximum count of updated frames
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_pio_nbegin(sndo_pcm_t *pcm, void ***ring_buffer, snd_pcm_uframes_t *frames)
{
	int err;
	unsigned ch;
	const snd_pcm_channel_area_t *areas;

	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_begin(pcm->playback, &areas, &pcm->p_offset, frames);
	if (err < 0)
		return err;
	if (*frames < pcm->transfer_block) {
		frames = 0;
	} else {
		*frames -= *frames % pcm->transfer_block;
		for (ch = 0; ch < pcm->channels; ch++)
			ring_buffer[ch] = areas->addr + (areas->first / 8) + pcm->p_offset * pcm->samplebytes;
	}
	return 0;
}

/**
 * \brief Finish the playback noninterleave frame update (commit data to hardware)
 * \param pcm ordinary PCM handle
 * \param frames count of updated frames
 * \return count of transferred frames on success otherwise a negative error code
 */
snd_pcm_sframes_t sndo_pcm_pio_nend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	if (frames <= 0)
		return -EINVAL;
	if (frames % pcm->transfer_block)
		return -EINVAL;
	return snd_pcm_mmap_commit(pcm->playback, pcm->p_offset, frames);
}

/**
 * \brief Begin the capture interleaved frame update
 * \param pcm ordinary PCM handle
 * \param ring_buffer returned pointer to actual destination area
 * \param frames returned maximum count of updated frames
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_cio_ibegin(sndo_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames)
{
	int err;
	const snd_pcm_channel_area_t *areas;

	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_begin(pcm->capture, &areas, &pcm->c_offset, frames);
	if (err < 0)
		return err;
	if (*frames < pcm->transfer_block) {
		frames = 0;
	} else {
		*frames -= *frames % pcm->transfer_block;
		*ring_buffer = (char *)areas->addr + (areas->first / 8) + pcm->c_offset * pcm->samplebytes;
	}
	return 0;
}

/**
 * \brief Finish the capture interleave frame update (commit data to hardware)
 * \param pcm ordinary PCM handle
 * \param frames count of updated frames
 * \return count of transferred frames on success otherwise a negative error code
 */
snd_pcm_sframes_t sndo_pcm_cio_iend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	if (frames <= 0)
		return -EINVAL;
	if (frames % pcm->transfer_block)
		return -EINVAL;
	return snd_pcm_mmap_commit(pcm->capture, pcm->p_offset, frames);
}

/**
 * \brief Begin the capture noninterleaved frame update
 * \param pcm ordinary PCM handle
 * \param ring_buffer returned pointer to actual destination area
 * \param frames returned maximum count of updated frames
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_cio_nbegin(sndo_pcm_t *pcm, void ***ring_buffer, snd_pcm_uframes_t *frames)
{
	int err;
	unsigned ch;
	const snd_pcm_channel_area_t *areas;

	err = sndo_pcm_check_setup(pcm);
	if (err < 0)
		return err;
	err = snd_pcm_mmap_begin(pcm->capture, &areas, &pcm->c_offset, frames);
	if (err < 0)
		return err;
	if (*frames < pcm->transfer_block) {
		frames = 0;
	} else {
		*frames -= *frames % pcm->transfer_block;
		for (ch = 0; ch < pcm->channels; ch++)
			ring_buffer[ch] = areas->addr + (areas->first / 8) + pcm->c_offset * pcm->samplebytes;
	}
	return 0;
}

/**
 * \brief Finish the capture noninterleave frame update (commit data to hardware)
 * \param pcm ordinary PCM handle
 * \param frames count of updated frames
 * \return count of transferred frames on success otherwise a negative error code
 */
snd_pcm_sframes_t sndo_pcm_cio_nend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	if (frames <= 0)
		return -EINVAL;
	if (frames % pcm->transfer_block)
		return -EINVAL;
	return snd_pcm_mmap_commit(pcm->capture, pcm->c_offset, frames);
}

/*
 *  helpers
 */
 
static int sndo_pcm_setup(sndo_pcm_t *pcm)
{
	int err;

	err = sndo_pcm_drain(pcm);
	if (err < 0)
		return err;
	if (!pcm->setting_up) {
		int err = sndo_pcm_param_reset(pcm);
		if (err < 0)
			return err;
		pcm->setting_up = 1;
	}
	return 0;
}

static int sndo_pcm_initialize(sndo_pcm_t *pcm)
{
	int err;
	snd_pcm_uframes_t boundary;
	snd_pcm_uframes_t p_period_size = ~0UL, c_period_size = ~0UL;
	snd_pcm_uframes_t p_buffer_size = ~0UL, c_buffer_size = ~0UL;

	if (pcm->playback) {
		err = snd_pcm_hw_params(pcm->playback, pcm->p_hw_params);
		if (err < 0)
			return err;
		err = snd_pcm_hw_params_get_period_size(pcm->p_hw_params, &p_period_size, NULL);
		if (err < 0)
			return err;
		err = snd_pcm_hw_params_get_buffer_size(pcm->p_hw_params, &p_buffer_size);
		if (err < 0)
			return err;
	}
	if (pcm->capture) {
		err = snd_pcm_hw_params(pcm->capture, pcm->c_hw_params);
		if (err < 0)
			return err;
		err = snd_pcm_hw_params_get_period_size(pcm->c_hw_params, &c_period_size, NULL);
		if (err < 0)
			return err;
		err = snd_pcm_hw_params_get_buffer_size(pcm->c_hw_params, &c_buffer_size);
		if (err < 0)
			return err;
	}
	if (p_period_size < c_period_size)
		pcm->transfer_block = p_period_size;
	else
		pcm->transfer_block = c_period_size;
	if (p_buffer_size < c_buffer_size)
		pcm->ring_size = p_buffer_size;
	else
		pcm->ring_size = c_buffer_size;
	if (pcm->playback) {
		err = snd_pcm_sw_params_get_boundary(pcm->p_sw_params, &boundary);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_start_threshold(pcm->playback, pcm->p_sw_params, boundary);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_stop_threshold(pcm->playback, pcm->p_sw_params, pcm->xrun == SNDO_PCM_XRUN_IGNORE ? boundary : pcm->ring_size);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_xfer_align(pcm->playback, pcm->p_sw_params, 1);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_avail_min(pcm->playback, pcm->p_sw_params, pcm->transfer_block);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params(pcm->playback, pcm->p_sw_params);
		if (err < 0)
			return err;
	}
	if (pcm->capture) {
		err = snd_pcm_sw_params_get_boundary(pcm->c_sw_params, &boundary);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_start_threshold(pcm->capture, pcm->c_sw_params, boundary);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_stop_threshold(pcm->capture, pcm->c_sw_params, pcm->xrun == SNDO_PCM_XRUN_IGNORE ? boundary : pcm->ring_size);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_xfer_align(pcm->capture, pcm->c_sw_params, 1);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params_set_avail_min(pcm->capture, pcm->c_sw_params, pcm->transfer_block);
		if (err < 0)
			return err;
		err = snd_pcm_sw_params(pcm->capture, pcm->c_sw_params);
		if (err < 0)
			return err;
	}
	pcm->initialized = 1;
	return 0;
}
