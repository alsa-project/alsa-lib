/*
 *  PCM - Hook functions
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
  
#include <dlfcn.h>
#include "pcm_local.h"

#ifndef DOC_HIDDEN
struct _snd_pcm_hook {
	snd_pcm_t *pcm;
	snd_pcm_hook_func_t func;
	void *private_data;
	struct list_head list;
};

typedef struct {
	snd_pcm_t *slave;
	int close_slave;
	struct list_head hooks[SND_PCM_HOOK_TYPE_LAST + 1];
} snd_pcm_hooks_t;

static int snd_pcm_hooks_close(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	struct list_head *pos, *next;
	unsigned int k;
	int err;
	if (h->close_slave) {
		err = snd_pcm_close(h->slave);
		if (err < 0)
			return err;
	}
	list_for_each_safe(pos, next, &h->hooks[SND_PCM_HOOK_TYPE_CLOSE]) {
		snd_pcm_hook_t *hook = list_entry(pos, snd_pcm_hook_t, list);
		err = hook->func(hook);
		if (err < 0)
			return err;
	}
	for (k = 0; k <= SND_PCM_HOOK_TYPE_LAST; ++k) {
		struct list_head *hooks = &h->hooks[k];
		while (!list_empty(hooks)) {
			snd_pcm_hook_t *hook;
			pos = hooks->next;
			hook = list_entry(pos, snd_pcm_hook_t, list);
			snd_pcm_hook_remove(hook);
		}
	}
	free(h);
	return 0;
}

static int snd_pcm_hooks_nonblock(snd_pcm_t *pcm, int nonblock)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_nonblock(h->slave, nonblock);
}

static int snd_pcm_hooks_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_async(h->slave, sig, pid);
}

static int snd_pcm_hooks_info(snd_pcm_t *pcm, snd_pcm_info_t *info)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_info(h->slave, info);
}

static int snd_pcm_hooks_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_channel_info(h->slave, info);
}

static int snd_pcm_hooks_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_status(h->slave, status);
}

static snd_pcm_state_t snd_pcm_hooks_state(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_state(h->slave);
}

static int snd_pcm_hooks_delay(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_delay(h->slave, delayp);
}

static int snd_pcm_hooks_prepare(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_prepare(h->slave);
}

static int snd_pcm_hooks_reset(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_reset(h->slave);
}

static int snd_pcm_hooks_start(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_start(h->slave);
}

static int snd_pcm_hooks_drop(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_drop(h->slave);
}

static int snd_pcm_hooks_drain(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_drain(h->slave);
}

static int snd_pcm_hooks_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_pause(h->slave, enable);
}

static snd_pcm_sframes_t snd_pcm_hooks_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_rewind(h->slave, frames);
}

static snd_pcm_sframes_t snd_pcm_hooks_writei(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_writei(h->slave, buffer, size);
}

static snd_pcm_sframes_t snd_pcm_hooks_writen(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_writen(h->slave, bufs, size);
}

static snd_pcm_sframes_t snd_pcm_hooks_readi(snd_pcm_t *pcm, void *buffer, snd_pcm_uframes_t size)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_readi(h->slave, buffer, size);
}

static snd_pcm_sframes_t snd_pcm_hooks_readn(snd_pcm_t *pcm, void **bufs, snd_pcm_uframes_t size)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_readn(h->slave, bufs, size);
}

static snd_pcm_sframes_t snd_pcm_hooks_mmap_commit(snd_pcm_t *pcm,
						   snd_pcm_uframes_t offset,
						   snd_pcm_uframes_t size)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_mmap_commit(h->slave, offset, size);
}

static snd_pcm_sframes_t snd_pcm_hooks_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_avail_update(h->slave);
}

static int snd_pcm_hooks_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_hw_refine(h->slave, params);
}

static int snd_pcm_hooks_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	struct list_head *pos, *next;
	int err = snd_pcm_hw_params(h->slave, params);
	if (err < 0)
		return err;
	list_for_each_safe(pos, next, &h->hooks[SND_PCM_HOOK_TYPE_HW_PARAMS]) {
		snd_pcm_hook_t *hook = list_entry(pos, snd_pcm_hook_t, list);
		err = hook->func(hook);
		if (err < 0)
			return err;
	}
	return 0;
}

static int snd_pcm_hooks_hw_free(snd_pcm_t *pcm)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	struct list_head *pos, *next;
	int err = snd_pcm_hw_free(h->slave);
	if (err < 0)
		return err;
	list_for_each_safe(pos, next, &h->hooks[SND_PCM_HOOK_TYPE_HW_FREE]) {
		snd_pcm_hook_t *hook = list_entry(pos, snd_pcm_hook_t, list);
		err = hook->func(hook);
		if (err < 0)
			return err;
	}
	return 0;
}

static int snd_pcm_hooks_sw_params(snd_pcm_t *pcm, snd_pcm_sw_params_t * params)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	return snd_pcm_sw_params(h->slave, params);
}

static int snd_pcm_hooks_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_hooks_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_hooks_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_hooks_t *h = pcm->private_data;
	snd_output_printf(out, "Hooks PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(h->slave, out);
}

snd_pcm_ops_t snd_pcm_hooks_ops = {
	close: snd_pcm_hooks_close,
	info: snd_pcm_hooks_info,
	hw_refine: snd_pcm_hooks_hw_refine,
	hw_params: snd_pcm_hooks_hw_params,
	hw_free: snd_pcm_hooks_hw_free,
	sw_params: snd_pcm_hooks_sw_params,
	channel_info: snd_pcm_hooks_channel_info,
	dump: snd_pcm_hooks_dump,
	nonblock: snd_pcm_hooks_nonblock,
	async: snd_pcm_hooks_async,
	mmap: snd_pcm_hooks_mmap,
	munmap: snd_pcm_hooks_munmap,
};

snd_pcm_fast_ops_t snd_pcm_hooks_fast_ops = {
	status: snd_pcm_hooks_status,
	state: snd_pcm_hooks_state,
	delay: snd_pcm_hooks_delay,
	prepare: snd_pcm_hooks_prepare,
	reset: snd_pcm_hooks_reset,
	start: snd_pcm_hooks_start,
	drop: snd_pcm_hooks_drop,
	drain: snd_pcm_hooks_drain,
	pause: snd_pcm_hooks_pause,
	rewind: snd_pcm_hooks_rewind,
	writei: snd_pcm_hooks_writei,
	writen: snd_pcm_hooks_writen,
	readi: snd_pcm_hooks_readi,
	readn: snd_pcm_hooks_readn,
	avail_update: snd_pcm_hooks_avail_update,
	mmap_commit: snd_pcm_hooks_mmap_commit,
};

int snd_pcm_hooks_open(snd_pcm_t **pcmp, const char *name, snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_hooks_t *h;
	unsigned int k;
	assert(pcmp && slave);
	h = calloc(1, sizeof(snd_pcm_hooks_t));
	if (!h)
		return -ENOMEM;
	h->slave = slave;
	h->close_slave = close_slave;
	for (k = 0; k <= SND_PCM_HOOK_TYPE_LAST; ++k) {
		INIT_LIST_HEAD(&h->hooks[k]);
	}
	pcm = calloc(1, sizeof(snd_pcm_t));
	if (!pcm) {
		free(h);
		return -ENOMEM;
	}
	if (name)
		pcm->name = strdup(name);
	pcm->type = SND_PCM_TYPE_HOOKS;
	pcm->stream = slave->stream;
	pcm->mode = slave->mode;
	pcm->ops = &snd_pcm_hooks_ops;
	pcm->op_arg = pcm;
	pcm->fast_ops = &snd_pcm_hooks_fast_ops;
	pcm->fast_op_arg = pcm;
	pcm->private_data = h;
	pcm->poll_fd = slave->poll_fd;
	pcm->hw_ptr = slave->hw_ptr;
	pcm->appl_ptr = slave->appl_ptr;
	*pcmp = pcm;

	return 0;
}

int snd_pcm_hook_add_conf(snd_pcm_t *pcm, snd_config_t *conf)
{
	int err;
	char buf[256];
	const char *str;
	const char *lib = NULL, *install = NULL;
	snd_config_t *type = NULL, *args = NULL;
	snd_config_iterator_t i, next;
	int (*install_func)(snd_pcm_t *pcm, snd_config_t *args);
	void *h;
	if (snd_config_get_string(conf, &str) >= 0) {
		err = snd_config_search_alias(snd_config, "pcm_hook", str, &conf);
		if (err < 0) {
			SNDERR("unknown pcm_hook %s", str);
			return err;
		}
	}
	if (snd_config_get_type(conf) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("Invalid hook definition");
		return -EINVAL;
	}
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "type") == 0) {
			type = n;
			continue;
		}
		if (strcmp(id, "args") == 0) {
			args = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!type) {
		SNDERR("type is not defined");
		return -EINVAL;
	}
	err = snd_config_get_string(type, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(type));
		return err;
	}
	err = snd_config_search_alias(snd_config, "pcm_hook_type", str, &type);
	if (err >= 0) {
		if (snd_config_get_type(type) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for PCM type %s definition", str);
			return -EINVAL;
		}
		snd_config_for_each(i, next, type) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = snd_config_get_id(n);
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					return -EINVAL;
				}
				continue;
			}
			if (strcmp(id, "install") == 0) {
				err = snd_config_get_string(n, &install);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					return -EINVAL;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
			return -EINVAL;
		}
	}
	if (!install) {
		install = buf;
		snprintf(buf, sizeof(buf), "_snd_pcm_hook_%s_install", str);
	}
	if (!lib)
		lib = ALSA_LIB;
	if (args && snd_config_get_string(args, &str) >= 0) {
		err = snd_config_search_alias(snd_config, "hook_args", str, &args);
		if (err < 0) {
			SNDERR("unknown hook_args %s", str);
			return err;
		}
	}
	h = dlopen(lib, RTLD_NOW);
	if (!h) {
		SNDERR("Cannot open shared library %s", lib);
		return -ENOENT;
	}
	install_func = dlsym(h, install);
	if (!install_func) {
		SNDERR("symbol %s is not defined inside %s", install, lib);
		dlclose(h);
		return -ENXIO;
	}
	err = install_func(pcm, args);
	if (err < 0)
		return err;
	return 0;
}

int _snd_pcm_hooks_open(snd_pcm_t **pcmp, const char *name,
			snd_config_t *conf, 
			snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *slave = NULL, *sconf;
	snd_config_t *hooks = NULL;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "slave") == 0) {
			slave = n;
			continue;
		}
		if (strcmp(id, "hooks") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			hooks = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	err = snd_pcm_slave_conf(slave, &sconf, 0);
	if (err < 0)
		return err;
	err = snd_pcm_open_slave(&spcm, sconf, stream, mode);
	if (err < 0)
		return err;
	err = snd_pcm_hooks_open(pcmp, name, spcm, 1);
	if (err < 0) {
		snd_pcm_close(spcm);
		return err;
	}
	if (!hooks)
		return 0;
	snd_config_for_each(i, next, hooks) {
		snd_config_t *n = snd_config_iterator_entry(i);
		err = snd_pcm_hook_add_conf(*pcmp, n);
		if (err < 0) {
			snd_pcm_close(*pcmp);
			return err;
		}
	}
	return 0;
}

#endif

/**
 * \brief Get PCM handle for a PCM hook
 * \param hook PCM hook handle
 * \return PCM handle
 */
snd_pcm_t *snd_pcm_hook_get_pcm(snd_pcm_hook_t *hook)
{
	assert(hook);
	return hook->pcm;
}

/**
 * \brief Get callback function private data for a PCM hook
 * \param hook PCM hook handle
 * \return callback function private data
 */
void *snd_pcm_hook_get_private(snd_pcm_hook_t *hook)
{
	assert(hook);
	return hook->private_data;
}

/**
 * \brief Add a PCM hook at end of hooks chain
 * \param hookp Returned PCM hook handle
 * \param pcm PCM handle
 * \param type PCM hook type
 * \param func PCM hook callback function
 * \param private_data PCM hook private data
 * \return 0 on success otherwise a negative error code
 *
 * Warning: an hook callback function cannot remove an hook of the same type
 * different from itself
 */
int snd_pcm_hook_add(snd_pcm_hook_t **hookp, snd_pcm_t *pcm,
		     snd_pcm_hook_type_t type,
		     snd_pcm_hook_func_t func, void *private_data)
{
	snd_pcm_hook_t *h;
	snd_pcm_hooks_t *hooks;
	assert(hookp && func);
	assert(snd_pcm_type(pcm) == SND_PCM_TYPE_HOOKS);
	h = calloc(1, sizeof(*h));
	if (!h)
		return -ENOMEM;
	h->pcm = pcm;
	h->func = func;
	h->private_data = private_data;
	hooks = pcm->private_data;
	list_add_tail(&h->list, &hooks->hooks[type]);
	*hookp = h;
	return 0;
}

/**
 * \brief Remove a PCM hook
 * \param hook PCM hook handle
 * \return 0 on success otherwise a negative error code
 *
 * Warning: an hook callback cannot remove an hook of the same type
 * different from itself
 */
int snd_pcm_hook_remove(snd_pcm_hook_t *hook)
{
	assert(hook);
	list_del(&hook->list);
	free(hook);
	return 0;
}

typedef struct {
	unsigned int lock: 1;
	unsigned int preserve: 1;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_value_t *val;
	snd_ctl_elem_value_t *mask;
	snd_ctl_elem_value_t *old;
	struct list_head list;
} snd_pcm_hook_ctl_elem_t;

typedef struct {
	snd_ctl_t *ctl;
	struct list_head elems;
} snd_pcm_hook_ctl_elems_t;

static int free_elems(snd_pcm_hook_ctl_elems_t *h)
{
	int err;
	while (!list_empty(&h->elems)) {
		snd_pcm_hook_ctl_elem_t *elem = list_entry(h->elems.next, snd_pcm_hook_ctl_elem_t, list);
		snd_ctl_elem_id_free(elem->id);
		snd_ctl_elem_info_free(elem->info);
		snd_ctl_elem_value_free(elem->val);
		snd_ctl_elem_value_free(elem->mask);
		snd_ctl_elem_value_free(elem->old);
		list_del(&elem->list);
		free(elem);
	}
	err = snd_ctl_close(h->ctl);
	if (err < 0)
		return err;
	free(h);
	return 0;
}

static int snd_pcm_hook_ctl_elems_hw_params(snd_pcm_hook_t *hook)
{
	snd_pcm_hook_ctl_elems_t *h = snd_pcm_hook_get_private(hook);
	struct list_head *pos;
	int err;
	unsigned int k;
	list_for_each(pos, &h->elems) {
		snd_pcm_hook_ctl_elem_t *elem = list_entry(pos, snd_pcm_hook_ctl_elem_t, list);
		unsigned int count;
		snd_ctl_elem_type_t type;
		if (elem->lock) {
			err = snd_ctl_elem_lock(h->ctl, elem->id);
			if (err < 0) {
				SNDERR("Cannot lock ctl elem");
				return err;
			}
		}
		err = snd_ctl_elem_read(h->ctl, elem->old);
		if (err < 0) {
			SNDERR("Cannot read ctl elem");
			return err;
		}
		count = snd_ctl_elem_info_get_count(elem->info);
		type = snd_ctl_elem_info_get_type(elem->info);
		switch (type) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			for (k = 0; k < count; ++k) {
				int old, val, mask;
				old = snd_ctl_elem_value_get_boolean(elem->old, k);
				mask = snd_ctl_elem_value_get_boolean(elem->mask, k);
				old &= ~mask;
				if (old) {
					val = snd_ctl_elem_value_get_boolean(elem->val, k);
					val |= old;
					snd_ctl_elem_value_set_boolean(elem->val, k, val);
				}
			}
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			for (k = 0; k < count; ++k) {
				long old, val, mask;
				old = snd_ctl_elem_value_get_integer(elem->old, k);
				mask = snd_ctl_elem_value_get_integer(elem->mask, k);
				old &= ~mask;
				if (old) {
					val = snd_ctl_elem_value_get_integer(elem->val, k);
					val |= old;
					snd_ctl_elem_value_set_integer(elem->val, k, val);
				}
			}
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			for (k = 0; k < count; ++k) {
				unsigned int old, val, mask;
				old = snd_ctl_elem_value_get_enumerated(elem->old, k);
				mask = snd_ctl_elem_value_get_enumerated(elem->mask, k);
				old &= ~mask;
				if (old) {
					val = snd_ctl_elem_value_get_enumerated(elem->val, k);
					val |= old;
					snd_ctl_elem_value_set_enumerated(elem->val, k, val);
				}
			}
			break;
		case SND_CTL_ELEM_TYPE_IEC958:
			count = sizeof(snd_aes_iec958_t);
			/* Fall through */
		case SND_CTL_ELEM_TYPE_BYTES:
			for (k = 0; k < count; ++k) {
				unsigned char old, val, mask;
				old = snd_ctl_elem_value_get_byte(elem->old, k);
				mask = snd_ctl_elem_value_get_byte(elem->mask, k);
				old &= ~mask;
				if (old) {
					val = snd_ctl_elem_value_get_byte(elem->val, k);
					val |= old;
					snd_ctl_elem_value_set_byte(elem->val, k, val);
				}
			}
			break;
		default:
			assert(0);
			break;
		}
		err = snd_ctl_elem_write(h->ctl, elem->val);
		if (err < 0) {
			SNDERR("Cannot write ctl elem");
			return err;
		}
	}
	return 0;
}

static int snd_pcm_hook_ctl_elems_hw_free(snd_pcm_hook_t *hook)
{
	snd_pcm_hook_ctl_elems_t *h = snd_pcm_hook_get_private(hook);
	struct list_head *pos;
	int err;
	list_for_each(pos, &h->elems) {
		snd_pcm_hook_ctl_elem_t *elem = list_entry(pos, snd_pcm_hook_ctl_elem_t, list);
		if (elem->lock) {
			err = snd_ctl_elem_unlock(h->ctl, elem->id);
			if (err < 0) {
				SNDERR("Cannot unlock ctl elem");
				return err;
			}
		}
		if (elem->preserve) {
			err = snd_ctl_elem_write(h->ctl, elem->old);
			if (err < 0) {
				SNDERR("Cannot restore ctl elem");
				return err;
			}
		}
	}
	return 0;
}

static int snd_pcm_hook_ctl_elems_close(snd_pcm_hook_t *hook)
{
	snd_pcm_hook_ctl_elems_t *h = snd_pcm_hook_get_private(hook);
	return free_elems(h);
}

int snd_config_get_iface(snd_config_t *conf)
{
	long v;
	const char *str;
	int err;
	snd_ctl_elem_iface_t idx;
	err = snd_config_get_integer(conf, &v);
	if (err >= 0) {
		if (v < 0 || v > SND_CTL_ELEM_IFACE_LAST) {
		_invalid_value:
			SNDERR("Invalid value for %s", snd_config_get_id(conf));
			return -EINVAL;
		}
		return v;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(conf));
		return -EINVAL;
	}
	for (idx = 0; idx <= SND_CTL_ELEM_IFACE_LAST; idx++) {
		if (strcasecmp(snd_ctl_elem_iface_name(idx), str) == 0)
			return idx;
	}
	goto _invalid_value;
}

int snd_config_get_bool(snd_config_t *conf)
{
	long v;
	const char *str;
	int err;
	unsigned int k;
	static struct {
		const char *str;
		int val;
	} b[] = {
		{ "false", 0 },
		{ "true", 1 },
		{ "no", 0 },
		{ "yes", 1 },
		{ "off", 0 },
		{ "on", 1 },
	};
	err = snd_config_get_integer(conf, &v);
	if (err >= 0) {
		if (v < 0 || v > 1) {
		_invalid_value:
			SNDERR("Invalid value for %s", snd_config_get_id(conf));
			return -EINVAL;
		}
		return v;
	}
	err = snd_config_get_string(conf, &str);
	if (err < 0) {
		SNDERR("Invalid type for %s", snd_config_get_id(conf));
		return -EINVAL;
	}
	for (k = 0; k < sizeof(b) / sizeof(*b); k++) {
		if (strcasecmp(b[k].str, str) == 0)
			return b[k].val;
	}
	goto _invalid_value;
}

int snd_config_get_ctl_elem_enumerated(snd_config_t *n, snd_ctl_t *ctl,
				       snd_ctl_elem_info_t *info)
{
	const char *str;
	long val;
	unsigned int idx, items;
	switch (snd_enum_to_int(snd_config_get_type(n))) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_config_get_integer(n, &val);
		return val;
	case SND_CONFIG_TYPE_STRING:
		snd_config_get_string(n, &str);
		break;
	default:
		return -1;
	}
	items = snd_ctl_elem_info_get_items(info);
	for (idx = 0; idx < items; idx++) {
		int err;
		snd_ctl_elem_info_set_item(info, idx);
		err = snd_ctl_elem_info(ctl, info);
		if (err < 0) {
			SNDERR("Cannot obtain info for CTL elem");
			return err;
		}
		if (strcmp(str, snd_ctl_elem_info_get_item_name(info)) == 0)
			return idx;
	}
	return -1;
}

int snd_config_get_ctl_elem_value(snd_config_t *conf,
				  snd_ctl_t *ctl,
				  snd_ctl_elem_value_t *val,
				  snd_ctl_elem_value_t *mask,
				  snd_ctl_elem_info_t *info)
{
	int err;
	snd_config_iterator_t i, next;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_type_t type;
	unsigned int count;
	long v;
	long idx;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_value_get_id(val, id);
	count = snd_ctl_elem_info_get_count(info);
	type = snd_ctl_elem_info_get_type(info);
	if (count == 1) {
		switch (snd_enum_to_int(type)) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			v = snd_config_get_bool(conf);
			if (v >= 0) {
				snd_ctl_elem_value_set_boolean(val, 0, v);
				if (mask)
					snd_ctl_elem_value_set_boolean(mask, 0, 1);
				return 0;
			}
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			err = snd_config_get_integer(conf, &v);
			if (err == 0) {
				snd_ctl_elem_value_set_integer(val, 0, v);
				if (mask)
					snd_ctl_elem_value_set_integer(mask, 0, ~0L);
				return 0;
			}
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			v = snd_config_get_ctl_elem_enumerated(conf, ctl, info);
			if (v >= 0) {
				snd_ctl_elem_value_set_enumerated(val, 0, v);
				if (mask)
					snd_ctl_elem_value_set_enumerated(mask, 0, ~0);
				return 0;
			}
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
		case SND_CTL_ELEM_TYPE_IEC958:
			break;
		default:
			SNDERR("Unknow control type: %d", snd_enum_to_int(type));
			return -EINVAL;
		}
	}
	switch (snd_enum_to_int(type)) {
	case SND_CTL_ELEM_TYPE_IEC958:
		count = sizeof(snd_aes_iec958_t);
		/* Fall through */
	case SND_CTL_ELEM_TYPE_BYTES:
	{
		const char *buf;
		err = snd_config_get_string(conf, &buf);
		if (err >= 0) {
			int c1 = 0;
			unsigned int len = strlen(buf);
			unsigned int idx = 0;
			if (len % 2 != 0 || len > count * 2) {
			_bad_content:
				SNDERR("bad value content\n");
				return -EINVAL;
			}
			while (*buf) {
				int c = *buf++;
				if (c >= '0' && c <= '9')
					c -= '0';
				else if (c >= 'a' && c <= 'f')
					c = c - 'a' + 10;
				else if (c >= 'A' && c <= 'F')
					c = c - 'A' + 10;
				else {
					goto _bad_content;
				}
				if (idx % 2 == 1) {
					snd_ctl_elem_value_set_byte(val, idx / 2, c1 << 4 | c);
					if (mask)
						snd_ctl_elem_value_set_byte(mask, idx / 2, 0xff);
				} else
					c1 = c;
				idx++;
			}
			return 0;
		}
	}
	default:
		break;
	}
	if (snd_config_get_type(conf) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("bad value type");
		return -EINVAL;
	}

	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		err = safe_strtol(snd_config_get_id(n), &idx);
		if (err < 0 || idx < 0 || (unsigned int) idx >= count) {
			SNDERR("bad value index");
			return -EINVAL;
		}
		switch (snd_enum_to_int(type)) {
		case SND_CTL_ELEM_TYPE_BOOLEAN:
			v = snd_config_get_bool(n);
			if (v < 0)
				goto _bad_content;
			snd_ctl_elem_value_set_boolean(val, idx, v);
			if (mask)
				snd_ctl_elem_value_set_boolean(mask, idx, 1);
			break;
		case SND_CTL_ELEM_TYPE_INTEGER:
			err = snd_config_get_integer(n, &v);
			if (err < 0)
				goto _bad_content;
			snd_ctl_elem_value_set_integer(val, idx, v);
			if (mask)
				snd_ctl_elem_value_set_integer(mask, idx, ~0L);
			break;
		case SND_CTL_ELEM_TYPE_ENUMERATED:
			v = snd_config_get_ctl_elem_enumerated(n, ctl, info);
			if (v < 0)
				goto _bad_content;
			snd_ctl_elem_value_set_enumerated(val, idx, v);
			if (mask)
				snd_ctl_elem_value_set_enumerated(mask, idx, ~0);
			break;
		case SND_CTL_ELEM_TYPE_BYTES:
		case SND_CTL_ELEM_TYPE_IEC958:
			err = snd_config_get_integer(n, &v);
			if (err < 0 || v < 0 || v > 255)
				goto _bad_content;
			snd_ctl_elem_value_set_byte(val, idx, v);
			if (mask)
				snd_ctl_elem_value_set_byte(mask, idx, 0xff);
			break;
		default:
			break;
		}
	}
	return 0;
}

static int add_elem(snd_pcm_hook_ctl_elems_t *h, snd_config_t *conf, snd_pcm_info_t *info)
{
	snd_config_iterator_t i, next;
	int iface = SND_CTL_ELEM_IFACE_PCM;
	const char *name = NULL;
	long index = 0;
	long device = -1;
	long subdevice = -1;
	int lock = 0;
	int preserve = 0;
	snd_config_t *value = NULL, *mask = NULL;
	snd_pcm_hook_ctl_elem_t *elem;
	int err;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id = snd_config_get_id(n);
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "iface") == 0) {
			iface = snd_config_get_iface(n);
			if (iface < 0)
				return iface;
			continue;
		}
		if (strcmp(id, "name") == 0) {
			err = snd_config_get_string(n, &name);
			if (err < 0) {
			_invalid_type:
				SNDERR("Invalid type for %s", id);
				return err;
			}
			continue;
		}
		if (strcmp(id, "index") == 0) {
			err = snd_config_get_integer(n, &index);
			if (err < 0)
				goto _invalid_type;
			continue;
		}
		if (strcmp(id, "device") == 0) {
			err = snd_config_get_integer(n, &device);
			if (err < 0)
				goto _invalid_type;
			continue;
		}
		if (strcmp(id, "subdevice") == 0) {
			err = snd_config_get_integer(n, &subdevice);
			if (err < 0)
				goto _invalid_type;
			continue;
		}
		if (strcmp(id, "lock") == 0) {
			lock = snd_config_get_bool(n);
			if (lock < 0) {
				err = lock;
			_invalid_value:
				SNDERR("Invalid value for %s", id);
				return err;
			}
			continue;
		}
		if (strcmp(id, "preserve") == 0) {
			preserve = snd_config_get_bool(n);
			if (preserve < 0) {
				err = preserve;
				goto _invalid_value;
			}
			continue;
		}
		if (strcmp(id, "value") == 0) {
			value = n;
			continue;
		}
		if (strcmp(id, "mask") == 0) {
			mask = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!name) {
		SNDERR("Missing control name");
		return -EINVAL;
	}
	if (!value) {
		SNDERR("Missing control value");
		return -EINVAL;
	}
	if (device < 0) {
		if (iface == SND_CTL_ELEM_IFACE_PCM)
			device = snd_pcm_info_get_device(info);
		else
			device = 0;
	}
	if (subdevice < 0) {
		if (iface == SND_CTL_ELEM_IFACE_PCM)
			subdevice = snd_pcm_info_get_subdevice(info);
		else
			subdevice = 0;
	}
	elem = calloc(1, sizeof(*elem));
	if (!elem)
		return -ENOMEM;
	err = snd_ctl_elem_id_malloc(&elem->id);
	if (err < 0)
		goto _err;
	err = snd_ctl_elem_info_malloc(&elem->info);
	if (err < 0)
		goto _err;
	err = snd_ctl_elem_value_malloc(&elem->val);
	if (err < 0)
		goto _err;
	err = snd_ctl_elem_value_malloc(&elem->mask);
	if (err < 0)
		goto _err;
	err = snd_ctl_elem_value_malloc(&elem->old);
	if (err < 0)
		goto _err;
	elem->lock = lock;
	elem->preserve = preserve;
	snd_ctl_elem_id_set_interface(elem->id, iface);
	snd_ctl_elem_id_set_name(elem->id, name);
	snd_ctl_elem_id_set_index(elem->id, index);
	snd_ctl_elem_id_set_device(elem->id, device);
	snd_ctl_elem_id_set_subdevice(elem->id, subdevice);
	snd_ctl_elem_info_set_id(elem->info, elem->id);
	err = snd_ctl_elem_info(h->ctl, elem->info);
	if (err < 0) {
		SNDERR("Cannot obtain info for CTL elem");
		goto _err;
	}
	snd_ctl_elem_value_set_id(elem->val, elem->id);
	snd_ctl_elem_value_set_id(elem->old, elem->id);
	if (mask) {
		err = snd_config_get_ctl_elem_value(value, h->ctl, elem->val, NULL, elem->info);
		if (err < 0)
			goto _err;
		err = snd_config_get_ctl_elem_value(mask, h->ctl, elem->mask, NULL, elem->info);
		if (err < 0)
			goto _err;
	} else {
		err = snd_config_get_ctl_elem_value(value, h->ctl, elem->val, elem->mask, elem->info);
		if (err < 0)
			goto _err;
	}
		
	err = snd_config_get_ctl_elem_value(value, h->ctl, elem->val, elem->mask, elem->info);
	if (err < 0)
		goto _err;
	list_add_tail(&elem->list, &h->elems);
	return 0;

 _err:
	if (elem->id)
		snd_ctl_elem_id_free(elem->id);
	if (elem->info)
		snd_ctl_elem_info_free(elem->info);
	if (elem->val)
		snd_ctl_elem_value_free(elem->val);
	if (elem->mask)
		snd_ctl_elem_value_free(elem->mask);
	if (elem->old)
		snd_ctl_elem_value_free(elem->old);
	free(elem);
	return err;
}

int _snd_pcm_hook_ctl_elems_install(snd_pcm_t *pcm, snd_config_t *conf)
{
	int err;
	int card;
	snd_pcm_info_t *info;
	char ctl_name[16];
	snd_ctl_t *ctl;
	snd_pcm_hook_ctl_elems_t *h;
	snd_config_iterator_t i, next;
	snd_pcm_hook_t *h_hw_params = NULL, *h_hw_free = NULL, *h_close = NULL;
	assert(conf);
	assert(snd_config_get_type(conf) == SND_CONFIG_TYPE_COMPOUND);
	snd_pcm_info_alloca(&info);
	err = snd_pcm_info(pcm, info);
	if (err < 0)
		return err;
	card = snd_pcm_info_get_card(info);
	if (card < 0) {
		SNDERR("No card for this PCM");
		return -EINVAL;
	}
	sprintf(ctl_name, "hw:%d", card);
	err = snd_ctl_open(&ctl, ctl_name, 0);
	if (err < 0) {
		SNDERR("Cannot open CTL %s", ctl_name);
		return err;
	}
	h = calloc(1, sizeof(*h));
	if (!h) {
		snd_ctl_close(ctl);
		return -ENOMEM;
	}
	h->ctl = ctl;
	INIT_LIST_HEAD(&h->elems);
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		err = add_elem(h, n, info);
		if (err < 0) {
			free_elems(h);
			return err;
		}
	}
	err = snd_pcm_hook_add(&h_hw_params, pcm, SND_PCM_HOOK_TYPE_HW_PARAMS,
			       snd_pcm_hook_ctl_elems_hw_params, h);
	if (err < 0)
		goto _err;
	err = snd_pcm_hook_add(&h_hw_free, pcm, SND_PCM_HOOK_TYPE_HW_FREE,
			       snd_pcm_hook_ctl_elems_hw_free, h);
	if (err < 0)
		goto _err;
	err = snd_pcm_hook_add(&h_close, pcm, SND_PCM_HOOK_TYPE_CLOSE,
			       snd_pcm_hook_ctl_elems_close, h);
	if (err < 0)
		goto _err;
	return 0;
 _err:
	if (h_hw_params)
		snd_pcm_hook_remove(h_hw_params);
	if (h_hw_free)
		snd_pcm_hook_remove(h_hw_free);
	if (h_close)
		snd_pcm_hook_remove(h_close);
	free_elems(h);
	return err;
}

