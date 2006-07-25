/**
 * \file src/instr/simple.c
 * \brief Simple Wave Format Support
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 1999-2001
 */
/*
 *  Simple Wave Format Support
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include "local.h"
#include <sound/ainstr_simple.h>

/**
 * \brief Free simple instrument
 * \param simple Simple instrument handle
 * \return 0 on success otherwise a negative error code
 */
int snd_instr_simple_free(snd_instr_simple_t *simple)
{
	if (simple == NULL)
		return 0;
	free(simple);
	return 0;
}

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

/**
 * \brief Convert the simple instrument to byte stream
 * \param simple Simple instrument handle
 * \param name Simple instrument name
 * \param __data Result - allocated byte stream
 * \param __size Result - size of allocated byte stream
 * \return 0 on success otherwise a negative error code
 */
int snd_instr_simple_convert_to_stream(snd_instr_simple_t *simple,
				       const char *name,
				       snd_instr_header_t **__data,
				       size_t *__size)
{
	snd_instr_header_t *put;
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
	if (snd_instr_header_malloc(&put, sizeof(simple_xinstrument_t) + size) < 0)
		return -ENOMEM;
	/* build header */
	if (name)
		snd_instr_header_set_name(put, name);
	snd_instr_header_set_type(put, SND_SEQ_INSTR_ATYPE_DATA);
	snd_instr_header_set_format(put, SND_SEQ_INSTR_ID_SIMPLE);
	/* build data section */
	xinstr = (simple_xinstrument_t *)snd_instr_header_get_data(put);
	xinstr->stype = SIMPLE_STRU_INSTR;
	xinstr->share_id[0] = __cpu_to_le32(instr->share_id[0]);
	xinstr->share_id[1] = __cpu_to_le32(instr->share_id[1]);
	xinstr->share_id[2] = __cpu_to_le32(instr->share_id[2]);
	xinstr->share_id[3] = __cpu_to_le32(instr->share_id[3]);
	xinstr->format = __cpu_to_le32(instr->format);
	xinstr->size = __cpu_to_le32(instr->size);
	xinstr->start = __cpu_to_le32(instr->start);
	xinstr->loop_start = __cpu_to_le32(instr->loop_start);
	xinstr->loop_end = __cpu_to_le32(instr->loop_end);
	xinstr->loop_repeat = __cpu_to_le16(instr->loop_repeat);
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

/**
 * \brief Convert the byte stream to simple instrument
 * \param __data byte stream
 * \param size size of byte stream
 * \param simple Result - simple instrument handle
 * \return 0 on success otherwise a negative error code
 */
#ifndef DOXYGEN
int snd_instr_simple_convert_from_stream(snd_instr_header_t *__data ATTRIBUTE_UNUSED,
					 size_t size ATTRIBUTE_UNUSED,
					 snd_instr_simple_t **simple ATTRIBUTE_UNUSED)
#else
int snd_instr_simple_convert_from_stream(snd_instr_header_t *__data,
					 size_t size,
					 snd_instr_simple_t **simple)
#endif
{
	/* TODO */
	return -ENXIO;
}
