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

/*
 * Abbreviations:
 *
 * FLVOL    - Front Left Volume (0-1000)
 * FCLVOL   - Front Center Left Volume (0-1000)
 * FCVOL    - Front Center Volume (0-1000)
 * FCRVOL   - Front Center Right Volume (0-1000)
 * FRVOL    - Front Right Volume (0-1000)
 * FSLVOL   - Front Side Left Volume (0-1000)
 * FSRVOL   - Front Side Right Volume (0-1000)
 * RSLVOL   - Rear Side Left Volume (0-1000)
 * RSRVOL   - Rear Side Right Volume (0-1000)
 * RLVOL    - Rear Left Volume (0-1000)
 * RCVOL    - Rear Center Volume (0-1000)
 * RRVOL    - Rear Right Volume (0-1000)
 * LFEVOL   - Low Frequency Effects (Subwoofer) Volume (0-1000)
 * OVRVOL   - Overhead Volume (0-1000)
 */

/** Ordinary Mixer I/O type */
enum sndo_mixer_io_type {

	/*
	 *  playback section
	 */

	/* Master */
	SNDO_MIO_MASTER_FLVOL = 0 * 0x40,
	SNDO_MIO_MASTER_FCLVOL,
	SNDO_MIO_MASTER_FCVOL,
	SNDO_MIO_MASTER_FCRVOL,
	SNDO_MIO_MASTER_FRVOL,
	SNDO_MIO_MASTER_FSLVOL,
	SNDO_MIO_MASTER_FSRVOL,
	SNDO_MIO_MASTER_RSLVOL,
	SNDO_MIO_MASTER_RSRVOL,
	SNDO_MIO_MASTER_RLVOL,
	SNDO_MIO_MASTER_RCVOL,
	SNDO_MIO_MASTER_RRVOL,
	SNDO_MIO_MASTER_LFEVOL,
	SNDO_MIO_MASTER_OVRVOL,

	/* PCM */
	SNDO_MIO_PCM_FLVOL = 1 * 0x40,
	SNDO_MIO_PCM_FCLVOL,
	SNDO_MIO_PCM_FCVOL,
	SNDO_MIO_PCM_FCRVOL,
	SNDO_MIO_PCM_FRVOL,
	SNDO_MIO_PCM_FSLVOL,
	SNDO_MIO_PCM_FSRVOL,
	SNDO_MIO_PCM_RSLVOL,
	SNDO_MIO_PCM_RSRVOL,
	SNDO_MIO_PCM_RLVOL,
	SNDO_MIO_PCM_RCVOL,
	SNDO_MIO_PCM_RRVOL,
	SNDO_MIO_PCM_LFEVOL,
	SNDO_MIO_PCM_OVRVOL,

	/* LINE */
	SNDO_MIO_LINE_FLVOL = 2 * 0x40,
	SNDO_MIO_LINE_FCLVOL,
	SNDO_MIO_LINE_FCVOL,
	SNDO_MIO_LINE_FCRVOL,
	SNDO_MIO_LINE_FRVOL,
	SNDO_MIO_LINE_FSLVOL,
	SNDO_MIO_LINE_FSRVOL,
	SNDO_MIO_LINE_RSLVOL,
	SNDO_MIO_LINE_RSRVOL,
	SNDO_MIO_LINE_RLVOL,
	SNDO_MIO_LINE_RCVOL,
	SNDO_MIO_LINE_RRVOL,
	SNDO_MIO_LINE_LFEVOL,
	SNDO_MIO_LINE_OVRVOL,

	/* MIC */
	SNDO_MIO_MIC_FLVOL = 3 * 0x40,
	SNDO_MIO_MIC_FCLVOL,
	SNDO_MIO_MIC_FCVOL,
	SNDO_MIO_MIC_FCRVOL,
	SNDO_MIO_MIC_FRVOL,
	SNDO_MIO_MIC_FSLVOL,
	SNDO_MIO_MIC_FSRVOL,
	SNDO_MIO_MIC_RSLVOL,
	SNDO_MIO_MIC_RSRVOL,
	SNDO_MIO_MIC_RLVOL,
	SNDO_MIO_MIC_RCVOL,
	SNDO_MIO_MIC_RRVOL,
	SNDO_MIO_MIC_LFEVOL,
	SNDO_MIO_MIC_OVRVOL,

	/* CD */
	SNDO_MIO_CD_FLVOL = 4 * 0x40,
	SNDO_MIO_CD_FCLVOL,
	SNDO_MIO_CD_FCVOL,
	SNDO_MIO_CD_FCRVOL,
	SNDO_MIO_CD_FRVOL,
	SNDO_MIO_CD_FSLVOL,
	SNDO_MIO_CD_FSRVOL,
	SNDO_MIO_CD_RSLVOL,
	SNDO_MIO_CD_RSRVOL,
	SNDO_MIO_CD_RLVOL,
	SNDO_MIO_CD_RCVOL,
	SNDO_MIO_CD_RRVOL,
	SNDO_MIO_CD_LFEVOL,
	SNDO_MIO_CD_OVRVOL,

	/* AUX */
	SNDO_MIO_AUX_FLVOL = 5 * 0x40,
	SNDO_MIO_AUX_FCLVOL,
	SNDO_MIO_AUX_FCVOL,
	SNDO_MIO_AUX_FCRVOL,
	SNDO_MIO_AUX_FRVOL,
	SNDO_MIO_AUX_FSLVOL,
	SNDO_MIO_AUX_FSRVOL,
	SNDO_MIO_AUX_RSLVOL,
	SNDO_MIO_AUX_RSRVOL,
	SNDO_MIO_AUX_RLVOL,
	SNDO_MIO_AUX_RCVOL,
	SNDO_MIO_AUX_RRVOL,
	SNDO_MIO_AUX_LFEVOL,
	SNDO_MIO_AUX_OVRVOL,

	/*
	 *  capture section
	 */

	/* capture gain */
	SNDO_MIO_CGAIN_FL = 0x8000,
	SNDO_MIO_CGAIN_FCL,
	SNDO_MIO_CGAIN_FC,
	SNDO_MIO_CGAIN_FCR,
	SNDO_MIO_CGAIN_FR,
	SNDO_MIO_CGAIN_FSL,
	SNDO_MIO_CGAIN_FSR,
	SNDO_MIO_CGAIN_RSL,
	SNDO_MIO_CGAIN_RSR,
	SNDO_MIO_CGAIN_RL,
	SNDO_MIO_CGAIN_RC,
	SNDO_MIO_CGAIN_RR,
	SNDO_MIO_CGAIN_LFE,
	SNDO_MIO_CGAIN_OVR,

	/* capture source (0 = off, 1 = on) */
	SNDO_MIO_CSOURCE_MIC = 0x8100,
	SNDO_MIO_CSOURCE_LINE,
	SNDO_MIO_CSOURCE_CD,
	SNDO_MIO_CSOURCE_AUX,
	SNDO_MIO_CSOURCE_MIX,

	/* misc */
	SNDO_MIO_STEREO = 0x8200,	/* (0 = off, 1 = on) standard stereo source, might be converted to use all outputs */
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

int sndo_mixer_open(sndo_mixer_t **pmixer, const char *playback_name, const char *capture_name, struct alisp_cfg *lconf);
int sndo_mixer_open_pcm(sndo_mixer_t **pmixer, snd_pcm_t *playback_pcm, snd_pcm_t *capture_pcm, struct alisp_cfg *lconf);
int sndo_mixer_close(sndo_mixer_t *mixer);
int sndo_mixer_poll_descriptors_count(sndo_mixer_t *mixer);
int sndo_mixer_poll_descriptors(sndo_mixer_t *mixer, struct pollfd *pfds, unsigned int space);
int sndo_mixer_poll_descriptors_revents(sndo_mixer_t *mixer, struct pollfd *pfds, unsigned int nfds, unsigned short *revents);
int sndo_mixer_io_get_name(enum sndo_mixer_io_type type, char **name);
int sndo_mixer_io_get(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val);
int sndo_mixer_io_set(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val);
int sndo_mixer_io_try_set(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val);
int sndo_mixer_io_get_dB(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val);
int sndo_mixer_io_set_dB(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val);
int sndo_mixer_io_try_set_dB(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val);
int sndo_mixer_io_change(sndo_mixer_t *mixer, enum sndo_mixer_io_type *changed, int changed_array_size);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_MIXER_SIMPLE_H */
