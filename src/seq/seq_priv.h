/*
 *  Sequencer Interface - definition of sequencer event handler
 *  Copyright (c) 1998 by Jaroslav Kysela <perex@suse.cz>
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

#ifndef __SEQ_PRIV_H
#define __SEQ_PRIV_H

typedef struct snd_stru_seq_cell {
	snd_seq_event_t ev;
	struct snd_stru_seq_cell *next;
} snd_seq_cell_t;

struct snd_seq {
	int client;		/* client number */
	int fd;
	/* buffers */
	char *obuf;		/* output buffer */
	size_t obufsize;		/* output buffer size */
	size_t obufused;		/* output buffer used size */
	char *ibuf;		/* input buffer */
	size_t ibufsize;		/* input buffer size */
	/* input queue */
	int cells;
	snd_seq_cell_t *head;
	snd_seq_cell_t *tail;
};

#endif
