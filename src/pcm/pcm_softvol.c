/**
 * \file pcm/pcm_softvol.c
 * \ingroup PCM_Plugins
 * \brief PCM Soft Volume Plugin Interface
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 2004
 */
/*
 *  PCM - Soft Volume Plugin
 *  Copyright (c) 2004 by Takashi Iwai <tiwai@suse.de>
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
  
#include <byteswap.h>
#include <math.h>
#include "pcm_local.h"
#include "pcm_plugin.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_softvol = "";
#endif

#ifndef DOC_HIDDEN

typedef struct {
	/* This field need to be the first */
	snd_pcm_plugin_t plug;
	snd_pcm_format_t sformat;
	unsigned int cchannels;
	snd_ctl_t *ctl;
	snd_ctl_elem_value_t elem;
	unsigned int cur_vol[2];
	unsigned int max_val;
	double min_dB;
	unsigned short *dB_value;
} snd_pcm_softvol_t;

#define VOL_SCALE_SHIFT		16

#define PRESET_RESOLUTION	256
#define PRESET_MIN_DB		-48.0

static unsigned short preset_dB_value[PRESET_RESOLUTION] = {
	0x0000, 0x0104, 0x010a, 0x0110, 0x0116, 0x011c, 0x0122, 0x0129,
	0x012f, 0x0136, 0x013d, 0x0144, 0x014b, 0x0152, 0x015a, 0x0161,
	0x0169, 0x0171, 0x0179, 0x0181, 0x018a, 0x0193, 0x019c, 0x01a5,
	0x01ae, 0x01b7, 0x01c1, 0x01cb, 0x01d5, 0x01df, 0x01ea, 0x01f5,
	0x0200, 0x020b, 0x0216, 0x0222, 0x022e, 0x023b, 0x0247, 0x0254,
	0x0261, 0x026e, 0x027c, 0x028a, 0x0298, 0x02a7, 0x02b6, 0x02c5,
	0x02d5, 0x02e5, 0x02f5, 0x0306, 0x0317, 0x0328, 0x033a, 0x034c,
	0x035f, 0x0372, 0x0385, 0x0399, 0x03ad, 0x03c2, 0x03d7, 0x03ed,
	0x0403, 0x041a, 0x0431, 0x0448, 0x0460, 0x0479, 0x0492, 0x04ac,
	0x04c6, 0x04e1, 0x04fd, 0x0519, 0x0535, 0x0553, 0x0571, 0x058f,
	0x05af, 0x05cf, 0x05ef, 0x0611, 0x0633, 0x0656, 0x067a, 0x069e,
	0x06c3, 0x06ea, 0x0710, 0x0738, 0x0761, 0x078a, 0x07b5, 0x07e0,
	0x080d, 0x083a, 0x0868, 0x0898, 0x08c8, 0x08fa, 0x092c, 0x0960,
	0x0995, 0x09cb, 0x0a02, 0x0a3a, 0x0a74, 0x0aae, 0x0aeb, 0x0b28,
	0x0b67, 0x0ba7, 0x0be9, 0x0c2c, 0x0c70, 0x0cb6, 0x0cfe, 0x0d47,
	0x0d92, 0x0dde, 0x0e2d, 0x0e7c, 0x0ece, 0x0f21, 0x0f76, 0x0fce,
	0x1027, 0x1081, 0x10de, 0x113d, 0x119f, 0x1202, 0x1267, 0x12cf,
	0x1339, 0x13a5, 0x1414, 0x1485, 0x14f8, 0x156e, 0x15e7, 0x1662,
	0x16e0, 0x1761, 0x17e5, 0x186b, 0x18f5, 0x1981, 0x1a11, 0x1aa4,
	0x1b3a, 0x1bd3, 0x1c70, 0x1d10, 0x1db4, 0x1e5b, 0x1f06, 0x1fb4,
	0x2067, 0x211d, 0x21d8, 0x2297, 0x2359, 0x2420, 0x24ec, 0x25bc,
	0x2690, 0x2769, 0x2847, 0x292a, 0x2a12, 0x2aff, 0x2bf1, 0x2ce8,
	0x2de5, 0x2ee8, 0x2ff0, 0x30fe, 0x3211, 0x332b, 0x344c, 0x3572,
	0x369f, 0x37d2, 0x390d, 0x3a4e, 0x3b96, 0x3ce6, 0x3e3d, 0x3f9b,
	0x4101, 0x426f, 0x43e6, 0x4564, 0x46eb, 0x487a, 0x4a12, 0x4bb3,
	0x4d5d, 0x4f11, 0x50ce, 0x5295, 0x5466, 0x5642, 0x5827, 0x5a18,
	0x5c13, 0x5e19, 0x602b, 0x6249, 0x6472, 0x66a8, 0x68ea, 0x6b39,
	0x6d94, 0x6ffd, 0x7274, 0x74f8, 0x778b, 0x7a2c, 0x7cdc, 0x7f9b,
	0x826a, 0x8548, 0x8836, 0x8b35, 0x8e45, 0x9166, 0x9499, 0x97de,
	0x9b35, 0x9e9f, 0xa21c, 0xa5ad, 0xa952, 0xad0b, 0xb0da, 0xb4bd,
	0xb8b7, 0xbcc7, 0xc0ee, 0xc52d, 0xc983, 0xcdf1, 0xd279, 0xd71a,
	0xdbd5, 0xe0ab, 0xe59c, 0xeaa9, 0xefd3, 0xf519, 0xfa7d, 0xffff,
};

#endif /* DOC_HIDDEN */

/* (32bit x 16bit) >> 16 */
typedef union {
	int i;
	short s[2];
} val_t;
static inline int MULTI_DIV_int(int a, unsigned short b)
{
	val_t v, x, y;
	v.i = a;
	y.i = 0;
#if __BYTE_ORDER == __LITTLE_ENDIAN
	x.i = (unsigned int)v.s[0] * b;
	y.s[0] = x.s[1];
	y.i += (int)v.s[1] * b;
#else
	x.i = (unsigned int)v.s[1] * b;
	y.s[1] = x.s[0];
	y.i += (int)v.s[0] * b;
#endif
	return y.i;
}

/* (16bit x 16bit) >> 16 */
#define MULTI_DIV_short(src,scale) (((int)(src) * (scale)) >> VOL_SCALE_SHIFT)


/*
 * apply volumue attenuation
 *
 * TODO: use SIMD operations
 */

#define CONVERT_AREA(TYPE) do {	\
	unsigned int ch, fr; \
	TYPE *src, *dst; \
	for (ch = 0; ch < channels; ch++) { \
		src_area = &src_areas[ch]; \
		dst_area = &dst_areas[ch]; \
		src = snd_pcm_channel_area_addr(src_area, src_offset); \
		dst = snd_pcm_channel_area_addr(dst_area, dst_offset); \
		src_step = snd_pcm_channel_area_step(src_area) / sizeof(TYPE); \
		dst_step = snd_pcm_channel_area_step(dst_area) / sizeof(TYPE); \
		GET_VOL_SCALE; \
		fr = frames; \
		if (! vol_scale) { \
			while (fr--) { \
				*dst = 0; \
				dst += dst_step; \
			} \
		} else if (vol_scale == 0xffff) { \
			while (fr--) { \
				*dst = *src; \
				src += src_step; \
				dst += dst_step; \
			} \
		} else { \
			while (fr--) { \
				*dst = MULTI_DIV_##TYPE(*src, vol_scale);\
				src += src_step; \
				dst += dst_step; \
			} \
		} \
	} \
} while (0)
	

		
#define GET_VOL_SCALE \
	switch (ch) { \
	case 0: \
	case 2: \
		vol_scale = (channels == ch + 1) ? vol_c : vol[0]; \
		break; \
	case 4: \
	case 5: \
		vol_scale = vol_c; \
		break; \
	default: \
		vol_scale = vol[ch & 1]; \
		break; \
	}

/* 2-channel stereo control */
static void softvol_convert_stereo_vol(snd_pcm_softvol_t *svol,
				       const snd_pcm_channel_area_t *dst_areas,
				       snd_pcm_uframes_t dst_offset,
				       const snd_pcm_channel_area_t *src_areas,
				       snd_pcm_uframes_t src_offset,
				       unsigned int channels,
				       snd_pcm_uframes_t frames)
{
	const snd_pcm_channel_area_t *dst_area, *src_area;
	unsigned int src_step, dst_step;
	unsigned int vol_scale, vol[2], vol_c;

	if (svol->cur_vol[0] == 0 && svol->cur_vol[1] == 0) {
		snd_pcm_areas_silence(dst_areas, dst_offset, channels, frames,
				      svol->sformat);
		return;
	} else if (svol->cur_vol[0] == svol->max_val &&
		   svol->cur_vol[1] == svol->max_val) {
		snd_pcm_areas_copy(dst_areas, dst_offset, src_areas, src_offset,
				   channels, frames, svol->sformat);
		return;
	}

	vol[0] = svol->dB_value[svol->cur_vol[0]];
	vol[1] = svol->dB_value[svol->cur_vol[1]];
	vol_c = svol->dB_value[(svol->cur_vol[0] + svol->cur_vol[1]) / 2];
	if (svol->sformat == SND_PCM_FORMAT_S16) {
		/* 16bit samples */
		CONVERT_AREA(short);
	} else {
		/* 32bit samples */
		CONVERT_AREA(int);
	}
}

#undef GET_VOL_SCALE
#define GET_VOL_SCALE

/* mono control */
static void softvol_convert_mono_vol(snd_pcm_softvol_t *svol,
				     const snd_pcm_channel_area_t *dst_areas,
				     snd_pcm_uframes_t dst_offset,
				     const snd_pcm_channel_area_t *src_areas,
				     snd_pcm_uframes_t src_offset,
				     unsigned int channels,
				     snd_pcm_uframes_t frames)
{
	const snd_pcm_channel_area_t *dst_area, *src_area;
	unsigned int src_step, dst_step;
	unsigned int vol_scale;

	if (svol->cur_vol[0] == 0) {
		snd_pcm_areas_silence(dst_areas, dst_offset, channels, frames,
				      svol->sformat);
		return;
	} else if (svol->cur_vol[0] == svol->max_val) {
		snd_pcm_areas_copy(dst_areas, dst_offset, src_areas, src_offset,
				   channels, frames, svol->sformat);
		return;
	}

	vol_scale = svol->dB_value[svol->cur_vol[0]];
	if (svol->sformat == SND_PCM_FORMAT_S16) {
		/* 16bit samples */
		CONVERT_AREA(short);
	} else {
		/* 32bit samples */
		CONVERT_AREA(int);
	}
}

/*
 * get the current volume value from driver
 *
 * TODO: mmap support?
 */
static void get_current_volume(snd_pcm_softvol_t *svol)
{
	unsigned int val;
	unsigned int i;

	if (snd_ctl_elem_read(svol->ctl, &svol->elem) < 0)
		return;
	for (i = 0; i < svol->cchannels; i++) {
		val = svol->elem.value.integer.value[i];
		if (val > svol->max_val)
			val = svol->max_val;
		svol->cur_vol[i] = val;
	}
}

static void softvol_free(snd_pcm_softvol_t *svol)
{
	if (svol->plug.close_slave)
		snd_pcm_close(svol->plug.slave);
	if (svol->ctl)
		snd_ctl_close(svol->ctl);
	if (svol->dB_value && svol->dB_value != preset_dB_value)
		free(svol->dB_value);
	free(svol);
}

static int snd_pcm_softvol_close(snd_pcm_t *pcm)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	softvol_free(svol);
	return 0;
}

static int snd_pcm_softvol_hw_refine_cprepare(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
					      snd_pcm_hw_params_t *params)
{
	int err;
	snd_pcm_access_mask_t access_mask = { SND_PCM_ACCBIT_SHM };
	snd_pcm_format_mask_t format_mask = { SND_PCM_FMTBIT_LINEAR };
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_ACCESS,
					 &access_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set_mask(params, SND_PCM_HW_PARAM_FORMAT,
					 &format_mask);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_params_set_subformat(params, SND_PCM_SUBFORMAT_STD);
	if (err < 0)
		return err;
	err = _snd_pcm_hw_param_set_min(params, SND_PCM_HW_PARAM_CHANNELS, 1, 0);
	if (err < 0)
		return err;
	params->info &= ~(SND_PCM_INFO_MMAP | SND_PCM_INFO_MMAP_VALID);
	return 0;
}

static int snd_pcm_softvol_hw_refine_sprepare(snd_pcm_t *pcm, snd_pcm_hw_params_t *sparams)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	snd_pcm_access_mask_t saccess_mask = { SND_PCM_ACCBIT_MMAP };
	_snd_pcm_hw_params_any(sparams);
	_snd_pcm_hw_param_set_mask(sparams, SND_PCM_HW_PARAM_ACCESS,
				   &saccess_mask);
	if (svol->sformat != SND_PCM_FORMAT_UNKNOWN) {
		_snd_pcm_hw_params_set_format(sparams, svol->sformat);
		_snd_pcm_hw_params_set_subformat(sparams, SND_PCM_SUBFORMAT_STD);
	}
	return 0;
}

static int snd_pcm_softvol_hw_refine_schange(snd_pcm_t *pcm,
					     snd_pcm_hw_params_t *params,
					     snd_pcm_hw_params_t *sparams)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	if (svol->sformat == SND_PCM_FORMAT_UNKNOWN)
		links |= (SND_PCM_HW_PARBIT_FORMAT | 
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS);
	err = _snd_pcm_hw_params_refine(sparams, links, params);
	if (err < 0)
		return err;
	return 0;
}
	
static int snd_pcm_softvol_hw_refine_cchange(snd_pcm_t *pcm,
					     snd_pcm_hw_params_t *params,
					    snd_pcm_hw_params_t *sparams)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	int err;
	unsigned int links = (SND_PCM_HW_PARBIT_CHANNELS |
			      SND_PCM_HW_PARBIT_RATE |
			      SND_PCM_HW_PARBIT_PERIODS |
			      SND_PCM_HW_PARBIT_PERIOD_SIZE |
			      SND_PCM_HW_PARBIT_PERIOD_TIME |
			      SND_PCM_HW_PARBIT_BUFFER_SIZE |
			      SND_PCM_HW_PARBIT_BUFFER_TIME |
			      SND_PCM_HW_PARBIT_TICK_TIME);
	if (svol->sformat == SND_PCM_FORMAT_UNKNOWN)
		links |= (SND_PCM_HW_PARBIT_FORMAT | 
			  SND_PCM_HW_PARBIT_SUBFORMAT |
			  SND_PCM_HW_PARBIT_SAMPLE_BITS);
	err = _snd_pcm_hw_params_refine(params, links, sparams);
	if (err < 0)
		return err;
	return 0;
}

static int snd_pcm_softvol_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	return snd_pcm_hw_refine_slave(pcm, params,
				       snd_pcm_softvol_hw_refine_cprepare,
				       snd_pcm_softvol_hw_refine_cchange,
				       snd_pcm_softvol_hw_refine_sprepare,
				       snd_pcm_softvol_hw_refine_schange,
				       snd_pcm_plugin_hw_refine_slave);
}

static int snd_pcm_softvol_hw_params(snd_pcm_t *pcm, snd_pcm_hw_params_t * params)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	snd_pcm_t *slave = svol->plug.slave;
	int err = snd_pcm_hw_params_slave(pcm, params,
					  snd_pcm_softvol_hw_refine_cchange,
					  snd_pcm_softvol_hw_refine_sprepare,
					  snd_pcm_softvol_hw_refine_schange,
					  snd_pcm_plugin_hw_params_slave);
	if (err < 0)
		return err;
	if (slave->format != SND_PCM_FORMAT_S16 &&
	    slave->format != SND_PCM_FORMAT_S32) {
		SNDERR("softvol supports only S16 or S32");
		return -EINVAL;
	}
	svol->sformat = slave->format;
	return 0;
}

static snd_pcm_uframes_t
snd_pcm_softvol_write_areas(snd_pcm_t *pcm,
			    const snd_pcm_channel_area_t *areas,
			    snd_pcm_uframes_t offset,
			    snd_pcm_uframes_t size,
			    const snd_pcm_channel_area_t *slave_areas,
			    snd_pcm_uframes_t slave_offset,
			    snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	if (size > *slave_sizep)
		size = *slave_sizep;
	get_current_volume(svol);
	if (svol->cchannels == 1)
		softvol_convert_mono_vol(svol, slave_areas, slave_offset,
					 areas, offset, pcm->channels, size);
	else
		softvol_convert_stereo_vol(svol, slave_areas, slave_offset,
					   areas, offset, pcm->channels, size);
	*slave_sizep = size;
	return size;
}

static snd_pcm_uframes_t
snd_pcm_softvol_read_areas(snd_pcm_t *pcm,
			   const snd_pcm_channel_area_t *areas,
			   snd_pcm_uframes_t offset,
			   snd_pcm_uframes_t size,
			   const snd_pcm_channel_area_t *slave_areas,
			   snd_pcm_uframes_t slave_offset,
			   snd_pcm_uframes_t *slave_sizep)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	if (size > *slave_sizep)
		size = *slave_sizep;
	get_current_volume(svol);
	if (svol->cchannels == 1)
		softvol_convert_mono_vol(svol, areas, offset, slave_areas,
					 slave_offset, pcm->channels, size);
	else
		softvol_convert_stereo_vol(svol, areas, offset, slave_areas,
					   slave_offset, pcm->channels, size);
	*slave_sizep = size;
	return size;
}

static void snd_pcm_softvol_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	snd_pcm_softvol_t *svol = pcm->private_data;
	snd_output_printf(out, "Soft volume PCM\n");
	snd_output_printf(out, "Control: %s\n", svol->elem.id.name);
	snd_output_printf(out, "min_dB: %g\n", svol->min_dB);
	snd_output_printf(out, "resolution: %d\n", svol->max_val + 1);
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
	snd_output_printf(out, "Slave: ");
	snd_pcm_dump(svol->plug.slave, out);
}

static int add_user_ctl(snd_pcm_softvol_t *svol, snd_ctl_elem_info_t *cinfo, int count)
{
	int err;
	int i;

	err = snd_ctl_elem_add_integer(svol->ctl, &cinfo->id, count, 0, svol->max_val, 0);
	if (err < 0)
		return err;
	/* set max value as default */
	for (i = 0; i < count; i++)
		svol->elem.value.integer.value[i] = svol->max_val;
	return snd_ctl_elem_write(svol->ctl, &svol->elem);
}

/*
 * load and set up user-control
 * returns 0 if the user-control is found or created,
 * returns 1 if the control is a hw control,
 * or a negative error code
 */
static int softvol_load_control(snd_pcm_t *pcm, snd_pcm_softvol_t *svol,
				int ctl_card, snd_ctl_elem_id_t *ctl_id,
				int cchannels, double min_dB, int resolution)
{
	char tmp_name[32];
	snd_pcm_info_t *info;
	snd_ctl_elem_info_t *cinfo;
	int err;
	unsigned int i;

	if (ctl_card < 0) {
		snd_pcm_info_alloca(&info);
		err = snd_pcm_info(pcm, info);
		if (err < 0)
			return err;
		ctl_card = snd_pcm_info_get_card(info);
		if (ctl_card < 0) {
			SNDERR("No card defined for softvol control");
			return -EINVAL;
		}
	}
	sprintf(tmp_name, "hw:%d", ctl_card);
	err = snd_ctl_open(&svol->ctl, tmp_name, 0);
	if (err < 0) {
		SNDERR("Cannot open CTL %s", tmp_name);
		return err;
	}

	svol->elem.id = *ctl_id;
	svol->max_val = resolution - 1;
	svol->min_dB = min_dB;

	snd_ctl_elem_info_alloca(&cinfo);
	snd_ctl_elem_info_set_id(cinfo, ctl_id);
	if ((err = snd_ctl_elem_info(svol->ctl, cinfo)) < 0) {
		if (err != -ENOENT) {
			SNDERR("Cannot get info for CTL %s", tmp_name);
			return err;
		}
		err = add_user_ctl(svol, cinfo, cchannels);
		if (err < 0) {
			SNDERR("Cannot add a control");
			return err;
		}
	} else {
		if (! (cinfo->access & SNDRV_CTL_ELEM_ACCESS_USER)) {
			/* hardware control exists */
			return 1; /* notify */

		} else if (cinfo->type != SND_CTL_ELEM_TYPE_INTEGER ||
			   cinfo->count != (unsigned int)cchannels ||
			   cinfo->value.integer.min != 0 ||
			   cinfo->value.integer.max != resolution - 1) {
			if ((err = snd_ctl_elem_remove(svol->ctl, &cinfo->id)) < 0) {
				SNDERR("Control %s mismatch", tmp_name);
				return err;
			}
			snd_ctl_elem_info_set_id(cinfo, ctl_id); /* reset numid */
			if ((err = add_user_ctl(svol, cinfo, cchannels)) < 0) {
				SNDERR("Cannot add a control");
				return err;
			}
		}
	}

	if (min_dB == PRESET_MIN_DB && resolution == PRESET_RESOLUTION)
		svol->dB_value = preset_dB_value;
	else {
#ifndef HAVE_SOFT_FLOAT
		svol->dB_value = calloc(resolution, sizeof(unsigned short));
		if (! svol->dB_value) {
			SNDERR("cannot allocate dB table");
			return -ENOMEM;
		}
		svol->min_dB = min_dB;
		for (i = 1; i < svol->max_val; i++) {
			double db = svol->min_dB - ((i - 1) * svol->min_dB) / (svol->max_val - 1);
			double v = (pow(10.0, db / 20.0) * (double)(1 << VOL_SCALE_SHIFT));
			svol->dB_value[i] = (unsigned short)v;
		}
		svol->dB_value[svol->max_val] = 65535;
#else
		SNDERR("Cannot handle the given min_dB and resolution");
		return -EINVAL;
#endif
	}
	return 0;
}

static snd_pcm_ops_t snd_pcm_softvol_ops = {
	.close = snd_pcm_softvol_close,
	.info = snd_pcm_plugin_info,
	.hw_refine = snd_pcm_softvol_hw_refine,
	.hw_params = snd_pcm_softvol_hw_params,
	.hw_free = snd_pcm_plugin_hw_free,
	.sw_params = snd_pcm_plugin_sw_params,
	.channel_info = snd_pcm_plugin_channel_info,
	.dump = snd_pcm_softvol_dump,
	.nonblock = snd_pcm_plugin_nonblock,
	.async = snd_pcm_plugin_async,
	.poll_revents = snd_pcm_plugin_poll_revents,
	.mmap = snd_pcm_plugin_mmap,
	.munmap = snd_pcm_plugin_munmap,
};

/**
 * \brief Creates a new SoftVolume PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param sformat Slave format
 * \param card card index of the control
 * \param min_dB minimal dB value
 * \param resolution resolution of control
 * \param slave Slave PCM handle
 * \param close_slave When set, the slave PCM handle is closed with copy PCM
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_softvol_open(snd_pcm_t **pcmp, const char *name,
			 snd_pcm_format_t sformat,
			 int ctl_card, snd_ctl_elem_id_t *ctl_id,
			 int cchannels,
			 double min_dB, int resolution,
			 snd_pcm_t *slave, int close_slave)
{
	snd_pcm_t *pcm;
	snd_pcm_softvol_t *svol;
	int err;
	assert(pcmp && slave);
	if (sformat != SND_PCM_FORMAT_UNKNOWN &&
	    sformat != SND_PCM_FORMAT_S16 &&
	    sformat != SND_PCM_FORMAT_S32)
		return -EINVAL;
	svol = calloc(1, sizeof(*svol));
	if (! svol)
		return -ENOMEM;
	err = softvol_load_control(slave, svol, ctl_card, ctl_id, cchannels,
				   min_dB, resolution);
	if (err < 0) {
		softvol_free(svol);
		return err;
	}
	if (err > 0) { /* hardware control - no need for softvol! */
		softvol_free(svol);
		*pcmp = slave; /* just pass the slave */
		return 0;
	}

	/* do softvol */
	snd_pcm_plugin_init(&svol->plug);
	svol->sformat = sformat;
	svol->cchannels = cchannels;
	svol->plug.read = snd_pcm_softvol_read_areas;
	svol->plug.write = snd_pcm_softvol_write_areas;
	svol->plug.undo_read = snd_pcm_plugin_undo_read_generic;
	svol->plug.undo_write = snd_pcm_plugin_undo_write_generic;
	svol->plug.slave = slave;
	svol->plug.close_slave = close_slave;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_SOFTVOL, name, slave->stream, slave->mode);
	if (err < 0) {
		softvol_free(svol);
		return err;
	}
	pcm->ops = &snd_pcm_softvol_ops;
	pcm->fast_ops = &snd_pcm_plugin_fast_ops;
	pcm->private_data = svol;
	pcm->poll_fd = slave->poll_fd;
	pcm->poll_events = slave->poll_events;
	snd_pcm_set_hw_ptr(pcm, &svol->plug.hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &svol->plug.appl_ptr, -1, 0);
	*pcmp = pcm;

	return 0;
}

/*
 * parse card index and id for the softvol control
 */
static int parse_control_id(snd_config_t *conf, snd_ctl_elem_id_t *ctl_id, int *cardp,
			    int *cchannelsp)
{
	snd_config_iterator_t i, next;
	int iface = SND_CTL_ELEM_IFACE_MIXER;
	const char *name = NULL;
	long index = 0;
	long device = -1;
	long subdevice = -1;
	int err;

	*cardp = -1;
	*cchannelsp = 2;
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (strcmp(id, "comment") == 0)
			continue;
		if (strcmp(id, "card") == 0) {
			long v;
			if ((err = snd_config_get_integer(n, &v)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			*cardp = v;
			continue;
		}
		if (strcmp(id, "iface") == 0 || strcmp(id, "interface") == 0) {
			const char *ptr;
			if ((err = snd_config_get_string(n, &ptr)) < 0) {
				SNDERR("field %s is not a string", id);
				goto _err;
			}
			if ((err = snd_config_get_ctl_iface_ascii(ptr)) < 0) {
				SNDERR("Invalid value for '%s'", id);
				goto _err;
			}
			iface = err;
			continue;
		}
		if (strcmp(id, "name") == 0) {
			if ((err = snd_config_get_string(n, &name)) < 0) {
				SNDERR("field %s is not a string", id);
				goto _err;
			}
			continue;
		}
		if (strcmp(id, "index") == 0) {
			if ((err = snd_config_get_integer(n, &index)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			continue;
		}
		if (strcmp(id, "device") == 0) {
			if ((err = snd_config_get_integer(n, &device)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			continue;
		}
		if (strcmp(id, "subdevice") == 0) {
			if ((err = snd_config_get_integer(n, &subdevice)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			continue;
		}
		if (strcmp(id, "count") == 0) {
			long v;
			if ((err = snd_config_get_integer(n, &v)) < 0) {
				SNDERR("field %s is not an integer", id);
				goto _err;
			}
			if (v < 1 || v > 2) {
				SNDERR("Invalid count %ld", v);
				goto _err;
			}
			*cchannelsp = v;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (name == NULL) {
		SNDERR("Missing control name");
		err = -EINVAL;
		goto _err;
	}
	if (device < 0)
		device = 0;
	if (subdevice < 0)
		subdevice = 0;

	snd_ctl_elem_id_set_interface(ctl_id, iface);
	snd_ctl_elem_id_set_name(ctl_id, name);
	snd_ctl_elem_id_set_index(ctl_id, index);
	snd_ctl_elem_id_set_device(ctl_id, device);
	snd_ctl_elem_id_set_subdevice(ctl_id, subdevice);

	return 0;

 _err:
	return err;
}

/*! \page pcm_plugins

\section pcm_plugins_softvol Plugin: Soft Volume

This plugin applies the software volume attenuation.
The format, rate and channels must match for both of source and destination.

When the control is stereo (count=2), the channels are assumed to be either
mono, 2.0, 2.1, 4.0, 4.1, 5.1 or 7.1.

If the control already exists and it's a system control (i.e. no
user-defined control), the plugin simply passes its slave without
any changes.

\code
pcm.name {
        type softvol            # Soft Volume conversion PCM
        slave STR               # Slave name
        # or
        slave {                 # Slave definition
                pcm STR         # Slave PCM name
                # or
                pcm { }         # Slave PCM definition
                [format STR]    # Slave format
        }
        control {
	        name STR        # control element id string
		[card INT]      # control card index
		[iface STR]     # interface of the element
		[index INT]     # index of the element
		[device INT]    # device number of the element
		[subdevice INT] # subdevice number of the element
		[count INT]     # control channels 1 or 2 (default: 2)
	}
	[min_dB REAL]           # minimal dB value (default: -48.0)
	[resolution INT]        # resolution (default: 256)
}
\endcode

\subsection pcm_plugins_softvol_funcref Function reference

<UL>
  <LI>snd_pcm_softvol_open()
  <LI>_snd_pcm_softvol_open()
</UL>

*/

/**
 * \brief Creates a new Soft Volume PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with Soft Volume PCM description
 * \param stream Stream type
 * \param mode Stream mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_softvol_open(snd_pcm_t **pcmp, const char *name,
			  snd_config_t *root, snd_config_t *conf, 
			  snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	int err;
	snd_pcm_t *spcm;
	snd_config_t *slave = NULL, *sconf;
	snd_config_t *control = NULL;
	snd_pcm_format_t sformat = SND_PCM_FORMAT_UNKNOWN;
	snd_ctl_elem_id_t *ctl_id;
	int resolution = PRESET_RESOLUTION;
	double min_dB = PRESET_MIN_DB;
	int card = -1, cchannels = 2;

	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "slave") == 0) {
			slave = n;
			continue;
		}
		if (strcmp(id, "control") == 0) {
			control = n;
			continue;
		}
		if (strcmp(id, "resolution") == 0) {
			long v;
			err = snd_config_get_integer(n, &v);
			if (err < 0) {
				SNDERR("Invalid resolution value");
				return err;
			}
			resolution = v;
			continue;
		}
		if (strcmp(id, "min_dB") == 0) {
			err = snd_config_get_real(n, &min_dB);
			if (err < 0) {
				SNDERR("Invalid min_dB value");
				return err;
			}
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!slave) {
		SNDERR("slave is not defined");
		return -EINVAL;
	}
	if (!control) {
		SNDERR("control is not defined");
		return -EINVAL;
	}
	if (min_dB >= 0) {
		SNDERR("min_dB must be a negative value");
		return -EINVAL;
	}
	if (resolution < 0 || resolution > 1024) {
		SNDERR("Invalid resolution value %d", resolution);
		return -EINVAL;
	}
	err = snd_pcm_slave_conf(root, slave, &sconf, 1,
				 SND_PCM_HW_PARAM_FORMAT, 0, &sformat);
	if (err < 0)
		return err;
	if (sformat != SND_PCM_FORMAT_UNKNOWN &&
	    sformat != SND_PCM_FORMAT_S16 &&
	    sformat != SND_PCM_FORMAT_S32) {
		SNDERR("only S16 or S32 format is supported");
		snd_config_delete(sconf);
		return -EINVAL;
	}
	err = snd_pcm_open_slave(&spcm, root, sconf, stream, mode);
	snd_config_delete(sconf);
	if (err < 0)
		return err;
	snd_ctl_elem_id_alloca(&ctl_id);
	if ((err = parse_control_id(control, ctl_id, &card, &cchannels)) < 0) {
		snd_pcm_close(spcm);
		return err;
	}
	err = snd_pcm_softvol_open(pcmp, name, sformat, card, ctl_id, cchannels,
				   min_dB, resolution, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_softvol_open, SND_PCM_DLSYM_VERSION);
#endif
