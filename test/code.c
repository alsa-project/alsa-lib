#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#define rdtscll(val) \
     __asm__ __volatile__("rdtsc" : "=A" (val))

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

typedef short int s16;
typedef int s32;

#define CONFIG_SMP

#ifdef CONFIG_SMP
#define LOCK_PREFIX "lock ; "
#else
#define LOCK_PREFIX ""
#endif

struct __xchg_dummy { unsigned long a[100]; };
#define __xg(x) ((struct __xchg_dummy *)(x))

static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
				      unsigned long new, int size)
{
	unsigned long prev;
	switch (size) {
	case 1:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 2:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	case 4:
		__asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"
				     : "=a"(prev)
				     : "q"(new), "m"(*__xg(ptr)), "0"(old)
				     : "memory");
		return prev;
	}
	return old;
}

#define cmpxchg(ptr,o,n)\
	((__typeof__(*(ptr)))__cmpxchg((ptr),(unsigned long)(o),\
					(unsigned long)(n),sizeof(*(ptr))))

static inline void atomic_add(volatile int *dst, int v)
{
	__asm__ __volatile__(
		LOCK_PREFIX "addl %1,%0"
		:"=m" (*dst)
		:"ir" (v));
}

void mix_areas0(unsigned int size,
		volatile s16 *dst, s16 *src,
		volatile s32 *sum,
		unsigned int dst_step,
		unsigned int src_step,
		unsigned int sum_step)
{
	while (size-- > 0) {
		s32 sample = *dst + *src;
		if (unlikely(sample & 0xffff0000))
			*dst = sample > 0 ? 0x7fff : -0x8000;
		else
			*dst = sample;
		((char *)dst) += dst_step;
		((char *)src) += src_step;
	}
}

void mix_areas1(unsigned int size,
		volatile s16 *dst, s16 *src,
		volatile s32 *sum, unsigned int dst_step,
		unsigned int src_step, unsigned int sum_step)
{
	/*
	 *  ESI - src
	 *  EDI - dst
	 *  EBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - size
	 */
	__asm__ __volatile__ (
		"\n"

		/*
		 *  initialization, load EDX, ESI, EDI, EBX registers
		 */
		"\tmovl %0, %%edx\n"
		"\tmovl %1, %%edi\n"
		"\tmovl %2, %%esi\n"
		"\tmovl %3, %%ebx\n"

		/*
		 * while (size-- > 0) {
		 */
		"\tcmp $0, %%edx\n"
		"jz 6f\n"

		"1:"

		/*
		 *   sample = *src;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= *sum;
		 *   xadd(*sum, sample);
		 */
		"\tmovw $0, %%ax\n"
		"\tmovw $1, %%cx\n"
		"\tlock; cmpxchgw %%cx, (%%edi)\n"
		"\tmovswl (%%esi), %%ecx\n"
		"\tjnz 2f\n"
		"\tsubl (%%ebx), %%ecx\n"
		"2:"
		"\tlock; addl %%ecx, (%%ebx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(v);
		 *     *dst = sample;
		 *   } while (v != *sum);
		 */

		"3:"
		"\tmovl (%%ebx), %%ecx\n"
		"\tcmpl $0x7fff,%%ecx\n"
		"\tjg 4f\n"
		"\tcmpl $-0x8000,%%ecx\n"
		"\tjl 5f\n"
		"\tmovw %%cx, (%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
		"\tadd %4, %%edi\n"
		"\tadd %5, %%esi\n"
		"\tadd %6, %%ebx\n"
		"\tdecl %%edx\n"
		"\tjnz 1b\n"
		"\tjmp 6f\n"

		/*
		 *  sample > 0x7fff
		 */

		"4:"
		"\tmovw $0x7fff, %%ax\n"
		"\tmovw %%ax, (%%edi)\n"
		"\tcmpl %%ecx,(%%ebx)\n"
		"\tjnz 3b\n"
		"\tadd %4, %%edi\n"
		"\tadd %5, %%esi\n"
		"\tadd %6, %%ebx\n"
		"\tdecl %%edx\n"
		"\tjnz 1b\n"
		"\tjmp 6f\n"

		/*
		 *  sample < -0x8000
		 */

		"5:"
		"\tmovw $-0x8000, %%ax\n"
		"\tmovw %%ax, (%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 3b\n"
		"\tadd %4, %%edi\n"
		"\tadd %5, %%esi\n"
		"\tadd %6, %%ebx\n"
		"\tdecl %%edx\n"
		"\tjnz 1b\n"
		// "\tjmp 6f\n"
		
		"6:"

		: /* no output regs */
		: "m" (size), "m" (dst), "m" (src), "m" (sum), "m" (dst_step), "m" (src_step), "m" (sum_step)
		: "esi", "edi", "edx", "ecx", "ebx", "eax"
	);
}


void mix_areas1_mmx(unsigned int size,
		    volatile s16 *dst, s16 *src,
		    volatile s32 *sum, unsigned int dst_step,
		    unsigned int src_step, unsigned int sum_step)
{
	/*
	 *  ESI - src
	 *  EDI - dst
	 *  EBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - size
	 */
	__asm__ __volatile__ (
		"\n"

		/*
		 *  initialization, load EDX, ESI, EDI, EBX registers
		 */
		"\tmovl %0, %%edx\n"
		"\tmovl %1, %%edi\n"
		"\tmovl %2, %%esi\n"
		"\tmovl %3, %%ebx\n"

		/*
		 * while (size-- > 0) {
		 */
		"\tcmp $0, %%edx\n"
		"jz 6f\n"

		"1:"

		/*
		 *   sample = *src;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= *sum;
		 *   xadd(*sum, sample);
		 */
		"\tmovw $0, %%ax\n"
		"\tmovw $1, %%cx\n"
		"\tlock; cmpxchgw %%cx, (%%edi)\n"
		"\tmovswl (%%esi), %%ecx\n"
		"\tjnz 2f\n"
		"\tsubl (%%ebx), %%ecx\n"
		"2:"
		"\tlock; addl %%ecx, (%%ebx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(v);
		 *     *dst = sample;
		 *   } while (v != *sum);
		 */

		"3:"
		"\tmovl (%%ebx), %%ecx\n"
		"\tmovd %%ecx, %%mm0\n"
		"\tpackssdw %%mm1, %%mm0\n"
		"\tmovd %%mm0, %%eax\n"
		"\tmovw %%ax, (%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
		"\tadd %4, %%edi\n"
		"\tadd %5, %%esi\n"
		"\tadd %6, %%ebx\n"
		"\tdecl %%edx\n"
		"\tjnz 1b\n"
		"\tjmp 6f\n"
		
		"6:"

		"\temms\n"

		: /* no output regs */
		: "m" (size), "m" (dst), "m" (src), "m" (sum), "m" (dst_step), "m" (src_step), "m" (sum_step)
		: "esi", "edi", "edx", "ecx", "ebx", "eax"
	);
}


void mix_areas2(unsigned int size,
		volatile s16 *dst, s16 *src,
		volatile s32 *sum,
		unsigned int dst_step,
		unsigned int src_step,
		unsigned int sum_step)
{
	while (size-- > 0) {
		s32 sample = *src;
		if (cmpxchg(dst, 0, 1) == 0)
			sample -= *sum;
		atomic_add(sum, sample);
		do {
			sample = *sum;
			s16 s;
			if (unlikely(sample & 0x7fff0000))
				s = sample > 0 ? 0x7fff : -0x8000;
			else
				s = sample;
			*dst = s;
		} while (unlikely(sample != *sum));
		((char *)sum) += sum_step;
		((char *)dst) += dst_step;
		((char *)src) += src_step;
	}
}

int main(int argc, char **argv)
{
	int size = atoi(argv[1]);
	int n = atoi(argv[2]);
	int max = atoi(argv[3]);
	int i;
	unsigned long long begin, end;
	s16 *dst = malloc(sizeof(*dst) * size);
	s32 *sum = calloc(size, sizeof(*sum));
	s16 **srcs = malloc(sizeof(*srcs) * n);
	for (i = 0; i < n; i++) {
		int k;
		s16 *s;
		srcs[i] = s = malloc(sizeof(s16) * size);
		for (k = 0; k < size; ++k, ++s) {
			*s = (rand() % (max * 2)) - max;
		}
	}
	rdtscll(begin);
	for (i = 0; i < n; i++) {
		mix_areas0(size, dst, srcs[i], sum, 2, 2, 4);
	}
	rdtscll(end);
	printf("mix_areas0    : %lld\n", end - begin);
	rdtscll(begin);
	for (i = 0; i < n; i++) {
		mix_areas1(size, dst, srcs[i], sum, 2, 2, 4);
	}
	rdtscll(end);
	printf("mix_areas1    : %lld\n", end - begin);
	rdtscll(begin);
	for (i = 0; i < n; i++) {
		mix_areas1_mmx(size, dst, srcs[i], sum, 2, 2, 4);
	}
	rdtscll(end);
	printf("mix_areas1_mmx: %lld\n", end - begin);
	rdtscll(begin);
	for (i = 0; i < n; i++) {
		mix_areas2(size, dst, srcs[i], sum, 2, 2, 4);
	}
	rdtscll(end);
	printf("mix_areas2    : %lld\n", end - begin);
}
