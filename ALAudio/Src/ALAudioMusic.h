/*=============================================================================
	ALAudioMusic.cpp: MikMod driver for Unreal OpenAL audio output.

Revision history:
	* Created by Stéphan Kochen.
=============================================================================*/

#include "AL/al.h"
#include <mikmod.h>
#include "Core.h"

extern MDRIVER MusicDriver;
extern ALuint MusicSource;

MREADER* BuildMikModMemoryReader( BYTE* Data, INT Length );
void DestroyMikModMemoryReader( MREADER* Reader );
