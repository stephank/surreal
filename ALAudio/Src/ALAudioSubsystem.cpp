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
	alureInitDevice(NULL, NULL);
	SetVolumes();

	ALuint NewSources[NumSources];
	Sources = new FAudioSource[NumSources];
	alGenSources(NumSources, NewSources);
	for( INT i=0; i<NumSources; i++ )
		Sources[i].Id = NewSources[i];

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
		StopSound( i );

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

	// Compute priority.
	FLOAT Priority = SoundPriority( Viewport, Location, Volume, Radius );

	// If already playing, stop it.
	INT   Index        = -1;
	FLOAT BestPriority = Priority;
	for( INT i=0; i<NumSources; i++ )
	{
		FAudioSource& Source = Sources[i];
		if( (Source.Slot&~1)==(Slot&~1) )
		{
			// Skip if not interruptable.
			if( Slot&1 )
				return 0;

			// Stop the sound.
			Index = i;
			break;
		}
		else if( Source.Priority<=BestPriority )
		{
			Index = i;
			BestPriority = Source.Priority;
		}
	}

	// If no sound, or its priority is overruled, stop it.
	if( Index==-1 )
		return 0;

	// Start the sound.
	FAudioSource& Source = Sources[Index];
	alSourceStop( Source.Id );
	if( Sound!=(USound*)-1 )
	{
		const ALuint Id = Source.Id;
		alSourcei(	Id, AL_BUFFER,			GetBufferFromUSound(Sound)->Id );
		alSourcefv(	Id, AL_POSITION,		&Location.X );
		alSource3f(	Id, AL_VELOCITY,		0.f, 0.f, 0.f );
		alSourcef(	Id, AL_GAIN,			Volume );
		alSourcef(	Id, AL_MAX_DISTANCE,	Radius );
		alSourcef(	Id, AL_PITCH,			Pitch );
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
				StopSound( Source );
			else
				// Unbind regular sounds from actors.
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

	// Update the listener
	AActor *ViewActor = Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
	alListenerfv(AL_POSITION, &ViewActor->Location.X );

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
				for( INT j=0; j<NumSources; j++ )
					if( Sources[j].Slot==Slot )
						break;
				if( j==NumSources )
					PlaySound( Actor, Slot, Actor->AmbientSound, Actor->Location, AmbientFactor*Actor->SoundVolume/255.0, Actor->WorldSoundRadius(), Actor->SoundPitch/64.0 );
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
				StopSound( Source );
			}
			else
			{
				// Update basic sound properties.
				FLOAT Volume = 2.0 * (AmbientFactor*Source.Actor->SoundVolume/255.0);
				// XXX: Huh? What does light brightness have to do with it?
				if( Source.Actor->LightType!=LT_None )
				{
					FPlane Color;
					Volume *= Source.Actor->LightBrightness/255.0;
				}
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

		// We shouldn've been notified about this.
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
			StopSound( Source );
			continue;
		}

		// Update positioning from actor, if available.
		// FIXME: velocity and direction.
		if( Source.Actor )
		{
			Source.Location = Source.Actor->Location;
			alSourcefv( Source.Id, AL_POSITION, &Source.Location.X );
		}

		// Update the priority.
		Source.Priority = SoundPriority( Viewport, Source.Location, Source.Volume, Source.Radius );
	}
	unguard;

	unguard;
}
