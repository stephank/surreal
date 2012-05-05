#define AL_API  __attribute__((visibility("protected")))
#define ALC_API __attribute__((visibility("protected")))

#define ALSOFT_VERSION "1.14"

#define HAVE_ALSA
#define HAVE_PULSEAUDIO

#define HAVE_DLFCN_H
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
#define HAVE_STRTOF
#define HAVE_STDINT_H
#define HAVE_GCC_DESTRUCTOR
#define HAVE_GCC_FORMAT
#define HAVE_FLOAT_H
#define HAVE_FPU_CONTROL_H
#define HAVE_FENV_H
#define HAVE_FESETROUND
#define HAVE_PTHREAD_SETSCHEDPARAM
#define HAVE___RESTRICT

#ifdef __LP64__
#define SIZEOF_LONG 8
#else
#define SIZEOF_LONG 4
#endif
#define SIZEOF_LONG_LONG 8
