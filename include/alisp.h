/*
 *  ALSA lisp implementation
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
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

struct alisp_cfg {
	int verbose: 1,
	    warning: 1,
	    debug: 1;
	snd_input_t *in;	/* program code */
	snd_output_t *out;	/* program output */
	snd_output_t *vout;	/* verbose output */
	snd_output_t *wout;	/* warning output */
	snd_output_t *dout;	/* debug output */
	snd_config_t *root;
	snd_config_t *node;
};

int alsa_lisp(struct alisp_cfg *cfg);

extern struct alisp_object alsa_lisp_nil;
extern struct alisp_object alsa_lisp_t;
