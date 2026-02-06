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
 *  Copyright (C) 2026 Red Hat Inc.
 *  Authors: Jaroslav Kysela <perex@perex.cz>
 */

#include "ucm_local.h"

/*
 * get_string helper
 */
static int get_string(snd_config_t *compound, const char *key, const char **str)
{
	snd_config_t *node;
	int err;

	err = snd_config_search(compound, key, &node);
	if (err < 0)
		return err;
	return snd_config_get_string(node, str);
}

/*
 * get_integer helper
 */
static int get_integer(snd_config_t *compound, const char *key, long long *val)
{
	snd_config_type_t t;
	snd_config_t *node;
	const char *str;
	int err;

	err = snd_config_search(compound, key, &node);
	if (err < 0)
		return err;
	t = snd_config_get_type(node);
	if (t == SND_CONFIG_TYPE_INTEGER) {
		long i;
		err = snd_config_get_integer(node, &i);
		if (err >= 0)
			*val = i;
	} else if (t == SND_CONFIG_TYPE_INTEGER64) {
		err = snd_config_get_integer64(node, val);
	} else {
		err = snd_config_get_string(node, &str);
		if (err < 0)
			return err;
		err = safe_strtoll(str, val);
	}
	if (err < 0)
		return -EINVAL;

	return 0;
}

/*
 * Repeat pattern iterator
 */
struct repeat_iterator {
	const char *var_name;

	union {
		struct {
			long long current;
			long long last;
			long long step;
			int iteration;
			char value_buf[32];
		} integer;

		struct {
			snd_config_iterator_t pos;
			snd_config_iterator_t end;
			snd_config_t *array;
			char *value_str;
		} array;
	} u;

	int (*init)(struct repeat_iterator *it, snd_config_t *pattern);
	int (*next)(struct repeat_iterator *it, const char **value);
	void (*done)(struct repeat_iterator *it);
};

/*
 * Integer pattern iterator - initialization
 */
static int repeat_integer_init(struct repeat_iterator *it, snd_config_t *pattern)
{
	long long first;
	int err;

	err = get_integer(pattern, "First", &first);
	if (err < 0) {
		snd_error(UCM, "Repeat.Pattern.First is required for Integer type");
		return -EINVAL;
	}

	err = get_integer(pattern, "Last", &it->u.integer.last);
	if (err < 0) {
		snd_error(UCM, "Repeat.Pattern.Last is required for Integer type");
		return -EINVAL;
	}

	err = get_integer(pattern, "Step", &it->u.integer.step);
	if (err == -ENOENT) {
		it->u.integer.step = 1;
	} else if (err < 0) {
		snd_error(UCM, "Repeat.Pattern.Step parse error");
		return -EINVAL;
	}

	if (it->u.integer.step == 0) {
		snd_error(UCM, "Repeat.Pattern.Step cannot be zero");
		return -EINVAL;
	}

	it->u.integer.current = first;
	it->u.integer.iteration = 0;
	return 0;
}

/*
 * Integer pattern iterator - get next value
 * Returns: 1 if value available, 0 if end of iteration, negative on error
 */
static int repeat_integer_next(struct repeat_iterator *it, const char **value)
{
	const int max_iterations = 10000;
	int has_value;

	if (it->u.integer.iteration++ > max_iterations) {
		snd_error(UCM, "Repeat iteration limit exceeded");
		return -EINVAL;
	}

	if (it->u.integer.step > 0)
		has_value = (it->u.integer.current <= it->u.integer.last);
	else
		has_value = (it->u.integer.current >= it->u.integer.last);

	if (!has_value)
		return 0;

	snprintf(it->u.integer.value_buf, sizeof(it->u.integer.value_buf), "%lld", it->u.integer.current);
	*value = it->u.integer.value_buf;

	it->u.integer.current += it->u.integer.step;
	return 1;
}

/*
 * Array pattern iterator - initialization
 */
static int repeat_array_init(struct repeat_iterator *it, snd_config_t *pattern)
{
	int err;

	err = snd_config_search(pattern, "Array", &it->u.array.array);
	if (err < 0) {
		snd_error(UCM, "Repeat.Pattern.Array is required for Array type");
		return -EINVAL;
	}

	if (snd_config_get_type(it->u.array.array) != SND_CONFIG_TYPE_COMPOUND) {
		snd_error(UCM, "Repeat.Pattern.Array must be a compound");
		return -EINVAL;
	}

	it->u.array.pos = snd_config_iterator_first(it->u.array.array);
	it->u.array.end = snd_config_iterator_end(it->u.array.array);
	it->u.array.value_str = NULL;
	return 0;
}

/*
 * Array pattern iterator - get next value
 * Returns: 1 if value available, 0 if end of iteration, negative on error
 */
static int repeat_array_next(struct repeat_iterator *it, const char **value)
{
	snd_config_t *n;
	int err;

	/* Free previous value string */
	free(it->u.array.value_str);
	it->u.array.value_str = NULL;

	if (it->u.array.pos == it->u.array.end)
		return 0;

	n = snd_config_iterator_entry(it->u.array.pos);
	it->u.array.pos = snd_config_iterator_next(it->u.array.pos);

	err = snd_config_get_ascii(n, &it->u.array.value_str);
	if (err < 0) {
		snd_error(UCM, "Repeat.Pattern.Array element conversion error");
		return -EINVAL;
	}

	*value = it->u.array.value_str;
	return 1;
}

/*
 * Array pattern iterator - cleanup
 */
static void repeat_array_done(struct repeat_iterator *it)
{
	free(it->u.array.value_str);
	it->u.array.value_str = NULL;
}

/*
 * Evaluate repeat pattern using iterator
 */
static int evaluate_repeat_pattern(snd_use_case_mgr_t *uc_mgr,
				    snd_config_t *cfg,
				    snd_config_t *pattern,
				    snd_config_t *apply,
				    struct repeat_iterator *it)
{
	snd_config_t *apply_copy;
	const char *value;
	int err, ret;

	err = it->init(it, pattern);
	if (err < 0)
		return err;

	while ((ret = it->next(it, &value)) > 0) {
		err = uc_mgr_set_variable(uc_mgr, it->var_name, value);
		if (err < 0)
			goto __error;

		err = snd_config_copy(&apply_copy, apply);
		if (err < 0)
			goto __var_error;

		err = uc_mgr_evaluate_inplace(uc_mgr, apply_copy);
		if (err < 0)
			goto __copy_error;

		err = uc_mgr_config_tree_merge(uc_mgr, cfg, apply_copy, NULL, NULL);
		snd_config_delete(apply_copy);
		if (err < 0)
			goto __var_error;
	}

	if (ret < 0) {
		err = ret;
		goto __var_error;
	}

	uc_mgr_delete_variable(uc_mgr, it->var_name);

	if (it->done)
		it->done(it);

	return 0;

__copy_error:
	snd_config_delete(apply_copy);
__var_error:
	uc_mgr_delete_variable(uc_mgr, it->var_name);
__error:
	if (it->done)
		it->done(it);
	return err;
}

/*
 * Evaluate repeat (in-place)
 */
int uc_mgr_evaluate_repeat(snd_use_case_mgr_t *uc_mgr, snd_config_t *cfg)
{
	snd_config_iterator_t i, next;
	snd_config_t *repeat_blocks, *n, *pattern = NULL, *pattern_cfg = NULL;
	const char *id;
	int err;

	err = snd_config_search(cfg, "Repeat", &repeat_blocks);
	if (err == -ENOENT)
		return 1;
	if (err < 0)
		return err;

	if (uc_mgr->conf_format < 9) {
		snd_error(UCM, "Repeat is supported in v9+ syntax");
		err = -EINVAL;
		goto __error;
	}

	if (snd_config_get_type(repeat_blocks) != SND_CONFIG_TYPE_COMPOUND) {
		snd_error(UCM, "Repeat must be a compound");
		err = -EINVAL;
		goto __error;
	}

	snd_config_for_each(i, next, repeat_blocks) {
		snd_config_t *apply;
		struct repeat_iterator it;
		const char *var_name, *type_str;

		n = snd_config_iterator_entry(i);

		if (snd_config_get_id(n, &id) < 0)
			continue;

		err = snd_config_search(n, "Pattern", &pattern);
		if (err < 0) {
			snd_error(UCM, "Repeat.%s.Pattern is required", id);
			goto __error;
		}

		if (snd_config_get_type(pattern) == SND_CONFIG_TYPE_STRING) {
			const char *pattern_str;
			char *pattern_subst = NULL;

			err = snd_config_get_string(pattern, &pattern_str);
			if (err < 0)
				goto __error;

			err = uc_mgr_get_substituted_value(uc_mgr, &pattern_subst, pattern_str);
			if (err < 0)
				goto __error;

			err = snd_config_load_string(&pattern_cfg, pattern_subst, 0);
			free(pattern_subst);
			if (err < 0) {
				snd_error(UCM, "Repeat.%s.Pattern string parse error", id);
				goto __error;
			}
		} else {
			pattern_cfg = pattern;
		}

		err = get_string(pattern_cfg, "Variable", &var_name);
		if (err < 0) {
			snd_error(UCM, "Repeat.%s.Pattern.Variable is required", id);
			goto __pattern_error;
		}

		err = get_string(pattern_cfg, "Type", &type_str);
		if (err < 0) {
			snd_error(UCM, "Repeat.%s.Pattern.Type is required", id);
			goto __pattern_error;
		}

		err = snd_config_search(n, "Apply", &apply);
		if (err < 0) {
			snd_error(UCM, "Repeat.%s.Apply is required", id);
			goto __pattern_error;
		}

		memset(&it, 0, sizeof(it));
		it.var_name = var_name;

		if (strcmp(type_str, "Integer") == 0) {
			it.init = repeat_integer_init;
			it.next = repeat_integer_next;
			it.done = NULL;
		} else if (strcmp(type_str, "Array") == 0) {
			it.init = repeat_array_init;
			it.next = repeat_array_next;
			it.done = repeat_array_done;
		} else {
			snd_error(UCM, "Repeat.%s.Pattern.Type must be 'Integer' or 'Array'", id);
			err = -EINVAL;
			goto __pattern_error;
		}

		err = evaluate_repeat_pattern(uc_mgr, cfg, pattern_cfg, apply, &it);
		if (err < 0)
			goto __pattern_error;
		if (pattern_cfg != pattern) {
			snd_config_delete(pattern_cfg);
			pattern_cfg = NULL;
		}
	}

	err = 0;
__pattern_error:
	if (pattern_cfg && pattern_cfg != pattern)
		snd_config_delete(pattern_cfg);
__error:
	snd_config_delete(repeat_blocks);
	return err;
}
