/**
 * \file <alsa/pcm_ordinary.h>
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 *
 * Application interface library for the ALSA driver.
 * See the \ref pcm_ordinary page for more details.
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

#ifndef __ALSA_PCM_ORDINARY_H
#define __ALSA_PCM_ORDINARY_H

#include <alsa/asoundlib.h>

/** Ordinary PCM latency type */
enum sndo_pcm_latency_type {
	/** normal latency - for standard playback or capture
	    (estimated latency in one direction 350ms) (default) */
	SNDO_PCM_LATENCY_NORMAL = 0,
	/** medium latency - software phones etc.
	    (estimated latency in one direction maximally 50ms) */
	SNDO_PCM_LATENCY_MEDIUM,
	/** realtime latency - realtime applications (effect processors etc.)
	    (estimated latency in one direction 5ms) */
	SNDO_PCM_LATENCY_REALTIME
};

/** Ordinary PCM access type */
enum sndo_pcm_access_type {
	/** interleaved access - channels are interleaved without any gaps among samples (default) */
	SNDO_PCM_ACCESS_INTERLEAVED = 0,
	/** noninterleaved access - channels are separate without any gaps among samples */
	SNDO_PCM_ACCESS_NONINTERLEAVED
};

/** Ordinary PCM xrun type */
enum sndo_pcm_xrun_type {
	/** driver / library will ignore all xruns, the stream runs forever (default) */
	SNDO_PCM_XRUN_IGNORE = 0,
	/** driver / library stops the stream when an xrun occurs */
	SNDO_PCM_XRUN_STOP
};

typedef struct sndo_pcm sndo_pcm_t;

typedef int (sndo_pcm_engine_callback_t)(sndo_pcm_t *pcm);

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup PCM_ordinary PCM Ordinary Interface
 *  See the \ref pcm_ordinary page for more details.
 *  \{
 */

int sndo_pcm_open(sndo_pcm_t **pcm, const char *playback_name, const char *capture_name, struct alisp_cfg *lconf);
int sndo_pcm_close(sndo_pcm_t *pcm);
int sndo_pcm_poll_descriptors_count(sndo_pcm_t *pcm);
int sndo_pcm_poll_descriptors(sndo_pcm_t *pcm, struct pollfd *pfds, unsigned int space);
int sndo_pcm_start(sndo_pcm_t *pcm);
int sndo_pcm_drop(sndo_pcm_t *pcm);
int sndo_pcm_drain(sndo_pcm_t *pcm);
int sndo_pcm_delay(sndo_pcm_t *pcm, snd_pcm_sframes_t *delayp);
int sndo_pcm_transfer_block(sndo_pcm_t *pcm, snd_pcm_uframes_t *tblock);
int sndo_pcm_resume(sndo_pcm_t *pcm);
int sndo_pcm_wait(sndo_pcm_t *pcm, int timeout);
snd_pcm_t *sndo_pcm_raw_playback(sndo_pcm_t *pcm);
snd_pcm_t *sndo_pcm_raw_capture(sndo_pcm_t *pcm);

/**
 * \defgroup PCM_ordinary_params Parameters Functions
 * \ingroup PCM_ordinary
 * See the \ref pcm_ordinary page for more details.
 * \{
 */

int sndo_pcm_param_reset(sndo_pcm_t *pcm);
int sndo_pcm_param_access(sndo_pcm_t *pcm, enum sndo_pcm_access_type access);
int sndo_pcm_param_rate(sndo_pcm_t *pcm, unsigned int rate, unsigned int *used_rate);
int sndo_pcm_param_channels(sndo_pcm_t *pcm, unsigned int channels);
int sndo_pcm_param_format(sndo_pcm_t *pcm, snd_pcm_format_t format, snd_pcm_subformat_t subformat);
int sndo_pcm_param_latency(sndo_pcm_t *pcm, enum sndo_pcm_latency_type latency, snd_pcm_uframes_t *used_latency);
int sndo_pcm_param_xrun(sndo_pcm_t *pcm, enum sndo_pcm_xrun_type xrun);

/** \} */

/**
 * \defgroup PCM_ordinary_access Ring Buffer I/O Functions
 * \ingroup PCM_ordinary
 * See the \ref pcm_ordinary page for more details.
 * \{
 */

/* playback */
int sndo_pcm_pio_ibegin(sndo_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames);
snd_pcm_sframes_t sndo_pcm_pio_iend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames);
int sndo_pcm_pio_nbegin(sndo_pcm_t *pcm, void ***ring_buffer, snd_pcm_uframes_t *frames);
snd_pcm_sframes_t sndo_pcm_pio_nend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames);
/* capture */
int sndo_pcm_cio_ibegin(sndo_pcm_t *pcm, void **ring_buffer, snd_pcm_uframes_t *frames);
snd_pcm_sframes_t sndo_pcm_cio_iend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames);
int sndo_pcm_cio_nbegin(sndo_pcm_t *pcm, void ***ring_buffer, snd_pcm_uframes_t *frames);
snd_pcm_sframes_t sndo_pcm_cio_nend(sndo_pcm_t *pcm, snd_pcm_uframes_t frames);

/** \} */

/**
 * \defgroup PCM_ordinary_engine Callback like engine
 * \ingroup PCM_ordinary
 * See the \ref pcm_ordinary page for more details.
 * \{
 */

int sndo_pcm_set_private_data(sndo_pcm_t *pcm, void *private_data);
int sndo_pcm_get_private_data(sndo_pcm_t *pcm, void **private_data);
int sndo_pcm_engine(sndo_pcm_t *pcm,
		    sndo_pcm_engine_callback_t *playback,
		    sndo_pcm_engine_callback_t *capture);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_PCM_ORDINARY_H */
