/**
 * \file <alsa/mixer_simple.h>
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 *
 * Application interface library for the ALSA driver.
 * See the \ref mixer_simple page for more details.
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

/** Simple Mixer latency type */
enum snds_mixer_io_type {

	/** master volume - left (0-1000) */
	SNDS_MIO_MASTER_LVOL = 0,
	/** master volume - right (0-1000) */
	SNDS_MIO_MASTER_RVOL,
	/** master volume - left mute (0 = off, 1 = on) */
	SNDS_MIO_MASTER_LMUTE,
	/** master volume - right mute (0 = off, 1 = on) */
	SNDS_MIO_MASTER_RMUTE,

	/** pcm volume - left (0-1000) */
	SNDS_MIO_Mixer_LVOL,
	/** pcm volume - right (0-1000) */
	SNDS_MIO_Mixer_RVOL,
	/** pcm volume - left mute (0 = off, 1 = on) */
	SNDS_MIO_Mixer_LMUTE,
	/** pcm volume - right mute (0 = off, 1 = on) */
	SNDS_MIO_Mixer_RMUTE,

	/** CD volume - left (0-1000) */
	SNDS_MIO_CD_LVOL,
	/** CD volume - right (0-1000) */
	SNDS_MIO_CD_RVOL,
	/** CD volume - left mute (0 = off, 1 = on) */
	SNDS_MIO_CD_LMUTE,
	/** CD volume - right mute (0 = off, 1 = on) */
	SNDS_MIO_CD_RMUTE,

	/** AUX volume - left (0-1000) */
	SNDS_MIO_AUX_LVOL,
	/** CD volume - right (0-1000) */
	SNDS_MIO_AUX_RVOL,
	/** CD volume - left mute (0 = off, 1 = on) */
	SNDS_MIO_AUX_LMUTE,
	/** CD volume - right mute (0 = off, 1 = on) */
	SNDS_MIO_AUX_RMUTE,


	/** capture gain - left (0-1000) */
	SNDS_MIO_CGAIN_LVOL = 0x1000,
	/** capture gain - right (0-1000) */
	SNDS_MIO_CGAIN_RVOL,


	/** capture source - mic switch (0 = off, 1 = on) */
	SNDS_MIO_CSOURCE_MIC = 0x1100,
	/** capture source - line switch (0 = off, 1 = on)*/
	SNDS_MIO_CSOURCE_LINE,
	/** capture source - CD switch (0 = off, 1 = on) */
	SNDS_MIO_CSOURCE_CD,
	/** capture source - AUX switch (0 = off, 1 = on) */
	SNDS_MIO_CSOURCE_AUX,
	/** capture source - mix switch (0 = off, 1 = on) */
	SNDS_MIO_CSOURCE_MIX
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Mixer_simple Mixer Simple Interface
 *  See the \ref mixer_simple page for more details.
 *  \{
 */

int snds_mixer_open(snds_mixer_t **pcm, const char *playback_name, const char *capture_name);
int snds_mixer_open_lconf(snds_mixer_t **pcm, const char *plaback_name, const char *capture_name, snd_config_t *lconf);
int snds_mixer_close(snds_mixer_t *pcm);
int snds_mixer_poll_descriptors_count(snds_mixer_t *pcm);
int snds_mixer_poll_descriptors(snds_mixer_t *pcm, struct pollfd *pfds, unsigned int space);
int snds_mixer_poll_descriptors_revents(snds_mixer_t *pcm, struct pollfd *pfds, unsigned int nfds, unsigned short *revents);
int snds_mixer_io_get(snds_mixer_t *pcm, enum snds_mixer_io_type type, int *val);
int snds_mixer_io_set(snds_mixer_t *pcm, enum snds_mixer_io_type type, int val);
int snds_mixer_io_change(snds_mixer_t *mixer, enum snds_mixer_io_type *changed, int changed_array_size);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_MIXER_SIMPLE_H */
