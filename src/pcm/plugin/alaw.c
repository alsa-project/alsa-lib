/*
 *  A-Law conversion Plug-In Interface
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
 *                        Uros Bizjak <uros@kss-loka.si>
 *
 *  Based on reference implementation by Sun Microsystems, Inc.
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
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <sys/uio.h>
#include "../pcm_local.h"

#define	SIGN_BIT	(0x80)		/* Sign bit for a A-law byte. */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	NSEGS		(8)		/* Number of A-law segments. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */
#define	SEG_MASK	(0x70)		/* Segment field mask. */

static short alaw_seg_end[8] = {0xFF, 0x1FF, 0x3FF, 0x7FF,
				0xFFF, 0x1FFF, 0x3FFF, 0x7FFF};

static inline int search(int val, short *table, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (val <= *table++)
			return (i);
	}
	return (size);
}

/*
 * linear2alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
 *
 * linear2alaw() accepts an 16-bit integer and encodes it as A-law data.
 *
 *		Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	0000000wxyza			000wxyz
 *	0000001wxyza			001wxyz
 *	000001wxyzab			010wxyz
 *	00001wxyzabc			011wxyz
 *	0001wxyzabcd			100wxyz
 *	001wxyzabcde			101wxyz
 *	01wxyzabcdef			110wxyz
 *	1wxyzabcdefg			111wxyz
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
static unsigned char linear2alaw(int pcm_val)	/* 2's complement (16-bit range) */
{
	int		mask;
	int		seg;
	unsigned char	aval;

	if (pcm_val >= 0) {
		mask = 0xD5;		/* sign (7th) bit = 1 */
	} else {
		mask = 0x55;		/* sign bit = 0 */
		pcm_val = -pcm_val - 8;
	}

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, alaw_seg_end, NSEGS);

	/* Combine the sign, segment, and quantization bits. */

	if (seg >= 8)		/* out of range, return maximum value. */
		return (0x7F ^ mask);
	else {
		aval = seg << SEG_SHIFT;
		if (seg < 2)
			aval |= (pcm_val >> 4) & QUANT_MASK;
		else
			aval |= (pcm_val >> (seg + 3)) & QUANT_MASK;
		return (aval ^ mask);
	}
}

/*
 * alaw2linear() - Convert an A-law value to 16-bit linear PCM
 *
 */
static int alaw2linear(unsigned char a_val)
{
	int		t;
	int		seg;

	a_val ^= 0x55;

	t = (a_val & QUANT_MASK) << 4;
	seg = ((unsigned)a_val & SEG_MASK) >> SEG_SHIFT;
	switch (seg) {
	case 0:
		t += 8;
		break;
	case 1:
		t += 0x108;
		break;
	default:
		t += 0x108;
		t <<= seg - 1;
	}
	return ((a_val & SIGN_BIT) ? t : -t);
}


/*
 *  Basic A-Law plugin
 */

typedef void (*alaw_f)(snd_pcm_plugin_t *plugin,
		       const snd_pcm_plugin_channel_t *src_channels,
		       snd_pcm_plugin_channel_t *dst_channels,
		       size_t frames);

typedef struct alaw_private_data {
	alaw_f func;
	int conv;
} alaw_t;

static void alaw_decode(snd_pcm_plugin_t *plugin,
			const snd_pcm_plugin_channel_t *src_channels,
			snd_pcm_plugin_channel_t *dst_channels,
			size_t frames)
{
#define PUT_S16_LABELS
#include "plugin_ops.h"
#undef PUT_S16_LABELS
	alaw_t *data = (alaw_t *)plugin->extra_data;
	void *put = put_s16_labels[data->conv];
	int channel;
	int nchannels = plugin->src_format.channels;
	for (channel = 0; channel < nchannels; ++channel) {
		char *src;
		char *dst;
		int src_step, dst_step;
		size_t frames1;
		if (!src_channels[channel].enabled) {
			if (dst_channels[channel].wanted)
				snd_pcm_area_silence(&dst_channels[channel].area, 0, frames, plugin->dst_format.format);
			dst_channels[channel].enabled = 0;
			continue;
		}
		dst_channels[channel].enabled = 1;
		src = src_channels[channel].area.addr + src_channels[channel].area.first / 8;
		dst = dst_channels[channel].area.addr + dst_channels[channel].area.first / 8;
		src_step = src_channels[channel].area.step / 8;
		dst_step = dst_channels[channel].area.step / 8;
		frames1 = frames;
		while (frames1-- > 0) {
			signed short sample = alaw2linear(*src);
			goto *put;
#define PUT_S16_END after
#include "plugin_ops.h"
#undef PUT_S16_END
		after:
			src += src_step;
			dst += dst_step;
		}
	}
}

static void alaw_encode(snd_pcm_plugin_t *plugin,
			const snd_pcm_plugin_channel_t *src_channels,
			snd_pcm_plugin_channel_t *dst_channels,
			size_t frames)
{
#define GET_S16_LABELS
#include "plugin_ops.h"
#undef GET_S16_LABELS
	alaw_t *data = (alaw_t *)plugin->extra_data;
	void *get = get_s16_labels[data->conv];
	int channel;
	int nchannels = plugin->src_format.channels;
	signed short sample = 0;
	for (channel = 0; channel < nchannels; ++channel) {
		char *src;
		char *dst;
		int src_step, dst_step;
		size_t frames1;
		if (!src_channels[channel].enabled) {
			if (dst_channels[channel].wanted)
				snd_pcm_area_silence(&dst_channels[channel].area, 0, frames, plugin->dst_format.format);
			dst_channels[channel].enabled = 0;
			continue;
		}
		dst_channels[channel].enabled = 1;
		src = src_channels[channel].area.addr + src_channels[channel].area.first / 8;
		dst = dst_channels[channel].area.addr + dst_channels[channel].area.first / 8;
		src_step = src_channels[channel].area.step / 8;
		dst_step = dst_channels[channel].area.step / 8;
		frames1 = frames;
		while (frames1-- > 0) {
			goto *get;
#define GET_S16_END after
#include "plugin_ops.h"
#undef GET_S16_END
		after:
			*dst = linear2alaw(sample);
			src += src_step;
			dst += dst_step;
		}
	}
}

static ssize_t alaw_transfer(snd_pcm_plugin_t *plugin,
			     const snd_pcm_plugin_channel_t *src_channels,
			     snd_pcm_plugin_channel_t *dst_channels,
			     size_t frames)
{
	alaw_t *data;
	unsigned int channel;

	assert(plugin && src_channels && dst_channels);
	if (frames == 0)
		return 0;
	for (channel = 0; channel < plugin->src_format.channels; channel++) {
		assert(src_channels[channel].area.first % 8 == 0 && 
		       src_channels[channel].area.step % 8 == 0);
		assert(dst_channels[channel].area.first % 8 == 0 &&
		       dst_channels[channel].area.step % 8 == 0);
	}
	data = (alaw_t *)plugin->extra_data;
	data->func(plugin, src_channels, dst_channels, frames);
        return frames;
}

int snd_pcm_plugin_build_alaw(snd_pcm_plug_t *plug,
			      snd_pcm_format_t *src_format,
			      snd_pcm_format_t *dst_format,
			      snd_pcm_plugin_t **r_plugin)
{
	int err;
	alaw_t *data;
	snd_pcm_plugin_t *plugin;
	snd_pcm_format_t *format;
	alaw_f func;

	assert(r_plugin);
	*r_plugin = NULL;

	assert(src_format->rate == dst_format->rate);
	assert(src_format->channels == dst_format->channels);

	if (dst_format->format == SND_PCM_SFMT_A_LAW) {
		format = src_format;
		func = alaw_encode;
	}
	else if (src_format->format == SND_PCM_SFMT_A_LAW) {
		format = dst_format;
		func = alaw_decode;
	}
	else
		assert(0);
	assert(snd_pcm_format_linear(format->format));

	err = snd_pcm_plugin_build(plug, "A-Law<->linear conversion",
				   src_format, dst_format,
				   sizeof(alaw_t), &plugin);
	if (err < 0)
		return err;
	data = (alaw_t*)plugin->extra_data;
	data->func = func;
	data->conv = getput_index(format->format);
	plugin->transfer = alaw_transfer;
	*r_plugin = plugin;
	return 0;
}
