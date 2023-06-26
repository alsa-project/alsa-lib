#ifndef TEST_H_INCLUDED
#define TEST_H_INCLUDED

#include <stdio.h>
#include <alsa/asoundlib.h>

/* XXX this variable definition does not belong in a header file */
static int any_test_failed = 0;

#define TEST_CHECK(cond) do \
		if (!(cond)) { \
			fprintf(stderr, "%s:%d: test failed: %s\n", __FILE__, __LINE__, #cond); \
			any_test_failed = 1; \
		} \
	while (0)

#define TEST_CHECK_MSG(cond, msg) do \
		if (!(cond)) { \
			fprintf(stderr, "%s:%d: test failed: %s\n", __FILE__, __LINE__, msg); \
			any_test_failed = 1; \
		} \
	while (0)

#define ALSA_CHECK(fn) ({ \
		int err = fn; \
		if (err < 0) { \
			fprintf(stderr, "%s:%d: ALSA function call failed (%s): %s\n", \
				__FILE__, __LINE__, snd_strerror(err), #fn); \
			any_test_failed = 1; \
		} \
		err; \
	})

#define ALSA_CHECK_FAIL(fn) ({ \
		int err = fn; \
		if (err >= 0) { \
			fprintf(stderr, "%s:%d: ALSA function call succeeded, but should fail (%s): %s\n", \
				__FILE__, __LINE__, snd_strerror(err), #fn); \
			any_test_failed = 1; \
		} \
		0; \
	})

#define SUB_CALL(fn) do \
		if (fn < 0) { \
			return -1; \
		} \
	while (0)

#define ALSA_CALL(fn) SUB_CALL(ALSA_CHECK(fn))

#define ALSA_CALL_FAIL(fn) SUB_CALL(ALSA_CHECK_FAIL(fn))

#define TEST_FAILED() any_test_failed

#define TEST_CALL(fn) do { \
		fn; \
		if (TEST_FAILED()) { \
			return 1; \
		} \
	} while (0)

#endif
