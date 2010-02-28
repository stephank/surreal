/*=============================================================================
	AudioCoreSDL.h: Core audio implementation for SDL.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jeremy Muhlich
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "SDLAudioPrivate.h"

/*------------------------------------------------------------------------------------
	AudioCore Globals.
------------------------------------------------------------------------------------*/

#define FRAGMENT_SIZE 0x0000400

INT SDL_Initialized = 0;
INT Mix_Initialized = 0;

/*------------------------------------------------------------------------------------
	AudioCore Implementation.
------------------------------------------------------------------------------------*/

INT OpenAudio( INT Rate, _WORD Format, INT PhysChannels )
{
	/* Initialize the SDL library */
	if ( !SDL_Initialized ) 
	{
		if ( SDL_Init(SDL_INIT_AUDIO) < 0 ) 
		{
			debugf( NAME_Init, TEXT("Couldn't initialize SDL: %s"), SDL_GetError() );
            		return 0;
		} else {
			SDL_Initialized = 1;
		}
	} else {
	  debugf( NAME_Init, TEXT("OpenAudio: SDL already initialized"));
	}

	if ( !Mix_Initialized ) 
	{
                if ( Mix_OpenAudio(Rate, Format, PhysChannels, FRAGMENT_SIZE) < 0) 
		{
			debugf( NAME_Init, TEXT("Failed to open audio device: %s"), SDL_GetError() );
			return 0;
		} else {
			INT audio_rate;
			_WORD audio_format;
			INT audio_channels;
      
			Mix_Initialized = 1;
			Mix_QuerySpec(&audio_rate, &audio_format, &audio_channels);
			debugf( NAME_Init, TEXT("Opened audio device at %d Hz %d bit %s"),
				audio_rate, (audio_format&0xFF),
				(audio_channels > 1) ? "stereo" : "mono" );
		}
	} else {
	  debugf( NAME_Init, TEXT("OpenAudio: Mixer already initialized"));
	}

	return 1;
}

INT ReopenAudioDevice( INT Rate, _WORD Format, INT PhysChannels )
{
	//ALock;
  /*
	CloseAudio();
	INT Result = OpenAudio( Rate, Format, PhysChannels );
	//AUnlock;

	return Result;
  */

  /*
    SDL_mixer hangs if you try to init/deinit/init, so we'll just pretend
    we actually reopened the audio device and UT will never even know...
   */
  return 1;
}

void CloseAudio()
{
	Mix_CloseAudio();
	Mix_Initialized = 0;
}

// Audio flow control.
void PlayAudio()
{
	/* won't enter this routine till enough space, see AudioWait */
	return;
}
