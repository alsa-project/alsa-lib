/*
 *  PCM - Automatically generated functions
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
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
  
#include "pcm_local.h"



size_t snd_pcm_access_mask_sizeof()
{
	return sizeof(snd_pcm_access_mask_t);
}

int snd_pcm_access_mask_malloc(snd_pcm_access_mask_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_access_mask_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_pcm_access_mask_free(snd_pcm_access_mask_t *obj)
{
	free(obj);
}

void snd_pcm_access_mask_copy(snd_pcm_access_mask_t *dst, const snd_pcm_access_mask_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_pcm_access_mask_none(snd_pcm_access_mask_t *mask)
{
	snd_mask_none((snd_mask_t *) mask);
}

void snd_pcm_access_mask_any(snd_pcm_access_mask_t *mask)
{
	snd_mask_any((snd_mask_t *) mask);
}

int snd_pcm_access_mask_test(const snd_pcm_access_mask_t *mask, snd_pcm_access_t val)
{
	return snd_mask_test((snd_mask_t *) mask, (unsigned long) val);
}

void snd_pcm_access_mask_set(snd_pcm_access_mask_t *mask, snd_pcm_access_t val)
{
	snd_mask_set((snd_mask_t *) mask, (unsigned long) val);
}

void snd_pcm_access_mask_reset(snd_pcm_access_mask_t *mask, snd_pcm_access_t val)
{
	snd_mask_reset((snd_mask_t *) mask, (unsigned long) val);
}


size_t snd_pcm_format_mask_sizeof()
{
	return sizeof(snd_pcm_format_mask_t);
}

int snd_pcm_format_mask_malloc(snd_pcm_format_mask_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_format_mask_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_pcm_format_mask_free(snd_pcm_format_mask_t *obj)
{
	free(obj);
}

void snd_pcm_format_mask_copy(snd_pcm_format_mask_t *dst, const snd_pcm_format_mask_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_pcm_format_mask_none(snd_pcm_format_mask_t *mask)
{
	snd_mask_none((snd_mask_t *) mask);
}

void snd_pcm_format_mask_any(snd_pcm_format_mask_t *mask)
{
	snd_mask_any((snd_mask_t *) mask);
}

int snd_pcm_format_mask_test(const snd_pcm_format_mask_t *mask, snd_pcm_format_t val)
{
	return snd_mask_test((snd_mask_t *) mask, (unsigned long) val);
}

void snd_pcm_format_mask_set(snd_pcm_format_mask_t *mask, snd_pcm_format_t val)
{
	snd_mask_set((snd_mask_t *) mask, (unsigned long) val);
}

void snd_pcm_format_mask_reset(snd_pcm_format_mask_t *mask, snd_pcm_format_t val)
{
	snd_mask_reset((snd_mask_t *) mask, (unsigned long) val);
}


size_t snd_pcm_subformat_mask_sizeof()
{
	return sizeof(snd_pcm_subformat_mask_t);
}

int snd_pcm_subformat_mask_malloc(snd_pcm_subformat_mask_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_subformat_mask_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_pcm_subformat_mask_free(snd_pcm_subformat_mask_t *obj)
{
	free(obj);
}

void snd_pcm_subformat_mask_copy(snd_pcm_subformat_mask_t *dst, const snd_pcm_subformat_mask_t *src)
{
	assert(dst && src);
	*dst = *src;
}

void snd_pcm_subformat_mask_none(snd_pcm_subformat_mask_t *mask)
{
	snd_mask_none((snd_mask_t *) mask);
}

void snd_pcm_subformat_mask_any(snd_pcm_subformat_mask_t *mask)
{
	snd_mask_any((snd_mask_t *) mask);
}

int snd_pcm_subformat_mask_test(const snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val)
{
	return snd_mask_test((snd_mask_t *) mask, (unsigned long) val);
}

void snd_pcm_subformat_mask_set(snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val)
{
	snd_mask_set((snd_mask_t *) mask, (unsigned long) val);
}

void snd_pcm_subformat_mask_reset(snd_pcm_subformat_mask_t *mask, snd_pcm_subformat_t val)
{
	snd_mask_reset((snd_mask_t *) mask, (unsigned long) val);
}


size_t snd_pcm_hw_params_sizeof()
{
	return sizeof(snd_pcm_hw_params_t);
}

int snd_pcm_hw_params_malloc(snd_pcm_hw_params_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_hw_params_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_pcm_hw_params_free(snd_pcm_hw_params_t *obj)
{
	free(obj);
}

void snd_pcm_hw_params_copy(snd_pcm_hw_params_t *dst, const snd_pcm_hw_params_t *src)
{
	assert(dst && src);
	*dst = *src;
}

snd_pcm_access_t snd_pcm_hw_params_get_access(const snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_ACCESS, NULL));
}

int snd_pcm_hw_params_test_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_ACCESS, snd_enum_to_int(val), 0);
}

int snd_pcm_hw_params_set_access(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_ACCESS, snd_enum_to_int(val), 0);
}

snd_pcm_access_t snd_pcm_hw_params_set_access_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_ACCESS, NULL));
}

snd_pcm_access_t snd_pcm_hw_params_set_access_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_ACCESS, NULL));
}

int snd_pcm_hw_params_set_access_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_mask_t *mask)
{
	return snd_pcm_hw_param_set_mask(pcm, params, SND_TRY, SND_PCM_HW_PARAM_ACCESS, (snd_mask_t *) mask);
}


snd_pcm_format_t snd_pcm_hw_params_get_format(const snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_FORMAT, NULL));
}

int snd_pcm_hw_params_test_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_FORMAT, snd_enum_to_int(val), 0);
}

int snd_pcm_hw_params_set_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_FORMAT, snd_enum_to_int(val), 0);
}

snd_pcm_format_t snd_pcm_hw_params_set_format_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_FORMAT, NULL));
}

snd_pcm_format_t snd_pcm_hw_params_set_format_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_FORMAT, NULL));
}

int snd_pcm_hw_params_set_format_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_mask_t *mask)
{
	return snd_pcm_hw_param_set_mask(pcm, params, SND_TRY, SND_PCM_HW_PARAM_FORMAT, (snd_mask_t *) mask);
}


int snd_pcm_hw_params_test_subformat(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_SUBFORMAT, snd_enum_to_int(val), 0);
}

snd_pcm_subformat_t snd_pcm_hw_params_get_subformat(const snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_SUBFORMAT, NULL));
}

int snd_pcm_hw_params_set_subformat(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_SUBFORMAT, snd_enum_to_int(val), 0);
}

snd_pcm_subformat_t snd_pcm_hw_params_set_subformat_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_SUBFORMAT, NULL));
}

snd_pcm_subformat_t snd_pcm_hw_params_set_subformat_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_int_to_enum(snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_SUBFORMAT, NULL));
}

int snd_pcm_hw_params_set_subformat_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_subformat_mask_t *mask)
{
	return snd_pcm_hw_param_set_mask(pcm, params, SND_TRY, SND_PCM_HW_PARAM_SUBFORMAT, (snd_mask_t *) mask);
}


unsigned int snd_pcm_hw_params_get_channels(const snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_CHANNELS, NULL);
}

unsigned int snd_pcm_hw_params_get_channels_min(const snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_CHANNELS, NULL);
}

unsigned int snd_pcm_hw_params_get_channels_max(const snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_CHANNELS, NULL);
}

int snd_pcm_hw_params_test_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_CHANNELS, val, 0);
}

int snd_pcm_hw_params_set_channels(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, val, 0);
}

int snd_pcm_hw_params_set_channels_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

int snd_pcm_hw_params_set_channels_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

int snd_pcm_hw_params_set_channels_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, unsigned int *max)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_CHANNELS, min, NULL, max, NULL);
}

unsigned int snd_pcm_hw_params_set_channels_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_CHANNELS, val, NULL);
}

unsigned int snd_pcm_hw_params_set_channels_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_CHANNELS, NULL);
}

unsigned int snd_pcm_hw_params_set_channels_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_CHANNELS, NULL);
}


unsigned int snd_pcm_hw_params_get_rate(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_RATE, dir);
}

unsigned int snd_pcm_hw_params_get_rate_min(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_RATE, dir);
}

unsigned int snd_pcm_hw_params_get_rate_max(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_RATE, dir);
}

int snd_pcm_hw_params_test_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_RATE, val, dir);
}

int snd_pcm_hw_params_set_rate(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, val, dir);
}

int snd_pcm_hw_params_set_rate_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, val, dir);
}

int snd_pcm_hw_params_set_rate_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, val, dir);
}

int snd_pcm_hw_params_set_rate_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_RATE, min, mindir, max, maxdir);
}

unsigned int snd_pcm_hw_params_set_rate_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_RATE, val, dir);
}

unsigned int snd_pcm_hw_params_set_rate_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_RATE, dir);
}

unsigned int snd_pcm_hw_params_set_rate_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_RATE, dir);
}


unsigned int snd_pcm_hw_params_get_period_time(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_PERIOD_TIME, dir);
}

unsigned int snd_pcm_hw_params_get_period_time_min(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_PERIOD_TIME, dir);
}

unsigned int snd_pcm_hw_params_get_period_time_max(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_PERIOD_TIME, dir);
}

int snd_pcm_hw_params_test_period_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

int snd_pcm_hw_params_set_period_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

int snd_pcm_hw_params_set_period_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

int snd_pcm_hw_params_set_period_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

int snd_pcm_hw_params_set_period_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_TIME, min, mindir, max, maxdir);
}

unsigned int snd_pcm_hw_params_set_period_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_PERIOD_TIME, val, dir);
}

unsigned int snd_pcm_hw_params_set_period_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_PERIOD_TIME, dir);
}

unsigned int snd_pcm_hw_params_set_period_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_PERIOD_TIME, dir);
}


snd_pcm_uframes_t snd_pcm_hw_params_get_period_size(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_PERIOD_SIZE, dir);
}

snd_pcm_uframes_t snd_pcm_hw_params_get_period_size_min(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_PERIOD_SIZE, dir);
}

snd_pcm_uframes_t snd_pcm_hw_params_get_period_size_max(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_PERIOD_SIZE, dir);
}

int snd_pcm_hw_params_test_period_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_PERIOD_SIZE, val, dir);
}

int snd_pcm_hw_params_set_period_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, val, dir);
}

int snd_pcm_hw_params_set_period_size_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	*val = _val;
	return err;
}

int snd_pcm_hw_params_set_period_size_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val, int *dir)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, &_val, dir);
	*val = _val;
	return err;
}

int snd_pcm_hw_params_set_period_size_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *min, int *mindir, snd_pcm_uframes_t *max, int *maxdir)
{
	unsigned int _min = *min;
	unsigned int _max = *max;
	int err = snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE, &_min, mindir, &_max, maxdir);
	*min = _min;
	*max = _max;
	return err;
}

snd_pcm_uframes_t snd_pcm_hw_params_set_period_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val, int *dir)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_PERIOD_SIZE, val, dir);
}

snd_pcm_uframes_t snd_pcm_hw_params_set_period_size_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_PERIOD_SIZE, dir);
}

snd_pcm_uframes_t snd_pcm_hw_params_set_period_size_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_PERIOD_SIZE, dir);
}

int snd_pcm_hw_params_set_period_size_integer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_integer(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIOD_SIZE);
}


unsigned int snd_pcm_hw_params_get_periods(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_PERIODS, dir);
}

unsigned int snd_pcm_hw_params_get_periods_min(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_PERIODS, dir);
}

unsigned int snd_pcm_hw_params_get_periods_max(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_PERIODS, dir);
}

int snd_pcm_hw_params_test_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

int snd_pcm_hw_params_set_periods(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

int snd_pcm_hw_params_set_periods_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

int snd_pcm_hw_params_set_periods_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

int snd_pcm_hw_params_set_periods_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS, min, mindir, max, maxdir);
}

unsigned int snd_pcm_hw_params_set_periods_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_PERIODS, val, dir);
}

unsigned int snd_pcm_hw_params_set_periods_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_PERIODS, dir);
}

unsigned int snd_pcm_hw_params_set_periods_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_PERIODS, dir);
}

int snd_pcm_hw_params_set_periods_integer(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_integer(pcm, params, SND_TRY, SND_PCM_HW_PARAM_PERIODS);
}


unsigned int snd_pcm_hw_params_get_buffer_time(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_BUFFER_TIME, dir);
}

unsigned int snd_pcm_hw_params_get_buffer_time_min(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_BUFFER_TIME, dir);
}

unsigned int snd_pcm_hw_params_get_buffer_time_max(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_BUFFER_TIME, dir);
}

int snd_pcm_hw_params_test_buffer_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

int snd_pcm_hw_params_set_buffer_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

int snd_pcm_hw_params_set_buffer_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

int snd_pcm_hw_params_set_buffer_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

int snd_pcm_hw_params_set_buffer_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_TIME, min, mindir, max, maxdir);
}

unsigned int snd_pcm_hw_params_set_buffer_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_BUFFER_TIME, val, dir);
}

unsigned int snd_pcm_hw_params_set_buffer_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_BUFFER_TIME, dir);
}

unsigned int snd_pcm_hw_params_set_buffer_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_BUFFER_TIME, dir);
}


snd_pcm_uframes_t snd_pcm_hw_params_get_buffer_size(const snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_BUFFER_SIZE, NULL);
}

snd_pcm_uframes_t snd_pcm_hw_params_get_buffer_size_min(const snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_BUFFER_SIZE, NULL);
}

snd_pcm_uframes_t snd_pcm_hw_params_get_buffer_size_max(const snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_BUFFER_SIZE, NULL);
}

int snd_pcm_hw_params_test_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_BUFFER_SIZE, val, 0);
}

int snd_pcm_hw_params_set_buffer_size(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, val, 0);
}

int snd_pcm_hw_params_set_buffer_size_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	*val = _val;
	return err;
}

int snd_pcm_hw_params_set_buffer_size_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *val)
{
	unsigned int _val = *val;
	int err = snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, &_val, NULL);
	*val = _val;
	return err;
}

int snd_pcm_hw_params_set_buffer_size_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t *min, snd_pcm_uframes_t *max)
{
	unsigned int _min = *min;
	unsigned int _max = *max;
	int err = snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_BUFFER_SIZE, &_min, NULL, &_max, NULL);
	*min = _min;
	*max = _max;
	return err;
}

snd_pcm_uframes_t snd_pcm_hw_params_set_buffer_size_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_uframes_t val)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_BUFFER_SIZE, val, NULL);
}

snd_pcm_uframes_t snd_pcm_hw_params_set_buffer_size_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_BUFFER_SIZE, NULL);
}

snd_pcm_uframes_t snd_pcm_hw_params_set_buffer_size_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_BUFFER_SIZE, NULL);
}


unsigned int snd_pcm_hw_params_get_tick_time(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get(params, SND_PCM_HW_PARAM_TICK_TIME, dir);
}

unsigned int snd_pcm_hw_params_get_tick_time_min(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_min(params, SND_PCM_HW_PARAM_TICK_TIME, dir);
}

unsigned int snd_pcm_hw_params_get_tick_time_max(const snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_get_max(params, SND_PCM_HW_PARAM_TICK_TIME, dir);
}

int snd_pcm_hw_params_test_tick_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TEST, SND_PCM_HW_PARAM_TICK_TIME, val, dir);
}

int snd_pcm_hw_params_set_tick_time(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int dir)
{
	return snd_pcm_hw_param_set(pcm, params, SND_TRY, SND_PCM_HW_PARAM_TICK_TIME, val, dir);
}

int snd_pcm_hw_params_set_tick_time_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_min(pcm, params, SND_TRY, SND_PCM_HW_PARAM_TICK_TIME, val, dir);
}

int snd_pcm_hw_params_set_tick_time_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir)
{
	return snd_pcm_hw_param_set_max(pcm, params, SND_TRY, SND_PCM_HW_PARAM_TICK_TIME, val, dir);
}

int snd_pcm_hw_params_set_tick_time_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *min, int *mindir, unsigned int *max, int *maxdir)
{
	return snd_pcm_hw_param_set_minmax(pcm, params, SND_TRY, SND_PCM_HW_PARAM_TICK_TIME, min, mindir, max, maxdir);
}

unsigned int snd_pcm_hw_params_set_tick_time_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val, int *dir)
{
	return snd_pcm_hw_param_set_near(pcm, params, SND_PCM_HW_PARAM_TICK_TIME, val, dir);
}

unsigned int snd_pcm_hw_params_set_tick_time_first(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_first(pcm, params, SND_PCM_HW_PARAM_TICK_TIME, dir);
}

unsigned int snd_pcm_hw_params_set_tick_time_last(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, int *dir)
{
	return snd_pcm_hw_param_set_last(pcm, params, SND_PCM_HW_PARAM_TICK_TIME, dir);
}


size_t snd_pcm_sw_params_sizeof()
{
	return sizeof(snd_pcm_sw_params_t);
}

int snd_pcm_sw_params_malloc(snd_pcm_sw_params_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_sw_params_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_pcm_sw_params_free(snd_pcm_sw_params_t *obj)
{
	free(obj);
}

void snd_pcm_sw_params_copy(snd_pcm_sw_params_t *dst, const snd_pcm_sw_params_t *src)
{
	assert(dst && src);
	*dst = *src;
}

int snd_pcm_sw_params_set_start_mode(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_start_t val)
{
	assert(pcm && params);
	assert(val <= SND_PCM_START_LAST);
	params->start_mode = snd_enum_to_int(val);
	return 0;
}

snd_pcm_start_t snd_pcm_sw_params_get_start_mode(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return snd_int_to_enum(params->start_mode);
}


int snd_pcm_sw_params_set_xrun_mode(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_xrun_t val)
{
	assert(pcm && params);
	assert(val <= SND_PCM_XRUN_LAST);
	params->xrun_mode = snd_enum_to_int(val);
	return 0;
}

snd_pcm_xrun_t snd_pcm_sw_params_get_xrun_mode(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return snd_int_to_enum(params->xrun_mode);
}


int snd_pcm_sw_params_set_tstamp_mode(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_tstamp_t val)
{
	assert(pcm && params);
	assert(val <= SND_PCM_TSTAMP_LAST);
	params->tstamp_mode = snd_enum_to_int(val);
	return 0;
}

snd_pcm_tstamp_t snd_pcm_sw_params_get_tstamp_mode(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return snd_int_to_enum(params->tstamp_mode);
}


int snd_pcm_sw_params_set_period_step(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, unsigned int val)
{
	assert(pcm && params);
	params->period_step = val;
	return 0;
}

unsigned int snd_pcm_sw_params_get_period_step(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return params->period_step;
}


int snd_pcm_sw_params_set_sleep_min(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, unsigned int val)
{
	assert(pcm && params);
	params->sleep_min = val;
	return 0;
}

unsigned int snd_pcm_sw_params_get_sleep_min(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return params->sleep_min;
}


int snd_pcm_sw_params_set_avail_min(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
{
	assert(pcm && params);
	params->avail_min = val;
	return 0;
}

snd_pcm_uframes_t snd_pcm_sw_params_get_avail_min(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return params->avail_min;
}


int snd_pcm_sw_params_set_xfer_align(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
{
	assert(pcm && params);
	assert(val % pcm->min_align == 0);
	params->xfer_align = val;
	return 0;
}

snd_pcm_uframes_t snd_pcm_sw_params_get_xfer_align(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return params->xfer_align;
}


int snd_pcm_sw_params_set_silence_threshold(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
{
	assert(pcm && params);
	assert(val + params->silence_size <= pcm->buffer_size);
	params->silence_threshold = val;
	return 0;
}

snd_pcm_uframes_t snd_pcm_sw_params_get_silence_threshold(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return params->silence_threshold;
}


int snd_pcm_sw_params_set_silence_size(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t *params, snd_pcm_uframes_t val)
{
	assert(pcm && params);
	assert(val + params->silence_threshold <= pcm->buffer_size);
	params->silence_size = val;
	return 0;
}

snd_pcm_uframes_t snd_pcm_sw_params_get_silence_size(const snd_pcm_sw_params_t *params)
{
	assert(params);
	return params->silence_size;
}


size_t snd_pcm_status_sizeof()
{
	return sizeof(snd_pcm_status_t);
}

int snd_pcm_status_malloc(snd_pcm_status_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_status_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_pcm_status_free(snd_pcm_status_t *obj)
{
	free(obj);
}

void snd_pcm_status_copy(snd_pcm_status_t *dst, const snd_pcm_status_t *src)
{
	assert(dst && src);
	*dst = *src;
}

snd_pcm_state_t snd_pcm_status_get_state(const snd_pcm_status_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->state);
}

void snd_pcm_status_get_trigger_tstamp(const snd_pcm_status_t *obj, snd_timestamp_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->trigger_tstamp;
}

void snd_pcm_status_get_tstamp(const snd_pcm_status_t *obj, snd_timestamp_t *ptr)
{
	assert(obj && ptr);
	*ptr = obj->tstamp;
}

snd_pcm_sframes_t snd_pcm_status_get_delay(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->delay;
}

snd_pcm_uframes_t snd_pcm_status_get_avail(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->avail;
}

snd_pcm_uframes_t snd_pcm_status_get_avail_max(const snd_pcm_status_t *obj)
{
	assert(obj);
	return obj->avail_max;
}

size_t snd_pcm_info_sizeof()
{
	return sizeof(snd_pcm_info_t);
}

int snd_pcm_info_malloc(snd_pcm_info_t **ptr)
{
	assert(ptr);
	*ptr = calloc(1, sizeof(snd_pcm_info_t));
	if (!*ptr)
		return -ENOMEM;
	return 0;
}

void snd_pcm_info_free(snd_pcm_info_t *obj)
{
	free(obj);
}

void snd_pcm_info_copy(snd_pcm_info_t *dst, const snd_pcm_info_t *src)
{
	assert(dst && src);
	*dst = *src;
}

unsigned int snd_pcm_info_get_device(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->device;
}

unsigned int snd_pcm_info_get_subdevice(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->subdevice;
}

snd_pcm_stream_t snd_pcm_info_get_stream(const snd_pcm_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->stream);
}

int snd_pcm_info_get_card(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->card;
}

const char *snd_pcm_info_get_id(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->id;
}

const char *snd_pcm_info_get_name(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->name;
}

const char *snd_pcm_info_get_subdevice_name(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->subname;
}

snd_pcm_class_t snd_pcm_info_get_class(const snd_pcm_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->dev_class);
}

snd_pcm_subclass_t snd_pcm_info_get_subclass(const snd_pcm_info_t *obj)
{
	assert(obj);
	return snd_int_to_enum(obj->dev_subclass);
}

unsigned int snd_pcm_info_get_subdevices_count(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->subdevices_count;
}

unsigned int snd_pcm_info_get_subdevices_avail(const snd_pcm_info_t *obj)
{
	assert(obj);
	return obj->subdevices_avail;
}

void snd_pcm_info_set_device(snd_pcm_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->device = val;
}

void snd_pcm_info_set_subdevice(snd_pcm_info_t *obj, unsigned int val)
{
	assert(obj);
	obj->subdevice = val;
}

void snd_pcm_info_set_stream(snd_pcm_info_t *obj, snd_pcm_stream_t val)
{
	assert(obj);
	obj->stream = snd_enum_to_int(val);
}

