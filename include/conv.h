/****************************************************************************
 *                                                                          *
 *                                conv.h                                    *
 *                        Binary Value Conversion                           *
 *                                                                          *
 ****************************************************************************/

#ifdef SND_LITTLE_ENDIAN

#define snd_host_to_LE_16(val)	(val)
#define snd_LE_to_host_16(val)	(val)
#define snd_host_to_LE_32(val)	(val)
#define snd_LE_to_host_32(val)	(val)
#define snd_host_to_BE_16(val)	((((val)>>8)&0xff)|(((val)<<8)&0xff00))
#define snd_BE_to_host_16(val)	snd_host_to_BE_16(val)
#define snd_host_to_BE_32(val)	((((val)>>24)&0xff)|(((val)>>16)&0xff00)|\
                                 (((val)<<16)&0xff0000)|(((val)<<24)&0xff000000))
#define snd_BE_to_host_32(val)	snd_host_to_BE_32(val)

#else                                      

#define snd_host_to_BE_16(val)	(val)
#define snd_BE_to_host_16(val)	(val)
#define snd_host_to_BE_16(val)	(val)
#define snd_BE_to_host_16(val)	(val)
#define snd_host_to_LE_16(val)	((((val)>>8)&0xff)|(((val)<<8)&0xff00))
#define snd_LE_to_host_16(val)	snd_host_to_LE_16(val)
#define snd_host_to_LE_32(val)	((((val)>>24)&0xff)|(((val)>>16)&0xff00)|\
                                 (((val)<<16)&0xff0000)|(((val)<<24)&0xff000000))
#define snd_LE_to_host_32(val)	snd_host_to_LE_32(val)

#endif

