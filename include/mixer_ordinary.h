/**
 * \file <alsa/mixer_ordinary.h>
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 *
 * Application interface library for the ALSA driver.
 * See the \ref mixer_ordinary page for more details.
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

#ifndef __ALSA_MIXER_SIMPLE_H
#define __ALSA_MIXER_SIMPLE_H

#include <alsa/asoundlib.h>

/** Ordinary Mixer I/O type */
enum sndo_mixer_io_type {

	/*
	 *  playback section
	 */

	/** Master volume - left (0-1000) */
	SNDO_MIO_MASTER_LVOL = 0,
	/** Master volume - right (0-1000) */
	SNDO_MIO_MASTER_RVOL,
	/** Master volume - left surround (0-1000) */
	SNDO_MIO_MASTER_LSVOL = 0,
	/** Master volume - right surround (0-1000) */
	SNDO_MIO_MASTER_RSVOL,
	/** Master volume - center (0-1000) */
	SNDO_MIO_MASTER_CVOL = 0,
	/** Master volume - LFE (0-1000) */
	SNDO_MIO_MASTER_LFEVOL,
	/** Master volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_LMUTE,
	/** Master volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_RMUTE,
	/** Master volume - left surround mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_LSMUTE,
	/** Master volume - right surround mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_RSMUTE,
	/** Master volume - center mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_CMUTE,
	/** Master volume - LFE mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_LFEMUTE,

	/** PCM volume - left (0-1000) */
	SNDO_MIO_PCM_LVOL = 0,
	/** PCM volume - right (0-1000) */
	SNDO_MIO_PCM_RVOL,
	/** PCM volume - left surround (0-1000) */
	SNDO_MIO_PCM_LSVOL = 0,
	/** PCM volume - right surround (0-1000) */
	SNDO_MIO_PCM_RSVOL,
	/** PCM volume - center (0-1000) */
	SNDO_MIO_PCM_CVOL = 0,
	/** PCM volume - LFE (0-1000) */
	SNDO_MIO_PCM_LFEVOL,
	/** PCM volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_PCM_LMUTE,
	/** PCM volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_PCM_RMUTE,
	/** PCM volume - left surround mute (0 = off, 1 = on) */
	SNDO_MIO_PCM_LSMUTE,
	/** PCM volume - right surround mute (0 = off, 1 = on) */
	SNDO_MIO_PCM_RSMUTE,
	/** PCM volume - center mute (0 = off, 1 = on) */
	SNDO_MIO_PCM_CMUTE,
	/** PCM volume - LFE mute (0 = off, 1 = on) */
	SNDO_MIO_PCM_LFEMUTE,

	/** LINE volume - left (0-1000) */
	SNDO_MIO_LINE_LVOL = 0,
	/** LINE volume - right (0-1000) */
	SNDO_MIO_LINE_RVOL,
	/** LINE volume - left surround (0-1000) */
	SNDO_MIO_LINE_LSVOL = 0,
	/** LINE volume - right surround (0-1000) */
	SNDO_MIO_LINE_RSVOL,
	/** LINE volume - center (0-1000) */
	SNDO_MIO_LINE_CVOL = 0,
	/** LINE volume - LFE (0-1000) */
	SNDO_MIO_LINE_LFEVOL,
	/** LINE volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_LINE_LMUTE,
	/** LINE volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_LINE_RMUTE,
	/** LINE volume - left surround mute (0 = off, 1 = on) */
	SNDO_MIO_LINE_LSMUTE,
	/** LINE volume - right surround mute (0 = off, 1 = on) */
	SNDO_MIO_LINE_RSMUTE,
	/** LINE volume - center mute (0 = off, 1 = on) */
	SNDO_MIO_LINE_CMUTE,
	/** LINE volume - LFE mute (0 = off, 1 = on) */
	SNDO_MIO_LINE_LFEMUTE,

	/** MIC volume - left (0-1000) */
	SNDO_MIO_MIC_LVOL = 0,
	/** MIC volume - right (0-1000) */
	SNDO_MIO_MIC_RVOL,
	/** MIC volume - left surround (0-1000) */
	SNDO_MIO_MIC_LSVOL = 0,
	/** MIC volume - right surround (0-1000) */
	SNDO_MIO_MIC_RSVOL,
	/** MIC volume - center (0-1000) */
	SNDO_MIO_MIC_CVOL = 0,
	/** MIC volume - LFE (0-1000) */
	SNDO_MIO_MIC_LFEVOL,
	/** MIC volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_MIC_LMUTE,
	/** MIC volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_MIC_RMUTE,
	/** MIC volume - left surround mute (0 = off, 1 = on) */
	SNDO_MIO_MIC_LSMUTE,
	/** MIC volume - right surround mute (0 = off, 1 = on) */
	SNDO_MIO_MIC_RSMUTE,
	/** MIC volume - center mute (0 = off, 1 = on) */
	SNDO_MIO_MIC_CMUTE,
	/** MIC volume - LFE mute (0 = off, 1 = on) */
	SNDO_MIO_MIC_LFEMUTE,

	/** CD volume - left (0-1000) */
	SNDO_MIO_CD_LVOL = 0,
	/** CD volume - right (0-1000) */
	SNDO_MIO_CD_RVOL,
	/** CD volume - left surround (0-1000) */
	SNDO_MIO_CD_LSVOL = 0,
	/** CD volume - right surround (0-1000) */
	SNDO_MIO_CD_RSVOL,
	/** CD volume - center (0-1000) */
	SNDO_MIO_CD_CVOL = 0,
	/** CD volume - LFE (0-1000) */
	SNDO_MIO_CD_LFEVOL,
	/** CD volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_CD_LMUTE,
	/** CD volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_CD_RMUTE,
	/** CD volume - left surround mute (0 = off, 1 = on) */
	SNDO_MIO_CD_LSMUTE,
	/** CD volume - right surround mute (0 = off, 1 = on) */
	SNDO_MIO_CD_RSMUTE,
	/** CD volume - center mute (0 = off, 1 = on) */
	SNDO_MIO_CD_CMUTE,
	/** CD volume - LFE mute (0 = off, 1 = on) */
	SNDO_MIO_CD_LFEMUTE,

	/*
	 *  capture section
	 */

	/** capture gain - left (0-1000) */
	SNDO_MIO_CGAIN_LVOL = 0x1000,
	/** capture gain - right (0-1000) */
	SNDO_MIO_CGAIN_RVOL,
	/** capture gain - left surround (0-1000) */
	SNDO_MIO_CGAIN_LSVOL,
	/** capture gain - right surround (0-1000) */
	SNDO_MIO_CGAIN_RSVOL,
	/** capture gain - center (0-1000) */
	SNDO_MIO_CGAIN_CVOL,
	/** capture gain - LFE (0-1000) */
	SNDO_MIO_CGAIN_LFEVOL,

	/** capture source - MIC exclusive switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_MIC = 0x1100,
	/** capture source - LINE exclusive switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_LINE,
	/** capture source - CD exclusive switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_CD,
	/** capture source - AUX exclusive switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_AUX,
	/** capture source - MIX exclusive switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_MIX
};

typedef struct sndo_mixer sndo_mixer_t;
struct alisp_cfg;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Mixer_ordinary Mixer Ordinary Interface
 *  See the \ref mixer_ordinary page for more details.
 *  \{
 */

int sndo_mixer_open(sndo_mixer_t **pmixer, snd_pcm_t *playback_pcm, snd_pcm_t *capture_pcm, struct alisp_cfg *lconf);
int sndo_mixer_close(sndo_mixer_t *mixer);
int sndo_mixer_poll_descriptors_count(sndo_mixer_t *mixer);
int sndo_mixer_poll_descriptors(sndo_mixer_t *mixer, struct pollfd *pfds, unsigned int space);
int sndo_mixer_poll_descriptors_revents(sndo_mixer_t *mixer, struct pollfd *pfds, unsigned int nfds, unsigned short *revents);
int sndo_mixer_io_get(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val);
int sndo_mixer_io_set(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int val);
int sndo_mixer_io_change(sndo_mixer_t *mixer, enum sndo_mixer_io_type *changed, int changed_array_size);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_MIXER_SIMPLE_H */
