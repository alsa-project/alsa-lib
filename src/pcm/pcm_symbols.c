/*
 *  PCM Symbols
 *  Copyright (c) 2001 by Jaroslav Kysela <perex@suse.cz>
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

#ifndef PIC

#include "config.h"

extern const char *_snd_module_pcm_adpcm;
extern const char *_snd_module_pcm_alaw;
extern const char *_snd_module_pcm_copy;
extern const char *_snd_module_pcm_file;
extern const char *_snd_module_pcm_hooks;
extern const char *_snd_module_pcm_hw;
extern const char *_snd_module_pcm_linear;
extern const char *_snd_module_pcm_meter;
extern const char *_snd_module_pcm_mulaw;
extern const char *_snd_module_pcm_multi;
extern const char *_snd_module_pcm_null;
extern const char *_snd_module_pcm_plug;
extern const char *_snd_module_pcm_rate;
extern const char *_snd_module_pcm_route;
extern const char *_snd_module_pcm_share;
extern const char *_snd_module_pcm_shm;
extern const char *_snd_module_pcm_lfloat;
extern const char *_snd_module_pcm_ladspa;
extern const char *_snd_module_pcm_dmix;
extern const char *_snd_module_pcm_dsnoop;
extern const char *_snd_module_pcm_dshare;
extern const char *_snd_module_pcm_asym;
extern const char *_snd_module_pcm_iec958;
extern const char *_snd_module_pcm_softvol;

static const char **snd_pcm_open_objects[] = {
	&_snd_module_pcm_adpcm,
	&_snd_module_pcm_alaw,
	&_snd_module_pcm_copy,
	&_snd_module_pcm_file,
	&_snd_module_pcm_hooks,
	&_snd_module_pcm_hw,
	&_snd_module_pcm_linear,
	&_snd_module_pcm_meter,
	&_snd_module_pcm_mulaw,
	&_snd_module_pcm_multi,
	&_snd_module_pcm_null,
	&_snd_module_pcm_plug,
	&_snd_module_pcm_rate,
	&_snd_module_pcm_route,
	&_snd_module_pcm_share,
	&_snd_module_pcm_shm,
	&_snd_module_pcm_lfloat,
	&_snd_module_pcm_ladspa,
	&_snd_module_pcm_dmix,
	&_snd_module_pcm_dsnoop,
	&_snd_module_pcm_dshare,
	&_snd_module_pcm_asym,
	&_snd_module_pcm_iec958,
	&_snd_module_pcm_softvol
};
	
void *snd_pcm_open_symbols(void)
{
	return (void *)snd_pcm_open_objects[0];
}

#endif /* !PIC */
