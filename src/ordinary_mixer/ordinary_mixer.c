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
#include "alisp.h"
#include "mixer_ordinary.h"

struct sndo_mixer {
	struct alisp_cfg *cfg;
	struct alisp_instance *alisp;
	int hctl_count;
	snd_hctl_t **hctl;
	int _free_cfg;
};

/**
 * \brief Opens a ordinary mixer instance
 * \param pmixer Returned ordinary mixer handle
 * \param playback_pcm handle of the playback PCM
 * \param capture_pcm handle of the capture PCM
 * \param lconf Local configuration (might be NULL - use global configuration)
 * \return 0 on success otherwise a negative error code
 */
int sndo_mixer_open(sndo_mixer_t **pmixer,
		    snd_pcm_t *playback_pcm,
		    snd_pcm_t *capture_pcm,
		    struct alisp_cfg *lconf)
{
	struct alisp_cfg *cfg = lconf;
	struct alisp_instance *alisp;
	struct alisp_seq_iterator *iterator;
	sndo_mixer_t *mixer;
	int err, count;
	long val;

	*pmixer = NULL;
	if (cfg == NULL) {
		char *file;
		snd_input_t *input;
		file = getenv("ALSA_ORDINARY_MIXER");
		if (!file)
			file = DATADIR "/alsa/sndo-mixer.alisp";
		if ((err = snd_input_stdio_open(&input, file, "r")) < 0) {
			SNDERR("unable to open alisp file '%s'", file);
			return err;
		}
		cfg = alsa_lisp_default_cfg(input);
		if (cfg == NULL)
			return -ENOMEM;
	}
	err = alsa_lisp(cfg, &alisp);
	if (err < 0)
		goto __error;
	err = alsa_lisp_function(alisp, &iterator, "sndo_mixer_open", "%ppcm%ppcm", playback_pcm, capture_pcm);
	if (err < 0) {
		alsa_lisp_free(alisp);
		goto __error;
	}
	err = alsa_lisp_seq_integer(iterator, &val);
	if (err == 0 && val < 0)
		err = val;
	if (err < 0) {
		alsa_lisp_free(alisp);
		goto __error;
	}
	count = 0;
	if (alsa_lisp_seq_first(alisp, "hctls", &iterator) == 0) {
		count = alsa_lisp_seq_count(iterator);
		if (count < 0)
			count = 0;
	}
	mixer = malloc(sizeof(sndo_mixer_t) + count * sizeof(snd_hctl_t *));
	if (mixer == NULL) {
		alsa_lisp_free(alisp);
		err = -ENOMEM;
		goto __error;
	}
	memset(mixer, 0, sizeof(sndo_mixer_t));
	if (count > 0) {
		mixer->hctl = (snd_hctl_t **)(mixer + 1);
		do {
			if (alsa_lisp_seq_pointer(iterator, "hctl", (void **)&mixer->hctl[mixer->hctl_count++]))
				break;
		} while (mixer->hctl_count < count && alsa_lisp_seq_next(&iterator) == 0);
		if (mixer->hctl_count < count) {
			mixer->hctl = NULL;
			mixer->hctl_count = 0;
		}
	}
	mixer->alisp = alisp;
	mixer->cfg = cfg;
	mixer->_free_cfg = cfg != lconf;
	*pmixer = mixer;
	return 0;
      __error:
	if (cfg != lconf)
		alsa_lisp_default_cfg_free(cfg);
	return err;
}

/**
 * \brief Closes a ordinary mixer instance
 * \param mixer Ordinary mixer handle to close
 * \return 0 on success otherwise a negative error code
 */
int sndo_mixer_close(sndo_mixer_t *mixer)
{
	int res;
	
	res = alsa_lisp_function(mixer->alisp, NULL, "sndo_mixer_close", "n");
	alsa_lisp_free(mixer->alisp);
	if (mixer->_free_cfg)
		alsa_lisp_default_cfg_free(mixer->cfg);
	free(mixer);
	return res;
}

/**
 * \brief get count of poll descriptors for ordinary mixer handle
 * \param mixer ordinary mixer handle
 * \return count of poll descriptors
 */
int sndo_mixer_poll_descriptors_count(sndo_mixer_t *mixer)
{
	int idx, err, res = -EIO;

	if (mixer->hctl_count > 0) {
		for (idx = 0; idx < mixer->hctl_count; idx++) {
			err = snd_hctl_poll_descriptors_count(mixer->hctl[idx]);
			if (err < 0)
				return err;
			res += err;
		}
	} else {
		struct alisp_seq_iterator *result;
		long val;
		err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_poll_descriptors_count", "n");
		if (err < 0)
			return err;
		err = alsa_lisp_seq_integer(result, &val);
		return err < 0 ? err : val;
	}
	return res;
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
	//return snd_mixer_poll_descriptors(mixer->mixer, pfds, space);
	return -ENODEV;
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
	//return snd_mixer_poll_descriptors_revents(mixer->mixer, pfds, nfds, revents);
	return -ENODEV;
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
