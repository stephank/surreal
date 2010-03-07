/*=============================================================================
	ALAudioSubsystem.cpp: Unreal OpenAL audio interface object.

Revision history:
	* Created by Stéphan Kochen (based on Audio).
=============================================================================*/

#include "ALAudioSubsystem.h"

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

	new(GetClass(),TEXT("NumSources"), 		RF_Public)UIntProperty   (CPP_PROPERTY(NumSources		), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("MusicVolume"),		RF_Public)UByteProperty  (CPP_PROPERTY(MusicVolume		), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("SoundVolume"),		RF_Public)UByteProperty  (CPP_PROPERTY(SoundVolume		), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("AmbientFactor"),	RF_Public)UFloatProperty (CPP_PROPERTY(AmbientFactor	), TEXT("Audio"), CPF_Config );

	unguard;
}

/*------------------------------------------------------------------------------------
	UObject Interface.
------------------------------------------------------------------------------------*/

void UOpenALAudioSubsystem::PostEditChange()
{
	guard(UOpenALAudioSubsystem::PostEditChange);

	// Validate configurable variables.
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

		alureShutdownDevice();

		debugf( NAME_Exit, TEXT("Generic audio subsystem shut down.") );
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
		alureShutdownDevice();

	Super::ShutdownAfterError();

	unguard;
}

/*------------------------------------------------------------------------------------
	UAudioSubsystem Interface.
------------------------------------------------------------------------------------*/

UBOOL UOpenALAudioSubsystem::Init()
{
	guard(UOpenALAudioSubsystem::Init);

	// FIXME: Init OpenAL
	alureInitDevice( NULL, NULL );
	alDistanceModel( AL_INVERSE_DISTANCE_CLAMPED );
	// Metre per second to units per second, where units per meter is 52.5.
	// Taken from: http://wiki.beyondunreal.com/Legacy:General_Scale_And_Dimensions
	alSpeedOfSound( 343.3f * 52.5f );
	SetVolumes();

	ALuint NewSources[NumSources];
	Sources = new FAudioSource[NumSources];
	alGenSources( NumSources, NewSources );
	for( INT i=0; i<NumSources; i++ )
	{
		// FIXME: Heavy distortion without this, but why?
		alSourcef( NewSources[i], AL_MAX_GAIN, .001f );
		Sources[i].Id = NewSources[i];
	}

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

	// FIXME: Stop all sources and contexts
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

		// FIXME: create buffer
		FAudioBuffer *Sample = new FAudioBuffer;
		Sample->Id = alureCreateBufferFromMemory(&Sound->Data(0), Sound->Data.Num());
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

		// FIXME: destroy buffer
		FAudioBuffer* Sample = (FAudioBuffer*)Sound->Handle;
		alDeleteBuffers(1, &Sample->Id);
		delete Sample;
	}

	unguard;
}

void UOpenALAudioSubsystem::RegisterMusic( UMusic* Music )
{
	// FIXME
}

void UOpenALAudioSubsystem::UnregisterMusic( UMusic* Music )
{
	// FIXME
}

UBOOL UOpenALAudioSubsystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UOpenALAudioSubsystem::Exec);

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
	guard(UOpenALAudioSubsystem::PlaySound);

	check(Radius);
	if( !Viewport || !Sound )
		return 0;

	// Allocate a new slot if requested.
	// XXX: What's the logic here?
	if( (Slot&14)==2*SLOT_None )
		Slot = 16 * --FreeSlot;

	// Compute our priority.
	FLOAT Priority = SoundPriority( Viewport, Location, Volume, Radius );

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
		// Update will override Location with Actor's Location anyways.
		// XXX: Should we use Location and not attach to Actor instead?
		if( Actor )
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

	AActor *ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;

	// Update the listener.
	{
		FVector Direction = ViewActor->Rotation.Vector();
		FLOAT Orientation[6] = { Direction.X, Direction.Y, Direction.Z, 0.f, 0.f, 1.f };
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
				// See if there's already an existing slot.
				for( INT j=0; j<NumSources; j++ )
					if( Sources[j].Slot==Slot )
						break;
				// If not, start playing.
				if( j==NumSources )
					PlaySound(
						Actor, Slot, Actor->AmbientSound, Actor->Location,
						AmbientFactor*Actor->SoundVolume/255.0,
						Actor->WorldSoundRadius(),
						Actor->SoundPitch/64.0 );
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
		Source.Priority = SoundPriority( Viewport, Source.Location, Source.Volume, Source.Radius );
	}
	unguard;

	unguard;
}
