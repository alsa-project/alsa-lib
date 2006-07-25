/**
 * \file src/instr/fm.c
 * \brief FM (OPL2/3) Instrument Format Support
 * \author Uros Bizjak <uros@kss-loka.si>
 * \date 2000-2001
 */
/*
 *  FM (OPL2/3) Instrument Format Support
 *  Copyright (c) 2000 Uros Bizjak <uros@kss-loka.si>
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
#include <sound/ainstr_fm.h>

/**
 * \brief Free the FM instrument handle
 * \param fm FM instrument handle
 * \return 0 on success otherwise a negative error code
 */
int snd_instr_fm_free(snd_instr_fm_t *fm)
{
	if (fm == NULL)
		return 0;
	free(fm);
	return 0;
}

/**
 * \brief Convert the FM instrument to byte stream
 * \param fm FM instrument handle
 * \param name FM instrument name
 * \param __data Result - allocated byte stream
 * \param __size Result - size of allocated byte stream
 * \return 0 on success otherwise a negative error code
 */
int snd_instr_fm_convert_to_stream(snd_instr_fm_t *fm,
				   const char *name,
				   snd_instr_header_t **__data,
				   size_t *__size)
{
	snd_instr_header_t *put;
	fm_instrument_t *instr;
	fm_xinstrument_t *xinstr;
	int idx;

	if (fm == NULL || __data == NULL)
		return -EINVAL;
	instr = (fm_instrument_t *)fm;
	*__data = NULL;
	*__size = 0;
	if (snd_instr_header_malloc(&put, sizeof(fm_xinstrument_t)) < 0)
		return -ENOMEM;
	/* build header */
	if (name)
		snd_instr_header_set_name(put, name);
	snd_instr_header_set_type(put, SND_SEQ_INSTR_ATYPE_DATA);
	snd_instr_header_set_format(put, SND_SEQ_INSTR_ID_OPL2_3);
	/* build data section */
	xinstr = (fm_xinstrument_t *)snd_instr_header_get_data(put);
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

/**
 * \brief Convert the byte stream to FM instrument
 * \param __data Input - byte stream containing FM instrument definition
 * \param size Input - size of byte stream
 * \param simple Result - allocated FM instrument handle
 * \return 0 on success otherwise a negative error code
 */
#ifndef DOXYGEN
int snd_instr_fm_convert_from_stream(snd_instr_header_t *__data ATTRIBUTE_UNUSED,
				     size_t size ATTRIBUTE_UNUSED,
				     snd_instr_fm_t **simple ATTRIBUTE_UNUSED)
#else
int snd_instr_fm_convert_from_stream(snd_instr_header_t *__data,
				     size_t size,
				     snd_instr_fm_t **simple)
#endif
{
	/* TODO */
	return -ENXIO;
}
