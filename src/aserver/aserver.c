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

typedef struct client client_t;

typedef struct {
	int (*open)(client_t *client, int *cookie);
	int (*cmd)(client_t *client);
	int (*close)(client_t *client);
	int (*poll_prepare)(client_t *client, struct pollfd *pfds, int pindex);
	void (*poll_events)(client_t *client, struct pollfd *pfds);
} transport_ops_t;

struct client {
	struct socket {
		int fd;
		int pindex;
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
			int pindex;
		} pcm;
#if 0
		struct {
			snd_ctl_t *handle;
		} control;
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
	enum { CLOSED = 0, STOPPED, NORMAL, UNKNOWN } state;
	int cookie;
	union {
		struct {
			int ctrl_id;
			void *ctrl;
		} shm;
	} transport;
};

#define PENDINGS_MAX 4
#define CLIENTS_MAX 2

client_t clients[CLIENTS_MAX];
int clients_count = 0;

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
	client->device.pcm.fd = snd_pcm_file_descriptor(pcm);

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
	client->state = CLOSED;
	return 0;
}

int pcm_poll_prepare(client_t *client, struct pollfd *pfds, int pindex)
{
	struct pollfd *pfd = &pfds[pindex];
	pfd->events = 0;
	switch (client->state) {
	case UNKNOWN:
		pfd->events = client->stream == SND_PCM_STREAM_PLAYBACK ? POLLOUT : POLLIN;
	case NORMAL:
		pfd->fd = client->device.pcm.fd;
		client->device.pcm.pindex = pindex;
		return 1;
		break;
	default:
		client->device.pcm.pindex = -1;
		return 0;
	}
}

void pcm_poll_events(client_t *client, struct pollfd *pfds)
{
	int n;
	char zero = 0;
	struct pollfd *pfd;
	if (client->device.pcm.pindex < 0)
		return;
	pfd = &pfds[client->device.pcm.pindex];
	if (pfd->revents & POLLIN) {
		client->state = NORMAL;
		n = write(client->data.fd, &zero, 1);
		if (n != 1) {
			perrno("write");
			exit(1);
		}
	} else if (pfd->revents & POLLOUT) {
		client->state = NORMAL;
		n = read(client->data.fd, &zero, 1);
		if (n != 1) {
			perrno("read");
			exit(1);
		}
	}
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
	case SND_PCM_IOCTL_FRAME_IO:
		ctrl->result = snd_pcm_frame_io(pcm, ctrl->u.frame_io);
		break;
	case SND_PCM_IOCTL_PREPARE:
		ctrl->result = snd_pcm_prepare(pcm);
		break;
	case SND_PCM_IOCTL_GO:
		ctrl->result = snd_pcm_go(pcm);
		break;
	case SND_PCM_IOCTL_FLUSH:
		ctrl->result = snd_pcm_flush(pcm);
		break;
	case SND_PCM_IOCTL_DRAIN:
		ctrl->result = snd_pcm_drain(pcm);
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
	case SND_PCM_IOCTL_WRITE_FRAMES:
	{
		size_t maxsize = PCM_SHM_DATA_MAXLEN;
		client->state = UNKNOWN;
		maxsize = snd_pcm_bytes_to_frames(pcm, maxsize);
		if (ctrl->u.write.count > maxsize) {
			ctrl->result = -EFAULT;
			break;
		}
		/* FIXME: blocking */
		ctrl->result = snd_pcm_write(pcm, ctrl->data, ctrl->u.write.count);
		break;
	}
	case SND_PCM_IOCTL_READ_FRAMES:
	{
		size_t maxsize = PCM_SHM_DATA_MAXLEN;
		client->state = UNKNOWN;
		maxsize = snd_pcm_bytes_to_frames(pcm, maxsize);
		if (ctrl->u.read.count > maxsize) {
			ctrl->result = -EFAULT;
			break;
		}
		/* FIXME: blocking */
		ctrl->result = snd_pcm_read(pcm, ctrl->data, ctrl->u.read.count);
		break;
	}
	case SND_PCM_IOCTL_WRITEV_FRAMES:
	{
		/* FIXME: interleaved */
		size_t maxsize = PCM_SHM_DATA_MAXLEN;
		unsigned long k;
		struct iovec *vector;
		size_t vecsize;
		char *base;
		int bits_per_sample = snd_pcm_samples_to_bytes(pcm, 8);
		client->state = UNKNOWN;
		vecsize = ctrl->u.writev.count * sizeof(struct iovec);
		if (vecsize > maxsize) {
			ctrl->result = -EFAULT;
			break;
		}
		maxsize -= vecsize;
		vector = (struct iovec *) ctrl->data;
		base = ctrl->data + vecsize;
		for (k = 0; k < ctrl->u.writev.count; ++k) {
			unsigned long ofs = (unsigned long) vector[k].iov_base;
			size_t len = vector[k].iov_len * bits_per_sample / 8;
			if (ofs + len > maxsize)
				return -EFAULT;
			vector[k].iov_base = base + ofs;
		}
		/* FIXME: blocking */
		ctrl->result = snd_pcm_writev(pcm, vector, ctrl->u.writev.count);
		break;
	}
	case SND_PCM_IOCTL_READV_FRAMES:
	{
		/* FIXME: interleaved */
		size_t maxsize = PCM_SHM_DATA_MAXLEN;
		unsigned long k;
		struct iovec *vector;
		size_t vecsize;
		char *base;
		int bits_per_sample = snd_pcm_samples_to_bytes(pcm, 8);
		client->state = UNKNOWN;
		vecsize = ctrl->u.readv.count * sizeof(struct iovec);
		if (vecsize > maxsize) {
			ctrl->result = -EFAULT;
			break;
		}
		maxsize -= vecsize;
		vector = (struct iovec *) ctrl->data;
		base = ctrl->data + vecsize;
		for (k = 0; k < ctrl->u.readv.count; ++k) {
			unsigned long ofs = (unsigned long) vector[k].iov_base;
			size_t len = vector[k].iov_len * bits_per_sample / 8;
			if (ofs + len > maxsize)
				return -EFAULT;
			vector[k].iov_base = base + ofs;
		}
		/* FIXME: blocking */
		ctrl->result = snd_pcm_readv(pcm, vector, ctrl->u.readv.count);
		break;
	}
	case SND_PCM_IOCTL_FRAME_DATA:
		client->state = UNKNOWN;
		ctrl->result = snd_pcm_frame_data(pcm, ctrl->u.frame_data);
		break;
	case SND_PCM_IOCTL_LINK:
	{
		int k;
		for (k = 0; k < clients_count; ++k) {
			if (clients[k].state == CLOSED)
				continue;
			if (clients[k].data.fd == ctrl->u.link) {
				ctrl->result = snd_pcm_link(pcm, clients[k].device.pcm.handle);
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
#if 0
	case SND_PCM_IOCTL_MUNMAP_DATA:
	case SND_PCM_IOCTL_MUNMAP_CONTROL:
	case SND_PCM_IOCTL_MUNMAP_STATUS:
		ctrl->result = 0;
		break;
	}
#endif
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
	return 0;
}

transport_ops_t pcm_shm_ops = {
	open: pcm_shm_open,
	cmd: pcm_shm_cmd,
	close: pcm_shm_close,
	poll_prepare: pcm_poll_prepare,
	poll_events: pcm_poll_events,
};

void snd_client_open(client_t *client)
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
		client->state = STOPPED;
		ans.result = client->data.fd;
	}

 _answer:
	err = write(client->ctrl.fd, &ans, sizeof(ans));
	if (err != sizeof(ans)) {
		perrno("write");
		exit(1);
	}
	return;
}

int server(char *sockname, int port)
{
	typedef struct {
		int fd;
		int pindex;
		int local;
	} master_t;
	master_t masters[2];
	int masters_count = 0;
	typedef struct {
		int fd;
		int pindex;
		uint32_t cookie;
		int local;
	} pending_t;
	pending_t pendings[PENDINGS_MAX];
	int pendings_count = 0;
	struct pollfd pfds[CLIENTS_MAX * 3 + 16];
	int pfds_count;
	int err;
	int k;

	if (sockname) {
		int master = make_local_socket(sockname);
		if (master < 0)
			return master;
		masters[masters_count].fd = master;
		masters[masters_count].local = 1;
		masters_count++;
	}
	if (port >= 0) {
		int master = make_inet_socket(port);
		if (master < 0)
			return master;
		masters[masters_count].fd = master;
		masters[masters_count].local = 0;
		masters_count++;
	}

	if (masters_count == 0)
		return -EINVAL;

	for (k = 0; k < masters_count; ++k) {
		master_t *master = &masters[k];
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

	for (k = 0; k < PENDINGS_MAX; ++k) {
		pendings[k].fd = -1;
	}

	while (1) {
		pfds_count = 0;
		
		/* Prepare to poll masters */
		if (pendings_count < PENDINGS_MAX) {
			for (k = 0; k < masters_count; ++k) {
				master_t *master = &masters[k];
				master->pindex = pfds_count;
				pfds[pfds_count].fd = master->fd;
				pfds[pfds_count].events = POLLIN;
				pfds_count++;
			}
		} else {
			for (k = 0; k < masters_count; ++k) {
				master_t *master = &masters[k];
				master->pindex = -1;
			}
		}

		/* Prepare to poll pendings */
		for (k = 0; k < PENDINGS_MAX; ++k) {
			pending_t *pending = &pendings[k];
			if (pending->fd < 0 ||
			    pending->cookie != 0) {
				pending->pindex = -1;
				continue;
			}
			pending->pindex = pfds_count;
			pfds[pfds_count].fd = pending->fd;
			pfds[pfds_count].events = POLLHUP;
			if (pendings_count < PENDINGS_MAX &&
			    clients_count < CLIENTS_MAX)
				pfds[pfds_count].events |= POLLIN;
			pfds_count++;
		}

		/* Prepare to poll clients */
		for (k = 0; k < clients_count; ++k) {
			client_t *client = &clients[k];
			client->data.pindex = pfds_count;
			pfds[pfds_count].fd = client->data.fd;
			pfds[pfds_count].events = POLLHUP;
			pfds_count++;

			client->ctrl.pindex = pfds_count;
			pfds[pfds_count].fd = client->ctrl.fd;
			pfds[pfds_count].events = POLLIN | POLLHUP;
			pfds_count++;
		}

		/* Prepare to poll devices */
		for (k = 0; k < clients_count; ++k) {
			client_t *client = &clients[k];
			int n;
			if (client->state == CLOSED)
				continue;
			n = client->ops->poll_prepare(client, pfds, pfds_count);
			pfds_count += n;
		}
		

		/* Poll */
		do {
			err = poll(pfds, pfds_count, 1000);
		} while (err == 0);
		if (err < 0) {
			int result = -errno;
			perrno("poll");
			return result;
		}

		/* Handle clients events */
		for (k = clients_count - 1; k >= 0; --k) {
			client_t *client;
			struct pollfd *data_pfd = &pfds[clients[k].data.pindex];
			struct pollfd *ctrl_pfd = &pfds[clients[k].ctrl.pindex];
			if (!data_pfd->revents && !ctrl_pfd->revents)
				continue;
			client = &clients[k];
			if ((data_pfd->revents & POLLHUP) ||
			    (ctrl_pfd->revents & POLLHUP)) {
				if (client->state != CLOSED) {
					client->ops->close(client);
				}
				close(client->data.fd);
				if (client->ctrl.fd >= 0)
					close(client->ctrl.fd);
				memmove(client, client + 1,
					(clients_count - k) * sizeof(client_t));
				clients_count--;
				continue;
			}
			if (ctrl_pfd->revents & POLLIN) {
				if (client->state == CLOSED)
					snd_client_open(client);
				else
					client->ops->cmd(client);
			}
		}

		/* Handle device events */
		for (k = 0; k < clients_count; ++k) {
			client_t *client = &clients[k];
			client->ops->poll_events(client, pfds);
		}

		/* Handle pending events */
		for (k = 0; k < PENDINGS_MAX; ++k) {
			struct pollfd *pfd;
			uint32_t cookie;
			int j;
			pending_t *pending = &pendings[k];
			client_t *client;
			int remove = 0;
			if (pending->pindex < 0)
				continue;
			pfd = &pfds[pending->pindex];
			if (!pfd->revents)
				continue;
			if (pfd->revents & POLLHUP)
				remove = 1;
			else {
				if (clients_count >= CLIENTS_MAX)
					continue;
				err = read(pfd->fd, &cookie, sizeof(cookie));
				if (err != sizeof(cookie))
					remove = 1;
				else {
					err = write(pfd->fd, &cookie, sizeof(cookie));
					if (err != sizeof(cookie))
						remove = 1;
				}
			}
			if (remove) {
				close(pending->fd);
				pending->fd = -1;
				pendings_count--;
				continue;
			}

			for (j = 0; j < PENDINGS_MAX; ++j) {
				if (pendings[j].cookie == cookie)
					break;
			}
			if (j == PENDINGS_MAX) {
				pendings[k].cookie = cookie;
				continue;
			}
			
			client = &clients[clients_count];
			memset(client, 0, sizeof(*client));
			client->data.fd = pendings[j].fd;
			client->data.local = pendings[j].local;
			client->ctrl.fd = pendings[k].fd;
			client->ctrl.local = pendings[k].local;
			client->state = CLOSED;
			clients_count++;
			pendings[j].fd = -1;
			pendings[k].fd = -1;
			pendings_count -= 2;
		}

		/* Handle master events */
		for (k = 0; k < masters_count; ++k) {
			struct pollfd *pfd;
			master_t *master;
			int sock;
			if (pendings_count >= PENDINGS_MAX)
				break;
			master = &masters[k];
			if (master->pindex < 0)
				continue;
			pfd = &pfds[master->pindex];
			if (!pfd->revents)
				continue;
			
			sock = accept(master->fd, 0, 0);
			if (sock < 0) {
				int result = -errno;
                                perrno("accept");
                                return result;
			} else {
				int j;
				for (j = 0; j < PENDINGS_MAX; ++j) {
					if (pendings[j].fd < 0)
						break;
				}
				assert(j < PENDINGS_MAX);
				pendings[j].fd = sock;
				pendings[j].local = master->local;
				pendings[j].cookie = 0;
				pendings_count++;
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
