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
  

#define SND_PCM_IOCTL_MMAP_DATA		_IO ('A', 0xf0)
#define SND_PCM_IOCTL_MMAP_CONTROL	_IO ('A', 0xf1)
#define SND_PCM_IOCTL_MMAP_STATUS	_IO ('A', 0xf2)
#define SND_PCM_IOCTL_MUNMAP_DATA	_IO ('A', 0xf3)
#define SND_PCM_IOCTL_MUNMAP_CONTROL	_IO ('A', 0xf4)
#define SND_PCM_IOCTL_MUNMAP_STATUS	_IO ('A', 0xf5)
#define SND_PCM_IOCTL_CLOSE		_IO ('A', 0xf6)

typedef struct {
	int result;
	int cmd;
	union {
		snd_pcm_info_t info;
		snd_pcm_params_t params;
		snd_pcm_params_info_t params_info;
		snd_pcm_setup_t setup;
		snd_pcm_status_t status;
		int pause;
		snd_pcm_channel_info_t channel_info;
		snd_pcm_channel_params_t channel_params;
		snd_pcm_channel_setup_t channel_setup;
		off_t frame_data;
		int frame_io;
		int link;
		snd_xfer_t read;
		snd_xfer_t write;
		snd_xferv_t readv;
		snd_xferv_t writev;
	} u;
	char data[0];
} snd_pcm_client_shm_t;

#define PCM_SHM_SIZE 65536
#define PCM_SHM_DATA_MAXLEN (PCM_SHM_SIZE - offsetof(snd_pcm_client_shm_t, data))
		
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

struct cmsg_fd
{
    int len;   /* sizeof structure */
    int level; /* SOL_SOCKET */
    int type;  /* SCM_RIGHTS */
    int fd;    /* fd to pass */
};
