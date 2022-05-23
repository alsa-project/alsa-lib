/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  Support for the verb/device/modifier core logic and API,
 *  command line tool and file parser was kindly sponsored by
 *  Texas Instruments Inc.
 *  Support for multiple active modifiers and devices,
 *  transition sequences, multiple client access and user defined use
 *  cases was kindly sponsored by Wolfson Microelectronics PLC.
 *
 *  Copyright (C) 2020 Red Hat Inc.
 *  Authors: Jaroslav Kysela <perex@perex.cz>
 */

#include "ucm_local.h"

static int get_string(snd_config_t *compound, const char *key, const char **str)
{
	snd_config_t *node;
	int err;

	err = snd_config_search(compound, key, &node);
	if (err < 0)
		return err;
	return snd_config_get_string(node, str);
}

static int include_eval_one(snd_use_case_mgr_t *uc_mgr,
			    snd_config_t *inc,
			    snd_config_t **result,
			    snd_config_t **before,
			    snd_config_t **after)
{
	const char *file;
	char *s;
	int err;

	*result = NULL;

	if (snd_config_get_type(inc) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for Include.1");
		return -EINVAL;
	}

	err = get_string(inc, "File", &file);
	if (err < 0) {
		uc_error("file expected (Include)");
		return -EINVAL;
	}

	err = snd_config_search(inc, "Before", before);
	if (err < 0 && err != -ENOENT) {
		uc_error("before block identifier error");
		return -EINVAL;
	}

	err = snd_config_search(inc, "After", after);
	if (err < 0 && err != -ENOENT) {
		uc_error("before block identifier error");
		return -EINVAL;
	}

	err = uc_mgr_get_substituted_value(uc_mgr, &s, file);
	if (err < 0)
		return err;
	err = uc_mgr_config_load_file(uc_mgr, s, result);
	free(s);
	return err;
}

#if 0
static void config_dump(snd_config_t *cfg)
{
	snd_output_t *out;
	snd_output_stdio_attach(&out, stderr, 0);
	snd_output_printf(out, "-----\n");
	snd_config_save(cfg, out);
	snd_output_close(out);
}
#endif

static int find_position_node(snd_use_case_mgr_t *uc_mgr,
			      snd_config_t **res, snd_config_t *dst,
			      const char *id, snd_config_t *pos)
{
	const char *s;
	char *s1;
	int err;

	err = get_string(pos, id, &s);
	if (err < 0 && err != -ENOENT)
		return err;
	if (err == 0) {
		err = uc_mgr_get_substituted_value(uc_mgr, &s1, s);
		if (err < 0)
			return err;
		err = snd_config_search(dst, s1, res);
		free(s1);
		if (err < 0 && err != -ENOENT)
			return err;
	}
	return 0;
}

static int merge_it(snd_config_t *dst, snd_config_t *n, snd_config_t **_dn)
{
	snd_config_t *dn;
	const char *id;
	int err;

	err = snd_config_get_id(n, &id);
	if (err < 0)
		return err;
	err = snd_config_search(dst, id, &dn);
	if (err < 0)
		return err;
	err = snd_config_merge(dn, n, 0); /* merge / append mode */
	if (err < 0)
		snd_config_delete(n);
	else
		*_dn = dn;
	return err;
}

static int compound_merge(snd_use_case_mgr_t *uc_mgr, const char *id,
			  snd_config_t *dst, snd_config_t *src,
			  snd_config_t *before, snd_config_t *after)
{
	snd_config_iterator_t i, next;
	snd_config_t *n, *_before = NULL, *_after = NULL;
	char tmpid[32];
	int err, array, idx;

	if (snd_config_get_type(src) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for the merged block");
		return -EINVAL;
	}

	if (before) {
		err = find_position_node(uc_mgr, &_before, dst, id, before);
		if (err < 0)
			return err;
	}
	if (after) {
		err = find_position_node(uc_mgr, &_after, dst, id, after);
		if (err < 0)
			return err;
	}

	/* direct merge? */
	if (!_before && !_after)
		return snd_config_merge(dst, src, 0);	/* merge / append mode */

	if (_before && _after) {
		uc_error("defined both before and after identifiers in the If or Include block");
		return -EINVAL;
	}

	array = snd_config_is_array(dst);
	if (array < 0) {
		uc_error("destination configuration node is not a compound");
		return array;
	}
	if (array && snd_config_is_array(src) <= 0) {
		uc_error("source configuration node is not an array");
		return -EINVAL;
	}

	idx = 0;

	/* for array, use a temporary non-clashing identifier */
	if (array > 0) {
		snd_config_for_each(i, next, dst) {
			n = snd_config_iterator_entry(i);
			snprintf(tmpid, sizeof(tmpid), "_tmp_%d", idx++);
			err = snd_config_set_id(n, tmpid);
			if (err < 0)
				return err;
		}
	}

	snd_config_for_each(i, next, src) {
		n = snd_config_iterator_entry(i);
		err = snd_config_remove(n);
		if (err < 0)
			return err;
		/* for array, use a temporary non-clashing identifier */
		if (array > 0) {
			snprintf(tmpid, sizeof(tmpid), "_tmp_%d", idx++);
			err = snd_config_set_id(n, tmpid);
			if (err < 0)
				return err;
		}
		if (_before) {
			err = snd_config_add_before(_before, n);
			if (err == -EEXIST)
				err = merge_it(dst, n, &n);
			if (err < 0)
				return err;
			_before = NULL;
			_after = n;
		} else if (_after) {
			err = snd_config_add_after(_after, n);
			if (err == -EEXIST)
				err = merge_it(dst, n, &n);
			if (err < 0)
				return err;
			_after = n;
		}
	}

	/* set new indexes for the final array */
	if (array > 0) {
		idx = 0;
		snd_config_for_each(i, next, dst) {
			n = snd_config_iterator_entry(i);
			snprintf(tmpid, sizeof(tmpid), "%d", idx++);
			err = snd_config_set_id(n, tmpid);
			if (err < 0)
				return err;
		}
	}

	snd_config_delete(src);
	return 0;
}

int uc_mgr_config_tree_merge(snd_use_case_mgr_t *uc_mgr,
			     snd_config_t *parent, snd_config_t *new_ctx,
			     snd_config_t *before, snd_config_t *after)
{
	snd_config_iterator_t i, next;
	snd_config_t *n, *parent2;
	const char *id;
	int err;

	err = uc_mgr_substitute_tree(uc_mgr, new_ctx);
	if (err < 0)
		return err;

	snd_config_for_each(i, next, new_ctx) {
		n = snd_config_iterator_entry(i);
		err = snd_config_remove(n);
		if (err < 0)
			return err;
		err = snd_config_get_id(n, &id);
		if (err < 0) {
__add:
			err = snd_config_add(parent, n);
			if (err < 0)
				return err;
		} else {
			err = snd_config_search(parent, id, &parent2);
			if (err == -ENOENT)
				goto __add;
			err = compound_merge(uc_mgr, id, parent2, n, before, after);
			if (err < 0) {
				snd_config_delete(n);
				return err;
			}
		}
	}
	return 0;
}

/*
 * put back the included configuration to the parent
 */
int uc_mgr_evaluate_include(snd_use_case_mgr_t *uc_mgr,
			      snd_config_t *parent,
			      snd_config_t *inc)
{
	snd_config_iterator_t i, next;
	snd_config_t *a, *n, *before, *after;
	int err;

	if (uc_mgr->conf_format < 3) {
		uc_error("in-place include is supported in v3+ syntax");
		return -EINVAL;
	}

	if (snd_config_get_type(inc) != SND_CONFIG_TYPE_COMPOUND) {
		uc_error("compound type expected for Include");
		return -EINVAL;
	}

	snd_config_for_each(i, next, inc) {
		n = snd_config_iterator_entry(i);
		before = after = NULL;
		err = include_eval_one(uc_mgr, n, &a, &before, &after);
		if (err < 0)
			return err;
		if (a == NULL)
			continue;
		err = uc_mgr_evaluate_inplace(uc_mgr, a);
		if (err < 0) {
			snd_config_delete(a);
			return err;
		}
		err = uc_mgr_config_tree_merge(uc_mgr, parent, a, before, after);
		snd_config_delete(a);
		if (err < 0)
			return err;
	}
	return 0;
}
