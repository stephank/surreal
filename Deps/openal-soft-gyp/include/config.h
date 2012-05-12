#define ALSOFT_VERSION "1.14"


#if defined(WIN32)

#define AL_API  __declspec(dllexport)
#define ALC_API __declspec(dllexport)

#define HAVE_MMDEVAPI
#define HAVE_DSOUND
#define HAVE_WINMM

#define HAVE_GUIDDEF_H
#define HAVE__CONTROLFP


#elif defined(LINUX)

#define AL_API  __attribute__((visibility("protected")))
#define ALC_API __attribute__((visibility("protected")))

#define HAVE_ALSA
#define HAVE_PULSEAUDIO

#define HAVE_DLFCN_H
#define HAVE_STRTOF
#define HAVE_GCC_DESTRUCTOR
#define HAVE_GCC_FORMAT
#define HAVE_FPU_CONTROL_H
#define HAVE_FENV_H
#define HAVE_FESETROUND
#define HAVE_PTHREAD_SETSCHEDPARAM


#else

#error Unsupported platform for OpenAL Soft gyp build

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

#define SIZEOF_LONG sizeof(long)
#define SIZEOF_LONG_LONG sizeof(long long)
