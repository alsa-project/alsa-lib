/**
 * \file simple_mixer/simple_mixer.c
 * \ingroup Mixer_simple
 * \brief Simple mixer interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 *
 * Simple mixer interface is a high level abtraction for soundcard's
 * mixing.
 *
 * See the \ref Mixer_simple page for more details.
 */
/*
 *  Simple Mixer Interface - main file
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
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

/*! \page Mixer_simple Simple mixer interface

<P>Write something here</P>

\section Mixer_simple_overview

Write something here

*/
/**
 * \example ../test/simple_mixer.c
 * \anchor example_simple_mixer
 */

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdarg.h>
#include <signal.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <limits.h>
#include "local.h"
#include "mixer_simple.h"

struct snds_mixer {
	snd_mixer_t *mixer;
};

/**
 * \brief Opens a simple mixer instance
 * \param pmixer Returned simple mixer handle
 * \param playback_name ASCII identifier of the simple mixer handle (playback controls)
 * \param capture_name ASCII identifier of the simple mixer handle (capture controls)
 * \param lconf Local configuration (might be NULL - use global configuration)
 * \return 0 on success otherwise a negative error code
 */
int snds_mixer_open(snds_mixer_t **pmixer,
		    const char *playback_name,
		    const char *capture_name,
		    snd_config_t *lconf)
{
	*pmixer = NULL;
	return -ENODEV;
}

/**
 * \brief Closes a simple mixer instance
 * \param mixer Simple mixer handle to close
 * \return 0 on success otherwise a negative error code
 */
int snds_mixer_close(snds_mixer_t *mixer)
{
	return -ENODEV;
}

/**
 * \brief get count of poll descriptors for simple mixer handle
 * \param mixer simple mixer handle
 * \return count of poll descriptors
 */
int snds_mixer_poll_descriptors_count(snds_mixer_t *mixer)
{
	return snd_mixer_poll_descriptors_count(mixer->mixer);
}

/**
 * \brief get poll descriptors
 * \param mixer simple mixer handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */     
int snds_mixer_poll_descriptors(snds_mixer_t *mixer, struct pollfd *pfds, unsigned int space)
{
	return snd_mixer_poll_descriptors(mixer->mixer, pfds, space);
}

/**
 * \brief get returned events from poll descriptors
 * \param mixer simple mixer handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents returned events
 * \return zero if success, otherwise a negative error code
 */ 
int snds_mixer_poll_descriptors_revents(snds_mixer_t *mixer, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	return snd_mixer_poll_descriptors_revents(mixer->mixer, pfds, nfds, revents);
}

/**
 * \brief get simple mixer io control value
 * \param mixer simple mixer handle
 * \param type io type
 * \param val returned value
 * \return zero if success, otherwise a negative error code
 */ 
int snds_mixer_io_get(snds_mixer_t *mixer, enum snds_mixer_io_type type, int *val)
{
	return -ENODEV;
}

/**
 * \brief set simple mixer io control value
 * \param mixer simple mixer handle
 * \param type io type
 * \param val desired value
 * \return zero if success, otherwise a negative error code
 */ 
int snds_mixer_io_set(snds_mixer_t *mixer, enum snds_mixer_io_type type, int val)
{
	return -ENODEV;
}

/**
 * \brief get simple mixer io control change notification
 * \param mixer simple mixer handle
 * \param changed list of changed io types
 * \param changed_array_size size of list of changed io types
 * \return zero if success, otherwise a negative error code
 */ 
int snds_mixer_io_change(snds_mixer_t *mixer, enum snds_mixer_io_type *changed, int changed_array_size)
{
	return -ENODEV;
}
