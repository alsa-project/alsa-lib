/*
 *  Async notification helpers
 *  Copyright (c) 2001 by Abramo Bagnara <abramo@alsa-project.org>
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

#include "pcm/pcm_local.h"
#include "control/control_local.h"
#include <signal.h>

#ifdef SND_ASYNC_RT_SIGNAL
int snd_async_signo;
void snd_async_init(void) __attribute__ ((constructor));

void snd_async_init(void)
{
	snd_async_signo = __libc_allocate_rtsig(0);
	if (snd_async_signo < 0) {
		SNDERR("Unable to find a RT signal to use for snd_async");
		exit(1);
	}
}
#else
int snd_async_signo = SIGIO;
#endif

static struct list_head snd_async_handlers;

static void snd_async_handler(int signo ATTRIBUTE_UNUSED, siginfo_t *siginfo, void *context ATTRIBUTE_UNUSED)
{
	int fd;
	struct list_head *i;
	assert(siginfo->si_code = SI_SIGIO);
	fd = siginfo->si_fd;
	list_for_each(i, &snd_async_handlers) {
		snd_async_handler_t *h = list_entry(i, snd_async_handler_t, glist);
		if (h->fd == fd) {
			h->callback(h);
			// break;
		}
	}
}

int snd_async_add_handler(snd_async_handler_t **handler, int fd, 
			  snd_async_callback_t callback, void *private_data)
{
	snd_async_handler_t *h;
	int was_empty;
	h = malloc(sizeof(*h));
	if (!h)
		return -ENOMEM;
	h->fd = fd;
	h->callback = callback;
	h->private_data = private_data;
	was_empty = list_empty(&snd_async_handlers);
	list_add_tail(&h->glist, &snd_async_handlers);
	*handler = h;
	if (was_empty) {
		int err;
		struct sigaction act;
		act.sa_flags = SA_RESTART | SA_SIGINFO;
		act.sa_sigaction = snd_async_handler;
		sigemptyset(&act.sa_mask);
		err = sigaction(snd_async_signo, &act, NULL);
		if (err < 0) {
			SYSERR("sigaction");
			return -errno;
		}
	}
	return 0;
}

int snd_async_del_handler(snd_async_handler_t *handler)
{
	int err = 0;
	list_del(&handler->glist);
	if (list_empty(&snd_async_handlers)) {
		struct sigaction act;
		act.sa_flags = 0;
		act.sa_handler = SIG_DFL;
		err = sigaction(snd_async_signo, &act, NULL);
		if (err < 0) {
			SYSERR("sigaction");
			return -errno;
		}
	}
	if (handler->type == SND_ASYNC_HANDLER_GENERIC)
		goto _end;
	list_del(&handler->hlist);
	if (!list_empty(&handler->hlist))
		goto _end;
	switch (handler->type) {
	case SND_ASYNC_HANDLER_PCM:
		err = snd_pcm_async(handler->u.pcm, -1, 1);
		break;
	case SND_ASYNC_HANDLER_CTL:
		err = snd_ctl_async(handler->u.ctl, -1, 1);
		break;
	default:
		assert(0);
	}
 _end:
	free(handler);
	return err;
}

int snd_async_handler_get_fd(snd_async_handler_t *handler)
{
	return handler->fd;
}

void *snd_async_handler_get_callback_private(snd_async_handler_t *handler)
{
	return handler->private_data;
}

