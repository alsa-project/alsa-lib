/*
 *  ALSA client/server header file
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
  

#include "../src/pcm/pcm_local.h"

#define SND_PCM_IOCTL_CARD		_IO ('A', 0xf0)
#define SND_PCM_IOCTL_STATE		_IO ('A', 0xf1)
#define SND_PCM_IOCTL_MMAP		_IO ('A', 0xf2)
#define SND_PCM_IOCTL_MUNMAP		_IO ('A', 0xf3)
#define SND_PCM_IOCTL_MMAP_FORWARD	_IO ('A', 0xf4)
#define SND_PCM_IOCTL_AVAIL_UPDATE	_IO ('A', 0xf5)
#define SND_PCM_IOCTL_ASYNC		_IO ('A', 0xf6)
#define SND_PCM_IOCTL_CLOSE		_IO ('A', 0xf7)
#define SND_PCM_IOCTL_POLL_DESCRIPTOR	_IO ('A', 0xf8)

typedef struct {
	long result;
	int cmd;
	size_t hw_ptr;
	size_t appl_ptr;
	union {
		struct {
			int sig;
			pid_t pid;
		} async;
		snd_pcm_info_t info;
		snd_pcm_hw_params_t hw_refine;
		snd_pcm_hw_params_t hw_params;
		snd_pcm_sw_params_t sw_params;
		snd_pcm_sw_params_t sw_refine;
		snd_pcm_status_t status;
		struct {
			ssize_t frames;
		} delay;
		struct {
			int enable;
		} pause;
		snd_pcm_channel_info_t channel_info;
		struct {
			ssize_t frames;
		} rewind;
		struct {
			int fd;
		} link;
		struct {
			ssize_t frames;
		} mmap_forward;
	} u;
	char data[0];
} snd_pcm_shm_ctrl_t;

#define PCM_SHM_SIZE sizeof(snd_pcm_shm_ctrl_t)
		
#define SND_CTL_IOCTL_CARD		_IO ('U', 0xf0)
#define SND_CTL_IOCTL_READ		_IOR('U', 0xf1, snd_ctl_event_t)
#define SND_CTL_IOCTL_CLOSE		_IO ('U', 0xf2)
#define SND_CTL_IOCTL_POLL_DESCRIPTOR	_IO ('U', 0xf3)

typedef struct {
	int result;
	int cmd;
	union {
		int device;
		snd_ctl_hw_info_t hw_info;
		snd_control_list_t clist;
		snd_control_info_t cinfo;
		snd_control_t cread;
		snd_control_t cwrite;
		snd_hwdep_info_t hwdep_info;
		snd_pcm_info_t pcm_info;
		int pcm_prefer_subdevice;
		snd_rawmidi_info_t rawmidi_info;
		int rawmidi_prefer_subdevice;
		snd_ctl_event_t read;
	} u;
	char data[0];
} snd_ctl_shm_ctrl_t;

#define CTL_SHM_SIZE 65536
#define CTL_SHM_DATA_MAXLEN (CTL_SHM_SIZE - offsetof(snd_ctl_shm_ctrl_t, data))

typedef struct {
	unsigned char dev_type;
	unsigned char transport_type;
	unsigned char stream;
	unsigned char mode;
	unsigned char namelen;
	char name[0];
} snd_client_open_request_t;

typedef struct {
	long result;
	int cookie;
} snd_client_open_answer_t;

