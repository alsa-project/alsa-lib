/****************************************************************************
 *                                                                          *
 *                                conv.h                                    *
 *                        Binary Value Conversion                           *
 *                                                                          *
 ****************************************************************************/

/**
 *  \defgroup BConv Binary Value Conversion
 *  Binary Value Conversion
 *  \{
 */

/** convert 16-bit value from host to Little Endian byte order */
#define snd_host_to_LE_16(val)	__cpu_to_le16(val)
/** convert 16-bit value from Little Endian to host byte order */
#define snd_LE_to_host_16(val)	__le16_to_cpu(val)
/** convert 32-bit value from host to Little Endian byte order */
#define snd_host_to_LE_32(val)	__cpu_to_le32(val)
/** convert 32-bit value from Little Endian to host byte order */
#define snd_LE_to_host_32(val)	__le32_to_cpu(val)
/** convert 16-bit value from host to Big Endian byte order */
#define snd_host_to_BE_16(val)	__cpu_to_be16(val)
/** convert 16-bit value from Big Endian to host byte order */
#define snd_BE_to_host_16(val)	__be16_to_cpu(val)
/** convert 32-bit value from host to Big Endian byte order */
#define snd_host_to_BE_32(val)	__cpu_to_be32(val)
/** convert 32-bit value from Big Endian to host byte order */
#define snd_BE_to_host_32(val)	__be32_to_cpu(val)

/** \} */

