#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16 __attribute__((aligned(1), may_alias));
typedef int16_t  s16 __attribute__((aligned(1), may_alias));
typedef uint32_t u32 __attribute__((aligned(1), may_alias));
typedef int32_t  s32 __attribute__((aligned(1), may_alias));
