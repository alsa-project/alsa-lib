/*
 *  InterWave FFFF Format Support
 *  Copyright (c) 1999 by Jaroslav Kysela <perex@suse.cz>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include "asoundlib.h"
#include <linux/ainstr_iw.h>

/*
 *  defines
 */

#define IW_RAM_FILE		"/proc/asound/%i/gus-ram-%i"
#define IW_ROM_FILE		"/proc/asound/%i/gus-rom-%i"

#undef IW_ROM_DEBUG

#define IW_ROM_PROGRAM_VERSION_MAJOR	1
#define IW_ROM_PROGRAM_VERSION_MINOR	0

#define IW_FORMAT_8BIT		0x01
#define IW_FORMAT_SIGNED	0x02
#define IW_FORMAT_FORWARD	0x04
#define IW_FORMAT_LOOP		0x08
#define IW_FORMAT_BIDIR		0x10
#define IW_FORMAT_ULAW		0x20
#define IW_FORMAT_ROM		0x80

/*
 *  structures
 */

typedef __u32 ID;

struct header {
	ID id;
	__u32 length;
};

struct program {
	/* 00 */ ID id;
	/* 04 */ ID version;
};

#define PATCH_SIZE 20

struct patch {
	/* 00 */ ID id;
	/* 04 */ __u16 nlayers;
	/* 06 */ __u8 layer_mode;
	/* 07 */ __u8 exclusion_mode;
	/* 08 */ __u16 exclusion_group;
	/* 10 */ __u8 effect1;
	/* 11 */ __u8 effect1_depth;
	/* 12 */ __u8 effect2;
	/* 13 */ __u8 effect2_depth;
	/* 14 */ __u8 bank;
	/* 15 */ __u8 program;
	/* 16 */ char *__layer;		/* don't use */
};

#define LFO_SIZE 8

struct LFO {
	/* 00 */ __u16 freq;
	/* 02 */ __s16 depth;
	/* 04 */ __s16 sweep;
	/* 06 */ __u8 shape;
	/* 07 */ __u8 delay;
};

#define LAYER_SIZE 42

struct layer {
	/* 00 */ ID id;
	/* 04 */ __u8 nwaves;
	/* 05 */ __u8 flags;
	/* 06 */ __u8 high_range;
	/* 07 */ __u8 low_range;
	/* 08 */ __u8 pan;
	/* 09 */ __u8 pan_freq_scale;
	/* 10 */ struct LFO tremolo;
	/* 18 */ struct LFO vibrato;
	/* 26 */ __u8 velocity_mode;
	/* 27 */ __u8 attenuation;
	/* 28 */ __u16 freq_scale;
	/* 30 */ __u8 freq_center;
	/* 31 */ __u8 layer_event;
	/* 32 */ ID penv;
	/* 34 */ ID venv;
	/* 38 */ char *wave;
};

#define WAVE_SIZE 37

struct wave {
	/* 00 */ ID id;
	/* 04 */ __u32 size;
	/* 08 */ __u32 start;
	/* 12 */ __u32 loopstart;
	/* 16 */ __u32 loopend;
	/* 20 */ __u32 m_start;
	/* 24 */ __u32 sample_ratio;
	/* 28 */ __u8 attenuation;
	/* 29 */ __u8 low_note;
	/* 30 */ __u8 high_note;
	/* 31 */ __u8 format;
	/* 32 */ __u8 m_format;
	/* 33 */ ID data_id;
};

#define ENVELOPE_SIZE 8

struct envelope {
	/* 00 */ ID id;
	/* 04 */ __u8 num_envelopes;
	/* 05 */ __u8 flags;
	/* 06 */ __u8 mode;
	/* 07 */ __u8 index_type;
};

#define ENVELOPE_RECORD_SIZE 12

struct envelope_record {
	/* 00 */ __u16 nattack;
	/* 02 */ __u16 nrelease;
	/* 04 */ __u16 sustain_offset;
	/* 06 */ __u16 sustain_rate;
	/* 08 */ __u16 release_rate;
	/* 10 */ __u8 hirange;
	/* 11 */ __u8 pad;
};

/*
 *  variables
 */

#define IW_ID_VALUE(a, b, c, d) (a|(b<<8)|(c<<16)|(d<<24))

#define file_header	IW_ID_VALUE('F', 'F', 'F', 'F')
#define patch_header	IW_ID_VALUE('P', 'T', 'C', 'H')
#define program_header	IW_ID_VALUE('P', 'R', 'O', 'G')
#define layer_header	IW_ID_VALUE('L', 'A', 'Y', 'R')
#define wave_header	IW_ID_VALUE('W', 'A', 'V', 'E')
#define envp_header	IW_ID_VALUE('E', 'N', 'V', 'P')
#if 0
#define text_header	IW_ID_VALUE('T', 'E', 'X', 'T')
#define data_header	IW_ID_VALUE('D', 'A', 'T', 'A')
#define copyright_header IW_ID_VALUE('C', 'P', 'R', 'T')
#endif

struct snd_iwffff_handle {
	int rom;
	unsigned char *fff_data;
	unsigned int fff_size;
	char *fff_filename;
	char *dat_filename;
	unsigned int start_addr;
	unsigned int share_id1;
	unsigned int share_id2;
	unsigned int share_id3;
};

/*
 *  local functions
 */

static int iwffff_get_rom_header(int card, int bank, iwffff_rom_header_t *header)
{
	int fd;
	char filename[128];
	
	sprintf(filename, IW_ROM_FILE, card, bank);
	if ((fd = open(filename, O_RDONLY)) < 0)
		return -errno;
	if (read(fd, header, sizeof(*header)) != sizeof(*header)) {
		close(fd);
		return -EIO;
	}
	return fd;
}

int snd_instr_iwffff_open(snd_iwffff_handle_t **handle, const char *name_fff, const char *name_dat)
{
	snd_iwffff_handle_t *iwf;
	struct stat info;
	struct header ffff;
	int fd;

	if (handle == NULL)
		return -EINVAL;
	*handle = NULL;
	if (stat(name_fff, &info) < 0)
		return -ENOENT;
	if (stat(name_dat, &info) < 0)
		return -ENOENT;
	/* ok.. at first - look for FFFF ROM */
	if ((fd = open(name_fff, O_RDONLY)) < 0)
		return -errno;
	if (read(fd, &ffff, sizeof(ffff)) != sizeof(ffff)) {
		close(fd);
		return -EIO;
	}
	if ((iwf = malloc(sizeof(*iwf))) == NULL) {
		close(fd);
		return -ENOMEM;
	}
	memset(iwf, 0, sizeof(*iwf));
	iwf->fff_size = snd_LE_to_host_32(ffff.length);
	iwf->fff_filename = strdup(name_fff);
	iwf->dat_filename = strdup(name_dat);
	iwf->fff_data = malloc(iwf->fff_size);
	if (iwf->fff_data == NULL) {
		free(iwf);
		close(fd);
		return -ENOMEM;
	}
	if (read(fd, iwf->fff_data, iwf->fff_size) != iwf->fff_size) {
		free(iwf->fff_data);
		free(iwf);
		close(fd);
		return -EIO;
	}
	iwf->share_id1 = IWFFFF_SHARE_FILE;
#ifdef __alpha__
	iwf->share_id2 = (info.st_dev << 16) | ((info.st_ino >> 32) & 0xffff);
#else
	iwf->share_id2 = info.st_dev << 16;
#endif
	iwf->share_id3 = info.st_ino;	/* can be 64-bit -> overflow */
	*handle = iwf;
	return 0;
}

int snd_instr_iwffff_open_rom(snd_iwffff_handle_t **handle, int card, int bank, int file)
{
	unsigned int next_ffff;
	struct header ffff;
	snd_iwffff_handle_t *iwf;
	iwffff_rom_header_t header;
	int fd, index;

 	if (handle == NULL)
 		return -EINVAL;
	*handle = NULL;
	index = 0;
	if (bank > 4 || file > 255)
		return -1; 
	fd = iwffff_get_rom_header(card, bank, &header);
	if (fd < 0)
		return fd;
	while (read(fd, (char *)&ffff, sizeof(ffff)) == sizeof(ffff)) {
		if (ffff.id != file_header)
			break;
		ffff.length = snd_LE_to_host_32(ffff.length);
		next_ffff = lseek(fd, 0, SEEK_CUR) + ffff.length;
		if (file == index) {
#ifdef IW_ROM_DEBUG
			fprintf(stderr, "file header at 0x%x size 0x%x\n", rom_pos - sizeof(ffff), ffff.length);
#endif
			iwf = malloc(sizeof(*iwf));
			if (iwf == NULL) {
				close(fd);
				return -ENOMEM;
			}
			memset(iwf, 0, sizeof(*iwf));
			iwf->fff_data = malloc(iwf->fff_size = ffff.length);
			if (iwf->fff_data == NULL) {
				free(iwf);
				close(fd);
				return -ENOMEM;
			}
			if (read(fd, iwf->fff_data, ffff.length) != ffff.length) {
				free(iwf->fff_data);
				free(iwf);
				close(fd);
				return -ENOMEM;
			}
			close(fd);
			iwf->start_addr = bank*4L*1024L*1024L;
			iwf->rom = 1;
			*handle = iwf;
			return 0;
        	}
		index++;
		lseek(fd, SEEK_CUR, next_ffff);
	}
	close(fd);
	return -ENOENT;
}

int snd_instr_iwffff_close(snd_iwffff_handle_t *iwf)
{
	if (iwf == NULL)
		return -EINVAL;
	if (iwf->dat_filename)
		free(iwf->dat_filename);
	if (iwf->fff_filename)
		free(iwf->fff_filename);
	if (iwf->fff_data)
		free(iwf->fff_data);
	free(iwf);
	return 0;
}

static void free_wave(iwffff_wave_t *wave)
{
	if (wave == NULL)
		return;
	if (wave->address.ptr != NULL)
		free(wave->address.ptr);
	free(wave);
}

static void free_envelope(iwffff_env_t *envp)
{
	iwffff_env_record_t *rec, *nrec;
	
	if (envp == NULL)
		return;
	rec = envp->record;
	while (rec != NULL) {
		nrec = rec->next;
		free(rec);
		rec = nrec;
	}
	free(envp);
}

static void free_layer(iwffff_layer_t *layer)
{
	iwffff_wave_t *wave, *nwave;

	if (layer == NULL)
		return;

	free_envelope(&layer->penv);
	free_envelope(&layer->venv);
	
	wave = layer->wave;
	while (wave != NULL) {
		nwave = wave->next;
		free_wave(wave);
		wave = nwave;
	}
	
	free(layer);
}

int snd_instr_iwffff_free(snd_instr_iwffff_t *__instr)
{
	iwffff_instrument_t *instr = (iwffff_instrument_t *)__instr; 
	iwffff_layer_t *layer, *nlayer;
	
	if (instr == NULL)
		return -EINVAL;
	layer = instr->layer;
	while (layer != NULL) {
		nlayer = layer->next;
		free_layer(layer);
		layer = nlayer;
	}
	free(instr);
	return 0;
}

static char *look_for_id(snd_iwffff_handle_t *iwf, unsigned char *start,
			 unsigned char *end, ID id)
{
	if (!start)
		return NULL;
	while ((long)start < (long)end) {
		if (((struct header *)start)->id == id)
			return start;
		start += sizeof(struct header) + snd_LE_to_host_32(((struct header *)start)->length);
	}
	return NULL;
}

static void copy_modulation(iwffff_lfo_t *glfo, unsigned char *buffer)
{
	glfo->freq  = snd_LE_to_host_16(*(((unsigned short *)buffer) + 0/2));
	glfo->depth = snd_LE_to_host_16(*(((unsigned short *)buffer) + 2/1));
	glfo->sweep = snd_LE_to_host_16(*(((unsigned short *)buffer) + 4/2));
	glfo->shape = buffer[6];
	glfo->delay = buffer[7];
}

static int copy_envelope(snd_iwffff_handle_t *iwf, iwffff_env_t *genv, ID eid)
{
	int idx, idx1;
	unsigned char *ptr, *end;
	unsigned char *envelope, *record;
	iwffff_env_record_t *grecord, *precord;
	unsigned short nattack, nrelease, *points;
	iwffff_env_point_t *rpoints;
  
	if (eid == 0)
		return -EINVAL;
	ptr = iwf->fff_data;
	end = iwf->fff_data + iwf->fff_size;
	while (1) {
		ptr = look_for_id(iwf, ptr, end, envp_header);
		if (ptr == NULL)
			return -ENOENT;
		envelope = ptr + sizeof(struct header);
		if (*((ID *)envelope) == eid) {
			genv->flags = envelope[5];
			genv->mode = envelope[6];
			genv->index = envelope[7];
			genv->record = precord = NULL;
			record = envelope + ENVELOPE_SIZE;
			for (idx = 0; idx < envelope[4]; idx++) {
				nattack = snd_LE_to_host_16(*(((unsigned short *)record) + 0/2));
				nrelease = snd_LE_to_host_16(*(((unsigned short*)record) + 2/2));
				grecord = calloc(1, sizeof(*grecord) + (nattack + nrelease) * sizeof(iwffff_env_point_t));
				if (grecord == NULL)
					return -ENOMEM;
				grecord->nattack = nattack;
				grecord->nrelease = nrelease;
				grecord->sustain_offset = snd_LE_to_host_16(*(((unsigned short*)record) + 4/2));
				grecord->sustain_rate = snd_LE_to_host_16(*(((unsigned short *)record) + 6/2));
				grecord->release_rate = snd_LE_to_host_16(*(((unsigned short *)record) + 8/2));
				grecord->hirange = record[10];
				points = (short *)(record + ENVELOPE_RECORD_SIZE);
				rpoints = (iwffff_env_point_t *)(grecord + 1);
				for (idx1 = 0; idx1 < grecord->nattack + grecord->nrelease; idx1++) {
					rpoints[idx1].offset = *points++;
					rpoints[idx1].rate = *points++;
				}
				grecord->next = NULL;
				if (genv->record == NULL) {
					genv->record = grecord;
				} else {
					precord->next = grecord;
				}
				precord = grecord;
				record += ENVELOPE_RECORD_SIZE + 2 * sizeof(short) * (grecord->nattack + grecord->nrelease);
			}
			return 0;
		}
		ptr += sizeof(struct header) + snd_LE_to_host_32(((struct header *)ptr)->length);
	}
}

static int load_iw_wave(snd_iwffff_handle_t *file,
			unsigned int start,
			unsigned int size,
			unsigned char **result)
{
	int fd, res;
  
	*result = NULL;
	if ((fd = open(file->dat_filename, O_RDONLY)) < 0)
		return -errno;
	if (lseek(fd, start, SEEK_SET) < 0) {
		res = -errno;
		close(fd);
		return res;
	}
	*result = malloc(size);
	if (*result == NULL) {
		close(fd);
		return -ENOMEM;
	}
	if (read(fd, result, size) != size) {
		free(*result);
		*result = NULL;
		close(fd);
		return -errno;
	}
	close(fd);
	return 0;
}

static int load_iw_patch(snd_iwffff_handle_t *iwf, iwffff_instrument_t *instr,
			 unsigned char *patch)
{
	unsigned char *layer, *wave;
	iwffff_layer_t *glayer, *player;
	iwffff_wave_t *gwave, *pwave;
	int idx_layer, idx_wave, result;
	unsigned char *current;

#ifdef IW_ROM_DEBUG
	fprintf(stderr, "load_iw_patch - nlayers = %i\n", snd_LE_to_host_16(*(((unsigned short *)patch) + 8/2));
#endif
	instr->layer_type = patch[6];
	instr->exclusion = patch[7];
	instr->exclusion_group = snd_LE_to_host_16(*(((unsigned short *)patch) + 8/2));
	instr->effect1 = patch[10];
	instr->effect1_depth = patch[11];
	instr->effect2 = patch[12];
	instr->effect2_depth = patch[13];
	current = (char *)patch + sizeof(struct patch);
	instr->layer = player = NULL;
	for (idx_layer = 0; idx_layer < snd_LE_to_host_16(*(((unsigned short *)patch) + 4/2)); idx_layer++) {
		if (((struct header *)current)->id != layer_header) {
			return -EIO;
		}
		layer = current + sizeof(struct header);
		glayer = calloc(1, sizeof(*glayer));
		glayer->flags = layer[5];
		glayer->high_range = layer[6];
		glayer->low_range = layer[7];
		glayer->pan = layer[8];
		glayer->pan_freq_scale = layer[9];
		copy_modulation(&glayer->tremolo, layer + 10);
		copy_modulation(&glayer->vibrato, layer + 18);
		glayer->velocity_mode = layer[26];
		glayer->attenuation = layer[27];
		glayer->freq_scale = snd_LE_to_host_16(*(((unsigned short *)layer) + 28/2));
		glayer->freq_center = layer[30];
		glayer->layer_event = layer[31];
		if (copy_envelope(iwf, &glayer->penv, *((ID *)(layer + 32))) < 0) {
			free_layer(glayer);
			return -EIO;
		}
		if (copy_envelope(iwf, &glayer->venv, *((ID *)(layer + 36))) < 0) {
			free_layer(glayer);
			return -EIO;
		}
		current += sizeof(struct header) + snd_LE_to_host_32(((struct header *)current)->length);
		glayer->wave = pwave = NULL;
		for (idx_wave = 0; idx_wave < layer[4]; idx_wave++) {
			unsigned char format;
			int w_16;
        
			if (((struct header *)current)->id != wave_header) {
				free_layer(glayer);
				return -EIO;
			}
			wave = current + sizeof(struct header);
			format = wave[31];
			w_16 = format & IW_FORMAT_8BIT ? 1 : 2; 
			gwave = calloc(1, sizeof(*gwave));
			gwave->size = snd_LE_to_host_32(*(((unsigned int *)wave) + 4/4)) * w_16;
			gwave->start = iwf->start_addr;
			gwave->loop_start = snd_LE_to_host_32(*(((unsigned int *)wave) + 12/4)) * w_16;
			gwave->loop_end = snd_LE_to_host_32(*(((unsigned int *)wave) + 16/4)) * w_16;
			gwave->sample_ratio = snd_LE_to_host_32(*(((unsigned int *)wave) + 24/4));
			gwave->attenuation = wave[28];
			gwave->low_note = wave[29];
			gwave->high_note = wave[30];
			if (!(format & IW_FORMAT_8BIT))
				gwave->format |= IWFFFF_WAVE_16BIT;
			if (!(format & IW_FORMAT_SIGNED))
				gwave->format |= IWFFFF_WAVE_UNSIGNED;
			if (!(format & IW_FORMAT_FORWARD))
				gwave->format |= IWFFFF_WAVE_BACKWARD;
			if (format & IW_FORMAT_LOOP)
				gwave->format |= IWFFFF_WAVE_LOOP;
			if (format & IW_FORMAT_BIDIR)
				gwave->format |= IWFFFF_WAVE_BIDIR;
			if (format & IW_FORMAT_ULAW)
				gwave->format |= IWFFFF_WAVE_ULAW;
			if (format & IW_FORMAT_ROM) {
				gwave->format |= IWFFFF_WAVE_ROM;
				gwave->address.memory = snd_LE_to_host_32(*(((unsigned int *)wave) + 8/4));
			} else {
				gwave->share_id[1] = iwf->share_id1;
				gwave->share_id[2] = iwf->share_id2;
				gwave->share_id[3] = iwf->share_id3;
				gwave->share_id[4] = snd_LE_to_host_32(*(((unsigned int *)wave) + 8/4));
				result = load_iw_wave(iwf, gwave->share_id[2], gwave->size, &gwave->address.ptr);
				if (result < 0) {
					free_wave(gwave);
					return result;
				}
			}
			if (glayer->wave == NULL) {
				glayer->wave = gwave;
			} else {
				pwave->next = gwave;
			}
			pwave = gwave;
			current += sizeof(struct header) + snd_LE_to_host_32(((struct header *)current)->length);
		}
		if (instr->layer == NULL) {
			instr->layer = glayer;
		} else {
			player->next = glayer;
		}
		player = glayer;
	}
	return 0;
}

int snd_instr_iwffff_load(snd_iwffff_handle_t *iwf, int bank, int prg, snd_instr_iwffff_t **__iwffff)
{
	unsigned char *ptr, *end;
	unsigned char *program, *patch;
	struct header *header;
	iwffff_instrument_t *iwffff;
	int result;

	if (iwf == NULL || __iwffff == NULL)
		return -EINVAL;
	__iwffff = NULL;
	if (bank < 0 || bank > 255 || prg < 0 || prg > 255)
		return -EINVAL;
	iwffff = (iwffff_instrument_t *)__iwffff;
	ptr = iwf->fff_data;
 	end = iwf->fff_data + iwf->fff_size;
	while (1) {
		ptr = look_for_id(iwf, ptr, end, program_header);
		if (ptr == NULL)
			break;
		program = ptr + sizeof(struct header);
		if (snd_LE_to_host_16(*(((unsigned short *)program) + 4/2)) != IW_ROM_PROGRAM_VERSION_MAJOR ||
		    snd_LE_to_host_16(*(((unsigned short *)program) + 6/2)) != IW_ROM_PROGRAM_VERSION_MINOR)
			return -EINVAL;
		header = (struct header *)(ptr + sizeof(struct header) + sizeof(struct program));
		if (header->id == patch_header) {
			patch = ptr + sizeof(struct header) + sizeof(struct program) + sizeof(struct header);
			if (patch[14] == bank && patch[15] == prg) {
				iwffff = (iwffff_instrument_t *)malloc(sizeof(*iwffff));
				if (iwffff == NULL)
					return -ENOMEM;
				result = load_iw_patch(iwf, iwffff, patch);
				if (result < 0) {
					snd_instr_iwffff_free(iwffff);
				} else {
					*__iwffff = iwffff;
				}
				return result;
			}
		}
		ptr += sizeof(struct header) + snd_LE_to_host_32(((struct header *)ptr)->length);
	}
	return -ENOENT;
}

static int iwffff_evp_record_size(iwffff_env_t *env)
{
	iwffff_env_record_t *rec;
	int size;

	if (env == NULL)
		return 0;
	size = 0;
	for (rec = env->record; rec; rec = rec->next) {
		size += sizeof(iwffff_xenv_record_t);
		size += sizeof(iwffff_xenv_point_t) *
			(rec->nattack + rec->nrelease);
	}
	return size;
}

static int iwffff_size(iwffff_instrument_t *instr)
{
	int size;
	iwffff_layer_t *layer;
	iwffff_wave_t *wave;
	
	if (instr == NULL)
		return 0;
	size = sizeof(iwffff_xinstrument_t);
	for (layer = instr->layer; layer; layer = layer->next) {
		size += sizeof(iwffff_xlayer_t);
		size += iwffff_evp_record_size(&layer->penv);
		size += iwffff_evp_record_size(&layer->venv);
		for (wave = layer->wave; wave; wave = wave->next) {
			size += sizeof(iwffff_xwave_t);
			if (!(wave->format & IWFFFF_WAVE_ROM))
				size += wave->size;
		}
	}
	return size;
}

static void copy_lfo_to_stream(iwffff_xlfo_t *xlfo, iwffff_lfo_t *lfo)
{
	xlfo->freq = snd_htoi_16(lfo->freq);
	xlfo->depth = snd_htoi_16(lfo->depth);
	xlfo->sweep = snd_htoi_16(lfo->sweep);
	xlfo->shape = lfo->shape;
	xlfo->delay = lfo->delay;
	
}

static int copy_env_to_stream(iwffff_xenv_t *xenv, iwffff_env_t *env, __u32 stype)
{
	int size, idx;
	char *ptr;
	iwffff_xenv_record_t *xrec;
	iwffff_env_record_t *rec;
	iwffff_xenv_point_t *xpoint;
	iwffff_env_point_t *point;

	xenv->flags = env->flags;
	xenv->mode = env->mode;
	xenv->index = env->index;
	size = 0;
	ptr = (char *)(xenv + 1);
	for (rec = env->record; rec; rec = rec->next) {
		xrec = (iwffff_xenv_record_t *)ptr;
		ptr += sizeof(*xrec);
		size += sizeof(*xrec);
		xrec->stype = stype;
		xrec->nattack = snd_htoi_16(rec->nattack);
		xrec->nrelease = snd_htoi_16(rec->nrelease);
		xrec->sustain_offset = snd_htoi_16(rec->sustain_offset);
		xrec->sustain_rate = snd_htoi_16(rec->sustain_rate);
		xrec->release_rate = snd_htoi_16(rec->release_rate);
		xrec->hirange = rec->hirange;
		point = (iwffff_env_point_t *)(rec + 1);
		for (idx = 0; idx < xrec->nattack + xrec->nrelease; idx++) {
			xpoint = (iwffff_xenv_point_t *)ptr;
			ptr += sizeof(*xpoint);
			size += sizeof(*xpoint);
			xpoint->offset = snd_htoi_16(point->offset);
			xpoint->rate = snd_htoi_16(point->rate);
			point++;
		}
	}
	return size;
}

int snd_instr_iwffff_conv_to_stream(snd_instr_iwffff_t *iwffff,
				    const char *name,
				    snd_seq_instr_put_t **__data,
				    long *__size)
{
	snd_seq_instr_put_t *put;
	snd_seq_instr_data_t *data;
	int size;
	char *ptr;
	iwffff_instrument_t *instr;
	iwffff_xinstrument_t *xinstr;
	iwffff_layer_t *layer;
	iwffff_xlayer_t *xlayer;
	iwffff_wave_t *wave;
	iwffff_xwave_t *xwave;
	
	if (iwffff == NULL || __data == NULL)
		return -EINVAL;
	instr = (iwffff_instrument_t *)iwffff;
	*__data = NULL;
	*__size = 0;
	size = sizeof(*data) + iwffff_size(iwffff);
	put = (snd_seq_instr_put_t *)malloc(sizeof(*put) + size);
	if (put == NULL)
		return -ENOMEM;
	/* build header */
	bzero(put, sizeof(*put));
	data = &put->data;
	if (name)
		strncpy(data->name, name, sizeof(data->name)-1);
	data->type = SND_SEQ_INSTR_ATYPE_DATA;
	strcpy(data->data.format, SND_SEQ_INSTR_ID_INTERWAVE);
	/* build data section */
	xinstr = (iwffff_xinstrument_t *)(data + 1);
	xinstr->stype = IWFFFF_STRU_INSTR;
	xinstr->exclusion = snd_htoi_16(instr->exclusion);
	xinstr->layer_type = snd_htoi_16(instr->layer_type);
	xinstr->exclusion_group = snd_htoi_16(instr->exclusion_group);
	xinstr->effect1 = instr->effect1;
	xinstr->effect1_depth = instr->effect1_depth;
	xinstr->effect2 = instr->effect2;
	xinstr->effect2_depth = instr->effect2_depth;
	ptr = (char *)(xinstr + 1);
	for (layer = instr->layer; layer; layer = layer->next) {
		xlayer = (iwffff_xlayer_t *)ptr;
		ptr += sizeof(*xlayer);
		xlayer->stype = IWFFFF_STRU_LAYER;
		xlayer->flags = layer->flags;
		xlayer->velocity_mode = layer->velocity_mode;
		xlayer->layer_event = layer->layer_event;
		xlayer->low_range = layer->low_range;
		xlayer->high_range = layer->high_range;
		xlayer->pan = layer->pan;
		xlayer->pan_freq_scale = layer->pan_freq_scale;
		xlayer->attenuation = layer->attenuation;
		copy_lfo_to_stream(&xlayer->tremolo, &layer->tremolo);
		copy_lfo_to_stream(&xlayer->vibrato, &layer->vibrato);
		xlayer->freq_scale = snd_htoi_16(layer->freq_scale);
		xlayer->freq_center = layer->freq_center;
		ptr += copy_env_to_stream(&xlayer->penv, &layer->penv, IWFFFF_STRU_ENV_RECP);
		ptr += copy_env_to_stream(&xlayer->venv, &layer->venv, IWFFFF_STRU_ENV_RECV);
		for (wave = layer->wave; wave; wave = wave->next) {
			xwave = (iwffff_xwave_t *)ptr;
			ptr += sizeof(*xwave);
			xwave->stype = IWFFFF_STRU_WAVE;
			xwave->share_id[1] = snd_htoi_32(wave->share_id[1]);
			xwave->share_id[2] = snd_htoi_32(wave->share_id[2]);
			xwave->share_id[3] = snd_htoi_32(wave->share_id[3]);
			xwave->share_id[4] = snd_htoi_32(wave->share_id[4]);
			xwave->format = snd_htoi_32(wave->format);
			xwave->size = snd_htoi_32(wave->size);
			xwave->start = snd_htoi_32(wave->start);
			xwave->loop_start = snd_htoi_32(wave->loop_start);
			xwave->loop_end = snd_htoi_32(wave->loop_end);
			xwave->loop_repeat = snd_htoi_32(wave->loop_repeat);
			xwave->sample_ratio = snd_htoi_32(wave->sample_ratio);
			xwave->attenuation = wave->attenuation;
			xwave->low_note = wave->low_note;
			xwave->high_note = wave->high_note;
			if (!(xwave->format & IWFFFF_WAVE_ROM)) {
				memcpy(ptr, wave->address.ptr, wave->size);
				ptr += wave->size;
			} else {
				xwave->offset = snd_htoi_32(wave->address.memory);
			}
		}
	}
	/* write result */
	*__data = put;
	*__size = size;
	return 0;
}

int snd_instr_iwffff_convert_from_stream(snd_seq_instr_get_t *data,
					 long size,
					 snd_instr_iwffff_t **iwffff)
{
	/* TODO */
	return -ENXIO;
}
