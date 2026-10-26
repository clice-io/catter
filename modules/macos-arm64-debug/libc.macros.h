#pragma once
#define LC_ALL_MASK			(  LC_COLLATE_MASK 					 | LC_CTYPE_MASK 					 | LC_MESSAGES_MASK 					 | LC_MONETARY_MASK 					 | LC_NUMERIC_MASK 					 | LC_TIME_MASK )
#define LC_COLLATE_MASK			(1 << 0)
#define LC_CTYPE_MASK			(1 << 1)
#define LC_MESSAGES_MASK		(1 << 2)
#define LC_MONETARY_MASK		(1 << 3)
#define LC_NUMERIC_MASK			(1 << 4)
#define LC_TIME_MASK			(1 << 5)
#define	_IONBF	2		/* setvbuf should set unbuffered */
#define	stdout	__stdoutp
#define	stderr	__stderrp
#define __ASSERT_FILE_NAME __FILE_NAME__
#define	assert(e)     (__builtin_expect(!(e), 0) ? __assert_rtn(__func__, __ASSERT_FILE_NAME, __LINE__, #e) : (void)0)
#define	LC_ALL		0
#define PTHREAD_MUTEX_INITIALIZER {_PTHREAD_MUTEX_SIG_init, {0}}
#define PTHREAD_COND_INITIALIZER {_PTHREAD_COND_SIG_init, {0}}
#define PTHREAD_ONCE_INIT {_PTHREAD_ONCE_SIG_init, {0}}
#define _PTHREAD_MUTEX_SIG_init		0x32AAABA7
#define _PTHREAD_COND_SIG_init		0x3CB0B1BB
#define _PTHREAD_ONCE_SIG_init		0x30B1BCBA
#define INT32_MAX        2147483647
#define INT64_MAX        9223372036854775807LL
#    define WCHAR_MAX     __WCHAR_MAX__
#define SEEK_END        2       /* set file offset to EOF plus offset */
#define errno (*__error())
#define ERANGE          34              /* Result too large */
#define SIGHUP  1       /* hangup */
#define SIGINT  2       /* interrupt */
#define SIGKILL 9       /* kill (cannot be caught or ignored) */
#define SIGTERM 15      /* software termination signal from kill */
#define SIG_DFL         (void (*)(int))0
#define SIG_IGN         (void (*)(int))1
#define sa_handler      __sigaction_u.__sa_handler
#define sa_sigaction    __sigaction_u.__sa_sigaction
#define SA_RESTART      0x0002  /* restart system on signal return */
#define SA_SIGINFO      0x0040  /* signal handler with SA_SIGINFO args */
#define SIG_UNBLOCK     2       /* unblock specified signal set */
#define _LIBCPP_ABI_NAMESPACE __1
#define _LIBCPP_HARDENING_MODE_DEFAULT 2
#define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#define offsetof(t, d) __builtin_offsetof(t, d)
#define vcreate_u8(__p0) __extension__ ({   uint8x8_t __ret;   uint64_t __promote = __p0;   __ret = __builtin_bit_cast(uint8x8_t, __promote);   __ret; })
#define vextq_u8(__p0, __p1, __p2) __extension__ ({   uint8x16_t __ret;   uint8x16_t __s0 = __p0;   uint8x16_t __s1 = __p1;   __ret = __builtin_bit_cast(uint8x16_t, __builtin_neon_vextq_v(__builtin_bit_cast(int8x16_t, __s0), __builtin_bit_cast(int8x16_t, __s1), __p2, 48));   __ret; })
#define vextq_s8(__p0, __p1, __p2) __extension__ ({   int8x16_t __ret;   int8x16_t __s0 = __p0;   int8x16_t __s1 = __p1;   __ret = __builtin_bit_cast(int8x16_t, __builtin_neon_vextq_v(__builtin_bit_cast(int8x16_t, __s0), __builtin_bit_cast(int8x16_t, __s1), __p2, 32));   __ret; })
#define vgetq_lane_u64(__p0, __p1) __extension__ ({   uint64_t __ret;   uint64x2_t __s0 = __p0;   __ret = __builtin_bit_cast(uint64_t, __builtin_neon_vgetq_lane_i64(__builtin_bit_cast(int64x2_t, __s0), __p1));   __ret; })
#define vgetq_lane_u16(__p0, __p1) __extension__ ({   uint16_t __ret;   uint16x8_t __s0 = __p0;   __ret = __builtin_bit_cast(uint16_t, __builtin_neon_vgetq_lane_i16(__builtin_bit_cast(int16x8_t, __s0), __p1));   __ret; })
#define vget_lane_u64(__p0, __p1) __extension__ ({   uint64_t __ret;   uint64x1_t __s0 = __p0;   __ret = __builtin_bit_cast(uint64_t, __builtin_neon_vget_lane_i64(__builtin_bit_cast(int64x1_t, __s0), __p1));   __ret; })
#define vld1q_u8(__p0) __extension__ ({   uint8x16_t __ret;   __ret = __builtin_bit_cast(uint8x16_t, __builtin_neon_vld1q_v(__p0, 48));   __ret; })
#define vld1q_s8(__p0) __extension__ ({   int8x16_t __ret;   __ret = __builtin_bit_cast(int8x16_t, __builtin_neon_vld1q_v(__p0, 32));   __ret; })
#define vshlq_n_u8(__p0, __p1) __extension__ ({   uint8x16_t __ret;   uint8x16_t __s0 = __p0;   __ret = __builtin_bit_cast(uint8x16_t, __builtin_neon_vshlq_n_v(__builtin_bit_cast(int8x16_t, __s0), __p1, 48));   __ret; })
#define vshrq_n_u8(__p0, __p1) __extension__ ({   uint8x16_t __ret;   uint8x16_t __s0 = __p0;   __ret = __builtin_bit_cast(uint8x16_t, __builtin_neon_vshrq_n_v(__builtin_bit_cast(int8x16_t, __s0), __p1, 48));   __ret; })
#define vshrn_n_u16(__p0, __p1) __extension__ ({   uint8x8_t __ret;   uint16x8_t __s0 = __p0;   __ret = __builtin_bit_cast(uint8x8_t, __builtin_neon_vshrn_n_v(__builtin_bit_cast(int8x16_t, __s0), __p1, 16));   __ret; })
#define vst1q_u8(__p0, __p1) __extension__ ({   uint8x16_t __s1 = __p1;   __builtin_neon_vst1q_v(__p0, __builtin_bit_cast(int8x16_t, __s1), 48); })
#define vst1q_s8(__p0, __p1) __extension__ ({   int8x16_t __s1 = __p1;   __builtin_neon_vst1q_v(__p0, __builtin_bit_cast(int8x16_t, __s1), 32); })
#define vst1_u8(__p0, __p1) __extension__ ({   uint8x8_t __s1 = __p1;   __builtin_neon_vst1_v(__p0, __builtin_bit_cast(int8x8_t, __s1), 16); })
#define LONG_MAX  __LONG_MAX__
#define CHAR_BIT  __CHAR_BIT__
