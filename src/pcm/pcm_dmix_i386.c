/*
 * optimized mixing code for i386
 */

#define MIX_AREAS1 mix_areas1
#define MIX_AREAS1_MMX mix_areas1_mmx
#define MIX_AREAS2 mix_areas2
#define MIX_AREAS3 mix_areas3
#define MIX_AREAS3_CMOV mix_areas3_cmov
#define LOCK_PREFIX ""
#include "pcm_dmix_i386.h"
#undef MIX_AREAS1
#undef MIX_AREAS1_MMX
#undef MIX_AREAS2
#undef MIX_AREAS3
#undef MIX_AREAS3_CMOV
#undef LOCK_PREFIX

#define MIX_AREAS1 mix_areas1_smp
#define MIX_AREAS1_MMX mix_areas1_smp_mmx
#define MIX_AREAS2 mix_areas2_smp
#define MIX_AREAS3 mix_areas3_smp
#define MIX_AREAS3_CMOV mix_areas3_smp_cmov
#define LOCK_PREFIX "lock ; "
#include "pcm_dmix_i386.h"
#undef MIX_AREAS1
#undef MIX_AREAS1_MMX
#undef MIX_AREAS2
#undef MIX_AREAS3
#undef MIX_AREAS3_CMOV
#undef LOCK_PREFIX
 
static unsigned long long dmix_supported_format =
	(1ULL << SND_PCM_FORMAT_S16_LE) |
	(1ULL << SND_PCM_FORMAT_S32_LE) |
	(1ULL << SND_PCM_FORMAT_S24_3LE);

static void mix_select_callbacks(snd_pcm_direct_t *dmix)
{
	FILE *in;
	char line[255];
	int smp = 0, mmx = 0, cmov = 0;
	
	/* try to determine the capabilities of the CPU */
	in = fopen("/proc/cpuinfo", "r");
	if (in) {
		while (!feof(in)) {
			fgets(line, sizeof(line), in);
			if (!strncmp(line, "processor", 9))
				smp++;
			else if (!strncmp(line, "flags", 5)) {
				if (strstr(line, " mmx"))
					mmx = 1;
				if (strstr(line, " cmov"))
					cmov = 1;
			}
		}
		fclose(in);
	}
	if (mmx) {
		dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp_mmx : mix_areas1_mmx;
	} else {
		dmix->u.dmix.mix_areas1 = smp > 1 ? mix_areas1_smp : mix_areas1;
	}
	dmix->u.dmix.mix_areas2 = smp > 1 ? mix_areas2_smp : mix_areas2;
	if (cmov) {
		dmix->u.dmix.mix_areas3 = smp > 1 ? mix_areas3_smp_cmov : mix_areas3_cmov;
	} else {
		dmix->u.dmix.mix_areas3 = smp > 1 ? mix_areas3_smp: mix_areas3;
	}
}
