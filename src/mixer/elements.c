/*
 *  Mixer Interface - elements
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include "asoundlib.h"

static inline void safe_free(void **ptr)
{
	if (*ptr)
		free(*ptr);
	*ptr = NULL;
}

int snd_mixer_element_has_info(snd_mixer_eid_t *eid)
{
	if (!eid)
		return -EINVAL;
	switch (eid->type) {
	case SND_MIXER_ETYPE_INPUT:
	case SND_MIXER_ETYPE_OUTPUT:
	case SND_MIXER_ETYPE_CAPTURE1:
	case SND_MIXER_ETYPE_CAPTURE2:
	case SND_MIXER_ETYPE_CAPTURE3:
	case SND_MIXER_ETYPE_PLAYBACK1:
	case SND_MIXER_ETYPE_PLAYBACK2:
	case SND_MIXER_ETYPE_PLAYBACK3:
	case SND_MIXER_ETYPE_ADC:
	case SND_MIXER_ETYPE_DAC:
	case SND_MIXER_ETYPE_SWITCH3:
	case SND_MIXER_ETYPE_VOLUME1:
	case SND_MIXER_ETYPE_VOLUME2:
	case SND_MIXER_ETYPE_ACCU1:
	case SND_MIXER_ETYPE_ACCU2:
	case SND_MIXER_ETYPE_ACCU3:
	case SND_MIXER_ETYPE_MUX1:
	case SND_MIXER_ETYPE_MUX2:
	case SND_MIXER_ETYPE_TONE_CONTROL1:
	case SND_MIXER_ETYPE_PAN_CONTROL1:
	case SND_MIXER_ETYPE_3D_EFFECT1:
	case SND_MIXER_ETYPE_PRE_EFFECT1:
		return 1;
	}
	return 0;
}

int snd_mixer_element_info_build(snd_mixer_t *handle, snd_mixer_element_info_t *element)
{
	int err;
	
	if (!handle || !element)
		return -EINVAL;
	if ((err = snd_mixer_element_info(handle, element)) < 0)
		return err;
	switch (element->eid.type) {
	case SND_MIXER_ETYPE_INPUT:
	case SND_MIXER_ETYPE_OUTPUT:
		element->data.io.channels_size = element->data.io.channels_over;
		element->data.io.channels = element->data.io.channels_over = 0;
		element->data.io.pchannels = (snd_mixer_channel_t *)malloc(element->data.io.channels_size * sizeof(snd_mixer_channel_t));
		if (!element->data.io.pchannels)
			return -ENOMEM;
		if ((err = snd_mixer_element_info(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_CAPTURE1:
	case SND_MIXER_ETYPE_PLAYBACK1:
		element->data.pcm1.devices_size = element->data.pcm1.devices_over;
		element->data.pcm1.devices = element->data.pcm1.devices_over = 0;
		element->data.pcm1.pdevices = (int *)malloc(element->data.pcm1.devices_size * sizeof(int));
		if (!element->data.pcm1.pdevices)
			return -ENOMEM;
		if ((err = snd_mixer_element_info(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		element->data.volume1.range_size = element->data.volume1.range_over;
		element->data.volume1.range = element->data.volume1.range_over = 0;
		element->data.volume1.prange = (struct snd_mixer_element_volume1_range *)malloc(element->data.volume1.range_size * sizeof(struct snd_mixer_element_volume1_range));
		if (!element->data.volume1.prange)
			return -ENOMEM;
		if ((err = snd_mixer_element_info(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_VOLUME2:
		element->data.volume2.schannels_size = element->data.volume2.schannels_over;
		element->data.volume2.schannels = element->data.volume2.schannels_over = 0;
		element->data.volume2.pschannels = (snd_mixer_channel_t *)malloc(element->data.volume2.schannels_size * sizeof(snd_mixer_channel_t));
		if (!element->data.volume2.pschannels)
			return -ENOMEM;
		element->data.volume2.range_size = element->data.volume2.range_over;
		element->data.volume2.range = element->data.volume2.range_over = 0;
		element->data.volume2.prange = (struct snd_mixer_element_volume2_range *)malloc(element->data.volume2.range_size * sizeof(struct snd_mixer_element_volume2_range));
		if (!element->data.volume1.prange) {
			safe_free((void **)&element->data.volume2.pschannels);
			return -ENOMEM;
		}
		if ((err = snd_mixer_element_info(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_ACCU3:
		element->data.accu3.range_size = element->data.accu3.range_over;
		element->data.accu3.range = element->data.accu3.range_over = 0;
		element->data.accu3.prange = (struct snd_mixer_element_accu3_range *)malloc(element->data.accu3.range_size * sizeof(struct snd_mixer_element_accu3_range));
		if (!element->data.accu3.prange)
			return -ENOMEM;
		if ((err = snd_mixer_element_info(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_PRE_EFFECT1:
		element->data.peffect1.items_size = element->data.peffect1.items_over;
		element->data.peffect1.items = element->data.peffect1.items_over = 0;
		element->data.peffect1.pitems = (struct snd_mixer_element_pre_effect1_info_item *)malloc(element->data.peffect1.items_size * sizeof(struct snd_mixer_element_pre_effect1_info_item));
		if (!element->data.peffect1.pitems) 
			return -ENOMEM;
		element->data.peffect1.parameters_size = element->data.peffect1.parameters_over;
		element->data.peffect1.parameters = element->data.peffect1.parameters_over = 0;
		element->data.peffect1.pparameters = (struct snd_mixer_element_pre_effect1_info_parameter *)malloc(element->data.peffect1.parameters_size * sizeof(struct snd_mixer_element_pre_effect1_info_parameter));
		if (!element->data.peffect1.pparameters) {
			safe_free((void **)&element->data.peffect1.pitems);
			return -ENOMEM;
		}
		if ((err = snd_mixer_element_info(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_PAN_CONTROL1:
		element->data.pc1.range_size = element->data.pc1.range_over;
		element->data.pc1.range = element->data.pc1.range_over = 0;
		element->data.pc1.prange = (struct snd_mixer_element_pan_control1_range *)malloc(element->data.pc1.range_size * sizeof(struct snd_mixer_element_pan_control1_range));
		if (!element->data.pc1.prange)
			return -ENOMEM;
		if ((err = snd_mixer_element_info(handle, element)) < 0)
			return err;
		break;
	}
	return 0;
}

int snd_mixer_element_info_free(snd_mixer_element_info_t *element)
{
	if (!element)
		return -EINVAL;
	switch (element->eid.type) {
	case SND_MIXER_ETYPE_INPUT:
	case SND_MIXER_ETYPE_OUTPUT:
		safe_free((void **)&element->data.io.pchannels);
		break;
	case SND_MIXER_ETYPE_CAPTURE1:
	case SND_MIXER_ETYPE_PLAYBACK1:
		safe_free((void **)&element->data.pcm1.pdevices);
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		safe_free((void **)&element->data.volume1.prange);
		break;
	case SND_MIXER_ETYPE_VOLUME2:
		safe_free((void **)&element->data.volume2.pschannels);
		safe_free((void **)&element->data.volume1.prange);
		break;
	case SND_MIXER_ETYPE_ACCU3:
		safe_free((void **)&element->data.accu3.prange);
		break;
	case SND_MIXER_ETYPE_PRE_EFFECT1:
		safe_free((void **)&element->data.peffect1.pitems);
		safe_free((void **)&element->data.peffect1.pparameters);
		break;
	case SND_MIXER_ETYPE_PAN_CONTROL1:
		safe_free((void **)&element->data.pc1.prange);
		break;
	}
	return 0;
}

int snd_mixer_element_has_control(snd_mixer_eid_t *eid)
{
	if (!eid)
		return -EINVAL;
	switch (eid->type) {
	case SND_MIXER_ETYPE_SWITCH1:
	case SND_MIXER_ETYPE_SWITCH2:
	case SND_MIXER_ETYPE_SWITCH3:
	case SND_MIXER_ETYPE_VOLUME1:
	case SND_MIXER_ETYPE_VOLUME2:
	case SND_MIXER_ETYPE_ACCU3:
	case SND_MIXER_ETYPE_MUX1:
	case SND_MIXER_ETYPE_MUX2:
	case SND_MIXER_ETYPE_TONE_CONTROL1:
	case SND_MIXER_ETYPE_PAN_CONTROL1:
	case SND_MIXER_ETYPE_3D_EFFECT1:
	case SND_MIXER_ETYPE_PRE_EFFECT1:
		return 1;
	}
	return 0;
}

int snd_mixer_element_build(snd_mixer_t *handle, snd_mixer_element_t *element)
{
	int err;
	
	if (!handle || !element)
		return -EINVAL;
	if ((err = snd_mixer_element_read(handle, element)) < 0)
		return err;
	switch (element->eid.type) {
	case SND_MIXER_ETYPE_SWITCH1:
		element->data.switch1.sw_size = element->data.switch1.sw_over;
		element->data.switch1.sw = element->data.switch1.sw_over = 0;
		element->data.switch1.psw = (unsigned int *)malloc(((element->data.switch1.sw_size + 31) / 32) * sizeof(unsigned int));
		if (!element->data.switch1.psw)
			return -ENOMEM;
		if ((err = snd_mixer_element_read(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_SWITCH3:
		element->data.switch3.rsw_size = element->data.switch3.rsw_over;
		element->data.switch3.rsw = element->data.switch3.rsw_over = 0;
		element->data.switch3.prsw = (unsigned int *)malloc(((element->data.switch3.rsw_size + 31) / 32) * sizeof(unsigned int));
		if (!element->data.switch3.prsw)
			return -ENOMEM;
		if ((err = snd_mixer_element_read(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		element->data.volume1.channels_size = element->data.volume1.channels_over;
		element->data.volume1.channels = element->data.volume1.channels_over = 0;
		element->data.volume1.pchannels = (int *)malloc(element->data.volume1.channels_size * sizeof(int));
		if (!element->data.volume1.pchannels)
			return -ENOMEM;
		if ((err = snd_mixer_element_read(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_VOLUME2:
		element->data.volume2.achannels_size = element->data.volume2.achannels_over;
		element->data.volume2.achannels = element->data.volume2.achannels_over = 0;
		element->data.volume2.pachannels = (int *)malloc(element->data.volume2.achannels_size * sizeof(int));
		if (!element->data.volume2.pachannels)
			return -ENOMEM;
		if ((err = snd_mixer_element_read(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_ACCU3:
		element->data.accu3.channels_size = element->data.accu3.channels_over;
		element->data.accu3.channels = element->data.accu3.channels_over = 0;
		element->data.accu3.pchannels = (int *)malloc(element->data.accu3.channels_size * sizeof(int));
		if (!element->data.accu3.pchannels)
			return -ENOMEM;
		if ((err = snd_mixer_element_read(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_MUX1:
		element->data.mux1.sel_size = element->data.mux1.sel_over;
		element->data.mux1.sel = element->data.mux1.sel_over = 0;
		element->data.mux1.psel = (snd_mixer_eid_t *)malloc(element->data.mux1.sel_size * sizeof(snd_mixer_eid_t));
		if (!element->data.mux1.psel)
			return -ENOMEM;
		if ((err = snd_mixer_element_read(handle, element)) < 0)
			return err;
		break;
	case SND_MIXER_ETYPE_PRE_EFFECT1:
		if (element->data.peffect1.item < 0) {
			element->data.peffect1.parameters_size = element->data.peffect1.parameters_over;
			element->data.peffect1.parameters = element->data.peffect1.parameters_over = 0;
			element->data.peffect1.pparameters = (int *)malloc(element->data.peffect1.parameters_size * sizeof(int));
			if (!element->data.peffect1.pparameters)
				return -ENOMEM;
			if ((err = snd_mixer_element_read(handle, element)) < 0)
				return err;
		}
		break;
	case SND_MIXER_ETYPE_PAN_CONTROL1:
		element->data.pc1.pan_size = element->data.pc1.pan_over;
		element->data.pc1.pan = element->data.pc1.pan_over = 0;
		element->data.pc1.ppan = (int *)malloc(element->data.pc1.pan_size * sizeof(int));
		if (!element->data.pc1.ppan)
			return -ENOMEM;
		if ((err = snd_mixer_element_read(handle, element)) < 0)
			return err;
		break;
	}
	return 0;
}

int snd_mixer_element_free(snd_mixer_element_t *element)
{
	if (!element)
		return -EINVAL;
	switch (element->eid.type) {
	case SND_MIXER_ETYPE_SWITCH1:
		safe_free((void **)&element->data.switch1.psw);
		break;
	case SND_MIXER_ETYPE_SWITCH3:
		safe_free((void **)&element->data.switch3.prsw);
		break;
	case SND_MIXER_ETYPE_VOLUME1:
		safe_free((void **)&element->data.volume1.pchannels);
		break;
	case SND_MIXER_ETYPE_VOLUME2:
		safe_free((void **)&element->data.volume2.pachannels);
		break;
	case SND_MIXER_ETYPE_ACCU3:
		safe_free((void **)&element->data.accu3.pchannels);
		break;
	case SND_MIXER_ETYPE_MUX1:
		safe_free((void **)&element->data.mux1.psel);
		break;
	case SND_MIXER_ETYPE_PRE_EFFECT1:
		if (element->data.peffect1.item < 0)
			safe_free((void **)&element->data.peffect1.pparameters);
		break;
	case SND_MIXER_ETYPE_PAN_CONTROL1:
		safe_free((void **)&element->data.pc1.ppan);
	}
	return 0;
}
