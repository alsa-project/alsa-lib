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

#define NO_ASSIGN	0xffffffff

typedef enum _snd_pcm_ladspa_policy {
	SND_PCM_LADSPA_POLICY_NONE,		/* use bindings only */
	SND_PCM_LADSPA_POLICY_DUPLICATE		/* duplicate bindings for all channels */
} snd_pcm_ladspa_policy_t;

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	struct list_head pplugins;
	struct list_head cplugins;
} snd_pcm_ladspa_t;

typedef struct {
	LADSPA_PortDescriptor pdesc;		/* port description */
	unsigned int port_bindings_size;	/* size of array */
	unsigned int *port_bindings;		/* index = channel number, value = LADSPA input port */
	unsigned int controls_size;		/* size of array */
	LADSPA_Data *controls;			/* index = LADSPA control port */
} snd_pcm_ladspa_plugin_io_t;

typedef struct {
	struct list_head list;
	snd_pcm_ladspa_policy_t policy;
	char *filename;
	void *dl_handle;
	const LADSPA_Descriptor *desc;
	snd_pcm_ladspa_plugin_io_t input;
	snd_pcm_ladspa_plugin_io_t output;
} snd_pcm_ladspa_plugin_t;

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
	snd_pcm_ladspa_plugins_dump(&ladspa->pplugins, out);
	snd_pcm_ladspa_plugins_dump(&ladspa->cplugins, out);
	snd_output_printf(out, "LADSPA PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(ladspa->plug.slave, out);
}

snd_pcm_ops_t snd_pcm_ladspa_ops = {
	close: snd_pcm_ladspa_close,
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
					SNDERR("Invalid channel number: %s", id);
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
			unsigned int port;
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
			array[port] = (LADSPA_Data)dval;
		}
	}
	return 0;
}

static int snd_pcm_ladspa_add_plugin(struct list_head *list,
				     const char *path,
				     snd_config_t *plugin)
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
	list_add(&lplug->list, list);
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
					snd_config_t *plugins)
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
				err = snd_pcm_ladspa_add_plugin(list, path, n);
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

int snd_pcm_ladspa_open(snd_pcm_t **pcmp, const char *name,
			const char *ladspa_path,
			snd_config_t *ladspa_pplugins,
			snd_config_t *ladspa_cplugins,
			snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_ladspa_t *ladspa;
	int err;

	assert(pcmp && (ladspa_pplugins || ladspa_cplugins) && slave);

	if (!ladspa_path && !(ladspa_path = getenv("LADSPA_PATH")))
		return -ENOENT;
	ladspa = calloc(1, sizeof(snd_pcm_ladspa_t));
	if (!ladspa)
		return -ENOMEM;
	ladspa->plug.read = snd_pcm_ladspa_read_areas;
	ladspa->plug.write = snd_pcm_ladspa_write_areas;
	ladspa->plug.slave = slave;
	ladspa->plug.close_slave = close_slave;

	INIT_LIST_HEAD(&ladspa->pplugins);
	INIT_LIST_HEAD(&ladspa->cplugins);

	err = snd_pcm_ladspa_build_plugins(&ladspa->pplugins, ladspa_path, ladspa_pplugins);
	if (err < 0) {
		snd_pcm_ladspa_free(ladspa);
		return err;
	}
	err = snd_pcm_ladspa_build_plugins(&ladspa->cplugins, ladspa_path, ladspa_cplugins);
	if (err < 0) {
		snd_pcm_ladspa_free(ladspa);
		return err;
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
	const char *path = NULL;
	snd_config_t *pplugins = NULL, *cplugins = NULL;
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
SND_DLSYM_BUILD_VERSION(_snd_pcm_ladspa_open, SND_PCM_DLSYM_VERSION);
