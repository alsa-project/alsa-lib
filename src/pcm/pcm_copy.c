/*
 *  PCM - Copy conversion
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
  
#include <byteswap.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
} snd_pcm_copy_t;

static int snd_pcm_copy_hw_refine_cprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params)
{
	int err;
	mask_t *access_mask = alloca(mask_sizeof());
	mask_load(access_mask, SND_PCM_ACCBIT_PLUGIN);
	err = _snd_pcm_hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS,
				     access_mask);
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_copy_hw_refine_sprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *sparams)
{
	mask_t *saccess_mask = alloca(mask_sizeof());
	mask_load(saccess_mask, SND_PCM_ACCBIT_MMAP);
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
			       saccess_mask);
	return 0;
}

static int snd_pcm_copy_hw_refine_schange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					  snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = ~SND_PCM_HW_PARBIT_ACCESS;
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_copy_hw_refine_cchange(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params,
					  snd_pcm_hw_params_t *sparams)
{
	int err;
	unsigned int links = ~SND_PCM_HW_PARBIT_ACCESS;
	err = _snd_pcm_hw_params_refine(params, links, sparams);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_copy_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_copy_hw_refine_cprepare,
				       snd_pcm_copy_hw_refine_cchange,
				       snd_pcm_copy_hw_refine_sprepare,
				       snd_pcm_copy_hw_refine_schange,
				       snd_pcm_plugin_hw_refine_slave);
}

static int snd_pcm_copy_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_params_slave(pcm, params,
				       snd_pcm_copy_hw_refine_cchange,
				       snd_pcm_copy_hw_refine_sprepare,
				       snd_pcm_copy_hw_refine_schange,
				       snd_pcm_plugin_hw_params_slave);
}

static snd_pcm_sframes_t snd_pcm_copy_write_areas(snd_pcm_t *pcm,
					const snd_pcm_channel_area_t *areas,
					snd_pcm_uframes_t offset,
					snd_pcm_uframes_t size,
					snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_copy_t *copy = pcm->private;
	snd_pcm_t *slave = copy->plug.slave;
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		snd_pcm_uframes_t frames = snd_pcm_mmap_playback_xfer(slave, size - xfer);
		
		snd_pcm_areas_copy(areas, offset, 
				   snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
				   pcm->channels, frames, pcm->format);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((snd_pcm_uframes_t)err == frames);
		offset += err;
		xfer += err;
		snd_pcm_mmap_hw_forward(pcm, err);
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

static snd_pcm_sframes_t snd_pcm_copy_read_areas(snd_pcm_t *pcm,
				       const snd_pcm_channel_area_t *areas,
				       snd_pcm_uframes_t offset,
				       snd_pcm_uframes_t size,
				       snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_copy_t *copy = pcm->private;
	snd_pcm_t *slave = copy->plug.slave;
	snd_pcm_uframes_t xfer = 0;
	snd_pcm_sframes_t err = 0;
	if (slave_sizep && *slave_sizep < size)
		size = *slave_sizep;
	assert(size > 0);
	while (xfer < size) {
		snd_pcm_uframes_t frames = snd_pcm_mmap_capture_xfer(slave, size - xfer);
		snd_pcm_areas_copy(snd_pcm_mmap_areas(slave), snd_pcm_mmap_offset(slave),
				   areas, offset, 
				   pcm->channels, frames, pcm->format);
		err = snd_pcm_mmap_forward(slave, frames);
		if (err < 0)
			break;
		assert((snd_pcm_uframes_t)err == frames);
		offset += err;
		xfer += err;
		snd_pcm_mmap_hw_forward(pcm, err);
	}
	if (xfer > 0) {
		if (slave_sizep)
			*slave_sizep = xfer;
		return xfer;
	}
	return err;
}

static void snd_pcm_copy_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_copy_t *copy = pcm->private;
	snd_output_printf(out, "Copy conversion PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(copy->plug.slave, out);
}

snd_pcm_ops_t snd_pcm_copy_ops = {
	close: snd_pcm_plugin_close,
	card: snd_pcm_plugin_card,
	info: snd_pcm_plugin_info,
	hw_refine: snd_pcm_copy_hw_refine,
	hw_params: snd_pcm_copy_hw_params,
	sw_params: snd_pcm_plugin_sw_params,
	channel_info: snd_pcm_plugin_channel_info,
	dump: snd_pcm_copy_dump,
	nonblock: snd_pcm_plugin_nonblock,
	async: snd_pcm_plugin_async,
	mmap: snd_pcm_plugin_mmap,
	munmap: snd_pcm_plugin_munmap,
};

int snd_pcm_copy_open(snd_pcm_t **pcmp, char *name, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_copy_t *copy;
	assert(pcmp && slave);
	copy = calloc(1, sizeof(snd_pcm_copy_t));
	if (!copy) {
		return -ENOMEM;
	}
	copy->plug.read = snd_pcm_copy_read_areas;
	copy->plug.write = snd_pcm_copy_write_areas;
	copy->plug.slave = slave;
	copy->plug.close_slave = close_slave;

	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(copy);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_COPY;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_copy_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private = copy;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = &copy->plug.hw_ptr;
	pcm->appl_ptr = &copy->plug.appl_ptr;
	*pcmp = pcm;

	return 0;
}

int _snd_pcm_copy_open(snd_pcm_t **pcmp, char *name,
			 snd_config_t *conf, 
			 int stream, int mode)
{
	snd_config_iterator_t i;
	char *sname = NULL;
	int err;
	snd_pcm_t *spcm;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "sname") == 0) {
			err = snd_config_string_get(n, &sname);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		ERR("Unknown field %s", n->id);
		return -EINVAL;
	}
	if (!sname) {
		ERR("sname is not defined");
		return -EINVAL;
	}
	/* This is needed cause snd_config_update may destroy config */
	sname = strdup(sname);
	if (!sname)
		return  -ENOMEM;
	err = snd_pcm_open(&spcm, sname, stream, mode);
	free(sname);
	if (err < 0)
		return err;
	err = snd_pcm_copy_open(pcmp, name, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}

