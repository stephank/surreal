#define ALSOFT_VERSION "1.14"


/* Exports */

#if defined(WIN32)
# define AL_API  __declspec(dllexport)
# define ALC_API __declspec(dllexport)
#else
# define AL_API  __attribute__((visibility("protected")))
# define ALC_API __attribute__((visibility("protected")))
#endif


/* Backends */

#if defined(WIN32)
# define HAVE_MMDEVAPI
# define HAVE_DSOUND
# define HAVE_WINMM
#elif defined(LINUX)
# define HAVE_ALSA
# define HAVE_PULSEAUDIO
#elif defined(__APPLE__)
# define HAVE_COREAUDIO
#else
# error Unsupported platform for OpenAL Soft gyp build
#endif


/* Platform support */

#if defined(WIN32)
# define HAVE_GUIDDEF_H
# define HAVE__CONTROLFP
#elif defined(LINUX)
# define HAVE_DLFCN_H
# define HAVE_FPU_CONTROL_H
#elif defined(__APPLE__)
# define HAVE_RESTRICT
#endif

#if !defined(WIN32)
# define HAVE_STRTOF
# define HAVE_GCC_DESTRUCTOR
# define HAVE_GCC_FORMAT
# define HAVE_FENV_H
# define HAVE_FESETROUND
# define HAVE_PTHREAD_SETSCHEDPARAM
#endif

#define HAVE_STAT
#define HAVE_POWF
#define HAVE_SQRTF
#define HAVE_COSF
#define HAVE_SINF
#define HAVE_ACOSF
#define HAVE_ASINF
#define HAVE_ATANF
#define HAVE_ATAN2F
#define HAVE_FABSF
#define HAVE_LOG10F
#define HAVE_FLOORF
#define HAVE_STDINT_H
#define HAVE_FLOAT_H
#define HAVE___RESTRICT
