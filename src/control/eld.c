/**
 * \file control/eld.c
 * \brief ELD decoder
 * \author Jaroslav Kysela <perex@perex>
 * \date 2022
 */
/*
 *  Control Interface - Decode ELD
 *
 *  Copyright (c) 2022 Jaroslav Kysela <perex@perex.cz>
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
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "control_local.h"

static void __fill_eld_ctl_id(snd_ctl_elem_id_t *id, int dev, int subdev)
{
	snd_ctl_elem_id_set_interface(id, SND_CTL_ELEM_IFACE_PCM);
	snd_ctl_elem_id_set_name(id, "ELD");
	snd_ctl_elem_id_set_device(id, dev);
	snd_ctl_elem_id_set_index(id, subdev);
}

int __snd_pcm_info_eld_fixup(snd_pcm_info_t * info)
{
	snd_ctl_t *ctl;
	snd_ctl_elem_info_t cinfo = {0};
	snd_ctl_elem_value_t value = {0};
	unsigned char *eld;
	unsigned int l, spc;
	char *s, c;
	int ret, valid;

	ret = snd_ctl_hw_open(&ctl, NULL, info->card, 0);
	if (ret < 0) {
		SYSMSG("Cannot open the associated CTL\n");
		return ret;
	}

	__fill_eld_ctl_id(&cinfo.id, info->device, info->subdevice);
	value.id = cinfo.id;
	ret = snd_ctl_elem_info(ctl, &cinfo);
	if (ret >= 0 && cinfo.type == SND_CTL_ELEM_TYPE_BYTES)
		ret = snd_ctl_elem_read(ctl, &value);
	snd_ctl_close(ctl);
	if (ret == -ENOENT || cinfo.type != SND_CTL_ELEM_TYPE_BYTES || cinfo.count == 0)
		return 0;
	if (ret < 0) {
		SYSMSG("Cannot read ELD\n");
		return ret;
	}
	/* decode connected HDMI device name */
	eld = value.value.bytes.data;
	if (cinfo.count < 20 || cinfo.count > 256)
		return -EIO;
	l = eld[4] & 0x1f;
	if (l == 0)
		/* no monitor name detected */
		goto __present;
	if (l > 16 || 20 + l > cinfo.count) {
		SNDERR("ELD decode failed, using old HDMI output names\n");
		return 0;
	}
	s = alloca(l + 1);
	/* sanitize */
	valid = 0;
	spc = 0;
	while (l > 0) {
		l--;
		c = eld[20 + l];
		if (c <= ' ' || c >= 0x7f) {
			s[l] = ' ';
		} else {
			valid += !!isalnum(c);
			s[l] = c;
			if (spc == 0)
				spc = l + 1;
		}
	}
	if (valid > 3) {
		s[spc] = '\0';
		snd_strlcpy((char *)info->name, s, sizeof(info->name));
	} else {
__present:
		strncat((char *)info->name, " *", sizeof(info->name) - 1);
		((char *)info->name)[sizeof(info->name)-1] = '\0';
	}
	return 0;
}
