/*
 *  FM (OPL2/3) Instrument Format Support
 *  Copyright (c) 2000 Uros Bizjak <uros@kss-loka.si>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include "local.h"
#include <asm/byteorder.h>
#include <sound/ainstr_fm.h>

int snd_instr_fm_free(snd_instr_simple_t *fm)
{
	if (fm == NULL)
		return 0;
	free(fm);
	return 0;
}

int snd_instr_fm_convert_to_stream(snd_instr_fm_t *fm,
				   const char *name,
				   snd_seq_instr_put_t **__data,
				   size_t *__size)
{
	snd_seq_instr_put_t *put;
	snd_seq_instr_data_t *data;
	fm_instrument_t *instr;
	fm_xinstrument_t *xinstr;
	int idx;

	if (fm == NULL || __data == NULL)
		return -EINVAL;
	instr = (fm_instrument_t *)fm;
	*__data = NULL;
	*__size = 0;
	put = (snd_seq_instr_put_t *)malloc(sizeof(*put) + sizeof(fm_xinstrument_t));
	if (put == NULL)
		return -ENOMEM;
	/* build header */
	memset(put, 0, sizeof(*put));
	data = &put->data;
	if (name)
		strncpy(data->name, name, sizeof(data->name)-1);
	data->type = SND_SEQ_INSTR_ATYPE_DATA;
	strcpy(data->data.format, SND_SEQ_INSTR_ID_OPL2_3);
	/* build data section */
	xinstr = (fm_xinstrument_t *)(data + 1);
	xinstr->stype = FM_STRU_INSTR;
	xinstr->share_id[0] = __cpu_to_le32(instr->share_id[0]);
	xinstr->share_id[1] = __cpu_to_le32(instr->share_id[1]);
	xinstr->share_id[2] = __cpu_to_le32(instr->share_id[2]);
	xinstr->share_id[3] = __cpu_to_le32(instr->share_id[3]);
	xinstr->type = instr->type;
	for (idx = 0; idx < 4; idx++) {
		xinstr->op[idx].am_vib = instr->op[idx].am_vib;
		xinstr->op[idx].ksl_level = instr->op[idx].ksl_level;
		xinstr->op[idx].attack_decay = instr->op[idx].attack_decay;
		xinstr->op[idx].sustain_release = instr->op[idx].sustain_release;
		xinstr->op[idx].wave_select = instr->op[idx].wave_select;
	}
	for (idx = 0; idx < 2; idx++) {
		xinstr->feedback_connection[idx] = instr->feedback_connection[idx];
	}
	xinstr->echo_delay = instr->echo_delay;
	xinstr->echo_atten = instr->echo_atten;
	xinstr->chorus_spread = instr->chorus_spread;
	xinstr->trnsps = instr->trnsps;
	xinstr->fix_dur = instr->fix_dur;
	xinstr->modes = instr->modes;
	xinstr->fix_key = instr->fix_key;

	/* write result */
	*__data = put;
	*__size = sizeof(*put) + sizeof(fm_xinstrument_t);
	return 0;
}

int snd_instr_fm_convert_from_stream(snd_seq_instr_get_t *__data ATTRIBUTE_UNUSED,
				     size_t size ATTRIBUTE_UNUSED,
				     snd_instr_fm_t **simple ATTRIBUTE_UNUSED)
{
	/* TODO */
	return -ENXIO;
}
