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
#define as_s8(ptr) (*(int8_t*)(ptr))
#define as_s16(ptr) (*(int16_t*)(ptr))
#define as_s32(ptr) (*(int32_t*)(ptr))


#ifdef COPY_LABELS
/* src_wid src_endswap sign_toggle dst_wid dst_endswap */
static void *copy_labels[4 * 2 * 2 * 4 * 2] = {
	&&copy_xxx1_xxx1,	 /*  8h ->  8h */
	&&copy_xxx1_xxx1,	 /*  8h ->  8s */
	&&copy_xxx1_xx10,	 /*  8h -> 16h */
	&&copy_xxx1_xx01,	 /*  8h -> 16s */
	&&copy_xxx1_x100,	 /*  8h -> 24h */
	&&copy_xxx1_001x,	 /*  8h -> 24s */
	&&copy_xxx1_1000,	 /*  8h -> 32h */
	&&copy_xxx1_0001,	 /*  8h -> 32s */
	&&copy_xxx1_xxx9,	 /*  8h ^>  8h */
	&&copy_xxx1_xxx9,	 /*  8h ^>  8s */
	&&copy_xxx1_xx90,	 /*  8h ^> 16h */
	&&copy_xxx1_xx09,	 /*  8h ^> 16s */
	&&copy_xxx1_x900,	 /*  8h ^> 24h */
	&&copy_xxx1_009x,	 /*  8h ^> 24s */
	&&copy_xxx1_9000,	 /*  8h ^> 32h */
	&&copy_xxx1_0009,	 /*  8h ^> 32s */
	&&copy_xxx1_xxx1,	 /*  8s ->  8h */
	&&copy_xxx1_xxx1,	 /*  8s ->  8s */
	&&copy_xxx1_xx10,	 /*  8s -> 16h */
	&&copy_xxx1_xx01,	 /*  8s -> 16s */
	&&copy_xxx1_x100,	 /*  8s -> 24h */
	&&copy_xxx1_001x,	 /*  8s -> 24s */
	&&copy_xxx1_1000,	 /*  8s -> 32h */
	&&copy_xxx1_0001,	 /*  8s -> 32s */
	&&copy_xxx1_xxx9,	 /*  8s ^>  8h */
	&&copy_xxx1_xxx9,	 /*  8s ^>  8s */
	&&copy_xxx1_xx90,	 /*  8s ^> 16h */
	&&copy_xxx1_xx09,	 /*  8s ^> 16s */
	&&copy_xxx1_x900,	 /*  8s ^> 24h */
	&&copy_xxx1_009x,	 /*  8s ^> 24s */
	&&copy_xxx1_9000,	 /*  8s ^> 32h */
	&&copy_xxx1_0009,	 /*  8s ^> 32s */
	&&copy_xx12_xxx1,	 /* 16h ->  8h */
	&&copy_xx12_xxx1,	 /* 16h ->  8s */
	&&copy_xx12_xx12,	 /* 16h -> 16h */
	&&copy_xx12_xx21,	 /* 16h -> 16s */
	&&copy_xx12_x120,	 /* 16h -> 24h */
	&&copy_xx12_021x,	 /* 16h -> 24s */
	&&copy_xx12_1200,	 /* 16h -> 32h */
	&&copy_xx12_0021,	 /* 16h -> 32s */
	&&copy_xx12_xxx9,	 /* 16h ^>  8h */
	&&copy_xx12_xxx9,	 /* 16h ^>  8s */
	&&copy_xx12_xx92,	 /* 16h ^> 16h */
	&&copy_xx12_xx29,	 /* 16h ^> 16s */
	&&copy_xx12_x920,	 /* 16h ^> 24h */
	&&copy_xx12_029x,	 /* 16h ^> 24s */
	&&copy_xx12_9200,	 /* 16h ^> 32h */
	&&copy_xx12_0029,	 /* 16h ^> 32s */
	&&copy_xx12_xxx2,	 /* 16s ->  8h */
	&&copy_xx12_xxx2,	 /* 16s ->  8s */
	&&copy_xx12_xx21,	 /* 16s -> 16h */
	&&copy_xx12_xx12,	 /* 16s -> 16s */
	&&copy_xx12_x210,	 /* 16s -> 24h */
	&&copy_xx12_012x,	 /* 16s -> 24s */
	&&copy_xx12_2100,	 /* 16s -> 32h */
	&&copy_xx12_0012,	 /* 16s -> 32s */
	&&copy_xx12_xxxA,	 /* 16s ^>  8h */
	&&copy_xx12_xxxA,	 /* 16s ^>  8s */
	&&copy_xx12_xxA1,	 /* 16s ^> 16h */
	&&copy_xx12_xx1A,	 /* 16s ^> 16s */
	&&copy_xx12_xA10,	 /* 16s ^> 24h */
	&&copy_xx12_01Ax,	 /* 16s ^> 24s */
	&&copy_xx12_A100,	 /* 16s ^> 32h */
	&&copy_xx12_001A,	 /* 16s ^> 32s */
	&&copy_x123_xxx1,	 /* 24h ->  8h */
	&&copy_x123_xxx1,	 /* 24h ->  8s */
	&&copy_x123_xx12,	 /* 24h -> 16h */
	&&copy_x123_xx21,	 /* 24h -> 16s */
	&&copy_x123_x123,	 /* 24h -> 24h */
	&&copy_x123_321x,	 /* 24h -> 24s */
	&&copy_x123_1230,	 /* 24h -> 32h */
	&&copy_x123_0321,	 /* 24h -> 32s */
	&&copy_x123_xxx9,	 /* 24h ^>  8h */
	&&copy_x123_xxx9,	 /* 24h ^>  8s */
	&&copy_x123_xx92,	 /* 24h ^> 16h */
	&&copy_x123_xx29,	 /* 24h ^> 16s */
	&&copy_x123_x923,	 /* 24h ^> 24h */
	&&copy_x123_329x,	 /* 24h ^> 24s */
	&&copy_x123_9230,	 /* 24h ^> 32h */
	&&copy_x123_0329,	 /* 24h ^> 32s */
	&&copy_123x_xxx3,	 /* 24s ->  8h */
	&&copy_123x_xxx3,	 /* 24s ->  8s */
	&&copy_123x_xx32,	 /* 24s -> 16h */
	&&copy_123x_xx23,	 /* 24s -> 16s */
	&&copy_123x_x321,	 /* 24s -> 24h */
	&&copy_123x_123x,	 /* 24s -> 24s */
	&&copy_123x_3210,	 /* 24s -> 32h */
	&&copy_123x_0123,	 /* 24s -> 32s */
	&&copy_123x_xxxB,	 /* 24s ^>  8h */
	&&copy_123x_xxxB,	 /* 24s ^>  8s */
	&&copy_123x_xxB2,	 /* 24s ^> 16h */
	&&copy_123x_xx2B,	 /* 24s ^> 16s */
	&&copy_123x_xB21,	 /* 24s ^> 24h */
	&&copy_123x_12Bx,	 /* 24s ^> 24s */
	&&copy_123x_B210,	 /* 24s ^> 32h */
	&&copy_123x_012B,	 /* 24s ^> 32s */
	&&copy_1234_xxx1,	 /* 32h ->  8h */
	&&copy_1234_xxx1,	 /* 32h ->  8s */
	&&copy_1234_xx12,	 /* 32h -> 16h */
	&&copy_1234_xx21,	 /* 32h -> 16s */
	&&copy_1234_x123,	 /* 32h -> 24h */
	&&copy_1234_321x,	 /* 32h -> 24s */
	&&copy_1234_1234,	 /* 32h -> 32h */
	&&copy_1234_4321,	 /* 32h -> 32s */
	&&copy_1234_xxx9,	 /* 32h ^>  8h */
	&&copy_1234_xxx9,	 /* 32h ^>  8s */
	&&copy_1234_xx92,	 /* 32h ^> 16h */
	&&copy_1234_xx29,	 /* 32h ^> 16s */
	&&copy_1234_x923,	 /* 32h ^> 24h */
	&&copy_1234_329x,	 /* 32h ^> 24s */
	&&copy_1234_9234,	 /* 32h ^> 32h */
	&&copy_1234_4329,	 /* 32h ^> 32s */
	&&copy_1234_xxx4,	 /* 32s ->  8h */
	&&copy_1234_xxx4,	 /* 32s ->  8s */
	&&copy_1234_xx43,	 /* 32s -> 16h */
	&&copy_1234_xx34,	 /* 32s -> 16s */
	&&copy_1234_x432,	 /* 32s -> 24h */
	&&copy_1234_234x,	 /* 32s -> 24s */
	&&copy_1234_4321,	 /* 32s -> 32h */
	&&copy_1234_1234,	 /* 32s -> 32s */
	&&copy_1234_xxxC,	 /* 32s ^>  8h */
	&&copy_1234_xxxC,	 /* 32s ^>  8s */
	&&copy_1234_xxC3,	 /* 32s ^> 16h */
	&&copy_1234_xx3C,	 /* 32s ^> 16s */
	&&copy_1234_xC32,	 /* 32s ^> 24h */
	&&copy_1234_23Cx,	 /* 32s ^> 24s */
	&&copy_1234_C321,	 /* 32s ^> 32h */
	&&copy_1234_123C,	 /* 32s ^> 32s */
};
#endif

#ifdef COPY_END
while(0) {
copy_xxx1_xxx1: as_u8(dst) = as_u8(src); goto COPY_END;
copy_xxx1_xx10: as_u16(dst) = (u_int16_t)as_u8(src) << 8; goto COPY_END;
copy_xxx1_xx01: as_u16(dst) = (u_int16_t)as_u8(src); goto COPY_END;
copy_xxx1_x100: as_u32(dst) = (u_int32_t)as_u8(src) << 16; goto COPY_END;
copy_xxx1_001x: as_u32(dst) = (u_int32_t)as_u8(src) << 8; goto COPY_END;
copy_xxx1_1000: as_u32(dst) = (u_int32_t)as_u8(src) << 24; goto COPY_END;
copy_xxx1_0001: as_u32(dst) = (u_int32_t)as_u8(src); goto COPY_END;
copy_xxx1_xxx9: as_u8(dst) = as_u8(src) ^ 0x80; goto COPY_END;
copy_xxx1_xx90: as_u16(dst) = (u_int16_t)(as_u8(src) ^ 0x80) << 8; goto COPY_END;
copy_xxx1_xx09: as_u16(dst) = (u_int16_t)(as_u8(src) ^ 0x80); goto COPY_END;
copy_xxx1_x900: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80) << 16; goto COPY_END;
copy_xxx1_009x: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80) << 8; goto COPY_END;
copy_xxx1_9000: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80) << 24; goto COPY_END;
copy_xxx1_0009: as_u32(dst) = (u_int32_t)(as_u8(src) ^ 0x80); goto COPY_END;
copy_xx12_xxx1: as_u8(dst) = as_u16(src) >> 8; goto COPY_END;
copy_xx12_xx12: as_u16(dst) = as_u16(src); goto COPY_END;
copy_xx12_xx21: as_u16(dst) = bswap_16(as_u16(src)); goto COPY_END;
copy_xx12_x120: as_u32(dst) = (u_int32_t)as_u16(src) << 8; goto COPY_END;
copy_xx12_021x: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)) << 8; goto COPY_END;
copy_xx12_1200: as_u32(dst) = (u_int32_t)as_u16(src) << 16; goto COPY_END;
copy_xx12_0021: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)); goto COPY_END;
copy_xx12_xxx9: as_u8(dst) = (as_u16(src) >> 8) ^ 0x80; goto COPY_END;
copy_xx12_xx92: as_u16(dst) = as_u16(src) ^ 0x8000; goto COPY_END;
copy_xx12_xx29: as_u16(dst) = bswap_16(as_u16(src)) ^ 0x80; goto COPY_END;
copy_xx12_x920: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x8000) << 8; goto COPY_END;
copy_xx12_029x: as_u32(dst) = (u_int32_t)(bswap_16(as_u16(src)) ^ 0x80) << 8; goto COPY_END;
copy_xx12_9200: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x8000) << 16; goto COPY_END;
copy_xx12_0029: as_u32(dst) = (u_int32_t)(bswap_16(as_u16(src)) ^ 0x80); goto COPY_END;
copy_xx12_xxx2: as_u8(dst) = as_u16(src) & 0xff; goto COPY_END;
copy_xx12_x210: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)) << 8; goto COPY_END;
copy_xx12_012x: as_u32(dst) = (u_int32_t)as_u16(src) << 8; goto COPY_END;
copy_xx12_2100: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src)) << 16; goto COPY_END;
copy_xx12_0012: as_u32(dst) = (u_int32_t)as_u16(src); goto COPY_END; 
copy_xx12_xxxA: as_u8(dst) = (as_u16(src) ^ 0x80) & 0xff; goto COPY_END;
copy_xx12_xxA1: as_u16(dst) = bswap_16(as_u16(src) ^ 0x80); goto COPY_END;
copy_xx12_xx1A: as_u16(dst) = as_u16(src) ^ 0x80; goto COPY_END;
copy_xx12_xA10: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src) ^ 0x80) << 8; goto COPY_END;
copy_xx12_01Ax: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x80) << 8; goto COPY_END;
copy_xx12_A100: as_u32(dst) = (u_int32_t)bswap_16(as_u16(src) ^ 0x80) << 16; goto COPY_END;
copy_xx12_001A: as_u32(dst) = (u_int32_t)(as_u16(src) ^ 0x80); goto COPY_END;
copy_x123_xxx1: as_u8(dst) = as_u32(src) >> 16; goto COPY_END;
copy_x123_xx12: as_u16(dst) = as_u32(src) >> 8; goto COPY_END;
copy_x123_xx21: as_u16(dst) = bswap_16(as_u32(src) >> 8); goto COPY_END;
copy_x123_x123: as_u32(dst) = as_u32(src); goto COPY_END;
copy_x123_321x: as_u32(dst) = bswap_32(as_u32(src)); goto COPY_END;
copy_x123_1230: as_u32(dst) = as_u32(src) << 8; goto COPY_END;
copy_x123_0321: as_u32(dst) = bswap_32(as_u32(src)) >> 8; goto COPY_END;
copy_x123_xxx9: as_u8(dst) = (as_u32(src) >> 16) ^ 0x80; goto COPY_END;
copy_x123_xx92: as_u16(dst) = (as_u32(src) >> 8) ^ 0x8000; goto COPY_END;
copy_x123_xx29: as_u16(dst) = bswap_16(as_u32(src) >> 8) ^ 0x80; goto COPY_END;
copy_x123_x923: as_u32(dst) = as_u32(src) ^ 0x800000; goto COPY_END;
copy_x123_329x: as_u32(dst) = bswap_32(as_u32(src)) ^ 0x8000; goto COPY_END;
copy_x123_9230: as_u32(dst) = (as_u32(src) ^ 0x800000) << 8; goto COPY_END;
copy_x123_0329: as_u32(dst) = (bswap_32(as_u32(src)) >> 8) ^ 0x80; goto COPY_END;
copy_123x_xxx3: as_u8(dst) = (as_u32(src) >> 8) & 0xff; goto COPY_END;
copy_123x_xx32: as_u16(dst) = bswap_16(as_u32(src) >> 8); goto COPY_END;
copy_123x_xx23: as_u16(dst) = (as_u32(src) >> 8) & 0xffff; goto COPY_END;
copy_123x_x321: as_u32(dst) = bswap_32(as_u32(src)); goto COPY_END;
copy_123x_123x: as_u32(dst) = as_u32(src); goto COPY_END;
copy_123x_3210: as_u32(dst) = bswap_32(as_u32(src)) << 8; goto COPY_END;
copy_123x_0123: as_u32(dst) = as_u32(src) >> 8; goto COPY_END;
copy_123x_xxxB: as_u8(dst) = ((as_u32(src) >> 8) & 0xff) ^ 0x80; goto COPY_END;
copy_123x_xxB2: as_u16(dst) = bswap_16((as_u32(src) >> 8) ^ 0x80); goto COPY_END;
copy_123x_xx2B: as_u16(dst) = ((as_u32(src) >> 8) & 0xffff) ^ 0x80; goto COPY_END;
copy_123x_xB21: as_u32(dst) = bswap_32(as_u32(src)) ^ 0x800000; goto COPY_END;
copy_123x_12Bx: as_u32(dst) = as_u32(src) ^ 0x8000; goto COPY_END;
copy_123x_B210: as_u32(dst) = bswap_32(as_u32(src) ^ 0x8000) << 8; goto COPY_END;
copy_123x_012B: as_u32(dst) = (as_u32(src) >> 8) ^ 0x80; goto COPY_END;
copy_1234_xxx1: as_u8(dst) = as_u32(src) >> 24; goto COPY_END;
copy_1234_xx12: as_u16(dst) = as_u32(src) >> 16; goto COPY_END;
copy_1234_xx21: as_u16(dst) = bswap_16(as_u32(src) >> 16); goto COPY_END;
copy_1234_x123: as_u32(dst) = as_u32(src) >> 8; goto COPY_END;
copy_1234_321x: as_u32(dst) = bswap_32(as_u32(src)) << 8; goto COPY_END;
copy_1234_1234: as_u32(dst) = as_u32(src); goto COPY_END;
copy_1234_4321: as_u32(dst) = bswap_32(as_u32(src)); goto COPY_END;
copy_1234_xxx9: as_u8(dst) = (as_u32(src) >> 24) ^ 0x80; goto COPY_END;
copy_1234_xx92: as_u16(dst) = (as_u32(src) >> 16) ^ 0x8000; goto COPY_END;
copy_1234_xx29: as_u16(dst) = bswap_16(as_u32(src) >> 16) ^ 0x80; goto COPY_END;
copy_1234_x923: as_u32(dst) = (as_u32(src) >> 8) ^ 0x800000; goto COPY_END;
copy_1234_329x: as_u32(dst) = (bswap_32(as_u32(src)) ^ 0x80) << 8; goto COPY_END;
copy_1234_9234: as_u32(dst) = as_u32(src) ^ 0x80000000; goto COPY_END;
copy_1234_4329: as_u32(dst) = bswap_32(as_u32(src)) ^ 0x80; goto COPY_END;
copy_1234_xxx4: as_u8(dst) = as_u32(src) & 0xff; goto COPY_END;
copy_1234_xx43: as_u16(dst) = bswap_16(as_u32(src)); goto COPY_END;
copy_1234_xx34: as_u16(dst) = as_u32(src) & 0xffff; goto COPY_END;
copy_1234_x432: as_u32(dst) = bswap_32(as_u32(src)) >> 8; goto COPY_END;
copy_1234_234x: as_u32(dst) = as_u32(src) << 8; goto COPY_END;
copy_1234_xxxC: as_u8(dst) = (as_u32(src) & 0xff) ^ 0x80; goto COPY_END;
copy_1234_xxC3: as_u16(dst) = bswap_16(as_u32(src) ^ 0x80); goto COPY_END;
copy_1234_xx3C: as_u16(dst) = (as_u32(src) & 0xffff) ^ 0x80; goto COPY_END;
copy_1234_xC32: as_u32(dst) = (bswap_32(as_u32(src)) >> 8) ^ 0x800000; goto COPY_END;
copy_1234_23Cx: as_u32(dst) = (as_u32(src) ^ 0x80) << 8; goto COPY_END;
copy_1234_C321: as_u32(dst) = bswap_32(as_u32(src) ^ 0x80); goto COPY_END;
copy_1234_123C: as_u32(dst) = as_u32(src) ^ 0x80; goto COPY_END;
}
#endif

#ifdef GET16_LABELS
/* src_wid src_endswap sign_toggle */
static void *get16_labels[4 * 2 * 2] = {
	&&get16_xxx1_xx10,	 /*  8h -> 16h */
	&&get16_xxx1_xx90,	 /*  8h ^> 16h */
	&&get16_xxx1_xx10,	 /*  8s -> 16h */
	&&get16_xxx1_xx90,	 /*  8s ^> 16h */
	&&get16_xx12_xx12,	 /* 16h -> 16h */
	&&get16_xx12_xx92,	 /* 16h ^> 16h */
	&&get16_xx12_xx21,	 /* 16s -> 16h */
	&&get16_xx12_xxA1,	 /* 16s ^> 16h */
	&&get16_x123_xx12,	 /* 24h -> 16h */
	&&get16_x123_xx92,	 /* 24h ^> 16h */
	&&get16_123x_xx32,	 /* 24s -> 16h */
	&&get16_123x_xxB2,	 /* 24s ^> 16h */
	&&get16_1234_xx12,	 /* 32h -> 16h */
	&&get16_1234_xx92,	 /* 32h ^> 16h */
	&&get16_1234_xx43,	 /* 32s -> 16h */
	&&get16_1234_xxC3,	 /* 32s ^> 16h */
};
#endif

#ifdef GET16_END
while(0) {
get16_xxx1_xx10: sample = (u_int32_t)as_u8(src) << 8; goto GET16_END;
get16_xxx1_xx90: sample = (u_int32_t)(as_u8(src) ^ 0x80) << 8; goto GET16_END;
get16_xx12_xx12: sample = as_u16(src); goto GET16_END;
get16_xx12_xx92: sample = as_u16(src) ^ 0x8000; goto GET16_END;
get16_xx12_xx21: sample = bswap_16(as_u16(src)); goto GET16_END;
get16_xx12_xxA1: sample = bswap_16(as_u16(src) ^ 0x80); goto GET16_END;
get16_x123_xx12: sample = as_u32(src) >> 8; goto GET16_END;
get16_x123_xx92: sample = (as_u32(src) >> 8) ^ 0x8000; goto GET16_END;
get16_123x_xx32: sample = bswap_16(as_u32(src) >> 8); goto GET16_END;
get16_123x_xxB2: sample = bswap_16((as_u32(src) >> 8) ^ 0x8000); goto GET16_END;
get16_1234_xx12: sample = as_u32(src) >> 16; goto GET16_END;
get16_1234_xx92: sample = (as_u32(src) >> 16) ^ 0x8000; goto GET16_END;
get16_1234_xx43: sample = bswap_16(as_u32(src)); goto GET16_END;
get16_1234_xxC3: sample = bswap_16(as_u32(src) ^ 0x80); goto GET16_END;
}
#endif

#ifdef PUT16_LABELS
/* dst_wid dst_endswap sign_toggle*/
static void *put16_labels[4 * 2 * 2 * 4 * 2] = {
	&&put16_xx12_xxx1,	 /* 16h ->  8h */
	&&put16_xx12_xxx9,	 /* 16h ^>  8h */
	&&put16_xx12_xxx1,	 /* 16h ->  8s */
	&&put16_xx12_xxx9,	 /* 16h ^>  8s */
	&&put16_xx12_xx12,	 /* 16h -> 16h */
	&&put16_xx12_xx92,	 /* 16h ^> 16h */
	&&put16_xx12_xx21,	 /* 16h -> 16s */
	&&put16_xx12_xx29,	 /* 16h ^> 16s */
	&&put16_xx12_x120,	 /* 16h -> 24h */
	&&put16_xx12_x920,	 /* 16h ^> 24h */
	&&put16_xx12_021x,	 /* 16h -> 24s */
	&&put16_xx12_029x,	 /* 16h ^> 24s */
	&&put16_xx12_1200,	 /* 16h -> 32h */
	&&put16_xx12_9200,	 /* 16h ^> 32h */
	&&put16_xx12_0021,	 /* 16h -> 32s */
	&&put16_xx12_0029,	 /* 16h ^> 32s */
};
#endif

#ifdef PUT16_END
while (0) {
put16_xx12_xxx1: as_u8(dst) = sample >> 8; goto PUT16_END;
put16_xx12_xxx9: as_u8(dst) = (sample >> 8) ^ 0x80; goto PUT16_END;
put16_xx12_xx12: as_u16(dst) = sample; goto PUT16_END;
put16_xx12_xx92: as_u16(dst) = sample ^ 0x8000; goto PUT16_END;
put16_xx12_xx21: as_u16(dst) = bswap_16(sample); goto PUT16_END;
put16_xx12_xx29: as_u16(dst) = bswap_16(sample) ^ 0x80; goto PUT16_END;
put16_xx12_x120: as_u32(dst) = (u_int32_t)sample << 8; goto PUT16_END;
put16_xx12_x920: as_u32(dst) = (u_int32_t)(sample ^ 0x8000) << 8; goto PUT16_END;
put16_xx12_021x: as_u32(dst) = (u_int32_t)bswap_16(sample) << 8; goto PUT16_END;
put16_xx12_029x: as_u32(dst) = (u_int32_t)(bswap_16(sample) ^ 0x80) << 8; goto PUT16_END;
put16_xx12_1200: as_u32(dst) = (u_int32_t)sample << 16; goto PUT16_END;
put16_xx12_9200: as_u32(dst) = (u_int32_t)(sample ^ 0x8000) << 16; goto PUT16_END;
put16_xx12_0021: as_u32(dst) = (u_int32_t)bswap_16(sample); goto PUT16_END;
put16_xx12_0029: as_u32(dst) = (u_int32_t)bswap_16(sample) ^ 0x80; goto PUT16_END;
}
#endif

#ifdef GET32_LABELS
/* src_wid src_endswap sign_toggle */
static void *get32_labels[4 * 2 * 2] = {
	&&get32_xxx1_1000,	 /*  8h -> 32h */
	&&get32_xxx1_9000,	 /*  8h ^> 32h */
	&&get32_xxx1_1000,	 /*  8s -> 32h */
	&&get32_xxx1_9000,	 /*  8s ^> 32h */
	&&get32_xx12_1200,	 /* 16h -> 32h */
	&&get32_xx12_9200,	 /* 16h ^> 32h */
	&&get32_xx12_2100,	 /* 16s -> 32h */
	&&get32_xx12_A100,	 /* 16s ^> 32h */
	&&get32_x123_1230,	 /* 24h -> 32h */
	&&get32_x123_9230,	 /* 24h ^> 32h */
	&&get32_123x_3210,	 /* 24s -> 32h */
	&&get32_123x_B210,	 /* 24s ^> 32h */
	&&get32_1234_1234,	 /* 32h -> 32h */
	&&get32_1234_9234,	 /* 32h ^> 32h */
	&&get32_1234_4321,	 /* 32s -> 32h */
	&&get32_1234_C321,	 /* 32s ^> 32h */
};
#endif

#ifdef GET32_END
while (0) {
get32_xxx1_1000: sample = (u_int32_t)as_u8(src) << 24; goto goto GET32_END;
get32_xxx1_9000: sample = (u_int32_t)(as_u8(src) ^ 0x80) << 24; goto goto GET32_END;
get32_xx12_1200: sample = (u_int32_t)as_u16(src) << 16; goto goto GET32_END;
get32_xx12_9200: sample = (u_int32_t)(as_u16(src) ^ 0x8000) << 16; goto goto GET32_END;
get32_xx12_2100: sample = (u_int32_t)bswap_16(as_u16(src)) << 16; goto goto GET32_END;
get32_xx12_A100: sample = (u_int32_t)bswap_16(as_u16(src) ^ 0x80) << 16; goto goto GET32_END;
get32_x123_1230: sample = as_u32(src) << 8; goto goto GET32_END;
get32_x123_9230: sample = (as_u32(src) << 8) ^ 0x80000000; goto goto GET32_END;
get32_123x_3210: sample = bswap_32(as_u32(src) >> 8); goto goto GET32_END;
get32_123x_B210: sample = bswap_32((as_u32(src) >> 8) ^ 0x80); goto goto GET32_END;
get32_1234_1234: sample = as_u32(src); goto goto GET32_END;
get32_1234_9234: sample = as_u32(src) ^ 0x80000000; goto goto GET32_END;
get32_1234_4321: sample = bswap_32(as_u32(src)); goto goto GET32_END;
get32_1234_C321: sample = bswap_32(as_u32(src) ^ 0x80); goto goto GET32_END;
}
#endif

#ifdef PUT32_LABELS
/* dst_wid dst_endswap sign_toggle*/
static void *put32_labels[4 * 2 * 2] = {
	&&put32_1234_xxx1,	 /* 32h ->  8h */
	&&put32_1234_xxx9,	 /* 32h ^>  8h */
	&&put32_1234_xxx1,	 /* 32h ->  8s */
	&&put32_1234_xxx9,	 /* 32h ^>  8s */
	&&put32_1234_xx12,	 /* 32h -> 16h */
	&&put32_1234_xx92,	 /* 32h ^> 16h */
	&&put32_1234_xx21,	 /* 32h -> 16s */
	&&put32_1234_xx29,	 /* 32h ^> 16s */
	&&put32_1234_x123,	 /* 32h -> 24h */
	&&put32_1234_x923,	 /* 32h ^> 24h */
	&&put32_1234_321x,	 /* 32h -> 24s */
	&&put32_1234_329x,	 /* 32h ^> 24s */
	&&put32_1234_1234,	 /* 32h -> 32h */
	&&put32_1234_9234,	 /* 32h ^> 32h */
	&&put32_1234_4321,	 /* 32h -> 32s */
	&&put32_1234_4329,	 /* 32h ^> 32s */
};
#endif

#ifdef PUT32_END
while (0) {
put32_1234_xxx1: as_u8(dst) = sample >> 24; goto PUT32_END;
put32_1234_xxx9: as_u8(dst) = (sample >> 24) ^ 0x80; goto PUT32_END;
put32_1234_xx12: as_u16(dst) = sample >> 16; goto PUT32_END;
put32_1234_xx92: as_u16(dst) = (sample >> 16) ^ 0x8000; goto PUT32_END;
put32_1234_xx21: as_u16(dst) = bswap_16(sample >> 16); goto PUT32_END;
put32_1234_xx29: as_u16(dst) = bswap_16(sample >> 16) ^ 0x80; goto PUT32_END;
put32_1234_x123: as_u32(dst) = sample >> 8; goto PUT32_END;
put32_1234_x923: as_u32(dst) = (sample >> 8) ^ 0x800000; goto PUT32_END;
put32_1234_321x: as_u32(dst) = bswap_32(sample) << 8; goto PUT32_END;
put32_1234_329x: as_u32(dst) = (bswap_32(sample) ^ 0x80) << 8; goto PUT32_END;
put32_1234_1234: as_u32(dst) = sample; goto PUT32_END;
put32_1234_9234: as_u32(dst) = sample ^ 0x80000000; goto PUT32_END;
put32_1234_4321: as_u32(dst) = bswap_32(sample); goto PUT32_END;
put32_1234_4329: as_u32(dst) = bswap_32(sample) ^ 0x80; goto PUT32_END;
}
#endif

#ifdef GET_LABELS
/* width endswap sign_toggle*/
static void *get_labels[4 * 2 * 2] = {
	&&get_s8,	/* s8  ->  s8  */
	&&get_u8,	/* u8  ->  s8  */
	&&get_s8,	/* s8  ->  s8  */
	&&get_u8,	/* u8  ->  s8  */
	&&get_s16h,	/* s16h -> s16h */
	&&get_u16h,	/* u16h -> s16h */
	&&get_s16s,	/* s16s -> s16h */
	&&get_u16s,	/* u16s -> s16h */
	&&get_s24h,	/* s24h -> s32h */
	&&get_u24h,	/* u24h -> s32h */
	&&get_s24s,	/* s24s -> s32h */
	&&get_u24s,	/* u24s -> s32h */
	&&get_s32h,	/* s32h -> s32h */
	&&get_u32h,	/* u32h -> s32h */
	&&get_s32s,	/* s32s -> s32h */
	&&get_u32s,	/* u32s -> s32h */
};
#endif

#ifdef GET_END
while (0) {
get_s8: sample = as_s8(src); goto GET_END;
get_u8: sample = (int8_t)(as_u8(src) ^ 0x80); goto GET_END;
get_s16h: sample = as_s16(src); goto GET_END;
get_u16h: sample = (int16_t)(as_u16(src) ^ 0x8000); goto GET_END;
get_s16s: sample = (int16_t)bswap_16(as_u16(src)); goto GET_END;
get_u16s: sample = (int16_t)(bswap_16(as_u16(src) ^ 0x80)); goto GET_END;
get_s24h: sample = (int32_t)(as_u32(src) << 8) / 256; goto GET_END;
get_u24h: sample = (as_u32(src) ^ 0x80000000); goto GET_END;
get_s24s: sample = (int32_t)(bswap_32(as_u32(src)) << 8) / 256; goto GET_END;
get_u24s: sample = bswap_32(as_u32(src) ^ 0x80); goto GET_END;
get_s32h: sample = as_s32(src); goto GET_END;
get_u32h: sample = as_u32(src) ^ 0x80000000; goto GET_END;
get_s32s: sample = bswap_32(as_u32(src)); goto GET_END;
get_u32s: sample = bswap_32(as_u32(src) ^ 0x80); goto GET_END;
}
#endif

#ifdef PUT_LABELS
/* width endswap sign_toggle*/
static void *put_labels[4 * 2 * 2] = {
	&&put_s8,	/* s8  ->  s8  */
	&&put_u8,	/* u8  ->  s8  */
	&&put_s8,	/* s8  ->  s8  */
	&&put_u8,	/* u8  ->  s8  */
	&&put_s16h,	/* s16h -> s16h */
	&&put_u16h,	/* u16h -> s16h */
	&&put_s16s,	/* s16s -> s16h */
	&&put_u16s,	/* u16s -> s16h */
	&&put_s24h,	/* s24h -> s32h */
	&&put_u24h,	/* u24h -> s32h */
	&&put_s24s,	/* s24s -> s32h */
	&&put_u24s,	/* u24s -> s32h */
	&&put_s32h,	/* s32h -> s32h */
	&&put_u32h,	/* u32h -> s32h */
	&&put_s32s,	/* s32s -> s32h */
	&&put_u32s,	/* u32s -> s32h */
};
#endif

#ifdef PUT_END
put_s8: as_s8(dst) = sample; goto PUT_END;
put_u8: as_u8(dst) = sample ^ 0x80; goto PUT_END;
put_s16h: as_s16(dst) = sample; goto PUT_END;
put_u16h: as_u16(dst) = sample ^ 0x8000; goto PUT_END;
put_s16s: as_s16(dst) = bswap_16(sample); goto PUT_END;
put_u16s: as_u16(dst) = bswap_16(sample ^ 0x80); goto PUT_END;
put_s24h: as_s24(dst) = sample & 0xffffff; goto PUT_END;
put_u24h: as_u24(dst) = sample ^ 0x80000000; goto PUT_END;
put_s24s: as_s24(dst) = bswap_32(sample & 0xffffff); goto PUT_END;
put_u24s: as_u24(dst) = bswap_32(sample ^ 0x80); goto PUT_END;
put_s32h: as_s32(dst) = sample; goto PUT_END;
put_u32h: as_u32(dst) = sample ^ 0x80000000; goto PUT_END;
put_s32s: as_s32(dst) = bswap_32(sample); goto PUT_END;
put_u32s: as_u32(dst) = bswap_32(sample ^ 0x80); goto PUT_END;
#endif

#undef as_u8
#undef as_u16
#undef as_u32
#undef as_s8
#undef as_s16
#undef as_s32
