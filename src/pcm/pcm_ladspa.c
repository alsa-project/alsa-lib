/**
 * \file pcm/pcm_ladspa.c
 * \ingroup PCM_Plugins
 * \brief ALSA Plugin <-> LADSPA Plugin Interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2001
 */
/*
 *  PCM - LADSPA integration plugin
 *  Copyright (c) 2001 by Jaroslav Kysela <perex@suse.cz>
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
  
#include <dirent.h>
#include <dlfcn.h>
#include <wordexp.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

#include "ladspa.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_ladspa = "";
#endif

#ifndef DOC_HIDDEN

#define NO_ASSIGN	0xffffffff

typedef enum _snd_pcm_ladspa_policy {
	SND_PCM_LADSPA_POLICY_NONE,		/* use bindings only */
	SND_PCM_LADSPA_POLICY_DUPLICATE		/* duplicate bindings for all channels */
} snd_pcm_ladspa_policy_t;

typedef struct snd_pcm_ladspa_instance snd_pcm_ladspa_instance_t;

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	struct list_head pplugins;
	struct list_head cplugins;
	unsigned int instances_channels;
	snd_pcm_ladspa_instance_t **finstances;
} snd_pcm_ladspa_t;

typedef struct {
	struct list_head list;
	LADSPA_Handle *handle;
} snd_pcm_ladspa_subinstance_t;

struct snd_pcm_ladspa_instance {
	struct list_head list;
	const LADSPA_Descriptor *desc;
	LADSPA_Handle *handle;
	LADSPA_Data *m_data;
	LADSPA_Data *in_data;
	LADSPA_Data *out_data;
	unsigned int depth;
	unsigned int channel;
	unsigned int in_port;
	unsigned int out_port;
	snd_pcm_ladspa_instance_t *prev;
	snd_pcm_ladspa_instance_t *next;
};

typedef struct {
	LADSPA_PortDescriptor pdesc;		/* port description */
	unsigned int port_bindings_size;	/* size of array */
	unsigned int *port_bindings;		/* index = channel number, value = LADSPA port */
	unsigned int controls_size;		/* size of array */
	LADSPA_Data *controls;			/* index = LADSPA control port index */
} snd_pcm_ladspa_plugin_io_t;

typedef struct {
	struct list_head list;
	snd_pcm_ladspa_policy_t policy;
	char *filename;
	void *dl_handle;
	const LADSPA_Descriptor *desc;
	snd_pcm_ladspa_plugin_io_t input;
	snd_pcm_ladspa_plugin_io_t output;
	struct list_head instances;
} snd_pcm_ladspa_plugin_t;

#endif /* DOC_HIDDEN */

static int snd_pcm_ladspa_find_port(unsigned int *res,
				    snd_pcm_ladspa_plugin_t *lplug,
				    LADSPA_PortDescriptor pdesc,
				    unsigned int port_idx)
{
	unsigned long idx;

	for (idx = 0; idx < lplug->desc->PortCount; idx++)
		if ((lplug->desc->PortDescriptors[idx] & pdesc) == pdesc) {
			if (port_idx == 0) {
				*res = idx;
				return 0;
			}
			port_idx--;
		}
	return -EINVAL;
}

static int snd_pcm_ladspa_find_sport(unsigned int *res,
				     snd_pcm_ladspa_plugin_t *lplug,
				     LADSPA_PortDescriptor pdesc,
				     const char *port_name)
{
	unsigned long idx;

	for (idx = 0; idx < lplug->desc->PortCount; idx++)
		if ((lplug->desc->PortDescriptors[idx] & pdesc) == pdesc &&
		    !strcmp(lplug->desc->PortNames[idx], port_name)) {
			*res = idx;
			return 0;
		}
	return -EINVAL;
}

static int snd_pcm_ladspa_find_port_idx(unsigned int *res,
					snd_pcm_ladspa_plugin_t *lplug,
					LADSPA_PortDescriptor pdesc,
					unsigned int port)
{
	unsigned long idx;
	unsigned int r = 0;

	if (port >= lplug->desc->PortCount)
		return -EINVAL;
	for (idx = 0; idx < port; idx++)
		if ((lplug->desc->PortDescriptors[idx] & pdesc) == pdesc)
			r++;
	*res = r;
	return 0;
}

static void snd_pcm_ladspa_free_plugins(struct list_head *plugins)
{
	while (!list_empty(plugins)) {
		snd_pcm_ladspa_plugin_t *plugin = list_entry(plugins->next, snd_pcm_ladspa_plugin_t, list);
		if (plugin->dl_handle)
			dlclose(plugin->dl_handle);
		if (plugin->filename)
			free(plugin->filename);
		list_del(&plugin->list);
		free(plugin);
	}
}

static void snd_pcm_ladspa_free(snd_pcm_ladspa_t *ladspa)
{
	snd_pcm_ladspa_free_plugins(&ladspa->pplugins);
	snd_pcm_ladspa_free_plugins(&ladspa->cplugins);
}

static int snd_pcm_ladspa_close(snd_pcm_t *pcm)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;

	snd_pcm_ladspa_free(ladspa);
	return snd_pcm_plugin_close(pcm);
}

static int snd_pcm_ladspa_hw_refine_cprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *params)
{
	// snd_pcm_ladspa_t *ladspa = pcm->private_data;
	int err;
	snd_pcm_access_mask_t access_mask = { SND_PCM_ACCBIT_SHMN };
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

static int snd_pcm_ladspa_hw_refine_sprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t *sparams)
{
	// snd_pcm_ladspa_t *ladspa = pcm->private_data;
	snd_pcm_access_mask_t saccess_mask = { SND_PCM_ACCBIT_MMAPN };
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
	// snd_pcm_ladspa_t *ladspa = pcm->private_data;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_ladspa_hw_refine_cchange,
					  snd_pcm_ladspa_hw_refine_sprepare,
					  snd_pcm_ladspa_hw_refine_schange,
					  snd_pcm_plugin_hw_params_slave);
	if (err < 0)
		return err;
	return 0;
}

static void snd_pcm_ladspa_free_instances(snd_pcm_t *pcm, snd_pcm_ladspa_t *ladspa, int cleanup)
{
	struct list_head *list, *pos, *pos1, *next1;
	
	if (ladspa->instances_channels == 0)
		return;
	list = pcm->stream == SND_PCM_STREAM_PLAYBACK ? &ladspa->pplugins : &ladspa->cplugins;
	list_for_each(pos, list) {
		snd_pcm_ladspa_plugin_t *plugin = list_entry(pos, snd_pcm_ladspa_plugin_t, list);
		list_for_each_safe(pos1, next1, &plugin->instances) {
			snd_pcm_ladspa_instance_t *instance = list_entry(pos1, snd_pcm_ladspa_instance_t, list);
			if (plugin->desc->deactivate)
				plugin->desc->deactivate(instance->handle);
			if (cleanup) {
				if (plugin->desc->cleanup)
					plugin->desc->cleanup(instance->handle);
				if (instance->m_data)
					free(instance->m_data);
				list_del(&(instance->list));
				free(instance);
			} else {
				if (plugin->desc->activate)
					plugin->desc->activate(instance->handle);
			}
		}
		if (cleanup) {
			assert(list_empty(&plugin->instances));
		}
	}
	if (cleanup) {
		ladspa->instances_channels = 0;
		if (ladspa->finstances) {
			free(ladspa->finstances);
			ladspa->finstances = NULL;
		}
	}
}

static int snd_pcm_ladspa_connect(snd_pcm_ladspa_plugin_t *plugin ATTRIBUTE_UNUSED,
				  snd_pcm_ladspa_plugin_io_t *io,
				  snd_pcm_ladspa_instance_t *instance,
				  unsigned int channel,
				  unsigned int port)
{
	if (instance->channel == NO_ASSIGN)
		instance->channel = channel;
	else if (instance->channel != channel)
		return -EINVAL;
	if (io->pdesc == LADSPA_PORT_OUTPUT) {
		instance->out_port = port;
	} else {
		instance->in_port = port;
	}
	return 0;
}

static int snd_pcm_ladspa_connect_plugin(snd_pcm_ladspa_plugin_t *plugin,
					 snd_pcm_ladspa_plugin_io_t *io,
					 snd_pcm_ladspa_instance_t *instance,
					 unsigned int idx)
{
	unsigned int port;
	int err;

	assert(plugin->policy == SND_PCM_LADSPA_POLICY_NONE);
	if (io->port_bindings_size > 0) {
		if (idx >= io->port_bindings_size)
			return instance->channel != NO_ASSIGN ? -EINVAL : 0;
		port = io->port_bindings[idx];
	} else {
		err = snd_pcm_ladspa_find_port(&port, plugin, io->pdesc | LADSPA_PORT_AUDIO, idx);
		if (err < 0)
			return instance->channel != NO_ASSIGN ? err : 0;
	}
	return snd_pcm_ladspa_connect(plugin, io, instance, idx, port);
}

static int snd_pcm_ladspa_connect_plugin_duplicate(snd_pcm_ladspa_plugin_t *plugin,
						   snd_pcm_ladspa_plugin_io_t *io,
						   snd_pcm_ladspa_instance_t *instance,
						   unsigned int idx)
{
	unsigned int port;
	int err;

	assert(plugin->policy == SND_PCM_LADSPA_POLICY_DUPLICATE);
	if (io->port_bindings_size > 0) {
		port = io->port_bindings[0];
	} else {
		err = snd_pcm_ladspa_find_port(&port, plugin, io->pdesc | LADSPA_PORT_AUDIO, 0);
		if (err < 0)
			return err;
	}
	return snd_pcm_ladspa_connect(plugin, io, instance, idx, port);
}

static int snd_pcm_ladspa_connect_controls(snd_pcm_ladspa_plugin_t *plugin,
					   snd_pcm_ladspa_plugin_io_t *io,
					   snd_pcm_ladspa_instance_t *instance)
{
	unsigned long idx, midx;

	for (idx = midx = 0; idx < plugin->desc->PortCount; idx++)
		if ((plugin->desc->PortDescriptors[idx] & (io->pdesc | LADSPA_PORT_CONTROL)) == (io->pdesc | LADSPA_PORT_CONTROL)) {
			if (io->controls_size > midx) {
				plugin->desc->connect_port(instance->handle, idx, &io->controls[midx]);
			} else {
				return -EINVAL;
			}
			midx++;
		}
	return 0;
}

static int snd_pcm_ladspa_allocate_instances(snd_pcm_t *pcm, snd_pcm_ladspa_t *ladspa)
{
	struct list_head *list, *pos;
	unsigned int depth, idx, count;
	snd_pcm_ladspa_instance_t *instance;
	int err;
	
	if (ladspa->instances_channels == 0)
		return 0;
	list = pcm->stream == SND_PCM_STREAM_PLAYBACK ? &ladspa->pplugins : &ladspa->cplugins;
	depth = 0;
	list_for_each(pos, list) {
		snd_pcm_ladspa_plugin_t *plugin = list_entry(pos, snd_pcm_ladspa_plugin_t, list);
		count = 1;
		if (plugin->policy == SND_PCM_LADSPA_POLICY_DUPLICATE)
			count = pcm->channels;
		for (idx = 0; idx < count; idx++) {
			instance = (snd_pcm_ladspa_instance_t *)calloc(1, sizeof(snd_pcm_ladspa_instance_t));
			if (instance == NULL)
				return -ENOMEM;
			instance->desc = plugin->desc;
			instance->handle = plugin->desc->instantiate(plugin->desc, pcm->rate);
			instance->depth = depth;
			instance->channel = NO_ASSIGN;
			if (instance->handle == NULL) {
				SNDERR("Unable to create instance of LADSPA plugin '%s'", plugin->desc->Name);
				free(instance);
				return -EINVAL;
			}
			list_add_tail(&instance->list, &plugin->instances);
			if (plugin->desc->activate)
				plugin->desc->activate(instance->handle);
			if (plugin->policy == SND_PCM_LADSPA_POLICY_DUPLICATE) {
				err = snd_pcm_ladspa_connect_plugin_duplicate(plugin, &plugin->input, instance, idx);
				if (err < 0) {
					SNDERR("Unable to connect duplicate input port of plugin '%s' channel %u depth %u", plugin->desc->Name, idx, instance->depth);
					return err;
				}
				err = snd_pcm_ladspa_connect_plugin_duplicate(plugin, &plugin->output, instance, idx);
				if (err < 0) {
					SNDERR("Unable to connect duplicate output port of plugin '%s' channel %u depth %u", plugin->desc->Name, idx, instance->depth);
					return err;
				}
			}
			err = snd_pcm_ladspa_connect_controls(plugin, &plugin->input, instance);
			assert(err >= 0);
			err = snd_pcm_ladspa_connect_controls(plugin, &plugin->output, instance);
			assert(err >= 0);
		}
		if (plugin->policy == SND_PCM_LADSPA_POLICY_NONE) {
			instance = list_entry(plugin->instances.next, snd_pcm_ladspa_instance_t, list);
			for (idx = 0; idx < pcm->channels; idx++) {
				err = snd_pcm_ladspa_connect_plugin(plugin, &plugin->input, instance, idx);
				if (err < 0) {
					SNDERR("Unable to connect input port of plugin '%s' channel %u depth %u", plugin->desc->Name, idx, depth);
					return err;
				}
				err = snd_pcm_ladspa_connect_plugin(plugin, &plugin->output, instance, idx);
				if (err < 0) {
					SNDERR("Unable to connect output port of plugin '%s' channel %u depth %u", plugin->desc->Name, idx, depth);
					return err;
				}
			}
		}
		depth++;
	}
	return 0;
}

static int snd_pcm_ladspa_allocate_imemory(snd_pcm_ladspa_instance_t *instance, size_t alloc_size)
{
	if (instance->prev)
		instance->in_data = instance->prev->out_data;
	else
		instance->in_data = NULL;
	if (!instance->prev ||
	    (instance->next && LADSPA_IS_INPLACE_BROKEN(instance->desc->Properties))) {
		instance->m_data = (LADSPA_Data *)malloc(alloc_size * sizeof(LADSPA_Data));
		if (instance->m_data == NULL)
			return -ENOMEM;
		instance->out_data = instance->m_data;
	} else {
		instance->out_data = instance->in_data;
	}
	return 0;
}

static int snd_pcm_ladspa_allocate_memory(snd_pcm_t *pcm, snd_pcm_ladspa_t *ladspa)
{
	struct list_head *list, *pos, *pos1;
	snd_pcm_ladspa_instance_t *instance, *prev;
	unsigned int channel;
	int err;
	
	if (ladspa->instances_channels == 0)
		return 0;
	ladspa->finstances = (snd_pcm_ladspa_instance_t **)calloc(ladspa->instances_channels, sizeof(snd_pcm_ladspa_instance_t *));
	if (ladspa->finstances == NULL)
		return -ENOMEM;
	list = pcm->stream == SND_PCM_STREAM_PLAYBACK ? &ladspa->pplugins : &ladspa->cplugins;
	for (channel = 0; channel < ladspa->instances_channels; channel++) {
		prev = NULL;
		list_for_each(pos, list) {
			snd_pcm_ladspa_plugin_t *plugin = list_entry(pos, snd_pcm_ladspa_plugin_t, list);
			instance = NULL;
			if (list_empty(&plugin->instances))
				continue;
			list_for_each(pos1, &plugin->instances) {
				instance = list_entry(pos1, snd_pcm_ladspa_instance_t, list);
				if (instance->channel == NO_ASSIGN) {
					SNDERR("channel %u is not assigned for plugin '%s' depth %u", plugin->desc->Name, instance->channel, instance->depth);
					return -EINVAL;
				}
				if (instance->channel != channel) {
					instance = NULL;
					continue;
				}
				break;
			}
			if (instance == NULL)
				continue;
			if (ladspa->finstances[channel] == NULL)
				ladspa->finstances[channel] = instance;
			instance->prev = prev;
			if (prev == NULL) {
				prev = instance;
				continue;		/* nothing to do */
			}
			prev->next = instance;
		}
	}
	for (channel = 0; channel < ladspa->instances_channels; channel++) {
		instance = ladspa->finstances[channel];
		if (instance == NULL)
			continue;
		err = snd_pcm_ladspa_allocate_imemory(instance, pcm->buffer_size);
		if (err < 0)
			return err;
		break;
	}
	return 0;
}

static int snd_pcm_ladspa_init(snd_pcm_t *pcm)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	int err;
	
	if (pcm->channels != ladspa->instances_channels) {
		snd_pcm_ladspa_free_instances(pcm, ladspa, 1);
		ladspa->instances_channels = pcm->channels;
		err = snd_pcm_ladspa_allocate_instances(pcm, ladspa);
		if (err < 0) {
			snd_pcm_ladspa_free_instances(pcm, ladspa, 1);
			return err;
		}
		err = snd_pcm_ladspa_allocate_memory(pcm, ladspa);
		if (err < 0) {
			snd_pcm_ladspa_free_instances(pcm, ladspa, 1);
			return err;
		}
	} else {
		snd_pcm_ladspa_free_instances(pcm, ladspa, 0);
	}
	return 0;
}

static int snd_pcm_ladspa_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;

	snd_pcm_ladspa_free_instances(pcm, ladspa, 1);
	return snd_pcm_plugin_hw_free(pcm);
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
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	unsigned int channel;
	
	if (size > *slave_sizep)
		size = *slave_sizep;
#if 0	// no processing - for testing purposes only
	snd_pcm_areas_copy(slave_areas, slave_offset,
			   areas, offset,
			   pcm->channels, size, pcm->format);
#else
	for (channel = 0; channel < ladspa->instances_channels; channel++) {
		LADSPA_Data *data = (LADSPA_Data *)((char *)areas[channel].addr + (areas[channel].first / 8));
		snd_pcm_ladspa_instance_t *instance = ladspa->finstances[channel];
		data += offset;
		if (instance == NULL)
			snd_pcm_area_copy(&slave_areas[channel], slave_offset,
					  &areas[channel], offset,
					  size, SND_PCM_FORMAT_FLOAT);
		while (instance) {
			if (instance->in_data != NULL)
				data = instance->in_data;
			instance->desc->connect_port(instance->handle, instance->in_port, data);
			if (instance->next == NULL) {
				data = (LADSPA_Data *)((char *)slave_areas[channel].addr + (slave_areas[channel].first / 8));
				data += slave_offset;
			} else if (instance->out_data != NULL)
				data = instance->out_data;
			instance->desc->connect_port(instance->handle, instance->out_port, data);
			instance->desc->run(instance->handle, size);
			instance = instance->next;
		}
	}
#endif
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
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	unsigned int channel;

	if (size > *slave_sizep)
		size = *slave_sizep;
#if 0	// no processing - for testing purposes only
	snd_pcm_areas_copy(areas, offset,
			   slave_areas, slave_offset,
			   pcm->channels, size, pcm->format);
#else
	for (channel = 0; channel < ladspa->instances_channels; channel++) {
		LADSPA_Data *data = (LADSPA_Data *)((char *)slave_areas[channel].addr + (slave_areas[channel].first / 8));
		snd_pcm_ladspa_instance_t *instance = ladspa->finstances[channel];
		data += slave_offset;
		if (instance == NULL)
			snd_pcm_area_copy(&slave_areas[channel], slave_offset,
					  &areas[channel], offset,
					  size, SND_PCM_FORMAT_FLOAT);
		while (instance) {
			if (instance->in_data != NULL)
				data = instance->in_data;
			instance->desc->connect_port(instance->handle, instance->in_port, data);
			if (instance->next == NULL) {
				data = (LADSPA_Data *)((char *)areas[channel].addr + (areas[channel].first / 8));
				data += offset;
			} else if (instance->out_data != NULL)
				data = instance->out_data;
			instance->desc->connect_port(instance->handle, instance->out_port, data);
			instance->desc->run(instance->handle, size);
			instance = instance->next;
		}
	}
#endif
	*slave_sizep = size;
	return size;
}

static void snd_pcm_ladspa_dump_direction(snd_pcm_ladspa_plugin_io_t *io, snd_output_t *out)
{
	unsigned int idx;

	if (io->port_bindings_size == 0)
		goto __control;
	snd_output_printf(out, "Audio %s port bindings:", io->pdesc == LADSPA_PORT_INPUT ? "input" : "output");
	for (idx = 0; idx < io->port_bindings_size; idx++) {
		if (io->port_bindings[idx] != NO_ASSIGN) 
			continue;
		snd_output_printf(out, " %i -> %i", idx, io->port_bindings[idx]);
	}
	snd_output_printf(out, "\n");
      __control:
      	if (io->controls_size == 0)
      		return;
	snd_output_printf(out, "Control %s port initial values:", io->pdesc == LADSPA_PORT_INPUT ? "input" : "output");
	for (idx = 0; idx < io->controls_size; idx++)
		snd_output_printf(out, " %i = %.8f", idx, io->controls[idx]);
	snd_output_printf(out, "\n");
}

static void snd_pcm_ladspa_plugins_dump(struct list_head *list, snd_output_t *out)
{
	struct list_head *pos;
	
	list_for_each(pos, list) {
		snd_pcm_ladspa_plugin_t *plugin = list_entry(pos, snd_pcm_ladspa_plugin_t, list);
		snd_pcm_ladspa_dump_direction(&plugin->input, out);
		snd_pcm_ladspa_dump_direction(&plugin->output, out);
	}
}

static void snd_pcm_ladspa_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_ladspa_t *ladspa = pcm->private_data;
	snd_output_printf(out, "LADSPA PCM\n");
	snd_pcm_ladspa_plugins_dump(&ladspa->pplugins, out);
	snd_pcm_ladspa_plugins_dump(&ladspa->cplugins, out);
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(ladspa->plug.slave, out);
}

static snd_pcm_ops_t snd_pcm_ladspa_ops = {
	.close = snd_pcm_ladspa_close,
	.info = snd_pcm_plugin_info,
	.hw_refine = snd_pcm_ladspa_hw_refine,
	.hw_params = snd_pcm_ladspa_hw_params,
	.hw_free = snd_pcm_ladspa_hw_free,
	.sw_params = snd_pcm_plugin_sw_params,
	.channel_info = snd_pcm_plugin_channel_info,
	.dump = snd_pcm_ladspa_dump,
	.nonblock = snd_pcm_plugin_nonblock,
	.async = snd_pcm_plugin_async,
	.poll_revents = snd_pcm_plugin_poll_revents,
	.mmap = snd_pcm_plugin_mmap,
	.munmap = snd_pcm_plugin_munmap,
};

static int snd_pcm_ladspa_check_file(snd_pcm_ladspa_plugin_t * const plugin,
				     const char *filename,
				     const char *label,
				     const unsigned long ladspa_id)
{
	void *handle;

	assert(filename);
	handle = dlopen(filename, RTLD_LAZY);
	if (handle) {
		LADSPA_Descriptor_Function fcn = (LADSPA_Descriptor_Function)dlsym(handle, "ladspa_descriptor");
		if (fcn) {
			long idx;
			const LADSPA_Descriptor *d;
			for (idx = 0; (d = fcn(idx)) != NULL; idx++) {
				if (strcmp(label, d->Label))
					continue;
				if (ladspa_id > 0 && d->UniqueID != ladspa_id)
					continue;
				plugin->filename = strdup(filename);
				if (plugin->filename == NULL)
					return -ENOMEM;
				plugin->dl_handle = handle;
				plugin->desc = d;
				return 1;
			}
		}
		dlclose(handle);
	}
	return -ENOENT;
}

static int snd_pcm_ladspa_check_dir(snd_pcm_ladspa_plugin_t * const plugin,
				    const char *path,
				    const char *label,
				    const unsigned long ladspa_id)
{
	DIR *dir;
	struct dirent * dirent;
	int len = strlen(path), err;
	int need_slash;
	char *filename;
	
	if (len < 1)
		return 0;
	need_slash = path[len - 1] != '/';
	
	dir = opendir(path);
	if (!dir)
		return -ENOENT;
		
	while (1) {
		dirent = readdir(dir);
		if (!dirent) {
			closedir(dir);
			return 0;
		}
		
		filename = malloc(len + strlen(dirent->d_name) + 1 + need_slash);
		strcpy(filename, path);
		if (need_slash)
			strcat(filename, "/");
		strcat(filename, dirent->d_name);
		err = snd_pcm_ladspa_check_file(plugin, filename, label, ladspa_id);
		free(filename);
		if (err < 0 && err != -ENOENT)
			return err;
		if (err > 0)
			return 1;
	}
	/* never reached */
	return 0;
}

static int snd_pcm_ladspa_look_for_plugin(snd_pcm_ladspa_plugin_t * const plugin,
					  const char *path,
					  const char *label,
					  const long ladspa_id)
{
	const char *c;
	size_t l;
	wordexp_t we;
	int err;
	
	for (c = path; (l = strcspn(c, ": ")) > 0; ) {
		char name[l + 1];
		memcpy(name, c, l);
		name[l] = 0;
		err = wordexp(name, &we, WRDE_NOCMD);
		switch (err) {
		case WRDE_NOSPACE:
			return -ENOMEM;
		case 0:
			if (we.we_wordc == 1)
				break;
			/* Fall through */
		default:
			return -EINVAL;
		}
		err = snd_pcm_ladspa_check_dir(plugin, we.we_wordv[0], label, ladspa_id);
		wordfree(&we);
		if (err < 0)
			return err;
		if (err > 0)
			return 0;
		c += l;
		if (!*c)
			break;
		c++;
	}
	return -ENOENT;
}					  

static int snd_pcm_ladspa_parse_ioconfig(snd_pcm_ladspa_plugin_t *lplug,
					 snd_pcm_ladspa_plugin_io_t *io,
					 snd_config_t *conf)
{
	snd_config_iterator_t i, next;
	snd_config_t *bindings = NULL, *controls = NULL;
	int err;
	
	if (conf == NULL)
		return 0;
	if (snd_config_get_type(conf) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("input or output definition must be a compound");
		return -EINVAL;
	}
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (strcmp(id, "bindings") == 0) {
			bindings = n;
			continue;
		}
		if (strcmp(id, "controls") == 0) {
			controls = n;
			continue;
		}
	}
	if (bindings) {
		unsigned int count = 0;
		unsigned int *array;
		if (snd_config_get_type(bindings) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("bindings definition must be a compound");
			return -EINVAL;
		}
		snd_config_for_each(i, next, bindings) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			long channel;
			if (snd_config_get_id(n, &id) < 0)
				continue;
			err = safe_strtol(id, &channel);
			if (err < 0 || channel < 0) {
				SNDERR("Invalid channel number: %s", id);
				return -EINVAL;
			}
			if (lplug->policy == SND_PCM_LADSPA_POLICY_DUPLICATE && channel > 0) {
				SNDERR("Wrong channel specification for duplicate policy");
				return -EINVAL;
			}
			if (count < (unsigned int)(channel + 1))
				count = (unsigned int)(channel + 1);
		}
		if (count > 0) {
			array = (unsigned int *)calloc(count, sizeof(unsigned int));
			memset(array, 0xff, count * sizeof(unsigned int));
			io->port_bindings_size = count;
			io->port_bindings = array;
			snd_config_for_each(i, next, bindings) {
				snd_config_t *n = snd_config_iterator_entry(i);
				const char *id, *sport;
				long channel, port;
				if (snd_config_get_id(n, &id) < 0)
					continue;
				err = safe_strtol(id, &channel);
				if (err < 0 || channel < 0) {
					assert(0);	/* should never happen */
					return -EINVAL;
				}
				err = snd_config_get_integer(n, &port);
				if (err >= 0) {
					err = snd_pcm_ladspa_find_port(&array[channel], lplug, io->pdesc | LADSPA_PORT_AUDIO, port);
					if (err < 0) {
						SNDERR("Unable to find an audio port (%li) for channel %s", port);
						return err;
					}
					continue;
				}
				err = snd_config_get_string(n, &sport);
				if (err < 0) {
					SNDERR("Invalid LADSPA port field type for %s", id);
					return -EINVAL;
				}
				err = snd_pcm_ladspa_find_sport(&array[channel], lplug, io->pdesc | LADSPA_PORT_AUDIO, sport);
				if (err < 0) {
					SNDERR("Unable to find an audio port (%s) for channel %s", sport, id);
					return err;
				}
			}
		}
	}
	if (controls) {
		unsigned int count = 0;
		LADSPA_Data *array;
		unsigned long idx;
		if (snd_config_get_type(controls) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("controls definition must be a compound");
			return -EINVAL;
		}
		for (idx = 0; idx < lplug->desc->PortCount; idx++)
			if ((lplug->desc->PortDescriptors[idx] & (io->pdesc | LADSPA_PORT_CONTROL)) == (io->pdesc | LADSPA_PORT_CONTROL))
				count++;
		array = (LADSPA_Data *)calloc(count, sizeof(LADSPA_Data));
		io->controls_size = count;
		io->controls = array;
		snd_config_for_each(i, next, controls) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			long lval;
			unsigned int port, uval;
			double dval;
			if (snd_config_get_id(n, &id) < 0)
				continue;
			err = safe_strtol(id, &lval);
			if (err >= 0) {
				err = snd_pcm_ladspa_find_port(&port, lplug, io->pdesc | LADSPA_PORT_CONTROL, lval);
			} else {
				err = snd_pcm_ladspa_find_sport(&port, lplug, io->pdesc | LADSPA_PORT_CONTROL, id);
			}
			if (err < 0) {
				SNDERR("Unable to find an control port (%s)", id);
				return err;
			}
			if (snd_config_get_ireal(n, &dval) < 0) {
				SNDERR("Control port %s has not an float or integer value", id);
				return err;
			}
			err = snd_pcm_ladspa_find_port_idx(&uval, lplug, io->pdesc | LADSPA_PORT_CONTROL, port);
			if (err < 0) {
				SNDERR("internal error");
				return err;
			}
			array[uval] = (LADSPA_Data)dval;
		}
	}
	return 0;
}

static int snd_pcm_ladspa_add_plugin(struct list_head *list,
				     const char *path,
				     snd_config_t *plugin,
				     int reverse)
{
	snd_config_iterator_t i, next;
	const char *label = NULL, *filename = NULL;
	long ladspa_id = 0;
	int err;
	snd_pcm_ladspa_plugin_t *lplug;
	snd_pcm_ladspa_policy_t policy = SND_PCM_LADSPA_POLICY_DUPLICATE;
	snd_config_t *input = NULL, *output = NULL;

	snd_config_for_each(i, next, plugin) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (strcmp(id, "label") == 0) {
			err = snd_config_get_string(n, &label);
			if (err < 0)
				return err;
			continue;
		}
		if (strcmp(id, "id") == 0) {
			err = snd_config_get_integer(n, &ladspa_id);
			if (err < 0)
				return err;
			continue;
		}
		if (strcmp(id, "filename") == 0) {
			err = snd_config_get_string(n, &filename);
			if (err < 0)
				return err;
			continue;
		}
		if (strcmp(id, "input") == 0) {
			input = n;
			continue;
		}
		if (strcmp(id, "output") == 0) {
			output = n;
			continue;
		}
		if (strcmp(id, "policy") == 0) {
			const char *str;
			err = snd_config_get_string(n, &str);
			if (err < 0) {
				SNDERR("policy field must be a string");
				return err;
			}
			if (strcmp(str, "none") == 0)
				policy = SND_PCM_LADSPA_POLICY_NONE;
			else if (strcmp(str, "duplicate") == 0)
				policy = SND_PCM_LADSPA_POLICY_DUPLICATE;
			else {
				SNDERR("unknown policy definition");
				return -EINVAL;
			}
			continue;
		}
	}
	if (label == NULL && ladspa_id <= 0) {
		SNDERR("no plugin label or id");
		return -EINVAL;
	}
	lplug = (snd_pcm_ladspa_plugin_t *)calloc(1, sizeof(snd_pcm_ladspa_plugin_t));
	if (lplug == NULL)
		return -ENOMEM;
	lplug->policy = policy;
	lplug->input.pdesc = LADSPA_PORT_INPUT;
	lplug->output.pdesc = LADSPA_PORT_OUTPUT;
	INIT_LIST_HEAD(&lplug->instances);
	if (filename) {
		err = snd_pcm_ladspa_check_file(lplug, filename, label, ladspa_id);
		if (err < 0) {
			SNDERR("Unable to load plugin '%s' ID %li, filename '%s'", label, ladspa_id, filename);
			free(lplug);
			return err;
		}
	} else {
		err = snd_pcm_ladspa_look_for_plugin(lplug, path, label, ladspa_id);
		if (err < 0) {
			SNDERR("Unable to find or load plugin '%s' ID %li, path '%s'", label, ladspa_id, path);
			free(lplug);
			return err;
		}
	}
	if (!reverse) {
		list_add_tail(&lplug->list, list);
	} else {
		list_add(&lplug->list, list);
	}
	err = snd_pcm_ladspa_parse_ioconfig(lplug, &lplug->input, input);
	if (err < 0)
		return err;
	err = snd_pcm_ladspa_parse_ioconfig(lplug, &lplug->output, output);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_ladspa_build_plugins(struct list_head *list,
					const char *path,
					snd_config_t *plugins,
					int reverse)
{
	snd_config_iterator_t i, next;
	int idx = 0, hit, err;

	if (plugins == NULL)	/* nothing TODO */
		return 0;
	if (snd_config_get_type(plugins) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("plugins must be defined inside a compound");
		return -EINVAL;
	}
	do {
		hit = 0;
		snd_config_for_each(i, next, plugins) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			long i;
			if (snd_config_get_id(n, &id) < 0)
				continue;
			err = safe_strtol(id, &i);
			if (err < 0) {
				SNDERR("id of field %s is not an integer", id);
				return err;
			}
			if (i == idx) {
				idx++;
				err = snd_pcm_ladspa_add_plugin(list, path, n, reverse);
				if (err < 0)
					return err;
				hit = 1;
			}
		}
	} while (hit);
	if (list_empty(list)) {
		SNDERR("empty plugin list is not accepted");
		return -EINVAL;
	}
	return 0;
}

/**
 * \brief Creates a new LADSPA<->ALSA Plugin
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param sformat Slave (destination) format
 * \param slave Slave PCM handle
 * \param close_slave When set, the slave PCM handle is closed with copy PCM
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_ladspa_open(snd_pcm_t **pcmp, const char *name,
			const char *ladspa_path,
			snd_config_t *ladspa_pplugins,
			snd_config_t *ladspa_cplugins,
			snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_ladspa_t *ladspa;
	int err, reverse = 0;

	assert(pcmp && (ladspa_pplugins || ladspa_cplugins) && slave);

	if (!ladspa_path && !(ladspa_path = getenv("LADSPA_PATH")))
		return -ENOENT;
	ladspa = calloc(1, sizeof(snd_pcm_ladspa_t));
	if (!ladspa)
		return -ENOMEM;
	snd_pcm_plugin_init(&ladspa->plug);
	ladspa->plug.init = snd_pcm_ladspa_init;
	ladspa->plug.read = snd_pcm_ladspa_read_areas;
	ladspa->plug.write = snd_pcm_ladspa_write_areas;
	ladspa->plug.undo_read = snd_pcm_plugin_undo_read_generic;
	ladspa->plug.undo_write = snd_pcm_plugin_undo_write_generic;
	ladspa->plug.slave = slave;
	ladspa->plug.close_slave = close_slave;

	INIT_LIST_HEAD(&ladspa->pplugins);
	INIT_LIST_HEAD(&ladspa->cplugins);

	if (slave->stream == SND_PCM_STREAM_PLAYBACK) {
		err = snd_pcm_ladspa_build_plugins(&ladspa->pplugins, ladspa_path, ladspa_pplugins, reverse);
		if (err < 0) {
			snd_pcm_ladspa_free(ladspa);
			return err;
		}
	}
	if (slave->stream == SND_PCM_STREAM_CAPTURE) {
		if (ladspa_cplugins == ladspa_pplugins)
			reverse = 1;
		err = snd_pcm_ladspa_build_plugins(&ladspa->cplugins, ladspa_path, ladspa_cplugins, reverse);
		if (err < 0) {
			snd_pcm_ladspa_free(ladspa);
			return err;
		}
	}

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_LADSPA, name, slave->stream, slave->mode);
	if (err < 0) {
		snd_pcm_ladspa_free(ladspa);
		return err;
	}
	pcm->ops = &snd_pcm_ladspa_ops;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->private_data = ladspa;
	pcm->poll_fd = slave->poll_fd;
	pcm->poll_events = slave->poll_events;
	snd_pcm_set_hw_ptr(pcm, &ladspa->plug.hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &ladspa->plug.appl_ptr, -1, 0);
	*pcmp = pcm;

	return 0;
}

/*! \page pcm_plugins

\section pcm_plugins_ladpsa Plugin: LADSPA <-> ALSA

This plugin allows to apply a set of LADPSA plugins.
The input and output format is always #SND_PCM_FORMAT_FLOAT (note: this type
can be either little or big-endian depending on architecture).

The policy duplicate means that there must be only one binding definition for
channel zero. This definition is automatically duplicated for all channels.

Instances of LADSPA plugins are created dynamically.

\code
pcm.name {
        type ladspa             # ALSA<->LADSPA PCM
        slave STR               # Slave name
        # or
        slave {                 # Slave definition
                pcm STR         # Slave PCM name
                # or
                pcm { }         # Slave PCM definition
        }
	[path STR]		# Path (directory) with LADSPA plugins
	plugins |		# Definition for both directions
        playback_plugins |	# Definition for playback direction
	capture_plugins {	# Definition for capture direction
		N {		# Configuration for LADPSA plugin N
			[id INT]	# LADSPA plugin ID (for example 1043)
			[label STR]	# LADSPA plugin label (for example 'delay_5s')
			[filename STR]	# Full filename of .so library with LADSPA plugin code
			[policy STR]	# Policy can be 'none' or 'duplicate'
			input | output {
				bindings {
					C INT or STR	# C - channel, INT - audio port index, STR - audio port name
				}
				controls {
					I INT or REAL	# I - control port index, INT or REAL - control value
					# or
					STR INT or REAL	# STR - control port name, INT or REAL - control value
				}
			}
		}
	}
}
\endcode

\subsection pcm_plugins_ladspa_funcref Function reference

<UL>
  <LI>snd_pcm_ladspa_open()
  <LI>_snd_pcm_ladspa_open()
</UL>

*/

/**
 * \brief Creates a new LADSPA<->ALSA PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with LADSPA<->ALSA PCM description
 * \param stream Stream type
 * \param mode Stream mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_ladspa_open(snd_pcm_t **pcmp, const char *name,
			 snd_config_t *root, snd_config_t *conf, 
			 snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *slave = NULL, *sconf;
	const char *path = NULL;
	snd_config_t *plugins = NULL, *pplugins = NULL, *cplugins = NULL;
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
		if (strcmp(id, "path") == 0) {
			snd_config_get_string(n, &path);
			continue;
		}
		if (strcmp(id, "plugins") == 0) {
			plugins = n;
			continue;
		}
		if (strcmp(id, "playback_plugins") == 0) {
			pplugins = n;
			continue;
		}
		if (strcmp(id, "capture_plugins") == 0) {
			cplugins = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	if (plugins) {
		if (pplugins || cplugins) {
			SNDERR("'plugins' definition cannot be combined with 'playback_plugins' or 'capture_plugins'");
			return -EINVAL;
		}
		pplugins = plugins;
		cplugins = plugins;
	}
	err = snd_pcm_slave_conf(root, slave, &sconf, 0);
	if (err < 0)
		return err;
	err = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
	snd_config_delete(sconf);
	if (err < 0)
		return err;
	err = snd_pcm_ladspa_open(pcmp, name, path, pplugins, cplugins, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_ladspa_open, SND_PCM_DLSYM_VERSION);
#endif
