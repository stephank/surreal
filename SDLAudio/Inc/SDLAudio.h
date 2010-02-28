/*=============================================================================
	Audio.h: Unreal audio public header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_SDLAUDIO
#define _INC_SDLAUDIO

/*----------------------------------------------------------------------------
	API.
----------------------------------------------------------------------------*/

#ifndef SDLAUDIO_API
	#define SDLAUDIO_API DLL_IMPORT
#endif

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "Engine.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	Audio compiler specific includes.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	Audio public includes.
-----------------------------------------------------------------------------*/

#include "SDLAudioLibrary.h"
#include "SDLAudioSubsystem.h"
#include "AudioCoreSDL.h"
extern "C" {
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
}
		
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

/*
  Some helpful debugging crap I tossed in to make guard et al. do something
  useful.  I tried compiling with _DEBUG and _REALLY_WANT_DEBUG defined but
  it didn't seem to have any effect...

  Currently commented out.  Probably never even going to use it again but why
  not leave it in?
 */

#if 0

#undef  guard
#define guard(str) { static const char *___guardmsg = TEXT(#str); \
                     debugf(NAME_Init, "== --> %s ==", ___guardmsg);
#undef  unguard
#define unguard      debugf(NAME_Init, "== <-- %s ==", ___guardmsg); }

#endif


#endif
