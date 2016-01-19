#ifndef MAT_ARCH_CC
#define MAT_ARCH_CC

#include <inttypes.h>
#include <stdio.h>
#include <stdarg.h>

extern int ser_printf(const char* s, ...);

#define BYTE_ORDER LITTLE_ENDIAN

typedef int8_t s8_t;
typedef int16_t s16_t;
typedef int32_t s32_t;
typedef uint8_t u8_t;
typedef uint16_t u16_t;
typedef uint32_t u32_t;

typedef uintptr_t mem_ptr_t;

#define LWIP_ERR_T int

#define U16_F "hu"
#define S16_F "hd"
#define X16_F "hx"
#define U32_F "lu"
#define S32_F "ld"
#define X32_F "lx"

#define PACK_STRUCT_FIELD(x)    x
#define PACK_STRUCT_STRUCT  __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#define LWIP_PLATFORM_ASSERT(x)
// ser_printf(x)
#define LWIP_PLATFORM_DIAG(x)
// do {ser_printf x;} while(0)

#endif
