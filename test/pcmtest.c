#include <stdio.h>
#include <string.h>
#include "../include/asoundlib.h"

void info_channel(snd_pcm_t *handle, int channel, char *id)
{
	snd_pcm_stream_info_t stream_info;
	int err;
	
	bzero(&stream_info, sizeof(stream_info));
	stream_info.channel = channel;
	if ((err = snd_pcm_stream_info(handle, &stream_info))<0) {
		fprintf(stderr, "channel info error: %s\n", snd_strerror(err));
		return;
	}
	printf("%s INFO:\n", id);
	printf("  subdevice      : %i\n", stream_info.subdevice);
	printf("  subname        : '%s'\n", stream_info.subname);
	printf("  channel        : %i\n", stream_info.channel);
	printf("  mode           : ");
	switch (stream_info.mode) {
	case SND_PCM_MODE_FRAME:
		printf("frame\n");
		break;
	case SND_PCM_MODE_FRAGMENT:
		printf("fragment\n");
		break;
	default:
		printf("unknown\n");
	}
	printf("  sync           : 0x%x, 0x%x, 0x%x, 0x%x\n",
			stream_info.sync.id32[0],
			stream_info.sync.id32[1],
			stream_info.sync.id32[2],
			stream_info.sync.id32[3]);
	printf("  flags          :");
	if (stream_info.flags & SND_PCM_STREAM_INFO_MMAP)
		printf(" mmap");
	if (stream_info.flags & SND_PCM_STREAM_INFO_FRAME)
		printf(" frame");
	if (stream_info.flags & SND_PCM_STREAM_INFO_FRAGMENT)
		printf(" fragment");
	if (stream_info.flags & SND_PCM_STREAM_INFO_BATCH)
		printf(" batch");
	if (stream_info.flags & SND_PCM_STREAM_INFO_INTERLEAVE)
		printf(" interleave");
	if (stream_info.flags & SND_PCM_STREAM_INFO_NONINTERLEAVE)
		printf(" noninterleave");
	if (stream_info.flags & SND_PCM_STREAM_INFO_BLOCK_TRANSFER)
		printf(" block_transfer");
	if (stream_info.flags & SND_PCM_STREAM_INFO_OVERRANGE)
		printf(" overrange");
	printf("\n");
	printf("  formats        :");
	if (stream_info.formats & SND_PCM_FMT_MU_LAW)
		printf(" mu-Law");
	if (stream_info.formats & SND_PCM_FMT_A_LAW)
		printf(" a-Law");
	if (stream_info.formats & SND_PCM_FMT_IMA_ADPCM)
		printf(" IMA-ADPCM");
	if (stream_info.formats & SND_PCM_FMT_U8)
		printf(" U8");
	if (stream_info.formats & SND_PCM_FMT_S16_LE)
		printf(" S16-LE");
	if (stream_info.formats & SND_PCM_FMT_S16_BE)
		printf(" S16-BE");
	if (stream_info.formats & SND_PCM_FMT_S8)
		printf(" S8");
	if (stream_info.formats & SND_PCM_FMT_U16_LE)
		printf(" U16-LE");
	if (stream_info.formats & SND_PCM_FMT_U16_BE)
		printf(" U16-BE");
	if (stream_info.formats & SND_PCM_FMT_MPEG)
		printf(" MPEG");
	if (stream_info.formats & SND_PCM_FMT_GSM)
		printf(" GSM");
	if (stream_info.formats & SND_PCM_FMT_S24_LE)
		printf(" S24-LE");
	if (stream_info.formats & SND_PCM_FMT_S24_BE)
		printf(" S24-BE");
	if (stream_info.formats & SND_PCM_FMT_U24_LE)
		printf(" U24-LE");
	if (stream_info.formats & SND_PCM_FMT_U24_BE)
		printf(" U24-BE");
	if (stream_info.formats & SND_PCM_FMT_S32_LE)
		printf(" S32-LE");
	if (stream_info.formats & SND_PCM_FMT_S32_BE)
		printf(" S32-BE");
	if (stream_info.formats & SND_PCM_FMT_U32_LE)
		printf(" U32-LE");
	if (stream_info.formats & SND_PCM_FMT_U32_BE)
		printf(" U32-BE");
	if (stream_info.formats & SND_PCM_FMT_FLOAT)
		printf(" Float");
	if (stream_info.formats & SND_PCM_FMT_FLOAT64)
		printf(" Float64");
	if (stream_info.formats & SND_PCM_FMT_IEC958_SUBFRAME_LE)
		printf(" IEC958-LE");
	if (stream_info.formats & SND_PCM_FMT_IEC958_SUBFRAME_BE)
		printf(" IEC958-BE");
	if (stream_info.formats & SND_PCM_FMT_SPECIAL)
		printf(" Special");
	printf("\n");
	printf("  rates          :");
	if (stream_info.rates & SND_PCM_RATE_CONTINUOUS)
		printf(" Continuous");
	if (stream_info.rates & SND_PCM_RATE_KNOT)
		printf(" Knot");
	if (stream_info.rates & SND_PCM_RATE_8000)
		printf(" 8000");
	if (stream_info.rates & SND_PCM_RATE_11025)
		printf(" 11025");
	if (stream_info.rates & SND_PCM_RATE_16000)
		printf(" 16000");
	if (stream_info.rates & SND_PCM_RATE_22050)
		printf(" 22050");
	if (stream_info.rates & SND_PCM_RATE_32000)
		printf(" 32000");
	if (stream_info.rates & SND_PCM_RATE_44100)
		printf(" 44100");
	if (stream_info.rates & SND_PCM_RATE_48000)
		printf(" 48000");
	if (stream_info.rates & SND_PCM_RATE_88200)
		printf(" 88200");
	if (stream_info.rates & SND_PCM_RATE_96000)
		printf(" 96000");
	if (stream_info.rates & SND_PCM_RATE_176400)
		printf(" 176400");
	if (stream_info.rates & SND_PCM_RATE_192000)
		printf(" 192000");
	printf("\n");
	printf("  min_rate       : %i\n", stream_info.min_rate);
	printf("  max_rate       : %i\n", stream_info.max_rate);
	printf("  min_channels   : %i\n", stream_info.min_channels);
	printf("  max_channels   : %i\n", stream_info.max_channels);
	printf("  buffer_size    : %i\n", stream_info.buffer_size);
	printf("  min_frag_size  : %i\n", stream_info.min_fragment_size);
	printf("  max_frag_size  : %i\n", stream_info.max_fragment_size);
	printf("  fragment_align : %i\n", stream_info.fragment_align);
	printf("  fifo_size      : %i\n", stream_info.fifo_size);
	printf("  mmap_size      : %li\n", stream_info.mmap_size);
	printf("  mixer_device   : %i\n", stream_info.mixer_device);
	printf("  mixer_eid      : '%s',%i,%i\n", stream_info.mixer_eid.name, stream_info.mixer_eid.index, stream_info.mixer_eid.type);
}

void info(void)
{
	snd_pcm_t *handle;
	snd_pcm_info_t info;
	int err;

	if ((err = snd_pcm_open(&handle, 0, 0, SND_PCM_OPEN_DUPLEX))<0) {
		fprintf(stderr, "open error: %s\n", snd_strerror(err));
		return;
	}
	if ((err = snd_pcm_info(handle, &info))<0) {
		fprintf(stderr, "pcm info error: %s\n", snd_strerror(err));
		return;
	}
	printf("INFO:\n");
	printf("  type      : 0x%x\n", info.type);
	printf("  flags     : 0x%x\n", info.flags);
	printf("  id        : '%s'\n", info.id);
	printf("  name      : '%s'\n", info.name);
	printf("  playback  : %i\n", info.playback);
	printf("  capture   : %i\n", info.capture);
	if (info.flags & SND_PCM_INFO_PLAYBACK)
		info_channel(handle, SND_PCM_STREAM_PLAYBACK, "Playback");
	if (info.flags & SND_PCM_INFO_CAPTURE)
		info_channel(handle, SND_PCM_STREAM_CAPTURE, "Capture");
	snd_pcm_close(handle);
}

int main(void )
{
	info();
	return 0;
}
