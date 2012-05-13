#define ALURE_VER_MAJOR 1
#define ALURE_VER_MINOR 2


/* Exports */

#if !defined(ALURE_STATIC_LIBRARY)
# if defined(WIN32)
#  define ALURE_API __declspec(dllexport)
# else
#  define ALURE_API __attribute__((visibility("protected")))
# endif
#endif


/* Platform support */

#if defined(WIN32)
# define HAVE_WINDOWS_H
# define HAVE__FSEEKI64
#else
# define HAVE_GCC_CONSTRUCTOR
# define HAVE_GCC_VISIBILITY
# define HAVE_SYS_WAIT_H
# define HAVE_FSEEKO
# define HAVE_NANOSLEEP
#endif

#define HAVE_SYS_TYPES_H
#define HAVE_SIGNAL_H
