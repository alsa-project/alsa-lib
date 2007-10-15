/**
 * \file include/conv.h
 * \brief Application interface library for the ALSA driver
 * \author Jaroslav Kysela <perex@perex.cz>
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Takashi Iwai <tiwai@suse.de>
 * \date 1998-2001
 *
 * Application interface library for the ALSA driver
 */
/*
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

#ifndef __ALSA_CONV_H
#define __ALSA_CONV_H

/**
 *  \defgroup BConv Binary Value Conversion
 *  Helper macros to convert binary values to/from a specific byte order.
 *  \{
 */

/** Converts a 16-bit value from host to little endian byte order. */
#define snd_host_to_LE_16(val)	__cpu_to_le16(val)
/** Converts a 16-bit value from little endian to host byte order. */
#define snd_LE_to_host_16(val)	__le16_to_cpu(val)
/** Converts a 32-bit value from host to little endian byte order. */
#define snd_host_to_LE_32(val)	__cpu_to_le32(val)
/** Converts a 32-bit value from little endian to host byte order. */
#define snd_LE_to_host_32(val)	__le32_to_cpu(val)
/** Converts a 16-bit value from host to big endian byte order. */
#define snd_host_to_BE_16(val)	__cpu_to_be16(val)
/** Converts a 16-bit value from big endian to host byte order. */
#define snd_BE_to_host_16(val)	__be16_to_cpu(val)
/** Converts a 32-bit value from host to big endian byte order. */
#define snd_host_to_BE_32(val)	__cpu_to_be32(val)
/** Converts a 32-bit value from big endian to host byte order. */
#define snd_BE_to_host_32(val)	__be32_to_cpu(val)

/** \} */

#endif /* __ALSA_CONV_H */

