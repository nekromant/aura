#ifndef ENDIAN_H
#define ENDIAN_H

enum aura_endianness
{
	AURA_ENDIAN_LITTLE=0,
	AURA_ENDIAN_BIG,
};


#define __swap16(value)                                 \
        ((((uint16_t)((value) & 0x00FF)) << 8) |        \
         (((uint16_t)((value) & 0xFF00)) >> 8))

#define __swap32(value)                                 \
        ((((uint32_t)((value) & 0x000000FF)) << 24) |   \
         (((uint32_t)((value) & 0x0000FF00)) << 8) |    \
         (((uint32_t)((value) & 0x00FF0000)) >> 8) |    \
         (((uint32_t)((value) & 0xFF000000)) >> 24))

#define __swap64(value)                                         \
        (((((uint64_t)value)<<56) & 0xFF00000000000000ULL)  |     \
         ((((uint64_t)value)<<40) & 0x00FF000000000000ULL)  |     \
         ((((uint64_t)value)<<24) & 0x0000FF0000000000ULL)  |     \
         ((((uint64_t)value)<< 8) & 0x000000FF00000000ULL)  |     \
         ((((uint64_t)value)>> 8) & 0x00000000FF000000ULL)  |     \
         ((((uint64_t)value)>>24) & 0x0000000000FF0000ULL)  |     \
         ((((uint64_t)value)>>40) & 0x000000000000FF00ULL)  |     \
         ((((uint64_t)value)>>56) & 0x00000000000000FFULL))

#endif
