/**
 * \file <alsa/pcm_simple.h>
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 *
 * Application interface library for the ALSA driver.
 * See the \ref pcm_simple page for more details.
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

#ifndef __ALSA_PCM_SIMPLE_H
#define __ALSA_PCM_SIMPLE_H

#include <alsa/pcm.h>

/** Simple PCM latency type */
enum snds_pcm_latency_type {
	/** normal latency - for standard playback or capture
	    (estimated latency in one direction 350ms) */
	SNDS_PCM_LATENCY_NORMAL = 0,
	/** medium latency - software phones etc.
	    (estimated latency in one direction 50ms) */
	SNDS_PCM_LATENCY_MEDIUM,
	/** realtime latency - realtime applications (effect processors etc.)
	    (estimated latency in one direction 5ms) */
	SNDS_PCM_LATENCY_REALTIME
};

/** Simple PCM access type */
enum snds_pcm_access_type {
	/** interleaved access - channels are interleaved without any gaps among samples */
	SNDS_PCM_ACCESS_INTERLEAVED = 0,
	/** noninterleaved access - channels are separate without any gaps among samples */
	SNDS_PCM_ACCESS_NONINTERLEAVED
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup PCM_simple PCM Simple Interface
 *  See the \ref pcm_simple page for more details.
 *  \{
 */

int snds_pcm_open(snds_pcm_t **pcm, const char *playback_name, const char *capture_name);
int snds_pcm_open_lconf(snds_pcm_t **pcm, const char *plaback_name, const char *capture_name, snd_config_t *lconf);
int snds_pcm_close(snds_pcm_t *pcm);
int snds_pcm_poll_descriptors_count(snds_pcm_t *pcm);
int snds_pcm_poll_descriptors(snds_pcm_t *pcm, struct pollfd *pfds, unsigned int space);
int snds_pcm_poll_descriptors_revents(snds_pcm_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents);
int snds_pcm_start(snds_pcm_t *pcm);
int snds_pcm_drop(snds_pcm_t *pcm);
int snds_pcm_drain(snds_pcm_t *pcm);
int snds_pcm_delay(snds_pcm_t *pcm, snd_pcm_sframes_t *delayp);
int snds_pcm_resume(snd_pcm_t *pcm);
int snds_pcm_wait(snds_pcm_t *pcm, int timeout);
snd_pcm_t *snds_pcm_raw_playback(snds_pcm_t *pcm);
snd_pcm_t *snds_pcm_raw_capture(snds_pcm_t *pcm);

/**
 * \defgroup PCM_simple_params Parameters Functions
 * \ingroup PCM_simple
 * See the \ref pcm_simple page for more details.
 * \{
 */

int snds_pcm_param_rate(snds_pcm_t *pcm, unsigned int rate, unsigned int *used_rate);
int snds_pcm_param_channels(snds_pcm_t *pcm, unsigned int channels, unsigned int *used_channels);
int snds_pcm_param_format(snds_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_subformat_t subformat);
int snds_pcm_param_latency(snds_pcm_t *pcm, enum snds_pcm_latency_type latency);
int snds_pcm_param_access(snds_pcm_t *pcm, enum snds_pcm_access_type access);

/** \} */

/**
 * \defgroup PCM_simple_access Ring Buffer I/O Functions
 * \ingroup PCM_simple
 * See the \ref pcm_simple page for more details.
 * \{
 */

/* playback */
int snds_pcm_pio_ibegin(snds_pcm_t *pcm, void *ring_buffer, snd_pcm_uframes_t *frames);
int snds_pcm_pio_iend(snds_pcm_t *pcm, snd_pcm_uframes_t frames);
int snds_pcm_pio_nbegin(snds_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames);
int snds_pcm_pio_nend(snds_pcm_t *pcm, snd_pcm_uframes_t frames);
/* capture */
int snds_pcm_cio_ibegin(snds_pcm_t *pcm, void *ring_buffer, snd_pcm_uframes_t *frames);
int snds_pcm_cio_iend(snds_pcm_t *pcm, snd_pcm_uframes_t frames);
int snds_pcm_cio_nbegin(snds_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames);
int snds_pcm_cio_nend(snds_pcm_t *pcm, snd_pcm_uframes_t frames);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_PCM_SIMPLE_H */
