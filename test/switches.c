#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include "../include/asoundlib.h"

const char *get_type(unsigned int type)
{
	switch (type) {
	case SND_SW_TYPE_BOOLEAN:
		return "Boolean";
	case SND_SW_TYPE_BYTE:
		return "Byte";
	case SND_SW_TYPE_WORD:
		return "Word";
	case SND_SW_TYPE_DWORD:
		return "Double Word";
	case SND_SW_TYPE_LIST:
		return "List";
	case SND_SW_TYPE_LIST_ITEM:
		return "List Item";
	case SND_SW_TYPE_USER:
		return "User";
	default:
		return "Unknown";
	}
}

const char *get_interface(int iface)
{
	switch (iface) {
	case SND_CTL_IFACE_CONTROL:
		return "control";
	case SND_CTL_IFACE_MIXER:
		return "mixer";
	case SND_CTL_IFACE_PCM:
		return "pcm";
	case SND_CTL_IFACE_RAWMIDI:
		return "rawmidi";
	default:
		return "unknown";
	}
}

void print_switch(snd_ctl_t *ctl_handle, char *space, char *prefix, snd_switch_t *sw)
{
	snd_switch_t sw1;
	int low, err;

	printf("%s%s : '%s' [%s]\n", space, prefix, sw->name, get_type(sw->type));
	if (sw->type == SND_SW_TYPE_LIST) {
		for (low = sw->low; low <= sw->high; low++) {
			memcpy(&sw1, sw,  sizeof(sw1));
			sw1.type = SND_SW_TYPE_LIST_ITEM;
			sw1.low = sw1.high = low;
			if ((err = snd_ctl_switch_read(ctl_handle, &sw1)) < 0) {
				printf("Switch list item read failed for %s interface and device %i stream %i: %s\n", get_interface(sw->iface), sw->device, sw->stream, snd_strerror(err));
				continue;
			}
			printf("  %s%s : '%s' [%s] {%s}\n", space, prefix, sw1.name, get_type(sw1.type), sw1.value.item);
		}
	}
}

void process(snd_ctl_t *ctl_handle, char *space, char *prefix, int iface, int device, int stream)
{
	snd_switch_list_t list;
	snd_switch_t sw;
	int err, idx;

	bzero(&list, sizeof(list));
	list.iface = iface;
	list.device = device;
	list.stream = stream;
	if ((err = snd_ctl_switch_list(ctl_handle, &list)) < 0) {
		printf("Switch listing failed for the %s interface and the device %i: %s\n", get_interface(iface), device, snd_strerror(err));
		return;
	}
	if (list.switches_over <= 0)
		return;
	list.switches_size = list.switches_over + 16;
	list.switches = list.switches_over = 0;
	list.pswitches = malloc(sizeof(snd_switch_list_item_t) * list.switches_size);
	if (!list.pswitches) {
		printf("No enough memory... (%i switches)\n", list.switches_size);
		return;
	}
	if ((err = snd_ctl_switch_list(ctl_handle, &list)) < 0) {
		printf("Second switch listing failed for the %s interface and the device %i: %s\n", get_interface(iface), device, snd_strerror(err));
		return;
	}
	for (idx = 0; idx < list.switches; idx++) {
		bzero(&sw, sizeof(sw));
		sw.iface = iface;
		sw.device = device;
		sw.stream = stream;
		strncpy(sw.name, list.pswitches[idx].name, sizeof(sw.name));
		if ((err = snd_ctl_switch_read(ctl_handle, &sw)) < 0) {
			printf("Switch read failed for the %s interface and the device %i stream %i: %s\n", get_interface(iface), device, stream, snd_strerror(err));
			continue;
		}
		print_switch(ctl_handle, space, prefix, &sw);
	}
	free(list.pswitches);
}

int main(void)
{
	snd_ctl_t *ctl_handle;
	int cards, card, err, idx;
	snd_ctl_hw_info_t info;

	cards = snd_cards();
	printf("Detected %i soundcard%s...\n", cards, cards > 1 ? "s" : "");
	if (cards <= 0) {
		printf("Giving up...\n");
		return 0;
	}
	/* control interface */
	for (card = 0; card < cards; card++) {
		if ((err = snd_ctl_open(&ctl_handle, card)) < 0) {
			printf("CTL open error: %s\n", snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_hw_info(ctl_handle, &info)) < 0) {
			printf("HWINFO error: %s\n", snd_strerror(err));
			continue;
		}
		printf("CARD %i:\n", card);
		process(ctl_handle, "  ", "Control", SND_CTL_IFACE_CONTROL, 0, 0);
		for (idx = 0; idx < info.mixerdevs; idx++)
			process(ctl_handle, "  ", "Mixer", SND_CTL_IFACE_MIXER, idx, 0);
		for (idx = 0; idx < info.pcmdevs; idx++) {
			process(ctl_handle, "  ", "PCM playback", SND_CTL_IFACE_PCM, idx, SND_PCM_STREAM_PLAYBACK);
			process(ctl_handle, "  ", "PCM capture", SND_CTL_IFACE_PCM, idx, SND_PCM_STREAM_CAPTURE);
		}
		snd_ctl_close(ctl_handle);
	}
	return 0;
}
