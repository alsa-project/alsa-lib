/**
 * \file ordinary_mixer/ordinary_mixer.c
 * \ingroup Mixer_ordinary
 * \brief Ordinary mixer interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003
 *
 * Ordinary mixer interface is a high level abtraction for soundcard's
 * mixing.
 *
 * See the \ref Mixer_ordinary page for more details.
 */
/*
 *  Ordinary Mixer Interface - main file
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

/*! \page Mixer_ordinary Ordinary mixer interface

<P>Write something here</P>

\section Mixer_ordinary_overview

Write something here

*/
/**
 * \example ../test/ordinary_mixer.c
 * \anchor example_ordinary_mixer
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
#include "mixer_ordinary.h"

struct sndo_mixer {
	snd_mixer_t *mixer;
};

/**
 * \brief Opens a ordinary mixer instance
 * \param pmixer Returned ordinary mixer handle
 * \param playback_name ASCII identifier of the ordinary mixer handle (playback controls)
 * \param capture_name ASCII identifier of the ordinary mixer handle (capture controls)
 * \param lconf Local configuration (might be NULL - use global configuration)
 * \return 0 on success otherwise a negative error code
 */
int sndo_mixer_open(sndo_mixer_t **pmixer,
		    const char *playback_name,
		    const char *capture_name,
		    snd_config_t *lconf)
{
	*pmixer = NULL;
	return -ENODEV;
}

/**
 * \brief Closes a ordinary mixer instance
 * \param mixer Ordinary mixer handle to close
 * \return 0 on success otherwise a negative error code
 */
int sndo_mixer_close(sndo_mixer_t *mixer)
{
	return -ENODEV;
}

/**
 * \brief get count of poll descriptors for ordinary mixer handle
 * \param mixer ordinary mixer handle
 * \return count of poll descriptors
 */
int sndo_mixer_poll_descriptors_count(sndo_mixer_t *mixer)
{
	return snd_mixer_poll_descriptors_count(mixer->mixer);
}

/**
 * \brief get poll descriptors
 * \param mixer ordinary mixer handle
 * \param pfds array of poll descriptors
 * \param space space in the poll descriptor array
 * \return count of filled descriptors
 */     
int sndo_mixer_poll_descriptors(sndo_mixer_t *mixer, struct pollfd *pfds, unsigned int space)
{
	return snd_mixer_poll_descriptors(mixer->mixer, pfds, space);
}

/**
 * \brief get returned events from poll descriptors
 * \param mixer ordinary mixer handle
 * \param pfds array of poll descriptors
 * \param nfds count of poll descriptors
 * \param revents returned events
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_poll_descriptors_revents(sndo_mixer_t *mixer, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	return snd_mixer_poll_descriptors_revents(mixer->mixer, pfds, nfds, revents);
}

/**
 * \brief get ordinary mixer io control value
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val returned value
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_io_get(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val)
{
	return -ENODEV;
}

/**
 * \brief set ordinary mixer io control value
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val desired value
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_io_set(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int val)
{
	return -ENODEV;
}

/**
 * \brief get ordinary mixer io control change notification
 * \param mixer ordinary mixer handle
 * \param changed list of changed io types
 * \param changed_array_size size of list of changed io types
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_io_change(sndo_mixer_t *mixer, enum sndo_mixer_io_type *changed, int changed_array_size)
{
	return -ENODEV;
}
