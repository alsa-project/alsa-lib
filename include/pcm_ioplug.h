/*
 * ALSA external PCM plugin SDK (draft version)
 *
 * Copyright (c) 2005 Takashi Iwai <tiwai@suse.de>
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

#ifndef __ALSA_PCM_IOPLUG_H
#define __ALSA_PCM_IOPLUG_H

/* hw constraints */
enum {
	SND_PCM_IOPLUG_HW_ACCESS = 0,
	SND_PCM_IOPLUG_HW_FORMAT,
	SND_PCM_IOPLUG_HW_CHANNELS,
	SND_PCM_IOPLUG_HW_RATE,
	SND_PCM_IOPLUG_HW_PERIOD_BYTES,
	SND_PCM_IOPLUG_HW_BUFFER_BYTES,
	SND_PCM_IOPLUG_HW_PERIODS,
	SND_PCM_IOPLUG_HW_PARAMS
};
	
typedef struct snd_pcm_ioplug snd_pcm_ioplug_t;
typedef struct snd_pcm_ioplug_callback snd_pcm_ioplug_callback_t;

/* exported pcm data */
struct snd_pcm_ioplug {
	/* must be filled before calling snd_pcm_ioplug_create() */
	const char *name;
	int poll_fd;
	unsigned int poll_events;
	unsigned int mmap_rw;		/* pseudo mmap */
	const snd_pcm_ioplug_callback_t *callback;
	void *private_data;
	/* filled by snd_pcm_ioplug_open() */
	snd_pcm_t *pcm;
	/* read-only status */
	snd_pcm_stream_t stream;
	snd_pcm_state_t state;
	volatile snd_pcm_uframes_t appl_ptr;
	volatile snd_pcm_uframes_t hw_ptr;
	int nonblock;
	/* filled in hw_params */
	snd_pcm_access_t access;
	snd_pcm_format_t format;
	unsigned int channels;
	unsigned int rate;
	snd_pcm_uframes_t period_size;
	snd_pcm_uframes_t buffer_size;
};

/* callback table */
struct snd_pcm_ioplug_callback {
	/* required */
	int (*start)(snd_pcm_ioplug_t *io);
	int (*stop)(snd_pcm_ioplug_t *io);
	snd_pcm_sframes_t (*pointer)(snd_pcm_ioplug_t *io);
	/* optional */
	snd_pcm_sframes_t (*transfer)(snd_pcm_ioplug_t *io,
				      const snd_pcm_channel_area_t *areas,
				      snd_pcm_uframes_t offset,
				      snd_pcm_uframes_t size);
	int (*close)(snd_pcm_ioplug_t *io);
	int (*hw_params)(snd_pcm_ioplug_t *io, snd_pcm_hw_params_t *params);
	int (*hw_free)(snd_pcm_ioplug_t *io);
	int (*sw_params)(snd_pcm_ioplug_t *io, snd_pcm_sw_params_t *params);
	int (*prepare)(snd_pcm_ioplug_t *io);
	int (*drain)(snd_pcm_ioplug_t *io);
	int (*pause)(snd_pcm_ioplug_t *io, int enable);
	int (*resume)(snd_pcm_ioplug_t *io);
	int (*poll_revents)(snd_pcm_ioplug_t *io, struct pollfd *pfd, unsigned int nfds, unsigned short *revents);
	void (*dump)(snd_pcm_ioplug_t *io, snd_output_t *out);
};


int snd_pcm_ioplug_create(snd_pcm_ioplug_t *io, const char *name,
			  snd_pcm_stream_t stream, int mode);
int snd_pcm_ioplug_delete(snd_pcm_ioplug_t *io);

/* update poll_fd and mmap_rw */
int snd_pcm_ioplug_reinit_status(snd_pcm_ioplug_t *ioplug);

/* get a mmap area (for mmap_rw only) */
const snd_pcm_channel_area_t *snd_pcm_ioplug_mmap_areas(snd_pcm_ioplug_t *ioplug);

/* clear hw_parameter setting */
void snd_pcm_ioplug_params_reset(snd_pcm_ioplug_t *io);

/* hw_parameter setting */
int snd_pcm_ioplug_set_param_minmax(snd_pcm_ioplug_t *io, int type, unsigned int min, unsigned int max);
int snd_pcm_ioplug_set_param_list(snd_pcm_ioplug_t *io, int type, unsigned int num_list, const unsigned int *list);

#endif /* __ALSA_PCM_IOPLUG_H */
