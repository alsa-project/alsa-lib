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
#include <sys/mman.h>
#include <netinet/in.h>
#include <netdb.h>
#include "pcm_local.h"
#include "aserver.h"

typedef struct {
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

static void clean_poll(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	struct pollfd pfd;
	int err;
	char buf[1];
	pfd.fd = client->data_fd;
	switch (pcm->stream) {
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

static int snd_pcm_client_shm_action(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	int err;
	char buf[1];
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	clean_poll(pcm);
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

static int snd_pcm_client_shm_action_fd(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	int err;
	char buf[1];
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int fd;
	clean_poll(pcm);
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

static int snd_pcm_client_shm_close(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int result;
	ctrl->cmd = SND_PCM_IOCTL_CLOSE;
	result = snd_pcm_client_shm_action(pcm);
	if (result >= 0)
		result = ctrl->result;
	shmdt((void *)ctrl);
	close(client->data_fd);
	close(client->ctrl_fd);
	free(client);
	return result;
}

static int snd_pcm_client_shm_nonblock(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int nonblock ATTRIBUTE_UNUSED)
{
	return 0;
}

static int snd_pcm_client_async(snd_pcm_t *pcm, int sig, pid_t pid)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_ASYNC;
	ctrl->u.async.sig = sig;
	if (pid == 0)
		pid = getpid();
	ctrl->u.async.pid = pid;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
//	ctrl->u.info = *info;
	ctrl->cmd = SND_PCM_IOCTL_INFO;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*info = ctrl->u.info;
	return ctrl->result;
}

static int snd_pcm_client_shm_params_info(snd_pcm_t *pcm, snd_pcm_params_info_t * info)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PARAMS_INFO;
	ctrl->u.params_info = *info;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*info = ctrl->u.params_info;
	return ctrl->result;
}

static int snd_pcm_client_shm_params(snd_pcm_t *pcm, snd_pcm_params_t * params)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PARAMS;
	ctrl->u.params = *params;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*params = ctrl->u.params;
	return ctrl->result;
}

static int snd_pcm_client_shm_setup(snd_pcm_t *pcm, snd_pcm_setup_t * setup)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_SETUP;
	// ctrl->u.setup = *setup;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*setup = ctrl->u.setup;
	return ctrl->result;
}

static int snd_pcm_client_shm_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_CHANNEL_INFO;
	ctrl->u.channel_info = *info;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*info = ctrl->u.channel_info;
	return ctrl->result;
}

static int snd_pcm_client_shm_channel_params(snd_pcm_t *pcm, snd_pcm_channel_params_t * params)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_CHANNEL_PARAMS;
	ctrl->u.channel_params = *params;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*params = ctrl->u.channel_params;
	return ctrl->result;
}

static int snd_pcm_client_shm_channel_setup(snd_pcm_t *pcm, snd_pcm_channel_setup_t * setup)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_CHANNEL_SETUP;
	ctrl->u.channel_setup = *setup;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*setup = ctrl->u.channel_setup;
	return ctrl->result;
}

static int snd_pcm_client_shm_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_STATUS;
	// ctrl->u.status = *status;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*status = ctrl->u.status;
	return ctrl->result;
}

static int snd_pcm_client_shm_state(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_STATE;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_delay(snd_pcm_t *pcm, ssize_t *delayp)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_DELAY;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	*delayp = ctrl->u.delay;
	return ctrl->result;
}

static ssize_t snd_pcm_client_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_AVAIL_UPDATE;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_prepare(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PREPARE;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_start(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_START;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_drop(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_DROP;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_drain(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_DRAIN;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_PAUSE;
	ctrl->u.pause = enable;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static ssize_t snd_pcm_client_shm_rewind(snd_pcm_t *pcm, size_t frames)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_REWIND;
	ctrl->u.rewind = frames;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_shm_mmap_status(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	void *ptr;
	int fd;
	ctrl->cmd = SND_PCM_IOCTL_MMAP_STATUS;
	fd = snd_pcm_client_shm_action_fd(pcm);
	if (fd < 0)
		return fd;
	/* FIXME: not mmap */
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_status_t), PROT_READ, MAP_FILE|MAP_SHARED, 
		   fd, SND_PCM_MMAP_OFFSET_STATUS);
	close(fd);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	pcm->mmap_status = ptr;
	return 0;
}

static int snd_pcm_client_shm_mmap_control(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	void *ptr;
	int fd;
	ctrl->cmd = SND_PCM_IOCTL_MMAP_CONTROL;
	fd = snd_pcm_client_shm_action_fd(pcm);
	if (fd < 0)
		return fd;
	/* FIXME: not mmap */
	ptr = mmap(NULL, sizeof(snd_pcm_mmap_control_t), PROT_READ|PROT_WRITE, MAP_FILE|MAP_SHARED, 
		   fd, SND_PCM_MMAP_OFFSET_CONTROL);
	close(fd);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	pcm->mmap_control = ptr;
	return 0;
}

static int snd_pcm_client_shm_mmap_data(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	void *ptr;
	int prot;
	int fd;
	ctrl->cmd = SND_PCM_IOCTL_MMAP_DATA;
	fd = snd_pcm_client_shm_action_fd(pcm);
	if (fd < 0)
		return fd;
	/* FIXME: not mmap */
	prot = pcm->stream == SND_PCM_STREAM_PLAYBACK ? PROT_WRITE : PROT_READ;
	ptr = mmap(NULL, pcm->setup.mmap_bytes, prot, MAP_FILE|MAP_SHARED, 
		     fd, SND_PCM_MMAP_OFFSET_DATA);
	close(fd);
	if (ptr == MAP_FAILED || ptr == NULL)
		return -errno;
	pcm->mmap_data = ptr;
	return 0;
}

static int snd_pcm_client_shm_munmap_status(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_MUNMAP_STATUS;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	/* FIXME: not mmap */
	if (munmap((void*)pcm->mmap_status, sizeof(*pcm->mmap_status)) < 0)
		return -errno;
	return ctrl->result;
}

static int snd_pcm_client_shm_munmap_control(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_MUNMAP_CONTROL;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	/* FIXME: not mmap */
	if (munmap(pcm->mmap_control, sizeof(*pcm->mmap_control)) < 0)
		return -errno;
	return ctrl->result;
}

static int snd_pcm_client_shm_munmap_data(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_MUNMAP_DATA;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	/* FIXME: not mmap */
	if (munmap(pcm->mmap_data, pcm->setup.mmap_bytes) < 0)
		return -errno;
	return ctrl->result;
}

static ssize_t snd_pcm_client_mmap_forward(snd_pcm_t *pcm, size_t size)
{
	snd_pcm_client_t *client = pcm->private;
	snd_pcm_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->cmd = SND_PCM_IOCTL_MMAP_FORWARD;
	ctrl->u.mmap_forward = size;
	err = snd_pcm_client_shm_action(pcm);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_pcm_client_poll_descriptor(snd_pcm_t *pcm)
{
	snd_pcm_client_t *client = pcm->private;
	return client->data_fd;
}

static int snd_pcm_client_channels_mask(snd_pcm_t *pcm ATTRIBUTE_UNUSED,
					bitset_t *cmask ATTRIBUTE_UNUSED)
{
	return 0;
}

static void snd_pcm_client_dump(snd_pcm_t *pcm, FILE *fp)
{
	fprintf(fp, "Client PCM\n");
	if (pcm->valid_setup) {
		fprintf(fp, "\nIts setup is:\n");
		snd_pcm_dump_setup(pcm, fp);
	}
}

struct snd_pcm_ops snd_pcm_client_ops = {
	close: snd_pcm_client_shm_close,
	info: snd_pcm_client_shm_info,
	params_info: snd_pcm_client_shm_params_info,
	params: snd_pcm_client_shm_params,
	setup: snd_pcm_client_shm_setup,
	channel_info: snd_pcm_client_shm_channel_info,
	channel_params: snd_pcm_client_shm_channel_params,
	channel_setup: snd_pcm_client_shm_channel_setup,
	dump: snd_pcm_client_dump,
	nonblock: snd_pcm_client_shm_nonblock,
	async: snd_pcm_client_async,
	mmap_status: snd_pcm_client_shm_mmap_status,
	mmap_control: snd_pcm_client_shm_mmap_control,
	mmap_data: snd_pcm_client_shm_mmap_data,
	munmap_status: snd_pcm_client_shm_munmap_status,
	munmap_control: snd_pcm_client_shm_munmap_control,
	munmap_data: snd_pcm_client_shm_munmap_data,
};

struct snd_pcm_fast_ops snd_pcm_client_fast_ops = {
	status: snd_pcm_client_shm_status,
	state: snd_pcm_client_shm_state,
	delay: snd_pcm_client_shm_delay,
	prepare: snd_pcm_client_shm_prepare,
	start: snd_pcm_client_shm_start,
	drop: snd_pcm_client_shm_drop,
	drain: snd_pcm_client_shm_drain,
	pause: snd_pcm_client_shm_pause,
	rewind: snd_pcm_client_shm_rewind,
	writei: snd_pcm_mmap_writei,
	writen: snd_pcm_mmap_writen,
	readi: snd_pcm_mmap_readi,
	readn: snd_pcm_mmap_readn,
	poll_descriptor: snd_pcm_client_poll_descriptor,
	channels_mask: snd_pcm_client_channels_mask,
	avail_update: snd_pcm_client_avail_update,
	mmap_forward: snd_pcm_client_mmap_forward,
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

	client = calloc(1, sizeof(snd_pcm_client_t));
	if (!client) {
		result = -ENOMEM;
		goto _err;
	}

	client->data_fd = fds[0];
	client->ctrl_fd = fds[1];
	switch (transport) {
	case SND_TRANSPORT_TYPE_SHM:
		client->u.shm.ctrl = ctrl;
		break;
	}

	handle = calloc(1, sizeof(snd_pcm_t));
	if (!handle) {
		free(client);
		result = -ENOMEM;
		goto _err;
	}
	handle->type = SND_PCM_TYPE_CLIENT;
	handle->stream = stream;
	handle->ops = &snd_pcm_client_ops;
	handle->op_arg = handle;
	handle->fast_ops = &snd_pcm_client_fast_ops;
	handle->fast_op_arg = handle;
	handle->mode = mode;
	handle->private = client;
	err = snd_pcm_init(handle);
	if (err < 0) {
		snd_pcm_close(handle);
		return err;
	}
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

int _snd_pcm_client_open(snd_pcm_t **pcmp, char *name, snd_config_t *conf,
			 int stream, int mode)
{
	snd_config_iterator_t i;
	char *socket = NULL;
	char *sname = NULL;
	char *host = NULL;
	long port = -1;
	int err;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "stream") == 0)
			continue;
		if (strcmp(n->id, "socket") == 0) {
			err = snd_config_string_get(n, &socket);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "host") == 0) {
			err = snd_config_string_get(n, &host);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "port") == 0) {
			err = snd_config_integer_get(n, &port);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		if (strcmp(n->id, "sname") == 0) {
			err = snd_config_string_get(n, &sname);
			if (err < 0)
				return -EINVAL;
			continue;
		}
		return -EINVAL;
	}
	if (!sname)
		return -EINVAL;
	if (socket) {
		if (port >= 0 || host)
			return -EINVAL;
		return snd_pcm_client_create(pcmp, socket, -1, SND_TRANSPORT_TYPE_SHM, sname, stream, mode);
	} else  {
		if (port < 0 || !name)
			return -EINVAL;
		return snd_pcm_client_create(pcmp, host, port, SND_TRANSPORT_TYPE_TCP, sname, stream, mode);
	}
}
				
