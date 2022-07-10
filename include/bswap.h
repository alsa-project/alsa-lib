/*
 *  ALSA lib - compatibility header for providing byte swapping macros
 *  Copyright (c) 2016 by  Thomas Klausner <wiz@NetBSD.org>
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

#ifndef __BSWAP_H
#define __BSWAP_H

#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
#include <sys/endian.h>
#define bswap_16 bswap16
#define bswap_32 bswap32
#define bswap_64 bswap64
#elif defined(__OpenBSD__)
#include <sys/endian.h>
#define bswap_16 swap16
#define bswap_32 swap32
#define bswap_64 swap64
#elif defined (__sun)
#include <sys/byteorder.h>
#define bswap_16 BSWAP_16
#define bswap_32 BSWAP_32
#define bswap_64 BSWAP_64
#else
#include <byteswap.h>
#endif

#endif
