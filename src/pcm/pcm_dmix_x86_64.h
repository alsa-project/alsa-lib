/**
 * \file pcm/pcm_dmix_x86_64.h
 * \ingroup PCM_Plugins
 * \brief PCM Direct Stream Mixing (dmix) Plugin Interface - X86-64 assembler code
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 2003
 */
/*
 *  PCM - Direct Stream Mixing
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@perex.cz>
 *                        Takashi Iwai <tiwai@suse.de>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#if defined(__GNUC__) && __GNUC__ < 5 && defined(__PIC__)
#  define BOUNDED_RBX
#endif

/*
 *  MMX optimized
 */
static void MIX_AREAS_16(unsigned int size,
			 volatile signed short *dst, signed short *src,
			 volatile signed int *sum, size_t dst_step,
			 size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_RBX
	unsigned long long old_rbx;
#endif
	/*
	 *  RSI - src
	 *  RDI - dst
	 *  RBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_RBX
		"\tmovq %%rbx, %[old_rbx]\n"
#endif
		/*
		 *  initialization, load RSI, RDI, RBX registers
		 */
#ifndef _ILP32
		"\tmovq %[dst], %%rdi\n"
		"\tmovq %[src], %%rsi\n"
		"\tmovq %[sum], %%rbx\n"
#else
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
#endif

		/*
		 * while (size-- > 0) {
		 */
		"\tcmpl $0, %[size]\n"
		"jz 6f\n"

		"\t.p2align 4,,15\n"

		"1:"

		/*
		 *   sample = *src;
		 *   sum_sample = *sum;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= sum_sample;
		 *   xadd(*sum, sample);
		 */
		"\tmovw $0, %%ax\n"
		"\tmovw $1, %%cx\n"
		"\tmovl (%%rbx), %%edx\n"
		"\t" LOCK_PREFIX "cmpxchgw %%cx, (%%rdi)\n"
		"\tmovswl (%%rsi), %%ecx\n"
		"\tjnz 2f\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"2:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%rbx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(v);
		 *     *dst = sample;
		 *   } while (v != *sum);
		 */

		"3:"
		"\tmovl (%%rbx), %%ecx\n"
		"\tmovd %%ecx, %%mm0\n"
		"\tpackssdw %%mm1, %%mm0\n"
		"\tmovd %%mm0, %%eax\n"
		"\tmovw %%ax, (%%rdi)\n"
		"\tcmpl %%ecx, (%%rbx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
#ifndef _ILP32
		"\taddq %[dst_step], %%rdi\n"
		"\taddq %[src_step], %%rsi\n"
		"\taddq %[sum_step], %%rbx\n"
#else
		"\taddl %[dst_step], %%edi\n"
		"\taddl %[src_step], %%esi\n"
		"\taddl %[sum_step], %%ebx\n"
#endif
		"\tdecl %[size]\n"
		"\tjnz 1b\n"

		"6:"

		"\temms\n"
#ifdef BOUNDED_RBX
		"\tmovq %[old_rbx], %%rbx\n"
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_RBX
		  , [old_rbx] "=m" (old_rbx)
#endif
	        : [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "rsi", "rdi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_RBX
		  , "rbx"
#endif
#ifdef HAVE_MMX
		  , "mm0"
#else
		  , "st", "st(1)", "st(2)", "st(3)",
		  "st(4)", "st(5)", "st(6)", "st(7)"
#endif
	);
}

/*
 *  32-bit version (24-bit resolution)
 */
static void MIX_AREAS_32(unsigned int size,
			 volatile signed int *dst, signed int *src,
			 volatile signed int *sum, size_t dst_step,
			 size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_RBX
	unsigned long long old_rbx;
#endif
	/*
	 *  RSI - src
	 *  RDI - dst
	 *  RBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_RBX
		"\tmovq %%rbx, %[old_rbx]\n"
#endif
		/*
		 *  initialization, load RSI, RDI, RBX registers
		 */
#ifndef _ILP32
		"\tmovq %[dst], %%rdi\n"
		"\tmovq %[src], %%rsi\n"
		"\tmovq %[sum], %%rbx\n"
#else
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
#endif

		/*
		 * while (size-- > 0) {
		 */
		"\tcmpl $0, %[size]\n"
		"jz 6f\n"

		"\t.p2align 4,,15\n"

		"1:"

		/*
		 *   sample = *src;
		 *   sum_sample = *sum;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= sum_sample;
		 *   xadd(*sum, sample);
		 */
		"\tmovl $0, %%eax\n"
		"\tmovl $1, %%ecx\n"
		"\tmovl (%%rbx), %%edx\n"
		"\t" LOCK_PREFIX "cmpxchgl %%ecx, (%%rdi)\n"
		"\tjnz 2f\n"
		"\tmovl (%%rsi), %%ecx\n"
		/* sample >>= 8 */
		"\tsarl $8, %%ecx\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"\tjmp 21f\n"
		"2:"
		"\tmovl (%%rsi), %%ecx\n"
		/* sample >>= 8 */
		"\tsarl $8, %%ecx\n"
		"21:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%rbx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(v);
		 *     *dst = sample;
		 *   } while (v != *sum);
		 */

		"3:"
		"\tmovl (%%rbx), %%ecx\n"
		/*
		 *  if (sample > 0x7fff00)
		 */
		"\tmovl $0x7fffff, %%eax\n"
		"\tcmpl %%eax, %%ecx\n"
		"\tjg 4f\n"
		/*
		 *  if (sample < -0x800000)
		 */
		"\tmovl $-0x800000, %%eax\n"
		"\tcmpl %%eax, %%ecx\n"
		"\tjl 4f\n"
		"\tmovl %%ecx, %%eax\n"
		"4:"
		/*
		 *  sample <<= 8;
		 */
		"\tsall $8, %%eax\n"
		"\tmovl %%eax, (%%rdi)\n"
		"\tcmpl %%ecx, (%%rbx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
#ifndef _ILP32
		"\taddq %[dst_step], %%rdi\n"
		"\taddq %[src_step], %%rsi\n"
		"\taddq %[sum_step], %%rbx\n"
#else
		"\taddl %[dst_step], %%edi\n"
		"\taddl %[src_step], %%esi\n"
		"\taddl %[sum_step], %%ebx\n"
#endif
		"\tdecl %[size]\n"
		"\tjnz 1b\n"

		"6:"
#ifdef BOUNDED_RBX
		"\tmovq %[old_rbx], %%rbx\n"
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_RBX
		  , [old_rbx] "=m" (old_rbx)
#endif
	        : [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "rsi", "rdi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_RBX
		  , "rbx"
#endif
	);
}

/*
 *  24-bit version
 */
static void MIX_AREAS_24(unsigned int size,
			 volatile unsigned char *dst, unsigned char *src,
			 volatile signed int *sum, size_t dst_step,
			 size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_RBX
	unsigned long long old_rbx;
#endif
	/*
	 *  RSI - src
	 *  RDI - dst
	 *  RBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_RBX
		"\tmovq %%rbx, %[old_rbx]\n"
#endif
		/*
		 *  initialization, load RSI, RDI, RBX registers
		 */
#ifndef _ILP32
		"\tmovq %[dst], %%rdi\n"
		"\tmovq %[src], %%rsi\n"
		"\tmovq %[sum], %%rbx\n"
#else
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
#endif

		/*
		 * while (size-- > 0) {
		 */
		"\tcmpl $0, %[size]\n"
		"jz 6f\n"

		"\t.p2align 4,,15\n"

		"1:"

		/*
		 *   sample = *src;
		 *   sum_sample = *sum;
		 *   if (test_and_set_bit(0, dst) == 0)
		 *     sample -= sum_sample;
		 *   *sum += sample;
		 */
		"\tmovsbl 2(%%rsi), %%eax\n"
		"\tmovzwl (%%rsi), %%ecx\n"
		"\tmovl (%%rbx), %%edx\n"
		"\tsall $16, %%eax\n"
		"\torl %%eax, %%ecx\n"
		"\t" LOCK_PREFIX "btsw $0, (%%rdi)\n"
		"\tjc 2f\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"2:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%rbx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(sample);
		 *     *dst = sample | 1;
		 *   } while (old_sample != *sum);
		 */

		"3:"
		"\tmovl (%%rbx), %%ecx\n"

		"\tmovl $0x7fffff, %%eax\n"
		"\tmovl $-0x7fffff, %%edx\n"
		"\tcmpl %%eax, %%ecx\n"
		"\tcmovng %%ecx, %%eax\n"
		"\tcmpl %%edx, %%ecx\n"
		"\tcmovl %%edx, %%eax\n"

		"\torl $1, %%eax\n"
		"\tmovw %%ax, (%%rdi)\n"
		"\tshrl $16, %%eax\n"
		"\tmovb %%al, 2(%%rdi)\n"

		"\tcmpl %%ecx, (%%rbx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
#ifndef _ILP32
		"\taddq %[dst_step], %%rdi\n"
		"\taddq %[src_step], %%rsi\n"
		"\taddq %[sum_step], %%rbx\n"
#else
		"\taddl %[dst_step], %%edi\n"
		"\taddl %[src_step], %%esi\n"
		"\taddl %[sum_step], %%ebx\n"
#endif
		"\tdecl %[size]\n"
		"\tjnz 1b\n"

		"6:"
#ifdef BOUNDED_RBX
		"\tmovq %[old_rbx], %%rbx\n"
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_RBX
		  , [old_rbx] "=m" (old_rbx)
#endif
	        : [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "rsi", "rdi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_RBX
		  , "rbx"
#endif
	);
}

#ifdef BOUNDED_RBX
#  undef BOUNDED_RBX
#endif
