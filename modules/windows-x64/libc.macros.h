#pragma once
#define _MT
#  define _M_AMD64 100
#  define _M_X64 100
#define assert(_Expression) ((void)0)
#define LC_ALL 0
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5
#define ERANGE 34
#define INT32_MAX 2147483647
#define INT64_MAX 9223372036854775807LL
#define WCHAR_MAX 0xffffU
#define SEEK_END 2
#define _IONBF 0x0004
#define errno (*_errno())
#define stdout (__acrt_iob_func(1))
#define stderr (__acrt_iob_func(2))
#define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#define offsetof(t, d) __builtin_offsetof(t, d)
#define LONG_MAX  __LONG_MAX__
#define CHAR_BIT  __CHAR_BIT__
