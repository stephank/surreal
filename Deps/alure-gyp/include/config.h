#define ALURE_VER_MAJOR 1
#define ALURE_VER_MINOR 2


#if defined(LINUX)

#ifndef ALURE_STATIC_LIBRARY
#define ALURE_API __attribute__((visibility("protected")))
#endif

#define HAVE_GCC_CONSTRUCTOR
#define HAVE_GCC_VISIBILITY

#define HAVE_SYS_WAIT_H
#define HAVE_FSEEKO
#define HAVE_NANOSLEEP


#elif defined(WIN32)

#ifndef ALURE_STATIC_LIBRARY
#define ALURE_API __declspec(dllexport)
#endif

#define HAVE_WINDOWS_H
#define HAVE__FSEEKI64


#else

#error Unsupported platform for ALURE gyp build

#endif


#define HAVE_SYS_TYPES_H
#define HAVE_SIGNAL_H
