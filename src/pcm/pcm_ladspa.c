/*
 *  PCM - LADSPA integration plugin
 *  Copyright (c) 2001 by Jaroslav Kysela <perex@suse.cz>
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
  
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

#include "ladspa.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_ladspa = "";
#endif

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
} snd_pcm_ladspa_t;

static int snd_pcm_ladspa_hw_refine_cprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	int err;
	snd_pcm_access_mask_t access_mask = { SND_PCM_ACCBIT_SHM };
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_ACCESS,
					 &access_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_set_format(params, SND_PCM_FORMAT_FLOAT);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_set_subformat(params, SND_PCM_SUBFORMAT_STD);
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_ladspa_hw_refine_sprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *sparams)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	snd_pcm_access_mask_t saccess_mask = { SND_PCM_ACCBIT_MMAP };
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_set_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				   &saccess_mask);
	_snd_pcm_hw_params_set_format(sparams, SND_PCM_FORMAT_FLOAT);
	_snd_pcm_hw_params_set_subformat(sparams, SND_PCM_SUBFORMAT_STD);
	return 0;
}

static int snd_pcm_ladspa_hw_refine_schange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					    snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_ladspa_hw_refine_cchange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					    snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	err = _snd_pcm_hw_params_refine(params, links, sparams);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_ladspa_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_ladspa_hw_refine_cprepare,
				       snd_pcm_ladspa_hw_refine_cchange,
				       snd_pcm_ladspa_hw_refine_sprepare,
				       snd_pcm_ladspa_hw_refine_schange,
				       snd_pcm_plugin_hw_refine_slave);
}

static int snd_pcm_ladspa_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_ladspa_hw_refine_cchange,
					  snd_pcm_ladspa_hw_refine_sprepare,
					  snd_pcm_ladspa_hw_refine_schange,
					  snd_pcm_plugin_hw_params_slave);
	if (err < 0)
		return err;

	return 0;
}

static snd_pcm_uframes_t
snd_pcm_ladspa_write_areas(snd_pcm_t *pcm,
			   const snd_pcm_channel_area_t *areas,
			   snd_pcm_uframes_t offset,
			   snd_pcm_uframes_t size,
			   const snd_pcm_channel_area_t *slave_areas,
			   snd_pcm_uframes_t slave_offset,
			   snd_pcm_uframes_t *slave_sizep)
{
	// snd_pcm_ladspa_t *ladspa = pcm->private_data;
	if (size > *slave_sizep)
		size = *slave_sizep;
	*slave_sizep = size;
	return size;
}

static snd_pcm_uframes_t
snd_pcm_ladspa_read_areas(snd_pcm_t *pcm,
			  const snd_pcm_channel_area_t *areas,
			  snd_pcm_uframes_t offset,
			  snd_pcm_uframes_t size,
			  const snd_pcm_channel_area_t *slave_areas,
			  snd_pcm_uframes_t slave_offset,
			  snd_pcm_uframes_t *slave_sizep)
{
	// snd_pcm_ladspa_t *ladspa = pcm->private_data;
	if (size > *slave_sizep)
		size = *slave_sizep;
	*slave_sizep = size;
	return size;
}

static void snd_pcm_ladspa_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	snd_output_printf(out, "LADSPA PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(ladspa->plug.slave, out);
}

snd_pcm_ops_t snd_pcm_ladspa_ops = {
	close: snd_pcm_plugin_close,
	info: snd_pcm_plugin_info,
	hw_refine: snd_pcm_ladspa_hw_refine,
	hw_params: snd_pcm_ladspa_hw_params,
	hw_free: snd_pcm_plugin_hw_free,
	sw_params: snd_pcm_plugin_sw_params,
	channel_info: snd_pcm_plugin_channel_info,
	dump: snd_pcm_ladspa_dump,
	nonblock: snd_pcm_plugin_nonblock,
	async: snd_pcm_plugin_async,
	mmap: snd_pcm_plugin_mmap,
	munmap: snd_pcm_plugin_munmap,
};

int snd_pcm_ladspa_open(snd_pcm_t **pcmp, const char *name, snd_pcm_format_t sformat, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_ladspa_t *ladspa;
	int err;
	assert(pcmp && slave);
	if (sformat != SND_PCM_FORMAT_FLOAT)
		return -EINVAL;
	ladspa = calloc(1, sizeof(snd_pcm_ladspa_t));
	if (!ladspa) {
		return -ENOMEM;
	}
	ladspa->plug.read = snd_pcm_ladspa_read_areas;
	ladspa->plug.write = snd_pcm_ladspa_write_areas;
	ladspa->plug.slave = slave;
	ladspa->plug.close_slave = close_slave;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_LADSPA, name, slave->stream, slave->mode);
	if (err < 0) {
		free(ladspa);
		return err;
	}
	pcm->ops = &snd_pcm_ladspa_ops;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->private_data = ladspa;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = &ladspa->plug.hw_ptr;
	pcm->appl_ptr = &ladspa->plug.appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_ladspa_open(snd_pcm_t **pcmp, const char *name,
			 snd_config_t *root, snd_config_t *conf, 
			 snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *slave = NULL, *sconf;
	snd_pcm_format_t sformat;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "slave") == 0) {
			slave = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	err = snd_pcm_slave_conf(root, slave, &sconf, 1,
				 SND_PCM_HW_PARAM_FORMAT, SCONF_MANDATORY, &sformat);
	if (err < 0)
		return err;
	if (sformat != SND_PCM_FORMAT_FLOAT) {
	    	snd_config_delete(sconf);
		SNDERR("invalid slave format, should be float");
		return -EINVAL;
	}
	err = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
	snd_config_delete(sconf);
	if (err < 0)
		return err;
	err = snd_pcm_ladspa_open(pcmp, name, sformat, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
SND_DLSYM_BUILD_VERSION(_snd_pcm_ladspa_open, SND_PCM_DLSYM_VERSION);
