/*=============================================================================
	ALAudioMusic.cpp: MikMod driver for Unreal OpenAL audio output.

Revision history:
	* Created by Stéphan Kochen.
=============================================================================*/

#include "ALAudioMusic.h"

/*------------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------------*/

ALuint MusicSource;

/*------------------------------------------------------------------------------------
	Driver implementation
------------------------------------------------------------------------------------*/

static BOOL UnMM_IsPresent()
{
	return 1;
}

static BOOL UnMM_Init()
{
	// FIXME
	return 0;
}

static void UnMM_Exit()
{
	// FIXME
}

static BOOL UnMM_Reset()
{
	// FIXME
	return 0;
}

static void UnMM_Update()
{
	// FIXME
}

/*------------------------------------------------------------------------------------
	Driver definition
------------------------------------------------------------------------------------*/

MDRIVER MusicDriver = {
	NULL,
	"Unreal OpenAL",
	"Unreal OpenAL driver",
	0,
	255,
	"unreal",
	NULL,
	UnMM_IsPresent,
	VC_SampleLoad,
	VC_SampleUnload,
	VC_SampleSpace,
	VC_SampleLength,
	UnMM_Init,
	UnMM_Exit,
	UnMM_Reset,
	VC_SetNumVoices,
	VC_PlayStart,
	VC_PlayStop,
	UnMM_Update,
	NULL,
	VC_VoiceSetVolume,
	VC_VoiceGetVolume,
	VC_VoiceSetFrequency,
	VC_VoiceGetFrequency,
	VC_VoiceSetPanning,
	VC_VoiceGetPanning,
	VC_VoicePlay,
	VC_VoiceStop,
	VC_VoiceStopped,
	VC_VoiceGetPosition,
	VC_VoiceRealVolume
};
