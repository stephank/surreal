/*=============================================================================
	AudioLibrary.h: Unreal general audio management functions.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jeremy Muhlich, based on Generic Audio AudioLibrary.h
=============================================================================*/

/*------------------------------------------------------------------------------------
	Helpers
------------------------------------------------------------------------------------*/

#define CheckAudioLib(f)        if( !AudioInitialized ) return f;

/*------------------------------------------------------------------------------------
	Constants
------------------------------------------------------------------------------------*/

#define AUDIO_TOTALCHANNELS	64
#define AUDIO_TOTALVOICES	256

#define AUDIO_MINPAN		0
#define AUDIO_MIDPAN		16384
#define AUDIO_MAXPAN		32767
#define AUDIO_SURPAN		32768

#define AUDIO_MINVOLUME		0
#define AUDIO_MAXVOLUME		MIX_MAX_VOLUME

#define AUDIO_DISABLED		0
#define AUDIO_ENABLED		1

#define AUDIO_MINSAMPLE8	-128
#define AUDIO_MAXSAMPLE8	127

#define AUDIO_MINSAMPLE16	-32768
#define AUDIO_MAXSAMPLE16	32767

#define AUDIO_MONO_CHANNELS	1
#define AUDIO_STEREO_CHANNELS	2

/*------------------------------------------------------------------------------------
	Flags
------------------------------------------------------------------------------------*/

// from SDL_audio.h :

/* Audio format flags (defaults to LSB byte order) */
//#define AUDIO_U8        0x0008  /* Unsigned 8-bit samples */
//#define AUDIO_S8        0x8008  /* Signed 8-bit samples */
//#define AUDIO_U16LSB    0x0010  /* Unsigned 16-bit samples */
//#define AUDIO_S16LSB    0x8010  /* Signed 16-bit samples */
//#define AUDIO_U16MSB    0x1010  /* As above, but big-endian byte order */
//#define AUDIO_S16MSB    0x9010  /* As above, but big-endian byte order */

#define AUDIO_8BIT		8
#define AUDIO_16BIT		16

// we're going to force signed little-endian samples for now
#define AUDIO_BASE_FLAGS        0x8000

// what is this all about?
//#define AUDIO_SHOLD		0
//#define AUDIO_COSINE		4

// not implemented (yet?)
//#define AUDIO_2DAUDIO		0
//#define AUDIO_3DAUDIO		8

#define VOICE_AUTO		0
#define VOICE_DISABLED		0
#define VOICE_ENABLED		1
#define VOICE_ACTIVE		2
#define VOICE_FINISHED		4

/*------------------------------------------------------------------------------------
	Structs
------------------------------------------------------------------------------------*/

struct AudioVector
{
	FLOAT X, Y, Z, W;
};

struct MemChunk
{
	BYTE*	Data;
	DWORD	DataLength;
	DWORD	Position;
};

struct Sample
{
	DWORD	Size;			// Size of the rest of structure.
	_WORD	Panning;		// Panning position.
	_WORD	Volume;			// Volume level.
	_WORD	Type;			// Loop or sample type.
	DWORD	Length;			// Length in samples.
	DWORD	LoopStart;		// Start of loop in samples.
	DWORD	LoopEnd;		// End of loop in samples.
	DWORD	SamplesPerSec;		// Samples per second.
	void*	Data;			// Data.
};

struct AudioThread
{
	void*	Thread;
	INT		Valid;
	INT		Exited;
};

struct AudioMutex
{
	void*	Mutex;
};

/*------------------------------------------------------------------------------------
	Globals
------------------------------------------------------------------------------------*/

extern INT		AudioFormat;
extern INT		AudioRate;

// State
extern INT   		AudioInitialized;
extern INT		AudioPaused;

// Audio
extern INT		MusicVoices;
extern INT		SampleVoices;

// Volume
extern FLOAT		CurSampleVolume;

// Threads
extern AudioThread	MixingThread;
extern AudioMutex	Mutex;

/*------------------------------------------------------------------------------------
        Interface
------------------------------------------------------------------------------------*/

// Library control.
UBOOL AudioInit( INT Rate, _WORD Format, INT PhysChannels );
UBOOL AudioReinit( INT Rate, _WORD Format, INT PhysChannels );
UBOOL AudioShutdown();
UBOOL AllocateVoices( INT Channels );

// Master control.
UBOOL AudioStartOutput( INT Rate, _WORD Format, INT PhysChannels );
UBOOL AudioStopOutput();

// Sample control.
//Sample* LoadSample( MemChunk* LoadChunk, const TCHAR* SampleName );
//UBOOL UnloadSample( Sample* UnloadSample );
//Voice* StartSample( INT Voice, Sample* Sample, INT Freq, INT Volume, INT Panning );
//UBOOL StopSample( Voice* StopVoice );
//UBOOL SampleFinished( Voice* Sample );
//void UpdateSample( Voice* InVoice, INT Freq, INT Volume, INT Panning );

// Voice control.
//Voice* AcquireVoice();
//void GetVoiceStats( VoiceStats* InStats, Voice* InVoice );

// Volume control.
UBOOL SetSampleVolume( FLOAT Volume );
UBOOL SetMusicVolume( FLOAT Volume );
UBOOL SetCDAudioVolume( FLOAT Volume );

// CD Audio control.
UBOOL StartCDAudio( INT Track );
UBOOL StopCDAudio();
