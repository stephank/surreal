/*=============================================================================
	ALAudioSubsystem.cpp: Unreal OpenAL audio interface object.

Revision history:
	* Created by Stéphan Kochen (based on Audio).
=============================================================================*/

#include "ALAudioSubsystem.h"

/*------------------------------------------------------------------------------------
	Utility
------------------------------------------------------------------------------------*/

static void CheckALErrorFlag( const TCHAR* Tag )
{
	guard(CheckALErrorFlag);

	ALenum Error = alGetError();
	if( Error != AL_NO_ERROR )
	{
		const ALchar* Msg = alGetString( Error );
		if( Msg == NULL ) Msg = "Unknown error";
		appErrorf( TEXT("%s failure: %s (%d)"), Tag, Msg, Error );
	}

	unguard;
}

/*------------------------------------------------------------------------------------
	UOpenALAudioSubsystem
------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UOpenALAudioSubsystem);

UOpenALAudioSubsystem::UOpenALAudioSubsystem()
{
	guard(UOpenALAudioSubsystem::UOpenALAudioSubsystem);

	unguard;
}

void UOpenALAudioSubsystem::StaticConstructor()
{
	guard(UOpenALAudioSubsystem::StaticConstructor);

	UEnum* OutputRates = new(GetClass(),TEXT("OutputRates")) UEnum( NULL );
		new( OutputRates->Names ) FName( TEXT("8000Hz" ) );
		new( OutputRates->Names ) FName( TEXT("11025Hz") );
		new( OutputRates->Names ) FName( TEXT("16000Hz") );
		new( OutputRates->Names ) FName( TEXT("22050Hz") );
		new( OutputRates->Names ) FName( TEXT("32000Hz") );
		new( OutputRates->Names ) FName( TEXT("44100Hz") );
		new( OutputRates->Names ) FName( TEXT("48000Hz") );

	new(GetClass(),TEXT("OutputRate"),			RF_Public) UByteProperty(	CPP_PROPERTY(OutputRate),		TEXT("Audio"), CPF_Config, OutputRates );
	new(GetClass(),TEXT("NumSources"),			RF_Public) UIntProperty(	CPP_PROPERTY(NumSources),		TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("MusicVolume"),			RF_Public) UByteProperty(	CPP_PROPERTY(MusicVolume),		TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("SoundVolume"),			RF_Public) UByteProperty(	CPP_PROPERTY(SoundVolume),		TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("AmbientFactor"),		RF_Public) UFloatProperty(	CPP_PROPERTY(AmbientFactor),	TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("HighQualityMusic"),	RF_Public) UBoolProperty(	CPP_PROPERTY(HighQualityMusic),	TEXT("Audio"), CPF_Config );

	unguard;
}

/*------------------------------------------------------------------------------------
	UObject Interface.
------------------------------------------------------------------------------------*/

void UOpenALAudioSubsystem::PostEditChange()
{
	guard(UOpenALAudioSubsystem::PostEditChange);

	// Validate configurable variables.
	OutputRate		= Clamp(OutputRate,(BYTE)0,(BYTE)6);
	NumSources 		= Clamp(NumSources,1,256);
	AmbientFactor   = Clamp(AmbientFactor,0.f,10.f);
	SetVolumes();

	unguard;
}

void UOpenALAudioSubsystem::Destroy()
{
	guard(UOpenALAudioSubsystem::Destroy);

	if( Initialized )
	{
		// Unhook.
		USound::Audio = NULL;
		UMusic::Audio = NULL;

		// Shut down viewport.
		SetViewport( NULL );

		// Cleanup.
		MikMod_Exit();
		alDeleteSources( 1, &MusicSource );
		for( INT i=0; i<NumSources; i++ )
			alDeleteSources( 1, &Sources[i].Id );
		alureShutdownDevice();
	}

	Super::Destroy();

	unguard;
}

void UOpenALAudioSubsystem::ShutdownAfterError()
{
	guard(UOpenALAudioSubsystem::ShutdownAfterError);

	// Unhook.
	USound::Audio = NULL;
	UMusic::Audio = NULL;

	debugf( NAME_Exit, TEXT("UOpenALAudioSubsystem::ShutdownAfterError") );

	if( Initialized )
	{
		MikMod_Exit();
		alureShutdownDevice();
	}

	Super::ShutdownAfterError();

	unguard;
}

/*------------------------------------------------------------------------------------
	UAudioSubsystem Interface.
------------------------------------------------------------------------------------*/

UBOOL UOpenALAudioSubsystem::Init()
{
	guard(UOpenALAudioSubsystem::Init);

	INT Rate = GetActualOutputRate();


	// OpenAL / ALURE initialization
	ALCint ContextAttrs[] = { ALC_FREQUENCY, Rate, 0 };
	if( alureInitDevice( NULL, ContextAttrs ) == AL_FALSE )
		appErrorf( TEXT("Couldn't initialize OpenAL: %s"), alureGetErrorString() );

	alDistanceModel( AL_LINEAR_DISTANCE_CLAMPED );
	CheckALErrorFlag( TEXT("alDistanceModel") );

	// Metre per second to units per second, where units per meter is 52.5.
	// Taken from: http://wiki.beyondunreal.com/Legacy:General_Scale_And_Dimensions
	alSpeedOfSound( 343.3f * 52.5f );
	CheckALErrorFlag( TEXT("alSpeedOfSound") );

	ALuint* NewSources = new ALuint[NumSources + 1];
	Sources = new FAudioSource[NumSources];
	alGenSources( NumSources + 1, NewSources );
	CheckALErrorFlag( TEXT("alGenSources") );
	MusicSource = NewSources[0];
	for( INT i=0; i<NumSources; i++ )
		Sources[i].Id = NewSources[i+1];
	delete[] NewSources;

	// Fix the music source to 0 values
	alSource3f(	MusicSource, AL_POSITION,			0.f, 0.f, 0.f );
	alSource3f(	MusicSource, AL_VELOCITY,			0.f, 0.f, 0.f );
	alSource3f(	MusicSource, AL_DIRECTION,			0.f, 0.f, 0.f );
	alSourcef(	MusicSource, AL_ROLLOFF_FACTOR,		0.f );
	alSourcei(	MusicSource, AL_SOURCE_RELATIVE,	AL_TRUE );

	SetVolumes();
	CheckALErrorFlag( TEXT("SetVolumes") );


	// MikMod initialization
	MikMod_RegisterDriver( &MusicDriver );

	// Register only formats that are known to be supported by UT.
	// Taken from: http://wiki.beyondunreal.com/Music
	MikMod_RegisterLoader( &load_mod );
	MikMod_RegisterLoader( &load_s3m );
	MikMod_RegisterLoader( &load_stm );
	MikMod_RegisterLoader( &load_it  );
	MikMod_RegisterLoader( &load_xm  );
	MikMod_RegisterLoader( &load_far );
	MikMod_RegisterLoader( &load_669 );

	md_mixfreq = Rate;
	if ( HighQualityMusic )
		md_mode |= DMODE_HQMIXER;
	if( MikMod_Init( "" ) )
		appErrorf( TEXT("Couldn't initialize MikMod: %s"), MikMod_strerror( MikMod_errno ) );


	// Initialized!
	USound::Audio = this;
	UMusic::Audio = this;
	Initialized = 1;

	return 1;
	unguard;
}

void UOpenALAudioSubsystem::SetViewport( UViewport* InViewport )
{
	guard(UOpenALAudioSubsystem::SetViewport);

	// Stop all sources.
	StopMusic();
	for( INT i=0; i<NumSources; i++ )
		StopSource( i );

	Viewport = InViewport;

	unguard;
}

UViewport* UOpenALAudioSubsystem::GetViewport()
{
	guard(UOpenALAudioSubsystem::GetViewport);
	return Viewport;
	unguard;
}

void UOpenALAudioSubsystem::RegisterSound( USound* Sound )
{
	guard(UOpenALAudioSubsystem::RegisterSound);

	checkSlow(Sound);
	if( !Sound->Handle )
	{
		// Set the handle to avoid reentrance.
		Sound->Handle = (void*)-1;

		// Load the data.
		Sound->Data.Load();
		debugf( NAME_DevSound, TEXT("Register sound: %s (%i)"), Sound->GetPathName(), Sound->Data.Num() );
		check(Sound->Data.Num()>0);

		// Flush errors.
		alGetError();

		// Create the buffer.
		FAudioBuffer *Sample = new FAudioBuffer;
		Sample->Id = alureCreateBufferFromMemory( &Sound->Data(0), Sound->Data.Num() );
		if( Sample->Id == AL_NONE )
			appErrorf(
				TEXT("Couldn't create buffer for sound '%s': %s"),
				Sound->GetPathName(), alureGetErrorString()
			);
		Sound->Handle = Sample;

		// Unload the data.
		Sound->Data.Unload();
	}

	unguard;
}

void UOpenALAudioSubsystem::UnregisterSound( USound* Sound )
{
	guard(UOpenALAudioSubsystem::UnregisterSound);

	check(Sound);
	if( Sound->Handle )
	{
		debugf( NAME_DevSound, TEXT("Unregister sound: %s"), Sound->GetFullName() );

		FAudioBuffer* Sample = (FAudioBuffer*)Sound->Handle;
		alDeleteBuffers(1, &Sample->Id);
		delete Sample;
	}

	unguard;
}

void UOpenALAudioSubsystem::RegisterMusic( UMusic* Music )
{
	guard(UOpenALAudioSubsystem::RegisterMusic);

	checkSlow(Music);
	if( !Music->Handle )
	{
		// Set the handle to avoid reentrance.
		Music->Handle = (void*)-1;

		// Load the data.
		Music->Data.Load();
		debugf( NAME_DevMusic, TEXT("Register music: %s (%i)"), Music->GetPathName(), Music->Data.Num() );
		check(Music->Data.Num()>0);

		// Load the module.
		MREADER* Reader = BuildMikModMemoryReader( &Music->Data(0), Music->Data.Num() );
		MODULE* Module = Player_LoadGeneric( Reader, 64, 0 );
		DestroyMikModMemoryReader( Reader );
		if( Module == NULL )
			appErrorf(
				TEXT("Couldn't load music '%s': %s"),
				Music->GetPathName(), MikMod_strerror( MikMod_errno )
			);
		Music->Handle = Module;

		// Enable looping and wrapping.
		Module->loop = Module->wrap = 1;

		// Unload the data.
		Music->Data.Unload();
	}

	unguard;
}

void UOpenALAudioSubsystem::UnregisterMusic( UMusic* Music )
{
	guard(UOpenALAudioSubsystem::UnregisterMusic);

	check(Music);
	if( Music->Handle )
	{
		debugf( NAME_DevMusic, TEXT("Unregister music: %s"), Music->GetFullName() );

		MODULE* Module = (MODULE*)Music->Handle;
		Player_Free( Module );
	}

	unguard;
}

UBOOL UOpenALAudioSubsystem::PlaySound
(
	AActor*	Actor,
	INT		Slot,
	USound*	Sound,
	FVector	Location,
	FLOAT	Volume,
	FLOAT	Radius,
	FLOAT	Pitch
)
{
	return PlaySound( Actor, Slot, Sound, Location, Volume, Radius, Pitch, 0 );
}

UBOOL UOpenALAudioSubsystem::PlaySound
(
	AActor*	Actor,
	INT		Slot,
	USound*	Sound,
	FVector	Location,
	FLOAT	Volume,
	FLOAT	Radius,
	FLOAT	Pitch,
	UBOOL	Looping
)
{
	guard(UOpenALAudioSubsystem::PlaySound);

	check(Radius);
	if( !Viewport || !Sound )
		return 0;

	// Allocate a new slot if requested.
	// XXX: What's the logic here?
	if( (Slot&14)==2*SLOT_None )
		Slot = 16 * --FreeSlot;

	// Compute our priority.
	FLOAT Priority = SoundPriority( Location, Volume, Radius );

	INT   Index        = -1;
	FLOAT BestPriority = Priority;
	for( INT i=0; i<NumSources; i++ )
	{
		FAudioSource& Source = Sources[i];

		// Check if the slot is already in use.
		if( (Source.Slot&~1)==(Slot&~1) )
		{
			// Stop processing if not told to override.
			if( Slot&1 )
				return 0;

			// Override the existing sound.
			Index = i;
			break;
		}

		// Find the lowest priority sound below our own priority
		// and override it. (Unless the above applies.)
		if( Source.Priority<=BestPriority )
		{
			Index = i;
			BestPriority = Source.Priority;
		}
	}

	// Didn't match an existing slot, or couldn't override a lower
	// priority sound. Give up.
	if( Index==-1 )
		return 0;

	// Stop the old sound.
	FAudioSource& Source = Sources[Index];
	alSourceStop( Source.Id );

	// And start the new sound.
	if( Sound!=(USound*)-1 )
	{
		const ALuint Id = Source.Id;
		alSourcei( Id, AL_BUFFER,		GetBufferFromUSound(Sound)->Id );
		alSourcef( Id, AL_GAIN,			Volume );
		alSourcef( Id, AL_MAX_DISTANCE,	Radius );
		alSourcef( Id, AL_PITCH,		Pitch );
		alSourcei( Id, AL_LOOPING,		Looping ? AL_TRUE : AL_FALSE );
		if( Actor == Viewport->Actor )
		{
			// Don't attentuate or position viewport actor sounds at all.
			// (These are sounds like the announcer, menu clicks, etc.)
			alSource3f(	Id, AL_POSITION,		0.f, 0.f, 0.f );
			alSource3f(	Id, AL_VELOCITY,		0.f, 0.f, 0.f );
			alSourcef(	Id, AL_ROLLOFF_FACTOR,	0.f );
			alSourcei(	Id, AL_SOURCE_RELATIVE,	AL_TRUE );
			Actor = NULL;
		}
		else
		{
			// Negate the above
			alSourcef(	Id, AL_ROLLOFF_FACTOR,	1.f );
			alSourcei(	Id, AL_SOURCE_RELATIVE,	AL_FALSE );
			// Update will override Location with Actor's Location anyways.
			// XXX: Should we use Location and not attach to Actor instead?
			if ( Actor )
			{
				alSourcefv(	Id, AL_POSITION,	&Actor->Location.X );
				alSourcefv(	Id, AL_VELOCITY,	&Actor->Velocity.X );
			}
			else
			{
				ALfloat ZeroVelocity[3] = { 0.f, 0.f, 0.f };
				alSourcefv(	Id, AL_POSITION,	&Location.X );
				alSourcefv(	Id, AL_VELOCITY,	ZeroVelocity );
			}
		}
		alSourcePlay( Id );
		Source.Fill( Actor, Sound, Slot, Location, Volume, Radius, Priority );
	}

	return 1;

	unguard;
}

void UOpenALAudioSubsystem::NoteDestroy( AActor* Actor )
{
	guard(UOpenALAudioSubsystem::NoteDestroy);
	check(Actor);
	check(Actor->IsValid());

	// Stop referencing actor.
	for( INT i=0; i<NumSources; i++ )
	{
		FAudioSource& Source = Sources[i];
		if( Source.Actor==Actor )
		{
			// Stop ambient sound when actor dies.
			if( (Source.Slot&14)==SLOT_Ambient*2 )
				StopSource( Source );

			// Unbind regular sounds from their actors.
			else
				Source.Actor = NULL;
		}
	}

	unguard;
}

void UOpenALAudioSubsystem::Update( FPointRegion Region, FCoords& Coords )
{
	guard(UOpenALAudioSubsystem::Update);

	if( !Viewport )
		return;

	AActor *ViewActor = FindViewActor();

	guard(UpdateMusic);
	if( Viewport->Actor->Song != PlayingSong )
	{
		StopMusic();
		PlayingSong = Viewport->Actor->Song;
		if( PlayingSong != NULL )
		{
			MODULE* Module = GetModuleFromUMusic( PlayingSong );
			Player_Start( Module );
		}
	}
	if( Player_Active() )
		MikMod_Update();
	unguard;

	// Update the listener.
	{
		FVector At = ViewActor->Rotation.Vector();
		FVector Up = ViewActor->Rotation.UpVector();
		FLOAT Orientation[6] = { At.X, At.Y, At.Z, Up.X, Up.Y, Up.Z };
		alListenerfv( AL_POSITION,		&ViewActor->Location.X );
		alListenerfv( AL_VELOCITY,		&ViewActor->Velocity.X );
		alListenerfv( AL_ORIENTATION,	Orientation );
	}

	// See if any new ambient sounds need to be started.
	UBOOL Realtime = Viewport->IsRealtime() && Viewport->Actor->Level->Pauser==TEXT("");
	if( Realtime )
	{
		guard(StartAmbience);
		for( INT i=0; i<Viewport->Actor->GetLevel()->Actors.Num(); i++ )
		{
			AActor* Actor = Viewport->Actor->GetLevel()->Actors(i);
			if
			(	Actor
			&&	Actor->AmbientSound
			&&	FDistSquared(ViewActor->Location,Actor->Location)<=Square(Actor->WorldSoundRadius()) )
			{
				INT Slot = Actor->GetIndex()*16+SLOT_Ambient*2;
				INT j;
				// See if there's already an existing slot.
				for( j=0; j<NumSources; j++ )
					if( Sources[j].Slot==Slot )
						break;
				// If not, start playing.
				if( j==NumSources )
					PlaySound(
						Actor, Slot, Actor->AmbientSound, Actor->Location,
						AmbientFactor*Actor->SoundVolume/255.0,
						Actor->WorldSoundRadius(),
						Actor->SoundPitch/64.0,
						1 );
			}
		}
		unguard;
	}

	// Update all playing ambient sounds.
	guard(UpdateAmbience);
	for( INT i=0; i<NumSources; i++ )
	{
		FAudioSource& Source = Sources[i];
		if( (Source.Slot&14)==SLOT_Ambient*2 )
		{
			check(Source.Actor);
			if
			(	FDistSquared(ViewActor->Location,Source.Actor->Location)>Square(Source.Actor->WorldSoundRadius())
			||	Source.Actor->AmbientSound!=Source.Sound 
			||  !Realtime )
			{
				// Ambient sound went out of range.
				StopSource( Source );
			}
			else
			{
				// Update basic sound properties.
				FLOAT Volume = 2.0 * (AmbientFactor*Source.Actor->SoundVolume/255.0);
				// XXX: Huh? What does light brightness have to do with it?
				if( Source.Actor->LightType!=LT_None )
					Volume *= Source.Actor->LightBrightness/255.0;
				Source.Volume = Volume;
				Source.Radius = Source.Actor->WorldSoundRadius();

				const ALuint Id = Source.Id;
				alSourcef( Id, AL_GAIN,			Source.Volume );
				alSourcef( Id, AL_MAX_DISTANCE,	Source.Radius );
				alSourcef( Id, AL_PITCH,		Source.Actor->SoundPitch/64.0 );
			}
		}
	}
	unguard;

	// Update all active sounds.
	guard(UpdateSounds);
	for( INT Index=0; Index<NumSources; Index++ )
	{
		FAudioSource& Source = Sources[Index];

		// We should've been notified about this.
		if( Source.Actor )
			check(Source.Actor->IsValid());

		// Check if the sound is playing.
		if( Source.Slot==0 )
			continue;

		// Check if the sound is finished.
		ALint state;
		alGetSourcei( Source.Id, AL_SOURCE_STATE, &state );
		if( state==AL_STOPPED )
		{
			StopSource( Source );
			continue;
		}

		// Update positioning from actor, if available.
		if( Source.Actor )
		{
			Source.Location = Source.Actor->Location;
			alSourcefv( Source.Id, AL_POSITION, &Source.Actor->Location.X );
			alSourcefv( Source.Id, AL_VELOCITY, &Source.Actor->Velocity.X );
		}

		// Update the priority.
		Source.Priority = SoundPriority( Source.Location, Source.Volume, Source.Radius );
	}
	unguard;

	unguard;
}
