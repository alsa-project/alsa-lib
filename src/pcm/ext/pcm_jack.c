/**
 * \file pcm/pcm_jack.c
 * \ingroup PCM_Plugins
 * \brief PCM Jack Plugin Interface
 * \author Maarten de Boer <mdeboer@iua.upf.es>
 * \date 2003
 */
/*
 *  PCM - File plugin
 *  Copyright (c) 2003 by Maarten de Boer <mdeboer@iua.upf.es>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "config.h"

#include <byteswap.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "pcm_local.h"
#include <jack/jack.h>

#undef PCM_JACK_DEBUG

#ifndef PIC
/* entry for static linking */
const char *_snd_module_pcm_jack = "";
#endif

#ifndef DOC_HIDDEN

typedef enum _jack_format {
	SND_PCM_JACK_FORMAT_RAW
} snd_pcm_jack_format_t;

typedef struct {
	int fd;
	int activated;		/* jack is activated? */
	snd_htimestamp_t trigger_tstamp;
	snd_pcm_uframes_t avail_max;
	snd_pcm_state_t state;
	snd_pcm_uframes_t appl_ptr;
	snd_pcm_uframes_t hw_ptr;

	char** playback_ports;
	char** capture_ports;
	unsigned int playback_ports_n;
	unsigned int capture_ports_n;

	unsigned int channels;
	snd_pcm_channel_area_t *areas;

	jack_port_t **ports;
	jack_client_t *client;
} snd_pcm_jack_t;

#endif /* DOC_HIDDEN */

static int snd_pcm_jack_close(snd_pcm_t *pcm)
{
	snd_pcm_jack_t *jack = pcm->private_data;
	int err = 0;

#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_close\n"); fflush(stdout);
#endif
	if (jack->client)
	{
		jack_client_close(jack->client);
		jack->client = 0;
	}

	if (jack->playback_ports)
		free(jack->playback_ports);
	if (jack->capture_ports)
		free(jack->capture_ports);
	if (jack->areas)
		free(jack->areas);
	free(jack);

	return err;
}

static int snd_pcm_jack_nonblock(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int nonblock ATTRIBUTE_UNUSED)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_nonblock\n"); fflush(stdout);
#endif
	return 0;
}

static int snd_pcm_jack_async(snd_pcm_t *pcm ATTRIBUTE_UNUSED, int sig ATTRIBUTE_UNUSED, pid_t pid ATTRIBUTE_UNUSED)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_async\n"); fflush(stdout);
#endif
	return -ENOSYS;
}

static int snd_pcm_jack_poll_revents(snd_pcm_t *pcm ATTRIBUTE_UNUSED, struct pollfd *pfds, unsigned int nfds, unsigned short *revents)
{
	// snd_pcm_jack_t *jack = pcm->private_data;
	char buf[1];
	
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_poll_revents\n"); fflush(stdout);
#endif
	
	assert(pfds && nfds == 1 && revents);

	read(pfds[0].fd, buf, 1);

	*revents = pfds[0].revents;
	return 0;
}

static int snd_pcm_jack_info(snd_pcm_t *pcm, snd_pcm_info_t * info)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_info\n"); fflush(stdout);
#endif
	memset(info, 0, sizeof(*info));
	info->stream = pcm->stream;
	info->card = -1;
	strncpy(info->id, pcm->name, sizeof(info->id));
	strncpy(info->name, pcm->name, sizeof(info->name));
	strncpy(info->subname, pcm->name, sizeof(info->subname));
	info->subdevices_count = 1;
	return 0;
}

static int snd_pcm_jack_channel_info(snd_pcm_t *pcm, snd_pcm_channel_info_t * info)
{
	// snd_pcm_jack_t *jack = pcm->private_data;
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_channel_info\n"); fflush(stdout);
#endif
	return snd_pcm_channel_info_shm(pcm, info, -1);
}

static int snd_pcm_jack_status(snd_pcm_t *pcm, snd_pcm_status_t * status)
{
	snd_pcm_jack_t *jack = pcm->private_data;
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_status\n"); fflush(stdout);
#endif
	memset(status, 0, sizeof(*status));
	status->state = jack->state;
	status->trigger_tstamp = jack->trigger_tstamp;
	// gettimeofday(&status->tstamp, 0);
	status->avail = pcm->buffer_size;
	status->avail_max = jack->avail_max;
	return 0;
}

static snd_pcm_state_t snd_pcm_jack_state(snd_pcm_t *pcm)
{
	snd_pcm_jack_t *jack = pcm->private_data;
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_state\n"); fflush(stdout);
#endif
	return jack->state;
}

static int snd_pcm_jack_hwsync(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_hwsync\n"); fflush(stdout);
#endif
	return 0;
}

static int snd_pcm_jack_delay(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sframes_t *delayp)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_delay\n"); fflush(stdout);
#endif
	*delayp = snd_pcm_mmap_hw_avail(pcm); 
	return 0;
}

int
snd_pcm_jack_process_cb (jack_nframes_t nframes, snd_pcm_t *pcm)
{
	snd_pcm_jack_t *jack = pcm->private_data;
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t xfer = 0, samples;
	char buf[1];
	unsigned int channel;
	
#ifdef PCM_JACK_DEBUG
	printf("PROCESS %d! (%u)\n",jack->state, (unsigned)nframes);
#endif

	for (channel = 0; channel < jack->channels; channel++) {
		jack->areas[channel].addr = 
			jack_port_get_buffer (jack->ports[channel], nframes);
		jack->areas[channel].first = 0;
		jack->areas[channel].step = pcm->sample_bits;
	}
		
	if (jack->state != SND_PCM_STATE_RUNNING) {
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
			for (channel = 0; channel < jack->channels; channel++) {
				snd_pcm_area_silence(&jack->areas[channel], 0, nframes, pcm->format);
			}
			return 0;
		}
	}
	
	areas = snd_pcm_mmap_areas(pcm);

	while (xfer < nframes)
	{
		snd_pcm_uframes_t frames = nframes - xfer;
		snd_pcm_uframes_t offset = snd_pcm_mmap_hw_offset(pcm);
		snd_pcm_uframes_t cont = pcm->buffer_size - offset;

		if (cont < frames)
			frames = cont;

#ifdef PCM_JACK_DEBUG
		printf("snd_pcm_jack_process_cb hw=%d=%d + nframes=%d / frames=%d / bufsize=%d\n",
			(int)offset,(int)jack->hw_ptr,(int)nframes,(int)frames,(int)pcm->buffer_size); fflush(stdout);
#endif

		for (channel = 0; channel < jack->channels; channel++)
		{
			if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
				snd_pcm_area_copy(&jack->areas[channel], xfer, &areas[channel], offset, frames, pcm->format);
			} else {
				snd_pcm_area_copy(&areas[channel], offset, &jack->areas[channel], xfer, frames, pcm->format);
			}
		}
		
		snd_pcm_mmap_hw_forward(pcm,frames);
		xfer += frames;
	}

	if (pcm->stop_threshold < pcm->boundary) {
		samples = snd_pcm_mmap_avail(pcm);
		if (samples >= pcm->stop_threshold) {
			struct timeval tv;
			gettimeofday(&tv, 0);
			jack->trigger_tstamp.tv_sec = tv.tv_sec;
			jack->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
			jack->state = SND_PCM_STATE_XRUN;
			jack->avail_max = samples;
		}
	}
	                         	
	write(jack->fd,buf,1); /* for polling */

#ifdef PCM_JACK_DEBUG
	printf("jack_process = %d\n",(int)snd_pcm_mmap_hw_offset(pcm)); fflush(stdout);
#endif
	
	return 0;      
}

static int snd_pcm_jack_prepare(snd_pcm_t *pcm)
{
	unsigned int i;

	snd_pcm_jack_t *jack = pcm->private_data;
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_prepare\n"); fflush(stdout);
#endif
	jack->state = SND_PCM_STATE_PREPARED;
	*pcm->appl.ptr = 0;
	*pcm->hw.ptr = 0;

	jack->appl_ptr = jack->hw_ptr = 0;

	jack->ports = calloc (pcm->channels, sizeof(jack_port_t*));
	for (i = 0; i < pcm->channels; i++)
	{
		char port_name[32];
		if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {

			sprintf(port_name,"out_%03d\n",i);
			jack->ports[i] = jack_port_register (
				jack->client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		}else{
			sprintf(port_name,"in__%03d\n",i);
			jack->ports[i] = jack_port_register (
				jack->client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		}
	}

	jack_set_process_callback (jack->client,
		(JackProcessCallback)snd_pcm_jack_process_cb, pcm);
	return 0;
}

static int snd_pcm_jack_reset(snd_pcm_t *pcm)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_reset\n"); fflush(stdout);
#endif
	*pcm->appl.ptr = 0;
	*pcm->hw.ptr = 0;
	return 0;
}

static int snd_pcm_jack_start(snd_pcm_t *pcm)
{
	snd_pcm_jack_t *jack = pcm->private_data;
	unsigned int i;
	struct timeval tv;
	
	assert(jack->state == SND_PCM_STATE_PREPARED);

#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_start\n"); fflush(stdout);
#endif

	if (jack_activate (jack->client))
		return -EIO;
	jack->activated = 1;

	if (pcm->stream == SND_PCM_STREAM_PLAYBACK) {
		for (i = 0; i < pcm->channels && i < jack->playback_ports_n; i++)
		{	
			if ( jack->playback_ports[i]) {
				if (jack_connect (jack->client, 
					jack_port_name (jack->ports[i]), 
					jack->playback_ports[i]))
				{
					fprintf(stderr, "cannot connect %s to %s\n",
						jack_port_name(jack->ports[i]),
						jack->playback_ports[i]);
					return -EIO;
				}
#ifdef PCM_JACK_DEBUG
				printf("connected %s to %s\n",
					jack_port_name(jack->ports[i]),
					jack->playback_ports[i]);
				fflush(stdout);
#endif
			}
		}
	}else{
		for (i = 0; i < pcm->channels && i < jack->capture_ports_n; i++)
		{	
			if ( jack->capture_ports[i]) {
				if (jack_connect (jack->client, 
					jack->capture_ports[i],
					jack_port_name (jack->ports[i])))
				{
					fprintf(stderr, "cannot connect %s to %s\n",
						jack->capture_ports[i],
						jack_port_name(jack->ports[i]));
					return -EIO;
				}
#ifdef PCM_JACK_DEBUG
				printf("connected %s to %s\n",
					jack->capture_ports[i],
					jack_port_name(jack->ports[i]));
				fflush(stdout);
#endif
			}
		}
	}
	
	gettimeofday(&tv, 0);
	jack->trigger_tstamp.tv_sec = tv.tv_sec;
	jack->trigger_tstamp.tv_nsec = tv.tv_usec * 1000L;
	jack->state = SND_PCM_STATE_RUNNING;

	return 0;
}

static int snd_pcm_jack_drop(snd_pcm_t *pcm)
{
	snd_pcm_jack_t *jack = pcm->private_data;
	
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_drop\n"); fflush(stdout);
#endif
	assert(jack->state != SND_PCM_STATE_OPEN);
#if 0
	if (jack->activated) {
		printf("deactivate\n");
		jack_deactivate(jack->client);
		printf("deactivate done\n");
		jack->activated = 0;
	}
	{
	unsigned i;
	for (i = 0; i < pcm->channels; i++) {
		if (jack->ports[i]) {
			jack_port_unregister(jack->client, jack->ports[i]);
			jack->ports[i] = NULL;
		}
	}
	}
#endif
	jack->state = SND_PCM_STATE_SETUP;
	return 0;
}

static int snd_pcm_jack_drain(snd_pcm_t *pcm)
{
	snd_pcm_jack_t *jack = pcm->private_data;
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_drain\n"); fflush(stdout);
#endif
	assert(jack->state != SND_PCM_STATE_OPEN);
	return snd_pcm_jack_drop(pcm);
}

static int snd_pcm_jack_pause(snd_pcm_t *pcm, int enable)
{
	snd_pcm_jack_t *jack = pcm->private_data;
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_pause\n"); fflush(stdout);
#endif
	if (enable) {
		if (jack->state != SND_PCM_STATE_RUNNING)
			return -EBADFD;
		jack->state = SND_PCM_STATE_PAUSED;
	} else {
		if (jack->state != SND_PCM_STATE_PAUSED)
			return -EBADFD;
		jack->state = SND_PCM_STATE_RUNNING;
	}
	return 0;
}

static snd_pcm_sframes_t snd_pcm_jack_rewind(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_rewind\n"); fflush(stdout);
#endif
	snd_pcm_mmap_appl_backward(pcm, frames);
	return frames;
}

static snd_pcm_sframes_t snd_pcm_jack_forward(snd_pcm_t *pcm, snd_pcm_uframes_t frames)
{
	snd_pcm_sframes_t avail;

#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_forward\n"); fflush(stdout);
#endif
	avail = snd_pcm_mmap_avail(pcm);
	if (avail < 0)
		return 0;
	if (frames > (snd_pcm_uframes_t) avail)
		frames = (snd_pcm_uframes_t) avail;
	snd_pcm_mmap_appl_forward(pcm, frames);
	return frames;
}

static int snd_pcm_jack_resume(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	return 0;
}

static snd_pcm_sframes_t snd_pcm_jack_mmap_commit(snd_pcm_t *pcm,
						  snd_pcm_uframes_t offset ATTRIBUTE_UNUSED,
						  snd_pcm_uframes_t size)
{
	snd_pcm_mmap_appl_forward(pcm, size);
	return size;
}

static snd_pcm_sframes_t snd_pcm_jack_avail_update(snd_pcm_t *pcm)
{
	snd_pcm_jack_t *jack = pcm->private_data;
	snd_pcm_sframes_t ret = snd_pcm_mmap_avail(pcm);
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_avail_update appl=%d hw=%d ret=%d\n",(int)jack->appl_ptr,(int)jack->hw_ptr,(int)ret); fflush(stdout);
#endif
	return ret;
}

static inline snd_mask_t *hw_param_mask(snd_pcm_hw_params_t *params,
					snd_pcm_hw_param_t var)
{
	return &params->masks[var - SND_PCM_HW_PARAM_FIRST_MASK];
}

static inline snd_interval_t *hw_param_interval(snd_pcm_hw_params_t *params,
						snd_pcm_hw_param_t var)
{
	return &params->intervals[var - SND_PCM_HW_PARAM_FIRST_INTERVAL];
}

static int snd_pcm_jack_hw_refine(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
	int err;
	snd_pcm_jack_t *jack = pcm->private_data;
	static snd_mask_t access = { .bits = { 
					(1<<SNDRV_PCM_ACCESS_MMAP_INTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_MMAP_NONINTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_RW_INTERLEAVED) |
					(1<<SNDRV_PCM_ACCESS_RW_NONINTERLEAVED),
					0, 0, 0 } };
	static snd_pcm_format_mask_t format_mask = { { 1U<<SNDRV_PCM_FORMAT_FLOAT } };
	snd_interval_t t;

#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_hw_refine\n"); fflush(stdout);
#endif

	err = snd_mask_refine(hw_param_mask(params, SND_PCM_HW_PARAM_ACCESS), &access);
	if (err < 0)
		return err;
	err = snd_mask_refine(hw_param_mask(params, SND_PCM_HW_PARAM_FORMAT), &format_mask);
	if (err < 0)
		return err;
	err = snd_interval_refine_set(hw_param_interval(params, SND_PCM_HW_PARAM_CHANNELS), jack->channels);
	if (err < 0)
		return err;
	err = snd_interval_refine_set(hw_param_interval(params, SND_PCM_HW_PARAM_RATE), jack_get_sample_rate(jack->client));
	if (err < 0)
		return err;
	/* limit to something useful */
	snd_interval_set_minmax(&t, 128, 1024*1024);
	err = snd_interval_refine(hw_param_interval(params, SND_PCM_HW_PARAM_BUFFER_SIZE), &t);
	if (err < 0)
		return err;
	snd_interval_set_minmax(&t, 64, 1024*1024);
	err = snd_interval_refine(hw_param_interval(params, SND_PCM_HW_PARAM_PERIOD_SIZE), &t);
	if (err < 0)
		return err;
	snd_interval_set_minmax(&t, 2, 64);
	err = snd_interval_refine(hw_param_interval(params, SND_PCM_HW_PARAM_PERIODS), &t);
	if (err < 0)
		return err;

	return 0;
}

static int snd_pcm_jack_hw_params(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_hw_params_t * params ATTRIBUTE_UNUSED)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_hw_params\n"); fflush(stdout);
#endif
	return 0;
}

static int snd_pcm_jack_hw_free(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_hw_free\n"); fflush(stdout);
#endif
	return 0;
}

static int snd_pcm_jack_sw_params(snd_pcm_t *pcm ATTRIBUTE_UNUSED, snd_pcm_sw_params_t * params ATTRIBUTE_UNUSED)
{
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_sw_params\n"); fflush(stdout);
#endif
	return 0;
}

static int snd_pcm_jack_mmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	// snd_pcm_jack_t *jack = pcm->private_data;
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_mmap\n"); fflush(stdout);
#endif

	return 0;
}

static int snd_pcm_jack_munmap(snd_pcm_t *pcm ATTRIBUTE_UNUSED)
{
	// snd_pcm_jack_t *jack = pcm->private_data;
	return 0;
}

static void snd_pcm_jack_dump(snd_pcm_t *pcm, snd_output_t *out)
{
	// snd_pcm_jack_t *jack = pcm->private_data;
	snd_output_printf(out, "Jack PCM\n");
	if (pcm->setup) {
		snd_output_printf(out, "Its setup is:\n");
		snd_pcm_dump_setup(pcm, out);
	}
}

static snd_pcm_ops_t snd_pcm_jack_ops = {
	.close = snd_pcm_jack_close,
	.info = snd_pcm_jack_info,
	.hw_refine = snd_pcm_jack_hw_refine,
	.hw_params = snd_pcm_jack_hw_params,
	.hw_free = snd_pcm_jack_hw_free,
	.sw_params = snd_pcm_jack_sw_params,
	.channel_info = snd_pcm_jack_channel_info,
	.dump = snd_pcm_jack_dump,
	.nonblock = snd_pcm_jack_nonblock,
	.async = snd_pcm_jack_async,
	.poll_revents = snd_pcm_jack_poll_revents,
	.mmap = snd_pcm_jack_mmap,
	.munmap = snd_pcm_jack_munmap,
};

static snd_pcm_fast_ops_t snd_pcm_jack_fast_ops = {
	.status = snd_pcm_jack_status,
	.state = snd_pcm_jack_state,
	.hwsync = snd_pcm_jack_hwsync,
	.delay = snd_pcm_jack_delay,
	.prepare = snd_pcm_jack_prepare,
	.reset = snd_pcm_jack_reset,
	.start = snd_pcm_jack_start,
	.drop = snd_pcm_jack_drop,
	.drain = snd_pcm_jack_drain,
	.pause = snd_pcm_jack_pause,
	.rewind = snd_pcm_jack_rewind,
	.forward = snd_pcm_jack_forward,
	.resume = snd_pcm_jack_resume,
	.writei = snd_pcm_mmap_writei,
	.writen = snd_pcm_mmap_writen,
	.readi = snd_pcm_mmap_readi,
	.readn = snd_pcm_mmap_readn,
	.avail_update = snd_pcm_jack_avail_update,
	.mmap_commit = snd_pcm_jack_mmap_commit,
};

static int parse_ports(snd_config_t *conf,char*** ret_ports,int *ret_n)
{
	snd_config_iterator_t i, next;
	char** ports = NULL;
	unsigned int cnt = 0;
	unsigned int channel;

	if (conf) {
		snd_config_for_each(i, next, conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			if (snd_config_get_id(n, &id) < 0)
				continue;
			cnt++;
		}
		ports = calloc(cnt,sizeof(char*));
		for (channel = 0; channel < cnt; channel++)
			ports[channel] = NULL;
		
		snd_config_for_each(i, next, conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id;
			const char *port;

			if (snd_config_get_id(n, &id) < 0)
				continue;

			channel = atoi(id);

			if (snd_config_get_string(n, &port) < 0)
				continue;

			ports[channel] = port ? strdup(port) : NULL;
		}
	}
	*ret_ports = ports;
	*ret_n = cnt;
	return 0;
}

/**
 * \brief Creates a new jack PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param stream Stream type
 * \param mode Stream mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int snd_pcm_jack_open(snd_pcm_t **pcmp, const char *name,
	snd_config_t *playback_conf,
	snd_config_t *capture_conf,
	snd_pcm_stream_t stream, int mode)
{
	snd_pcm_t *pcm;
	snd_pcm_jack_t *jack;
	int err;
	int fd[2];
	
	assert(pcmp);
#ifdef PCM_JACK_DEBUG
	printf("snd_pcm_jack_open\n"); fflush(stdout);
#endif
	jack = calloc(1, sizeof(snd_pcm_jack_t));
	if (!jack) {
		return -ENOMEM;
	}

	jack->playback_ports = NULL;
	jack->playback_ports_n = 0;

	jack->capture_ports = NULL;
	jack->capture_ports_n = 0;
	
	err = parse_ports(playback_conf, &jack->playback_ports,&jack->playback_ports_n);
	if (err)
		goto _free;

	err = parse_ports(capture_conf, &jack->capture_ports,&jack->capture_ports_n);
	if (err)
		goto _free;

	if (stream == SND_PCM_STREAM_PLAYBACK) {
		jack->channels = jack->playback_ports_n;
		jack->client = jack_client_new("alsaP");
	}
	else {
		jack->channels = jack->capture_ports_n;
		jack->client = jack_client_new("alsaC");
	}

	if (jack->channels == 0) {
		SNDERR("define the %s_ports section\n", stream == SND_PCM_STREAM_PLAYBACK ? "playback" : "capture");
		goto _free;
	}

	if (jack->client==0) {
		err = -ENOENT;	
		goto _free;
	}
	
	jack->state = SND_PCM_STATE_OPEN;

	err = snd_pcm_new(&pcm, SND_PCM_TYPE_JACK, name, stream, mode);
	
	if (err < 0) {
		goto _free;
	}

	pcm->ops = &snd_pcm_jack_ops;
	pcm->fast_ops = &snd_pcm_jack_fast_ops;
	pcm->private_data = jack;

	pcm->mmap_rw = 1;

	socketpair(AF_LOCAL, SOCK_STREAM, 0, fd);
	
	jack->fd = fd[0];
	pcm->poll_fd = fd[1];
	pcm->poll_events = POLLIN;

	jack->areas = calloc(jack->channels,sizeof(snd_pcm_channel_area_t));

	snd_pcm_set_hw_ptr(pcm, &jack->hw_ptr, -1, 0);
	snd_pcm_set_appl_ptr(pcm, &jack->appl_ptr, -1, 0);
	*pcmp = pcm;

	return 0;

_free:
	if (jack) {
		if (jack->client)
			jack_client_close(jack->client);
		if (jack->playback_ports)
		{
			unsigned int k;
			for (k = 0; k < jack->playback_ports_n; k++)
				if (jack->playback_ports[k])
					free(jack->playback_ports[k]);
			free(jack->playback_ports);
		}
		if (jack->capture_ports)
		{
			unsigned int k;
			for (k = 0; k < jack->capture_ports_n; k++)
				if (jack->capture_ports[k])
					free(jack->capture_ports[k]);
			free(jack->capture_ports);
		}
		free(jack);
	}
	return err;
}

/*! \page pcm_plugins

\section pcm_plugins_jack Plugin: Jack

*/

/**
 * \brief Creates a new Jack PCM
 * \param pcmp Returns created PCM handle
 * \param name Name of PCM
 * \param root Root configuration node
 * \param conf Configuration node with Jack PCM description
 * \param stream Stream type
 * \param mode Stream mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_pcm_jack_open(snd_pcm_t **pcmp, const char *name,
		       snd_config_t *root ATTRIBUTE_UNUSED, snd_config_t *conf, 
		       snd_pcm_stream_t stream, int mode)
{
	snd_config_iterator_t i, next;
	snd_config_t *playback_conf = NULL;
	snd_config_t *capture_conf = NULL;
	int err;
	
	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (snd_pcm_conf_generic_id(id))
			continue;
		if (strcmp(id, "playback_ports") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			playback_conf = n;
			continue;
		}
		if (strcmp(id, "capture_ports") == 0) {
			if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for %s", id);
				return -EINVAL;
			}
			capture_conf = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}

	err = snd_pcm_jack_open(pcmp, name, 
		playback_conf,
		capture_conf,
		stream, mode);
		
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_pcm_jack_open, SND_PCM_DLSYM_VERSION);
#endif
