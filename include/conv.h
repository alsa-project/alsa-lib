/****************************************************************************
 *                                                                          *
 *                                conv.h                                    *
 *                        Binary Value Conversion                           *
 *                                                                          *
 ****************************************************************************/

#define snd_host_to_LE_16(val)	__cpu_to_le16(val)
#define snd_LE_to_host_16(val)	__le16_to_cpu(val)
#define snd_host_to_LE_32(val)	__cpu_to_le32(val)
#define snd_LE_to_host_32(val)	__le32_to_cpu(val)
#define snd_host_to_BE_16(val)	__cpu_to_be16(val)
#define snd_BE_to_host_16(val)	__be16_to_cpu(val)
#define snd_host_to_BE_32(val)	__cpu_to_be32(val)
#define snd_BE_to_host_32(val)	__be32_to_cpu(val)

