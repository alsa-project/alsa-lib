/*
 *  Mu-Law conversion Plug-In Interface
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
  
#ifdef __KERNEL__
#include "../../include/driver.h"
#include "../../include/pcm.h"
#include "../../include/pcm_plugin.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <byteswap.h>
#include <sys/uio.h>
#include "../pcm_local.h"
#endif

#define	SIGN_BIT	(0x80)		/* Sign bit for a u-law byte. */
#define	QUANT_MASK	(0xf)		/* Quantization field mask. */
#define	NSEGS		(8)		/* Number of u-law segments. */
#define	SEG_SHIFT	(4)		/* Left shift for segment number. */
#define	SEG_MASK	(0x70)		/* Segment field mask. */

static short ulaw_seg_end[8] = {0xFF, 0x1FF, 0x3FF, 0x7FF,
				0xFFF, 0x1FFF, 0x3FFF, 0x7FFF};

static inline int search(int val, short *table, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		if (val <= *table++)
			return (i);
	}
	return size;
}

#define	BIAS		(0x84)		/* Bias for linear code. */

/*
 * linear2ulaw() - Convert a linear PCM value to u-law
 *
 * In order to simplify the encoding process, the original linear magnitude
 * is biased by adding 33 which shifts the encoding range from (0 - 8158) to
 * (33 - 8191). The result can be seen in the following encoding table:
 *
 *	Biased Linear Input Code	Compressed Code
 *	------------------------	---------------
 *	00000001wxyza			000wxyz
 *	0000001wxyzab			001wxyz
 *	000001wxyzabc			010wxyz
 *	00001wxyzabcd			011wxyz
 *	0001wxyzabcde			100wxyz
 *	001wxyzabcdef			101wxyz
 *	01wxyzabcdefg			110wxyz
 *	1wxyzabcdefgh			111wxyz
 *
 * Each biased linear code has a leading 1 which identifies the segment
 * number. The value of the segment number is equal to 7 minus the number
 * of leading 0's. The quantization interval is directly available as the
 * four bits wxyz.  * The trailing bits (a - h) are ignored.
 *
 * Ordinarily the complement of the resulting code word is used for
 * transmission, and so the code word is complemented before it is returned.
 *
 * For further information see John C. Bellamy's Digital Telephony, 1982,
 * John Wiley & Sons, pps 98-111 and 472-476.
 */
static unsigned char linear2ulaw(int pcm_val)	/* 2's complement (16-bit range) */
{
	int mask;
	int seg;
	unsigned char uval;

	/* Get the sign and the magnitude of the value. */
	if (pcm_val < 0) {
		pcm_val = BIAS - pcm_val;
		mask = 0x7F;
	} else {
		pcm_val += BIAS;
		mask = 0xFF;
	}

	/* Convert the scaled magnitude to segment number. */
	seg = search(pcm_val, ulaw_seg_end, NSEGS);

	/*
	 * Combine the sign, segment, quantization bits;
	 * and complement the code word.
	 */
	if (seg >= 8)		/* out of range, return maximum value. */
		return 0x7F ^ mask;
	else {
		uval = (seg << 4) | ((pcm_val >> (seg + 3)) & 0xF);
		return uval ^ mask;
	}
}

/*
 * ulaw2linear() - Convert a u-law value to 16-bit linear PCM
 *
 * First, a biased linear code is derived from the code word. An unbiased
 * output can then be obtained by subtracting 33 from the biased code.
 *
 * Note that this function expects to be passed the complement of the
 * original code word. This is in keeping with ISDN conventions.
 */
static int ulaw2linear(unsigned char u_val)
{
	int t;

	/* Complement to obtain normal u-law value. */
	u_val = ~u_val;

	/*
	 * Extract and bias the quantization bits. Then
	 * shift up by the segment number and subtract out the bias.
	 */
	t = ((u_val & QUANT_MASK) << 3) + BIAS;
	t <<= ((unsigned)u_val & SEG_MASK) >> SEG_SHIFT;

	return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
}

/*
 *  Basic Mu-Law plugin
 */

typedef void (*mulaw_f)(snd_pcm_plugin_t *plugin,
			const snd_pcm_plugin_voice_t *src_voices,
			snd_pcm_plugin_voice_t *dst_voices,
			size_t samples);

typedef struct mulaw_private_data {
	mulaw_f func;
	int conv;
} mulaw_t;

static void mulaw_decode(snd_pcm_plugin_t *plugin,
			const snd_pcm_plugin_voice_t *src_voices,
			snd_pcm_plugin_voice_t *dst_voices,
			size_t samples)
{
#define PUT16_LABELS
#include "plugin_ops.h"
#undef PUT16_LABELS
	mulaw_t *data = (mulaw_t *)plugin->extra_data;
	void *put = put16_labels[data->conv];
	int voice;
	int nvoices = plugin->src_format.voices;
	for (voice = 0; voice < nvoices; ++voice) {
		char *src;
		char *dst;
		int src_step, dst_step;
		size_t samples1;
		if (!src_voices[voice].enabled) {
			if (dst_voices[voice].wanted)
				snd_pcm_plugin_silence_voice(plugin, &dst_voices[voice], samples);
			dst_voices[voice].enabled = 0;
			continue;
		}
		dst_voices[voice].enabled = 1;
		src = src_voices[voice].addr + src_voices[voice].first / 8;
		dst = dst_voices[voice].addr + dst_voices[voice].first / 8;
		src_step = src_voices[voice].step / 8;
		dst_step = dst_voices[voice].step / 8;
		samples1 = samples;
		while (samples1-- > 0) {
			signed short sample = ulaw2linear(*src);
			goto *put;
#define PUT16_END after
#include "plugin_ops.h"
#undef PUT16_END
		after:
			src += src_step;
			dst += dst_step;
		}
	}
}

static void mulaw_encode(snd_pcm_plugin_t *plugin,
			const snd_pcm_plugin_voice_t *src_voices,
			snd_pcm_plugin_voice_t *dst_voices,
			size_t samples)
{
#define GET16_LABELS
#include "plugin_ops.h"
#undef GET16_LABELS
	mulaw_t *data = (mulaw_t *)plugin->extra_data;
	void *get = get16_labels[data->conv];
	int voice;
	int nvoices = plugin->src_format.voices;
	signed short sample = 0;
	for (voice = 0; voice < nvoices; ++voice) {
		char *src;
		char *dst;
		int src_step, dst_step;
		size_t samples1;
		if (!src_voices[voice].enabled) {
			if (dst_voices[voice].wanted)
				snd_pcm_plugin_silence_voice(plugin, &dst_voices[voice], samples);
			dst_voices[voice].enabled = 0;
			continue;
		}
		dst_voices[voice].enabled = 1;
		src = src_voices[voice].addr + src_voices[voice].first / 8;
		dst = dst_voices[voice].addr + dst_voices[voice].first / 8;
		src_step = src_voices[voice].step / 8;
		dst_step = dst_voices[voice].step / 8;
		samples1 = samples;
		while (samples1-- > 0) {
			goto *get;
#define GET16_END after
#include "plugin_ops.h"
#undef GET16_END
		after:
			*dst = linear2ulaw(sample);
			src += src_step;
			dst += dst_step;
		}
	}
}

static ssize_t mulaw_transfer(snd_pcm_plugin_t *plugin,
			      const snd_pcm_plugin_voice_t *src_voices,
			      snd_pcm_plugin_voice_t *dst_voices,
			      size_t samples)
{
	mulaw_t *data;
	unsigned int voice;

	if (plugin == NULL || src_voices == NULL || dst_voices == NULL)
		return -EFAULT;
	if (samples == 0)
		return 0;
	for (voice = 0; voice < plugin->src_format.voices; voice++) {
		if (src_voices[voice].first % 8 != 0 || 
		    src_voices[voice].step % 8 != 0)
			return -EINVAL;
		if (dst_voices[voice].first % 8 != 0 || 
		    dst_voices[voice].step % 8 != 0)
			return -EINVAL;
	}
	data = (mulaw_t *)plugin->extra_data;
	data->func(plugin, src_voices, dst_voices, samples);
	return samples;
}

int snd_pcm_plugin_build_mulaw(snd_pcm_plugin_handle_t *handle,
			       int channel,
			       snd_pcm_format_t *src_format,
			       snd_pcm_format_t *dst_format,
			       snd_pcm_plugin_t **r_plugin)
{
	int err;
	mulaw_t *data;
	snd_pcm_plugin_t *plugin;
	snd_pcm_format_t *format;
	mulaw_f func;

	if (r_plugin == NULL)
		return -EINVAL;
	*r_plugin = NULL;

	if (src_format->rate != dst_format->rate)
		return -EINVAL;
	if (src_format->voices != dst_format->voices)
		return -EINVAL;

	if (dst_format->format == SND_PCM_SFMT_MU_LAW) {
		format = src_format;
		func = mulaw_encode;
	}
	else if (src_format->format == SND_PCM_SFMT_MU_LAW) {
		format = dst_format;
		func = mulaw_decode;
	}
	else
		return -EINVAL;
	if (!snd_pcm_format_linear(format->format))
		return -EINVAL;

	err = snd_pcm_plugin_build(handle, channel,
				   "Mu-Law<->linear conversion",
				   src_format,
				   dst_format,
				   sizeof(mulaw_t),
				   &plugin);
	if (err < 0)
		return err;
	data = (mulaw_t*)plugin->extra_data;
	data->func = func;
	data->conv = getput_index(format->format);
	plugin->transfer = mulaw_transfer;
	*r_plugin = plugin;
	return 0;
}
