/*=============================================================================
	ALAudioMusic.cpp: MikMod driver for Unreal OpenAL audio output.

Revision history:
	* Created by Stéphan Kochen.
=============================================================================*/

#include "AL/al.h"
#include <mikmod.h>

extern MDRIVER MusicDriver;
extern ALuint MusicSource;
