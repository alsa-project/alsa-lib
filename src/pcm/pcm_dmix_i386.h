/**
 * \file pcm/pcm_dmix_i386.h
 * \ingroup PCM_Plugins
 * \brief PCM Direct Stream Mixing (dmix) Plugin Interface - I386 assembler code
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2002
 */
/*
 *  PCM - Direct Stream Mixing
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/*
 *  for plain i386
 */
static void MIX_AREAS1(unsigned int size,
		       volatile signed short *dst, signed short *src,
		       volatile signed int *sum, unsigned int dst_step,
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

		"\t.p2align 4,,15\n"

		"1:"

		/*
		 *   sample = *src;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= *sum;
		 *   xadd(*sum, sample);
		 */
		"\tmovw $0, %%ax\n"
		"\tmovw $1, %%cx\n"
		"\t" LOCK_PREFIX "cmpxchgw %%cx, (%%edi)\n"
		"\tmovswl (%%esi), %%ecx\n"
		"\tjnz 2f\n"
		"\tsubl (%%ebx), %%ecx\n"
		"2:"
		"\t" LOCK_PREFIX "addl %%ecx, (%%ebx)\n"

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

		"\t.p2align 4,,15\n"

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

		"\t.p2align 4,,15\n"

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

/*
 *  MMX optimized
 */
static void MIX_AREAS1_MMX(unsigned int size,
			   volatile signed short *dst, signed short *src,
			   volatile signed int *sum, unsigned int dst_step,
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

		"\t.p2align 4,,15\n"

		"1:"

		/*
		 *   sample = *src;
		 *   if (cmpxchg(*dst, 0, 1) == 0)
		 *     sample -= *sum;
		 *   xadd(*sum, sample);
		 */
		"\tmovw $0, %%ax\n"
		"\tmovw $1, %%cx\n"
		"\t" LOCK_PREFIX "cmpxchgw %%cx, (%%edi)\n"
		"\tmovswl (%%esi), %%ecx\n"
		"\tjnz 2f\n"
		"\tsubl (%%ebx), %%ecx\n"
		"2:"
		"\t" LOCK_PREFIX "addl %%ecx, (%%ebx)\n"

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
