#include <stdio.h>
#include <string.h>
#include "../include/asoundlib.h"

void info_channel(snd_pcm_t *handle, int channel, char *id)
{
	snd_pcm_channel_info_t chninfo;
	int err;
	
	bzero(&chninfo, sizeof(chninfo));
	chninfo.channel = channel;
	if ((err = snd_pcm_channel_info(handle, &chninfo))<0) {
		fprintf(stderr, "channel info error: %s\n", snd_strerror(err));
		return;
	}
	printf("%s INFO:\n", id);
	printf("  subdevice      : %i\n", chninfo.subdevice);
	printf("  subname        : '%s'\n", chninfo.subname);
	printf("  channel        : %i\n", chninfo.channel);
	printf("  mode           : ");
	switch (chninfo.mode) {
	case SND_PCM_MODE_STREAM:
		printf("stream\n");
		break;
	case SND_PCM_MODE_BLOCK:
		printf("block\n");
		break;
	default:
		printf("unknown\n");
	}
	printf("  sync           : 0x%x, 0x%x, 0x%x, 0x%x\n",
			chninfo.sync.id32[0],
			chninfo.sync.id32[1],
			chninfo.sync.id32[2],
			chninfo.sync.id32[3]);
	printf("  flags          :");
	if (chninfo.flags & SND_PCM_CHNINFO_MMAP)
		printf(" mmap");
	if (chninfo.flags & SND_PCM_CHNINFO_STREAM)
		printf(" stream");
	if (chninfo.flags & SND_PCM_CHNINFO_BLOCK)
		printf(" block");
	if (chninfo.flags & SND_PCM_CHNINFO_BATCH)
		printf(" batch");
	if (chninfo.flags & SND_PCM_CHNINFO_INTERLEAVE)
		printf(" interleave");
	if (chninfo.flags & SND_PCM_CHNINFO_NONINTERLEAVE)
		printf(" noninterleave");
	if (chninfo.flags & SND_PCM_CHNINFO_BLOCK_TRANSFER)
		printf(" block_transfer");
	if (chninfo.flags & SND_PCM_CHNINFO_OVERRANGE)
		printf(" overrange");
	printf("\n");
	printf("  formats        :");
	if (chninfo.formats & SND_PCM_FMT_MU_LAW)
		printf(" mu-Law");
	if (chninfo.formats & SND_PCM_FMT_A_LAW)
		printf(" a-Law");
	if (chninfo.formats & SND_PCM_FMT_IMA_ADPCM)
		printf(" IMA-ADPCM");
	if (chninfo.formats & SND_PCM_FMT_U8)
		printf(" U8");
	if (chninfo.formats & SND_PCM_FMT_S16_LE)
		printf(" S16-LE");
	if (chninfo.formats & SND_PCM_FMT_S16_BE)
		printf(" S16-BE");
	if (chninfo.formats & SND_PCM_FMT_S8)
		printf(" S8");
	if (chninfo.formats & SND_PCM_FMT_U16_LE)
		printf(" U16-LE");
	if (chninfo.formats & SND_PCM_FMT_U16_BE)
		printf(" U16-BE");
	if (chninfo.formats & SND_PCM_FMT_MPEG)
		printf(" MPEG");
	if (chninfo.formats & SND_PCM_FMT_GSM)
		printf(" GSM");
	if (chninfo.formats & SND_PCM_FMT_S24_LE)
		printf(" S24-LE");
	if (chninfo.formats & SND_PCM_FMT_S24_BE)
		printf(" S24-BE");
	if (chninfo.formats & SND_PCM_FMT_U24_LE)
		printf(" U24-LE");
	if (chninfo.formats & SND_PCM_FMT_U24_BE)
		printf(" U24-BE");
	if (chninfo.formats & SND_PCM_FMT_S32_LE)
		printf(" S32-LE");
	if (chninfo.formats & SND_PCM_FMT_S32_BE)
		printf(" S32-BE");
	if (chninfo.formats & SND_PCM_FMT_U32_LE)
		printf(" U32-LE");
	if (chninfo.formats & SND_PCM_FMT_U32_BE)
		printf(" U32-BE");
	if (chninfo.formats & SND_PCM_FMT_FLOAT)
		printf(" Float");
	if (chninfo.formats & SND_PCM_FMT_FLOAT64)
		printf(" Float64");
	if (chninfo.formats & SND_PCM_FMT_IEC958_SUBFRAME_LE)
		printf(" IEC958-LE");
	if (chninfo.formats & SND_PCM_FMT_IEC958_SUBFRAME_BE)
		printf(" IEC958-BE");
	if (chninfo.formats & SND_PCM_FMT_SPECIAL)
		printf(" Special");
	printf("\n");
	printf("  rates          :");
	if (chninfo.rates & SND_PCM_RATE_CONTINUOUS)
		printf(" Continuous");
	if (chninfo.rates & SND_PCM_RATE_KNOT)
		printf(" Knot");
	if (chninfo.rates & SND_PCM_RATE_8000)
		printf(" 8000");
	if (chninfo.rates & SND_PCM_RATE_11025)
		printf(" 11025");
	if (chninfo.rates & SND_PCM_RATE_16000)
		printf(" 16000");
	if (chninfo.rates & SND_PCM_RATE_22050)
		printf(" 22050");
	if (chninfo.rates & SND_PCM_RATE_32000)
		printf(" 32000");
	if (chninfo.rates & SND_PCM_RATE_44100)
		printf(" 44100");
	if (chninfo.rates & SND_PCM_RATE_48000)
		printf(" 48000");
	if (chninfo.rates & SND_PCM_RATE_88200)
		printf(" 88200");
	if (chninfo.rates & SND_PCM_RATE_96000)
		printf(" 96000");
	if (chninfo.rates & SND_PCM_RATE_176400)
		printf(" 176400");
	if (chninfo.rates & SND_PCM_RATE_192000)
		printf(" 192000");
	printf("\n");
	printf("  min_rate       : %i\n", chninfo.min_rate);
	printf("  max_rate       : %i\n", chninfo.max_rate);
	printf("  min_voices     : %i\n", chninfo.min_voices);
	printf("  max_voices     : %i\n", chninfo.max_voices);
	printf("  buffer_size    : %i\n", chninfo.buffer_size);
	printf("  min_frag_size  : %i\n", chninfo.min_fragment_size);
	printf("  max_frag_size  : %i\n", chninfo.max_fragment_size);
	printf("  fragment_align : %i\n", chninfo.fragment_align);
	printf("  fifo_size      : %i\n", chninfo.fifo_size);
	printf("  TBS            : %i\n", chninfo.transfer_block_size);
	printf("  mmap_size      : %li\n", chninfo.mmap_size);
	printf("  mixer_device   : %i\n", chninfo.mixer_device);
	printf("  mixer_eid      : '%s',%i,%i\n", chninfo.mixer_eid.name, chninfo.mixer_eid.index, chninfo.mixer_eid.type);
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
		info_channel(handle, SND_PCM_CHANNEL_PLAYBACK, "Playback");
	if (info.flags & SND_PCM_INFO_CAPTURE)
		info_channel(handle, SND_PCM_CHANNEL_CAPTURE, "Capture");
	snd_pcm_close(handle);
}

int main(void )
{
	info();
	return 0;
}
