/**
 * \file ordinary_pcm/ordinary_pcm.c
 * \ingroup PCM_ordinary
 * \brief Ordinary PCM interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
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
#include "local.h"
#include "pcm_ordinary.h"

struct sndo_pcm {
	snd_pcm_t *playback;
	snd_pcm_t *capture;
	snd_pcm_t *master;
};

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
		    snd_config_t *lconf)
{
	*ppcm = NULL;
	return -ENODEV;
}

/**
 * \brief Closes a ordinary pcm instance
 * \param pcm Ordinary pcm handle to close
 * \return 0 on success otherwise a negative error code
 */
int sndo_pcm_close(sndo_pcm_t *pcm)
{
	return -ENODEV;
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
	int err = 0;
	snd_pcm_sframes_t pdelay, cdelay;

	assert(pcm);
	assert(delayp);
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

int sndo_pcm_param_rate(sndo_pcm_t *pcm, unsigned int rate, unsigned int *used_rate)
{
	return -EIO;
}

int sndo_pcm_param_channels(sndo_pcm_t *pcm, unsigned int channels, unsigned int *used_channels)
{
	return -EIO;
}

int sndo_pcm_param_format(sndo_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_subformat_t subformat)
{
	return -EIO;
}

int sndo_pcm_param_latency(sndo_pcm_t *pcm, enum sndo_pcm_latency_type latency)
{
	return -EIO;
}

int sndo_pcm_param_access(sndo_pcm_t *pcm, enum sndo_pcm_access_type access)
{
	return -EIO;
}

int sndo_pcm_param_xrun(sndo_pcm_t *pcm, enum sndo_pcm_xrun_type xrun)
{
	return -EIO;
}

int sndo_pcm_pio_ibegin(sndo_pcm_t *pcm, void *ring_buffer, snd_pcm_uframes_t *frames)
{
	return -EIO;
}

int sndo_pcm_pio_iend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	return -EIO;
}

int sndo_pcm_pio_nbegin(sndo_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames)
{
	return -EIO;
}

int sndo_pcm_pio_nend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	return -EIO;
}

int sndo_pcm_cio_ibegin(sndo_pcm_t *pcm, void *ring_buffer, snd_pcm_uframes_t *frames)
{
	return -EIO;
}

int sndo_pcm_cio_iend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	return -EIO;
}

int sndo_pcm_cio_nbegin(sndo_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames)
{
	return -EIO;
}

int sndo_pcm_cio_nend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	return -EIO;
}
