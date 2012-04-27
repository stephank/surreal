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
	if( (INT)Amount > Remaining ) Amount = Remaining;
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

#define CheckALError() check(alGetError() == AL_NO_ERROR)

// XXX: Should these be configurable?
// For 44.1Khz 16-bit stereo, this buffer size amounts to a little under 3 seconds.
#define FRAGMENT_SIZE 32768
#define FRAGMENT_COUNT 16

// MusicBuffers is a ring of fragments we push to OpenAL.
static ALuint MusicBuffers[FRAGMENT_COUNT];
// CurrentBuffer points to the next buffer to fill.
static INT CurrentBuffer;
// ScratchArea is the buffer-buffer, the space MikMod writes to, and we in turn
// feed back into OpenAL.
static SBYTE ScratchArea[FRAGMENT_SIZE];


static BOOL UnMM_IsPresent()
{
	return 1;
}

static BOOL UnMM_Init()
{
	// Generate the buffers.
	alGenBuffers( FRAGMENT_COUNT, MusicBuffers );
	CheckALError();
	CurrentBuffer = 0;

	return VC_Init();
}

static void UnMM_Exit()
{
	VC_Exit();

	// Stop the music.
	alSourceStop( MusicSource );
	// Clear the buffer queue.
	alSourcei( MusicSource, AL_BUFFER, AL_NONE );
	// Destroy the buffers.
	alDeleteBuffers( FRAGMENT_COUNT, MusicBuffers );
}

static BOOL UnMM_Reset()
{
	UnMM_Exit();
	return UnMM_Init();
}

static void UnMM_PlayStop()
{
	VC_PlayStop();
	alSourceStop( MusicSource );
}

static void UnMM_Update()
{
	INT BuffersQueued, BuffersProcessed;
	alGetSourcei( MusicSource, AL_BUFFERS_QUEUED,		&BuffersQueued );
	alGetSourcei( MusicSource, AL_BUFFERS_PROCESSED,	&BuffersProcessed );

	INT BuffersToFill = FRAGMENT_COUNT - BuffersQueued + BuffersProcessed;
	if( !BuffersToFill )
		return;
	while( BuffersToFill )
	{
		ALuint Buffer = MusicBuffers[CurrentBuffer];
		// During initial buffering, this may actually fail, because the
		// buffers are fresh and have never been queued at all.
		// But that's okay, we just flush the error.
		alSourceUnqueueBuffers( MusicSource, 1, &Buffer );
		alGetError();

		// Read from MikMod and feed into OpenAL.
		ALsizei Length = VC_WriteBytes( ScratchArea, FRAGMENT_SIZE );
		alBufferData( Buffer, AL_FORMAT_STEREO16, ScratchArea, Length, md_mixfreq );
		alSourceQueueBuffers( MusicSource, 1, &Buffer );

		// Increment CurrentBuffer.
		CurrentBuffer = (CurrentBuffer + 1) % FRAGMENT_COUNT;
		BuffersToFill--;
	}

	// Always keep the source playing, even after an underrun.
	ALint SourceState;
	alGetSourcei( MusicSource, AL_SOURCE_STATE, &SourceState );
	if( SourceState != AL_PLAYING )
		alSourcePlay( MusicSource );
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
	UnMM_PlayStop,
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
