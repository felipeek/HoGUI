#pragma once

typedef float r32;
typedef double r64;
typedef char s8;
typedef short s16;
typedef int s32;
typedef long long int s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;
typedef int bool;

#define true 1
#define false 0

#define ARRAY_SIZE(X) (sizeof(X) / sizeof(*X))