/**
 * \file <alsa/asoundlib.h>
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@suse.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 1998-2003
 *
 * Application interface library for the ALSA driver
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

#ifndef __ASOUNDLIB_H
#define __ASOUNDLIB_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <endian.h>
#include <stdarg.h>
#include <sys/poll.h>
#include <errno.h>

#include "asoundef.h"
#include "version.h"
#include "global.h"
#include "input.h"
#include "output.h"
#include "error.h"
#include "conf.h"
#include "pcm.h"
#include "rawmidi.h"
#include "timer.h"
#include "hwdep.h"
#include "control.h"
#include "mixer.h"
#include "seq_event.h"
#include "seq.h"
#include "seqmid.h"
#include "seq_midi_event.h"
#include "conv.h"
#include "instr.h"

#endif /* __ASOUNDLIB_H */
