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
	snd_ctl_t *ctl;
	snd_ctl_elem_value_t elem;
	unsigned int max_val;
	double min_dB;
	unsigned short *dB_value;
} snd_pcm_softvol_t;

#define VOL_SCALE_SHIFT		16

#define PRESET_RESOLUTION	256
#define PRESET_MIN_DB		-48.0

static unsigned short preset_dB_value[PRESET_RESOLUTION] = {
	0x0000, 0x0930, 0x094f, 0x096e, 0x098e, 0x09af, 0x09cf, 0x09f0,
	0x0a12, 0x0a34, 0x0a56, 0x0a79, 0x0a9d, 0x0ac0, 0x0ae5, 0x0b0a,
	0x0b2f, 0x0b55, 0x0b7b, 0x0ba2, 0x0bc9, 0x0bf1, 0x0c19, 0x0c42,
	0x0c6b, 0x0c95, 0x0cc0, 0x0ceb, 0x0d16, 0x0d42, 0x0d6f, 0x0d9c,
	0x0dca, 0x0df9, 0x0e28, 0x0e58, 0x0e88, 0x0eb9, 0x0eeb, 0x0f1d,
	0x0f51, 0x0f84, 0x0fb9, 0x0fee, 0x1023, 0x105a, 0x1091, 0x10c9,
	0x1102, 0x113b, 0x1175, 0x11b0, 0x11ec, 0x1228, 0x1266, 0x12a4,
	0x12e3, 0x1322, 0x1363, 0x13a5, 0x13e7, 0x142a, 0x146e, 0x14b3,
	0x14f9, 0x1540, 0x1587, 0x15d0, 0x161a, 0x1664, 0x16b0, 0x16fd,
	0x174a, 0x1799, 0x17e8, 0x1839, 0x188b, 0x18de, 0x1932, 0x1987,
	0x19dd, 0x1a34, 0x1a8d, 0x1ae6, 0x1b41, 0x1b9d, 0x1bfa, 0x1c59,
	0x1cb8, 0x1d19, 0x1d7c, 0x1ddf, 0x1e44, 0x1eaa, 0x1f12, 0x1f7a,
	0x1fe5, 0x2050, 0x20bd, 0x212c, 0x219c, 0x220d, 0x2280, 0x22f5,
	0x236b, 0x23e2, 0x245b, 0x24d6, 0x2553, 0x25d1, 0x2650, 0x26d2,
	0x2755, 0x27d9, 0x2860, 0x28e8, 0x2972, 0x29fe, 0x2a8c, 0x2b1b,
	0x2bad, 0x2c40, 0x2cd6, 0x2d6d, 0x2e06, 0x2ea2, 0x2f3f, 0x2fdf,
	0x3080, 0x3124, 0x31ca, 0x3272, 0x331c, 0x33c9, 0x3477, 0x3529,
	0x35dc, 0x3692, 0x374a, 0x3805, 0x38c2, 0x3981, 0x3a43, 0x3b08,
	0x3bcf, 0x3c99, 0x3d66, 0x3e35, 0x3f07, 0x3fdc, 0x40b3, 0x418e,
	0x426b, 0x434b, 0x442e, 0x4514, 0x45fe, 0x46ea, 0x47d9, 0x48cc,
	0x49c2, 0x4aba, 0x4bb7, 0x4cb6, 0x4db9, 0x4ec0, 0x4fc9, 0x50d7,
	0x51e8, 0x52fc, 0x5414, 0x5530, 0x564f, 0x5773, 0x589a, 0x59c5,
	0x5af4, 0x5c27, 0x5d5e, 0x5e99, 0x5fd9, 0x611c, 0x6264, 0x63b0,
	0x6501, 0x6655, 0x67af, 0x690d, 0x6a6f, 0x6bd7, 0x6d43, 0x6eb3,
	0x7029, 0x71a4, 0x7323, 0x74a8, 0x7632, 0x77c1, 0x7955, 0x7aee,
	0x7c8d, 0x7e32, 0x7fdc, 0x818b, 0x8341, 0x84fc, 0x86bc, 0x8883,
	0x8a50, 0x8c23, 0x8dfc, 0x8fdb, 0x91c1, 0x93ad, 0x959f, 0x9798,
	0x9998, 0x9b9e, 0x9dac, 0x9fc0, 0xa1db, 0xa3fd, 0xa627, 0xa858,
	0xaa90, 0xacd0, 0xaf17, 0xb166, 0xb3bd, 0xb61c, 0xb882, 0xbaf1,
	0xbd68, 0xbfe7, 0xc26f, 0xc4ff, 0xc798, 0xca3a, 0xcce5, 0xcf98,
	0xd255, 0xd51b, 0xd7ea, 0xdac3, 0xdda5, 0xe092, 0xe388, 0xe688,
	0xe992, 0xeca6, 0xefc5, 0xf2ee, 0xf622, 0xf961, 0xfcab, 0xffff,
};

#endif /* DOC_HIDDEN */

/* (32bit x 16bit) >> 16 */
typedef union {
	int i;
	short s[2];
} val_t;
static inline int MULTI_DIV(int a, unsigned short b)
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

/*
 * apply volumue attenuation
 *
 * TODO: use SIMD operations
 */
static void snd_pcm_softvol_convert(snd_pcm_softvol_t *svol,
				    const snd_pcm_channel_area_t *dst_areas,
				    snd_pcm_uframes_t dst_offset,
				    const snd_pcm_channel_area_t *src_areas,
				    snd_pcm_uframes_t src_offset,
				    unsigned int channels,
				    snd_pcm_uframes_t frames,
				    unsigned int cur_vol)
{
	const snd_pcm_channel_area_t *dst_area, *src_area;
	unsigned int src_step, dst_step;
	unsigned int ch;
	unsigned int fr;
	unsigned int vol_scale;

	if (cur_vol == 0) {
		snd_pcm_areas_silence(dst_areas, dst_offset, channels, frames,
				      svol->sformat);
		return;
	} else if (cur_vol == svol->max_val) {
		snd_pcm_areas_copy(dst_areas, dst_offset, src_areas, src_offset,
				   channels, frames, svol->sformat);
		return;
	}

	vol_scale = svol->dB_value[cur_vol];
	if (svol->sformat == SND_PCM_FORMAT_S16) {
		/* 16bit samples */
		short *src, *dst;
		for (ch = 0; ch < channels; ch++) {
			src_area = &src_areas[ch];
			dst_area = &dst_areas[ch];
			src = snd_pcm_channel_area_addr(src_area, src_offset);
			dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
			src_step = snd_pcm_channel_area_step(src_area) / sizeof(short);
			dst_step = snd_pcm_channel_area_step(dst_area) / sizeof(short);
			fr = frames;
			while (fr--) {
				*dst = ((int)*src * vol_scale) >> VOL_SCALE_SHIFT;
				src += src_step;
				dst += dst_step;
			}
		}
	} else {
		/* 32bit samples */
		int *src, *dst;
		for (ch = 0; ch < channels; ch++) {
			src_area = &src_areas[ch];
			dst_area = &dst_areas[ch];
			src = snd_pcm_channel_area_addr(src_area, src_offset);
			dst = snd_pcm_channel_area_addr(dst_area, dst_offset);
			src_step = snd_pcm_channel_area_step(src_area) / sizeof(int);
			dst_step = snd_pcm_channel_area_step(dst_area) / sizeof(int);
			fr = frames;
			while (fr--) {
				*dst = MULTI_DIV(*src, vol_scale);
				src += src_step;
				dst += dst_step;
			}
		}
	}
}

/*
 * get the current volume value from driver
 *
 * TODO: mmap support?
 */
static unsigned int get_current_volume(snd_pcm_softvol_t *svol)
{
	unsigned int val;
	if (snd_ctl_elem_read(svol->ctl, &svol->elem) < 0)
		return 0;
	/* set max vol as default */
	val = svol->elem.value.integer.value[0];
	if (val > svol->max_val)
		val = svol->max_val;
	return val;
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
	_snd_pcm_hw_params_set_subformat(sparams, SND_PCM_SUBFORMAT_STD);
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
	snd_pcm_softvol_convert(svol, slave_areas, slave_offset,
				areas, offset, 
				pcm->channels,
				size, get_current_volume(svol));
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
	snd_pcm_softvol_convert(svol, areas, offset, 
				slave_areas, slave_offset,
				pcm->channels,
				size, get_current_volume(svol));
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

static int add_user_ctl(snd_pcm_softvol_t *svol, snd_ctl_elem_info_t *cinfo)
{
	int err;

	err = snd_ctl_elem_add_integer(svol->ctl, &cinfo->id, 1, 0, svol->max_val, 0);
	if (err < 0)
		return err;
	svol->elem.value.integer.value[0] = svol->max_val;
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
				double min_dB, int resolution)
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
		err = add_user_ctl(svol, cinfo);
		if (err < 0) {
			SNDERR("Cannot add a control");
			return err;
		}
	} else {
		if (! (cinfo->access & SNDRV_CTL_ELEM_ACCESS_USER)) {
			/* hardware control exists */
			return 1; /* notify */

		} else if (cinfo->type != SND_CTL_ELEM_TYPE_INTEGER ||
		    cinfo->count != 1 ||
		    cinfo->value.integer.min != 0 ||
		    cinfo->value.integer.max != resolution - 1) {
			snd_ctl_elem_remove(svol->ctl, &cinfo->id);
			err = add_user_ctl(svol, cinfo);
			if (err < 0) {
				SNDERR("Cannot replace a control");
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
			double v = (pow(2.0, db / 10.0) * (double)(1 << VOL_SCALE_SHIFT));
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
	err = softvol_load_control(slave, svol, ctl_card, ctl_id, min_dB, resolution);
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
static int parse_control_id(snd_config_t *conf, snd_ctl_elem_id_t *ctl_id, int *cardp)
{
	snd_config_iterator_t i, next;
	int iface = SND_CTL_ELEM_IFACE_MIXER;
	const char *name = NULL;
	long index = 0;
	long device = -1;
	long subdevice = -1;
	int err;

	*cardp = -1;
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
		[card STR]      # control name (e.g. hw:0)
		[iface STR]     # interface of the element
		[index INT]     # index of the element
		[device INT]    # device number of the element
		[subdevice INT] # subdevice number of the element
	}
	[min_dB REAL]           # minimal dB value (default: -48 dB)
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
	int card = -1;

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
	if ((err = parse_control_id(control, ctl_id, &card)) < 0) {
		snd_pcm_close(spcm);
		return err;
	}
	err = snd_pcm_softvol_open(pcmp, name, sformat, card, ctl_id, min_dB, resolution, spcm, 1);
	if (err < 0)
		snd_pcm_close(spcm);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_softvol_open, SND_PCM_DLSYM_VERSION);
#endif
