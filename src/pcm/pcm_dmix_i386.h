/**
 * \file pcm/pcm_dmix_i386.h
 * \ingroup PCM_Plugins
 * \brief PCM Direct Stream Mixing (dmix) Plugin Interface - I386 assembler code
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2003
 */
/*
 *  PCM - Direct Stream Mixing
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@perex.cz>
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
#  define BOUNDED_EBX
#endif

/*
 *  for plain i386
 */
static void MIX_AREAS_16(unsigned int size,
			 volatile signed short *dst, signed short *src,
			 volatile signed int *sum, size_t dst_step,
			 size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_EBX
	unsigned int old_ebx;
#endif
	/*
	 *  ESI - src
	 *  EDI - dst
	 *  EBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_EBX
		"\tmovl %%ebx, %[old_ebx]\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		/*
		 *  initialization, load ESI, EDI, EBX registers
		 */
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
		"\tcmpl $0, %[size]\n"
		"\tjnz 2f\n"
		"\tjmp 7f\n"


		/*
		 * for (;;)
		 */
		"\t.p2align 4,,15\n"
		"1:"
		"\tadd %[dst_step], %%edi\n"
		"\tadd %[src_step], %%esi\n"
		"\tadd %[sum_step], %%ebx\n"

		/*
		 *   sample = *src;
		 *   sum_sample = *sum;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= sum_sample;
		 *   xadd(*sum, sample);
		 */

		"2:"
		"\tmovw $0, %%ax\n"
		"\tmovw $1, %%cx\n"
		"\tmovl (%%ebx), %%edx\n"
		"\t" LOCK_PREFIX "cmpxchgw %%cx, (%%edi)\n"
		"\tmovswl (%%esi), %%ecx\n"
		"\tjnz 3f\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"3:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%ebx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(v);
		 *     *dst = sample;
		 *   } while (v != *sum);
		 */

		"4:"
		"\tmovl (%%ebx), %%ecx\n"
		"\tcmpl $0x7fff,%%ecx\n"
		"\tjg 5f\n"
		"\tcmpl $-0x8000,%%ecx\n"
		"\tjl 6f\n"
		"\tmovw %%cx, (%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 4b\n"

		/*
		 * while (size-- > 0)
		 */
		"\tdecl %[size]\n"
		"\tjnz 1b\n"
		"\tjmp 7f\n"

		/*
		 *  sample > 0x7fff
		 */

		"\t.p2align 4,,15\n"

		"5:"
		"\tmovw $0x7fff, (%%edi)\n"
		"\tcmpl %%ecx,(%%ebx)\n"
		"\tjnz 4b\n"
		"\tdecl %[size]\n"
		"\tjnz 1b\n"
		"\tjmp 7f\n"

		/*
		 *  sample < -0x8000
		 */

		"\t.p2align 4,,15\n"

		"6:"
		"\tmovw $-0x8000, (%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 4b\n"
		"\tdecl %[size]\n"
		"\tjnz 1b\n"

		"7:"
#ifdef BOUNDED_EBX
		"\tmovl %[old_ebx], %%ebx\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_EBX
		  , [old_ebx] "=m" (old_ebx)
#endif
		: [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "esi", "edi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_EBX
		  , "ebx"
#endif
	);
}

/*
 *  MMX optimized
 */
static void MIX_AREAS_16_MMX(unsigned int size,
			     volatile signed short *dst, signed short *src,
			     volatile signed int *sum, size_t dst_step,
			     size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_EBX
	unsigned int old_ebx;
#endif
	/*
	 *  ESI - src
	 *  EDI - dst
	 *  EBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_EBX
		"\tmovl %%ebx, %[old_ebx]\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		/*
		 *  initialization, load ESI, EDI, EBX registers
		 */
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
		"\tcmpl $0, %[size]\n"
		"\tjnz 2f\n"
		"\tjmp 5f\n"

		"\t.p2align 4,,15\n"
		"1:"
		"\tadd %[dst_step], %%edi\n"
		"\tadd %[src_step], %%esi\n"
		"\tadd %[sum_step], %%ebx\n"

		"2:"
		/*
		 *   sample = *src;
		 *   sum_sample = *sum;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= sum_sample;
		 *   xadd(*sum, sample);
		 */
		"\tmovw $0, %%ax\n"
		"\tmovw $1, %%cx\n"
		"\tmovl (%%ebx), %%edx\n"
		"\t" LOCK_PREFIX "cmpxchgw %%cx, (%%edi)\n"
		"\tmovswl (%%esi), %%ecx\n"
		"\tjnz 3f\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"3:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%ebx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(v);
		 *     *dst = sample;
		 *   } while (v != *sum);
		 */

		"4:"
		"\tmovl (%%ebx), %%ecx\n"
		"\tmovd %%ecx, %%mm0\n"
		"\tpackssdw %%mm1, %%mm0\n"
		"\tmovd %%mm0, %%eax\n"
		"\tmovw %%ax, (%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 4b\n"

		/*
		 * while (size-- > 0)
		 */
		"\tdecl %[size]\n"
		"\tjnz 1b\n"
		"\temms\n"
                "5:"
#ifdef BOUNDED_EBX
		"\tmovl %[old_ebx], %%ebx\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_EBX
		  , [old_ebx] "=m" (old_ebx)
#endif
		: [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "esi", "edi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_EBX
		  , "ebx"
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
 *  for plain i386, 32-bit version (24-bit resolution)
 */
static void MIX_AREAS_32(unsigned int size,
			 volatile signed int *dst, signed int *src,
			 volatile signed int *sum, size_t dst_step,
			 size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_EBX
	unsigned int old_ebx;
#endif
	/*
	 *  ESI - src
	 *  EDI - dst
	 *  EBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_EBX
		"\tmovl %%ebx, %[old_ebx]\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		/*
		 *  initialization, load ESI, EDI, EBX registers
		 */
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
		"\tcmpl $0, %[size]\n"
		"\tjnz 1f\n"
		"\tjmp 6f\n"

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
		"\tmovl (%%ebx), %%edx\n"
		"\t" LOCK_PREFIX "cmpxchgl %%ecx, (%%edi)\n"
		"\tjnz 2f\n"
		"\tmovl (%%esi), %%ecx\n"
		/* sample >>= 8 */
		"\tsarl $8, %%ecx\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"\tjmp 21f\n"
		"2:"
		"\tmovl (%%esi), %%ecx\n"
		/* sample >>= 8 */
		"\tsarl $8, %%ecx\n"
		"21:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%ebx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(v);
		 *     *dst = sample;
		 *   } while (v != *sum);
		 */

		"3:"
		"\tmovl (%%ebx), %%ecx\n"
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
		"\tmovl %%eax, (%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
		"\tdecl %[size]\n"
		"\tjz 6f\n"
		"\tadd %[dst_step], %%edi\n"
		"\tadd %[src_step], %%esi\n"
		"\tadd %[sum_step], %%ebx\n"
		"\tjmp 1b\n"

		"6:"
#ifdef BOUNDED_EBX
		"\tmovl %[old_ebx], %%ebx\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_EBX
		  , [old_ebx] "=m" (old_ebx)
#endif
		: [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "esi", "edi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_EBX
		  , "ebx"
#endif
	);
}

/*
 * 24-bit version for plain i386
 */
static void MIX_AREAS_24(unsigned int size,
			 volatile unsigned char *dst, unsigned char *src,
			 volatile signed int *sum, size_t dst_step,
			 size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_EBX
	unsigned int old_ebx;
#endif
	/*
	 *  ESI - src
	 *  EDI - dst
	 *  EBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_EBX
		"\tmovl %%ebx, %[old_ebx]\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		/*
		 *  initialization, load ESI, EDI, EBX registers
		 */
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
		"\tcmpl $0, %[size]\n"
		"\tjnz 1f\n"
		"\tjmp 6f\n"

		"\t.p2align 4,,15\n"

		"1:"

		/*
		 *   sample = *src;
		 *   sum_sample = *sum;
		 *   if (test_and_set_bit(0, dst) == 0)
		 *     sample -= sum_sample;
		 *   *sum += sample;
		 */
		"\tmovsbl 2(%%esi), %%eax\n"
		"\tmovzwl (%%esi), %%ecx\n"
		"\tmovl (%%ebx), %%edx\n"
		"\tsall $16, %%eax\n"
		"\torl %%eax, %%ecx\n"
		"\t" LOCK_PREFIX "btsw $0, (%%edi)\n"
		"\tjc 2f\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"2:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%ebx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(sample);
		 *     *dst = sample | 1;
		 *   } while (old_sample != *sum);
		 */

		"3:"
		"\tmovl (%%ebx), %%ecx\n"
		/*
		 *  if (sample > 0x7fffff)
		 */
		"\tmovl $0x7fffff, %%eax\n"
		"\tcmpl %%eax, %%ecx\n"
		"\tjg 4f\n"
		/*
		 *  if (sample < -0x7fffff)
		 */
		"\tmovl $-0x7fffff, %%eax\n"
		"\tcmpl %%eax, %%ecx\n"
		"\tjl 4f\n"
		"\tmovl %%ecx, %%eax\n"
		"\torl $1, %%eax\n"
		"4:"
		"\tmovw %%ax, (%%edi)\n"
		"\tshrl $16, %%eax\n"
		"\tmovb %%al, 2(%%edi)\n"
		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
		"\tdecl %[size]\n"
		"\tjz 6f\n"
		"\tadd %[dst_step], %%edi\n"
		"\tadd %[src_step], %%esi\n"
		"\tadd %[sum_step], %%ebx\n"
		"\tjmp 1b\n"

		"6:"
#ifdef BOUNDED_EBX
		"\tmovl %[old_ebx], %%ebx\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_EBX
		  , [old_ebx] "=m" (old_ebx)
#endif
		: [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "esi", "edi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_EBX
		  , "ebx"
#endif
	);
}

/*
 * 24-bit version for Pentium Pro/II
 */
static void MIX_AREAS_24_CMOV(unsigned int size,
			      volatile unsigned char *dst, unsigned char *src,
			      volatile signed int *sum, size_t dst_step,
			      size_t src_step, size_t sum_step)
{
#ifdef BOUNDED_EBX
	unsigned int old_ebx;
#endif
	/*
	 *  ESI - src
	 *  EDI - dst
	 *  EBX - sum
	 *  ECX - old sample
	 *  EAX - sample / temporary
	 *  EDX - temporary
	 */
	__asm__ __volatile__ (
		"\n"
#ifdef BOUNDED_EBX
		"\tmovl %%ebx, %[old_ebx]\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		/*
		 *  initialization, load ESI, EDI, EBX registers
		 */
		"\tmovl %[dst], %%edi\n"
		"\tmovl %[src], %%esi\n"
		"\tmovl %[sum], %%ebx\n"
		"\tcmpl $0, %[size]\n"
		"\tjz 6f\n"

		"\t.p2align 4,,15\n"

		"1:"

		/*
		 *   sample = *src;
		 *   sum_sample = *sum;
		 *   if (test_and_set_bit(0, dst) == 0)
		 *     sample -= sum_sample;
		 *   *sum += sample;
		 */
		"\tmovsbl 2(%%esi), %%eax\n"
		"\tmovzwl (%%esi), %%ecx\n"
		"\tmovl (%%ebx), %%edx\n"
		"\tsall $16, %%eax\n"
		"\t" LOCK_PREFIX "btsw $0, (%%edi)\n"
		"\tleal (%%ecx,%%eax,1), %%ecx\n"
		"\tjc 2f\n"
		"\t" XSUB " %%edx, %%ecx\n"
		"2:"
		"\t" LOCK_PREFIX XADD " %%ecx, (%%ebx)\n"

		/*
		 *   do {
		 *     sample = old_sample = *sum;
		 *     saturate(sample);
		 *     *dst = sample | 1;
		 *   } while (old_sample != *sum);
		 */

		"3:"
		"\tmovl (%%ebx), %%ecx\n"

		"\tmovl $0x7fffff, %%eax\n"
		"\tmovl $-0x7fffff, %%edx\n"
		"\tcmpl %%eax, %%ecx\n"
		"\tcmovng %%ecx, %%eax\n"
		"\tcmpl %%edx, %%ecx\n"
		"\tcmovl %%edx, %%eax\n"

		"\torl $1, %%eax\n"
		"\tmovw %%ax, (%%edi)\n"
		"\tshrl $16, %%eax\n"
		"\tmovb %%al, 2(%%edi)\n"

		"\tcmpl %%ecx, (%%ebx)\n"
		"\tjnz 3b\n"

		/*
		 * while (size-- > 0)
		 */
		"\tadd %[dst_step], %%edi\n"
		"\tadd %[src_step], %%esi\n"
		"\tadd %[sum_step], %%ebx\n"
		"\tdecl %[size]\n"
		"\tjnz 1b\n"

		"6:"
#ifdef BOUNDED_EBX
		"\tmovl %[old_ebx], %%ebx\n"	/* ebx is GOT pointer (-fPIC) */
#endif
		: [size] "+&rm" (size)
#ifdef BOUNDED_EBX
		  , [old_ebx] "=m" (old_ebx)
#endif
		: [dst] "m" (dst), [src] "m" (src), [sum] "m" (sum),
		  [dst_step] "im" (dst_step),  [src_step] "im" (src_step),
		  [sum_step] "im" (sum_step)
		: "esi", "edi", "edx", "ecx", "eax", "memory", "cc"
#ifndef BOUNDED_EBX
		  , "ebx"
#endif
	);
}

#ifdef BOUNDED_EBX
#  undef BOUNDED_EBX
#endif
