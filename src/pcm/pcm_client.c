/*
 *  PCM - Client
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <netdb.h>
#include "pcm_local.h"
#include "aserver.h"

typedef struct {
	snd_pcm_t *handle;
	int data_fd;
	int ctrl_fd;
	union {
		struct {
			void *ctrl;
		} shm;
	} u;
} snd_pcm_client_t;

int receive_fd(int socket, void *data, size_t len, int *fd)
{
    int ret;
    size_t cmsg_len = CMSG_LEN(sizeof(int));
    struct cmsghdr *cmsg = alloca(cmsg_len);
    int *fds = (int *) CMSG_DATA(cmsg);
    struct msghdr msghdr;
    struct iovec vec;

    vec.iov_base = (void *)&data;
    vec.iov_len = len;

    cmsg->cmsg_len = cmsg_len;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    *fds = -1;

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &vec;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = cmsg;
    msghdr.msg_controllen = cmsg_len;
    msghdr.msg_flags = 0;

    ret = recvmsg(socket, &msghdr, 0);
    if (ret < 0)
	    return -errno;
    *fd = *fds;
    return ret;
}

static void clean_state(snd_pcm_client_t *client)
{
	struct pollfd pfd;
	int err;
	char buf[1];
	pfd.fd = client->data_fd;
	switch (client->handle->stream) {
	case SND_PCM_STREAM_PLAYBACK:
		pfd.events = POLLOUT;
		while (1) {
			err = poll(&pfd, 1, 0);
			if (err == 0)
				break;
			assert(err > 0);
			err = write(client->data_fd, buf, 1);
			assert(err == 1);
		}
		break;
	case SND_PCM_STREAM_CAPTURE:
		pfd.events = POLLIN;
		while (1) {
			err = poll(&pfd, 1, 0);
			if (err == 0)
				break;
			assert(err > 0);
			err = read(client->data_fd, buf, 1);
			assert(err == 1);
		}
		break;
	}
}

static int snd_pcm_client_shm_action(snd_pcm_client_t *client)
{
	int err;
	char buf[1];
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	err = write(client->ctrl_fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	err = read(client->ctrl_fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	if (ctrl->cmd) {
		fprintf(stderr, "Server has not done the cmd\n");
		return -EBADFD;
	}
	return 0;
}

static int snd_pcm_client_shm_action_fd(snd_pcm_client_t *client)
{
	int err;
	char buf[1];
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int fd;
	err = write(client->ctrl_fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	err = receive_fd(client->ctrl_fd, buf, 1, &fd);
	if (err != 1)
		return -EBADFD;
	if (ctrl->cmd) {
		fprintf(stderr, "Server has not done the cmd\n");
		return -EBADFD;
	}
	if (ctrl->result < 0)
		return ctrl->result;
	return fd;
}

static int snd_pcm_client_shm_close(void *private)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int result;
	ctrl->cmd = SND_PCM_IOCTL_CLOSE;
	result = snd_pcm_client_shm_action(client);
	if (result >= 0)
		result = ctrl->result;
	shmdt((void *)ctrl);
	close(client->data_fd);
	close(client->ctrl_fd);
	return result;
}

static int snd_pcm_client_shm_nonblock(void *private, int nonblock)
{
	/* FIXME */
	return 0;
}

static int snd_pcm_client_shm_info(void *private, snd_pcm_info_t * info)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
//	ctrl->u.info = *info;
	ctrl->cmd = SND_PCM_IOCTL_INFO;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	memcpy(info, &ctrl->u.info, sizeof(*info));
	return ctrl->result;
}

static int snd_pcm_client_shm_params_info(void *private, snd_pcm_params_info_t * info)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PARAMS_INFO;
	ctrl->u.params_info = *info;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	*info = ctrl->u.params_info;
	return ctrl->result;
}

static int snd_pcm_client_shm_params(void *private, snd_pcm_params_t * params)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PARAMS;
	ctrl->u.params = *params;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	*params = ctrl->u.params;
	return ctrl->result;
}

static int snd_pcm_client_shm_setup(void *private, snd_pcm_setup_t * setup)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_SETUP;
	// ctrl->u.setup = *setup;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	*setup = ctrl->u.setup;
	return ctrl->result;
}

static int snd_pcm_client_shm_channel_info(void *private, snd_pcm_channel_info_t * info)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_CHANNEL_INFO;
	ctrl->u.channel_info = *info;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	*info = ctrl->u.channel_info;
	return ctrl->result;
}

static int snd_pcm_client_shm_channel_params(void *private, snd_pcm_channel_params_t * params)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_CHANNEL_PARAMS;
	ctrl->u.channel_params = *params;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	*params = ctrl->u.channel_params;
	return ctrl->result;
}

static int snd_pcm_client_shm_channel_setup(void *private, snd_pcm_channel_setup_t * setup)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_CHANNEL_SETUP;
	ctrl->u.channel_setup = *setup;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	*setup = ctrl->u.channel_setup;
	return ctrl->result;
}

static int snd_pcm_client_shm_status(void *private, snd_pcm_status_t * status)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_STATUS;
	// ctrl->u.status = *status;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	*status = ctrl->u.status;
	return ctrl->result;
}

static int snd_pcm_client_shm_state(void *private)
{
	snd_pcm_status_t status;
	int err = snd_pcm_client_shm_status(private, &status);
	if (err < 0)
		return err;
	return status.state;
}

static ssize_t snd_pcm_client_shm_frame_io(void *private, int update)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_FRAME_IO;
	ctrl->u.frame_io = update;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_prepare(void *private)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PREPARE;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_go(void *private)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_GO;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_drain(void *private)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_DRAIN;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_flush(void *private)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_FLUSH;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_pause(void *private, int enable)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PAUSE;
	ctrl->u.pause = enable;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static ssize_t snd_pcm_client_shm_frame_data(void *private, off_t offset)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_FRAME_DATA;
	ctrl->u.frame_data = offset;
	clean_state(client);
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static ssize_t snd_pcm_client_shm_write(void *private, snd_timestamp_t *tstamp, const void *buffer, size_t size)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	size_t maxsize = PCM_SHM_DATA_MAXLEN;
	size_t bytes = snd_pcm_frames_to_bytes(client->handle, size);
	int err;
	if (bytes > maxsize)
		return -EINVAL;
	ctrl->cmd = SND_PCM_IOCTL_WRITE_FRAMES;
//	ctrl->u.write.tstamp = *tstamp;
	ctrl->u.write.count = size;
	memcpy(ctrl->data, buffer, bytes);
	clean_state(client);
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static ssize_t snd_pcm_client_shm_writev(void *private, snd_timestamp_t *tstamp, const struct iovec *vector, unsigned long count)
{
	/* FIXME: interleaved */
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	size_t vecsize = count * sizeof(struct iovec);
	size_t maxsize = PCM_SHM_DATA_MAXLEN;
	int bits_per_sample = client->handle->bits_per_sample;
	char *base;
	struct iovec *vec;
	unsigned long k;
	size_t ofs;
	int err;
	if (vecsize > maxsize)
		return -EINVAL;
	maxsize -= vecsize;
	ctrl->cmd = SND_PCM_IOCTL_WRITEV_FRAMES;
//	ctrl->u.writev.tstamp = *tstamp;
	ctrl->u.writev.count = count;
	memcpy(ctrl->data, vector, vecsize);
	vec = (struct iovec *) ctrl->data;
	base = ctrl->data + vecsize;
	ofs = 0;
	for (k = 0; k < count; ++k) {
		size_t len = vector[k].iov_len * bits_per_sample / 8;
		memcpy(base + ofs, vector[k].iov_base, len);
		vec[k].iov_base = (void *) ofs;
		ofs += len;
	}
	clean_state(client);
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
}

static ssize_t snd_pcm_client_shm_read(void *private, snd_timestamp_t *tstamp, void *buffer, size_t size)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	size_t maxsize = PCM_SHM_DATA_MAXLEN;
	size_t bytes = snd_pcm_frames_to_bytes(client->handle, size);
	int err;
	if (bytes > maxsize)
		return -EINVAL;
	ctrl->cmd = SND_PCM_IOCTL_READ_FRAMES;
//	ctrl->u.read.tstamp = *tstamp;
	ctrl->u.read.count = size;
	clean_state(client);
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	if (ctrl->result <= 0)
		return ctrl->result;
	bytes = snd_pcm_frames_to_bytes(client->handle, ctrl->result);
	memcpy(buffer, ctrl->data, bytes);
	return ctrl->result;
}

ssize_t snd_pcm_client_shm_readv(void *private, snd_timestamp_t *tstamp, const struct iovec *vector, unsigned long count)
{
	/* FIXME: interleaved */
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	size_t vecsize = count * sizeof(struct iovec);
	size_t maxsize = PCM_SHM_DATA_MAXLEN;
	int bits_per_sample = client->handle->bits_per_sample;
	char *base;
	struct iovec *vec;
	unsigned long k;
	size_t ofs, bytes;
	int err;
	if (vecsize > maxsize)
		return -EINVAL;
	maxsize -= vecsize;
	ctrl->cmd = SND_PCM_IOCTL_WRITEV_FRAMES;
//	ctrl->u.writev.tstamp = *tstamp;
	ctrl->u.writev.count = count;
	memcpy(ctrl->data, vector, vecsize);
	vec = (struct iovec *) ctrl->data;
	base = ctrl->data + vecsize;
	ofs = 0;
	for (k = 0; k < count; ++k) {
		size_t len = vector[k].iov_len * bits_per_sample / 8;
		vec[k].iov_base = (void *) ofs;
		ofs += len;
	}
	clean_state(client);
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	if (ctrl->result <= 0)
		return ctrl->result;
	bytes = snd_pcm_frames_to_bytes(client->handle, ctrl->result);
	ofs = 0;
	for (k = 0; k < count; ++k) {
		/* FIXME: optimize partial read */
		size_t len = vector[k].iov_len * bits_per_sample / 8;
		memcpy(vector[k].iov_base, base + ofs, len);
		ofs += len;
	}
	return ctrl->result;
}

static int snd_pcm_client_shm_mmap_status(void *private, snd_pcm_mmap_status_t **status)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	void *ptr;
	int fd;
	ctrl->cmd = SND_PCM_IOCTL_MMAP_STATUS;
	fd = snd_pcm_client_shm_action_fd(client);
	if (fd < 0)
		return fd;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_status_t), PROT_READ, MAP_FILE|MAP_SHARED, 
		   fd, SND_PCM_MMAP_OFFSET_STATUS);
	close(fd);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	*status = ptr;
	return 0;
}

static int snd_pcm_client_shm_mmap_control(void *private, snd_pcm_mmap_control_t **control)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	void *ptr;
	int fd;
	ctrl->cmd = SND_PCM_IOCTL_MMAP_CONTROL;
	fd = snd_pcm_client_shm_action_fd(client);
	if (fd < 0)
		return fd;
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		   fd, SND_PCM_MMAP_OFFSET_CONTROL);
	close(fd);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	*control = ptr;
	return 0;
}

static int snd_pcm_client_shm_mmap_data(void *private, void **buffer, size_t bsize ATTRIBUTE_UNUSED)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	void *ptr;
	int prot;
	int fd;
	ctrl->cmd = SND_PCM_IOCTL_MMAP_DATA;
	fd = snd_pcm_client_shm_action_fd(client);
	if (fd < 0)
		return fd;
	prot = client->handle->stream == SND_PCM_STREAM_PLAYBACK ? PROT_WRITE : PROT_READ;
	ptr = mmap(NULL, bsize, prot, MAP_FILE|MAP_SHARED, 
		     fd, SND_PCM_MMAP_OFFSET_DATA);
	close(fd);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	*buffer = ptr;
	return 0;
}

static int snd_pcm_client_shm_munmap_status(void *private ATTRIBUTE_UNUSED, snd_pcm_mmap_status_t *status ATTRIBUTE_UNUSED)
{
#if 0
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_MUNMAP_STATUS;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
#else
	if (munmap(status, sizeof(*status)) < 0)
		return -errno;
	return 0;
#endif
}

static int snd_pcm_client_shm_munmap_control(void *private ATTRIBUTE_UNUSED, snd_pcm_mmap_control_t *control ATTRIBUTE_UNUSED)
{
#if 0
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_MUNMAP_CONTROL;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
#else
	if (munmap(control, sizeof(*control)) < 0)
		return -errno;
	return 0;
#endif
}

static int snd_pcm_client_shm_munmap_data(void *private ATTRIBUTE_UNUSED, void *buffer, size_t bsize)
{
#if 0
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_MUNMAP_DATA;
	err = snd_pcm_client_shm_action(client);
	if (err < 0)
		return err;
	return ctrl->result;
#else
	if (munmap(buffer, bsize) < 0)
		return -errno;
	return 0;
#endif
}

static int snd_pcm_client_file_descriptor(void *private)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	return client->data_fd;
}

static int snd_pcm_client_channels_mask(void *private ATTRIBUTE_UNUSED,
					bitset_t *client_vmask ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_client_dump(void *private, FILE *fp)
{
	snd_pcm_client_t *client = (snd_pcm_client_t*) private;
	snd_pcm_t *handle = client->handle;
	fprintf(fp, "Client PCM\n");
	if (handle->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(handle, fp);
	}
}

struct snd_pcm_ops snd_pcm_client_ops = {
	close: snd_pcm_client_shm_close,
	info: snd_pcm_client_shm_info,
	params_info: snd_pcm_client_shm_params_info,
	params: snd_pcm_client_shm_params,
	setup: snd_pcm_client_shm_setup,
	dump: snd_pcm_client_dump,
};

struct snd_pcm_fast_ops snd_pcm_client_fast_ops = {
	nonblock: snd_pcm_client_shm_nonblock,
	channel_info: snd_pcm_client_shm_channel_info,
	channel_params: snd_pcm_client_shm_channel_params,
	channel_setup: snd_pcm_client_shm_channel_setup,
	status: snd_pcm_client_shm_status,
	frame_io: snd_pcm_client_shm_frame_io,
	state: snd_pcm_client_shm_state,
	prepare: snd_pcm_client_shm_prepare,
	go: snd_pcm_client_shm_go,
	drain: snd_pcm_client_shm_drain,
	flush: snd_pcm_client_shm_flush,
	pause: snd_pcm_client_shm_pause,
	frame_data: snd_pcm_client_shm_frame_data,
	write: snd_pcm_client_shm_write,
	writev: snd_pcm_client_shm_writev,
	read: snd_pcm_client_shm_read,
	readv: snd_pcm_client_shm_readv,
	mmap_status: snd_pcm_client_shm_mmap_status,
	mmap_control: snd_pcm_client_shm_mmap_control,
	mmap_data: snd_pcm_client_shm_mmap_data,
	munmap_status: snd_pcm_client_shm_munmap_status,
	munmap_control: snd_pcm_client_shm_munmap_control,
	munmap_data: snd_pcm_client_shm_munmap_data,
	file_descriptor: snd_pcm_client_file_descriptor,
	channels_mask: snd_pcm_client_channels_mask,
};

static int make_local_socket(const char *filename)
{
	size_t l = strlen(filename);
	size_t size = offsetof(struct sockaddr_un, sun_path) + l;
	struct sockaddr_un *addr = alloca(size);
	int sock;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0)
		return -errno;
	
	addr->sun_family = AF_LOCAL;
	memcpy(addr->sun_path, filename, l);

	if (connect(sock, (struct sockaddr *) addr, size) < 0)
		return -errno;
	return sock;
}

static int make_inet_socket(const char *host, int port)
{
	struct sockaddr_in addr;
	int sock;
	struct hostent *h = gethostbyname(host);
	if (!h)
		return -ENOENT;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0)
		return -errno;
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	memcpy(&addr.sin_addr, h->h_addr_list[0], sizeof(struct in_addr));

	if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0)
		return -errno;
	return sock;
}

/* port == -1 -> PF_LOCAL and host is the socket name */
int snd_pcm_client_create(snd_pcm_t **handlep, char *host, int port, int transport, char *name, int stream, int mode)
{
	snd_pcm_t *handle;
	snd_pcm_client_t *client;
	snd_client_open_request_t *req;
	snd_client_open_answer_t ans;
	size_t namelen, reqlen;
	int err;
	int result;
	int fds[2] = {-1, -1};
	int k;
	snd_pcm_client_shm_t *ctrl = NULL;
	uint32_t rcookie, scookie = getpid();
	namelen = strlen(name);
	if (namelen > 255)
		return -EINVAL;

	for (k = 0; k < 2; ++k) {
		if (port == -1)
			fds[k] = make_local_socket(host);
		else
			fds[k] = make_inet_socket(host, port);
		if (fds[k] < 0) {
			result = fds[k];
			goto _err;
		}
		err = write(fds[k], &scookie, sizeof(scookie));
		if (err != sizeof(scookie)) {
			result = -EBADFD;
			goto _err;
		}
		err = read(fds[k], &rcookie, sizeof(rcookie));
		if (err != sizeof(rcookie) ||
		    rcookie != scookie) {
			result = -EBADFD;
			goto _err;
		}
	}

	reqlen = sizeof(*req) + namelen;
	req = alloca(reqlen);
	memcpy(req->name, name, namelen);
	req->dev_type = SND_DEV_TYPE_PCM;
	req->transport_type = transport;
	req->stream = stream;
	req->mode = mode;
	req->namelen = namelen;
	err = write(fds[1], req, reqlen);
	if (err < 0) {
		result = -errno;
		goto _err;
	}
	if ((size_t) err != reqlen) {
		result = -EINVAL;
		goto _err;
	}
	err = read(fds[1], &ans, sizeof(ans));
	if (err < 0) {
		result = -errno;
		goto _err;
	}
	if (err != sizeof(ans)) {
		result = -EINVAL;
		goto _err;
	}
	result = ans.result;
	if (result < 0)
		goto _err;

	switch (transport) {
	case SND_TRANSPORT_TYPE_SHM:
		ctrl = shmat(ans.cookie, 0, 0);
		if (!ctrl) {
			result = -errno;
			goto _err;
		}
		break;
	default:
		result = -ENOSYS;
		goto _err;
	}
		
	if (stream == SND_PCM_STREAM_PLAYBACK) {
		struct pollfd pfd;
		char buf[1];
		int bufsize = 1;
		pfd.fd = fds[0];
		pfd.events = POLLOUT;
		err = setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
		if (err < 0) {
			result = -errno;
			goto _err;
		}
		if (poll(&pfd, 1, 0) != 1) {
			result = -errno;
			goto _err;
		}
		while (1) {
			err = write(fds[0], buf, 1);
			if (err != 1) {
				result = -errno;
				goto _err;
			}
			err = poll(&pfd, 1, 0);
			if (err < 0) {
				result = -errno;
				goto _err;
			}
			if (err == 0)
				break;
		}
	}

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		result = -ENOMEM;
		goto _err;
	}
	client = calloc(1, sizeof(snd_pcm_client_t));
	if (!handle) {
		free(handle);
		result = -ENOMEM;
		goto _err;
	}

	client->handle = handle;
	client->data_fd = fds[0];
	client->ctrl_fd = fds[1];
	switch (transport) {
	case SND_TRANSPORT_TYPE_SHM:
		client->u.shm.ctrl = ctrl;
		break;
	}
	handle->type = SND_PCM_TYPE_CLIENT;
	handle->stream = stream;
	handle->ops = &snd_pcm_client_ops;
	handle->op_arg = client;
	handle->fast_ops = &snd_pcm_client_fast_ops;
	handle->fast_op_arg = client;
	handle->mode = mode;
	handle->private = client;
	*handlep = handle;
	return 0;

 _err:
	if (fds[0] >= 0)
		close(fds[0]);
	if (fds[1] >= 0)
		close(fds[1]);
	switch (transport) {
	case SND_TRANSPORT_TYPE_SHM:
		if (ctrl)
			shmdt(ctrl);
		break;
	}
	return result;
}

