/*
 *  ALSA lib - dynamic symbol versions
 *  Copyright (c) 2002 by Jaroslav Kysela <perex@suse.cz>
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

#ifndef __ALSA_SYMBOLS_H
#define __ALSA_SYMBOLS_H

#if defined(PIC) && defined(VERSIONED_SYMBOLS) /* might be also configurable */
#define USE_VERSIONED_SYMBOLS
#endif

#define INTERNAL_CONCAT2_2(Pre, Post) Pre##Post
#define INTERNAL(Name) INTERNAL_CONCAT2_2(__, Name)

#ifdef __powerpc64__
# define symbol_version(real, name, version) 			\
	__asm__ (".symver " #real "," #name "@" #version);	\
	__asm__ (".symver ." #real ",." #name "@" #version)
# define default_symbol_version(real, name, version) 		\
	__asm__ (".symver " #real "," #name "@@" #version);	\
	__asm__ (".symver ." #real ",." #name "@@" #version)
#else
# define symbol_version(real, name, version) \
	__asm__ (".symver " #real "," #name "@" #version)
# define default_symbol_version(real, name, version) \
	__asm__ (".symver " #real "," #name "@@" #version)
#endif

#ifdef USE_VERSIONED_SYMBOLS
#define use_symbol_version(real, name, version) \
		symbol_version(real, name, version)
#define use_default_symbol_version(real, name, version) \
		default_symbol_version(real, name, version)
#else
#define use_symbol_version(real, name, version) /* nothing */
#ifdef __powerpc64__
#define use_default_symbol_version(real, name, version) \
	__asm__ (".weak " #name); 			\
	__asm__ (".weak ." #name); 			\
	__asm__ (".set " #name "," #real);		\
	__asm__ (".set ." #name ",." #real)
#else
#define use_default_symbol_version(real, name, version) \
	__asm__ (".weak " #name); \
	__asm__ (".set " #name "," #real)
#endif
#endif

#endif /* __ALSA_SYMBOLS_H */
