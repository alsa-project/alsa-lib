#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include "../include/asoundlib.h"

static snd_ctl_t *ctl_handle;
static int sw_interface;
static int sw_device;

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

const char *get_interface(void)
{
	switch (sw_interface) {
	case 0:
		return "control";
	case 1:
		return "mixer";
	case 2:
		return "PCM playback";
	case 3:
		return "PCM record";
	case 4:
		return "rawmidi output";
	case 5:
		return "rawmidi input";
	default:
		return "unknown";
	}
}

int switch_list(snd_switch_list_t *list)
{
	switch (sw_interface) {
	case 0:
		return snd_ctl_switch_list(ctl_handle, list);
	case 1:
		return snd_ctl_mixer_switch_list(ctl_handle, sw_device, list);
	case 2:
		return snd_ctl_pcm_playback_switch_list(ctl_handle, sw_device, list);
	case 3:
		return snd_ctl_pcm_record_switch_list(ctl_handle, sw_device, list);
	case 4:
		return snd_ctl_rawmidi_output_switch_list(ctl_handle, sw_device, list);
	case 5:
		return snd_ctl_rawmidi_input_switch_list(ctl_handle, sw_device, list);
	default:
		return -ENOLINK;
	}
}

int switch_read(snd_switch_t *sw)
{
	switch (sw_interface) {
	case 0:
		return snd_ctl_switch_read(ctl_handle, sw);
	case 1:
		return snd_ctl_mixer_switch_read(ctl_handle, sw_device, sw);
	case 2:
		return snd_ctl_pcm_playback_switch_read(ctl_handle, sw_device, sw);
	case 3:
		return snd_ctl_pcm_record_switch_write(ctl_handle, sw_device, sw);
	case 4:
		return snd_ctl_rawmidi_output_switch_read(ctl_handle, sw_device, sw);
	case 5:
		return snd_ctl_rawmidi_input_switch_read(ctl_handle, sw_device, sw);
	default:
		return -ENOLINK;
	}
}

int switch_write(snd_switch_t *sw)
{
	switch (sw_interface) {
	case 0:
		return snd_ctl_switch_write(ctl_handle, sw);
	case 1:
		return snd_ctl_mixer_switch_write(ctl_handle, sw_device, sw);
	case 2:
		return snd_ctl_pcm_playback_switch_write(ctl_handle, sw_device, sw);
	case 3:
		return snd_ctl_pcm_record_switch_write(ctl_handle, sw_device, sw);
	case 4:
		return snd_ctl_rawmidi_output_switch_write(ctl_handle, sw_device, sw);
	case 5:
		return snd_ctl_rawmidi_input_switch_write(ctl_handle, sw_device, sw);
	default:
		return -ENOLINK;
	}
} 

void print_switch(char *space, char *prefix, snd_switch_t *sw)
{
	snd_switch_t sw1;
	int low, err;

	printf("%s%s : '%s' [%s]\n", space, prefix, sw->name, get_type(sw->type));
	if (sw->type == SND_SW_TYPE_LIST) {
		for (low = sw->low; low <= sw->high; low++) {
			memcpy(&sw1, sw,  sizeof(sw1));
			sw1.type = SND_SW_TYPE_LIST_ITEM;
			sw1.low = sw1.high = low;
			if ((err = switch_read(&sw1)) < 0) {
				printf("Switch list item read failed for %s interface and device %i: %s\n", get_interface(), sw_device, snd_strerror(err));
				continue;
			}
			printf("  %s%s : '%s' [%s] {%s}\n", space, prefix, sw1.name, get_type(sw1.type), sw1.value.item);
		}
	}
}

void process(char *space, char *prefix, int interface, int device)
{
	snd_switch_list_t list;
	snd_switch_t sw;
	int err, idx;

	sw_interface = interface;
	sw_device = device;
	bzero(&list, sizeof(list));
	if ((err = switch_list(&list)) < 0) {
		printf("Switch listing failed for the %s interface and the device %i: %s\n", get_interface(), device, snd_strerror(err));
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
	if ((err = switch_list(&list)) < 0) {
		printf("Second switch listing failed for the %s interface and the device %i: %s\n", get_interface(), device, snd_strerror(err));
		return;
	}
	for (idx = 0; idx < list.switches; idx++) {
		bzero(&sw, sizeof(sw));
		strncpy(sw.name, list.pswitches[idx].name, sizeof(sw.name));
		if ((err = switch_read(&sw)) < 0) {
			printf("Switch read failed for the %s interface and the device %i: %s\n", get_interface(), device, snd_strerror(err));
			continue;
		}
		print_switch(space, prefix, &sw);
	}
	free(list.pswitches);
}

int main(void)
{
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
		process("  ", "Control", 0, 0);
		for (idx = 0; idx < info.mixerdevs; idx++)
			process("  ", "Mixer", 1, idx);
		for (idx = 0; idx < info.pcmdevs; idx++) {
			process("  ", "PCM playback", 2, idx);
			process("  ", "PCM record", 3, idx);
		}
		snd_ctl_close(ctl_handle);
	}
	return 0;
}
