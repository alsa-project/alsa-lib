/*
 *  Simple Wave Format Support
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include "asoundlib.h"
#include <linux/ainstr_simple.h>

static long simple_size(simple_instrument_t *instr)
{
	int size;

	size = instr->size;
	if (instr->format & SIMPLE_WAVE_16BIT)
		size <<= 1;
	if (instr->format & SIMPLE_WAVE_STEREO)
		size <<= 1;
	return size;
}

int snd_instr_simple_convert_to_stream(snd_instr_simple_t *simple,
				       const char *name,
				       snd_seq_instr_put_t **__data,
				       size_t *__size)
{
	snd_seq_instr_put_t *put;
	snd_seq_instr_data_t *data;
	int size;
	char *ptr;
	simple_instrument_t *instr;
	simple_xinstrument_t *xinstr;
	
	if (simple == NULL || __data == NULL)
		return -EINVAL;
	instr = (simple_instrument_t *)simple;
	*__data = NULL;
	*__size = 0;
	size = simple_size(simple);
	put = (snd_seq_instr_put_t *)malloc(sizeof(*put) + sizeof(simple_xinstrument_t) + size);
	if (put == NULL)
		return -ENOMEM;
	/* build header */
	bzero(put, sizeof(*put));
	data = &put->data;
	if (name)
		strncpy(data->name, name, sizeof(data->name)-1);
	data->type = SND_SEQ_INSTR_ATYPE_DATA;
	strcpy(data->data.format, SND_SEQ_INSTR_ID_SIMPLE);
	/* build data section */
	xinstr = (simple_xinstrument_t *)(data + 1);
	xinstr->stype = SIMPLE_STRU_INSTR;
	xinstr->share_id[0] = snd_htoi_32(instr->share_id[0]);
	xinstr->share_id[1] = snd_htoi_32(instr->share_id[1]);
	xinstr->share_id[2] = snd_htoi_32(instr->share_id[2]);
	xinstr->share_id[3] = snd_htoi_32(instr->share_id[3]);
	xinstr->format = snd_htoi_32(instr->format);
	xinstr->size = snd_htoi_32(instr->size);
	xinstr->start = snd_htoi_32(instr->start);
	xinstr->loop_start = snd_htoi_32(instr->loop_start);
	xinstr->loop_end = snd_htoi_32(instr->loop_end);
	xinstr->loop_repeat = snd_htoi_16(instr->loop_repeat);
	xinstr->effect1 = instr->effect1;
	xinstr->effect1_depth = instr->effect1_depth;
	xinstr->effect2 = instr->effect2;
	xinstr->effect2_depth = instr->effect2_depth;
	ptr = (char *)(xinstr + 1);
	memcpy(ptr, instr->address.ptr, size);
	/* write result */
	*__data = put;
	*__size = sizeof(*put) + sizeof(simple_xinstrument_t) + size;
	return 0;
}

int snd_instr_simple_convert_from_stream(snd_seq_instr_get_t *__data UNUSED,
					 size_t size UNUSED,
					 snd_instr_simple_t **simple UNUSED)
{
	/* TODO */
	return -ENXIO;
}
