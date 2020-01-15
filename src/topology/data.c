/*
  Copyright(c) 2014-2015 Intel Corporation
  All rights reserved.

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  Authors: Mengdong Lin <mengdong.lin@intel.com>
           Yao Jin <yao.jin@intel.com>
           Liam Girdwood <liam.r.girdwood@linux.intel.com>
*/

#include "list.h"
#include "tplg_local.h"
#include <ctype.h>

#define UUID_FORMAT "\
%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:\
%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x"

/* Get private data buffer of an element */
struct snd_soc_tplg_private *get_priv_data(struct tplg_elem *elem)
{
	struct snd_soc_tplg_private *priv = NULL;

	switch (elem->type) {
	case SND_TPLG_TYPE_MANIFEST:
		priv = &elem->manifest->priv;
		break;

	case SND_TPLG_TYPE_MIXER:
		priv = &elem->mixer_ctrl->priv;
		break;

	case SND_TPLG_TYPE_ENUM:
		priv = &elem->enum_ctrl->priv;
		break;

	case SND_TPLG_TYPE_BYTES:
		priv = &elem->bytes_ext->priv;
		break;

	case SND_TPLG_TYPE_DAPM_WIDGET:
		priv = &elem->widget->priv;
		break;

	case SND_TPLG_TYPE_DAI:
		priv = &elem->dai->priv;
		break;
	case SND_TPLG_TYPE_BE:
		priv = &elem->link->priv;
		break;
	case SND_TPLG_TYPE_PCM:
		priv = &elem->pcm->priv;
		break;
	default:
		SNDERR("'%s': no support for private data for type %d",
			elem->id, elem->type);
	}

	return priv;
}

/* Parse references for the element, either a single data section
 * or a list of data sections.
 */
int tplg_parse_refs(snd_config_t *cfg, struct tplg_elem *elem,
		    unsigned int type)
{
	snd_config_type_t cfg_type;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *val = NULL;
	int err, count;

	cfg_type = snd_config_get_type(cfg);

	/* refer to a single data section */
	if (cfg_type == SND_CONFIG_TYPE_STRING) {
		if (snd_config_get_string(cfg, &val) < 0)
			return -EINVAL;

		tplg_dbg("\tref data: %s", val);
		err = tplg_ref_add(elem, type, val);
		if (err < 0)
			return err;
		return 1;
	}

	if (cfg_type != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("compound type expected for %s", elem->id);
		return -EINVAL;
	}

	/* refer to a list of data sections */
	count = 0;
	snd_config_for_each(i, next, cfg) {
		const char *val;

		n = snd_config_iterator_entry(i);
		if (snd_config_get_string(n, &val) < 0)
			continue;

		tplg_dbg("\tref data: %s", val);
		err = tplg_ref_add(elem, type, val);
		if (err < 0)
			return err;
		count++;
	}

	return count;
}

/* save references */
int tplg_save_refs(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		   struct tplg_elem *elem, unsigned int type,
		   const char *id, char **dst, const char *pfx)
{
	struct tplg_ref *ref, *last;
	struct list_head *pos;
	int err, count;

	count = 0;
	last = NULL;
	list_for_each(pos, &elem->ref_list) {
		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->type == type) {
			last = ref;
			count++;
		}
	}

	if (count == 0)
		return 0;

	if (count == 1)
		return tplg_save_printf(dst, pfx, "%s '%s'\n", id, last->id);

	err = tplg_save_printf(dst, pfx, "%s [\n", id);
	if (err < 0)
		return err;
	list_for_each(pos, &elem->ref_list) {
		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->type == type) {
			err = tplg_save_printf(dst, pfx, "\t'%s'\n", ref->id);
			if (err < 0)
				return err;
		}
	}

	return tplg_save_printf(dst, pfx, "]\n");
}

/* Get Private data from a file. */
static int tplg_parse_data_file(snd_config_t *cfg, struct tplg_elem *elem)
{
	struct snd_soc_tplg_private *priv = NULL;
	const char *value = NULL;
	char filename[PATH_MAX];
	char *env = getenv(ALSA_CONFIG_TPLG_VAR);
	FILE *fp;
	size_t size, bytes_read;
	int ret = 0;

	tplg_dbg("data DataFile: %s", elem->id);

	if (snd_config_get_string(cfg, &value) < 0)
		return -EINVAL;

	/* prepend alsa config directory to path */
	if (env)
		snprintf(filename, sizeof(filename), "%s/%s", env, value);
	else
		snprintf(filename, sizeof(filename), "%s/topology/%s",
			 snd_config_topdir(), value);

	fp = fopen(filename, "r");
	if (fp == NULL) {
		SNDERR("invalid data file path '%s'", filename);
		return -errno;
	}

	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0L, SEEK_SET);
	if (size <= 0) {
		SNDERR("invalid data file size %zu", size);
		ret = -EINVAL;
		goto err;
	}
	if (size > TPLG_MAX_PRIV_SIZE) {
		SNDERR("data file too big %zu", size);
		ret = -EINVAL;
		goto err;
	}

	priv = calloc(1, sizeof(*priv) + size);
	if (!priv) {
		ret = -ENOMEM;
		goto err;
	}

	bytes_read = fread(&priv->data, 1, size, fp);
	if (bytes_read != size) {
		ret = -errno;
		goto err;
	}

	elem->data = priv;
	priv->size = size;
	elem->size = sizeof(*priv) + size;

	if (fclose(fp) == EOF) {
		SNDERR("Cannot close data file.");
		return -errno;
	}
	return 0;

err:
	fclose(fp);
	if (priv)
		free(priv);
	return ret;
}

static void dump_priv_data(struct tplg_elem *elem ATTRIBUTE_UNUSED)
{
#ifdef TPLG_DEBUG
	struct snd_soc_tplg_private *priv = elem->data;
	unsigned char *p = (unsigned char *)priv->data;
	char buf[128], buf2[8];
	unsigned int i;

	tplg_dbg(" elem size = %d, priv data size = %d",
		elem->size, priv->size);

	buf[0] = '\0';
	for (i = 0; i < priv->size; i++) {
		if (i > 0 && (i % 16) == 0) {
			tplg_dbg("%s", buf);
			buf[0] = '\0';
		}

		snprintf(buf2, sizeof(buf2), " %02x", *p++);
		strcat(buf, buf2);
	}

	if (buf[0])
		tplg_dbg("%s", buf);
#endif
}

static inline int check_nibble(unsigned char c)
{
	return (c >= '0' && c <= '9') ||
	       (c >= 'a' && c <= 'f') ||
	       (c >= 'A' && c <= 'F');
}

/* get number of hex value elements in CSV list */
static int get_hex_num(const char *str)
{
	int delims, values, len = strlen(str);
	const char *s, *end = str + len;

	/* check "aa:bb:00" syntax */
	s = str;
	delims = values = 0;
	while (s < end) {
		/* skip white space */
		if (isspace(*s)) {
			s++;
			continue;
		}
		/* find delimeters */
		if (*s == ':') {
			delims++;
			s++;
			continue;
		}
		/* check 00 hexadecimal value */
		if (s + 1 <= end) {
			if (check_nibble(s[0]) && check_nibble(s[1])) {
				values++;
			} else {
				goto format2;
			}
			s++;
		}
		s++;
	}
	goto end;

format2:
	/* we expect "0x0, 0x0, 0x0" */
	s = str;
	delims = values = 0;
	while (s < end) {

		/* skip white space */
		if (isspace(*s)) {
			s++;
			continue;
		}

		/* find delimeters */
		if (*s == ',') {
			delims++;
			s++;
			continue;
		}

		/* find 0x[0-9] values */
		if (*s == '0' && s + 2 <= end) {
			if (s[1] == 'x' && check_nibble(s[2])) {
				if (check_nibble(s[3]))
					s++;
				values++;
				s += 2;
			}
		}

		s++;
	}

end:
	/* there should always be one less comma than value */
	if (values - 1 != delims)
		return -EINVAL;

	return values;
}

/* get uuid from a string made by 16 characters separated by commas */
static int get_uuid(const char *str, unsigned char *uuid_le)
{
	unsigned long int  val;
	char *tmp, *s = NULL;
	int values = 0, ret = 0;

	tmp = strdup(str);
	if (tmp == NULL)
		return -ENOMEM;

	if (strchr(tmp, ':') == NULL)
		goto data2;

	s = strtok(tmp, ":");
	while (s != NULL) {
		errno = 0;
		val = strtoul(s, NULL, 16);
		if ((errno == ERANGE && val == ULONG_MAX)
			|| (errno != 0 && val == 0)
			|| (val > UCHAR_MAX)) {
			SNDERR("invalid value for uuid");
			ret = -EINVAL;
			goto out;
		}

		*(uuid_le + values) = (unsigned char)val;

		values++;
		if (values >= 16)
			break;

		s = strtok(NULL, ":");
	}
	goto out;

data2:
	s = strtok(tmp, ",");

	while (s != NULL) {
		errno = 0;
		val = strtoul(s, NULL, 0);
		if ((errno == ERANGE && val == ULONG_MAX)
			|| (errno != 0 && val == 0)
			|| (val > UCHAR_MAX)) {
			SNDERR("invalid value for uuid");
			ret = -EINVAL;
			goto out;
		}

		*(uuid_le + values) = (unsigned char)val;

		values++;
		if (values >= 16)
			break;

		s = strtok(NULL, ",");
	}

	if (values < 16) {
		SNDERR("less than 16 integers for uuid");
		ret = -EINVAL;
	}

out:
	free(tmp);
	return ret;
}

static int write_hex(char *buf, char *str, int width)
{
	long val;
	void *p = &val;

        errno = 0;
	val = strtol(str, NULL, 16);

	if ((errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
		|| (errno != 0 && val == 0)) {
		return -EINVAL;
        }

	switch (width) {
	case 1:
		*(unsigned char *)buf = *(unsigned char *)p;
		break;
	case 2:
		*(unsigned short *)buf = *(unsigned short *)p;
		break;
	case 4:
		*(unsigned int *)buf = *(unsigned int *)p;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int copy_data_hex(char *data, int off, const char *str, int width)
{
	char *tmp, *s = NULL, *p = data;
	int ret;

	tmp = strdup(str);
	if (tmp == NULL)
		return -ENOMEM;

	p += off;
	s = strtok(tmp, ",:");

	while (s != NULL) {
		ret = write_hex(p, s, width);
		if (ret < 0) {
			free(tmp);
			return ret;
		}

		s = strtok(NULL, ",:");
		p += width;
	}

	free(tmp);
	return 0;
}

static int tplg_parse_data_hex(snd_config_t *cfg, struct tplg_elem *elem,
	int width)
{
	struct snd_soc_tplg_private *priv;
	const char *value = NULL;
	int size, esize, off, num;
	int ret;

	tplg_dbg(" data: %s", elem->id);

	if (snd_config_get_string(cfg, &value) < 0)
		return -EINVAL;

	num = get_hex_num(value);
	if (num <= 0) {
		SNDERR("malformed hex variable list %s", value);
		return -EINVAL;
	}

	size = num * width;
	priv = elem->data;

	if (size > TPLG_MAX_PRIV_SIZE) {
		SNDERR("data too big %d", size);
		return -EINVAL;
	}

	if (priv != NULL) {
		off = priv->size;
		esize = elem->size + size;
		priv = realloc(priv, esize);
	} else {
		off = 0;
		esize = sizeof(*priv) + size;
		priv = calloc(1, esize);
	}

	if (!priv)
		return -ENOMEM;

	elem->data = priv;
	priv->size += size;
	elem->size = esize;

	ret = copy_data_hex(priv->data, off, value, width);

	dump_priv_data(elem);
	return ret;
}

/* get the token integer value from its id */
static int get_token_value(const char *token_id,
			   struct tplg_vendor_tokens *tokens)
{
	unsigned int i;

	for (i = 0; i < tokens->num_tokens; i++) {
		if (strcmp(token_id, tokens->token[i].id) == 0)
			return tokens->token[i].value;
	}

	SNDERR("cannot find token id '%s'", token_id);
	return -1;
}

/* get the vendor tokens referred by the vendor tuples */
static struct tplg_elem *get_tokens(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;

	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		if (ref->type != SND_TPLG_TYPE_TOKEN)
			continue;

		if (!ref->elem) {
			ref->elem = tplg_elem_lookup(&tplg->token_list,
				ref->id, SND_TPLG_TYPE_TOKEN, elem->index);
		}

		return ref->elem;
	}

	return NULL;
}

/* check if a data element has tuples */
static bool has_tuples(struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;

	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->type == SND_TPLG_TYPE_TUPLE)
			return true;
	}

	return false;
}

/* get size of a tuple element from its type */
unsigned int tplg_get_tuple_size(int type)
{
	switch (type) {

	case SND_SOC_TPLG_TUPLE_TYPE_UUID:
		return sizeof(struct snd_soc_tplg_vendor_uuid_elem);

	case SND_SOC_TPLG_TUPLE_TYPE_STRING:
		return sizeof(struct snd_soc_tplg_vendor_string_elem);

	default:
		return sizeof(struct snd_soc_tplg_vendor_value_elem);
	}
}

/* Add a tuples object to the private buffer of its parent data element */
static int copy_tuples(struct tplg_elem *elem,
		       struct tplg_vendor_tuples *tuples,
		       struct tplg_vendor_tokens *tokens)
{
	struct snd_soc_tplg_private *priv = elem->data, *priv2;
	struct tplg_tuple_set *tuple_set;
	struct tplg_tuple *tuple;
	struct snd_soc_tplg_vendor_array *array;
	struct snd_soc_tplg_vendor_uuid_elem *uuid;
	struct snd_soc_tplg_vendor_string_elem *string;
	struct snd_soc_tplg_vendor_value_elem *value;
	int set_size, size, off;
	unsigned int i, j;
	int token_val;

	size = priv ? priv->size : 0; /* original private data size */

	/* scan each tuples set (one set per type) */
	for (i = 0; i < tuples->num_sets ; i++) {
		tuple_set = tuples->set[i];
		set_size = sizeof(struct snd_soc_tplg_vendor_array)
			+ tplg_get_tuple_size(tuple_set->type)
			* tuple_set->num_tuples;
		size += set_size;
		if (size > TPLG_MAX_PRIV_SIZE) {
			SNDERR("data too big %d", size);
			return -EINVAL;
		}

		if (priv != NULL) {
			priv2 = realloc(priv, sizeof(*priv) + size);
			if (priv2 == NULL) {
				free(priv);
				priv = NULL;
			} else {
				priv = priv2;
			}
		} else {
			priv = calloc(1, sizeof(*priv) + size);
		}
		if (!priv)
			return -ENOMEM;

		off = priv->size;
		priv->size = size; /* update private data size */
		elem->data = priv;

		array = (struct snd_soc_tplg_vendor_array *)(priv->data + off);
		memset(array, 0, set_size);
		array->size = set_size;
		array->type = tuple_set->type;
		array->num_elems = tuple_set->num_tuples;

		/* fill the private data buffer */
		for (j = 0; j < tuple_set->num_tuples; j++) {
			tuple = &tuple_set->tuple[j];
			token_val = get_token_value(tuple->token, tokens);
			if (token_val  < 0)
				return -EINVAL;

			switch (tuple_set->type) {
			case SND_SOC_TPLG_TUPLE_TYPE_UUID:
				uuid = &array->uuid[j];
				uuid->token = token_val;
				memcpy(uuid->uuid, tuple->uuid, 16);
				break;

			case SND_SOC_TPLG_TUPLE_TYPE_STRING:
				string = &array->string[j];
				string->token = token_val;
				snd_strlcpy(string->string, tuple->string,
					SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
				break;

			default:
				value = &array->value[j];
				value->token = token_val;
				value->value = tuple->value;
				break;
			}
		}
	}

	return 0;
}

/* build a data element from its tuples */
static int build_tuples(snd_tplg_t *tplg, struct tplg_elem *elem)
{
	struct tplg_ref *ref;
	struct list_head *base, *pos;
	struct tplg_elem *tuples, *tokens;
	int err;

	base = &elem->ref_list;
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);

		if (ref->type != SND_TPLG_TYPE_TUPLE)
			continue;

		tplg_dbg("tuples '%s' used by data '%s'", ref->id, elem->id);

		if (!ref->elem)
			ref->elem = tplg_elem_lookup(&tplg->tuple_list,
				ref->id, SND_TPLG_TYPE_TUPLE, elem->index);
		tuples = ref->elem;
		if (!tuples) {
			SNDERR("cannot find tuples %s", ref->id);
			return -EINVAL;
		}

		tokens = get_tokens(tplg, tuples);
		if (!tokens) {
			SNDERR("cannot find token for %s", ref->id);
			return -EINVAL;
		}

		/* a data object can have multiple tuples objects */
		err = copy_tuples(elem, tuples->tuples, tokens->tokens);
		if (err < 0)
			return err;
	}

	return 0;
}

struct tuple_type {
	unsigned int type;
	const char *name;
	unsigned int size;
};

static struct tuple_type tuple_types[] = {
	{
		.type = SND_SOC_TPLG_TUPLE_TYPE_UUID,
		.name = "uuid",
		.size = 4,
	},
	{
		.type = SND_SOC_TPLG_TUPLE_TYPE_STRING,
		.name = "string",
		.size = 6,
	},
	{
		.type = SND_SOC_TPLG_TUPLE_TYPE_BOOL,
		.name = "bool",
		.size = 4,
	},
	{
		.type = SND_SOC_TPLG_TUPLE_TYPE_BYTE,
		.name = "byte",
		.size = 4,
	},
	{
		.type = SND_SOC_TPLG_TUPLE_TYPE_SHORT,
		.name = "short",
		.size = 5,
	},
	{
		.type = SND_SOC_TPLG_TUPLE_TYPE_WORD,
		.name = "word",
		.size = 4
	},
};

static int get_tuple_type(const char *name)
{
	struct tuple_type *t;
	unsigned int i;

	/* skip initial index for sorting */
	while ((*name >= '0' && *name <= '9') || *name == '_')
		name++;
	for (i = 0; i < ARRAY_SIZE(tuple_types); i++) {
		t = &tuple_types[i];
		if (strncasecmp(t->name, name, t->size) == 0)
			return t->type;
	}
	return -EINVAL;
}

static const char *get_tuple_type_name(unsigned int type)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(tuple_types); i++)
		if (tuple_types[i].type == type)
			return tuple_types[i].name;
	return NULL;
}

static int parse_tuple_set(snd_config_t *cfg,
			   struct tplg_tuple_set **s)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *value;
	struct tplg_tuple_set *set;
	unsigned int num_tuples = 0;
	struct tplg_tuple *tuple;
	unsigned int tuple_val;
	int type, ival;

	snd_config_get_id(cfg, &id);

	type = get_tuple_type(id);
	if (type < 0) {
		SNDERR("invalid tuple type '%s'", id);
		return type;
	}

	snd_config_for_each(i, next, cfg)
		num_tuples++;
	if (!num_tuples)
		return 0;

	tplg_dbg("\t %d %s tuples:", num_tuples, id);
	set = calloc(1, sizeof(*set) + num_tuples * sizeof(struct tplg_tuple));
	if (!set)
		return -ENOMEM;

	set->type = type;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);

		/* get id */
		if (snd_config_get_id(n, &id) < 0)
			continue;

		tuple = &set->tuple[set->num_tuples];
		snd_strlcpy(tuple->token, id,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);

		switch (type) {
		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			if (snd_config_get_string(n, &value) < 0)
				continue;
			if (get_uuid(value, tuple->uuid) < 0)
				goto err;
			break;

		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			if (snd_config_get_string(n, &value) < 0)
				continue;
			snd_strlcpy(tuple->string, value,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
			tplg_dbg("\t\t%s = %s", tuple->token, tuple->string);
			break;

		case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
			ival = snd_config_get_bool(n);
			if (ival < 0)
				continue;
			tuple->value = ival;
			tplg_dbg("\t\t%s = %d", tuple->token, tuple->value);
			break;

		case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
		case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
		case SND_SOC_TPLG_TUPLE_TYPE_WORD:
			ival = tplg_get_unsigned(n, &tuple_val, 0);
			if (ival < 0) {
				SNDERR("tuple %s: %s", id, snd_strerror(ival));
				goto err;
			}

			if ((type == SND_SOC_TPLG_TUPLE_TYPE_WORD
					&& tuple_val > UINT_MAX)
				|| (type == SND_SOC_TPLG_TUPLE_TYPE_SHORT
					&& tuple_val > USHRT_MAX)
				|| (type == SND_SOC_TPLG_TUPLE_TYPE_BYTE
					&& tuple_val > UCHAR_MAX)) {
				SNDERR("tuple %s: invalid value", id);
				goto err;
			}

			tuple->value = tuple_val;
			tplg_dbg("\t\t%s = 0x%x", tuple->token, tuple->value);
			break;

		default:
			break;
		}

		set->num_tuples++;
	}

	*s = set;
	return 0;

err:
	free(set);
	return -EINVAL;
}

/* save tuple set */
static int tplg_save_tuple_set(struct tplg_vendor_tuples *tuples,
			       unsigned int set_index,
			       char **dst, const char *pfx)
{
	struct tplg_tuple_set *set;
	struct tplg_tuple *tuple;
	const char *s, *fmt;
	char buf[32];
	unsigned int i;
	int err;

	set = tuples->set[set_index];
	if (set->num_tuples == 0)
		return 0;
	s = get_tuple_type_name(set->type);
	if (s == NULL)
		return -EINVAL;
	if (tuples->num_sets < 10)
		fmt = "%u_";
	else if (tuples->num_sets < 100)
		fmt = "%02u_";
	else if (tuples->num_sets < 1000)
		fmt = "%03u_";
	else
		return -EINVAL;
	if (set->num_tuples > 1) {
		snprintf(buf, sizeof(buf), "tuples.%s%%s {\n", fmt);
		err = tplg_save_printf(dst, NULL, buf, set_index, s);
		if (err < 0)
			return err;
	}
	for (i = 0; i < set->num_tuples; i++) {
		tuple = &set->tuple[i];
		if (set->num_tuples == 1) {
			snprintf(buf, sizeof(buf), "tuples.%s%%s.'%%s' ", fmt);
			err = tplg_save_printf(dst, NULL, buf,
					       set_index, s, tuple->token);
		} else {
			err = tplg_save_printf(dst, pfx, "\t'%s' ",
					       tuple->token);
		}
		switch (set->type) {
		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			err = tplg_save_printf(dst, NULL, "'" UUID_FORMAT "'\n",
					       tuple->uuid[0], tuple->uuid[1],
					       tuple->uuid[2], tuple->uuid[3],
					       tuple->uuid[4], tuple->uuid[5],
					       tuple->uuid[6], tuple->uuid[7],
					       tuple->uuid[8], tuple->uuid[9],
					       tuple->uuid[10], tuple->uuid[11],
					       tuple->uuid[12], tuple->uuid[13],
					       tuple->uuid[14], tuple->uuid[15]);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			err = tplg_save_printf(dst, NULL, "'%s'\n",
					       tuple->string);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
		case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
		case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
			err = tplg_save_printf(dst, NULL, "%u\n", tuple->value);
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_WORD:
			tplg_nice_value_format(buf, sizeof(buf), tuple->value);
			err = tplg_save_printf(dst, NULL, "%s\n", buf);
			break;
		default:
			return -EINVAL;
		}
		if (err < 0)
			return err;
	}
	if (set->num_tuples > 1)
		return tplg_save_printf(dst, pfx, "}\n");
	return 0;
}

static int parse_tuple_sets(snd_config_t *cfg,
			    struct tplg_vendor_tuples *tuples)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	unsigned int num_tuple_sets = 0;
	int err;

	if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
		if (snd_config_get_id(cfg, &id) >= 0)
			SNDERR("compound type expected for %s", id);
		return -EINVAL;
	}

	snd_config_for_each(i, next, cfg) {
		num_tuple_sets++;
	}

	if (!num_tuple_sets)
		return 0;

	tuples->set = calloc(1, num_tuple_sets * sizeof(void *));
	if (!tuples->set)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("compound type expected for %s, is %d",
			       id, snd_config_get_type(n));
			return -EINVAL;
		}

		err = parse_tuple_set(n, &tuples->set[tuples->num_sets]);
		if (err < 0)
			return err;

		/* overlook empty tuple sets */
		if (tuples->set[tuples->num_sets])
			tuples->num_sets++;
	}

	return 0;
}

/* save tuple sets */
int tplg_save_tuple_sets(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			 struct tplg_elem *elem,
			 char **dst, const char *pfx)
{
	struct tplg_vendor_tuples *tuples = elem->tuples;
	unsigned int i;
	int err = 0;

	for (i = 0; i < tuples->num_sets; i++) {
		err = tplg_save_printf(dst, pfx, "");
		if (err < 0)
			break;
		err = tplg_save_tuple_set(tuples, i, dst, pfx);
		if (err < 0)
			break;
	}
	return err;
}

/* Parse vendor tokens
 */
int tplg_parse_tokens(snd_tplg_t *tplg, snd_config_t *cfg,
		      void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	struct tplg_elem *elem;
	struct tplg_vendor_tokens *tokens;
	int num_tokens = 0, value;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TOKEN);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {
		num_tokens++;
	}

	if (!num_tokens)
		return 0;

	tplg_dbg(" Vendor tokens: %s, %d tokens", elem->id, num_tokens);

	tokens = calloc(1, sizeof(*tokens)
			+ num_tokens * sizeof(struct tplg_token));
	if (!tokens)
		return -ENOMEM;
	elem->tokens = tokens;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (tplg_get_integer(n, &value, 0))
			continue;

		snd_strlcpy(tokens->token[tokens->num_tokens].id, id,
				SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
		tokens->token[tokens->num_tokens].value = value;
		tplg_dbg("\t\t %s : %d", tokens->token[tokens->num_tokens].id,
			tokens->token[tokens->num_tokens].value);
		tokens->num_tokens++;
	}

	return 0;
}

/* save vendor tokens */
int tplg_save_tokens(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		     struct tplg_elem *elem,
		     char **dst, const char *pfx)
{
	struct tplg_vendor_tokens *tokens = elem->tokens;
	unsigned int i;
	int err;

	if (!tokens || tokens->num_tokens == 0)
		return 0;

	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	if (err < 0)
		return err;
	for (i = 0; err >= 0 && i < tokens->num_tokens; i++)
		err = tplg_save_printf(dst, pfx, "\t'%s' %u\n",
				       tokens->token[i].id,
				       tokens->token[i].value);
	err = tplg_save_printf(dst, pfx, "}\n");
	if (err < 0)
		return err;
	return 0;
}

/* Parse vendor tuples.
 */
int tplg_parse_tuples(snd_tplg_t *tplg, snd_config_t *cfg,
		      void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id, *value;
	struct tplg_elem *elem;
	struct tplg_vendor_tuples *tuples;
	int err;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_TUPLE);
	if (!elem)
		return -ENOMEM;

	tplg_dbg(" Vendor Tuples: %s", elem->id);

	tuples = calloc(1, sizeof(*tuples));
	if (!tuples)
		return -ENOMEM;
	elem->tuples = tuples;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		if (strcmp(id, "tokens") == 0) {
			if (snd_config_get_string(n, &value) < 0)
				return -EINVAL;
			tplg_ref_add(elem, SND_TPLG_TYPE_TOKEN, value);
			tplg_dbg("\t refer to vendor tokens: %s", value);
		}

		if (strcmp(id, "tuples") == 0) {
			err = parse_tuple_sets(n, tuples);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

/* save vendor tuples */
int tplg_save_tuples(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		     struct tplg_elem *elem,
		     char **dst, const char *pfx)
{
	char pfx2[16];
	int err;

	if (!elem->tuples)
		return 0;

	err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_TOKEN,
				     "tokens", dst, pfx2);
	if (err >= 0)
		err = tplg_save_tuple_sets(tplg, elem, dst, pfx2);
	if (err >= 0)
		err = tplg_save_printf(dst, pfx, "}\n");
	return 0;
}

/* Free handler of tuples */
void tplg_free_tuples(void *obj)
{
	struct tplg_vendor_tuples *tuples = (struct tplg_vendor_tuples *)obj;
	unsigned int i;

	if (!tuples || !tuples->set)
		return;

	for (i = 0; i < tuples->num_sets; i++)
		free(tuples->set[i]);

	free(tuples->set);
}

/* Parse manifest's data references
 */
int tplg_parse_manifest_data(snd_tplg_t *tplg, snd_config_t *cfg,
			     void *private ATTRIBUTE_UNUSED)
{
	struct snd_soc_tplg_manifest *manifest;
	struct tplg_elem *elem;
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err;

	if (!list_empty(&tplg->manifest_list)) {
		SNDERR("already has manifest data");
		return -EINVAL;
	}

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_MANIFEST);
	if (!elem)
		return -ENOMEM;

	manifest = elem->manifest;
	manifest->size = elem->size;

	tplg_dbg(" Manifest: %s", elem->id);

	snd_config_for_each(i, next, cfg) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0)
			continue;

		/* skip comments */
		if (strcmp(id, "comment") == 0)
			continue;
		if (id[0] == '#')
			continue;


		if (strcmp(id, "data") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_DATA);
			if (err < 0)
				return err;
			continue;
		}
	}

	return 0;
}

/* save manifest data */
int tplg_save_manifest_data(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
			    struct tplg_elem *elem, char **dst,
			    const char *pfx)
{
	struct list_head *pos;
	struct tplg_ref *ref;
	int err, index, count;

	/* for each ref in this manifest elem */
	count = 0;
	list_for_each(pos, &elem->ref_list) {
		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->type != SND_TPLG_TYPE_DATA)
			continue;
		count++;
	}
	if (count == 0)
		return tplg_save_printf(dst, NULL,
					"'%s'.comment 'empty'\n", elem->id);
	if (count > 1) {
		err = tplg_save_printf(dst, NULL, "'%s'.data [\n", elem->id);
		if (err < 0)
			return err;
	}
	index = 0;
	list_for_each(pos, &elem->ref_list) {
		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->type != SND_TPLG_TYPE_DATA)
			continue;
		if (count == 1) {
			err = tplg_save_printf(dst, NULL, "'%s'.data.%u '%s'\n",
					       elem->id, index, ref->id);
		} else {
			err = tplg_save_printf(dst, pfx, "\t'%s'\n", ref->id);
			if (err < 0)
				return err;
		}
		index++;
	}
	if (count > 1) {
		err = tplg_save_printf(dst, pfx, "]\n");
		if (err < 0)
			return err;
	}
	return 0;
}

/* merge private data of manifest */
int tplg_build_manifest_data(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem = NULL;
	struct tplg_ref *ref;
	struct snd_soc_tplg_manifest *manifest;
	int err = 0;

	base = &tplg->manifest_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		break;
	}

	if (!elem) /* no manifest data */
		return 0;

	base = &elem->ref_list;

	/* for each ref in this manifest elem */
	list_for_each(pos, base) {

		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->elem)
			continue;

		if (ref->type == SND_TPLG_TYPE_DATA) {
			err = tplg_copy_data(tplg, elem, ref);
			if (err < 0)
				return err;
		}
	}

	manifest = elem->manifest;
	if (!manifest->priv.size) /* no manifest data */
		return 0;

	tplg->manifest_pdata = malloc(manifest->priv.size);
	if (!tplg->manifest_pdata)
		return -ENOMEM;

	tplg->manifest.priv.size = manifest->priv.size;
	memcpy(tplg->manifest_pdata, manifest->priv.data, manifest->priv.size);
	return 0;
}

/* Parse Private data.
 *
 * Object private data can either be from file or defined as bytes, shorts,
 * words, tuples.
 */
int tplg_parse_data(snd_tplg_t *tplg, snd_config_t *cfg,
		    void *private ATTRIBUTE_UNUSED)
{
	snd_config_iterator_t i, next;
	snd_config_t *n;
	const char *id;
	int err = 0, ival;
	struct tplg_elem *elem;

	elem = tplg_elem_new_common(tplg, cfg, NULL, SND_TPLG_TYPE_DATA);
	if (!elem)
		return -ENOMEM;

	snd_config_for_each(i, next, cfg) {

		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &id) < 0) {
			continue;
		}

		if (strcmp(id, "file") == 0) {
			err = tplg_parse_data_file(n, elem);
			if (err < 0) {
				SNDERR("failed to parse data file");
				return err;
			}
			continue;
		}

		if (strcmp(id, "bytes") == 0) {
			err = tplg_parse_data_hex(n, elem, 1);
			if (err < 0) {
				SNDERR("failed to parse data bytes");
				return err;
			}
			continue;
		}

		if (strcmp(id, "shorts") == 0) {
			err = tplg_parse_data_hex(n, elem, 2);
			if (err < 0) {
				SNDERR("failed to parse data shorts");
				return err;
			}
			continue;
		}

		if (strcmp(id, "words") == 0) {
			err = tplg_parse_data_hex(n, elem, 4);
			if (err < 0) {
				SNDERR("failed to parse data words");
				return err;
			}
			continue;
		}

		if (strcmp(id, "tuples") == 0) {
			err = tplg_parse_refs(n, elem, SND_TPLG_TYPE_TUPLE);
			if (err < 0)
				return err;
			continue;
		}

		if (strcmp(id, "type") == 0) {
			if (tplg_get_integer(n, &ival, 0))
				return -EINVAL;

			elem->vendor_type = ival;
			tplg_dbg("\t%s: %d", id, elem->index);
			continue;
		}
	}

	return err;
}

/* save data element */
int tplg_save_data(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		   struct tplg_elem *elem,
		   char **dst, const char *pfx)
{
	struct snd_soc_tplg_private *priv = elem->data;
	struct list_head *pos;
	struct tplg_ref *ref;
	char pfx2[16];
	unsigned int i, count;
	int err;

	count = 0;
	if (priv && priv->size > 0)
		count++;
	list_for_each(pos, &elem->ref_list) {
		ref = list_entry(pos, struct tplg_ref, list);
		if (ref->type == SND_TPLG_TYPE_TUPLE)
			count++;
	}
	if (elem->vendor_type > 0)
		count++;

	if (count > 1) {
		err = tplg_save_printf(dst, NULL, "'%s' {\n", elem->id);
		if (err >= 0)
			err = tplg_save_printf(dst, NULL, "");
	} else {
		err = tplg_save_printf(dst, NULL, "'%s'.", elem->id);
	}
	if (err >= 0 && priv && priv->size > 0) {
		if (count > 1) {
			err = tplg_save_printf(dst, pfx, "");
			if (err < 0)
				return err;
		}
		if (priv->size > 8) {
			err = tplg_save_printf(dst, NULL, "bytes\n");
			if (err >= 0)
				err = tplg_save_printf(dst, pfx, "\t'");
		} else {
			err = tplg_save_printf(dst, NULL, "bytes '");
		}
		if (err < 0)
			return err;
		for (i = 0; i < priv->size; i++) {
			if (i > 0 && (i % 8) == 0) {
				err = tplg_save_printf(dst, NULL, ":\n");
				if (err < 0)
					return err;
				err = tplg_save_printf(dst, pfx, "\t ");
				if (err < 0)
					return err;
			}
			err = tplg_save_printf(dst, NULL, "%s%02x",
					       (i % 8) == 0 ? "" : ":",
					       (unsigned char)priv->data[i]);
			if (err < 0)
				return err;
		}
		err = tplg_save_printf(dst, NULL, "'\n");
	}
	snprintf(pfx2, sizeof(pfx2), "%s\t", pfx ?: "");
	if (err >= 0)
		err = tplg_save_refs(tplg, elem, SND_TPLG_TYPE_TUPLE,
				     "tuples", dst,
				     count > 1 ? pfx2 : NULL);
	if (err >= 0 && elem->vendor_type > 0)
		err = tplg_save_printf(dst, pfx, "type %u",
				       elem->vendor_type);
	if (err >= 0 && count > 1)
		err = tplg_save_printf(dst, pfx, "}\n");
	return err;
}

/* Find a referenced data element and copy its data to the parent
 * element's private data buffer.
 * An element can refer to multiple data sections. Data of these sections
 * will be merged in the their reference order.
 */
int tplg_copy_data(snd_tplg_t *tplg, struct tplg_elem *elem,
		   struct tplg_ref *ref)
{
	struct tplg_elem *ref_elem;
	struct snd_soc_tplg_private *priv, *old_priv;
	int priv_data_size, old_priv_data_size;
	void *obj;

	ref_elem = tplg_elem_lookup(&tplg->pdata_list,
				     ref->id, SND_TPLG_TYPE_DATA, elem->index);
	if (!ref_elem) {
		SNDERR("cannot find data '%s' referenced by"
		       " element '%s'", ref->id, elem->id);
		return -EINVAL;
	}

	tplg_dbg("Data '%s' used by '%s'", ref->id, elem->id);
	/* overlook empty private data */
	if (!ref_elem->data || !ref_elem->data->size) {
		ref->elem = ref_elem;
		return 0;
	}

	old_priv = get_priv_data(elem);
	if (!old_priv)
		return -EINVAL;
	old_priv_data_size = old_priv->size;

	priv_data_size = ref_elem->data->size;
	obj = realloc(elem->obj,
			elem->size + priv_data_size);
	if (!obj)
		return -ENOMEM;
	elem->obj = obj;

	priv = get_priv_data(elem);
	if (!priv)
		return -EINVAL;

	/* merge the new data block */
	elem->size += priv_data_size;
	priv->size = priv_data_size + old_priv_data_size;
	ref_elem->compound_elem = 1;
	memcpy(priv->data + old_priv_data_size,
	       ref_elem->data->data, priv_data_size);

	ref->elem = ref_elem;
	return 0;
}

/* check data objects and build those with tuples */
int tplg_build_data(snd_tplg_t *tplg)
{
	struct list_head *base, *pos;
	struct tplg_elem *elem;
	int err = 0;

	base = &tplg->pdata_list;
	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);
		if (has_tuples(elem)) {
			err = build_tuples(tplg, elem);
			if (err < 0)
				return err;
		}
	}

	return 0;
}

/* decode manifest data */
int tplg_decode_manifest_data(snd_tplg_t *tplg,
			      size_t pos,
			      struct snd_soc_tplg_hdr *hdr,
			      void *bin, size_t size)
{
	struct snd_soc_tplg_manifest *m = bin;
	struct tplg_elem *elem;
	size_t off;

	if (hdr->index != 0) {
		SNDERR("manifest - wrong index %d", hdr->index);
		return -EINVAL;
	}

	if (sizeof(*m) > size) {
		SNDERR("manifest - wrong size %zd (minimal %zd)",
		       size, sizeof(*m));
		return -EINVAL;
	}

	if (m->size != sizeof(*m)) {
		SNDERR("manifest - wrong sructure size %d", m->size);
		return -EINVAL;
	}

	off = offsetof(struct snd_soc_tplg_manifest, priv);
	if (off + m->priv.size > size) {
		SNDERR("manifest - wrong private size %d", m->priv.size);
		return -EINVAL;
	}

	tplg->manifest = *m;

	bin += off;
	size -= off;
	pos += off;

	elem = tplg_elem_new_common(tplg, NULL, "manifest",
				    SND_TPLG_TYPE_MANIFEST);
	if (!elem)
		return -ENOMEM;

	tplg_log(tplg, 'D', pos, "manifest: private size %d", size);
	return tplg_add_data(tplg, elem, bin, size);
}

int tplg_add_token(snd_tplg_t *tplg, struct tplg_elem *parent,
		   unsigned int token,
		   char str_ref[SNDRV_CTL_ELEM_ID_NAME_MAXLEN])
{
	struct tplg_elem *elem;
	struct tplg_token *t;
	struct tplg_vendor_tokens *tokens;
	unsigned int i;
	size_t size;

	elem = tplg_elem_lookup(&tplg->token_list, parent->id,
				SND_TPLG_TYPE_TOKEN, parent->index);
	if (elem == NULL) {
		elem = tplg_elem_new_common(tplg, NULL, parent->id,
					    SND_TPLG_TYPE_TOKEN);
		if (!elem)
			return -ENOMEM;
	}

	tokens = elem->tokens;
	if (tokens) {
		for (i = 0; i < tokens->num_tokens; i++) {
			t = &tokens->token[i];
			if (t->value == token)
				goto found;
		}
		size = sizeof(*tokens) +
		       (tokens->num_tokens + 1) * sizeof(struct tplg_token);
		tokens = realloc(tokens, size);
	} else {
		size = sizeof(*tokens) + 1 * sizeof(struct tplg_token);
		tokens = calloc(1, size);
	}

	if (!tokens)
		return -ENOMEM;

	memset(&tokens->token[tokens->num_tokens], 0, sizeof(struct tplg_token));
	elem->tokens = tokens;
	t = &tokens->token[tokens->num_tokens];
	tokens->num_tokens++;
	snprintf(t->id, sizeof(t->id), "token%u", token);
	t->value = token;
found:
	snd_strlcpy(str_ref, t->id, SNDRV_CTL_ELEM_ID_NAME_MAXLEN);
	return 0;
}

static int tplg_verify_tuple_set(snd_tplg_t *tplg, size_t pos,
				 const void *bin, size_t size)
{
	const struct snd_soc_tplg_vendor_array *va;
	unsigned int j;

	va = bin;
	if (size < sizeof(*va) || size < va->size) {
		tplg_log(tplg, 'A', pos, "tuple set verify: wrong size %d", size);
		return -EINVAL;
	}

	switch (va->type) {
	case SND_SOC_TPLG_TUPLE_TYPE_UUID:
	case SND_SOC_TPLG_TUPLE_TYPE_STRING:
	case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
	case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
	case SND_SOC_TPLG_TUPLE_TYPE_WORD:
	case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
		break;
	default:
		tplg_log(tplg, 'A', pos, "tuple set verify: unknown array type %d", va->type);
		return -EINVAL;
	}

	j = tplg_get_tuple_size(va->type) * va->num_elems;
	if (j + sizeof(*va) != va->size) {
		tplg_log(tplg, 'A', pos, "tuple set verify: wrong vendor array size %d "
			 "(expected %d for %d count %d)",
			 va->size, j + sizeof(*va), va->type, va->num_elems);
		return -EINVAL;
	}

	if (va->num_elems > 4096) {
		tplg_log(tplg, 'A', pos, "tuple set verify: tuples overflow %d", va->num_elems);
		return -EINVAL;
	}

	return 0;
}

static int tplg_decode_tuple_set(snd_tplg_t *tplg,
				 size_t pos,
				 struct tplg_elem *parent,
				 struct tplg_tuple_set **_set,
				 const void *bin, size_t size)
{
	const struct snd_soc_tplg_vendor_array *va;
	struct tplg_tuple_set *set;
	struct tplg_tuple *tuple;
	unsigned int j;
	int err;

	va = bin;
	if (size < sizeof(*va) || size < va->size) {
		SNDERR("tuples: wrong size %d", size);
		return -EINVAL;
	}

	switch (va->type) {
	case SND_SOC_TPLG_TUPLE_TYPE_UUID:
	case SND_SOC_TPLG_TUPLE_TYPE_STRING:
	case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
	case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
	case SND_SOC_TPLG_TUPLE_TYPE_WORD:
	case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
		break;
	default:
		SNDERR("tuples: unknown array type %d", va->type);
		return -EINVAL;
	}

	j = tplg_get_tuple_size(va->type) * va->num_elems;
	if (j + sizeof(*va) != va->size) {
		SNDERR("tuples: wrong vendor array size %d "
		       "(expected %d for %d count %d)",
		       va->size, j + sizeof(*va), va->type, va->num_elems);
		return -EINVAL;
	}

	if (va->num_elems > 4096) {
		SNDERR("tuples: tuples overflow %d", va->num_elems);
		return -EINVAL;
	}

	set = calloc(1, sizeof(*set) + va->num_elems * sizeof(struct tplg_tuple));
	if (!set)
		return -ENOMEM;

	set->type = va->type;
	set->num_tuples = va->num_elems;

	tplg_log(tplg, 'A', pos, "tuple set: type %d (%s) tuples %d size %d", set->type,
		 get_tuple_type_name(set->type), set->num_tuples, va->size);
	for (j = 0; j < set->num_tuples; j++) {
		tuple = &set->tuple[j];
		switch (va->type) {
		case SND_SOC_TPLG_TUPLE_TYPE_UUID:
			err = tplg_add_token(tplg, parent, va->uuid[j].token,
					     tuple->token);
			if (err < 0)
				goto retval;
			memcpy(tuple->uuid, va->uuid[j].uuid,
			       sizeof(va->uuid[j].uuid));
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_STRING:
			err = tplg_add_token(tplg, parent, va->string[j].token,
					     tuple->token);
			if (err < 0)
				goto retval;
			snd_strlcpy(tuple->string, va->string[j].string,
				    sizeof(tuple->string));
			break;
		case SND_SOC_TPLG_TUPLE_TYPE_BOOL:
		case SND_SOC_TPLG_TUPLE_TYPE_BYTE:
		case SND_SOC_TPLG_TUPLE_TYPE_WORD:
		case SND_SOC_TPLG_TUPLE_TYPE_SHORT:
			err = tplg_add_token(tplg, parent, va->value[j].token,
					     tuple->token);
			if (err < 0)
				goto retval;
			tuple->value = va->value[j].value;
			break;
		}
	}

	*_set = set;
	return 0;

retval:
	free(set);
	return err;
}

/* verify tuples from the binary input */
static int tplg_verify_tuples(snd_tplg_t *tplg, size_t pos,
			      const void *bin, size_t size)
{
	const struct snd_soc_tplg_vendor_array *va;
	int err;

	if (size < sizeof(*va)) {
		tplg_log(tplg, 'A', pos, "tuples: small size %d", size);
		return -EINVAL;
	}

next:
	va = bin;
	if (size < sizeof(*va)) {
		tplg_log(tplg, 'A', pos, "tuples: unexpected vendor arry size %d", size);
		return -EINVAL;
	}

	err = tplg_verify_tuple_set(tplg, pos, va, va->size);
	if (err < 0)
		return err;

	bin += va->size;
	size -= va->size;
	pos += va->size;
	if (size > 0)
		goto next;

	return 0;
}

/* add tuples from the binary input */
static int tplg_decode_tuples(snd_tplg_t *tplg,
			      size_t pos,
			      struct tplg_elem *parent,
			      struct tplg_vendor_tuples *tuples,
			      const void *bin, size_t size)
{
	const struct snd_soc_tplg_vendor_array *va;
	struct tplg_tuple_set *set;
	int err;

	if (size < sizeof(*va)) {
		SNDERR("tuples: small size %d", size);
		return -EINVAL;
	}

next:
	va = bin;
	if (size < sizeof(*va)) {
		SNDERR("tuples: unexpected vendor arry size %d", size);
		return -EINVAL;
	}

	if (tuples->num_sets >= tuples->alloc_sets) {
		SNDERR("tuples: index overflow (%d)", tuples->num_sets);
		return -EINVAL;
	}

	err = tplg_decode_tuple_set(tplg, pos, parent, &set, va, va->size);
	if (err < 0)
		return err;
	tuples->set[tuples->num_sets++] = set;

	bin += va->size;
	size -= va->size;
	pos += va->size;
	if (size > 0)
		goto next;

	return 0;
}

/* decode private data */
int tplg_add_data(snd_tplg_t *tplg,
		  struct tplg_elem *parent,
		  const void *bin, size_t size)
{
	const struct snd_soc_tplg_private *tp;
	const struct snd_soc_tplg_vendor_array *va;
	struct tplg_elem *elem = NULL, *elem2 = NULL;
	struct tplg_vendor_tuples *tuples = NULL;
	char id[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];
	char suffix[16];
	size_t pos = 0, off;
	int err, num_tuples = 0, block = 0;

	if (size == 0)
		return 0;

	off = offsetof(struct snd_soc_tplg_private, array);

next:
	tp = bin;
	if (off + size < tp->size) {
		SNDERR("data: unexpected element size %d", size);
		return -EINVAL;
	}

	if (tplg_verify_tuples(tplg, pos, tp->array, tp->size) < 0) {
		if (tuples) {
			err = tplg_ref_add(elem, SND_TPLG_TYPE_TOKEN, parent->id);
			if (err < 0)
				return err;
			err = tplg_ref_add(elem2, SND_TPLG_TYPE_TUPLE, id);
			if (err < 0)
				return err;
			err = tplg_ref_add(parent, SND_TPLG_TYPE_DATA, id);
			if (err < 0)
				return err;
			tuples = NULL;
		}
		tplg_log(tplg, 'A', pos, "add bytes: size %d", tp->size);
		snprintf(suffix, sizeof(suffix), "data%u", block++);
		err = tplg_add_data_bytes(tplg, parent, suffix, tp->array, tp->size);
	} else {
		if (!tuples) {
			snprintf(id, sizeof(id), "%.30s:tuple%d", parent->id, (block++) & 0xffff);
			elem = tplg_elem_new_common(tplg, NULL, id, SND_TPLG_TYPE_TUPLE);
			if (!elem)
				return -ENOMEM;

			elem2 = tplg_elem_new_common(tplg, NULL, id, SND_TPLG_TYPE_DATA);
			if (!elem2)
				return -ENOMEM;

			tuples = calloc(1, sizeof(*tuples));
			if (!tuples)
				return -ENOMEM;
			elem->tuples = tuples;

			tuples->alloc_sets = (size / sizeof(*va)) + 1;
			tuples->set = calloc(1, tuples->alloc_sets * sizeof(void *));
			if (!tuples->set) {
				tuples->alloc_sets = 0;
				return -ENOMEM;
			}
		}
		tplg_log(tplg, 'A', pos, "decode tuples: size %d", tp->size);
		err = tplg_decode_tuples(tplg, pos, parent, tuples, tp->array, tp->size);
		num_tuples++;
	}
	if (err < 0)
		return err;

	bin += off + tp->size;
	size -= off + tp->size;
	pos += off + tp->size;
	if (size > 0)
		goto next;

	if (tuples && elem && elem2) {
		err = tplg_ref_add(elem, SND_TPLG_TYPE_TOKEN, parent->id);
		if (err < 0)
			return err;
		err = tplg_ref_add(elem2, SND_TPLG_TYPE_TUPLE, id);
		if (err < 0)
			return err;
		err = tplg_ref_add(parent, SND_TPLG_TYPE_DATA, id);
		if (err < 0)
			return err;
	}

	return 0;
}

/* add private data - bytes */
int tplg_add_data_bytes(snd_tplg_t *tplg, struct tplg_elem *parent,
			const char *suffix, const void *bin, size_t size)
{
	struct snd_soc_tplg_private *priv;
	struct tplg_elem *elem;
	char id[SNDRV_CTL_ELEM_ID_NAME_MAXLEN];

	if (suffix)
		snprintf(id, sizeof(id), "%.30s:%.12s", parent->id, suffix);
	else
		snd_strlcpy(id, parent->id, sizeof(id));
	elem = tplg_elem_new_common(tplg, NULL, id, SND_TPLG_TYPE_DATA);
	if (!elem)
		return -ENOMEM;

	priv = malloc(sizeof(*priv) + size);
	if (!priv)
		return -ENOMEM;
	memcpy(priv->data, bin, size);
	priv->size = size;
	elem->data = priv;

	return tplg_ref_add(parent, SND_TPLG_TYPE_DATA, id);
}

/* decode data from the binary input */
int tplg_decode_data(snd_tplg_t *tplg ATTRIBUTE_UNUSED,
		     size_t pos ATTRIBUTE_UNUSED,
		     struct snd_soc_tplg_hdr *hdr ATTRIBUTE_UNUSED,
		     void *bin ATTRIBUTE_UNUSED,
		     size_t size ATTRIBUTE_UNUSED)
{
	SNDERR("data type not expected");
	return -EINVAL;
}
