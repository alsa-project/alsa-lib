/**
 * \file ordinary_mixer/ordinary_mixer.c
 * \ingroup Mixer_ordinary
 * \brief Ordinary mixer interface
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2003,2004
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

int sndo_mixer_open1(sndo_mixer_t **pmixer,
		     const char *lisp_fcn,
		     const char *lisp_fmt,
		     const void *parg,
		     const void *carg,
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
		cfg->warning = 1;
#if 0
		cfg->debug = 1;
		cfg->verbose = 1;
#endif
	}
	err = alsa_lisp(cfg, &alisp);
	if (err < 0)
		goto __error;
	err = alsa_lisp_function(alisp, &iterator, lisp_fcn, lisp_fmt, parg, carg);
	if (err < 0) {
		alsa_lisp_free(alisp);
		goto __error;
	}
	err = alsa_lisp_seq_integer(iterator, &val);
	if (err == 0 && val < 0)
		err = val;
	alsa_lisp_result_free(alisp, iterator);
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
 * \brief Opens a ordinary mixer instance
 * \param pmixer Returned ordinary mixer handle
 * \param playback_name name for playback HCTL communication
 * \param capture_name name for capture HCTL communication
 * \param lconf Local configuration (might be NULL - use global configuration)
 * \return 0 on success otherwise a negative error code
 */
int sndo_mixer_open(sndo_mixer_t **pmixer,
		    const char *playback_name,
		    const char *capture_name,
		    struct alisp_cfg *lconf)
{
	return sndo_mixer_open1(pmixer, "sndo_mixer_open", "%s%s", playback_name, capture_name, lconf);
}


/**
 * \brief Opens a ordinary mixer instance
 * \param pmixer Returned ordinary mixer handle
 * \param playback_pcm handle of the playback PCM
 * \param capture_pcm handle of the capture PCM
 * \param lconf Local configuration (might be NULL - use global configuration)
 * \return 0 on success otherwise a negative error code
 */
int sndo_mixer_open_pcm(sndo_mixer_t **pmixer,
			snd_pcm_t *playback_pcm,
			snd_pcm_t *capture_pcm,
			struct alisp_cfg *lconf)
{
	return sndo_mixer_open1(pmixer, "sndo_mixer_open_pcm", "%ppcm%ppcm", playback_pcm, capture_pcm, lconf);
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
	int err, idx, res;

	if (mixer->hctl_count > 0) {
		for (idx = res = 0; idx < mixer->hctl_count && space > 0; idx++) {
			err = snd_hctl_poll_descriptors(mixer->hctl[idx], pfds, space);
			if (err < 0)
				return err;
			res += err;
			space -= err;
		}
		return res;
	} else {
		struct alisp_seq_iterator *result;
		long val;
		err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_poll_descriptors", "%i", space);
		if (err < 0)
			return err;
		assert(0);	/* FIXME: pass pfds to application */
		err = alsa_lisp_seq_integer(result, &val);
		return err < 0 ? err : val;
	}
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
	int err, idx, count, res;

	if (mixer->hctl_count > 0) {
		for (idx = res = 0; idx < mixer->hctl_count && nfds > 0; idx++) {
			err = snd_hctl_poll_descriptors_count(mixer->hctl[idx]);
			if (err < 0)
				return err;
			count = err;
			if (nfds < (unsigned int)err)
				return -EINVAL;
			err = snd_hctl_poll_descriptors_revents(mixer->hctl[idx], pfds, count, revents);
			if (err < 0)
				return err;
			if (err != count)
				return -EINVAL;
			pfds += count;
			nfds -= err;
			revents += count;
			res += count;
		}
		return res;
	} else {
		struct alisp_seq_iterator *result;
		long val, tmp;
		
		assert(0);	/* FIXME: pass pfds to alisp too */
		err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_poll_descriptors_revents", "%i", nfds);
		if (err < 0)
			return err;
		err = alsa_lisp_seq_integer(result, &val);
		if (err >= 0 && alsa_lisp_seq_count(result) > 1) {
			alsa_lisp_seq_next(&result);
			err = alsa_lisp_seq_integer(result, &tmp);
			*revents = tmp;
		} else {
			*revents = 0;
		}
		return err < 0 ? err : val;
	}
}

#define IOLINES 6

static const char *name_table1[] = {
	"Master",
	"PCM",
	"Line",
	"Mic"
	"CD",
	"AUX"
};

#define SPEAKERS 14

static const char *name_table2[] = {
	"Front Left",
	"Front Center Left",
	"Front Center",
	"Front Center Right",
	"Front Right",
	"Front Side Left",
	"Front Side Right",
	"Rear Side Left",
	"Rear Side Right",
	"Rear Left",
	"Rear Center",
	"Rear Right",
	"LFE (Subwoofer)",
	"Overhead"
};

#define CSOURCES 5

static const char *name_table3[] = {
	"Mic",
	"Line",
	"CD",
	"AUX",
	"Mix",
};

static int compose_string(char **result, const char *s1, const char *s2, const char *s3, const char *s4)
{
	int len = strlen(s1) + strlen(s2) + strlen(s3) + strlen(s4);
	*result = malloc(len + 1);
	if (*result == NULL)
		return -ENOMEM;
	strcpy(*result, s1);
	strcat(*result, s2);
	strcat(*result, s3);
	strcat(*result, s4);
	return 0;
}

/**
 * \brief get ordinary mixer io control value
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val returned value
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_io_get_name(enum sndo_mixer_io_type type, char **name)
{
	if (type < IOLINES * 0x40) {
		int tmp = type / 0x40;
		type %= 0x40;
		if (type < SPEAKERS)
			return compose_string(name, name_table1[tmp], " ", name_table2[type], " Volume");
	} else if (type >= SNDO_MIO_CGAIN_FL && type < SNDO_MIO_CGAIN_FL + SPEAKERS) {
		return compose_string(name, "Capture Gain ", name_table2[type - SNDO_MIO_CGAIN_FL], "", "");
	} else if (type >= SNDO_MIO_CSOURCE_MIC && type < SNDO_MIO_CSOURCE_MIC + CSOURCES) {
		return compose_string(name, "Capture Source ", name_table3[type - SNDO_MIO_CSOURCE_MIC], "", "");
	}
	return -ENOENT;
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
	struct alisp_seq_iterator *result;
	long val1;
	int err;

	err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_io_set", "%i", type);
	if (err < 0)
		return err;
	err = alsa_lisp_seq_integer(result, &val1);
	if (err < 0)
		return err;
	*val = val1;
	return 0;
}

/**
 * \brief set ordinary mixer io control value
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val desired value
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_io_set(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val)
{
	struct alisp_seq_iterator *result;
	long val1;
	int err;

	err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_io_set", "%i%i", type, *val);
	if (err < 0)
		return err;
	err = alsa_lisp_seq_integer(result, &val1);
	if (err < 0)
		return err;
	*val = val1;
	return 0;
}

/**
 * \brief try to set ordinary mixer io control value
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val desired value
 * \return zero if success, otherwise a negative error code
 *
 * This function does not update the value.
 * It only returns the real value which will be set.
 */ 
int sndo_mixer_io_try_set(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val)
{
	struct alisp_seq_iterator *result;
	long val1;
	int err;

	err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_io_try_set", "%i%i", type, *val);
	if (err < 0)
		return err;
	err = alsa_lisp_seq_integer(result, &val1);
	if (err < 0)
		return err;
	*val = val1;
	return 0;
}

/**
 * \brief get ordinary mixer io control value in dB (decibel units)
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val returned value in dB
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_io_get_dB(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val)
{
	struct alisp_seq_iterator *result;
	long val1;
	int err;

	err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_io_set_dB", "%i", type);
	if (err < 0)
		return err;
	err = alsa_lisp_seq_integer(result, &val1);
	if (err < 0)
		return err;
	*val = val1;
	return 0;
}

/**
 * \brief set ordinary mixer io control value in dB (decibel units)
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val desired value in dB
 * \return zero if success, otherwise a negative error code
 */ 
int sndo_mixer_io_set_dB(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val)
{
	struct alisp_seq_iterator *result;
	long val1;
	int err;

	err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_io_set", "%i%i", type, *val);
	if (err < 0)
		return err;
	err = alsa_lisp_seq_integer(result, &val1);
	if (err < 0)
		return err;
	*val = val1;
	return 0;
}

/**
 * \brief try to set ordinary mixer io control value in dB (decibel units)
 * \param mixer ordinary mixer handle
 * \param type io type
 * \param val desired and returned value in dB
 * \return zero if success, otherwise a negative error code
 *
 * This function does not update the value.
 * It only returns the real value which will be set.
 */ 
int sndo_mixer_io_try_set_dB(sndo_mixer_t *mixer, enum sndo_mixer_io_type type, int *val)
{
	struct alisp_seq_iterator *result;
	long val1;
	int err;

	err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_io_try_set", "%i%i", type, *val);
	if (err < 0)
		return err;
	err = alsa_lisp_seq_integer(result, &val1);
	if (err < 0)
		return err;
	*val = val1;
	return 0;
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
	struct alisp_seq_iterator *result;
	long val1;
	int err;

	err = alsa_lisp_function(mixer->alisp, &result, "sndo_mixer_io_change", "%i", changed_array_size);
	if (err < 0)
		return err;
	err = alsa_lisp_seq_integer(result, &val1);
	if (err < 0)
		return err;
	if (val1 < 0)
		return val1;
	while (changed_array_size-- > 0) {
		*changed = val1;
		if (!alsa_lisp_seq_next(&result))
			break;
		err = alsa_lisp_seq_integer(result, &val1);
		if (err < 0)
			return err;
	}
	return 0;
}
