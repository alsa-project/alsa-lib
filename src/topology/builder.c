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

/* write a block, track the position */
static ssize_t twrite(snd_tplg_t *tplg, void *data, size_t data_size)
{
	if (tplg->bin_pos + data_size > tplg->bin_size)
		return -EIO;
	memcpy(tplg->bin + tplg->bin_pos, data, data_size);
	tplg->bin_pos += data_size;
	return data_size;
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
	if (tplg->bin_pos != tplg->next_hdr_pos) {
		SNDERR("New header is at offset 0x%zx but file"
			" offset 0x%zx is %s by %ld bytes",
			tplg->next_hdr_pos, tplg->bin_pos,
			tplg->bin_pos > tplg->next_hdr_pos ? "ahead" : "behind",
			tplg->bin_pos - tplg->next_hdr_pos);
		return -EINVAL;
	}

	tplg_log(tplg, 'B', tplg->bin_pos,
		 "header index %d type %d count %d size 0x%lx/%ld vendor %d "
		 "version %d", index, type, count,
		 (long unsigned int)payload_size, (long int)payload_size,
		 vendor_type, version);

	tplg->next_hdr_pos += hdr.payload_size + sizeof(hdr);

	return twrite(tplg, &hdr, sizeof(hdr));
}

static int write_elem_block(snd_tplg_t *tplg,
			    struct list_head *base, size_t size,
			    int tplg_type, const char *obj_name)
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
				SNDERR("failed to write %s block %d",
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
					tplg_log(tplg, 'B', tplg->bin_pos,
						 "%s '%s': write %d bytes",
						 obj_name, elem->id, elem->size);
				else
					tplg_log(tplg, 'B', tplg->bin_pos,
						 "%s '%s -> %s -> %s': write %d bytes",
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
		SNDERR("size mismatch. Expected %zu wrote %zu",
			size, total_size);
		return -EIO;
	}

	return 0;
}

static size_t calc_manifest_size(snd_tplg_t *tplg)
{
	return sizeof(struct snd_soc_tplg_hdr) +
	       sizeof(tplg->manifest) +
	       tplg->manifest.priv.size;
}

static size_t calc_real_size(struct list_head *base)
{
	struct list_head *pos;
	struct tplg_elem *elem, *elem_next;
	size_t size = 0;

	list_for_each(pos, base) {

		elem = list_entry(pos, struct tplg_elem, list);

		/* compound elems have already been copied to other elems */
		if (elem->compound_elem)
			continue;

		if (elem->size <= 0)
			continue;

		size += elem->size;

		elem_next = list_entry(pos->next, struct tplg_elem, list);

		if ((pos->next == base) || (elem_next->index != elem->index))
			size += sizeof(struct snd_soc_tplg_hdr);
	}

	return size;
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

/* write the manifest including its private data */
static ssize_t write_manifest_data(snd_tplg_t *tplg)
{
	ssize_t ret;

	/* write the header for this block */
	ret = write_block_header(tplg, SND_SOC_TPLG_TYPE_MANIFEST, 0,
		tplg->version, 0,
		sizeof(tplg->manifest) + tplg->manifest.priv.size, 1);
	if (ret < 0) {
		SNDERR("failed to write manifest block");
		return ret;
	}

	tplg_log(tplg, 'B', tplg->bin_pos, "manifest: write %d bytes",
		 sizeof(tplg->manifest));
	ret = twrite(tplg, &tplg->manifest, sizeof(tplg->manifest));
	if (ret >= 0) {
		tplg_log(tplg, 'B', tplg->bin_pos,
			 "manifest: write %d priv bytes",
			 tplg->manifest.priv.size);
		ret = twrite(tplg, tplg->manifest_pdata, tplg->manifest.priv.size);
	}
	return ret;
}

int tplg_write_data(snd_tplg_t *tplg)
{
	struct tplg_table *tptr;
	struct list_head *list;
	ssize_t ret;
	size_t total_size, size;
	unsigned int index;

	/* calculate total size */
	total_size = calc_manifest_size(tplg);
	for (index = 0; index < tplg_table_items; index++) {
		tptr = &tplg_table[index];
		if (!tptr->build)
			continue;
		list = (struct list_head *)((void *)tplg + tptr->loff);
		size = calc_real_size(list);
		total_size += size;
	}

	/* allocate new binary output */
	free(tplg->bin);
	tplg->bin = malloc(total_size);
	tplg->bin_pos = 0;
	tplg->bin_size = total_size;
	if (tplg->bin == NULL) {
		tplg->bin_size = 0;
		return -ENOMEM;
	}

	/* write manifest */
	ret = write_manifest_data(tplg);
	if (ret < 0) {
		SNDERR("failed to write manifest %d", ret);
		return ret;
	}

	/* write all blocks */
	for (index = 0; index < tplg_table_items; index++) {
		tptr = &tplg_table[index];
		if (!tptr->build)
			continue;
		list = (struct list_head *)((void *)tplg + tptr->loff);
		/* calculate the block size in bytes for all elems in this list */
		size = calc_block_size(list);
		if (size == 0)
			continue;
		tplg_log(tplg, 'B', tplg->bin_pos,
			 "block size for type %s (%d:%d) is 0x%zx/%zd",
			 tptr->name, tptr->type,
			 tptr->tsoc, size, size);
		ret = write_elem_block(tplg, list, size,
				       tptr->tsoc, tptr->name);
		if (ret < 0) {
			SNDERR("failed to write %s elements: %s",
						tptr->name, snd_strerror(-ret));
			return ret;
		}
	}

	tplg_log(tplg, 'B', tplg->bin_pos, "total size is 0x%zx/%zd",
		 tplg->bin_pos, tplg->bin_pos);

	if (total_size != tplg->bin_pos) {
		SNDERR("total size mismatch (%zd != %zd)",
		       total_size, tplg->bin_pos);
		return -EINVAL;
	}

	return 0;
}
