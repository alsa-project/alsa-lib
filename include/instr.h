/**
 * \file include/instr.h
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@perex.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 1998-2001
 *
 * Application interface library for the ALSA driver
 */
/*
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

#ifndef __ALSA_INSTR_H
#define __ALSA_INSTR_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  \defgroup Instrument Instrument Interface
 *  The Instrument Interface.
 *  \{
 */

/* instrument get/put */

/** container for sequencer instrument header */
typedef struct _snd_instr_header snd_instr_header_t;

size_t snd_instr_header_sizeof(void);
#define snd_instr_header_alloca(ptr) \
do {\
	assert(ptr);\
	*ptr = (snd_instr_header_t *)alloca(snd_instr_header_sizeof());\
	memset(*ptr, 0, snd_instr_header_sizeof());\
} while (0) /**< allocate instrument header on stack */
int snd_instr_header_malloc(snd_instr_header_t **ptr, size_t len);
void snd_instr_header_free(snd_instr_header_t *ptr);
void snd_instr_header_copy(snd_instr_header_t *dst, const snd_instr_header_t *src);

const snd_seq_instr_t *snd_instr_header_get_id(const snd_instr_header_t *info);
snd_seq_instr_cluster_t snd_instr_header_get_cluster(const snd_instr_header_t *info);
unsigned int snd_instr_header_get_cmd(const snd_instr_header_t *info);
size_t snd_instr_header_get_len(const snd_instr_header_t *info);
const char *snd_instr_header_get_name(const snd_instr_header_t *info);
int snd_instr_header_get_type(const snd_instr_header_t *info);
const char *snd_instr_header_get_format(const snd_instr_header_t *info);
const snd_seq_instr_t *snd_instr_header_get_alias(const snd_instr_header_t *info);
void *snd_instr_header_get_data(const snd_instr_header_t *info);
int snd_instr_header_get_follow_alias(const snd_instr_header_t *info);

void snd_instr_header_set_id(snd_instr_header_t *info, const snd_seq_instr_t *id);
void snd_instr_header_set_cluster(snd_instr_header_t *info, snd_seq_instr_cluster_t cluster);
void snd_instr_header_set_cmd(snd_instr_header_t *info, unsigned int cmd);
void snd_instr_header_set_len(snd_instr_header_t *info, size_t len);
void snd_instr_header_set_name(snd_instr_header_t *info, const char *name);
void snd_instr_header_set_type(snd_instr_header_t *info, int type);
void snd_instr_header_set_format(snd_instr_header_t *info, const char *format);
void snd_instr_header_set_alias(snd_instr_header_t *info, const snd_seq_instr_t *instr);
void snd_instr_header_set_follow_alias(snd_instr_header_t *info, int val);

/**
 *  Instrument abstraction layer
 *     - based on events
 */

/** instrument types */
#define SND_SEQ_INSTR_ATYPE_DATA	0	/**< instrument data */
#define SND_SEQ_INSTR_ATYPE_ALIAS	1	/**< instrument alias */

/** instrument ASCII identifiers */
#define SND_SEQ_INSTR_ID_DLS1		"DLS1"		/**< DLS1 */
#define SND_SEQ_INSTR_ID_DLS2		"DLS2"		/**< DLS2 */
#define SND_SEQ_INSTR_ID_SIMPLE		"Simple Wave"	/**< Simple Wave */
#define SND_SEQ_INSTR_ID_SOUNDFONT	"SoundFont"	/**< SoundFont */
#define SND_SEQ_INSTR_ID_GUS_PATCH	"GUS Patch"	/**< Gravis Patch */
#define SND_SEQ_INSTR_ID_INTERWAVE	"Interwave FFFF" /**< InterWave FFFF */
#define SND_SEQ_INSTR_ID_OPL2_3		"OPL2/3 FM"	/**< OPL2/3 FM */
#define SND_SEQ_INSTR_ID_OPL4		"OPL4"		/**< OPL4 */

/** instrument types */
#define SND_SEQ_INSTR_TYPE0_DLS1	(1<<0)		/**< MIDI DLS v1 */
#define SND_SEQ_INSTR_TYPE0_DLS2	(1<<1)		/**< MIDI DLS v2 */
#define SND_SEQ_INSTR_TYPE1_SIMPLE	(1<<0)		/**< Simple Wave */
#define SND_SEQ_INSTR_TYPE1_SOUNDFONT	(1<<1)		/**< EMU SoundFont */
#define SND_SEQ_INSTR_TYPE1_GUS_PATCH	(1<<2)		/**< Gravis UltraSound Patch */
#define SND_SEQ_INSTR_TYPE1_INTERWAVE	(1<<3)		/**< InterWave FFFF */
#define SND_SEQ_INSTR_TYPE2_OPL2_3	(1<<0)		/**< Yamaha OPL2/3 FM */
#define SND_SEQ_INSTR_TYPE2_OPL4	(1<<1)		/**< Yamaha OPL4 */

/** put commands */
#define SND_SEQ_INSTR_PUT_CMD_CREATE	0	/**< create a new layer */
#define SND_SEQ_INSTR_PUT_CMD_REPLACE	1	/**< replace the old layer with new one */
#define SND_SEQ_INSTR_PUT_CMD_MODIFY	2	/**< modify the existing layer */
#define SND_SEQ_INSTR_PUT_CMD_ADD	3	/**< add one to the existing layer */
#define SND_SEQ_INSTR_PUT_CMD_REMOVE	4	/**< remove the layer */

/** get commands */
#define SND_SEQ_INSTR_GET_CMD_FULL	0	/**< get the full data stream */
#define SND_SEQ_INSTR_GET_CMD_PARTIAL	1	/**< get the partial data stream */

/* query flags */
#define SND_SEQ_INSTR_QUERY_FOLLOW_ALIAS (1<<0)	/**< follow alias to get the instrument data */

/** free commands */
#define SND_SEQ_INSTR_FREE_CMD_ALL	0	/**< remove all matching instruments */
#define SND_SEQ_INSTR_FREE_CMD_PRIVATE	1	/**< remove only private instruments */
#define SND_SEQ_INSTR_FREE_CMD_CLUSTER	2	/**< remove only cluster instruments */
#define SND_SEQ_INSTR_FREE_CMD_SINGLE	3	/**< remove single instrument */


/**
 * FM instrument support
 */

/** FM instrument data structure */
typedef void snd_instr_fm_t;

int snd_instr_fm_convert_to_stream(snd_instr_fm_t *fm, const char *name, snd_instr_header_t **put, size_t *size);
int snd_instr_fm_convert_from_stream(snd_instr_header_t *data, size_t size, snd_instr_fm_t **fm);
int snd_instr_fm_free(snd_instr_fm_t *fm);


/**
 * Simple Wave support
 */

/** simple instrument data structure */
typedef void snd_instr_simple_t;

int snd_instr_simple_convert_to_stream(snd_instr_simple_t *simple, const char *name, snd_instr_header_t **put, size_t *size);
int snd_instr_simple_convert_from_stream(snd_instr_header_t *data, size_t size, snd_instr_simple_t **simple);
int snd_instr_simple_free(snd_instr_simple_t *simple);


/**
 * InterWave FFFF support
 */

/** IW FFFF instrument data structure */
typedef void snd_instr_iwffff_t;
/** IW FFFF handler */
typedef struct _snd_iwffff_handle snd_iwffff_handle_t;

int snd_instr_iwffff_open(snd_iwffff_handle_t **handle, const char *name_fff, const char *name_dta);
int snd_instr_iwffff_open_rom(snd_iwffff_handle_t **handle, int card, int bank, int file);
int snd_instr_iwffff_open_rom_file(snd_iwffff_handle_t **handle, const char *name, int bank, int file);
int snd_instr_iwffff_close(snd_iwffff_handle_t *handle);
int snd_instr_iwffff_load(snd_iwffff_handle_t *handle, int bank, int prg, snd_instr_iwffff_t **iwffff);
int snd_instr_iwffff_convert_to_stream(snd_instr_iwffff_t *iwffff, const char *name, snd_instr_header_t **data, size_t *size);
int snd_instr_iwffff_convert_from_stream(snd_instr_header_t *data, size_t size, snd_instr_iwffff_t **iwffff);
int snd_instr_iwffff_free(snd_instr_iwffff_t *iwffff);

/** \} */

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_INSTR_H */

