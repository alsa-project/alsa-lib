/*
 *  ALSA server
 *  Copyright (c) by Abramo Bagnara <abramo@alsa-project.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <stddef.h>
#include <getopt.h>
#include <netinet/in.h>

#include "asoundlib.h"
#include "pcm_local.h"
#include "aserver.h"
#include "list.h"

char *command;

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define error(...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, __VA_ARGS__); \
	putc('\n', stderr); \
} while (0)
#else
#define error(args...) do {\
	fprintf(stderr, "%s: %s:%d: ", command, __FUNCTION__, __LINE__); \
	fprintf(stderr, ##args); \
	putc('\n', stderr); \
} while (0)
#endif	

#define perrno(string) error("%s", strerror(errno))

int make_local_socket(const char *filename)
{
	size_t l = strlen(filename);
	size_t size = offsetof(struct sockaddr_un, sun_path) + l;
	struct sockaddr_un *addr = alloca(size);
	int sock;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0) {
		int result = -errno;
		perrno("socket");
		return result;
	}
	
	unlink(filename);

	addr->sun_family = AF_LOCAL;
	memcpy(addr->sun_path, filename, l);

	if (bind(sock, (struct sockaddr *) addr, size) < 0) {
		int result = -errno;
		perrno("bind");
		return result;
	}

	return sock;
}

int make_inet_socket(int port)
{
	struct sockaddr_in addr;
	int sock;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		int result = -errno;
		perrno("socket");
		return result;
	}
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		int result = -errno;
		perrno("bind");
		return result;
	}

	return sock;
}

int send_fd(int socket, void *data, size_t len, int fd)
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
    *fds = fd;

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &vec;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = cmsg;
    msghdr.msg_controllen = cmsg_len;
    msghdr.msg_flags = 0;

    ret = sendmsg(socket, &msghdr, 0 );
    if (ret < 0) {
	    perrno("sendmsg");
	    return -errno;
    }
    return ret;
}

struct pollfd pollfds[OPEN_MAX];
unsigned int pollfds_count = 0;
typedef struct waiter waiter_t;
typedef int (*waiter_handler_t)(waiter_t *waiter, unsigned short events);
struct waiter {
	int fd;
	void *private_data;
	waiter_handler_t handler;
};
waiter_t waiters[OPEN_MAX];

void add_waiter(int fd, unsigned short events, waiter_handler_t handler,
		void *data)
{
	waiter_t *w = &waiters[fd];
	struct pollfd *pfd = &pollfds[pollfds_count];
	assert(!w->handler);
	pfd->fd = fd;
	pfd->events = events;
	pfd->revents = 0;
	w->fd = fd;
	w->private_data = data;
	w->handler = handler;
	pollfds_count++;
}

void del_waiter(int fd)
{
	waiter_t *w = &waiters[fd];
	unsigned int k;
	assert(w->handler);
	w->handler = 0;
	for (k = 0; k < pollfds_count; ++k) {
		if (pollfds[k].fd == fd)
			break;
	}
	assert(k < pollfds_count);
	pollfds_count--;
	memmove(&pollfds[k], &pollfds[k + 1], pollfds_count - k);
}

typedef struct {
	struct list_head list;
	int fd;
	int local;
} master_t;
LIST_HEAD(masters);

typedef struct client client_t;

typedef struct {
	int (*open)(client_t *client, int *cookie);
	int (*cmd)(client_t *client);
	int (*close)(client_t *client);
} transport_ops_t;

struct client {
	struct list_head list;
	struct socket {
		int fd;
		int local;
	} data, ctrl;
	int transport_type;
	int dev_type;
	char name[256];
	int stream;
	int mode;
	transport_ops_t *ops;
	union {
		struct {
			snd_pcm_t *handle;
			int fd;
		} pcm;
		struct {
			snd_ctl_t *handle;
			int fd;
		} control;
#if 0
		struct {
			snd_rawmidi_t *handle;
		} rawmidi;
		struct {
			snd_timer_open_t *handle;
		} timer;
		struct {
			snd_hwdep_t *handle;
		} hwdep;
		struct {
			snd_seq_t *handle;
		} seq;
#endif
	} device;
	int polling;
	int open;
	int cookie;
	union {
		struct {
			int ctrl_id;
			void *ctrl;
		} shm;
	} transport;
};

LIST_HEAD(clients);

typedef struct {
	struct list_head list;
	int fd;
	uint32_t cookie;
	int local;
} pending_t;
LIST_HEAD(pendings);

int pcm_handler(waiter_t *waiter, unsigned short events)
{
	client_t *client = waiter->private_data;
	char buf[1];
	ssize_t n;
	if (events & POLLIN) {
		n = write(client->data.fd, buf, 1);
		if (n != 1) {
			perrno("write");
			return -errno;
		}
	} else if (events & POLLOUT) {
		n = read(client->data.fd, buf, 1);
		if (n != 1) {
			perrno("read");
			return -errno;
		}
	}
	del_waiter(waiter->fd);
	client->polling = 0;
	return 0;
}

int pcm_shm_open(client_t *client, int *cookie)
{
	int shmid;
	snd_pcm_t *pcm;
	int err;
	int result;
	err = snd_pcm_open(&pcm, client->name, client->stream, client->mode);
	if (err < 0)
		return err;
	client->device.pcm.handle = pcm;
	client->device.pcm.fd = snd_pcm_poll_descriptor(pcm);

	shmid = shmget(IPC_PRIVATE, PCM_SHM_SIZE, 0666);
	if (shmid < 0) {
		result = -errno;
		perrno("shmget");
		goto _err;
	}
	client->transport.shm.ctrl_id = shmid;
	client->transport.shm.ctrl = shmat(shmid, 0, 0);
	if (!client->transport.shm.ctrl) {
		result = -errno;
		shmctl(shmid, IPC_RMID, 0);
		perrno("shmat");
		goto _err;
	}
	*cookie = shmid;
	return 0;

 _err:
	snd_pcm_close(pcm);
	return result;

}

int pcm_shm_close(client_t *client)
{
	int err;
	snd_pcm_client_shm_t *ctrl = client->transport.shm.ctrl;
	if (client->polling) {
		del_waiter(client->device.pcm.fd);
		client->polling = 0;
	}
	/* FIXME: blocking */
	err = snd_pcm_close(client->device.pcm.handle);
	ctrl->result = err;
	if (err < 0) 
		perrno("snd_pcm_close");
	if (client->transport.shm.ctrl) {
		err = shmdt((void *)client->transport.shm.ctrl);
		if (err < 0)
			perrno("shmdt");
		err = shmctl(client->transport.shm.ctrl_id, IPC_RMID, 0);
		if (err < 0)
			perrno("shmctl");
		client->transport.shm.ctrl = 0;
	}
	client->open = 0;
	return 0;
}

int pcm_shm_cmd(client_t *client)
{
	snd_pcm_client_shm_t *ctrl = client->transport.shm.ctrl;
	struct pollfd pfd;
	char buf[1];
	int err;
	int cmd;
	snd_pcm_t *pcm;
	err = read(client->ctrl.fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	cmd = ctrl->cmd;
	ctrl->cmd = 0;
	pcm = client->device.pcm.handle;
	switch (cmd) {
	case SND_PCM_IOCTL_ASYNC:
		ctrl->result = snd_pcm_async(pcm, ctrl->u.async.sig, ctrl->u.async.pid);
		break;
	case SND_PCM_IOCTL_INFO:
		ctrl->result = snd_pcm_info(pcm, &ctrl->u.info);
		break;
	case SND_PCM_IOCTL_PARAMS:
		ctrl->result = snd_pcm_params(pcm, &ctrl->u.params);
		break;
	case SND_PCM_IOCTL_PARAMS_INFO:
		ctrl->result = snd_pcm_params_info(pcm, &ctrl->u.params_info);
		break;
	case SND_PCM_IOCTL_SETUP:
		ctrl->result = snd_pcm_setup(pcm, &ctrl->u.setup);
		break;
	case SND_PCM_IOCTL_STATUS:
		ctrl->result = snd_pcm_status(pcm, &ctrl->u.status);
		break;
	case SND_PCM_IOCTL_STATE:
		ctrl->result = snd_pcm_state(pcm);
		break;
	case SND_PCM_IOCTL_DELAY:
		ctrl->result = snd_pcm_delay(pcm, &ctrl->u.delay);
		break;
	case SND_PCM_IOCTL_AVAIL_UPDATE:
		ctrl->result = snd_pcm_avail_update(pcm);
		break;
	case SND_PCM_IOCTL_PREPARE:
		ctrl->result = snd_pcm_prepare(pcm);
		break;
	case SND_PCM_IOCTL_START:
		ctrl->result = snd_pcm_start(pcm);
		break;
	case SND_PCM_IOCTL_DRAIN:
		ctrl->result = snd_pcm_drain(pcm);
		break;
	case SND_PCM_IOCTL_DROP:
		ctrl->result = snd_pcm_drop(pcm);
		break;
	case SND_PCM_IOCTL_PAUSE:
		ctrl->result = snd_pcm_pause(pcm, ctrl->u.pause);
		break;
	case SND_PCM_IOCTL_CHANNEL_INFO:
		ctrl->result = snd_pcm_channel_info(pcm, &ctrl->u.channel_info);
		break;
	case SND_PCM_IOCTL_CHANNEL_PARAMS:
		ctrl->result = snd_pcm_channel_params(pcm, &ctrl->u.channel_params);
		break;
	case SND_PCM_IOCTL_CHANNEL_SETUP:
		ctrl->result = snd_pcm_channel_setup(pcm, &ctrl->u.channel_setup);
		break;
	case SND_PCM_IOCTL_REWIND:
		ctrl->result = snd_pcm_rewind(pcm, ctrl->u.rewind);
		break;
	case SND_PCM_IOCTL_LINK:
	{
		struct list_head *item;
		list_for_each(item, &clients) {
			client_t *client = list_entry(item, client_t, list);
			if (!client->open)
				continue;
			if (client->data.fd == ctrl->u.link) {
				ctrl->result = snd_pcm_link(pcm, client->device.pcm.handle);
				break;
			}
		}
		ctrl->result = -EBADFD;
		break;
	}
	case SND_PCM_IOCTL_UNLINK:
		ctrl->result = snd_pcm_unlink(pcm);
		break;
	case SND_PCM_IOCTL_MMAP_DATA:
	case SND_PCM_IOCTL_MMAP_CONTROL:
	case SND_PCM_IOCTL_MMAP_STATUS:
	{
		pfd.fd = client->ctrl.fd;
		pfd.events = POLLHUP;
		if (poll(&pfd, 1, 0) == 1)
			return -EBADFD;
		err = send_fd(client->ctrl.fd, buf, 1, client->device.pcm.fd);
		if (err != 1)
			return -EBADFD;
		ctrl->result = 0;
		return 0;
	}
	case SND_PCM_IOCTL_MUNMAP_DATA:
	case SND_PCM_IOCTL_MUNMAP_CONTROL:
	case SND_PCM_IOCTL_MUNMAP_STATUS:
		ctrl->result = 0;
		break;
	case SND_PCM_IOCTL_MMAP_FORWARD:
		ctrl->result = snd_pcm_mmap_forward(pcm, ctrl->u.mmap_forward);
		break;
	case SND_PCM_IOCTL_CLOSE:
		client->ops->close(client);
		break;
	default:
		fprintf(stderr, "Bogus cmd: %x\n", ctrl->cmd);
		ctrl->result = -ENOSYS;
	}
	pfd.fd = client->ctrl.fd;
	pfd.events = POLLHUP;
	if (poll(&pfd, 1, 0) == 1)
		return -EBADFD;
	err = write(client->ctrl.fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	if (!client->polling) {
		add_waiter(client->device.pcm.fd, POLLIN | POLLOUT, pcm_handler, client);
		client->polling = 1;
	}
	return 0;
}

transport_ops_t pcm_shm_ops = {
	open: pcm_shm_open,
	cmd: pcm_shm_cmd,
	close: pcm_shm_close,
};

int ctl_handler(waiter_t *waiter, unsigned short events)
{
	client_t *client = waiter->private_data;
	char buf[1];
	ssize_t n;
	if (events & POLLIN) {
		n = write(client->data.fd, buf, 1);
		if (n != 1) {
			perrno("write");
			return -errno;
		}
	}
	del_waiter(waiter->fd);
	client->polling = 0;
	return 0;
}

int ctl_shm_open(client_t *client, int *cookie)
{
	int shmid;
	snd_ctl_t *ctl;
	int err;
	int result;
	err = snd_ctl_open(&ctl, client->name);
	if (err < 0)
		return err;
	client->device.control.handle = ctl;
	client->device.control.fd = snd_ctl_poll_descriptor(ctl);

	shmid = shmget(IPC_PRIVATE, CTL_SHM_SIZE, 0666);
	if (shmid < 0) {
		result = -errno;
		perrno("shmget");
		goto _err;
	}
	client->transport.shm.ctrl_id = shmid;
	client->transport.shm.ctrl = shmat(shmid, 0, 0);
	if (!client->transport.shm.ctrl) {
		result = -errno;
		shmctl(shmid, IPC_RMID, 0);
		perrno("shmat");
		goto _err;
	}
	*cookie = shmid;
	add_waiter(client->device.control.fd, POLLIN, ctl_handler, client);
	client->polling = 1;
	return 0;

 _err:
	snd_ctl_close(ctl);
	return result;

}

int ctl_shm_close(client_t *client)
{
	int err;
	snd_ctl_client_shm_t *ctrl = client->transport.shm.ctrl;
	if (client->polling) {
		del_waiter(client->device.control.fd);
		client->polling = 0;
	}
	/* FIXME: blocking */
	err = snd_ctl_close(client->device.control.handle);
	ctrl->result = err;
	if (err < 0) 
		perrno("snd_ctl_close");
	if (client->transport.shm.ctrl) {
		err = shmdt((void *)client->transport.shm.ctrl);
		if (err < 0)
			perrno("shmdt");
		err = shmctl(client->transport.shm.ctrl_id, IPC_RMID, 0);
		if (err < 0)
			perrno("shmctl");
		client->transport.shm.ctrl = 0;
	}
	client->open = 0;
	return 0;
}

extern int snd_ctl_read1(snd_ctl_t *ctl, snd_ctl_event_t *event);

int ctl_shm_cmd(client_t *client)
{
	snd_ctl_client_shm_t *ctrl = client->transport.shm.ctrl;
	struct pollfd pfd;
	char buf[1];
	int err;
	int cmd;
	snd_ctl_t *ctl;
	err = read(client->ctrl.fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	cmd = ctrl->cmd;
	ctrl->cmd = 0;
	ctl = client->device.control.handle;
	switch (cmd) {
	case SND_CTL_IOCTL_HW_INFO:
		ctrl->result = snd_ctl_hw_info(ctl, &ctrl->u.hw_info);
		break;
	case SND_CTL_IOCTL_CONTROL_LIST:
	{
		size_t maxsize = CTL_SHM_DATA_MAXLEN;
		if (ctrl->u.clist.controls_request * sizeof(*ctrl->u.clist.pids) > maxsize) {
			ctrl->result = -EFAULT;
			break;
		}
		ctrl->u.clist.pids = (snd_control_id_t*) ctrl->data;
		ctrl->result = snd_ctl_clist(ctl, &ctrl->u.clist);
		break;
	}
	case SND_CTL_IOCTL_CONTROL_INFO:
		ctrl->result = snd_ctl_cinfo(ctl, &ctrl->u.cinfo);
		break;
	case SND_CTL_IOCTL_CONTROL_READ:
		ctrl->result = snd_ctl_cread(ctl, &ctrl->u.cread);
		break;
	case SND_CTL_IOCTL_CONTROL_WRITE:
		ctrl->result = snd_ctl_cwrite(ctl, &ctrl->u.cwrite);
		break;
	case SND_CTL_IOCTL_HWDEP_INFO:
		ctrl->result = snd_ctl_hwdep_info(ctl, &ctrl->u.hwdep_info);
		break;
	case SND_CTL_IOCTL_PCM_INFO:
		ctrl->result = snd_ctl_pcm_info(ctl, &ctrl->u.pcm_info);
		break;
	case SND_CTL_IOCTL_PCM_PREFER_SUBDEVICE:
		ctrl->result = snd_ctl_pcm_prefer_subdevice(ctl, ctrl->u.pcm_prefer_subdevice);
		break;
	case SND_CTL_IOCTL_RAWMIDI_INFO:
		ctrl->result = snd_ctl_rawmidi_info(ctl, &ctrl->u.rawmidi_info);
		break;
	case SND_CTL_IOCTL_READ:
		ctrl->result = snd_ctl_read1(ctl, &ctrl->u.read);
		break;
	case SND_CTL_IOCTL_CLOSE:
		client->ops->close(client);
		break;
	default:
		fprintf(stderr, "Bogus cmd: %x\n", ctrl->cmd);
		ctrl->result = -ENOSYS;
	}
	pfd.fd = client->ctrl.fd;
	pfd.events = POLLHUP;
	if (poll(&pfd, 1, 0) == 1)
		return -EBADFD;
	err = write(client->ctrl.fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	if (!client->polling) {
		add_waiter(client->device.control.fd, POLLIN, ctl_handler, client);
		client->polling = 1;
	}
	return 0;
}

transport_ops_t ctl_shm_ops = {
	open: ctl_shm_open,
	cmd: ctl_shm_cmd,
	close: ctl_shm_close,
};

int snd_client_open(client_t *client)
{
	int err;
	snd_client_open_request_t req;
	snd_client_open_answer_t ans;
	char *name;
	memset(&ans, 0, sizeof(ans));
	err = read(client->ctrl.fd, &req, sizeof(req));
	if (err < 0) {
		perrno("read");
		exit(1);
	}
	if (err != sizeof(req)) {
		ans.result = -EINVAL;
		goto _answer;
	}
	name = alloca(req.namelen);
	err = read(client->ctrl.fd, name, req.namelen);
	if (err < 0) {
		perrno("read");
		exit(1);
	}
	if (err != req.namelen) {
		ans.result = -EINVAL;
		goto _answer;
	}

	switch (req.dev_type) {
	case SND_DEV_TYPE_PCM:
		switch (req.transport_type) {
		case SND_TRANSPORT_TYPE_SHM:
			client->ops = &pcm_shm_ops;
			break;
		default:
			ans.result = -EINVAL;
			goto _answer;
		}
		break;
	case SND_DEV_TYPE_CONTROL:
		switch (req.transport_type) {
		case SND_TRANSPORT_TYPE_SHM:
			client->ops = &ctl_shm_ops;
			break;
		default:
			ans.result = -EINVAL;
			goto _answer;
		}
		break;
	default:
		ans.result = -EINVAL;
		goto _answer;
	}

	name[req.namelen] = '\0';

	client->transport_type = req.transport_type;
	strcpy(client->name, name);
	client->stream = req.stream;
	client->mode = req.mode;

	err = client->ops->open(client, &ans.cookie);
	if (err < 0) {
		ans.result = err;
	} else {
		client->open = 1;
		ans.result = client->data.fd;
	}

 _answer:
	err = write(client->ctrl.fd, &ans, sizeof(ans));
	if (err != sizeof(ans)) {
		perrno("write");
		exit(1);
	}
	return 0;
}

int client_data_handler(waiter_t *waiter, unsigned short events ATTRIBUTE_UNUSED)
{
	client_t *client = waiter->private_data;
	if (client->open)
		client->ops->close(client);
	close(client->data.fd);
	close(client->ctrl.fd);
	del_waiter(client->data.fd);
	del_waiter(client->ctrl.fd);
	list_del(&client->list);
	free(client);
	return 0;
}

int client_ctrl_handler(waiter_t *waiter, unsigned short events)
{
	client_t *client = waiter->private_data;
	if (events & POLLHUP)
		return client_data_handler(waiter, events);
	if (client->open)
		return client->ops->cmd(client);
	else
		return snd_client_open(client);
}

int pending_handler(waiter_t *waiter, unsigned short events)
{
	pending_t *pending = waiter->private_data;
	pending_t *pdata;
	client_t *client;
	uint32_t cookie;
	struct list_head *item;
	int remove = 0;
	if (events & POLLHUP)
		remove = 1;
	else {
		int err = read(waiter->fd, &cookie, sizeof(cookie));
		if (err != sizeof(cookie))
			remove = 1;
		else {
			err = write(waiter->fd, &cookie, sizeof(cookie));
			if (err != sizeof(cookie))
				remove = 1;
		}
	}
	del_waiter(waiter->fd);
	if (remove) {
		close(waiter->fd);
		list_del(&pending->list);
		free(pending);
		return 0;
	}

	list_for_each(item, &pendings) {
		pdata = list_entry(item, pending_t, list);
		if (pdata->cookie == cookie)
			goto found;
	}
	pending->cookie = cookie;
	return 0;

 found:
	client = calloc(sizeof(*client), 1);
	client->data.fd = pdata->fd;
	client->data.local = pdata->local;
	client->ctrl.fd = waiter->fd;
	client->ctrl.local = pending->local;
	add_waiter(client->ctrl.fd, POLLIN | POLLHUP, client_ctrl_handler, client);
	add_waiter(client->data.fd, POLLHUP, client_data_handler, client);
	client->open = 0;
	list_add_tail(&client->list, &clients);
	list_del(&pending->list);
	list_del(&pdata->list);
	free(pending);
	free(pdata);
	return 0;
}

int master_handler(waiter_t *waiter, unsigned short events ATTRIBUTE_UNUSED)
{
	master_t *master = waiter->private_data;
	int sock;
	sock = accept(waiter->fd, 0, 0);
	if (sock < 0) {
		int result = -errno;
		perrno("accept");
		return result;
	} else {
		pending_t *pending = calloc(sizeof(*pending), 1);
		pending->fd = sock;
		pending->local = master->local;
		pending->cookie = 0;
		add_waiter(sock, POLLIN, pending_handler, pending);
		list_add_tail(&pending->list, &pendings);
	}
	return 0;
}

int server(char *sockname, int port)
{
	struct list_head *item;
	int err;
	unsigned int k;

	if (sockname) {
		int sock = make_local_socket(sockname);
		master_t *master;
		if (sock < 0)
			return sock;
		master = calloc(sizeof(*master), 1);
		master->fd = sock;
		master->local = 1;
		add_waiter(sock, POLLIN, master_handler, master);
		list_add_tail(&master->list, &masters);
	}
	if (port >= 0) {
		int sock = make_inet_socket(port);
		master_t *master;
		if (sock < 0)
			return sock;
		master = calloc(sizeof(*master), 1);
		master->fd = sock;
		master->local = 0;
		add_waiter(sock, POLLIN, master_handler, master);
		list_add_tail(&master->list, &masters);
	}

	if (list_empty(&masters))
		return -EINVAL;

	list_for_each(item, &masters) {
		master_t *master = list_entry(item, master_t, list);
		if (fcntl(master->fd, F_SETFL, O_NONBLOCK) < 0) {
			int result = -errno;
			perrno("fcntl");
			return result;
		}
		if (listen(master->fd, 4) < 0) {
			int result = -errno;
			perrno("listen");
			return result;
		}
	}

	while (1) {
		struct pollfd pfds[OPEN_MAX];
		size_t pfds_count;
		do {
			err = poll(pollfds, pollfds_count, 1000);
		} while (err == 0);
		if (err < 0) {
			perrno("poll");
			continue;
		}

		pfds_count = pollfds_count;
		memcpy(pfds, pollfds, sizeof(*pfds) * pfds_count);
		for (k = 0; k < pfds_count; k++) {
			struct pollfd *pfd = &pfds[k];
			if (pfd->revents) {
				waiter_t *w = &waiters[pfd->fd];
				if (!w->handler)
					continue;
				err = w->handler(w, pfd->revents);
				if (err < 0)
					perrno("handler");
			}
		}
	}
	return 0;
}
					

void usage()
{
	fprintf(stderr, "\
Usage: %s [OPTIONS]

--help			help
--version		print current version
-l,--local SOCKNAME	local socket name
-p,--port PORT		port number
", command);
}

int main(int argc, char **argv)
{
	static struct option long_options[] = {
		{"local", 1, 0, 'l'},
		{"port", 1, 0, 'p'},
		{"help", 0, 0, 'h'}
	};
	int c;
	char *local = NULL;
	int port = -1;
	command = argv[0];
	while ((c = getopt_long(argc, argv, "hl:p:", long_options, 0)) != -1) {
		switch (c) {
		case 'h':
			usage();
			return 0;
		case 'l':
			local = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default:
			fprintf(stderr, "Try `%s --help' for more information\n", command);
			return 1;
		}
	}
	if (!local && port == -1) {
		fprintf(stderr, "%s: you need to specify at least one master socket\n", command);
		return 1;
	}

	server(local, port);
	return 0;
}
