/*
 *  Control - Client
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
#include "control_local.h"
#include "aserver.h"

typedef struct {
	int data_fd;
	int ctrl_fd;
	union {
		struct {
			void *ctrl;
		} shm;
	} u;
} snd_ctl_client_t;

static void clean_poll(snd_ctl_t *ctl)
{
	snd_ctl_client_t *client = ctl->private;
	struct pollfd pfd;
	int err;
	char buf[1];
	pfd.fd = client->data_fd;
	pfd.events = POLLIN;
	while (1) {
		err = poll(&pfd, 1, 0);
		if (err == 0)
			break;
		assert(err > 0);
		err = read(client->data_fd, buf, 1);
		assert(err == 1);
	}
}

static int snd_ctl_client_shm_action(snd_ctl_t *ctl)
{
	snd_ctl_client_t *client = ctl->private;
	int err;
	char buf[1];
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	clean_poll(ctl);
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

static int snd_ctl_client_shm_close(snd_ctl_t *ctl)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int result;
	ctrl->cmd = SND_CTL_IOCTL_CLOSE;
	result = snd_ctl_client_shm_action(ctl);
	if (result >= 0)
		result = ctrl->result;
	shmdt((void *)ctrl);
	close(client->data_fd);
	close(client->ctrl_fd);
	free(client);
	return result;
}

static int snd_ctl_client_poll_descriptor(snd_ctl_t *ctl)
{
	snd_ctl_client_t *client = ctl->private;
	return client->data_fd;
}

static int snd_ctl_client_shm_hw_info(snd_ctl_t *ctl, snd_ctl_hw_info_t *info)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
//	ctrl->u.hw_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_HW_INFO;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.hw_info;
	return ctrl->result;
}

static int snd_ctl_client_shm_clist(snd_ctl_t *ctl, snd_control_list_t *list)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	size_t maxsize = CTL_SHM_DATA_MAXLEN;
	size_t bytes = list->controls_request * sizeof(*list->pids);
	int err;
	snd_control_id_t *pids = list->pids;
	if (bytes > maxsize)
		return -EINVAL;
	ctrl->u.clist = *list;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_LIST;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*list = ctrl->u.clist;
	list->pids = pids;
	memcpy(pids, ctrl->data, bytes);
	return ctrl->result;
}

static int snd_ctl_client_shm_cinfo(snd_ctl_t *ctl, snd_control_info_t *info)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.cinfo = *info;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_INFO;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.cinfo;
	return ctrl->result;
}

static int snd_ctl_client_shm_cread(snd_ctl_t *ctl, snd_control_t *control)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.cread = *control;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_READ;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*control = ctrl->u.cread;
	return ctrl->result;
}

static int snd_ctl_client_shm_cwrite(snd_ctl_t *ctl, snd_control_t *control)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.cwrite = *control;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_WRITE;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*control = ctrl->u.cwrite;
	return ctrl->result;
}

static int snd_ctl_client_shm_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.hwdep_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_HWDEP_INFO;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.hwdep_info;
	return ctrl->result;
}

static int snd_ctl_client_shm_pcm_info(snd_ctl_t *ctl, snd_pcm_info_t * info)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.pcm_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_PCM_INFO;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.pcm_info;
	return ctrl->result;
}

static int snd_ctl_client_shm_pcm_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.pcm_prefer_subdevice = subdev;
	ctrl->cmd = SND_CTL_IOCTL_PCM_PREFER_SUBDEVICE;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	return ctrl->result;
}

static int snd_ctl_client_shm_rawmidi_info(snd_ctl_t *ctl, snd_rawmidi_info_t * info)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.rawmidi_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_RAWMIDI_INFO;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.rawmidi_info;
	return ctrl->result;
}

static int snd_ctl_client_shm_read(snd_ctl_t *ctl, snd_ctl_event_t *event)
{
	snd_ctl_client_t *client = ctl->private;
	snd_ctl_client_shm_t *ctrl = client->u.shm.ctrl;
	int err;
	ctrl->u.read = *event;
	ctrl->cmd = SND_CTL_IOCTL_READ;
	err = snd_ctl_client_shm_action(ctl);
	if (err < 0)
		return err;
	*event = ctrl->u.read;
	return ctrl->result;
}

struct snd_ctl_ops snd_ctl_client_ops = {
	close: snd_ctl_client_shm_close,
	poll_descriptor: snd_ctl_client_poll_descriptor,
	hw_info: snd_ctl_client_shm_hw_info,
	clist: snd_ctl_client_shm_clist,
	cinfo: snd_ctl_client_shm_cinfo,
	cread: snd_ctl_client_shm_cread,
	cwrite: snd_ctl_client_shm_cwrite,
	hwdep_info: snd_ctl_client_shm_hwdep_info,
	pcm_info: snd_ctl_client_shm_pcm_info,
	pcm_prefer_subdevice: snd_ctl_client_shm_pcm_prefer_subdevice,
	rawmidi_info: snd_ctl_client_shm_rawmidi_info,
	read: snd_ctl_client_shm_read,
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
int snd_ctl_client_open(snd_ctl_t **handlep, char *host, int port, int transport, char *name)
{
	snd_ctl_t *ctl;
	snd_ctl_client_t *client;
	snd_client_open_request_t *req;
	snd_client_open_answer_t ans;
	size_t namelen, reqlen;
	int err;
	int result;
	int fds[2] = {-1, -1};
	int k;
	snd_ctl_client_shm_t *ctrl = NULL;
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
	req->dev_type = SND_DEV_TYPE_CONTROL;
	req->transport_type = transport;
	req->stream = 0;
	req->mode = 0;
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
		
	ctl = calloc(1, sizeof(snd_ctl_t));
	if (!ctl) {
		result = -ENOMEM;
		goto _err;
	}
	client = calloc(1, sizeof(snd_ctl_client_t));
	if (!ctl) {
		free(ctl);
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
	ctl->type = SND_CTL_TYPE_CLIENT;
	ctl->ops = &snd_ctl_client_ops;
	ctl->private = client;
	INIT_LIST_HEAD(&ctl->hlist);
	*handlep = ctl;
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

