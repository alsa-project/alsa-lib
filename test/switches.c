#include <stdio.h>
#include <string.h>
#include "../include/asoundlib.h"

const char *get_type(unsigned int type)
{
	switch (type) {
	case 0:
		return "Boolean";
	case 1:
		return "Byte";
	case 2:
		return "Word";
	case 3:
		return "DWord";
	case 4:
		return "User";
	default:
		return "Unknown";
	}
}

void main(void)
{
	int cards, card, device, direction, idx, count, err;
	void *handle, *chandle;
	struct snd_ctl_hw_info info;
	struct snd_ctl_switch ctl_switch;
	snd_mixer_switch_t mixer_switch;
	snd_pcm_info_t pcm_info;
	snd_pcm_switch_t pcm_switch;
	snd_rawmidi_switch_t rmidi_switch;
	snd_rawmidi_info_t rmidi_info;

	cards = snd_cards();
	printf("Detected %i soundcard%s...\n", cards, cards > 1 ? "s" : "");
	if (cards <= 0) {
		printf("Giving up...\n");
		return;
	}
	/* control interface */
	for (card = 0; card < cards; card++) {
		if ((err = snd_ctl_open(&handle, card)) < 0) {
			printf("CTL open error: %s\n", snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
			printf("CTL hw info error: %s\n", snd_strerror(err));
			continue;
		}
		if ((count = snd_ctl_switches(handle)) < 0) {
			printf("CTL switches error: %s\n", snd_strerror(count));
			continue;
		}
		for (idx = 0; idx < count; idx++) {
			if ((err = snd_ctl_switch_read(handle, idx, &ctl_switch)) < 0) {
				printf("CTL switch read error: %s\n", snd_strerror(count));
				continue;
			}
			printf("CTL switch: '%s' %s (%i-%i)\n", ctl_switch.name, get_type(ctl_switch.type), ctl_switch.low, ctl_switch.high);
			if ((err = snd_ctl_switch_write(handle, idx, &ctl_switch)) < 0) {
				printf("CTL switch write error: %s\n", snd_strerror(count));
				continue;
			}
		}
		if (count <= 0)
			printf("No CTL switches detected for soundcard #%i '%s'...\n", idx, info.name);
		snd_ctl_close(handle);
	}

	/* mixer interface */
	for (card = 0; card < cards; card++) {
		if ((err = snd_ctl_open(&handle, card)) < 0) {
			printf("CTL open error: %s\n", snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_hw_info(handle, &info)) < 0) {
			printf("CTL hw info error: %s\n", snd_strerror(err));
			continue;
		}
		snd_ctl_close(handle);
		for (device = 0; device < info.mixerdevs; device++) {
			if ((err = snd_mixer_open(&handle, card, device)) < 0) {
				printf("Mixer open error: %s\n", snd_strerror(err));
				continue;
			}
			if ((count = snd_mixer_switches(handle)) < 0) {
				printf("Mixer switches error: %s\n", snd_strerror(count));
				continue;
			}
			for (idx = 0; idx < count; idx++) {
				if ((err = snd_mixer_switch_read(handle, idx, &mixer_switch)) < 0) {
					printf("Mixer switch read error: %s\n", snd_strerror(count));
					continue;
				}
				printf("Mixer switch: '%s' %s (%i-%i)\n", mixer_switch.name, get_type(mixer_switch.type), mixer_switch.low, mixer_switch.high);
				if ((err = snd_mixer_switch_write(handle, idx, &mixer_switch)) < 0) {
					printf("Mixer switch write error: %s\n", snd_strerror(count));
					continue;
				}
			}
			if (count <= 0)
				printf("No mixer switches detected for soundcard #%i '%s'...\n", idx, info.name);
			snd_mixer_close(handle);
		}
	}

	/* pcm switches */
	for (card = 0; card < cards; card++) {
		if ((err = snd_ctl_open(&chandle, card)) < 0) {
			printf("CTL open error: %s\n", snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_hw_info(chandle, &info)) < 0) {
			printf("CTL hw info error: %s\n", snd_strerror(err));
			continue;
		}
		for (device = 0; device < info.pcmdevs; device++) {
			if ((err = snd_ctl_pcm_info(chandle, device, &pcm_info)) < 0) {
				printf("CTL PCM info error: %s\n", snd_strerror(err));
				continue;
			}
			for (direction = 0; direction < 2; direction++) {
				int (*switches) (void *handle);
				int (*switch_read) (void *handle, int switchn, snd_pcm_switch_t * data);
				int (*switch_write) (void *handle, int switchn, snd_pcm_switch_t * data);
				char *str;

				if (!(pcm_info.flags & (!direction ? SND_PCM_INFO_PLAYBACK : SND_PCM_INFO_RECORD)))
					continue;
				if ((err = snd_pcm_open(&handle, card, device, !direction ? SND_PCM_OPEN_PLAYBACK : SND_PCM_OPEN_RECORD)) < 0) {
					printf("PCM open error: %s\n", snd_strerror(err));
					continue;
				}
				if (!direction) {
					switches = snd_pcm_playback_switches;
					switch_read = snd_pcm_playback_switch_read;
					switch_write = snd_pcm_playback_switch_write;
					str = "playback";
				} else {
					switches = snd_pcm_record_switches;
					switch_read = snd_pcm_record_switch_read;
					switch_write = snd_pcm_record_switch_write;
					str = "record";
				}
				if ((count = switches(handle)) < 0) {
					printf("PCM %s switches error: %s\n", str, snd_strerror(count));
					continue;
				}
				for (idx = 0; idx < count; idx++) {
					if ((err = switch_read(handle, idx, &pcm_switch)) < 0) {
						printf("PCM %s switch read error: %s\n", str, snd_strerror(count));
						continue;
					}
					printf("PCM switch: '%s' %s (%i-%i)\n", pcm_switch.name, get_type(pcm_switch.type), pcm_switch.low, pcm_switch.high);
					if ((err = switch_write(handle, idx, &pcm_switch)) < 0) {
						printf("PCM %s switch write error: %s\n", str, snd_strerror(count));
						continue;
					}
				}
				if (count <= 0)
					printf("No PCM %s switches detected for soundcard #%i/#%i '%s'/'%s'...\n", str, idx, device, info.name, pcm_info.name);
				snd_pcm_close(handle);
			}
		}
		snd_ctl_close(chandle);
	}

	/* rawmidi switches */
	for (card = 0; card < cards; card++) {
		if ((err = snd_ctl_open(&chandle, card)) < 0) {
			printf("CTL open error: %s\n", snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_hw_info(chandle, &info)) < 0) {
			printf("CTL hw info error: %s\n", snd_strerror(err));
			continue;
		}
		for (device = 0; device < info.mididevs; device++) {
			if ((err = snd_ctl_rawmidi_info(chandle, device, &rmidi_info)) < 0) {
				printf("CTL RawMIDI info error: %s\n", snd_strerror(err));
				continue;
			}
			for (direction = 0; direction < 2; direction++) {
				int (*switches) (void *handle);
				int (*switch_read) (void *handle, int switchn, snd_rawmidi_switch_t * data);
				int (*switch_write) (void *handle, int switchn, snd_rawmidi_switch_t * data);
				char *str;

				if (!(pcm_info.flags & (!direction ? SND_RAWMIDI_INFO_OUTPUT : SND_RAWMIDI_INFO_INPUT)))
					continue;
				if ((err = snd_rawmidi_open(&handle, card, device, !direction ? SND_RAWMIDI_OPEN_OUTPUT : SND_RAWMIDI_OPEN_INPUT)) < 0) {
					printf("RawMIDI CTL open error: %s\n", snd_strerror(err));
					continue;
				}
				if (!direction) {
					switches = snd_rawmidi_output_switches;
					switch_read = snd_rawmidi_output_switch_read;
					switch_write = snd_rawmidi_output_switch_write;
					str = "output";
				} else {
					switches = snd_rawmidi_input_switches;
					switch_read = snd_rawmidi_input_switch_read;
					switch_write = snd_rawmidi_input_switch_write;
					str = "input";
				}
				if ((count = switches(handle)) < 0) {
					printf("RawMIDI %s switches error: %s\n", str, snd_strerror(count));
					continue;
				}
				for (idx = 0; idx < count; idx++) {
					if ((err = switch_read(handle, idx, &rmidi_switch)) < 0) {
						printf("RawMIDI %s switch read error: %s\n", str, snd_strerror(count));
						continue;
					}
					printf("RawMIDI switch: '%s' %s (%i-%i)\n", rmidi_switch.name, get_type(rmidi_switch.type), rmidi_switch.low, rmidi_switch.high);
					if ((err = switch_write(handle, idx, &rmidi_switch)) < 0) {
						printf("RawMIDI %s switch write error: %s\n", str, snd_strerror(count));
						continue;
					}
				}
				if (count <= 0)
					printf("No RawMIDI %s switches detected for soundcard #%i/#%i '%s'/'%s'...\n", str, idx, device, info.name, rmidi_info.name);
				snd_rawmidi_close(handle);
			}
		}
		snd_ctl_close(chandle);
	}

}
