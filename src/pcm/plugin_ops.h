/*
 *  Plugin sample operators with fast switch
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#define as_u8(ptr) (*(u_int8_t*)(ptr))
#define as_u16(ptr) (*(u_int16_t*)(ptr))
#define as_u32(ptr) (*(u_int32_t*)(ptr))
#define as_u64(ptr) (*(u_int64_t*)(ptr))
#define as_s8(ptr) (*(int8_t*)(ptr))
#define as_s16(ptr) (*(int16_t*)(ptr))
#define as_s32(ptr) (*(int32_t*)(ptr))
#define as_s64(ptr) (*(int64_t*)(ptr))

#ifdef COPY_LABELS
static void *copy_labels[4] = {
	&&copy_8,
	&&copy_16,
	&&copy_32,
	&&copy_64
};
#endif

#ifdef COPY_END
while(0) {
copy_8: as_s8(dst) = as_s8(src); goto COPY_END;
copy_16: as_s16(dst) = as_s16(src); goto COPY_END;
copy_32: as_s32(dst) = as_s32(src); goto COPY_END;
copy_64: as_s64(dst) = as_s64(src); goto COPY_END;
}
#endif

#ifdef CONV_LABELS
/* src_wid src_endswap sign_toggle dst_wid dst_endswap */
static void *conv_labels[4 * 2 * 2 * 4 * 2] = {
	&&conv_xxx1_xxx1,	 /*  8h ->  8h */
	&&conv_xxx1_xxx1,	 /*  8h ->  8s */
	&&conv_xxx1_xx10,	 /*  8h -> 16h */
	&&conv_xxx1_xx01,	 /*  8h -> 16s */
	&&conv_xxx1_x100,	 /*  8h -> 24h */
	&&conv_xxx1_001x,	 /*  8h -> 24s */
	&&conv_xxx1_1000,	 /*  8h -> 32h */
	&&conv_xxx1_0001,	 /*  8h -> 32s */
	&&conv_xxx1_xxx9,	 /*  8h ^>  8h */
	&&conv_xxx1_xxx9,	 /*  8h ^>  8s */
	&&conv_xxx1_xx90,	 /*  8h ^> 16h */
	&&conv_xxx1_xx09,	 /*  8h ^> 16s */
	&&conv_xxx1_x900,	 /*  8h ^> 24h */
	&&conv_xxx1_009x,	 /*  8h ^> 24s */
	&&conv_xxx1_9000,	 /*  8h ^> 32h */
	&&conv_xxx1_0009,	 /*  8h ^> 32s */
	&&conv_xxx1_xxx1,	 /*  8s ->  8h */
	&&conv_xxx1_xxx1,	 /*  8s ->  8s */
	&&conv_xxx1_xx10,	 /*  8s -> 16h */
	&&conv_xxx1_xx01,	 /*  8s -> 16s */
	&&conv_xxx1_x100,	 /*  8s -> 24h */
	&&conv_xxx1_001x,	 /*  8s -> 24s */
	&&conv_xxx1_1000,	 /*  8s -> 32h */
	&&conv_xxx1_0001,	 /*  8s -> 32s */
	&&conv_xxx1_xxx9,	 /*  8s ^>  8h */
	&&conv_xxx1_xxx9,	 /*  8s ^>  8s */
	&&conv_xxx1_xx90,	 /*  8s ^> 16h */
	&&conv_xxx1_xx09,	 /*  8s ^> 16s */
	&&conv_xxx1_x900,	 /*  8s ^> 24h */
	&&conv_xxx1_009x,	 /*  8s ^> 24s */
	&&conv_xxx1_9000,	 /*  8s ^> 32h */
	&&conv_xxx1_0009,	 /*  8s ^> 32s */
	&&conv_xx12_xxx1,	 /* 16h ->  8h */
	&&conv_xx12_xxx1,	 /* 16h ->  8s */
	&&conv_xx12_xx12,	 /* 16h -> 16h */
	&&conv_xx12_xx21,	 /* 16h -> 16s */
	&&conv_xx12_x120,	 /* 16h -> 24h */
	&&conv_xx12_021x,	 /* 16h -> 24s */
	&&conv_xx12_1200,	 /* 16h -> 32h */
	&&conv_xx12_0021,	 /* 16h -> 32s */
	&&conv_xx12_xxx9,	 /* 16h ^>  8h */
	&&conv_xx12_xxx9,	 /* 16h ^>  8s */
	&&conv_xx12_xx92,	 /* 16h ^> 16h */
	&&conv_xx12_xx29,	 /* 16h ^> 16s */
	&&conv_xx12_x920,	 /* 16h ^> 24h */
	&&conv_xx12_029x,	 /* 16h ^> 24s */
	&&conv_xx12_9200,	 /* 16h ^> 32h */
	&&conv_xx12_0029,	 /* 16h ^> 32s */
	&&conv_xx12_xxx2,	 /* 16s ->  8h */
	&&conv_xx12_xxx2,	 /* 16s ->  8s */
	&&conv_xx12_xx21,	 /* 16s -> 16h */
	&&conv_xx12_xx12,	 /* 16s -> 16s */
	&&conv_xx12_x210,	 /* 16s -> 24h */
	&&conv_xx12_012x,	 /* 16s -> 24s */
	&&conv_xx12_2100,	 /* 16s -> 32h */
	&&conv_xx12_0012,	 /* 16s -> 32s */
	&&conv_xx12_xxxA,	 /* 16s ^>  8h */
	&&conv_xx12_xxxA,	 /* 16s ^>  8s */
	&&conv_xx12_xxA1,	 /* 16s ^> 16h */
	&&conv_xx12_xx1A,	 /* 16s ^> 16s */
	&&conv_xx12_xA10,	 /* 16s ^> 24h */
	&&conv_xx12_01Ax,	 /* 16s ^> 24s */
	&&conv_xx12_A100,	 /* 16s ^> 32h */
	&&conv_xx12_001A,	 /* 16s ^> 32s */
	&&conv_x123_xxx1,	 /* 24h ->  8h */
	&&conv_x123_xxx1,	 /* 24h ->  8s */
	&&conv_x123_xx12,	 /* 24h -> 16h */
	&&conv_x123_xx21,	 /* 24h -> 16s */
	&&conv_x123_x123,	 /* 24h -> 24h */
	&&conv_x123_321x,	 /* 24h -> 24s */
	&&conv_x123_1230,	 /* 24h -> 32h */
	&&conv_x123_0321,	 /* 24h -> 32s */
	&&conv_x123_xxx9,	 /* 24h ^>  8h */
	&&conv_x123_xxx9,	 /* 24h ^>  8s */
	&&conv_x123_xx92,	 /* 24h ^> 16h */
	&&conv_x123_xx29,	 /* 24h ^> 16s */
	&&conv_x123_x923,	 /* 24h ^> 24h */
	&&conv_x123_329x,	 /* 24h ^> 24s */
	&&conv_x123_9230,	 /* 24h ^> 32h */
	&&conv_x123_0329,	 /* 24h ^> 32s */
	&&conv_123x_xxx3,	 /* 24s ->  8h */
	&&conv_123x_xxx3,	 /* 24s ->  8s */
	&&conv_123x_xx32,	 /* 24s -> 16h */
	&&conv_123x_xx23,	 /* 24s -> 16s */
	&&conv_123x_x321,	 /* 24s -> 24h */
	&&conv_123x_123x,	 /* 24s -> 24s */
	&&conv_123x_3210,	 /* 24s -> 32h */
	&&conv_123x_0123,	 /* 24s -> 32s */
	&&conv_123x_xxxB,	 /* 24s ^>  8h */
	&&conv_123x_xxxB,	 /* 24s ^>  8s */
	&&conv_123x_xxB2,	 /* 24s ^> 16h */
	&&conv_123x_xx2B,	 /* 24s ^> 16s */
	&&conv_123x_xB21,	 /* 24s ^> 24h */
	&&conv_123x_12Bx,	 /* 24s ^> 24s */
	&&conv_123x_B210,	 /* 24s ^> 32h */
	&&conv_123x_012B,	 /* 24s ^> 32s */
	&&conv_1234_xxx1,	 /* 32h ->  8h */
	&&conv_1234_xxx1,	 /* 32h ->  8s */
	&&conv_1234_xx12,	 /* 32h -> 16h */
	&&conv_1234_xx21,	 /* 32h -> 16s */
	&&conv_1234_x123,	 /* 32h -> 24h */
	&&conv_1234_321x,	 /* 32h -> 24s */
	&&conv_1234_1234,	 /* 32h -> 32h */
	&&conv_1234_4321,	 /* 32h -> 32s */
	&&conv_1234_xxx9,	 /* 32h ^>  8h */
	&&conv_1234_xxx9,	 /* 32h ^>  8s */
	&&conv_1234_xx92,	 /* 32h ^> 16h */
	&&conv_1234_xx29,	 /* 32h ^> 16s */
	&&conv_1234_x923,	 /* 32h ^> 24h */
	&&conv_1234_329x,	 /* 32h ^> 24s */
	&&conv_1234_9234,	 /* 32h ^> 32h */
	&&conv_1234_4329,	 /* 32h ^> 32s */
	&&conv_1234_xxx4,	 /* 32s ->  8h */
	&&conv_1234_xxx4,	 /* 32s ->  8s */
	&&conv_1234_xx43,	 /* 32s -> 16h */
	&&conv_1234_xx34,	 /* 32s -> 16s */
	&&conv_1234_x432,	 /* 32s -> 24h */
	&&conv_1234_234x,	 /* 32s -> 24s */
	&&conv_1234_4321,	 /* 32s -> 32h */
	&&conv_1234_1234,	 /* 32s -> 32s */
	&&conv_1234_xxxC,	 /* 32s ^>  8h */
	&&conv_1234_xxxC,	 /* 32s ^>  8s */
	&&conv_1234_xxC3,	 /* 32s ^> 16h */
	&&conv_1234_xx3C,	 /* 32s ^> 16s */
	&&conv_1234_xC32,	 /* 32s ^> 24h */
	&&conv_1234_23Cx,	 /* 32s ^> 24s */
	&&conv_1234_C321,	 /* 32s ^> 32h */
	&&conv_1234_123C,	 /* 32s ^> 32s */
};
#endif

#ifdef CONV_END
while(0) {
conv_xxx1_xxx1: as_u8(dst) = as_u8(src); goto CONV_END;
conv_xxx1_xx10: as_u16(dst) = (u_int16_t)as_u8(src) << 8; goto CONV_END;
conv_xxx1_xx01: as_u16(dst) = (u_int16_t)as_u8(src); goto CONV_END;
conv_xxx1_x100: as_u32(dst) = (u_int32_t)as_u8(src) << 16; goto CONV_END;
conv_xxx1_001x: as_u32(dst) = (u_int32_t)as_u8(src) << 8; goto CONV_END;
conv_xxx1_1000: as_u32(dst) = (u_int32_t)as_u8(src) << 24; goto CONV_END;
conv_xxx1_0001: as_u32(dst) = (u_int32_t)as_u8(src); goto CONV_END;
conv_xxx1_xxx9: as_u8(dst) = as_u8(src) ^ 0x80; goto CONV_END;
conv_xxx1_xx90: as_u16(dst) = (u_int16_t)(as_u8(src) ^ 0x80) << 8; goto CONV_END;
conv_xxx1_xx09: as_u16(dst) = (u_int16_t)(as_u8(src) ^ 0x80); goto CONV_END;
conv_xxx1_x900: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80) << 16; goto CONV_END;
conv_xxx1_009x: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80) << 8; goto CONV_END;
conv_xxx1_9000: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80) << 24; goto CONV_END;
conv_xxx1_0009: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80); goto CONV_END;
conv_xx12_xxx1: as_u8(dst) = as_u16(src) >> 8; goto CONV_END;
conv_xx12_xx12: as_u16(dst) = as_u16(src); goto CONV_END;
conv_xx12_xx21: as_u16(dst) = bswap_16(as_u16(src)); goto CONV_END;
conv_xx12_x120: as_u32(dst) = (u_int32_t)as_u16(src) << 8; goto CONV_END;
conv_xx12_021x: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)) << 8; goto CONV_END;
conv_xx12_1200: as_u32(dst) = (u_int32_t)as_u16(src) << 16; goto CONV_END;
conv_xx12_0021: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)); goto CONV_END;
conv_xx12_xxx9: as_u8(dst) = (as_u16(src) >> 8) ^ 0x80; goto CONV_END;
conv_xx12_xx92: as_u16(dst) = as_u16(src) ^ 0x8000; goto CONV_END;
conv_xx12_xx29: as_u16(dst) = bswap_16(as_u16(src)) ^ 0x80; goto CONV_END;
conv_xx12_x920: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x8000) << 8; goto CONV_END;
conv_xx12_029x: as_u32(dst) = (u_int32_t)(bswap_16(as_u16(src)) ^ 0x80) << 8; goto CONV_END;
conv_xx12_9200: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x8000) << 16; goto CONV_END;
conv_xx12_0029: as_u32(dst) = (u_int32_t)(bswap_16(as_u16(src)) ^ 0x80); goto CONV_END;
conv_xx12_xxx2: as_u8(dst) = as_u16(src) & 0xff; goto CONV_END;
conv_xx12_x210: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)) << 8; goto CONV_END;
conv_xx12_012x: as_u32(dst) = (u_int32_t)as_u16(src) << 8; goto CONV_END;
conv_xx12_2100: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)) << 16; goto CONV_END;
conv_xx12_0012: as_u32(dst) = (u_int32_t)as_u16(src); goto CONV_END; 
conv_xx12_xxxA: as_u8(dst) = (as_u16(src) ^ 0x80) & 0xff; goto CONV_END;
conv_xx12_xxA1: as_u16(dst) = bswap_16(as_u16(src) ^ 0x80); goto CONV_END;
conv_xx12_xx1A: as_u16(dst) = as_u16(src) ^ 0x80; goto CONV_END;
conv_xx12_xA10: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src) ^ 0x80) << 8; goto CONV_END;
conv_xx12_01Ax: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x80) << 8; goto CONV_END;
conv_xx12_A100: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src) ^ 0x80) << 16; goto CONV_END;
conv_xx12_001A: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x80); goto CONV_END;
conv_x123_xxx1: as_u8(dst) = as_u32(src) >> 16; goto CONV_END;
conv_x123_xx12: as_u16(dst) = as_u32(src) >> 8; goto CONV_END;
conv_x123_xx21: as_u16(dst) = bswap_16(as_u32(src) >> 8); goto CONV_END;
conv_x123_x123: as_u32(dst) = as_u32(src); goto CONV_END;
conv_x123_321x: as_u32(dst) = bswap_32(as_u32(src)); goto CONV_END;
conv_x123_1230: as_u32(dst) = as_u32(src) << 8; goto CONV_END;
conv_x123_0321: as_u32(dst) = bswap_32(as_u32(src)) >> 8; goto CONV_END;
conv_x123_xxx9: as_u8(dst) = (as_u32(src) >> 16) ^ 0x80; goto CONV_END;
conv_x123_xx92: as_u16(dst) = (as_u32(src) >> 8) ^ 0x8000; goto CONV_END;
conv_x123_xx29: as_u16(dst) = bswap_16(as_u32(src) >> 8) ^ 0x80; goto CONV_END;
conv_x123_x923: as_u32(dst) = as_u32(src) ^ 0x800000; goto CONV_END;
conv_x123_329x: as_u32(dst) = bswap_32(as_u32(src)) ^ 0x8000; goto CONV_END;
conv_x123_9230: as_u32(dst) = (as_u32(src) ^ 0x800000) << 8; goto CONV_END;
conv_x123_0329: as_u32(dst) = (bswap_32(as_u32(src)) >> 8) ^ 0x80; goto CONV_END;
conv_123x_xxx3: as_u8(dst) = (as_u32(src) >> 8) & 0xff; goto CONV_END;
conv_123x_xx32: as_u16(dst) = bswap_16(as_u32(src) >> 8); goto CONV_END;
conv_123x_xx23: as_u16(dst) = (as_u32(src) >> 8) & 0xffff; goto CONV_END;
conv_123x_x321: as_u32(dst) = bswap_32(as_u32(src)); goto CONV_END;
conv_123x_123x: as_u32(dst) = as_u32(src); goto CONV_END;
conv_123x_3210: as_u32(dst) = bswap_32(as_u32(src)) << 8; goto CONV_END;
conv_123x_0123: as_u32(dst) = as_u32(src) >> 8; goto CONV_END;
conv_123x_xxxB: as_u8(dst) = ((as_u32(src) >> 8) & 0xff) ^ 0x80; goto CONV_END;
conv_123x_xxB2: as_u16(dst) = bswap_16((as_u32(src) >> 8) ^ 0x80); goto CONV_END;
conv_123x_xx2B: as_u16(dst) = ((as_u32(src) >> 8) & 0xffff) ^ 0x80; goto CONV_END;
conv_123x_xB21: as_u32(dst) = bswap_32(as_u32(src)) ^ 0x800000; goto CONV_END;
conv_123x_12Bx: as_u32(dst) = as_u32(src) ^ 0x8000; goto CONV_END;
conv_123x_B210: as_u32(dst) = bswap_32(as_u32(src) ^ 0x8000) << 8; goto CONV_END;
conv_123x_012B: as_u32(dst) = (as_u32(src) >> 8) ^ 0x80; goto CONV_END;
conv_1234_xxx1: as_u8(dst) = as_u32(src) >> 24; goto CONV_END;
conv_1234_xx12: as_u16(dst) = as_u32(src) >> 16; goto CONV_END;
conv_1234_xx21: as_u16(dst) = bswap_16(as_u32(src) >> 16); goto CONV_END;
conv_1234_x123: as_u32(dst) = as_u32(src) >> 8; goto CONV_END;
conv_1234_321x: as_u32(dst) = bswap_32(as_u32(src)) << 8; goto CONV_END;
conv_1234_1234: as_u32(dst) = as_u32(src); goto CONV_END;
conv_1234_4321: as_u32(dst) = bswap_32(as_u32(src)); goto CONV_END;
conv_1234_xxx9: as_u8(dst) = (as_u32(src) >> 24) ^ 0x80; goto CONV_END;
conv_1234_xx92: as_u16(dst) = (as_u32(src) >> 16) ^ 0x8000; goto CONV_END;
conv_1234_xx29: as_u16(dst) = bswap_16(as_u32(src) >> 16) ^ 0x80; goto CONV_END;
conv_1234_x923: as_u32(dst) = (as_u32(src) >> 8) ^ 0x800000; goto CONV_END;
conv_1234_329x: as_u32(dst) = (bswap_32(as_u32(src)) ^ 0x80) << 8; goto CONV_END;
conv_1234_9234: as_u32(dst) = as_u32(src) ^ 0x80000000; goto CONV_END;
conv_1234_4329: as_u32(dst) = bswap_32(as_u32(src)) ^ 0x80; goto CONV_END;
conv_1234_xxx4: as_u8(dst) = as_u32(src) & 0xff; goto CONV_END;
conv_1234_xx43: as_u16(dst) = bswap_16(as_u32(src)); goto CONV_END;
conv_1234_xx34: as_u16(dst) = as_u32(src) & 0xffff; goto CONV_END;
conv_1234_x432: as_u32(dst) = bswap_32(as_u32(src)) >> 8; goto CONV_END;
conv_1234_234x: as_u32(dst) = as_u32(src) << 8; goto CONV_END;
conv_1234_xxxC: as_u8(dst) = (as_u32(src) & 0xff) ^ 0x80; goto CONV_END;
conv_1234_xxC3: as_u16(dst) = bswap_16(as_u32(src) ^ 0x80); goto CONV_END;
conv_1234_xx3C: as_u16(dst) = (as_u32(src) & 0xffff) ^ 0x80; goto CONV_END;
conv_1234_xC32: as_u32(dst) = (bswap_32(as_u32(src)) >> 8) ^ 0x800000; goto CONV_END;
conv_1234_23Cx: as_u32(dst) = (as_u32(src) ^ 0x80) << 8; goto CONV_END;
conv_1234_C321: as_u32(dst) = bswap_32(as_u32(src) ^ 0x80); goto CONV_END;
conv_1234_123C: as_u32(dst) = as_u32(src) ^ 0x80; goto CONV_END;
}
#endif

#ifdef GET16_LABELS
/* src_wid src_endswap sign_toggle */
static void *get16_labels[4 * 2 * 2] = {
	&&get16_1_10,	 /*  8h -> 16h */
	&&get16_1_90,	 /*  8h ^> 16h */
	&&get16_1_10,	 /*  8s -> 16h */
	&&get16_1_90,	 /*  8s ^> 16h */
	&&get16_12_12,	 /* 16h -> 16h */
	&&get16_12_92,	 /* 16h ^> 16h */
	&&get16_12_21,	 /* 16s -> 16h */
	&&get16_12_A1,	 /* 16s ^> 16h */
	&&get16_0123_12, /* 24h -> 16h */
	&&get16_0123_92, /* 24h ^> 16h */
	&&get16_1230_32, /* 24s -> 16h */
	&&get16_1230_B2, /* 24s ^> 16h */
	&&get16_1234_12, /* 32h -> 16h */
	&&get16_1234_92, /* 32h ^> 16h */
	&&get16_1234_43, /* 32s -> 16h */
	&&get16_1234_C3, /* 32s ^> 16h */
};
#endif

#ifdef GET16_END
while(0) {
get16_1_10: sample = (u_int16_t)as_u8(src) << 8; goto GET16_END;
get16_1_90: sample = (u_int16_t)(as_u8(src) ^ 0x80) << 8; goto GET16_END;
get16_12_12: sample = as_u16(src); goto GET16_END;
get16_12_92: sample = as_u16(src) ^ 0x8000; goto GET16_END;
get16_12_21: sample = bswap_16(as_u16(src)); goto GET16_END;
get16_12_A1: sample = bswap_16(as_u16(src) ^ 0x80); goto GET16_END;
get16_0123_12: sample = as_u32(src) >> 8; goto GET16_END;
get16_0123_92: sample = (as_u32(src) >> 8) ^ 0x8000; goto GET16_END;
get16_1230_32: sample = bswap_16(as_u32(src) >> 8); goto GET16_END;
get16_1230_B2: sample = bswap_16((as_u32(src) >> 8) ^ 0x8000); goto GET16_END;
get16_1234_12: sample = as_u32(src) >> 16; goto GET16_END;
get16_1234_92: sample = (as_u32(src) >> 16) ^ 0x8000; goto GET16_END;
get16_1234_43: sample = bswap_16(as_u32(src)); goto GET16_END;
get16_1234_C3: sample = bswap_16(as_u32(src) ^ 0x80); goto GET16_END;
}
#endif

#ifdef PUT16_LABELS
/* dst_wid dst_endswap sign_toggle */
static void *put16_labels[4 * 2 * 2 * 4 * 2] = {
	&&put16_12_1,	 /* 16h ->  8h */
	&&put16_12_9,	 /* 16h ^>  8h */
	&&put16_12_1,	 /* 16h ->  8s */
	&&put16_12_9,	 /* 16h ^>  8s */
	&&put16_12_12,	 /* 16h -> 16h */
	&&put16_12_92,	 /* 16h ^> 16h */
	&&put16_12_21,	 /* 16h -> 16s */
	&&put16_12_29,	 /* 16h ^> 16s */
	&&put16_12_0120,	 /* 16h -> 24h */
	&&put16_12_0920,	 /* 16h ^> 24h */
	&&put16_12_0210,	 /* 16h -> 24s */
	&&put16_12_0290,	 /* 16h ^> 24s */
	&&put16_12_1200,	 /* 16h -> 32h */
	&&put16_12_9200,	 /* 16h ^> 32h */
	&&put16_12_0021,	 /* 16h -> 32s */
	&&put16_12_0029,	 /* 16h ^> 32s */
};
#endif

#ifdef PUT16_END
while (0) {
put16_12_1: as_u8(dst) = sample >> 8; goto PUT16_END;
put16_12_9: as_u8(dst) = (sample >> 8) ^ 0x80; goto PUT16_END;
put16_12_12: as_u16(dst) = sample; goto PUT16_END;
put16_12_92: as_u16(dst) = sample ^ 0x8000; goto PUT16_END;
put16_12_21: as_u16(dst) = bswap_16(sample); goto PUT16_END;
put16_12_29: as_u16(dst) = bswap_16(sample) ^ 0x80; goto PUT16_END;
put16_12_0120: as_u32(dst) = (u_int32_t)sample << 8; goto PUT16_END;
put16_12_0920: as_u32(dst) = (u_int32_t)(sample ^ 0x8000) << 8; goto PUT16_END;
put16_12_0210: as_u32(dst) = (u_int32_t)bswap_16(sample) << 8; goto PUT16_END;
put16_12_0290: as_u32(dst) = (u_int32_t)(bswap_16(sample) ^ 0x80) << 8; goto PUT16_END;
put16_12_1200: as_u32(dst) = (u_int32_t)sample << 16; goto PUT16_END;
put16_12_9200: as_u32(dst) = (u_int32_t)(sample ^ 0x8000) << 16; goto PUT16_END;
put16_12_0021: as_u32(dst) = (u_int32_t)bswap_16(sample); goto PUT16_END;
put16_12_0029: as_u32(dst) = (u_int32_t)bswap_16(sample) ^ 0x80; goto PUT16_END;
}
#endif

#ifdef GET32_LABELS
/* src_wid src_endswap sign_toggle */
static void *get32_labels[4 * 2 * 2] = {
	&&get32_1_1000,	 	/*  8h -> 32h */
	&&get32_1_9000,	 	/*  8h ^> 32h */
	&&get32_1_1000,	 	/*  8s -> 32h */
	&&get32_1_9000,		 /*  8s ^> 32h */
	&&get32_12_1200,	 /* 16h -> 32h */
	&&get32_12_9200,	 /* 16h ^> 32h */
	&&get32_12_2100,	 /* 16s -> 32h */
	&&get32_12_A100,	 /* 16s ^> 32h */
	&&get32_0123_1230,	 /* 24h -> 32h */
	&&get32_0123_9230,	 /* 24h ^> 32h */
	&&get32_1230_3210,	 /* 24s -> 32h */
	&&get32_1230_B210,	 /* 24s ^> 32h */
	&&get32_1234_1234,	 /* 32h -> 32h */
	&&get32_1234_9234,	 /* 32h ^> 32h */
	&&get32_1234_4321,	 /* 32s -> 32h */
	&&get32_1234_C321,	 /* 32s ^> 32h */
};
#endif

#ifdef GET32_END
while (0) {
get32_1_1000: sample = (u_int32_t)as_u8(src) << 24; goto GET32_END;
get32_1_9000: sample = (u_int32_t)(as_u8(src) ^ 0x80) << 24; goto GET32_END;
get32_12_1200: sample = (u_int32_t)as_u16(src) << 16; goto GET32_END;
get32_12_9200: sample = (u_int32_t)(as_u16(src) ^ 0x8000) << 16; goto GET32_END;
get32_12_2100: sample = (u_int32_t)bswap_16(as_u16(src)) << 16; goto GET32_END;
get32_12_A100: sample = (u_int32_t)bswap_16(as_u16(src) ^ 0x80) << 16; goto GET32_END;
get32_0123_1230: sample = as_u32(src) << 8; goto GET32_END;
get32_0123_9230: sample = (as_u32(src) << 8) ^ 0x80000000; goto GET32_END;
get32_1230_3210: sample = bswap_32(as_u32(src) >> 8); goto GET32_END;
get32_1230_B210: sample = bswap_32((as_u32(src) >> 8) ^ 0x80); goto GET32_END;
get32_1234_1234: sample = as_u32(src); goto GET32_END;
get32_1234_9234: sample = as_u32(src) ^ 0x80000000; goto GET32_END;
get32_1234_4321: sample = bswap_32(as_u32(src)); goto GET32_END;
get32_1234_C321: sample = bswap_32(as_u32(src) ^ 0x80); goto GET32_END;
}
#endif

#ifdef PUT32_LABELS
/* dst_wid dst_endswap sign_toggle */
static void *put32_labels[4 * 2 * 2] = {
	&&put32_1234_1,	 	/* 32h ->  8h */
	&&put32_1234_9,	 	/* 32h ^>  8h */
	&&put32_1234_1,	 	/* 32h ->  8s */
	&&put32_1234_9,	 	/* 32h ^>  8s */
	&&put32_1234_12,	 /* 32h -> 16h */
	&&put32_1234_92,	 /* 32h ^> 16h */
	&&put32_1234_21,	 /* 32h -> 16s */
	&&put32_1234_29,	 /* 32h ^> 16s */
	&&put32_1234_0123,	 /* 32h -> 24h */
	&&put32_1234_0923,	 /* 32h ^> 24h */
	&&put32_1234_3210,	 /* 32h -> 24s */
	&&put32_1234_3290,	 /* 32h ^> 24s */
	&&put32_1234_1234,	 /* 32h -> 32h */
	&&put32_1234_9234,	 /* 32h ^> 32h */
	&&put32_1234_4321,	 /* 32h -> 32s */
	&&put32_1234_4329,	 /* 32h ^> 32s */
};
#endif

#ifdef PUT32_END
while (0) {
put32_1234_1: as_u8(dst) = sample >> 24; goto PUT32_END;
put32_1234_9: as_u8(dst) = (sample >> 24) ^ 0x80; goto PUT32_END;
put32_1234_12: as_u16(dst) = sample >> 16; goto PUT32_END;
put32_1234_92: as_u16(dst) = (sample >> 16) ^ 0x8000; goto PUT32_END;
put32_1234_21: as_u16(dst) = bswap_16(sample >> 16); goto PUT32_END;
put32_1234_29: as_u16(dst) = bswap_16(sample >> 16) ^ 0x80; goto PUT32_END;
put32_1234_0123: as_u32(dst) = sample >> 8; goto PUT32_END;
put32_1234_0923: as_u32(dst) = (sample >> 8) ^ 0x800000; goto PUT32_END;
put32_1234_3210: as_u32(dst) = bswap_32(sample) << 8; goto PUT32_END;
put32_1234_3290: as_u32(dst) = (bswap_32(sample) ^ 0x80) << 8; goto PUT32_END;
put32_1234_1234: as_u32(dst) = sample; goto PUT32_END;
put32_1234_9234: as_u32(dst) = sample ^ 0x80000000; goto PUT32_END;
put32_1234_4321: as_u32(dst) = bswap_32(sample); goto PUT32_END;
put32_1234_4329: as_u32(dst) = bswap_32(sample) ^ 0x80; goto PUT32_END;
}
#endif

#ifdef GETU_LABELS
/* width endswap sign_toggle */
static void *getu_labels[4 * 2 * 2] = {
	&&getu_1_1,		/*  8h ->  8h */
	&&getu_1_9,		/*  8h ^>  8h */
	&&getu_1_1,		/*  8s ->  8h */
	&&getu_1_9,		/*  8s ^>  8h */
	&&getu_12_12,		/* 16h -> 16h */
	&&getu_12_92,		/* 16h ^> 16h */
	&&getu_12_21,		/* 16s -> 16h */
	&&getu_12_A1,		/* 16s ^> 16h */
	&&getu_0123_0123,	/* 24h -> 24h */
	&&getu_0123_0923,	/* 24h ^> 24h */
	&&getu_1230_0321,	/* 24s -> 24h */
	&&getu_1230_0B21,	/* 24s ^> 24h */
	&&getu_1234_1234,	/* 32h -> 32h */
	&&getu_1234_9234,	/* 32h ^> 32h */
	&&getu_1234_4321,	/* 32s -> 32h */
	&&getu_1234_C321,	/* 32s ^> 32h */
};
#endif

#ifdef GETU_END
while (0) {
getu_1_1: sample = as_u8(src); goto GETU_END;
getu_1_9: sample = as_u8(src) ^ 0x80; goto GETU_END;
getu_12_12: sample = as_u16(src); goto GETU_END;
getu_12_92: sample = as_u16(src) ^ 0x8000; goto GETU_END;
getu_12_21: sample = bswap_16(as_u16(src)); goto GETU_END;
getu_12_A1: sample = bswap_16(as_u16(src) ^ 0x80); goto GETU_END;
getu_0123_0123: sample = as_u32(src); goto GETU_END;
getu_0123_0923: sample = (as_u32(src) ^ 0x800000); goto GETU_END;
getu_1230_0321: sample = bswap_32(as_u32(src)); goto GETU_END;
getu_1230_0B21: sample = bswap_32(as_u32(src) ^ 0x8000); goto GETU_END;
getu_1234_1234: sample = as_u32(src); goto GETU_END;
getu_1234_9234: sample = as_u32(src) ^ 0x80000000; goto GETU_END;
getu_1234_4321: sample = bswap_32(as_u32(src)); goto GETU_END;
getu_1234_C321: sample = bswap_32(as_u32(src) ^ 0x80); goto GETU_END;
}
#endif

#ifdef PUT_LABELS
/* width endswap sign_toggle */
static void *put_labels[4 * 2 * 2] = {
	&&put_1_1,		/*  8h ->  8h */
	&&put_1_9,		/*  8h ^>  8h */
	&&put_1_1,		/*  8h ->  8s */
	&&put_1_9,		/*  8h ^>  8s */
	&&put_12_12,		/* 16h -> 16h */
	&&put_12_92,		/* 16h ^> 16h */
	&&put_12_21,		/* 16h -> 16s */
	&&put_12_A1,		/* 16h ^> 16s */
	&&put_0123_0123,	/* 24h -> 24h */
	&&put_0123_0923,	/* 24h ^> 24h */
	&&put_0123_3210,	/* 24h -> 24s */
	&&put_0123_3290,	/* 24h ^> 24s */
	&&put_1234_1234,	/* 32h -> 32h */
	&&put_1234_9234,	/* 32h ^> 32h */
	&&put_1234_4321,	/* 32h -> 32s */
	&&put_1234_4329,	/* 32h ^> 32s */
};
#endif

#ifdef PUT_END
put_1_1: as_s8(dst) = sample; goto PUT_END;
put_1_9: as_u8(dst) = sample ^ 0x80; goto PUT_END;
put_12_12: as_s16(dst) = sample; goto PUT_END;
put_12_92: as_u16(dst) = sample ^ 0x8000; goto PUT_END;
put_12_21: as_s16(dst) = bswap_16(sample); goto PUT_END;
put_12_A1: as_u16(dst) = bswap_16(sample ^ 0x80); goto PUT_END;
put_0123_0123: as_s24(dst) = sample; goto PUT_END;
put_0123_0923: as_u24(dst) = sample ^ 0x800000; goto PUT_END;
put_0123_3210: as_s24(dst) = bswap_32(sample); goto PUT_END;
put_0123_3290: as_u24(dst) = bswap_32(sample) ^ 0x8000; goto PUT_END;
put_1234_1234: as_s32(dst) = sample; goto PUT_END;
put_1234_9234: as_u32(dst) = sample ^ 0x80000000; goto PUT_END;
put_1234_4321: as_s32(dst) = bswap_32(sample); goto PUT_END;
put_1234_4329: as_u32(dst) = bswap_32(sample) ^ 0x80; goto PUT_END;
#endif

#ifdef GETS_LABELS
static inline int32_t getS(const void *src, int src_sign, int src_wid, int src_end,
			   int dst_wid)
{
	int32_t s;
	switch (src_wid) {
	case 8:
		s = *(u_int8_t*)src;
		break;
	case 16:
		s = *(u_int16_t*)src;
		if (src_end)
			s = bswap_16(s);
		break;
	case 24:
	case 32:
		s = *(u_int32_t*)src;
		if (src_end)
			s = bswap_32(s);
		break;
	}
	if (!src_sign)
		s -= 1U << (src_wid - 1);
	if (src_wid < dst_wid)
		return s * (1 << (dst_wid - src_wid));
	else
		return s / (1 << (src_wid - dst_wid)); 
}

/* src_sign src_wid src_end dst_wid */
static void *gets_labels[2 * 4 * 2 * 4] = {
	&&gets_u8_8,	/*  u8h -> s8 */
	&&gets_u8_16,	/*  u8h -> s16 */
	&&gets_u8_24,	/*  u8h -> s24 */
	&&gets_u8_32,	/*  u8h -> s32 */
	&&gets_u8_8,	/*  u8s -> s8 */
	&&gets_u8_16,	/*  u8s -> s16 */
	&&gets_u8_24,	/*  u8s -> s24 */
	&&gets_u8_32,	/*  u8s -> s32 */
	&&gets_u16h_8,	/* u16h -> s8 */
	&&gets_u16h_16,	/* u16h -> s16 */
	&&gets_u16h_24,	/* u16h -> s24 */
	&&gets_u16h_32,	/* u16h -> s32 */
	&&gets_u16s_8,	/* u16s -> s8 */
	&&gets_u16s_16,	/* u16s -> s16 */
	&&gets_u16s_24,	/* u16s -> s24 */
	&&gets_u16s_32,	/* u16s -> s32 */
	&&gets_u24h_8,	/* u24h -> s8 */
	&&gets_u24h_16,	/* u24h -> s16 */
	&&gets_u24h_24,	/* u24h -> s24 */
	&&gets_u24h_32,	/* u24h -> s32 */
	&&gets_u24s_8,	/* u24s -> s8 */
	&&gets_u24s_16,	/* u24s -> s16 */
	&&gets_u24s_24,	/* u24s -> s24 */
	&&gets_u24s_32,	/* u24s -> s32 */
	&&gets_u32h_8,	/* u32h -> s8 */
	&&gets_u32h_16,	/* u32h -> s16 */
	&&gets_u32h_24,	/* u32h -> s24 */
	&&gets_u32h_32,	/* u32h -> s32 */
	&&gets_u32s_8,	/* u32s -> s8 */
	&&gets_u32s_16,	/* u32s -> s16 */
	&&gets_u32s_24,	/* u32s -> s24 */
	&&gets_u32s_32,	/* u32s -> s32 */
	&&gets_s8_8,	/*  s8h -> s8 */
	&&gets_s8_16,	/*  s8h -> s16 */
	&&gets_s8_24,	/*  s8h -> s24 */
	&&gets_s8_32,	/*  s8h -> s32 */
	&&gets_s8_8,	/*  s8s -> s8 */
	&&gets_s8_16,	/*  s8s -> s16 */
	&&gets_s8_24,	/*  s8s -> s24 */
	&&gets_s8_32,	/*  s8s -> s32 */
	&&gets_s16h_8,	/* s16h -> s8 */
	&&gets_s16h_16,	/* s16h -> s16 */
	&&gets_s16h_24,	/* s16h -> s24 */
	&&gets_s16h_32,	/* s16h -> s32 */
	&&gets_s16s_8,	/* s16s -> s8 */
	&&gets_s16s_16,	/* s16s -> s16 */
	&&gets_s16s_24,	/* s16s -> s24 */
	&&gets_s16s_32,	/* s16s -> s32 */
	&&gets_s24h_8,	/* s24h -> s8 */
	&&gets_s24h_16,	/* s24h -> s16 */
	&&gets_s24h_24,	/* s24h -> s24 */
	&&gets_s24h_32,	/* s24h -> s32 */
	&&gets_s24s_8,	/* s24s -> s8 */
	&&gets_s24s_16,	/* s24s -> s16 */
	&&gets_s24s_24,	/* s24s -> s24 */
	&&gets_s24s_32,	/* s24s -> s32 */
	&&gets_s32h_8,	/* s32h -> s8 */
	&&gets_s32h_16,	/* s32h -> s16 */
	&&gets_s32h_24,	/* s32h -> s24 */
	&&gets_s32h_32,	/* s32h -> s32 */
	&&gets_s32s_8,	/* s32s -> s8 */
	&&gets_s32s_16,	/* s32s -> s16 */
	&&gets_s32s_24,	/* s32s -> s24 */
	&&gets_s32s_32,	/* s32s -> s32 */
};
#endif

#ifdef GETS_END
gets_u8_8:    sample = getS(src, 0,  8, 0,  8); goto GETS_END;
gets_u8_16:   sample = getS(src, 0,  8, 0, 16); goto GETS_END;
gets_u8_24:   sample = getS(src, 0,  8, 0, 24); goto GETS_END;
gets_u8_32:   sample = getS(src, 0,  8, 0, 32); goto GETS_END;
gets_u16h_8:  sample = getS(src, 0, 16, 0,  8); goto GETS_END;
gets_u16h_16: sample = getS(src, 0, 16, 0, 16); goto GETS_END;
gets_u16h_24: sample = getS(src, 0, 16, 0, 24); goto GETS_END;
gets_u16h_32: sample = getS(src, 0, 16, 0, 32); goto GETS_END;
gets_u16s_8:  sample = getS(src, 0, 16, 1,  8); goto GETS_END;
gets_u16s_16: sample = getS(src, 0, 16, 1, 16); goto GETS_END;
gets_u16s_24: sample = getS(src, 0, 16, 1, 24); goto GETS_END;
gets_u16s_32: sample = getS(src, 0, 16, 1, 32); goto GETS_END;
gets_u24h_8:  sample = getS(src, 0, 24, 0,  8); goto GETS_END;
gets_u24h_16: sample = getS(src, 0, 24, 0, 16); goto GETS_END;
gets_u24h_24: sample = getS(src, 0, 24, 0, 24); goto GETS_END;
gets_u24h_32: sample = getS(src, 0, 24, 0, 32); goto GETS_END;
gets_u24s_8:  sample = getS(src, 0, 24, 1,  8); goto GETS_END;
gets_u24s_16: sample = getS(src, 0, 24, 1, 16); goto GETS_END;
gets_u24s_24: sample = getS(src, 0, 24, 1, 24); goto GETS_END;
gets_u24s_32: sample = getS(src, 0, 24, 1, 32); goto GETS_END;
gets_u32h_8:  sample = getS(src, 0, 32, 0,  8); goto GETS_END;
gets_u32h_16: sample = getS(src, 0, 32, 0, 16); goto GETS_END;
gets_u32h_24: sample = getS(src, 0, 32, 0, 24); goto GETS_END;
gets_u32h_32: sample = getS(src, 0, 32, 0, 32); goto GETS_END;
gets_u32s_8:  sample = getS(src, 0, 32, 1,  8); goto GETS_END;
gets_u32s_16: sample = getS(src, 0, 32, 1, 16); goto GETS_END;
gets_u32s_24: sample = getS(src, 0, 32, 1, 24); goto GETS_END;
gets_u32s_32: sample = getS(src, 0, 32, 1, 32); goto GETS_END;
gets_s8_8:    sample = getS(src, 1,  8, 0,  8); goto GETS_END;
gets_s8_16:   sample = getS(src, 1,  8, 0, 16); goto GETS_END;
gets_s8_24:   sample = getS(src, 1,  8, 0, 24); goto GETS_END;
gets_s8_32:   sample = getS(src, 1,  8, 0, 32); goto GETS_END;
gets_s16h_8:  sample = getS(src, 1, 16, 0,  8); goto GETS_END;
gets_s16h_16: sample = getS(src, 1, 16, 0, 16); goto GETS_END;
gets_s16h_24: sample = getS(src, 1, 16, 0, 24); goto GETS_END;
gets_s16h_32: sample = getS(src, 1, 16, 0, 32); goto GETS_END;
gets_s16s_8:  sample = getS(src, 1, 16, 1,  8); goto GETS_END;
gets_s16s_16: sample = getS(src, 1, 16, 1, 16); goto GETS_END;
gets_s16s_24: sample = getS(src, 1, 16, 1, 24); goto GETS_END;
gets_s16s_32: sample = getS(src, 1, 16, 1, 32); goto GETS_END;
gets_s24h_8:  sample = getS(src, 1, 24, 0,  8); goto GETS_END;
gets_s24h_16: sample = getS(src, 1, 24, 0, 16); goto GETS_END;
gets_s24h_24: sample = getS(src, 1, 24, 0, 24); goto GETS_END;
gets_s24h_32: sample = getS(src, 1, 24, 0, 32); goto GETS_END;
gets_s24s_8:  sample = getS(src, 1, 24, 1,  8); goto GETS_END;
gets_s24s_16: sample = getS(src, 1, 24, 1, 16); goto GETS_END;
gets_s24s_24: sample = getS(src, 1, 24, 1, 24); goto GETS_END;
gets_s24s_32: sample = getS(src, 1, 24, 1, 32); goto GETS_END;
gets_s32h_8:  sample = getS(src, 1, 32, 0,  8); goto GETS_END;
gets_s32h_16: sample = getS(src, 1, 32, 0, 16); goto GETS_END;
gets_s32h_24: sample = getS(src, 1, 32, 0, 24); goto GETS_END;
gets_s32h_32: sample = getS(src, 1, 32, 0, 32); goto GETS_END;
gets_s32s_8:  sample = getS(src, 1, 32, 1,  8); goto GETS_END;
gets_s32s_16: sample = getS(src, 1, 32, 1, 16); goto GETS_END;
gets_s32s_24: sample = getS(src, 1, 32, 1, 24); goto GETS_END;
gets_s32s_32: sample = getS(src, 1, 32, 1, 32); goto GETS_END;
#endif

#undef as_u8
#undef as_u16
#undef as_u32
#undef as_s8
#undef as_s16
#undef as_s32
