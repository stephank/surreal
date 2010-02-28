/*=============================================================================
	AudioLibrary.cpp: Unreal audio interface object.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jeremy Muhlich.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Audio includes.
------------------------------------------------------------------------------------*/

#include "SDLAudioPrivate.h"

/*------------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------------*/

// Raw Buffer
void* 		AudioBuffer=NULL;
void*		MixBuffer=NULL;
INT   		BufferSize=0;
INT		AudioFormat;
INT		AudioRate;

// State
INT   		AudioInitialized=0;
INT		AudioPaused=0;

// Audio
INT             MusicVoices;
INT		SampleVoices;

// Volume
FLOAT		CurSampleVolume;

// Threads
AudioThread	MixingThread;
AudioMutex	Mutex;

/*------------------------------------------------------------------------------------
	Library control.
------------------------------------------------------------------------------------*/

// Start the audio system.
UBOOL AudioInit( INT Rate, _WORD Format, INT PhysChannels )
{
	if (AudioInitialized)
		appErrorf( TEXT("Soundsystem already initialized!") );

	if (OpenAudio(Rate, Format, PhysChannels) == 0)
		return 0;

	AudioPaused = 1;
	AudioInitialized = 1;

	MusicVoices = 48;
	AllocateVoices(16);

	return 1;
}

// Restart the audio system.
UBOOL AudioReinit( INT Rate, _WORD Format, INT PhysChannels )
{
	AudioInitialized = 0;
	INT Result = ReopenAudioDevice( Rate, Format, PhysChannels );
	AudioInitialized = 1;

	return Result;
}

// Stop the audio system.
UBOOL AudioShutdown()
{
	AudioInitialized = 0;
	//DestroyAudioThread(&MixingThread);	
	CloseAudio();
	//DestroyAudioMutex(&Mutex);
	return 1;
}

// Allocate voices.
UBOOL AllocateVoices( INT RequestedChannels )
{
        CheckAudioLib(0);

        if (RequestedChannels + MusicVoices <= AUDIO_TOTALVOICES)
        {
                SampleVoices = Mix_AllocateChannels(RequestedChannels + MusicVoices);
                return 1;
        }
        return 0;
}

/*------------------------------------------------------------------------------------
	Master control.
------------------------------------------------------------------------------------*/

// Start [Unpause] all playing audio.
UBOOL AudioStartOutput( INT Rate, _WORD Format, INT PhysChannels )
{
	CheckAudioLib(0);

	//ALock;	
	AudioReinit( Rate, Format, PhysChannels );
	AudioPaused = 0;
	//AUnlock;

	return 1;
}

// Stop [Pause] all playing audio.
UBOOL AudioStopOutput()
{
	AudioPaused = 1;
	return (Mix_HaltMusic() == 0 && Mix_HaltChannel(-1) == 0);

  /* REF
	ALock;
	AudioPaused = 1;
	for (INT i=0; i++; i<AUDIO_TOTALVOICES)
	{
		if (Voices[i].State & VOICE_ACTIVE)
		{
			Voices[i].PlayPosition = 0;
			Voices[i].State &= ~VOICE_ACTIVE;
		}
	}
	AUnlock;

	return 1;
  */
}

/*------------------------------------------------------------------------------------
        Sample control.
------------------------------------------------------------------------------------*/

// Change a sample.
/*
void UpdateSample( Voice* InVoice, INT Freq, INT Volume, INT Panning )
{
        ALock;
        InVoice->Volume = Volume;
        InVoice->Panning = InVoice->Panning + ((Panning - InVoice->Panning)/2);
        InVoice->BasePanning = InVoice->Panning;
        AUnlock;
}
*/

/*------------------------------------------------------------------------------------
	Volume control.
------------------------------------------------------------------------------------*/

UBOOL SetSampleVolume( FLOAT Volume )
{
	CurSampleVolume = Volume;
	return 1;
}

UBOOL SetMusicVolume( FLOAT Volume )
{
	Mix_VolumeMusic((int) (AUDIO_MAXVOLUME*Volume));
	return 1;
}

UBOOL SetCDAudioVolume( FLOAT Volume )
{
	return 1;
}

/*------------------------------------------------------------------------------------
	CD Audio control.
------------------------------------------------------------------------------------*/

UBOOL StartCDAudio( INT Track )
{
	return 1;
}

UBOOL StopCDAudio()
{
	return 1;
}
