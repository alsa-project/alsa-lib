// An example program to create a virtual UMP Endpoint
//
// A client simply reads each UMP packet and sends to subscribers
// while the note on/off velocity is halved

#include <stdio.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <alsa/ump_msg.h>

/* make the note on/off velocity half for MIDI1 CVM */
static void midi1_half_note_velocity(snd_seq_ump_event_t *ev)
{
	snd_ump_msg_midi1_t *midi1 = (snd_ump_msg_midi1_t *)ev->ump;

	switch (snd_ump_msg_status(ev->ump)) {
	case SND_UMP_MSG_NOTE_OFF:
	case SND_UMP_MSG_NOTE_ON:
		midi1->note_on.velocity >>= 1;
		break;
	}
}

/* make the note on/off velocity half for MIDI2 CVM */
static void midi2_half_note_velocity(snd_seq_ump_event_t *ev)
{
	snd_ump_msg_midi2_t *midi2 = (snd_ump_msg_midi2_t *)ev->ump;

	switch (snd_ump_msg_status(ev->ump)) {
	case SND_UMP_MSG_NOTE_OFF:
	case SND_UMP_MSG_NOTE_ON:
		midi2->note_on.velocity >>= 1;
		break;
	}
}

static void help(void)
{
	printf("seq-ump-example: Create a virtual UMP Endpoint and Blocks\n"
	       "\n"
	       "Usage: seq-ump-example [OPTIONS]\n"
	       "\n"
	       "-n,--num-blocks blocks		Number of blocks (groups) to create\n"
	       "-m,--midi-version version	MIDI protocol version (1 or 2)\n"
	       "-N--name			UMP Endpoint name string\n"
	       "-P,--product name		UMP Product ID string\n"
	       "-M,--manufacturer id		UMP Manufacturer ID value (24bit)\n"
	       "-F,--family id			UMP Family ID value (16bit)\n"
	       "-O,--model id			UMP Model ID value (16bit)\n"
	       "-R,--sw-revision id		UMP Software Revision ID (32bit)\n");
}

int main(int argc, char **argv)
{
	int midi_version = 2;
	int num_blocks = 1;
	const char *name = "ACMESynth";
	const char *product = "Halfmoon";
	unsigned int manufacturer = 0x123456;
	unsigned int family = 0x1234;
	unsigned int model = 0xabcd;
	unsigned int sw_revision = 0x12345678;
	snd_seq_t *seq;
	snd_ump_endpoint_info_t *ep;
	snd_ump_block_info_t *blk;
	snd_seq_ump_event_t *ev;
	int i, c, err;
	unsigned char tmp[4];

	static const struct option long_option[] = {
		{"num-blocks", required_argument, 0, 'n'},
		{"midi-version", required_argument, 0, 'm'},
		{"name", required_argument, 0, 'N'},
		{"product", required_argument, 0, 'P'},
		{"manufacturer", required_argument, 0, 'M'},
		{"family", required_argument, 0, 'F'},
		{"model", required_argument, 0, 'O'},
		{"sw-revision", required_argument, 0, 'R'},
		{0, 0, 0, 0}
	};

	while ((c = getopt_long(argc, argv, "n:m:N:P:M:F:O:R:",
				long_option, NULL)) >= 0) {
		switch (c) {
		case 'n':
			num_blocks = atoi(optarg);
			break;
		case 'm':
			midi_version = atoi(optarg);
			break;
		case 'N':
			name = optarg;
			break;
		case 'P':
			product = optarg;
			break;
		case 'M':
			manufacturer = strtol(optarg, NULL, 0);
			break;
		case 'F':
			family = strtol(optarg, NULL, 0);
			break;
		case 'O':
			model = strtol(optarg, NULL, 0);
			break;
		case 'R':
			sw_revision = strtol(optarg, NULL, 0);
			break;
		default:
			help();
			return 1;
		}
	}

	err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	if (err < 0) {
		fprintf(stderr, "failed to open sequencer: %d\n", err);
		return 1;
	}

	snd_ump_endpoint_info_alloca(&ep);
	snd_ump_endpoint_info_set_name(ep, name);
	snd_ump_endpoint_info_set_product_id(ep, product);
	if (midi_version == 1) {
		snd_ump_endpoint_info_set_protocol_caps(ep, SND_UMP_EP_INFO_PROTO_MIDI1);
		snd_ump_endpoint_info_set_protocol(ep, SND_UMP_EP_INFO_PROTO_MIDI1);
	} else {
		snd_ump_endpoint_info_set_protocol_caps(ep, SND_UMP_EP_INFO_PROTO_MIDI2);
		snd_ump_endpoint_info_set_protocol(ep, SND_UMP_EP_INFO_PROTO_MIDI2);
	}
	snd_ump_endpoint_info_set_num_blocks(ep, num_blocks);
	snd_ump_endpoint_info_set_manufacturer_id(ep, manufacturer);
	snd_ump_endpoint_info_set_family_id(ep, family);
	snd_ump_endpoint_info_set_model_id(ep, model);
	for (i = 0; i < 4; i++)
		tmp[i] = (sw_revision >> ((3 - i) * 8)) & 0xff;
	snd_ump_endpoint_info_set_sw_revision(ep, tmp);

	err = snd_seq_create_ump_endpoint(seq, ep, num_blocks);
	if (err < 0) {
		fprintf(stderr, "failed to set UMP EP info: %d\n", err);
		return 1;
	}

	snd_ump_block_info_alloca(&blk);

	for (i = 0; i < num_blocks; i++) {
		char blkname[32];

		sprintf(blkname, "Filter %d", i + 1);
		snd_ump_block_info_set_name(blk, blkname);
		snd_ump_block_info_set_direction(blk, SND_UMP_DIR_BIDIRECTION);
		snd_ump_block_info_set_first_group(blk, i);
		snd_ump_block_info_set_num_groups(blk, 1);
		snd_ump_block_info_set_ui_hint(blk, SND_UMP_BLOCK_UI_HINT_BOTH);

		err = snd_seq_create_ump_block(seq, i, blk);
		if (err < 0) {
			fprintf(stderr, "failed to set UMP block info %d: %d\n",
				i, err);
			return 1;
		}
	}

	/* halve the incoming note-on / off velocity and pass through
	 * to subscribers
	 */
	while (snd_seq_ump_event_input(seq, &ev) >= 0) {
		if (!snd_seq_ev_is_ump(ev))
			continue;
		switch (snd_ump_msg_type(ev->ump)) {
		case SND_UMP_MSG_TYPE_MIDI1_CHANNEL_VOICE:
			midi1_half_note_velocity(ev);
			break;
		case SND_UMP_MSG_TYPE_MIDI2_CHANNEL_VOICE:
			midi2_half_note_velocity(ev);
			break;
		}

		snd_seq_ev_set_subs(ev);
		snd_seq_ev_set_direct(ev);
		snd_seq_ump_event_output(seq, ev);
		snd_seq_drain_output(seq);
	}

	return 0;
}
