/*
 * optimized mixing code for x86-64
 */

#define MIX_AREAS1 mix_areas1
#define MIX_AREAS2 mix_areas2
#define MIX_AREAS3 mix_areas3
#define LOCK_PREFIX ""
#include "pcm_dmix_x86_64.h"
#undef MIX_AREAS1
#undef MIX_AREAS2
#undef MIX_AREAS3
#undef LOCK_PREFIX

#define MIX_AREAS1 mix_areas1_smp
#define MIX_AREAS2 mix_areas2_smp
#define MIX_AREAS3 mix_areas3_smp
#define LOCK_PREFIX "lock ; "
#include "pcm_dmix_x86_64.h"
#undef MIX_AREAS1
#undef MIX_AREAS2
#undef MIX_AREAS3
#undef LOCK_PREFIX
 
static unsigned long long dmix_supported_format =
	(1ULL << SND_PCM_FORMAT_S16_LE) |
	(1ULL << SND_PCM_FORMAT_S32_LE) |
	(1ULL << SND_PCM_FORMAT_S24_3LE);

static void mix_select_callbacks(snd_pcm_direct_t *dmix)
{
	FILE *in;
	char line[255];
	int smp = 0;
	
	/* try to determine, if we have SMP */
	in = fopen("/proc/cpuinfo", "r");
	if (in) {
		while (!feof(in)) {
			fgets(line, sizeof(line), in);
			if (!strncmp(line, "processor", 9))
				smp++;
		}
		fclose(in);
	}
	// printf("SMP: %i\n", smp);
	dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp : mix_areas1;
	dmix->u.dmix.mix_areas2 = smp > 1 ? mix_areas2_smp : mix_areas2;
	dmix->u.dmix.mix_areas3 = smp > 1 ? mix_areas3_smp : mix_areas3;
}
