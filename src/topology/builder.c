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

/* verbose output detailing each object size and file position */
static void verbose(snd_tplg_t *tplg, const char *fmt, ...)
{
	va_list va;

	if (!tplg->verbose)
		return;

	va_start(va, fmt);
	fprintf(stdout, "0x%6.6zx/%6.6zd -", tplg->out_pos, tplg->out_pos);
	vfprintf(stdout, fmt, va);
	va_end(va);
}

/* write a block, track the position */
static ssize_t twrite(snd_tplg_t *tplg, void *data, size_t data_size)
{
	ssize_t r = write(tplg->out_fd, data, data_size);
	if (r != (ssize_t)data_size) {
		if (r < 0) {
			SNDERR("error: unable to write: %s", strerror(errno));
			return -errno;
		}
		tplg->out_pos += r;
		SNDERR("error: unable to write (partial)");
		return -EIO;
	}
	tplg->out_pos += r;
	return r;
}

/* write out block header to output file */
static ssize_t write_block_header(snd_tplg_t *tplg, unsigned int type,
				  unsigned int vendor_type,
				  unsigned int version, unsigned int index,
				  size_t payload_size, int count)
{
	struct snd_soc_tplg_hdr hdr;

	memset(&hdr, 0, sizeof(hdr));
	hdr.magic = SND_SOC_TPLG_MAGIC;
	hdr.abi = SND_SOC_TPLG_ABI_VERSION;
	hdr.type = type;
	hdr.vendor_type = vendor_type;
	hdr.version = version;
	hdr.payload_size = payload_size;
	hdr.index = index;
	hdr.size = sizeof(hdr);
	hdr.count = count;

	/* make sure file offset is aligned with the calculated HDR offset */
	if (tplg->out_pos != tplg->next_hdr_pos) {
		SNDERR("error: New header is at offset 0x%zx but file"
			" offset 0x%zx is %s by %ld bytes\n",
			tplg->next_hdr_pos, tplg->out_pos,
			tplg->out_pos > tplg->next_hdr_pos ? "ahead" : "behind",
			labs(tplg->out_pos - tplg->next_hdr_pos));
		return -EINVAL;
	}

	verbose(tplg, " header index %d type %d count %d size 0x%lx/%ld vendor %d "
		"version %d\n", index, type, count,
		(long unsigned int)payload_size, (long int)payload_size,
		vendor_type, version);

	tplg->next_hdr_pos += hdr.payload_size + sizeof(hdr);

	return twrite(tplg, &hdr, sizeof(hdr));
}

static int write_elem_block(snd_tplg_t *tplg,
	struct list_head *base, size_t size, int tplg_type, const char *obj_name)
{
	struct list_head *pos, *sub_pos, *sub_base;
	struct tplg_elem *elem, *elem_next;
	size_t total_size = 0, count = 0, block_size = 0;
	ssize_t ret, wsize;

	sub_base = base;
	list_for_each(pos, base) {
		/* find elems with the same index to make a block */
		elem = list_entry(pos, struct tplg_elem, list);

		if (elem->compound_elem)
			continue;

		elem_next = list_entry(pos->next, struct tplg_elem, list);
		block_size += elem->size;
		count++;

		if ((pos->next == base) || (elem_next->index != elem->index)) {
			/* write header for the block */
			ret = write_block_header(tplg, tplg_type, elem->vendor_type,
				tplg->version, elem->index, block_size, count);
			if (ret < 0) {
				SNDERR("error: failed to write %s block %d\n",
					obj_name, ret);
				return ret;
			}

			/* write elems for the block */
			list_for_each(sub_pos, sub_base) {
				elem = list_entry(sub_pos, struct tplg_elem, list);
				/* compound elems have already been copied to other elems */
				if (elem->compound_elem)
					continue;

				if (elem->type != SND_TPLG_TYPE_DAPM_GRAPH)
					verbose(tplg, " %s '%s': write %d bytes\n",
						obj_name, elem->id, elem->size);
				else
					verbose(tplg, " %s '%s -> %s -> %s': write %d bytes\n",
						obj_name, elem->route->source,
						elem->route->control,
						elem->route->sink, elem->size);

				wsize = twrite(tplg, elem->obj, elem->size);
				if (wsize < 0)
					return size;

				total_size += wsize;
				/* get to the end of sub list */
				if (sub_pos == pos)
					break;
			}
			/* the last elem of the current sub list as the head of 
			next sub list*/
			sub_base = pos;
			count = 0;
			block_size = 0;
		}
	}

	/* make sure we have written the correct size */
	if (total_size != size) {
		SNDERR("error: size mismatch. Expected %zu wrote %zu\n",
			size, total_size);
		return -EIO;
	}

	return 0;
}

static size_t calc_block_size(struct list_head *base)
{
	struct list_head *pos;
	struct tplg_elem *elem;
	size_t size = 0;

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);

		/* compound elems have already been copied to other elems */
		if (elem->compound_elem)
			continue;

		size += elem->size;
	}

	return size;
}

static int write_block(snd_tplg_t *tplg, struct list_head *base, int type)
{
	size_t size;

	/* calculate the block size in bytes for all elems in this list */
	size = calc_block_size(base);
	if (size == 0)
		return size;

	verbose(tplg, " block size for type %d is %zd\n", type, size);

	/* write each elem for this block */
	switch (type) {
	case SND_TPLG_TYPE_MIXER:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_MIXER, "mixer");
	case SND_TPLG_TYPE_BYTES:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_BYTES, "bytes");
	case SND_TPLG_TYPE_ENUM:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_ENUM, "enum");
	case SND_TPLG_TYPE_DAPM_GRAPH:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_DAPM_GRAPH, "route");
	case SND_TPLG_TYPE_DAPM_WIDGET:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_DAPM_WIDGET, "widget");
	case SND_TPLG_TYPE_PCM:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_PCM, "pcm");
	case SND_TPLG_TYPE_BE:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_BACKEND_LINK, "be");
	case SND_TPLG_TYPE_CC:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_CODEC_LINK, "cc");
	case SND_TPLG_TYPE_DATA:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_PDATA, "data");
	case SND_TPLG_TYPE_DAI:
		return write_elem_block(tplg, base, size,
			SND_SOC_TPLG_TYPE_DAI, "dai");
	default:
		return -EINVAL;
	}

	return 0;
}

/* write the manifest including its private data */
static ssize_t write_manifest_data(snd_tplg_t *tplg)
{
	ssize_t ret;

	/* write the header for this block */
	ret = write_block_header(tplg, SND_SOC_TPLG_TYPE_MANIFEST, 0,
		tplg->version, 0,
		sizeof(tplg->manifest) + tplg->manifest.priv.size, 1);
	if (ret < 0) {
		SNDERR("error: failed to write manifest block\n");
		return ret;
	}

	verbose(tplg, "manifest : write %d bytes\n", sizeof(tplg->manifest));
	ret = twrite(tplg, &tplg->manifest, sizeof(tplg->manifest));
	if (ret >= 0) {
		verbose(tplg, "manifest : write %d priv bytes\n", tplg->manifest.priv.size);
		ret = twrite(tplg, tplg->manifest_pdata, tplg->manifest.priv.size);
	}
	return ret;
}

int tplg_write_data(snd_tplg_t *tplg)
{
	struct wtable {
		const char *name;
		struct list_head *list;
		int type;
	} *wptr, wtable[] = {
		{
			.name = "control mixer elements",
			.list = &tplg->mixer_list,
			.type = SND_TPLG_TYPE_MIXER,
		},
		{
			.name = "control enum elements",
			.list = &tplg->enum_list,
			.type = SND_TPLG_TYPE_ENUM,
		},
		{
			.name = "control extended (bytes) elements",
			.list = &tplg->bytes_ext_list,
			.type = SND_TPLG_TYPE_BYTES,
		},
		{
			.name = "dapm widget elements",
			.list = &tplg->widget_list,
			.type = SND_TPLG_TYPE_DAPM_WIDGET,
		},
		{
			.name = "pcm elements",
			.list = &tplg->pcm_list,
			.type = SND_TPLG_TYPE_PCM,
		},
		{
			.name = "physical dai elements",
			.list = &tplg->dai_list,
			.type = SND_TPLG_TYPE_DAI,
		},
		{
			.name = "be elements",
			.list = &tplg->be_list,
			.type = SND_TPLG_TYPE_BE,
		},
		{
			.name = "cc elements",
			.list = &tplg->cc_list,
			.type = SND_TPLG_TYPE_CC,
		},
		{
			.name = "route (dapm graph) elements",
			.list = &tplg->route_list,
			.type = SND_TPLG_TYPE_DAPM_GRAPH,
		},
		{
			.name = "private data elements",
			.list = &tplg->pdata_list,
			.type = SND_TPLG_TYPE_DATA,
		},
	};

	ssize_t ret;
	unsigned int index;

	/* write manifest */
	ret = write_manifest_data(tplg);
	if (ret < 0) {
		SNDERR("failed to write manifest %d\n", ret);
		return ret;
	}

	/* write all blocks */
	for (index = 0; index < ARRAY_SIZE(wtable); index++) {
		wptr = &wtable[index];
		ret = write_block(tplg, wptr->list, wptr->type);
		if (ret < 0) {
			SNDERR("failed to write %s: %s\n", wptr->name, snd_strerror(-ret));
			return ret;
		}
	}

	return 0;
}
