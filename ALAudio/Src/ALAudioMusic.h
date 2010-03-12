/*=============================================================================
	ALAudioMusic.cpp: MikMod driver for Unreal OpenAL audio output.

Revision history:
	* Created by Stéphan Kochen.
=============================================================================*/

#ifdef _WIN32
#include <al.h>
#elif defined(__APPLE__)
#include <OpenAL/al.h>
#else
#include <AL/al.h>
#endif
#include <mikmod.h>
#include "Core.h"

extern MDRIVER MusicDriver;
extern ALuint MusicSource;

MREADER* BuildMikModMemoryReader( BYTE* Data, INT Length );
void DestroyMikModMemoryReader( MREADER* Reader );
