/*
  Copyright (c) 2019 Red Hat Inc.
  All rights reserved.

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  Authors: Jaroslav Kysela <perex@perex.cz>
*/

#include "list.h"
#include "tplg_local.h"

/* verbose output detailing each object size and file position */
void tplg_dv(snd_tplg_t *tplg, size_t pos, const char *fmt, ...)
{
	va_list va;

	if (!tplg->verbose)
		return;

	va_start(va, fmt);
	fprintf(stdout, "D0x%6.6zx/%6.6zd - ", pos, pos);
	vfprintf(stdout, fmt, va);
	va_end(va);
	putc('\n', stdout);
}

int tplg_decode_template(snd_tplg_t *tplg,
			 size_t pos,
			 struct snd_soc_tplg_hdr *hdr,
			 snd_tplg_obj_template_t *t)
{
	int type;

	type = tplg_get_type(hdr->type);
	tplg_dv(tplg, pos, "template: asoc type %d library type %d", hdr->type, type);
	if (type < 0)
		return type;

	memset(t, 0, sizeof(*t));
	t->type = type;
	t->index = hdr->index;
	t->version = hdr->version;
	t->vendor_type = hdr->vendor_type;
	tplg_dv(tplg, pos, "template: index %d version %d vendor_type %d",
		hdr->index, hdr->version, hdr->vendor_type);
	return 0;
}

int snd_tplg_decode(snd_tplg_t *tplg, void *bin, size_t size, int dflags)
{
	struct snd_soc_tplg_hdr *hdr;
	struct tplg_table *tptr;
	size_t pos;
	void *b = bin;
	unsigned int index;
	int err;

	if (dflags != 0)
		return -EINVAL;
	if (tplg == NULL || bin == NULL)
		return -EINVAL;
	while (1) {
		pos = b - bin;
		if (size == pos) {
			tplg_dv(tplg, pos, "block: success (total %zd)", size);
			return 0;
		}
		if (size - pos < sizeof(*hdr)) {
			tplg_dv(tplg, pos, "block: small size");
			SNDERR("incomplete header data to decode");
			return -EINVAL;
		}
		hdr = b;
		if (hdr->magic != SND_SOC_TPLG_MAGIC) {
			SNDERR("bad block magic %08x", hdr->magic);
			return -EINVAL;
		}

		tplg_dv(tplg, pos, "block: abi %d size %d payload size %d",
			hdr->abi, hdr->size, hdr->payload_size);
		if (hdr->abi != SND_SOC_TPLG_ABI_VERSION) {
			SNDERR("unsupported ABI version %d", hdr->abi);
			return -EINVAL;
		}
		if (hdr->size != sizeof(*hdr)) {
			SNDERR("header size mismatch");
			return -EINVAL;
		}

		if (size - pos < hdr->size + hdr->payload_size) {
			SNDERR("incomplete payload data to decode");
			return -EINVAL;
		}

		if (hdr->payload_size < 8) {
			SNDERR("wrong payload size %d", hdr->payload_size);
			return -EINVAL;
		}

		/* first block must be manifest */
		if (b == bin) {
			if (hdr->type != SND_SOC_TPLG_TYPE_MANIFEST) {
				SNDERR("first block must be manifest (value %d)", hdr->type);
				return -EINVAL;
			}
			err = snd_tplg_set_version(tplg, hdr->version);
			if (err < 0)
				return err;
		}

		pos += hdr->size;
		for (index = 0; index < tplg_table_items; index++) {
			tptr = &tplg_table[index];
			if (tptr->tsoc == (int)hdr->type)
				break;
		}
		if (index >= tplg_table_items || tptr->decod == NULL) {
			SNDERR("unknown block type %d", hdr->type);
			return -EINVAL;
		}
		tplg_dv(tplg, pos, "block: type %d - %s", hdr->type, tptr->name);
		err = tptr->decod(tplg, pos, hdr, b + hdr->size, hdr->payload_size);
		if (err < 0)
			return err;
		b += hdr->size + hdr->payload_size;
	}
}
