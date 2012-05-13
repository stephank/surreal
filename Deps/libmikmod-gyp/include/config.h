#define HAVE_FCNTL_H 1

#if !defined(__APPLE__)
#define HAVE_MALLOC_H 1
#endif

#if !defined(WIN32)
#define HAVE_INTTYPES_H 1
#define HAVE_MEMORY_H 1
#define HAVE_SETENV 1
#define HAVE_SNPRINTF 1
#define HAVE_SRANDOM 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRCASECMP 1
#define HAVE_STRDUP 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_STRSTR 1
#define HAVE_SYS_IOCTL_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_UNISTD_H 1
#define STDC_HEADERS 1
#endif

#if defined(__APPLE__)
# ifdef __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#elif !defined(WIN32)
# include <endian.h>
# if __BYTE_ORDER == __BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
# endif
#endif
