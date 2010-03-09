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
	Memory reader
------------------------------------------------------------------------------------*/

typedef struct _MikModMemoryReader
{
	MREADER Base;

	BYTE* Data;
	INT Length;

	INT Position;
} MikModMemoryReader;

#define DEFINE_READER MikModMemoryReader* Reader = (MikModMemoryReader*)Base;

// fseek equivalent
static BOOL MMMR_Seek( MREADER* Base, long Offset, int Whence )
{
	DEFINE_READER;
	switch( Whence )
	{
	case SEEK_SET:
		Reader->Position = Offset;
		break;
	case SEEK_CUR:
		Reader->Position += Offset;
		break;
	case SEEK_END:
		Reader->Position = Reader->Length - Offset;
		break;
	default:
		return -1;
	}
	return 0;
}

// ftell equivalent
static long MMMR_Tell( MREADER* Base )
{
	DEFINE_READER;
	return Reader->Position;
}

// fread equivalent
static BOOL MMMR_Read( MREADER* Base, void* Ptr, size_t Amount)
{
	DEFINE_READER;
	INT Remaining = Reader->Length - Reader->Position;
	if( Amount > Remaining ) Amount = Remaining;
	memcpy( Ptr, &Reader->Data[Reader->Position], Amount );
	Reader->Position += Amount;
	return Amount;
}

// fgetc equivalent
static int MMMR_Get( MREADER* Base )
{
	DEFINE_READER;
	return Reader->Data[Reader->Position++];
}

// feof equivalent
static BOOL MMMR_Eof( MREADER* Base )
{
	DEFINE_READER;
	return Reader->Position >= Reader->Length;
}

MREADER* BuildMikModMemoryReader( BYTE* Data, INT Length )
{
	MikModMemoryReader* Reader = new MikModMemoryReader;
	Reader->Base.Seek	= MMMR_Seek;
	Reader->Base.Tell	= MMMR_Tell;
	Reader->Base.Read	= MMMR_Read;
	Reader->Base.Get	= MMMR_Get;
	Reader->Base.Eof	= MMMR_Eof;
	Reader->Data		= Data;
	Reader->Length		= Length;
	Reader->Position	= 0;
	return &Reader->Base;
}

void DestroyMikModMemoryReader( MREADER* Base )
{
	DEFINE_READER;
	delete Reader;
}

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
	return VC_Init();
}

static void UnMM_Exit()
{
	VC_Exit();
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
