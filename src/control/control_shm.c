/*
 *  Control - SHM Client
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
	int socket;
	volatile snd_ctl_shm_ctrl_t *ctrl;
} snd_ctl_shm_t;

extern int receive_fd(int socket, void *data, size_t len, int *fd);

static int snd_ctl_shm_action(snd_ctl_t *ctl)
{
	snd_ctl_shm_t *shm = ctl->private;
	int err;
	char buf[1];
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	err = write(shm->socket, buf, 1);
	if (err != 1)
		return -EBADFD;
	err = read(shm->socket, buf, 1);
	if (err != 1)
		return -EBADFD;
	if (ctrl->cmd) {
		ERR("Server has not done the cmd");
		return -EBADFD;
	}
	return ctrl->result;
}

static int snd_ctl_shm_action_fd(snd_ctl_t *ctl, int *fd)
{
	snd_ctl_shm_t *shm = ctl->private;
	int err;
	char buf[1];
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	err = write(shm->socket, buf, 1);
	if (err != 1)
		return -EBADFD;
	err = receive_fd(shm->socket, buf, 1, fd);
	if (err != 1)
		return -EBADFD;
	if (ctrl->cmd) {
		ERR("Server has not done the cmd");
		return -EBADFD;
	}
	return ctrl->result;
}

static int snd_ctl_shm_close(snd_ctl_t *ctl)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int result;
	ctrl->cmd = SND_CTL_IOCTL_CLOSE;
	result = snd_ctl_shm_action(ctl);
	shmdt((void *)ctrl);
	close(shm->socket);
	free(shm);
	return result;
}

static int snd_ctl_shm_card(snd_ctl_t *ctl)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	ctrl->cmd = SND_CTL_IOCTL_CARD;
	return snd_ctl_shm_action(ctl);
}

static int snd_ctl_shm_poll_descriptor(snd_ctl_t *ctl)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int fd, err;
	ctrl->cmd = SND_CTL_IOCTL_POLL_DESCRIPTOR;
	err = snd_ctl_shm_action_fd(ctl, &fd);
	if (err < 0)
		return err;
	return fd;
}

static int snd_ctl_shm_hw_info(snd_ctl_t *ctl, snd_ctl_hw_info_t *info)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
//	ctrl->u.hw_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_HW_INFO;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.hw_info;
	return err;
}

static int snd_ctl_shm_clist(snd_ctl_t *ctl, snd_control_list_t *list)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	size_t maxsize = CTL_SHM_DATA_MAXLEN;
	size_t bytes = list->controls_request * sizeof(*list->pids);
	int err;
	snd_control_id_t *pids = list->pids;
	if (bytes > maxsize)
		return -EINVAL;
	ctrl->u.clist = *list;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_LIST;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*list = ctrl->u.clist;
	list->pids = pids;
	memcpy(pids, (void *)ctrl->data, bytes);
	return err;
}

static int snd_ctl_shm_cinfo(snd_ctl_t *ctl, snd_control_info_t *info)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.cinfo = *info;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_INFO;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.cinfo;
	return err;
}

static int snd_ctl_shm_cread(snd_ctl_t *ctl, snd_control_t *control)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.cread = *control;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_READ;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*control = ctrl->u.cread;
	return err;
}

static int snd_ctl_shm_cwrite(snd_ctl_t *ctl, snd_control_t *control)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.cwrite = *control;
	ctrl->cmd = SND_CTL_IOCTL_CONTROL_WRITE;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*control = ctrl->u.cwrite;
	return err;
}

static int snd_ctl_shm_hwdep_next_device(snd_ctl_t *ctl, int * device)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.device = *device;
	ctrl->cmd = SND_CTL_IOCTL_HWDEP_NEXT_DEVICE;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*device = ctrl->u.device;
	return err;
}

static int snd_ctl_shm_hwdep_info(snd_ctl_t *ctl, snd_hwdep_info_t * info)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.hwdep_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_HWDEP_INFO;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.hwdep_info;
	return err;
}

static int snd_ctl_shm_pcm_next_device(snd_ctl_t *ctl, int * device)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.device = *device;
	ctrl->cmd = SND_CTL_IOCTL_PCM_NEXT_DEVICE;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*device = ctrl->u.device;
	return err;
}

static int snd_ctl_shm_pcm_info(snd_ctl_t *ctl, snd_pcm_info_t * info)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.pcm_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_PCM_INFO;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.pcm_info;
	return err;
}

static int snd_ctl_shm_pcm_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.pcm_prefer_subdevice = subdev;
	ctrl->cmd = SND_CTL_IOCTL_PCM_PREFER_SUBDEVICE;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	return err;
}

static int snd_ctl_shm_rawmidi_next_device(snd_ctl_t *ctl, int * device)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.device = *device;
	ctrl->cmd = SND_CTL_IOCTL_RAWMIDI_NEXT_DEVICE;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*device = ctrl->u.device;
	return err;
}

static int snd_ctl_shm_rawmidi_info(snd_ctl_t *ctl, snd_rawmidi_info_t * info)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.rawmidi_info = *info;
	ctrl->cmd = SND_CTL_IOCTL_RAWMIDI_INFO;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*info = ctrl->u.rawmidi_info;
	return err;
}

static int snd_ctl_shm_rawmidi_prefer_subdevice(snd_ctl_t *ctl, int subdev)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.rawmidi_prefer_subdevice = subdev;
	ctrl->cmd = SND_CTL_IOCTL_RAWMIDI_PREFER_SUBDEVICE;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	return err;
}

static int snd_ctl_shm_read(snd_ctl_t *ctl, snd_ctl_event_t *event)
{
	snd_ctl_shm_t *shm = ctl->private;
	volatile snd_ctl_shm_ctrl_t *ctrl = shm->ctrl;
	int err;
	ctrl->u.read = *event;
	ctrl->cmd = SND_CTL_IOCTL_READ;
	err = snd_ctl_shm_action(ctl);
	if (err < 0)
		return err;
	*event = ctrl->u.read;
	return err;
}

snd_ctl_ops_t snd_ctl_shm_ops = {
	close: snd_ctl_shm_close,
	card: snd_ctl_shm_card,
	poll_descriptor: snd_ctl_shm_poll_descriptor,
	hw_info: snd_ctl_shm_hw_info,
	clist: snd_ctl_shm_clist,
	cinfo: snd_ctl_shm_cinfo,
	cread: snd_ctl_shm_cread,
	cwrite: snd_ctl_shm_cwrite,
	hwdep_next_device: snd_ctl_shm_hwdep_next_device,
	hwdep_info: snd_ctl_shm_hwdep_info,
	pcm_next_device: snd_ctl_shm_pcm_next_device,
	pcm_info: snd_ctl_shm_pcm_info,
	pcm_prefer_subdevice: snd_ctl_shm_pcm_prefer_subdevice,
	rawmidi_next_device: snd_ctl_shm_rawmidi_next_device,
	rawmidi_info: snd_ctl_shm_rawmidi_info,
	rawmidi_prefer_subdevice: snd_ctl_shm_rawmidi_prefer_subdevice,
	read: snd_ctl_shm_read,
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

#if 0
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
#endif

int snd_ctl_shm_open(snd_ctl_t **handlep, char *name, char *socket, char *sname)
{
	snd_ctl_t *ctl;
	snd_ctl_shm_t *shm = NULL;
	snd_client_open_request_t *req;
	snd_client_open_answer_t ans;
	size_t snamelen, reqlen;
	int err;
	int result;
	int sock = -1;
	snd_ctl_shm_ctrl_t *ctrl = NULL;
	snamelen = strlen(sname);
	if (snamelen > 255)
		return -EINVAL;

	result = make_local_socket(socket);
	if (result < 0) {
		ERR("server for socket %s is not running", socket);
		goto _err;
	}
	sock = result;

	reqlen = sizeof(*req) + snamelen;
	req = alloca(reqlen);
	memcpy(req->name, sname, snamelen);
	req->dev_type = SND_DEV_TYPE_CONTROL;
	req->transport_type = SND_TRANSPORT_TYPE_SHM;
	req->stream = 0;
	req->mode = 0;
	req->namelen = snamelen;
	err = write(sock, req, reqlen);
	if (err < 0) {
		ERR("write error");
		result = -errno;
		goto _err;
	}
	if ((size_t) err != reqlen) {
		ERR("write size error");
		result = -EINVAL;
		goto _err;
	}
	err = read(sock, &ans, sizeof(ans));
	if (err < 0) {
		ERR("read error");
		result = -errno;
		goto _err;
	}
	if (err != sizeof(ans)) {
		ERR("read size error");
		result = -EINVAL;
		goto _err;
	}
	result = ans.result;
	if (result < 0)
		goto _err;

	ctrl = shmat(ans.cookie, 0, 0);
	if (!ctrl) {
		result = -errno;
		goto _err;
	}
		
	ctl = calloc(1, sizeof(snd_ctl_t));
	if (!ctl) {
		result = -ENOMEM;
		goto _err;
	}
	shm = calloc(1, sizeof(snd_ctl_shm_t));
	if (!ctl) {
		free(ctl);
		result = -ENOMEM;
		goto _err;
	}

	shm->socket = sock;
	shm->ctrl = ctrl;

	if (name)
		ctl->name = strdup(name);
	ctl->type = SND_CTL_TYPE_SHM;
	ctl->ops = &snd_ctl_shm_ops;
	ctl->private = shm;
	INIT_LIST_HEAD(&ctl->hlist);
	*handlep = ctl;
	return 0;

 _err:
	close(sock);
	if (ctrl)
		shmdt(ctrl);
	if (shm)
		free(shm);
	return result;
}

extern int is_local(struct hostent *hent);

int _snd_ctl_shm_open(snd_ctl_t **handlep, char *name, snd_config_t *conf)
{
	snd_config_iterator_t i;
	char *server = NULL;
	char *sname = NULL;
	snd_config_t *sconfig;
	char *host = NULL;
	char *socket = NULL;
	long port = -1;
	int err;
	int local;
	struct hostent *h;
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "type") == 0)
			continue;
		if (strcmp(n->id, "server") == 0) {
			err = snd_config_string_get(n, &server);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "sname") == 0) {
			err = snd_config_string_get(n, &sname);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		ERR("Unknown field %s", n->id);
		return -EINVAL;
	}
	if (!sname) {
		ERR("sname is not defined");
		return -EINVAL;
	}
	if (!server) {
		ERR("server is not defined");
		return -EINVAL;
	}
	err = snd_config_searchv(snd_config, &sconfig, "server", server, 0);
	if (err < 0) {
		ERR("Unknown server %s", server);
		return -EINVAL;
	}
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "host") == 0) {
			err = snd_config_string_get(n, &host);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "socket") == 0) {
			err = snd_config_string_get(n, &socket);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		if (strcmp(n->id, "port") == 0) {
			err = snd_config_integer_get(n, &port);
			if (err < 0) {
				ERR("Invalid type for %s", n->id);
				return -EINVAL;
			}
			continue;
		}
		ERR("Unknown field %s", n->id);
		return -EINVAL;
	}

	if (!host) {
		ERR("host is not defined");
		return -EINVAL;
	}
	if (!socket) {
		ERR("socket is not defined");
		return -EINVAL;
	}
	h = gethostbyname(host);
	if (!h) {
		ERR("Cannot resolve %s", host);
		return -EINVAL;
	}
	local = is_local(h);
	if (!local) {
		ERR("%s is not the local host", host);
		return -EINVAL;
	}
	return snd_ctl_shm_open(handlep, name, socket, sname);
}
				
