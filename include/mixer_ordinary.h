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

#include "asoundlib.h"

/** Ordinary Mixer latency type */
enum sndo_mixer_io_type {

	/*
	 *  playback section
	 */

	/** master volume - left (0-1000) */
	SNDO_MIO_MASTER_LVOL = 0,
	/** master volume - right (0-1000) */
	SNDO_MIO_MASTER_RVOL,
	/** master volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_LMUTE,
	/** master volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_MASTER_RMUTE,

	/** pcm volume - left (0-1000) */
	SNDO_MIO_Mixer_LVOL,
	/** pcm volume - right (0-1000) */
	SNDO_MIO_Mixer_RVOL,
	/** pcm volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_Mixer_LMUTE,
	/** pcm volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_Mixer_RMUTE,

	/** CD volume - left (0-1000) */
	SNDO_MIO_CD_LVOL,
	/** CD volume - right (0-1000) */
	SNDO_MIO_CD_RVOL,
	/** CD volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_CD_LMUTE,
	/** CD volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_CD_RMUTE,

	/** AUX volume - left (0-1000) */
	SNDO_MIO_AUX_LVOL,
	/** CD volume - right (0-1000) */
	SNDO_MIO_AUX_RVOL,
	/** CD volume - left mute (0 = off, 1 = on) */
	SNDO_MIO_AUX_LMUTE,
	/** CD volume - right mute (0 = off, 1 = on) */
	SNDO_MIO_AUX_RMUTE,

	/*
	 *  capture section
	 */

	/** capture gain - left (0-1000) */
	SNDO_MIO_CGAIN_LVOL = 0x1000,
	/** capture gain - right (0-1000) */
	SNDO_MIO_CGAIN_RVOL,


	/** capture source - mic switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_MIC = 0x1100,
	/** capture source - line switch (0 = off, 1 = on)*/
	SNDO_MIO_CSOURCE_LINE,
	/** capture source - CD switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_CD,
	/** capture source - AUX switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_AUX,
	/** capture source - mix switch (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_MIX
};

typedef struct sndo_mixer sndo_mixer_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Mixer_ordinary Mixer Ordinary Interface
 *  See the \ref mixer_ordinary page for more details.
 *  \{
 */

int sndo_mixer_open(sndo_mixer_t **pmixer, const char *playback_name, const char *capture_name, snd_config_t *lconf);
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
