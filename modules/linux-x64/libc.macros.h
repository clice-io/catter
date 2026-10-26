#pragma once
#define	ERANGE		34	/* Math result not representable */
# define __ASSERT_VOID_CAST static_cast<void>
# define assert(expr)		(__ASSERT_VOID_CAST (0))
#   define errno (*__errno_location ())
#define __LC_CTYPE		 0
#define __LC_NUMERIC		 1
#define __LC_TIME		 2
#define __LC_COLLATE		 3
#define __LC_MONETARY		 4
#define __LC_MESSAGES		 5
#define __LC_ALL		 6
#define __LC_PAPER		 7
#define __LC_NAME		 8
#define __LC_ADDRESS		 9
#define __LC_TELEPHONE		10
#define __LC_MEASUREMENT	11
#define __LC_IDENTIFICATION	12
# define __PTHREAD_SPINS             0, 0
# define sa_handler	__sigaction_handler.sa_handler
# define sa_sigaction	__sigaction_handler.sa_sigaction
#define SA_SIGINFO    4		 /* Invoke signal-catching function with
				    three arguments instead of one.  */
# define SA_RESTART   0x10000000 /* Restart syscall on signal return.  */
#define	SIG_UNBLOCK   1		 /* Unblock signals.  */
# define si_pid		_sifields._kill.si_pid
#define SIG_DFL	((__sighandler_t) 0)		/* Default action.  */
#define SIG_IGN	((__sighandler_t) 1)		/* Ignore signal.  */
#define	SIGHUP		1	/* Hangup (POSIX).  */
#define	SIGINT		2	/* Interrupt (ANSI).  */
#define	SIGKILL		9	/* Kill, unblockable (POSIX).  */
#define	SIGTERM		15	/* Termination (ANSI).  */
#define	__GLIBC__	2
#define	__GLIBC_MINOR__	17
#define __GLIBC_PREREQ(maj, min) 	((__GLIBC__ << 16) + __GLIBC_MINOR__ >= ((maj) << 16) + (min))
#define	LC_ALL		  __LC_ALL
# define LC_CTYPE_MASK		(1 << __LC_CTYPE)
# define LC_NUMERIC_MASK	(1 << __LC_NUMERIC)
# define LC_TIME_MASK		(1 << __LC_TIME)
# define LC_COLLATE_MASK	(1 << __LC_COLLATE)
# define LC_MONETARY_MASK	(1 << __LC_MONETARY)
# define LC_MESSAGES_MASK	(1 << __LC_MESSAGES)
# define LC_PAPER_MASK		(1 << __LC_PAPER)
# define LC_NAME_MASK		(1 << __LC_NAME)
# define LC_ADDRESS_MASK	(1 << __LC_ADDRESS)
# define LC_TELEPHONE_MASK	(1 << __LC_TELEPHONE)
# define LC_MEASUREMENT_MASK	(1 << __LC_MEASUREMENT)
# define LC_IDENTIFICATION_MASK	(1 << __LC_IDENTIFICATION)
# define LC_ALL_MASK		(LC_CTYPE_MASK 				 | LC_NUMERIC_MASK 				 | LC_TIME_MASK 				 | LC_COLLATE_MASK 				 | LC_MONETARY_MASK 				 | LC_MESSAGES_MASK 				 | LC_PAPER_MASK 				 | LC_NAME_MASK 				 | LC_ADDRESS_MASK 				 | LC_TELEPHONE_MASK 				 | LC_MEASUREMENT_MASK 				 | LC_IDENTIFICATION_MASK 				 )
# define PTHREAD_MUTEX_INITIALIZER   { { 0, 0, 0, 0, 0, __PTHREAD_SPINS, { 0, 0 } } }
#define PTHREAD_COND_INITIALIZER { { 0, 0, 0, 0, 0, (void *) 0, 0, 0 } }
#define PTHREAD_ONCE_INIT 0
#  define __INT64_C(c)	c ## L
# define INT32_MAX		(2147483647)
# define INT64_MAX		(__INT64_C(9223372036854775807))
#define _IONBF 2		/* No buffering.  */
#define SEEK_END	2	/* Seek from end of file.  */
#define stdout stdout
#define stderr stderr
#define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#define offsetof(t, d) __builtin_offsetof(t, d)
#define LONG_MAX  __LONG_MAX__
#define CHAR_BIT  __CHAR_BIT__
