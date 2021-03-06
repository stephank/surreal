/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2011 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _SDL_config_h
#define _SDL_config_h

#include "SDL_platform.h"

/**
 *  \file SDL_config.h
 */

/* Contains build configuration specific to Unreal Tournament. */

/* We don't use these subsystems. */
/* FIXME: See if we can reduce on compiled objects. */
#define SDL_ATOMIC_DISABLED 1
#define SDL_AUDIO_DISABLED 1
#define SDL_CPUINFO_DISABLED 1
/* #undef SDL_EVENTS_DISABLED */
/* #undef SDL_FILE_DISABLED */
#define SDL_JOYSTICK_DISABLED 1
#define SDL_HAPTIC_DISABLED 1
/* #undef SDL_LOADSO_DISABLED */
#define SDL_RENDER_DISABLED 1
#define SDL_THREADS_DISABLED 1
#define SDL_TIMERS_DISABLED 1
/* #undef SDL_VIDEO_DISABLED */
#define SDL_POWER_DISABLED 1

/* Configurations for platforms we support in Unreal Tournament. */
#if defined(__WIN32__)
# include "SDL_config_windows.h"
#elif defined(__MACOSX__)
# include "SDL_config_macosx.h"
#elif defined(__LINUX__)
# include "SDL_config_linux.h"
#else
# error Unsupported platform for SDL gyp build
#endif

#endif /* _SDL_config_h */
