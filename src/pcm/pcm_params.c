/*
 *  PCM - Params functions
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
#define INTERVAL_INLINE
#define MASK_INLINE
#include "interval.h"
#include "mask.h"

static inline unsigned int add(unsigned int a, unsigned int b)
{
	if (a >= UINT_MAX - b)
		return UINT_MAX;
	return a + b;
}

static inline unsigned int sub(unsigned int a, unsigned int b)
{
	if (a > b)
		return a - b;
	return 0;
}

static inline int hw_is_mask(int var)
{
	return var >= SND_PCM_HW_PARAM_FIRST_MASK &&
		var <= SND_PCM_HW_PARAM_LAST_MASK;
}

static inline int hw_is_interval(int var)
{
	return var >= SND_PCM_HW_PARAM_FIRST_INTERVAL &&
		var <= SND_PCM_HW_PARAM_LAST_INTERVAL;
}

static inline mask_t *hw_param_mask(snd_pcm_hw_params_t *params,
				  unsigned int var)
{
	assert(hw_is_mask(var));
	return (mask_t*)&params->masks[var - SND_PCM_HW_PARAM_FIRST_MASK];
}

static inline interval_t *hw_param_interval(snd_pcm_hw_params_t *params,
					  unsigned int var)
{
	assert(hw_is_interval(var));
	return &params->intervals[var - SND_PCM_HW_PARAM_FIRST_INTERVAL];
}

static inline const mask_t *hw_param_mask_c(const snd_pcm_hw_params_t *params,
					  unsigned int var)
{
	return (const mask_t *)hw_param_mask((snd_pcm_hw_params_t*) params, var);
}

static inline const interval_t *hw_param_interval_c(const snd_pcm_hw_params_t *params,
						  unsigned int var)
{
	return (const interval_t *)hw_param_interval((snd_pcm_hw_params_t*) params, var);
}

void _snd_pcm_hw_param_any(snd_pcm_hw_params_t *params, unsigned int var)
{
	if (hw_is_mask(var)) {
		mask_all(hw_param_mask(params, var));
		params->appl_cmask |= 1 << var;
		return;
	}
	if (hw_is_interval(var)) {
		interval_all(hw_param_interval(params, var));
		params->appl_cmask |= 1 << var;
		return;
	}
	assert(0);
}

int snd_pcm_hw_param_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			 unsigned int var)
{
	_snd_pcm_hw_param_any(params, var);
	return snd_pcm_hw_refine(pcm, params);
}

void _snd_pcm_hw_params_any(snd_pcm_hw_params_t *params)
{
	unsigned int k;
	memset(params, 0, sizeof(*params));
	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; k++)
		_snd_pcm_hw_param_any(params, k);
	interval_setreal(hw_param_interval(params, SND_PCM_HW_PARAM_RATE));
	interval_setreal(hw_param_interval(params, SND_PCM_HW_PARAM_FRAGMENT_LENGTH));
	interval_setreal(hw_param_interval(params, SND_PCM_HW_PARAM_BUFFER_LENGTH));
	params->info = ~0U;
}

/* Fill PARAMS with full configuration space boundaries */
int snd_pcm_hw_params_any(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	_snd_pcm_hw_params_any(params);
	return snd_pcm_hw_refine(pcm, params);
}

/* Return the value for field PAR if it's fixed in configuration space 
   defined by PARAMS. Return -EINVAL otherwise
*/
int snd_pcm_hw_param_value(const snd_pcm_hw_params_t *params,
			    unsigned int var)
{
	if (hw_is_mask(var)) {
		const mask_t *mask = hw_param_mask_c(params, var);
		if (!mask_single(mask))
			return -EINVAL;
		return mask_value(mask);
	}
	if (hw_is_interval(var)) {
		const interval_t *i = hw_param_interval_c(params, var);
		if (!interval_single(i))
			return -EINVAL;
		return interval_value(i);
	}
	assert(0);
	return -EINVAL;
}

/* Return the minimum value for field PAR. */
unsigned int snd_pcm_hw_param_value_min(const snd_pcm_hw_params_t *params,
					 unsigned int var)
{
	if (hw_is_mask(var)) {
		return mask_min(hw_param_mask_c(params, var));
	}
	if (hw_is_interval(var)) {
		return interval_min(hw_param_interval_c(params, var));
	}
	assert(0);
	return -EINVAL;
}

/* Return the maximum value for field PAR. */
unsigned int snd_pcm_hw_param_value_max(const snd_pcm_hw_params_t *params,
					 unsigned int var)
{
	if (hw_is_mask(var)) {
		return mask_max(hw_param_mask_c(params, var));
	}
	if (hw_is_interval(var)) {
		return interval_max(hw_param_interval_c(params, var));
	}
	assert(0);
	return -EINVAL;
}

/* Return the mask for field PAR.
   This function can be called only for SND_PCM_HW_PARAM_ACCESS,
   SND_PCM_HW_PARAM_FORMAT, SND_PCM_HW_PARAM_SUBFORMAT. */
const mask_t *snd_pcm_hw_param_value_mask(const snd_pcm_hw_params_t *params,
					   unsigned int var)
{
	assert(hw_is_mask(var));
	return hw_param_mask_c(params, var);
}

/* Return the interval for field PAR.
   This function cannot be called for SND_PCM_HW_PARAM_ACCESS,
   SND_PCM_HW_PARAM_FORMAT, SND_PCM_HW_PARAM_SUBFORMAT. */
const interval_t *snd_pcm_hw_param_value_interval(const snd_pcm_hw_params_t *params,
						   unsigned int var)
{
	assert(hw_is_interval(var));
	return hw_param_interval_c(params, var);
}


/* --- Refinement functions --- */

int _snd_pcm_hw_param_first(snd_pcm_hw_params_t *params, int hw,
			    unsigned int var)
{
	int changed;
	if (hw_is_mask(var))
		changed = mask_refine_first(hw_param_mask(params, var));
	else if (hw_is_interval(var))
		changed = interval_refine_first(hw_param_interval(params, var));
	else {
		assert(0);
		return -EINVAL;
	}
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}


/* Inside configuration space defined by PARAMS remove from PAR all 
   values > minimum. Reduce configuration space accordingly.
   Return the minimum.
*/
int snd_pcm_hw_param_first(snd_pcm_t *pcm, 
			   snd_pcm_hw_params_t *params, unsigned int var)
{
	int changed = _snd_pcm_hw_param_first(params, 0, var);
	if (changed < 0)
		return changed;
	if (changed) {
		int err = snd_pcm_hw_refine(pcm, params);
		assert(err >= 0);
	}
	return snd_pcm_hw_param_value(params, var);
}

int _snd_pcm_hw_param_last(snd_pcm_hw_params_t *params, int hw,
			   unsigned int var)
{
	int changed;
	if (hw_is_mask(var))
		changed = mask_refine_last(hw_param_mask(params, var));
	else if (hw_is_interval(var))
		changed = interval_refine_last(hw_param_interval(params, var));
	else {
		assert(0);
		return -EINVAL;
	}
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}


/* Inside configuration space defined by PARAMS remove from PAR all 
   values < maximum. Reduce configuration space accordingly.
   Return the maximum.
*/
int snd_pcm_hw_param_last(snd_pcm_t *pcm, 
			  snd_pcm_hw_params_t *params, unsigned int var)
{
	int changed = _snd_pcm_hw_param_last(params, 0, var);
	if (changed < 0)
		return changed;
	if (changed) {
		int err = snd_pcm_hw_refine(pcm, params);
		assert(err >= 0);
	}
	return snd_pcm_hw_param_value(params, var);
}

int _snd_pcm_hw_param_min(snd_pcm_hw_params_t *params, int hw,
			  unsigned int var, unsigned int val)
{
	int changed;
	if (hw_is_mask(var))
		changed = mask_refine_min(hw_param_mask(params, var), val);
	else if (hw_is_interval(var))
		changed = interval_refine_min(hw_param_interval(params, var), val);
	else {
		assert(0);
		return -EINVAL;
	}
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}

/* Inside configuration space defined by PARAMS remove from PAR all 
   values < VAL. Reduce configuration space accordingly.
   Return new minimum or -EINVAL if the configuration space is empty
*/
int snd_pcm_hw_param_min(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  unsigned int var, unsigned int val)
{
	int changed = _snd_pcm_hw_param_min(params, 0, var, val);
	if (changed < 0)
		return changed;
	if (changed) {
		int err = snd_pcm_hw_refine(pcm, params);
		if (err < 0)
			return err;
	}
	return snd_pcm_hw_param_value_min(params, var);
}

int snd_pcm_hw_param_min_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     unsigned int var, unsigned int val)
{
	snd_pcm_hw_params_t save;
	int err;
	save = *params;
	err = snd_pcm_hw_param_min(pcm, params, var, val);
	if (err < 0)
		*params = save;
	return err;
}

int _snd_pcm_hw_param_max(snd_pcm_hw_params_t *params, int hw,
			   unsigned int var, unsigned int val)
{
	int changed;
	if (hw_is_mask(var))
		changed = mask_refine_max(hw_param_mask(params, var), val);
	else if (hw_is_interval(var))
		changed = interval_refine_max(hw_param_interval(params, var), val);
	else {
		assert(0);
		return -EINVAL;
	}
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}

/* Inside configuration space defined by PARAMS remove from PAR all 
   values >= VAL + 1. Reduce configuration space accordingly.
   Return new maximum or -EINVAL if the configuration space is empty
*/
int snd_pcm_hw_param_max(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  unsigned int var, unsigned int val)
{
	int changed = _snd_pcm_hw_param_max(params, 0, var, val);
	if (changed < 0)
		return changed;
	if (changed) {
		int err = snd_pcm_hw_refine(pcm, params);
		if (err < 0)
			return err;
	}
	return snd_pcm_hw_param_value_max(params, var);
}

int snd_pcm_hw_param_max_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     unsigned int var, unsigned int val)
{
	snd_pcm_hw_params_t save;
	int err;
	save = *params;
	err = snd_pcm_hw_param_max(pcm, params, var, val);
	if (err < 0)
		*params = save;
	return err;
}

int _snd_pcm_hw_param_minmax(snd_pcm_hw_params_t *params, int hw,
			      unsigned int var,
			      unsigned int min, unsigned int max)
{
	int changed, c1, c2;
	if (hw_is_mask(var)) {
		mask_t *mask = hw_param_mask(params, var);
		c1 = mask_refine_min(mask, min);
		if (c1 < 0)
			changed = c1;
		else {
			c2 = mask_refine_max(mask, max);
			if (c2 < 0)
				changed = c2;
			else
				changed = (c1 || c2);
		}
	}
	else if (hw_is_interval(var)) {
		interval_t *i = hw_param_interval(params, var);
		c1 = interval_refine_min(i, min);
		if (c1 < 0)
			changed = c1;
		else {
			c2 = interval_refine_max(i, max);
			if (c2 < 0)
				changed = c2;
			else
				changed = (c1 || c2);
		}
	} else {
		assert(0);
		return -EINVAL;
	}
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}

/* Inside configuration space defined by PARAMS remove from PAR all 
   values < MIN and all values > MAX. Reduce configuration space accordingly.
   Return 0 or -EINVAL if the configuration space is empty
*/
int snd_pcm_hw_param_minmax(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     unsigned int var,
			     unsigned int min, unsigned int max)
{
	int changed = _snd_pcm_hw_param_minmax(params, 0, var, min, max);
	if (changed < 0)
		return changed;
	if (changed) {
		int err = snd_pcm_hw_refine(pcm, params);
		if (err < 0)
			return err;
	}
	return 0;
}

int snd_pcm_hw_param_minmax_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
				unsigned int var,
				unsigned int min, unsigned int max)
{
	snd_pcm_hw_params_t save;
	int err;
	save = *params;
	err = snd_pcm_hw_param_minmax(pcm, params, var, min, max);
	if (err < 0)
		*params = save;
	return err;
}

int _snd_pcm_hw_param_set(snd_pcm_hw_params_t *params, int hw,
			   unsigned int var, unsigned int val)
{
	int changed;
	if (hw_is_mask(var))
		changed = mask_refine_set(hw_param_mask(params, var), val);
	else if (hw_is_interval(var))
		changed = interval_refine_set(hw_param_interval(params, var), val);
	else {
		assert(0);
		return -EINVAL;
	}
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}

/* Inside configuration space defined by PARAMS remove from PAR all 
   values < VAL and >= VAL +1. Reduce configuration space accordingly.
   Return VAL or -EINVAL if the configuration space is empty
*/
int snd_pcm_hw_param_set(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  unsigned int var, unsigned int val)
{
	int changed = _snd_pcm_hw_param_set(params, 0, var, val);
	if (changed < 0)
		return changed;
	if (changed) {
		int err = snd_pcm_hw_refine(pcm, params);
		if (err < 0)
			return err;
	}
	return snd_pcm_hw_param_value(params, var);
}

int snd_pcm_hw_param_set_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			     unsigned int var, unsigned int val)
{
	snd_pcm_hw_params_t save;
	int err;
	save = *params;
	err = snd_pcm_hw_param_set(pcm, params, var, val);
	if (err < 0)
		*params = save;
	return err;
}

int _snd_pcm_hw_param_mask(snd_pcm_hw_params_t *params, int hw,
			    unsigned int var, const mask_t *val)
{
	int changed;
	assert(hw_is_mask(var));
	changed = mask_refine(hw_param_mask(params, var), val);
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}

/* Inside configuration space defined by PARAMS remove from PAR all values
   not contained in MASK. Reduce configuration space accordingly.
   This function can be called only for SND_PCM_HW_PARAM_ACCESS,
   SND_PCM_HW_PARAM_FORMAT, SND_PCM_HW_PARAM_SUBFORMAT.
   Return 0 on success or -EINVAL
   if the configuration space is empty
*/
int snd_pcm_hw_param_mask(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			  unsigned int var, const mask_t *val)
{
	int changed = _snd_pcm_hw_param_mask(params, 0, var, val);
	if (changed < 0)
		return changed;
	if (changed) {
		int err = snd_pcm_hw_refine(pcm, params);
		if (err < 0)
			return err;
	}
	return 0;
}

int snd_pcm_hw_param_mask_try(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			      unsigned int var, const mask_t *val)
{
	snd_pcm_hw_params_t save;
	int err;
	save = *params;
	err = snd_pcm_hw_param_mask(pcm, params, var, val);
	if (err < 0)
		*params = save;
	return err;
}

/* Inside configuration space defined by PARAMS set PAR to the available value
   nearest to VAL. Reduce configuration space accordingly.
   This function cannot be called for SND_PCM_HW_PARAM_ACCESS,
   SND_PCM_HW_PARAM_FORMAT, SND_PCM_HW_PARAM_SUBFORMAT.
   Return the value found.
 */
int snd_pcm_hw_param_near(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			   unsigned int var, unsigned int val)
{
	snd_pcm_hw_params_t save;
	int v;
	unsigned int max1 = val, min2 = add(val, 1);
	unsigned int hw_cmask;
	save = *params;
	v = snd_pcm_hw_param_max(pcm, params, var, max1);
	if (v >= 0) {
		int v1;
		snd_pcm_hw_params_t params1;
		if (val == (unsigned int)v)
			goto _end;
		params1 = save;
		v1 = snd_pcm_hw_param_min(pcm, &params1, var, min2);
		if (v1 < 0)
			goto _end;
		if (val - v > v1 - val) {
			*params = params1;
			v = v1;
		}
	} else {
		*params = save;
		v = snd_pcm_hw_param_min(pcm, params, var, min2);
		assert(v >= 0);
	}
 _end:
	hw_cmask = params->hw_cmask;
	v = snd_pcm_hw_param_set(pcm, params, var, v);
	params->hw_cmask |= hw_cmask;
	assert(v >= 0);
	return v;
}

/* Inside configuration space defined by PARAMS set PAR to the available value
   nearest to VAL after OLD (values less than VAL are returned first).
   Reduce configuration space accordingly.
   This function cannot be called for SND_PCM_HW_PARAM_ACCESS,
   SND_PCM_HW_PARAM_FORMAT, SND_PCM_HW_PARAM_SUBFORMAT.
   Return the value found.
 */
int snd_pcm_hw_param_next(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			   unsigned int var, unsigned int val,
			   unsigned int old)
{
	snd_pcm_hw_params_t save;
	int v;
	unsigned int max1, min2;
	int diff = old - val;
	if (diff < 0) {
		max1 = sub(old, 1);
		min2 = add(val, -diff);
	} else {
		max1 = sub(val, diff + 1);
		min2 = add(old, 1);
	}
	save = *params;
	v = snd_pcm_hw_param_max(pcm, params, var, max1);
	if (v >= 0) {
		int v1;
		snd_pcm_hw_params_t params1;
		if (val == (unsigned int)v)
			goto _end;
		params1 = save;
		v1 = snd_pcm_hw_param_min(pcm, &params1, var, min2);
		if (v1 < 0)
			goto _end;
		if (val - v > v1 - val) {
			*params = params1;
			v = v1;
		}
	} else {
		*params = save;
		v = snd_pcm_hw_param_min(pcm, params, var, min2);
		if (v < 0)
			return v;
	}
 _end:
	v = snd_pcm_hw_param_set(pcm, params, var, v);
	return v;
}


/* ---- end of refinement functions ---- */

int snd_pcm_hw_param_empty(const snd_pcm_hw_params_t *params,
			    unsigned int var)
{
	if (hw_is_mask(var))
		return mask_empty(hw_param_mask_c(params, var));
	if (hw_is_interval(var))
		return interval_empty(hw_param_interval_c(params, var));
	assert(0);
	return -EINVAL;
}

/* Return rate numerator/denumerator obtainable for configuration space defined
   by PARAMS */
int snd_pcm_hw_params_info_rate(const snd_pcm_hw_params_t *params,
				unsigned int *rate_num, unsigned int *rate_den)
{
	if (params->rate_den == 0)
		return -EINVAL;
	*rate_num = params->rate_num;
	*rate_den = params->rate_den;
	return 0;
}

/* Return significative bits in sample for configuration space defined
   by PARAMS */
int snd_pcm_hw_params_info_msbits(const snd_pcm_hw_params_t *params)
{
	if (params->msbits == 0)
		return -EINVAL;
	return params->msbits;
}

/* Return info for configuration space defined by PARAMS */
int snd_pcm_hw_params_info_flags(const snd_pcm_hw_params_t *params)
{
	if (params->info == ~0U)
		return -EINVAL;
	return params->info;
}

/* Return fifo size for configuration space defined by PARAMS */
int snd_pcm_hw_params_info_fifo_size(const snd_pcm_hw_params_t *params)
{
	if (params->fifo_size == 0)
		return -EINVAL;
	return params->fifo_size;
}

/* Choose one configuration from configuration space defined by PARAMS
   The configuration choosen is that obtained fixing in this order:
   first access
   first format
   first subformat
   min channels
   min rate
   min fragment size
   max fragments
*/
void snd_pcm_hw_params_choose(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int err;
	unsigned int hw_cmask = params->hw_cmask;

	err = snd_pcm_hw_param_first(pcm, params, SND_PCM_HW_PARAM_ACCESS);
	assert(err >= 0);
	hw_cmask |= params->hw_cmask;

	err = snd_pcm_hw_param_first(pcm, params, SND_PCM_HW_PARAM_FORMAT);
	assert(err >= 0);
	hw_cmask |= params->hw_cmask;

	err = snd_pcm_hw_param_first(pcm, params, SND_PCM_HW_PARAM_SUBFORMAT);
	assert(err >= 0);
	hw_cmask |= params->hw_cmask;

	err = snd_pcm_hw_param_first(pcm, params, SND_PCM_HW_PARAM_CHANNELS);
	assert(err >= 0);
	hw_cmask |= params->hw_cmask;

	err = snd_pcm_hw_param_first(pcm, params, SND_PCM_HW_PARAM_RATE);
	assert(err >= 0);
	hw_cmask |= params->hw_cmask;

	err = snd_pcm_hw_param_first(pcm, params, SND_PCM_HW_PARAM_FRAGMENT_SIZE);
	assert(err >= 0);
	hw_cmask |= params->hw_cmask;

	err = snd_pcm_hw_param_last(pcm, params, SND_PCM_HW_PARAM_FRAGMENTS);
	assert(err >= 0);
	hw_cmask |= params->hw_cmask;

	params->hw_cmask = hw_cmask;
}

/* Strategies */

struct _snd_pcm_hw_strategy {
	unsigned int badness_min, badness_max;
	int (*choose_param)(const snd_pcm_hw_params_t *params,
			    snd_pcm_t *pcm,
			    const snd_pcm_hw_strategy_t *strategy);
	int (*next_value)(snd_pcm_hw_params_t *params,
			  unsigned int param,
			  int value,
			  snd_pcm_t *pcm,
			  const snd_pcm_hw_strategy_t *strategy);
	int (*min_badness)(const snd_pcm_hw_params_t *params,
			   unsigned int max_badness,
			   snd_pcm_t *pcm,
			   const snd_pcm_hw_strategy_t *strategy);
	void *private;
	void (*free)(snd_pcm_hw_strategy_t *strategy);
};

/* Independent badness */
typedef struct _snd_pcm_hw_strategy_simple snd_pcm_hw_strategy_simple_t;

struct _snd_pcm_hw_strategy_simple {
	int valid;
	unsigned int order;
	int (*next_value)(snd_pcm_hw_params_t *params,
			  unsigned int param,
			  int value,
			  snd_pcm_t *pcm,
			  const snd_pcm_hw_strategy_simple_t *par);
	unsigned int (*min_badness)(const snd_pcm_hw_params_t *params,
				    unsigned int param,
				    snd_pcm_t *pcm,
				    const snd_pcm_hw_strategy_simple_t *par);
	void *private;
	void (*free)(snd_pcm_hw_strategy_simple_t *strategy);
};

typedef struct _snd_pcm_hw_strategy_simple_near {
	int best;
	unsigned int mul;
} snd_pcm_hw_strategy_simple_near_t;

typedef struct _snd_pcm_hw_strategy_simple_choices {
	unsigned int count;
	/* choices need to be sorted on ascending badness */
	snd_pcm_hw_strategy_simple_choices_list_t *choices;
} snd_pcm_hw_strategy_simple_choices_t;

int snd_pcm_hw_param_test(const snd_pcm_hw_params_t *params,
			   unsigned int var, unsigned int val)
{
	if (hw_is_mask(var)) {
		const mask_t *mask = hw_param_mask_c(params, var);
		return mask_test(mask, val);
	}
	if (hw_is_interval(var)) {
		const interval_t *i = hw_param_interval_c(params, var);
		return interval_test(i, val);
	}
	assert(0);
	return -EINVAL;
}

unsigned int snd_pcm_hw_param_count(const snd_pcm_hw_params_t *params,
				     unsigned int var)
{
	if (hw_is_mask(var)) {
		const mask_t *mask = hw_param_mask_c(params, var);
		return mask_count(mask);
	}
	if (hw_is_interval(var)) {
		const interval_t *i = hw_param_interval_c(params, var);
		return interval_max(i) - interval_min(i) + 1;
	}
	assert(0);
	return 0;
}

int _snd_pcm_hw_param_refine(snd_pcm_hw_params_t *params, int hw,
			     unsigned int var,
			     const snd_pcm_hw_params_t *src)
{
	int changed = 0;
	if (hw_is_mask(var)) {
		mask_t *d = hw_param_mask(params, var);
		const mask_t *s = hw_param_mask_c(src, var);
		changed = mask_refine(d, s);
	} else if (hw_is_interval(var)) {
		interval_t *d = hw_param_interval(params, var);
		const interval_t *s = hw_param_interval_c(src, var);
		changed = interval_refine(d, s);
	} else
		assert(0);
	if (changed) {
		if (hw)
			params->hw_cmask |= 1 << var;
		else
			params->appl_cmask |= 1 << var;
	}
	return changed;
}
			     
void snd_pcm_hw_param_copy(snd_pcm_hw_params_t *params, unsigned int var,
			    const snd_pcm_hw_params_t *src)
{
	if (hw_is_mask(var)) {
		mask_t *d = hw_param_mask(params, var);
		const mask_t *s = hw_param_mask_c(src, var);
		mask_copy(d, s);
	}
	if (hw_is_interval(var)) {
		interval_t *d = hw_param_interval(params, var);
		const interval_t *s = hw_param_interval_c(src, var);
		interval_copy(d, s);
	}
	assert(0);
}

void snd_pcm_hw_param_dump(const snd_pcm_hw_params_t *params,
			   unsigned int var, FILE *fp)
{
	static const char *(*funcs[])(unsigned int k) = {
		[SND_PCM_HW_PARAM_ACCESS] = snd_pcm_access_name,
		[SND_PCM_HW_PARAM_FORMAT] = snd_pcm_format_name,
		[SND_PCM_HW_PARAM_SUBFORMAT] = snd_pcm_subformat_name,
	};
	if (hw_is_mask(var)) {
		const mask_t *mask = hw_param_mask_c(params, var);
		if (mask_empty(mask))
			fputs(" NONE", fp);
		else if (mask_full(mask))
			fputs(" ALL", fp);
		else {
			unsigned int k;
			const char *(*f)(unsigned int k);
			assert(var < sizeof(funcs) / sizeof(funcs[0]));
			f = funcs[var];
			assert(f);
			for (k = 0; k <= MASK_MAX; ++k) {
				if (mask_test(mask, k)) {
					putc(' ', fp);
					fputs(f(k), fp);
				}
			}
		}
		return;
	}
	if (hw_is_interval(var)) {
		interval_print(hw_param_interval_c(params, var), fp);
		return;
	}
	assert(0);
}

int snd_pcm_hw_params_dump(snd_pcm_hw_params_t *params, FILE *fp)
{
	unsigned int k;
	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; k++) {
		fprintf(fp, "%s: ", snd_pcm_hw_param_name(k));
		snd_pcm_hw_param_dump(params, k, fp);
		putc('\n', fp);
	}
	return 0;
}

int snd_pcm_hw_params_strategy(snd_pcm_t *pcm, snd_pcm_hw_params_t *params,
			       const snd_pcm_hw_strategy_t *strategy,
			       unsigned int badness_min,
			       unsigned int badness_max)
{
	snd_pcm_hw_params_t best_params;
	int var;
	int value;
	unsigned int best_badness;
	int badness = strategy->min_badness(params, badness_max, pcm, strategy);
	snd_pcm_hw_params_t params1;
#if 0
	printf("\nBadness: %d\n", badness);
	snd_pcm_hw_params_dump(params, stdout);
#endif
	if (badness < 0)
		return badness;
	if ((unsigned int)badness > badness_min)
		badness_min = badness_min;
	var = strategy->choose_param(params, pcm, strategy);
	if (var < 0)
		return badness;
	best_badness = UINT_MAX;
	value = -1;
	while (1) {
		unsigned int hw_cmask;
		params1 = *params;
		value = strategy->next_value(&params1, var, value, pcm, strategy);
		if (value < 0)
			break;
		hw_cmask = params1.hw_cmask;
		badness = snd_pcm_hw_params_strategy(pcm, &params1, strategy, badness_min, badness_max);
		params1.hw_cmask |= hw_cmask;
		if (badness >= 0) {
			if ((unsigned int) badness <= badness_min) {
				*params = params1;
				return badness;
			}
			best_badness = badness;
			best_params = params1;
			badness_max = badness - 1;
		}
	}
	if (best_badness == UINT_MAX) {
		return -EINVAL;
	}
	*params = best_params;
	return best_badness;
}

void snd_pcm_hw_strategy_simple_free(snd_pcm_hw_strategy_t *strategy)
{
	snd_pcm_hw_strategy_simple_t *pars = strategy->private;
	int k;
	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; ++k) {
		if (pars[k].valid && pars[k].free)
			pars[k].free(&pars[k]);
	}
	free(pars);
}

int snd_pcm_hw_strategy_simple_choose_param(const snd_pcm_hw_params_t *params,
					 snd_pcm_t *pcm ATTRIBUTE_UNUSED,
					 const snd_pcm_hw_strategy_t *strategy)
{
	unsigned int var;
	int best_var = -1;
	const snd_pcm_hw_strategy_simple_t *pars = strategy->private;
	unsigned int min_choices = UINT_MAX;
	unsigned int min_order = UINT_MAX;
	for (var = 0; var <= SND_PCM_HW_PARAM_LAST; ++var) {
		const snd_pcm_hw_strategy_simple_t *p = &pars[var];
		unsigned int choices;
		if (!p->valid)
			continue;
		choices = snd_pcm_hw_param_count(params, var);
		if (choices == 1)
			continue;
		assert(choices != 0);
		if (p->order < min_order ||
		    (p->order == min_order &&
		     choices < min_choices)) {
			min_order = p->order;
			min_choices = choices;
			best_var = var;
		}
	}
	return best_var;
}

int snd_pcm_hw_strategy_simple_next_value(snd_pcm_hw_params_t *params,
				       unsigned int var,
				       int value,
				       snd_pcm_t *pcm,
				       const snd_pcm_hw_strategy_t *strategy)
{
	const snd_pcm_hw_strategy_simple_t *pars = strategy->private;
	assert(pars[var].valid);
	return pars[var].next_value(params, var, value, pcm, &pars[var]);
}


int snd_pcm_hw_strategy_simple_min_badness(const snd_pcm_hw_params_t *params,
					unsigned int max_badness,
					snd_pcm_t *pcm,
					const snd_pcm_hw_strategy_t *strategy)
{
	unsigned int var;
	unsigned int badness = 0;
	const snd_pcm_hw_strategy_simple_t *pars = strategy->private;
	for (var = 0; var <= SND_PCM_HW_PARAM_LAST; ++var) {
		unsigned int b;
		if (!pars[var].valid)
			continue;
		b = pars[var].min_badness(params, var, pcm, &pars[var]);
		if (b > max_badness || max_badness - b < badness)
			return -E2BIG;
		badness += b;
	}
	return badness;
}


void snd_pcm_hw_strategy_simple_near_free(snd_pcm_hw_strategy_simple_t *par)
{
	snd_pcm_hw_strategy_simple_near_t *p = par->private;
	free(p);
}

unsigned int snd_pcm_hw_strategy_simple_near_min_badness(const snd_pcm_hw_params_t *params,
						      unsigned int var,
						      snd_pcm_t *pcm,
						      const snd_pcm_hw_strategy_simple_t *par)
{
	const snd_pcm_hw_strategy_simple_near_t *p = par->private;
	snd_pcm_hw_params_t params1 = *params;
	int value = snd_pcm_hw_param_near(pcm, &params1, var, p->best);
	int diff;
	assert(value >= 0);
	diff = p->best - value;
	if (diff < 0)
		diff = -diff;
	return diff * p->mul;
}
	
int snd_pcm_hw_strategy_simple_near_next_value(snd_pcm_hw_params_t *params,
					    unsigned int var,
					    int value,
					    snd_pcm_t *pcm,
					    const snd_pcm_hw_strategy_simple_t *par)
{
	const snd_pcm_hw_strategy_simple_near_t *p = par->private;
	if (value < 0) 
		return snd_pcm_hw_param_near(pcm, params, var, p->best);
	else
		return snd_pcm_hw_param_next(pcm, params, var, p->best, value);
}

void snd_pcm_hw_strategy_simple_choices_free(snd_pcm_hw_strategy_simple_t *par)
{
	snd_pcm_hw_strategy_simple_choices_t *p = par->private;
//	free(p->choices);
	free(p);
}

unsigned int snd_pcm_hw_strategy_simple_choices_min_badness(const snd_pcm_hw_params_t *params,
							 unsigned int var,
							 snd_pcm_t *pcm ATTRIBUTE_UNUSED,
							 const snd_pcm_hw_strategy_simple_t *par)
{
	const snd_pcm_hw_strategy_simple_choices_t *p = par->private;
	unsigned int k;
	for (k = 0; k < p->count; ++k) {
		if (snd_pcm_hw_param_test(params, var, p->choices[k].value))
			return p->choices[k].badness;
	}
	assert(0);
	return UINT_MAX;
}
	
int snd_pcm_hw_strategy_simple_choices_next_value(snd_pcm_hw_params_t *params,
					       unsigned int var,
					       int value,
					       snd_pcm_t *pcm,
					       const snd_pcm_hw_strategy_simple_t *par)
{
	const snd_pcm_hw_strategy_simple_choices_t *p = par->private;
	unsigned int k = 0;
	if (value >= 0) {
		for (; k < p->count; ++k) {
			if (p->choices[k].value == (unsigned int) value) {
				k++;
				break;
			}
		}
	}
	for (; k < p->count; ++k) {
		unsigned int v = p->choices[k].value;
		if (snd_pcm_hw_param_test(params, var, v)) {
			snd_pcm_hw_params_t save = *params;
			int err = snd_pcm_hw_param_set(pcm, params, var, v);
			if (err < 0) {
				*params = save;
				continue;
			}
			return v;
		}
	}
	return -1;
}

int snd_pcm_hw_strategy_free(snd_pcm_hw_strategy_t *strategy)
{
	if (strategy->free)
		strategy->free(strategy);
	free(strategy);
	return 0;
}

int snd_pcm_hw_strategy_simple(snd_pcm_hw_strategy_t **strategyp,
			    unsigned int badness_min,
			    unsigned int badness_max)
{
	snd_pcm_hw_strategy_simple_t *data;
	snd_pcm_hw_strategy_t *s;
	assert(strategyp);
	data = calloc(SND_PCM_HW_PARAM_LAST + 1, sizeof(*data));
	if (!data)
		return -ENOMEM;
	s = calloc(1, sizeof(*s));
	if (!s) {
		free(data);
		return -ENOMEM;
	}
	s->choose_param = snd_pcm_hw_strategy_simple_choose_param;
	s->next_value = snd_pcm_hw_strategy_simple_next_value;
	s->min_badness = snd_pcm_hw_strategy_simple_min_badness;
	s->badness_min = badness_min;
	s->badness_max = badness_max;
	s->private = data;
	s->free = snd_pcm_hw_strategy_simple_free;
	*strategyp = s;
	return 0;
}

int snd_pcm_hw_strategy_simple_near(snd_pcm_hw_strategy_t *strategy,
				 int order,
				 unsigned int var,
				 unsigned int best,
				 unsigned int mul)
{
	snd_pcm_hw_strategy_simple_t *s = strategy->private;
	snd_pcm_hw_strategy_simple_near_t *data;
	assert(strategy);
	assert(var <= SND_PCM_HW_PARAM_LAST);
	assert(!s->valid);
	data = calloc(1, sizeof(*data));
	if (!data)
		return -ENOMEM;
	data->best = best;
	data->mul = mul;
	s += var;
	s->order = order;
	s->valid = 1;
	s->next_value = snd_pcm_hw_strategy_simple_near_next_value;
	s->min_badness = snd_pcm_hw_strategy_simple_near_min_badness;
	s->private = data;
	s->free = snd_pcm_hw_strategy_simple_near_free;
	return 0;
}

int snd_pcm_hw_strategy_simple_choices(snd_pcm_hw_strategy_t *strategy,
				    int order,
				    unsigned int var,
				    unsigned int count,
				    snd_pcm_hw_strategy_simple_choices_list_t *choices)
{
	snd_pcm_hw_strategy_simple_t *s = strategy->private;
	snd_pcm_hw_strategy_simple_choices_t *data;
	assert(strategy);
	assert(var <= SND_PCM_HW_PARAM_LAST);
	assert(!s->valid);
	data = calloc(1, sizeof(*data));
	if (!data)
		return -ENOMEM;
	data->count = count;
	data->choices = choices;
	s += var;
	s->valid = 1;
	s->order = order;
	s->next_value = snd_pcm_hw_strategy_simple_choices_next_value;
	s->min_badness = snd_pcm_hw_strategy_simple_choices_min_badness;
	s->private = data;
	s->free = snd_pcm_hw_strategy_simple_choices_free;
	return 0;
}

int snd_pcm_hw_params_try_explain_failure1(snd_pcm_t *pcm,
					   snd_pcm_hw_params_t *fail,
					   snd_pcm_hw_params_t *success,
					   unsigned int depth,
					   FILE *fp)
{
	unsigned int var;
	snd_pcm_hw_params_t i;
	if (depth < 1)
		return -ENOENT;
	for (var = 0; var <= SND_PCM_HW_PARAM_LAST; var++) {
		int err;
		i = *success;
		snd_pcm_hw_param_copy(&i, var, fail);
		err = snd_pcm_hw_refine(pcm, &i);
		if (err == 0 && 
		    snd_pcm_hw_params_try_explain_failure1(pcm, fail, &i, depth - 1, fp) < 0)
			continue;
		fprintf(fp, "%s: ", snd_pcm_hw_param_name(var));
		snd_pcm_hw_param_dump(fail, var, fp);
		putc('\n', fp);
		return 0;
	}
	return -ENOENT;
}

int snd_pcm_hw_params_try_explain_failure(snd_pcm_t *pcm,
					  snd_pcm_hw_params_t *fail,
					  snd_pcm_hw_params_t *success,
					  unsigned int depth,
					  FILE *fp)
{
	snd_pcm_hw_params_t i, any;
	int err;
	unsigned int var;
	int done = 0;
	assert(pcm && fail);
	for (var = 0; var <= SND_PCM_HW_PARAM_LAST; var++) {
		if (!snd_pcm_hw_param_empty(fail, var))
			continue;
		fprintf(fp, "%s is empty\n", snd_pcm_hw_param_name(var));
		done = 1;
	}
	if (done)
		return 0;
	i = *fail;
	err = snd_pcm_hw_refine(pcm, &i);
	if (err == 0) {
		fprintf(fp, "Configuration is virtually correct\n");
		return 0;
	}
	if (!success) {
		snd_pcm_hw_params_any(pcm, &any);
		success = &any;
	}
	return snd_pcm_hw_params_try_explain_failure1(pcm, fail, success, depth, fp);
}

typedef struct _snd_pcm_hw_rule snd_pcm_hw_rule_t;

typedef int (*snd_pcm_hw_rule_func_t)(snd_pcm_hw_params_t *params,
				      snd_pcm_hw_rule_t *rule);

struct _snd_pcm_hw_rule {
	int var;
	snd_pcm_hw_rule_func_t func;
	int deps[4];
	void *private;
};

int snd_pcm_hw_rule_mul(snd_pcm_hw_params_t *params,
			snd_pcm_hw_rule_t *rule)
{
	return interval_mul(hw_param_interval(params, rule->var),
			    hw_param_interval(params, rule->deps[0]),
			    hw_param_interval(params, rule->deps[1]));
}

int snd_pcm_hw_rule_div(snd_pcm_hw_params_t *params,
			snd_pcm_hw_rule_t *rule)
{
	return interval_div(hw_param_interval(params, rule->var),
			    hw_param_interval(params, rule->deps[0]),
			    hw_param_interval(params, rule->deps[1]));
}

int snd_pcm_hw_rule_muldivk(snd_pcm_hw_params_t *params,
			    snd_pcm_hw_rule_t *rule)
{
	return interval_muldivk(hw_param_interval(params, rule->var),
				(unsigned long) rule->private,
				hw_param_interval(params, rule->deps[0]),
				hw_param_interval(params, rule->deps[1]));
}

int snd_pcm_hw_rule_mulkdiv(snd_pcm_hw_params_t *params,
			    snd_pcm_hw_rule_t *rule)
{
	return interval_mulkdiv(hw_param_interval(params, rule->var),
				(unsigned long) rule->private,
				hw_param_interval(params, rule->deps[0]),
				hw_param_interval(params, rule->deps[1]));
}

int snd_pcm_hw_rule_format(snd_pcm_hw_params_t *params,
			   snd_pcm_hw_rule_t *rule)
{
	int changed = 0;
	unsigned int k;
	mask_t *mask = hw_param_mask(params, rule->var);
	interval_t *i = hw_param_interval(params, rule->deps[0]);
	for (k = 0; k <= SND_PCM_FORMAT_LAST; ++k) {
		int bits;
		if (!mask_test(mask, k))
			continue;
		bits = snd_pcm_format_physical_width(k);
		if (bits < 0)
			continue;
		if (!interval_test(i, bits)) {
			mask_reset(mask, k);
			if (mask_empty(mask))
				return -EINVAL;
			changed = 1;
		}
	}
	return changed;
}


int snd_pcm_hw_rule_sample_bits(snd_pcm_hw_params_t *params,
				snd_pcm_hw_rule_t *rule)
{
	unsigned int min, max;
	unsigned int k;
	interval_t *i = hw_param_interval(params, rule->var);
	mask_t *mask = hw_param_mask(params, rule->deps[0]);
	int c, changed = 0;
	min = UINT_MAX;
	max = 0;
	for (k = 0; k <= SND_PCM_FORMAT_LAST; ++k) {
		int bits;
		if (!mask_test(mask, k))
			continue;
		bits = snd_pcm_format_physical_width(k);
		if (bits < 0)
			continue;
		if (min > (unsigned)bits)
			min = bits;
		if (max < (unsigned)bits)
			max = bits;
	}
	c = interval_refine_min(i, min);
	if (c < 0)
		return c;
	if (c)
		changed = 1;
	c = interval_refine_max(i, max);
	if (c < 0)
		return c;
	if (c)
		changed = 1;
	return changed;
}

static snd_pcm_hw_rule_t refine_rules[] = {
	{
		var: SND_PCM_HW_PARAM_FORMAT,
		func: snd_pcm_hw_rule_format,
		deps: { SND_PCM_HW_PARAM_SAMPLE_BITS, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_SAMPLE_BITS, 
		func: snd_pcm_hw_rule_sample_bits,
		deps: { SND_PCM_HW_PARAM_FORMAT, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_SAMPLE_BITS, 
		func: snd_pcm_hw_rule_div,
		deps: { SND_PCM_HW_PARAM_FRAME_BITS,
			SND_PCM_HW_PARAM_CHANNELS, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_FRAME_BITS, 
		func: snd_pcm_hw_rule_mul,
		deps: { SND_PCM_HW_PARAM_SAMPLE_BITS,
			SND_PCM_HW_PARAM_CHANNELS, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_FRAME_BITS, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_FRAGMENT_BYTES,
			SND_PCM_HW_PARAM_FRAGMENT_SIZE, -1 },
		private: (void*) 8,
	},
	{
		var: SND_PCM_HW_PARAM_FRAME_BITS, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_BUFFER_BYTES,
			SND_PCM_HW_PARAM_BUFFER_SIZE, -1 },
		private: (void*) 8,
	},
	{
		var: SND_PCM_HW_PARAM_CHANNELS, 
		func: snd_pcm_hw_rule_div,
		deps: { SND_PCM_HW_PARAM_FRAME_BITS,
			SND_PCM_HW_PARAM_SAMPLE_BITS, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_RATE, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_FRAGMENT_SIZE,
			SND_PCM_HW_PARAM_FRAGMENT_LENGTH, -1 },
		private: (void*) 1000000,
	},
	{
		var: SND_PCM_HW_PARAM_RATE, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_BUFFER_SIZE,
			SND_PCM_HW_PARAM_BUFFER_LENGTH, -1 },
		private: (void*) 1000000,
	},
	{
		var: SND_PCM_HW_PARAM_FRAGMENTS, 
		func: snd_pcm_hw_rule_div,
		deps: { SND_PCM_HW_PARAM_BUFFER_SIZE,
			SND_PCM_HW_PARAM_FRAGMENT_SIZE, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_FRAGMENT_SIZE, 
		func: snd_pcm_hw_rule_div,
		deps: { SND_PCM_HW_PARAM_BUFFER_SIZE,
			SND_PCM_HW_PARAM_FRAGMENTS, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_FRAGMENT_SIZE, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_FRAGMENT_BYTES,
			SND_PCM_HW_PARAM_FRAME_BITS, -1 },
		private: (void*) 8,
	},
	{
		var: SND_PCM_HW_PARAM_FRAGMENT_SIZE, 
		func: snd_pcm_hw_rule_muldivk,
		deps: { SND_PCM_HW_PARAM_FRAGMENT_LENGTH,
			SND_PCM_HW_PARAM_RATE, -1 },
		private: (void*) 1000000,
	},
	{
		var: SND_PCM_HW_PARAM_BUFFER_SIZE, 
		func: snd_pcm_hw_rule_mul,
		deps: { SND_PCM_HW_PARAM_FRAGMENT_SIZE,
			SND_PCM_HW_PARAM_FRAGMENTS, -1 },
		private: 0,
	},
	{
		var: SND_PCM_HW_PARAM_BUFFER_SIZE, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_BUFFER_BYTES,
			SND_PCM_HW_PARAM_FRAME_BITS, -1 },
		private: (void*) 8,
	},
	{
		var: SND_PCM_HW_PARAM_BUFFER_SIZE, 
		func: snd_pcm_hw_rule_muldivk,
		deps: { SND_PCM_HW_PARAM_BUFFER_LENGTH,
			SND_PCM_HW_PARAM_RATE, -1 },
		private: (void*) 1000000,
	},
	{
		var: SND_PCM_HW_PARAM_FRAGMENT_BYTES, 
		func: snd_pcm_hw_rule_muldivk,
		deps: { SND_PCM_HW_PARAM_FRAGMENT_SIZE,
			SND_PCM_HW_PARAM_FRAME_BITS, -1 },
		private: (void*) 8,
	},
	{
		var: SND_PCM_HW_PARAM_BUFFER_BYTES, 
		func: snd_pcm_hw_rule_muldivk,
		deps: { SND_PCM_HW_PARAM_BUFFER_SIZE,
			SND_PCM_HW_PARAM_FRAME_BITS, -1 },
		private: (void*) 8,
	},
	{
		var: SND_PCM_HW_PARAM_FRAGMENT_LENGTH, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_FRAGMENT_SIZE,
			SND_PCM_HW_PARAM_RATE, -1 },
		private: (void*) 1000000,
	},
	{
		var: SND_PCM_HW_PARAM_BUFFER_LENGTH, 
		func: snd_pcm_hw_rule_mulkdiv,
		deps: { SND_PCM_HW_PARAM_BUFFER_SIZE,
			SND_PCM_HW_PARAM_RATE, -1 },
		private: (void*) 1000000,
	},
};

#define RULES (sizeof(refine_rules) / sizeof(refine_rules[0]))

static mask_t refine_masks[SND_PCM_HW_PARAM_LAST_MASK - SND_PCM_HW_PARAM_FIRST_MASK + 1] = {
	[SND_PCM_HW_PARAM_ACCESS - SND_PCM_HW_PARAM_FIRST_MASK] = {
		bits: (1 << (SND_PCM_ACCESS_LAST + 1)) - 1,
	},
	[SND_PCM_HW_PARAM_FORMAT - SND_PCM_HW_PARAM_FIRST_MASK] = {
		bits: 0x81ffffff,
	},
	[SND_PCM_HW_PARAM_SUBFORMAT - SND_PCM_HW_PARAM_FIRST_MASK] = {
		bits: (1 << (SND_PCM_SUBFORMAT_LAST + 1)) - 1,
	},
};

static interval_t refine_intervals[SND_PCM_HW_PARAM_LAST_INTERVAL - SND_PCM_HW_PARAM_FIRST_INTERVAL + 1] = {
	[SND_PCM_HW_PARAM_CHANNELS - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_RATE - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_FRAGMENT_LENGTH - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_FRAGMENT_SIZE - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_FRAGMENTS - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_BUFFER_LENGTH - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_BUFFER_SIZE - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_SAMPLE_BITS - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_FRAME_BITS - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_FRAGMENT_BYTES - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
	[SND_PCM_HW_PARAM_BUFFER_BYTES - SND_PCM_HW_PARAM_FIRST_INTERVAL] = {
		min: 1, max: UINT_MAX,
		openmin: 0, openmax: 0, real: 0, empty: 0,
	},
};



int _snd_pcm_hw_refine(snd_pcm_hw_params_t *params)
{
	unsigned int k;
	interval_t *i;
	unsigned int rstamps[RULES];
	unsigned int vstamps[SND_PCM_HW_PARAM_LAST + 1];
	unsigned int stamp = 2;
	int err, changed;

	for (k = SND_PCM_HW_PARAM_FIRST_MASK; k <= SND_PCM_HW_PARAM_LAST_MASK; k++) {
		if (!params->appl_cmask & (1 << k))
			continue;
		err = mask_refine(hw_param_mask(params, k),
				  &refine_masks[k - SND_PCM_HW_PARAM_FIRST_MASK]);
		if (err < 0)
			return err;
	}

	for (k = SND_PCM_HW_PARAM_FIRST_INTERVAL; k <= SND_PCM_HW_PARAM_LAST_INTERVAL; k++) {
		if (!params->appl_cmask & (1 << k))
			continue;
		err = interval_refine(hw_param_interval(params, k),
				      &refine_intervals[k - SND_PCM_HW_PARAM_FIRST_INTERVAL]);
		if (err < 0)
			return err;
	}

	for (k = 0; k < RULES; k++)
		rstamps[k] = 0;
	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; k++)
		vstamps[k] = (params->appl_cmask & (1 << k)) ? 1 : 0;
	params->appl_cmask = 0;
	params->hw_cmask = 0;
	changed = 1;
	while (changed) {
		changed = 0;
		for (k = 0; k < RULES; k++) {
			snd_pcm_hw_rule_t *r = &refine_rules[k];
			unsigned int d;
			int doit = 0;
#ifdef RULES_DEBUG
			interval_t *i;
#endif
			for (d = 0; r->deps[d] >= 0; d++) {
				if (vstamps[r->deps[d]] > rstamps[k]) {
					doit = 1;
					break;
				}
			}
			if (!doit)
				continue;
#ifdef RULES_DEBUG
			i = hw_param_interval(params, r->var);
			fprintf(stderr, "Rule %d: %u ", k, r->var);
			interval_print(i, stderr);
#endif
			err = r->func(params, r);
#ifdef RULES_DEBUG
			interval_print(i, stderr);
			putc('\n', stderr);
#endif
			rstamps[k] = stamp;
			if (err && r->var >= 0) {
				params->hw_cmask |= 1 << r->var;
				vstamps[r->var] = stamp;
				changed = 1;
			}
			if (err < 0)
				return err;
			stamp++;
		}
	}
	if (!params->msbits) {
		i = hw_param_interval(params, SND_PCM_HW_PARAM_SAMPLE_BITS);
		if (interval_single(i))
			params->msbits = interval_value(i);
	}

	if (!params->rate_den) {
		i = hw_param_interval(params, SND_PCM_HW_PARAM_RATE);
		if (interval_single(i)) {
			params->rate_num = interval_value(i);
			params->rate_den = 1;
		}
	}
	return 0;
}

int snd_pcm_hw_refine2(snd_pcm_hw_params_t *params,
		       snd_pcm_hw_params_t *sparams,
		       int (*func)(snd_pcm_t *slave,
				   snd_pcm_hw_params_t *sparams),
		       snd_pcm_t *slave, 
		       unsigned int links)
{
	unsigned int k;
	int err, error = 0;
	unsigned int client_from_slave = ~0U;
	unsigned int client_refine = ~0U;
	unsigned int slave_from_client = ~0U;
	unsigned int slave_refine = ~0U;
	unsigned int hw_cmask = params->hw_cmask;

	while (client_from_slave || client_refine) {
		for (k = 0; k <= SND_PCM_HW_PARAM_LAST; ++k) {
			int changed;
			if (!(links & (1 << k)))
				continue;
			if (!(client_from_slave & (1 << k)))
				continue;
			changed = _snd_pcm_hw_param_refine(params, 1, k, sparams);
			if (changed) {
				hw_cmask |= 1 << k;
				slave_from_client |= 1 << k;
				client_refine |= 1 << k;
			}
			if (changed < 0)
				error = changed;
		}
		client_from_slave = 0;
		if (error)
			break;
		if (client_refine) {
			params->appl_cmask = client_refine;
			client_refine = 0;
			err = _snd_pcm_hw_refine(params);
			hw_cmask |= params->hw_cmask;
			slave_from_client |= params->hw_cmask;
			if (err < 0) {
				error = err;
				break;
			}
		}
	
		for (k = 0; k <= SND_PCM_HW_PARAM_LAST; ++k) {
			int changed;
			if (!(links & (1 << k)))
				continue;
			if (!(slave_from_client & (1 << k)))
				continue;
			changed = _snd_pcm_hw_param_refine(sparams, 1, k, params);
			if (changed) {
				slave_refine |= 1 << k;
				client_from_slave |= 1 << k;
			}
			if (changed < 0)
				error = changed;
		}
		slave_from_client = 0;
		if (error)
			continue;
		if (slave_refine) {
			sparams->appl_cmask = slave_refine;
			slave_refine = 0;
			error = func(slave, sparams);
			client_from_slave |= sparams->hw_cmask;
		}
	}
	params->appl_cmask = 0;
	params->hw_cmask = hw_cmask;
	return error;
}

int snd_pcm_hw_params2(snd_pcm_hw_params_t *params,
		       snd_pcm_hw_params_t *sparams,
		       int (*func)(snd_pcm_t *slave,
				   snd_pcm_hw_params_t *sparams),
		       snd_pcm_t *slave, 
		       unsigned int links)
{
	unsigned int k;
	int err;
	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; ++k) {
		int changed;
		if (!(links & (1 << k)))
			continue;
		changed = _snd_pcm_hw_param_refine(sparams, 0, k, params);
		if (changed < 0)
			return changed;
	}
	sparams->appl_cmask = ~0U;
	err = func(slave, sparams);
	if (err >= 0)
		return err;

	for (k = 0; k <= SND_PCM_HW_PARAM_LAST; ++k) {
		int changed;
		if (!(links & (1 << k)))
			continue;
		if (!(sparams->hw_cmask & (1 << k)))
			continue;
		changed = _snd_pcm_hw_param_refine(params, 1, k, sparams);
	}
	return err;
}

int snd_pcm_sw_params_current(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	assert(pcm && params);
	assert(pcm->setup);
	params->start_mode = pcm->start_mode;
	params->ready_mode = pcm->ready_mode;
	params->xrun_mode = pcm->xrun_mode;
	params->silence_mode = pcm->silence_mode;
	params->tstamp_mode = pcm->tstamp_mode;
	params->avail_min = pcm->avail_min;
	params->xfer_align = pcm->xfer_align;
	params->silence_threshold = pcm->silence_threshold;
	params->silence_size = pcm->silence_size;
	params->boundary = pcm->boundary;
	return 0;
}

int snd_pcm_sw_params_default(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	assert(pcm && params);
	assert(pcm->setup);
	params->start_mode = SND_PCM_START_DATA;
	params->ready_mode = SND_PCM_READY_FRAGMENT;
	params->xrun_mode = SND_PCM_XRUN_FRAGMENT;
	params->silence_mode = SND_PCM_SILENCE_FRAGMENT;
	params->tstamp_mode = SND_PCM_TSTAMP_NONE;
	params->avail_min = pcm->fragment_size;
	params->xfer_align = pcm->fragment_size;
	params->silence_threshold = 0;
	params->silence_size = 0;
	params->boundary = LONG_MAX - pcm->buffer_size * 2 - LONG_MAX % pcm->buffer_size;
	return 0;
}

int snd_pcm_sw_param_value(snd_pcm_sw_params_t *params, unsigned int var)
{
	switch (var) {
	case SND_PCM_SW_PARAM_START_MODE:
		return params->start_mode;
	case SND_PCM_SW_PARAM_READY_MODE:
		return params->ready_mode;
	case SND_PCM_SW_PARAM_XRUN_MODE:
		return params->xrun_mode;
	case SND_PCM_SW_PARAM_SILENCE_MODE:
		return params->silence_mode;
	case SND_PCM_SW_PARAM_TSTAMP_MODE:
		return params->tstamp_mode;
	case SND_PCM_SW_PARAM_AVAIL_MIN:
		return params->avail_min;
	case SND_PCM_SW_PARAM_XFER_ALIGN:
		return params->xfer_align;
	case SND_PCM_SW_PARAM_SILENCE_THRESHOLD:
		return params->silence_threshold;
	case SND_PCM_SW_PARAM_SILENCE_SIZE:
		return params->silence_size;
	default:
		assert(0);
		return -EINVAL;
	}
}

int snd_pcm_sw_param_set(snd_pcm_t *pcm, snd_pcm_sw_params_t *params,
			 unsigned int var, unsigned int val)
{
	switch (var) {
	case SND_PCM_SW_PARAM_START_MODE:
		assert(val <= SND_PCM_START_LAST);
		params->start_mode = val;
		break;
	case SND_PCM_SW_PARAM_READY_MODE:
		assert(val <= SND_PCM_READY_LAST);
		params->ready_mode = val;
		break;
	case SND_PCM_SW_PARAM_XRUN_MODE:
		assert(val <= SND_PCM_XRUN_LAST);
		params->xrun_mode = val;
		break;
	case SND_PCM_SW_PARAM_SILENCE_MODE:
		assert(val <= SND_PCM_SILENCE_LAST);
		params->silence_mode = val;
		break;
	case SND_PCM_SW_PARAM_TSTAMP_MODE:
		assert(val <= SND_PCM_TSTAMP_LAST);
		params->tstamp_mode = val;
		break;
	case SND_PCM_SW_PARAM_AVAIL_MIN:
		assert(val > 0);
		params->avail_min = val;
		break;
	case SND_PCM_SW_PARAM_XFER_ALIGN:
		assert(val > 0 && val % pcm->min_align == 0);
		params->xfer_align = val;
		break;
	case SND_PCM_SW_PARAM_SILENCE_THRESHOLD:
		assert(val + params->silence_size <= pcm->buffer_size);
		params->silence_threshold = val;
		break;
	case SND_PCM_SW_PARAM_SILENCE_SIZE:
		assert(val + params->silence_threshold <= pcm->buffer_size);
		params->silence_size = val;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	return val;
}

int snd_pcm_sw_param_near(snd_pcm_t *pcm, snd_pcm_sw_params_t *params,
			  unsigned int var, unsigned int val)
{
	switch (var) {
	case SND_PCM_SW_PARAM_START_MODE:
		assert(val <= SND_PCM_START_LAST);
		params->start_mode = val;
		break;
	case SND_PCM_SW_PARAM_READY_MODE:
		assert(val <= SND_PCM_READY_LAST);
		params->ready_mode = val;
		break;
	case SND_PCM_SW_PARAM_XRUN_MODE:
		assert(val <= SND_PCM_XRUN_LAST);
		params->xrun_mode = val;
		break;
	case SND_PCM_SW_PARAM_SILENCE_MODE:
		assert(val <= SND_PCM_SILENCE_LAST);
		params->silence_mode = val;
		break;
	case SND_PCM_SW_PARAM_TSTAMP_MODE:
		assert(val <= SND_PCM_TSTAMP_LAST);
		params->tstamp_mode = val;
		break;
	case SND_PCM_SW_PARAM_AVAIL_MIN:
		if (val == 0)
			val = pcm->min_align;
		params->avail_min = val;
		break;
	case SND_PCM_SW_PARAM_XFER_ALIGN:
	{
		size_t r = val % pcm->min_align;
		if (r >= (pcm->min_align + 1) / 2)
			val += pcm->min_align - r;
		else
			val -= r;
		if (val == 0)
			val = pcm->min_align;
		params->xfer_align = val;
		break;
	}
	case SND_PCM_SW_PARAM_SILENCE_THRESHOLD:
		if (val > pcm->buffer_size - params->silence_size)
			val = pcm->buffer_size - params->silence_size;
		params->silence_threshold = val;
		break;
	case SND_PCM_SW_PARAM_SILENCE_SIZE:
		if (val > pcm->buffer_size - params->silence_threshold)
			val = pcm->buffer_size - params->silence_threshold;
		params->silence_size = val;
		break;
	default:
		assert(0);
		return -EINVAL;
	}
	return val;
}

void snd_pcm_sw_param_dump(const snd_pcm_sw_params_t *params,
			   unsigned int var, FILE *fp)
{
	switch (var) {
	case SND_PCM_SW_PARAM_START_MODE:
		fputs(snd_pcm_start_mode_name(params->start_mode), fp);
		break;
	case SND_PCM_SW_PARAM_READY_MODE:
		fputs(snd_pcm_ready_mode_name(params->ready_mode), fp);
		break;
	case SND_PCM_SW_PARAM_XRUN_MODE:
		fputs(snd_pcm_xrun_mode_name(params->xrun_mode), fp);
		break;
	case SND_PCM_SW_PARAM_SILENCE_MODE:
		fputs(snd_pcm_silence_mode_name(params->silence_mode), fp);
		break;
	case SND_PCM_SW_PARAM_TSTAMP_MODE:
		fputs(snd_pcm_tstamp_mode_name(params->tstamp_mode), fp);
		break;
	case SND_PCM_SW_PARAM_AVAIL_MIN:
		fprintf(fp, "%ld", (long) params->avail_min);
		break;
	case SND_PCM_SW_PARAM_XFER_ALIGN:
		fprintf(fp, "%ld", (long) params->xfer_align);
		break;
	case SND_PCM_SW_PARAM_SILENCE_THRESHOLD:
		fprintf(fp, "%ld", (long) params->silence_threshold);
		break;
	case SND_PCM_SW_PARAM_SILENCE_SIZE:
		fprintf(fp, "%ld", (long) params->silence_size);
		break;
	default:
		assert(0);
	}
}

int snd_pcm_sw_params_dump(snd_pcm_sw_params_t *params, FILE *fp)
{
	unsigned int k;
	for (k = 0; k <= SND_PCM_SW_PARAM_LAST; k++) {
		fprintf(fp, "%s: ", snd_pcm_sw_param_name(k));
		snd_pcm_sw_param_dump(params, k, fp);
		putc('\n', fp);
	}
	return 0;
}


int snd_pcm_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int err;
	assert(pcm && params);
	params->hw_cmask = 0;
	err = pcm->ops->hw_refine(pcm->op_arg, params);
	params->appl_cmask = 0;
	return err;
}

/* Install one of the configurations present in configuration
   space defined by PARAMS.
   The configuration choosen is that obtained fixing in this order:
   first access
   first format
   first subformat
   min channels
   min rate
   min fragment_size
   max fragments
   Return 0 on success or a negative number expressing the error.
*/
int snd_pcm_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int err;
	snd_pcm_sw_params_t sw;
	int fb, min_align;
	err = snd_pcm_hw_refine(pcm, params);
	if (err < 0)
		return err;
	snd_pcm_hw_params_choose(pcm, params);
	if (pcm->mmap_channels) {
		err = snd_pcm_munmap(pcm);
		if (err < 0)
			return err;
	}
	err = pcm->ops->hw_params(pcm->op_arg, params);
	if (err < 0)
		goto _mmap;

	pcm->setup = 1;
	pcm->access = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_ACCESS);
	pcm->format = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FORMAT);
	pcm->subformat = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_SUBFORMAT);
	pcm->channels = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_CHANNELS);
	pcm->rate = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_RATE);
	pcm->fragment_size = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FRAGMENT_SIZE);
	pcm->fragments = snd_pcm_hw_param_value(params, SND_PCM_HW_PARAM_FRAGMENTS);
	pcm->bits_per_sample = snd_pcm_format_physical_width(pcm->format);
	pcm->bits_per_frame = pcm->bits_per_sample * pcm->channels;
	fb = pcm->bits_per_frame;
	min_align = 1;
	while (fb % 8) {
		fb *= 2;
		min_align *= 2;
	}
	pcm->min_align = min_align;
	pcm->buffer_size = pcm->fragment_size * pcm->fragments;
	
	pcm->info = params->info;
	pcm->msbits = params->msbits;
	pcm->rate_num = params->rate_num;
	pcm->rate_den = params->rate_den;
	pcm->fifo_size = params->fifo_size;
	
	/* Default sw params */
	snd_pcm_sw_params_default(pcm, &sw);
	err = snd_pcm_sw_params(pcm, &sw);
	assert(err >= 0);

 _mmap:
	if (pcm->setup &&
	    (pcm->mmap_rw || 
	     (pcm->access == SND_PCM_ACCESS_MMAP_INTERLEAVED ||
	      pcm->access == SND_PCM_ACCESS_MMAP_NONINTERLEAVED ||
	      pcm->access == SND_PCM_ACCESS_MMAP_COMPLEX))) {
		int err;
		err = snd_pcm_mmap(pcm);
		if (err < 0)
			return err;
	}
	if (err >= 0)
		err = snd_pcm_prepare(pcm);
	return err;
}

int snd_pcm_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t *params)
{
	int err;
	err = pcm->ops->sw_params(pcm->op_arg, params);
	if (err < 0)
		return err;
	pcm->start_mode = params->start_mode;
	pcm->ready_mode = params->ready_mode;
	pcm->xrun_mode = params->xrun_mode;
	pcm->silence_mode = params->silence_mode;
	pcm->tstamp_mode = params->tstamp_mode;
	pcm->avail_min = params->avail_min;
	pcm->xfer_align = params->xfer_align;
	pcm->silence_threshold = params->silence_threshold;
	pcm->silence_size = params->silence_size;
	pcm->boundary = params->boundary;
	return 0;
}

