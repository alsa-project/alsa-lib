#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "test.h"

#define CARD_NAME "Loopback"
#define PERIOD_SIZE (16 * 1024)
#define CHANNELS 2
#define RATE 48000
#define FORMAT SND_PCM_FORMAT_S16_LE

int card_index;
static snd_pcm_t *opcm, *ipcm;
static char opcmdev[10], ipcmdev[10];

static int find_card(int *card_index) {
    int index = -1;
    ALSA_CALL(snd_card_next(&index));
    while (index >= 0) {
        char *name;
        ALSA_CALL(snd_card_get_name(index, &name) < 0);
		if (strcmp(name, CARD_NAME) == 0) {
        	free(name);
			*card_index = index;
			snprintf(opcmdev, sizeof(opcmdev), "hw:%i,0", index);
			snprintf(ipcmdev, sizeof(ipcmdev), "hw:%i,1", index);
			return 0;
		}
        free(name);
		ALSA_CALL(snd_card_next(&index) < 0);
    }

	TEST_CHECK_MSG(0, CARD_NAME" card not found");
	return 1;
}

static int setup_params(snd_pcm_t *pcm, int *bufsize) {
	snd_pcm_hw_params_t *hw;

    *bufsize = snd_pcm_format_size(FORMAT, PERIOD_SIZE) * CHANNELS;

	snd_pcm_hw_params_alloca(&hw);
	ALSA_CALL(snd_pcm_hw_params_any(pcm, hw));
	ALSA_CALL(snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED));
	ALSA_CALL(snd_pcm_hw_params_set_format(pcm, hw, FORMAT));
	ALSA_CALL(snd_pcm_hw_params_set_channels(pcm, hw, CHANNELS));
	ALSA_CALL(snd_pcm_hw_params_set_rate(pcm, hw, RATE, 0));
	ALSA_CALL(snd_pcm_hw_params_set_period_size(pcm, hw, PERIOD_SIZE, 0));
	ALSA_CALL(snd_pcm_hw_params_set_buffer_size(pcm, hw, *bufsize));
	ALSA_CALL(snd_pcm_hw_params(pcm, hw));

	return 0;
}

static int test_pass_bad_pointer_to_hw_params(void) {
	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	ALSA_CALL_FAIL(snd_pcm_hw_params(opcm, (snd_pcm_hw_params_t *) 1));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_set_hw_param_access_to_invalid_value(void) {
	snd_pcm_hw_params_t *hw;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	snd_pcm_hw_params_alloca(&hw);
	ALSA_CALL_FAIL(snd_pcm_hw_params_set_access(opcm, hw, SND_PCM_ACCESS_LAST + 1));
	ALSA_CALL_FAIL(snd_pcm_hw_params(opcm, hw));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_set_hw_param_format_to_invalid_value(void) {
	snd_pcm_hw_params_t *hw;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	snd_pcm_hw_params_alloca(&hw);
	ALSA_CALL_FAIL(snd_pcm_hw_params_set_format(opcm, hw, SND_PCM_FORMAT_LAST + 1));
	ALSA_CALL_FAIL(snd_pcm_hw_params(opcm, hw));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_set_hw_params_correctly(void) {
	int bufsize;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_free_device_when_it_is_used(void) {
    char *obuf;
	int bufsize;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	obuf = malloc(bufsize);
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));
	free(obuf);

	ALSA_CALL_FAIL(snd_pcm_hw_free(opcm));

	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_play_sound_and_capture_it(void) {
    char *obuf, *ibuf, val;
    int i, n, bufsize;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	ALSA_CALL(snd_pcm_open(&ipcm, ipcmdev, SND_PCM_STREAM_CAPTURE, 0));
	SUB_CALL(setup_params(ipcm, &bufsize));

	obuf = malloc(bufsize);
	ibuf = malloc(bufsize);

	val = rand() % 0x100 - 0x80;
	memset(obuf, val, bufsize);
	for (n = 0; n < 10; n++) {
		ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));
	}
	ALSA_CALL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE));
	for (i = 0; i < bufsize; i++) {
		TEST_CHECK(ibuf[i] == val);
		if (TEST_FAILED()) {
			break;
		}
	}
	ALSA_CALL(snd_pcm_drain(ipcm));
	ALSA_CALL_FAIL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE));

	free(ibuf);
	free(obuf);

	ALSA_CALL(snd_pcm_close(ipcm));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_play_drain(void) {
    char *obuf, val;
	int bufsize;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	obuf = malloc(bufsize);
	val = rand() % 0x100 - 0x80;
	memset(obuf, val, bufsize);

	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));
	ALSA_CALL(snd_pcm_drain(opcm));
	ALSA_CALL_FAIL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));

	free(obuf);

	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_status(void) {
    char *obuf, val;
	int bufsize;
    snd_pcm_status_t *status;
	snd_pcm_sframes_t delay1, delay2;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	obuf = malloc(bufsize);
	val = rand() % 0x100 - 0x80;
	memset(obuf, val, bufsize);
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));

    snd_pcm_status_alloca(&status);
	ALSA_CALL(snd_pcm_status(opcm, status));
	delay1 = snd_pcm_status_get_delay(status);
	ALSA_CALL(snd_pcm_delay(opcm, &delay2));
	TEST_CHECK_MSG(delay1 >= delay2 && (delay1 - delay2) < 100, "Delay values are wrong");

	ALSA_CALL(snd_pcm_drain(opcm));

	free(obuf);

	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_reset(void) {
    char *obuf, val;
	int bufsize;
	snd_pcm_sframes_t delay;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	obuf = malloc(bufsize);
	val = rand() % 0x100 - 0x80;
	memset(obuf, val, bufsize);
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));

	ALSA_CALL(snd_pcm_delay(opcm, &delay));
	TEST_CHECK(delay > 0);
	ALSA_CALL(snd_pcm_reset(opcm));
	ALSA_CALL(snd_pcm_delay(opcm, &delay));
	TEST_CHECK(delay == 0);

	free(obuf);

	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_start(void) {
	snd_pcm_sw_params_t *sw;
    char *obuf, *ibuf, val;
	int i, bufsize;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	ALSA_CALL(snd_pcm_open(&ipcm, ipcmdev, SND_PCM_STREAM_CAPTURE, 0));
	SUB_CALL(setup_params(ipcm, &bufsize));

	snd_pcm_sw_params_alloca(&sw);
	ALSA_CALL(snd_pcm_sw_params_current(opcm, sw));
	ALSA_CALL(snd_pcm_sw_params_set_start_threshold(opcm, sw, PERIOD_SIZE + 1));
	ALSA_CALL(snd_pcm_sw_params(opcm, sw));

	obuf = malloc(bufsize);
	ibuf = malloc(bufsize);

	val = rand() % 0x100 - 0x80;
	memset(obuf, val, bufsize);
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));

	ALSA_CALL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE));
	for (i = 0; i < bufsize; i++) {
		TEST_CHECK(ibuf[i] == 0);
		if (TEST_FAILED()) {
			break;
		}
	}

	ALSA_CALL(snd_pcm_start(opcm));

	ALSA_CALL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE));
	for (i = bufsize - 16; i < bufsize; i++) {
		TEST_CHECK(ibuf[i] == val);
		if (TEST_FAILED()) {
			break;
		}
	}

	free(obuf);

	ALSA_CALL(snd_pcm_close(ipcm));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_pause(void) {
    char *obuf, *ibuf, val;
	int i, n, bufsize, vals;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	ALSA_CALL(snd_pcm_open(&ipcm, ipcmdev, SND_PCM_STREAM_CAPTURE, 0));
	SUB_CALL(setup_params(ipcm, &bufsize));

	obuf = malloc(bufsize);
	ibuf = malloc(bufsize);

	val = rand() % 0x100 - 0x80;
	memset(obuf, val, bufsize);
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));
	ALSA_CALL(snd_pcm_pause(opcm, 1));

	ALSA_CALL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE));
	for (i = bufsize - 16; i < bufsize; i++) {
		TEST_CHECK(ibuf[i] == 0);
		if (TEST_FAILED()) {
			break;
		}
	}

	ALSA_CALL(snd_pcm_pause(opcm, 0));
	sleep(1);

	vals = 0;
	for (n = 0; n < 3; n++) {
		ALSA_CALL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE));
		for (i = 0; i < bufsize; i++) {
			if (ibuf[i] == val) {
				vals++;
			}
		}
	}
	TEST_CHECK(vals > 256);

	free(obuf);

	ALSA_CALL(snd_pcm_close(ipcm));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_rewind(void) {
	char *obuf, *ibuf;
	int i, bufsize, shift;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	ALSA_CALL(snd_pcm_open(&ipcm, ipcmdev, SND_PCM_STREAM_CAPTURE, 0));
	SUB_CALL(setup_params(ipcm, &bufsize));

	obuf = malloc(bufsize);
	ibuf = malloc(bufsize);

	for (i = 0; i < bufsize; i++) {
		obuf[i] = i % 128;
	}
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));

	ALSA_CALL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE / 2));
	ALSA_CALL(snd_pcm_rewind(ipcm, 16));
	ALSA_CALL(snd_pcm_readi(ipcm, ibuf + (bufsize / 2), PERIOD_SIZE / 2));
	shift = snd_pcm_format_size(FORMAT, 1) * CHANNELS * 16;
	TEST_CHECK(ibuf[bufsize / 2 - 1] - ibuf[bufsize / 2] == shift - 1);

	free(obuf);

	ALSA_CALL(snd_pcm_close(ipcm));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_forward(void) {
	char *obuf, *ibuf;
	int i, bufsize, shift;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	ALSA_CALL(snd_pcm_open(&ipcm, ipcmdev, SND_PCM_STREAM_CAPTURE, 0));
	SUB_CALL(setup_params(ipcm, &bufsize));

	obuf = malloc(bufsize);
	ibuf = malloc(bufsize);

	for (i = 0; i < bufsize; i++) {
		obuf[i] = i % 128;
	}
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));

	ALSA_CALL(snd_pcm_readi(ipcm, ibuf, PERIOD_SIZE / 2));
	ALSA_CALL(snd_pcm_forward(ipcm, 1));
	ALSA_CALL(snd_pcm_readi(ipcm, ibuf + (bufsize / 2), PERIOD_SIZE / 2));
	shift = snd_pcm_format_size(FORMAT, 1) * CHANNELS;
	TEST_CHECK(ibuf[bufsize / 2] == shift);
	for (i = bufsize - shift; i < bufsize; i++) {
		TEST_CHECK(ibuf[i] == 0);
	}

	free(obuf);

	ALSA_CALL(snd_pcm_close(ipcm));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

static int test_link(void) {
    char *obuf, val;
	int bufsize;
    snd_pcm_status_t *status;

	ALSA_CALL(snd_pcm_open(&opcm, opcmdev, SND_PCM_STREAM_PLAYBACK, 0));
	SUB_CALL(setup_params(opcm, &bufsize));

	ALSA_CALL(snd_pcm_open(&ipcm, ipcmdev, SND_PCM_STREAM_CAPTURE, 0));
	SUB_CALL(setup_params(ipcm, &bufsize));

	ALSA_CALL(snd_pcm_link(ipcm, opcm));

	obuf = malloc(bufsize);

	val = rand() % 0x100 - 0x80;
	memset(obuf, val, bufsize);
	ALSA_CALL(snd_pcm_writei(opcm, obuf, PERIOD_SIZE));

    snd_pcm_status_alloca(&status);

	ALSA_CALL(snd_pcm_pause(opcm, 1));
	ALSA_CALL(snd_pcm_status(ipcm, status));
	TEST_CHECK(snd_pcm_status_get_state(status) == SND_PCM_STATE_PAUSED);

	ALSA_CALL(snd_pcm_pause(opcm, 0));
	ALSA_CALL(snd_pcm_status(ipcm, status));
	TEST_CHECK(snd_pcm_status_get_state(status) == SND_PCM_STATE_RUNNING);

	ALSA_CALL(snd_pcm_unlink(ipcm));

	ALSA_CALL(snd_pcm_pause(opcm, 1));
	ALSA_CALL(snd_pcm_status(ipcm, status));
	TEST_CHECK(snd_pcm_status_get_state(status) == SND_PCM_STATE_RUNNING);

	free(obuf);

	ALSA_CALL(snd_pcm_close(ipcm));
	ALSA_CALL(snd_pcm_close(opcm));

	return 0;
}

int main(void) {
	srand(time(0));
	TEST_CALL(find_card(&card_index));
	TEST_CALL(test_pass_bad_pointer_to_hw_params());
	TEST_CALL(test_set_hw_param_access_to_invalid_value());
	TEST_CALL(test_set_hw_param_format_to_invalid_value());
	TEST_CALL(test_set_hw_params_correctly());
	TEST_CALL(test_free_device_when_it_is_used());
	TEST_CALL(test_play_sound_and_capture_it());
	TEST_CALL(test_play_drain());
	TEST_CALL(test_status());
	TEST_CALL(test_reset());
	TEST_CALL(test_start());
	TEST_CALL(test_pause());
	TEST_CALL(test_rewind());
	TEST_CALL(test_forward());
	TEST_CALL(test_link());
	return 0;
}
