/*
 *  Control Interface - local header file
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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

struct snd_ctl {
	int card;
	int fd;
	int ccount;
	int cerr;
	snd_hcontrol_t *cfirst;
	snd_hcontrol_t *clast;
	snd_ctl_csort_t *csort;
	snd_ctl_ccallback_rebuild_t *callback_rebuild;
	void *callback_rebuild_private_data;
	snd_ctl_ccallback_add_t *callback_add;
	void *callback_add_private_data;
};
