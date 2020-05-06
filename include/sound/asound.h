/* workaround for building with old glibc / kernel headers */
#ifdef __linux__
#include <linux/types.h>
#else
#include <sys/types.h>
#endif
#ifndef __kernel_long_t
#define __kernel_long_t long
#endif

#include <alsa/sound/uapi/asound.h>
