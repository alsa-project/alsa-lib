// SPDX-License-Identifier: GPL-2.0-or-later
// Inquire UMP Endpoint and Block info in various APIs

#include <stdio.h>
#include <alsa/asoundlib.h>

enum { UNKNOWN, SEQ, RAWMIDI, CTL };
static int mode = UNKNOWN;

static snd_seq_t *seq;
static int client;
static snd_ump_t *ump;
static snd_ctl_t *ctl;

static int get_ump_endpoint_info(snd_ump_endpoint_info_t *ep)
{
	switch (mode) {
	case SEQ:
		return snd_seq_get_ump_endpoint_info(seq, client, ep);
	case RAWMIDI:
		return snd_ump_endpoint_info(ump, ep);
	case CTL:
		return snd_ctl_ump_endpoint_info(ctl, ep);
	default:
		return -EINVAL;
	}
}

static int get_ump_block_info(int blk, snd_ump_block_info_t *bp)
{
	switch (mode) {
	case SEQ:
		return snd_seq_get_ump_block_info(seq, client, blk, bp);
	case RAWMIDI:
		snd_ump_block_info_set_block_id(bp, blk);
		return snd_ump_block_info(ump, bp);
	case CTL:
		snd_ump_block_info_set_block_id(bp, blk);
		return snd_ctl_ump_block_info(ctl, bp);
	default:
		return -EINVAL;
	}
}

static void help(void)
{
	printf("umpinfo: inquire UMP Endpoint and Block info\n"
	       "  umpinfo -c target	inquiry via control API\n"
	       "  umpinfo -m target	inquiry via UMP rawmidi API\n"
	       "  umpinfo -s client	inquiry via sequencer API\n"
	       "    target = hw:0, etc\n"
	       "    client = sequencer client number\n");
	exit(1);
}

int main(int argc, char **argv)
{
	const char *target = "hw:0";
	snd_ump_endpoint_info_t *ep;
	snd_ump_block_info_t *blk;
	const unsigned char *s;
	int c, i, num_blks;

	while ((c = getopt(argc, argv, "s:m:c:h")) >= 0) {
		switch (c) {
		case 's':
			mode = SEQ;
			client = atoi(target);
			break;
		case 'm':
			mode = RAWMIDI;
			target = optarg;
			break;
		case 'c':
			mode = CTL;
			target = optarg;
			break;
		default:
			help();
			break;
		}
	}

	switch (mode) {
	case SEQ:
		if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0)) {
			fprintf(stderr, "failed to open sequencer\n");
			return 1;
		}
		break;
	case RAWMIDI:
		if (snd_ump_open(&ump, NULL, target, 0)) {
			fprintf(stderr, "failed to open UMP rawmidi\n");
			return 1;
		}
		break;
	case CTL:
		if (snd_ctl_open(&ctl, target, 0)) {
			fprintf(stderr, "failed to open control\n");
			return 1;
		}
		break;
	default:
		help();
		break;
	}

	snd_ump_endpoint_info_alloca(&ep);
	snd_ump_block_info_alloca(&blk);

	if (get_ump_endpoint_info(ep)) {
		fprintf(stderr, "failed to get UMP EP info\n");
		return 1;
	}

	printf("Name: %s\n", snd_ump_endpoint_info_get_name(ep));
	printf("Product ID: %s\n", snd_ump_endpoint_info_get_product_id(ep));
	printf("Flags: 0x%x\n", snd_ump_endpoint_info_get_flags(ep));
	printf("Protocol Caps: 0x%x\n", snd_ump_endpoint_info_get_protocol_caps(ep));
	printf("Protocol: 0x%x\n", snd_ump_endpoint_info_get_protocol(ep));
	printf("Num Blocks: %d\n", snd_ump_endpoint_info_get_num_blocks(ep));
	printf("Version: 0x%x\n", snd_ump_endpoint_info_get_version(ep));
	printf("Manufacturer ID: 0x%x\n", snd_ump_endpoint_info_get_manufacturer_id(ep));
	printf("Family ID: 0x%x\n", snd_ump_endpoint_info_get_family_id(ep));
	printf("Model ID: 0x%x\n", snd_ump_endpoint_info_get_model_id(ep));
	s = snd_ump_endpoint_info_get_sw_revision(ep);
	printf("SW Revision: %02x:%02x:%02x:%02x\n", s[0], s[1], s[2], s[3]);
	num_blks = snd_ump_endpoint_info_get_num_blocks(ep);
	for (i = 0; i < num_blks; i++) {
		if (get_ump_block_info(i, blk)) {
			fprintf(stderr, "failed to get UMP Block info for %d\n", i);
			continue;
		}
		printf("\n");
		printf("Block ID: %d\n", snd_ump_block_info_get_block_id(blk));
		printf("Name: %s\n", snd_ump_block_info_get_name(blk));
		printf("Active: %d\n", snd_ump_block_info_get_active(blk));
		printf("Flags: 0x%x\n", snd_ump_block_info_get_flags(blk));
		printf("Direction: %d\n", snd_ump_block_info_get_direction(blk));
		printf("First Group: %d\n", snd_ump_block_info_get_first_group(blk));
		printf("Num Groups: %d\n", snd_ump_block_info_get_num_groups(blk));
		printf("MIDI-CI Version: %d\n", snd_ump_block_info_get_midi_ci_version(blk));
		printf("Sysex8 Streams: %d\n", snd_ump_block_info_get_sysex8_streams(blk));
	}

	return 0;
}
