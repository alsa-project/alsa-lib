/* SPDX-License-Identifier: LGPL-2.1+ */
#include "rawmidi.h"
#include "ump.h"
#include "ump_msg.h"

#ifndef DOC_HIDDEN
struct _snd_ump {
	snd_rawmidi_t *rawmidi;
	unsigned int flags;
	int is_input;
};
#endif /* DOC_HIDDEN */
