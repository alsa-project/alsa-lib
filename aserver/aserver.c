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
#include <netdb.h>
#include <limits.h>

#include "asoundlib.h"
#include "aserver.h"
#include "list.h"

char *command;

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define ERROR(...) do {\
	fprintf(stderr, "%s %s:%i:(%s) ", command, __FILE__, __LINE__, __FUNCTION__); \
	fprintf(stderr, __VA_ARGS__); \
	putc('\n', stderr); \
} while (0)
#else
#define ERROR(args...) do {\
	fprintf(stderr, "%s %s:%i:(%s) ", command, __FILE__, __LINE__, __FUNCTION__); \
	fprintf(stderr, ##args); \
	putc('\n', stderr); \
} while (0)
#endif	

#define SYSERROR(string) ERROR(string ": %s", strerror(errno))

int make_local_socket(const char *filename)
{
	size_t l = strlen(filename);
	size_t size = offsetof(struct sockaddr_un, sun_path) + l;
	struct sockaddr_un *addr = alloca(size);
	int sock;

	sock = socket(PF_LOCAL, SOCK_STREAM, 0);
	if (sock < 0) {
		int result = -errno;
		SYSERROR("socket failed");
		return result;
	}
	
	unlink(filename);

	addr->sun_family = AF_LOCAL;
	memcpy(addr->sun_path, filename, l);

	if (bind(sock, (struct sockaddr *) addr, size) < 0) {
		int result = -errno;
		SYSERROR("bind failed");
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
		SYSERROR("socket failed");
		return result;
	}
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		int result = -errno;
		SYSERROR("bind failed");
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
	    SYSERROR("sendmsg failed");
	    return -errno;
    }
    return ret;
}

struct pollfd *pollfds;
unsigned int pollfds_count = 0;
typedef struct waiter waiter_t;
typedef int (*waiter_handler_t)(waiter_t *waiter, unsigned short events);
struct waiter {
	int fd;
	void *private_data;
	waiter_handler_t handler;
};
waiter_t *waiters;

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

typedef struct client client_t;

typedef struct {
	int (*open)(client_t *client, int *cookie);
	int (*cmd)(client_t *client);
	int (*close)(client_t *client);
} transport_ops_t;

struct client {
	struct list_head list;
	int poll_fd;
	int ctrl_fd;
	int local;
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
} inet_pending_t;
LIST_HEAD(inet_pendings);

int pcm_handler(waiter_t *waiter, unsigned short events)
{
	client_t *client = waiter->private_data;
	char buf[1];
	ssize_t n;
	if (events & POLLIN) {
		n = write(client->poll_fd, buf, 1);
		if (n != 1) {
			SYSERROR("write failed");
			return -errno;
		}
	} else if (events & POLLOUT) {
		n = read(client->poll_fd, buf, 1);
		if (n != 1) {
			SYSERROR("read failed");
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
	err = snd_pcm_open(&pcm, client->name, client->stream, SND_PCM_NONBLOCK);
	if (err < 0)
		return err;
	client->device.pcm.handle = pcm;
	client->device.pcm.fd = snd_pcm_poll_descriptor(pcm);

	shmid = shmget(IPC_PRIVATE, PCM_SHM_SIZE, 0666);
	if (shmid < 0) {
		result = -errno;
		SYSERROR("shmget failed");
		goto _err;
	}
	client->transport.shm.ctrl_id = shmid;
	client->transport.shm.ctrl = shmat(shmid, 0, 0);
	if (client->transport.shm.ctrl == (void*) -1) {
		result = -errno;
		shmctl(shmid, IPC_RMID, 0);
		SYSERROR("shmat failed");
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
	snd_pcm_shm_ctrl_t *ctrl = client->transport.shm.ctrl;
	if (client->polling) {
		del_waiter(client->device.pcm.fd);
		client->polling = 0;
	}
	err = snd_pcm_close(client->device.pcm.handle);
	ctrl->result = err;
	if (err < 0) 
		ERROR("snd_pcm_close");
	if (client->transport.shm.ctrl) {
		err = shmdt((void *)client->transport.shm.ctrl);
		if (err < 0)
			SYSERROR("shmdt failed");
		err = shmctl(client->transport.shm.ctrl_id, IPC_RMID, 0);
		if (err < 0)
			SYSERROR("shmctl IPC_RMID failed");
		client->transport.shm.ctrl = 0;
	}
	client->open = 0;
	return 0;
}

int shm_ack(client_t *client)
{
	struct pollfd pfd;
	int err;
	char buf[1];
	pfd.fd = client->ctrl_fd;
	pfd.events = POLLHUP;
	if (poll(&pfd, 1, 0) == 1)
		return -EBADFD;
	err = write(client->ctrl_fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	return 0;
}

int shm_ack_fd(client_t *client, int fd)
{
	struct pollfd pfd;
	int err;
	char buf[1];
	pfd.fd = client->ctrl_fd;
	pfd.events = POLLHUP;
	if (poll(&pfd, 1, 0) == 1)
		return -EBADFD;
	err = send_fd(client->ctrl_fd, buf, 1, fd);
	if (err != 1)
		return -EBADFD;
	return 0;
}

int pcm_shm_cmd(client_t *client)
{
	volatile snd_pcm_shm_ctrl_t *ctrl = client->transport.shm.ctrl;
	char buf[1];
	int err;
	int cmd;
	snd_pcm_t *pcm;
	err = read(client->ctrl_fd, buf, 1);
	if (err != 1)
		return -EBADFD;
	cmd = ctrl->cmd;
	ctrl->cmd = 0;
	pcm = client->device.pcm.handle;
	switch (cmd) {
	case SND_PCM_IOCTL_CARD:
		ctrl->result = snd_pcm_card(pcm);
		break;
	case SND_PCM_IOCTL_ASYNC:
		ctrl->result = snd_pcm_async(pcm, ctrl->u.async.sig, ctrl->u.async.pid);
		break;
	case SND_PCM_IOCTL_INFO:
		ctrl->result = snd_pcm_info(pcm, (snd_pcm_info_t *) &ctrl->u.info);
		break;
	case SND_PCM_IOCTL_HW_REFINE:
		ctrl->result = snd_pcm_hw_refine(pcm, (snd_pcm_hw_params_t *) &ctrl->u.hw_refine);
		break;
	case SND_PCM_IOCTL_HW_PARAMS:
		ctrl->result = snd_pcm_hw_params(pcm, (snd_pcm_hw_params_t *) &ctrl->u.hw_params);
		break;
	case SND_PCM_IOCTL_HW_FREE:
		ctrl->result = snd_pcm_hw_free(pcm);
		break;
	case SND_PCM_IOCTL_SW_PARAMS:
		ctrl->result = snd_pcm_sw_params(pcm, (snd_pcm_sw_params_t *) &ctrl->u.sw_params);
		break;
	case SND_PCM_IOCTL_STATUS:
		ctrl->result = snd_pcm_status(pcm, (snd_pcm_status_t *) &ctrl->u.status);
		break;
	case SND_PCM_IOCTL_STATE:
		ctrl->result = snd_pcm_state(pcm);
		break;
	case SND_PCM_IOCTL_DELAY:
		ctrl->result = snd_pcm_delay(pcm, (snd_pcm_sframes_t *) &ctrl->u.delay.frames);
		break;
	case SND_PCM_IOCTL_AVAIL_UPDATE:
		ctrl->result = snd_pcm_avail_update(pcm);
		ctrl->hw_ptr = *pcm->hw_ptr;
		break;
	case SND_PCM_IOCTL_PREPARE:
		ctrl->result = snd_pcm_prepare(pcm);
		ctrl->appl_ptr = *pcm->appl_ptr;
		ctrl->hw_ptr = *pcm->hw_ptr;
		break;
	case SND_PCM_IOCTL_RESET:
		ctrl->result = snd_pcm_reset(pcm);
		ctrl->appl_ptr = *pcm->appl_ptr;
		ctrl->hw_ptr = *pcm->hw_ptr;
		break;
	case SND_PCM_IOCTL_START:
		ctrl->result = snd_pcm_start(pcm);
		ctrl->appl_ptr = *pcm->appl_ptr;
		ctrl->hw_ptr = *pcm->hw_ptr;
		break;
	case SND_PCM_IOCTL_DRAIN:
		ctrl->result = snd_pcm_drain(pcm);
		break;
	case SND_PCM_IOCTL_DROP:
		ctrl->result = snd_pcm_drop(pcm);
		break;
	case SND_PCM_IOCTL_PAUSE:
		ctrl->result = snd_pcm_pause(pcm, ctrl->u.pause.enable);
		break;
	case SND_PCM_IOCTL_CHANNEL_INFO:
		ctrl->result = snd_pcm_channel_info(pcm, (snd_pcm_channel_info_t *) &ctrl->u.channel_info);
		if (ctrl->result >= 0 &&
		    ctrl->u.channel_info.type == SND_PCM_AREA_MMAP)
			return shm_ack_fd(client, ctrl->u.channel_info.u.mmap.fd);
		break;
	case SND_PCM_IOCTL_REWIND:
		ctrl->result = snd_pcm_rewind(pcm, ctrl->u.rewind.frames);
		ctrl->appl_ptr = *pcm->appl_ptr;
		break;
	case SND_PCM_IOCTL_LINK:
	{
		/* FIXME */
		ctrl->result = -ENOSYS;
		break;
	}
	case SND_PCM_IOCTL_UNLINK:
		ctrl->result = snd_pcm_unlink(pcm);
		break;
	case SND_PCM_IOCTL_MMAP:
	{
		ctrl->result = snd_pcm_mmap(pcm);
	}
	case SND_PCM_IOCTL_MUNMAP:
	{
		ctrl->result = snd_pcm_munmap(pcm);
		break;
	}
	case SND_PCM_IOCTL_MMAP_FORWARD:
		ctrl->result = snd_pcm_mmap_forward(pcm, ctrl->u.mmap_forward.frames);
		ctrl->appl_ptr = *pcm->appl_ptr;
		break;
	case SND_PCM_IOCTL_POLL_DESCRIPTOR:
		ctrl->result = 0;
		return shm_ack_fd(client, snd_pcm_poll_descriptor(pcm));
	case SND_PCM_IOCTL_CLOSE:
		client->ops->close(client);
		break;
	default:
		ERROR("Bogus cmd: %x", ctrl->cmd);
		ctrl->result = -ENOSYS;
	}
	return shm_ack(client);
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
		n = write(client->poll_fd, buf, 1);
		if (n != 1) {
			SYSERROR("write failed");
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
		SYSERROR("shmget failed");
		goto _err;
	}
	client->transport.shm.ctrl_id = shmid;
	client->transport.shm.ctrl = shmat(shmid, 0, 0);
	if (!client->transport.shm.ctrl) {
		result = -errno;
		shmctl(shmid, IPC_RMID, 0);
		SYSERROR("shmat failed");
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
	snd_ctl_shm_ctrl_t *ctrl = client->transport.shm.ctrl;
	if (client->polling) {
		del_waiter(client->device.control.fd);
		client->polling = 0;
	}
	err = snd_ctl_close(client->device.control.handle);
	ctrl->result = err;
	if (err < 0) 
		ERROR("snd_ctl_close");
	if (client->transport.shm.ctrl) {
		err = shmdt((void *)client->transport.shm.ctrl);
		if (err < 0)
			SYSERROR("shmdt failed");
		err = shmctl(client->transport.shm.ctrl_id, IPC_RMID, 0);
		if (err < 0)
			SYSERROR("shmctl failed");
		client->transport.shm.ctrl = 0;
	}
	client->open = 0;
	return 0;
}

extern int snd_ctl_read1(snd_ctl_t *ctl, snd_ctl_event_t *event);

int ctl_shm_cmd(client_t *client)
{
	snd_ctl_shm_ctrl_t *ctrl = client->transport.shm.ctrl;
	char buf[1];
	int err;
	int cmd;
	snd_ctl_t *ctl;
	err = read(client->ctrl_fd, buf, 1);
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
	case SND_CTL_IOCTL_HWDEP_NEXT_DEVICE:
		ctrl->result = snd_ctl_hwdep_next_device(ctl, &ctrl->u.device);
		break;
	case SND_CTL_IOCTL_HWDEP_INFO:
		ctrl->result = snd_ctl_hwdep_info(ctl, &ctrl->u.hwdep_info);
		break;
	case SND_CTL_IOCTL_PCM_NEXT_DEVICE:
		ctrl->result = snd_ctl_pcm_next_device(ctl, &ctrl->u.device);
		break;
	case SND_CTL_IOCTL_PCM_INFO:
		ctrl->result = snd_ctl_pcm_info(ctl, &ctrl->u.pcm_info);
		break;
	case SND_CTL_IOCTL_PCM_PREFER_SUBDEVICE:
		ctrl->result = snd_ctl_pcm_prefer_subdevice(ctl, ctrl->u.pcm_prefer_subdevice);
		break;
	case SND_CTL_IOCTL_RAWMIDI_NEXT_DEVICE:
		ctrl->result = snd_ctl_rawmidi_next_device(ctl, &ctrl->u.device);
		break;
	case SND_CTL_IOCTL_RAWMIDI_INFO:
		ctrl->result = snd_ctl_rawmidi_info(ctl, &ctrl->u.rawmidi_info);
		break;
	case SND_CTL_IOCTL_RAWMIDI_PREFER_SUBDEVICE:
		ctrl->result = snd_ctl_rawmidi_prefer_subdevice(ctl, ctrl->u.rawmidi_prefer_subdevice);
		break;
	case SND_CTL_IOCTL_READ:
		ctrl->result = snd_ctl_read1(ctl, &ctrl->u.read);
		break;
	case SND_CTL_IOCTL_CLOSE:
		client->ops->close(client);
		break;
	case SND_CTL_IOCTL_CARD:
		ctrl->result = snd_ctl_card(ctl);
		break;
	case SND_CTL_IOCTL_POLL_DESCRIPTOR:
		ctrl->result = 0;
		return shm_ack_fd(client, snd_ctl_poll_descriptor(ctl));
	default:
		ERROR("Bogus cmd: %x", ctrl->cmd);
		ctrl->result = -ENOSYS;
	}
	return shm_ack(client);
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
	err = read(client->ctrl_fd, &req, sizeof(req));
	if (err < 0) {
		SYSERROR("read failed");
		exit(1);
	}
	if (err != sizeof(req)) {
		ans.result = -EINVAL;
		goto _answer;
	}
	name = alloca(req.namelen);
	err = read(client->ctrl_fd, name, req.namelen);
	if (err < 0) {
		SYSERROR("read failed");
		exit(1);
	}
	if (err != req.namelen) {
		ans.result = -EINVAL;
		goto _answer;
	}

	switch (req.transport_type) {
	case SND_TRANSPORT_TYPE_SHM:
		if (!client->local) {
			ans.result = -EINVAL;
			goto _answer;
		}
		switch (req.dev_type) {
		case SND_DEV_TYPE_PCM:
			client->ops = &pcm_shm_ops;
			break;
		case SND_DEV_TYPE_CONTROL:
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
		ans.result = 0;
	}

 _answer:
	err = write(client->ctrl_fd, &ans, sizeof(ans));
	if (err != sizeof(ans)) {
		SYSERROR("write failed");
		exit(1);
	}
	return 0;
}

int client_poll_handler(waiter_t *waiter, unsigned short events ATTRIBUTE_UNUSED)
{
	client_t *client = waiter->private_data;
	if (client->open)
		client->ops->close(client);
	close(client->poll_fd);
	close(client->ctrl_fd);
	del_waiter(client->poll_fd);
	del_waiter(client->ctrl_fd);
	list_del(&client->list);
	free(client);
	return 0;
}

int client_ctrl_handler(waiter_t *waiter, unsigned short events)
{
	client_t *client = waiter->private_data;
	if (events & POLLHUP) {
		if (client->open)
			client->ops->close(client);
		close(client->ctrl_fd);
		del_waiter(client->ctrl_fd);
		list_del(&client->list);
		free(client);
		return 0;
	}
	if (client->open)
		return client->ops->cmd(client);
	else
		return snd_client_open(client);
}

int inet_pending_handler(waiter_t *waiter, unsigned short events)
{
	inet_pending_t *pending = waiter->private_data;
	inet_pending_t *pdata;
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

	list_for_each(item, &inet_pendings) {
		pdata = list_entry(item, inet_pending_t, list);
		if (pdata->cookie == cookie)
			goto found;
	}
	pending->cookie = cookie;
	return 0;

 found:
	client = calloc(1, sizeof(*client));
	client->local = 0;
	client->poll_fd = pdata->fd;
	client->ctrl_fd = waiter->fd;
	add_waiter(client->ctrl_fd, POLLIN | POLLHUP, client_ctrl_handler, client);
	add_waiter(client->poll_fd, POLLHUP, client_poll_handler, client);
	client->open = 0;
	list_add_tail(&client->list, &clients);
	list_del(&pending->list);
	list_del(&pdata->list);
	free(pending);
	free(pdata);
	return 0;
}

int local_handler(waiter_t *waiter, unsigned short events ATTRIBUTE_UNUSED)
{
	int sock;
	sock = accept(waiter->fd, 0, 0);
	if (sock < 0) {
		int result = -errno;
		SYSERROR("accept failed");
		return result;
	} else {
		client_t *client = calloc(1, sizeof(*client));
		client->ctrl_fd = sock;
		client->local = 1;
		client->open = 0;
		add_waiter(sock, POLLIN | POLLHUP, client_ctrl_handler, client);
		list_add_tail(&client->list, &clients);
	}
	return 0;
}

int inet_handler(waiter_t *waiter, unsigned short events ATTRIBUTE_UNUSED)
{
	int sock;
	sock = accept(waiter->fd, 0, 0);
	if (sock < 0) {
		int result = -errno;
		SYSERROR("accept failed");
		return result;
	} else {
		inet_pending_t *pending = calloc(1, sizeof(*pending));
		pending->fd = sock;
		pending->cookie = 0;
		add_waiter(sock, POLLIN, inet_pending_handler, pending);
		list_add_tail(&pending->list, &inet_pendings);
	}
	return 0;
}

int server(char *sockname, int port)
{
	int err;
	unsigned int k;
	long open_max;
	int result;

	if (!sockname && port < 0)
		return -EINVAL;
	open_max = sysconf(_SC_OPEN_MAX);
	if (open_max < 0) {
		result = -errno;
		SYSERROR("sysconf failed");
		return result;
	}
	pollfds = calloc(open_max, sizeof(*pollfds));
	waiters = calloc(open_max, sizeof(*waiters));

	if (sockname) {
		int sock = make_local_socket(sockname);
		if (sock < 0)
			return sock;
		if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
			result = -errno;
			SYSERROR("fcntl O_NONBLOCK failed");
			goto _end;
		}
		if (listen(sock, 4) < 0) {
			result = -errno;
			SYSERROR("listen failed");
			goto _end;
		}
		add_waiter(sock, POLLIN, local_handler, NULL);
	}
	if (port >= 0) {
		int sock = make_inet_socket(port);
		if (sock < 0)
			return sock;
		if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
			result = -errno;
			SYSERROR("fcntl failed");
			goto _end;
		}
		if (listen(sock, 4) < 0) {
			result = -errno;
			SYSERROR("listen failed");
			goto _end;
		}
		add_waiter(sock, POLLIN, inet_handler, NULL);
	}

	while (1) {
		struct pollfd pfds[open_max];
		size_t pfds_count;
		do {
			err = poll(pollfds, pollfds_count, -1);
		} while (err == 0);
		if (err < 0) {
			SYSERROR("poll failed");
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
					ERROR("waiter handler failed");
			}
		}
	}
 _end:
	free(pollfds);
	free(waiters);
	return result;
}
					

void usage()
{
	fprintf(stderr, "\
Usage: %s [OPTIONS] server

--help			help
", command);
}

extern int is_local(struct hostent *hent);

int main(int argc, char **argv)
{
	static struct option long_options[] = {
		{"help", 0, 0, 'h'}
	};
	int c;
	snd_config_t *conf;
	snd_config_iterator_t i;
	char *socket = NULL;
	char *host = NULL;
	long port = -1;
	int err;
	char *srvname;
	struct hostent *h;
	command = argv[0];
	while ((c = getopt_long(argc, argv, "h", long_options, 0)) != -1) {
		switch (c) {
		case 'h':
			usage();
			return 0;
		default:
			fprintf(stderr, "Try `%s --help' for more information\n", command);
			return 1;
		}
	}
	if (argc - optind != 1) {
		ERROR("you need to specify server name");
		return 1;
	}
	err = snd_config_update();
	if (err < 0) {
		ERROR("cannot read configuration file");
		return 1;
	}
	srvname = argv[optind];
	err = snd_config_searchv(snd_config, &conf, "server", srvname, 0);
	if (err < 0) {
		ERROR("Missing definition for server %s", srvname);
		return 1;
	}
	snd_config_foreach(i, conf) {
		snd_config_t *n = snd_config_entry(i);
		if (strcmp(n->id, "comment") == 0)
			continue;
		if (strcmp(n->id, "host") == 0) {
			err = snd_config_string_get(n, &host);
			if (err < 0) {
				ERROR("Invalid type for %s", n->id);
				return 1;
			}
			continue;
		}
		if (strcmp(n->id, "socket") == 0) {
			err = snd_config_string_get(n, &socket);
			if (err < 0) {
				ERROR("Invalid type for %s", n->id);
				return 1;
			}
			continue;
		}
		if (strcmp(n->id, "port") == 0) {
			err = snd_config_integer_get(n, &port);
			if (err < 0) {
				ERROR("Invalid type for %s", n->id);
				return 1;
			}
			continue;
		}
		ERROR("Unknown field %s", n->id);
		return 1;
	}
	if (!host) {
		ERROR("host is not defined");
		return 1;
	}
	h = gethostbyname(host);
	if (!h) {
		ERROR("Cannot resolve %s", host);
		return 1;
	}
	if (!is_local(h)) {
		ERROR("%s is not the local host", host);
		return 1;
	}
	if (!socket && port < 0) {
		ERROR("either socket or port need to be defined");
		return 1;
	}
	server(socket, port);
	return 0;
}
