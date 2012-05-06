#ifndef ALURE_STATIC_LIBRARY
#define ALURE_API __attribute__((visibility("protected")))
#endif

#define ALURE_VER_MAJOR 1
#define ALURE_VER_MINOR 2

#define HAVE_GCC_CONSTRUCTOR
#define HAVE_GCC_VISIBILITY

#define HAVE_SYS_TYPES_H
#define HAVE_SYS_WAIT_H
#define HAVE_SIGNAL_H
#define HAVE_NANOSLEEP
#define HAVE_FSEEKO
