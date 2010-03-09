/*=============================================================================
	AudioSubsystem.h: Unreal audio interface object.

Revision history:
	* Created by Stéphan Kochen (based on Audio).
=============================================================================*/

#include "AL/al.h"
#include "AL/alure.h"
#include "Core.h"
#include "Engine.h"
#include "ALAudioMusic.h"

/*------------------------------------------------------------------------------------
	UOpenALAudioSubsystem.
------------------------------------------------------------------------------------*/

//
// Information about a loaded sound.
//
struct FAudioBuffer
{
	ALuint	Id;
};

//
// Information about a playing sound.
//
class FAudioSource
{
public:
	ALuint		Id;
	AActor*		Actor;
	USound*		Sound;
	INT			Slot;
	FVector		Location;
	FLOAT		Volume;
	FLOAT		Radius;
	FLOAT		Priority;

	FAudioSource()
	:	Id		(0)
	,	Actor	(NULL)
	,	Slot	(0)
	,	Priority(0.f)
	{}
	inline void Fill( AActor* InActor, USound* InSound, INT InSlot, FVector InLocation, FLOAT InVolume, FLOAT InRadius, FLOAT InPriority )
	{
		Actor		= InActor;
		Sound		= InSound;
		Slot		= InSlot;
		Location	= InLocation;
		Volume		= InVolume;
		Radius		= InRadius;
		Priority	= InPriority;
	}
};

class DLL_EXPORT_CLASS UOpenALAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(UOpenALAudioSubsystem,UAudioSubsystem,CLASS_Config,Audio)

protected:
	// Configuration.
	BITFIELD		Initialized;
	FLOAT			AmbientFactor;
	INT				NumSources;
	BYTE			MusicVolume;
	BYTE			SoundVolume;

	// Variables.
	UViewport*		Viewport;
	FAudioSource*	Sources;
	INT				FreeSlot;

public:
	// Constructor.
	UOpenALAudioSubsystem();
	void StaticConstructor();

	// UObject interface.
	void Destroy();
	void PostEditChange();
	void ShutdownAfterError();

	// UAudioSubsystem interface.
	UBOOL Init();
	void SetViewport( UViewport* Viewport );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void Update( FPointRegion Region, FCoords& Listener );
	void RegisterMusic( UMusic* Music );
	void RegisterSound( USound* Music );
	void UnregisterSound( USound* Sound );
	void UnregisterMusic( UMusic* Music );
	UBOOL PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch );
	void NoteDestroy( AActor* Actor );
	UBOOL GetLowQualitySetting() {};
	UViewport* GetViewport();
	void RenderAudioGeometry( FSceneNode* Frame ) {};
	void PostRender( FSceneNode* Frame ) {};

private:
	// Inlines.
	inline void SetVolumes()
	{
		guard(UOpenALAudioSubsystem::SetVolumes);

		// Normalize the volumes.
		FLOAT NormSoundVolume = SoundVolume/255.0;
		FLOAT NormMusicVolume = MusicVolume/255.0;

		// Set music and effects volumes.
		alListenerf( AL_GAIN, NormSoundVolume );
		// FIXME: Music volume is relative to effects volume here.
		alSourcef( MusicSource, AL_GAIN, NormMusicVolume );

		unguard;
	}

	inline FAudioBuffer* GetBufferFromUSound( USound* Sound )
	{
		check(Sound);
		if( !Sound->Handle )
			RegisterSound( Sound );
		return (FAudioBuffer*)Sound->Handle;
	}

	inline AActor* FindViewActor()
	{
		return Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget : Viewport->Actor;
	}

	inline FLOAT SoundPriority( FVector Location, FLOAT Volume, FLOAT Radius )
	{
		return Volume * (1.0 - (Location - FindViewActor()->Location).Size()/Radius);
	}

	inline void StopSource( INT Index )
	{
		FAudioSource& Source = Sources[Index];
		StopSource(Source);
	}
	inline void StopSource( FAudioSource& Source )
	{
		FVector ZeroLocation(0.f, 0.f, 0.f);
		alSourceStop( Source.Id );
		Source.Fill( NULL, NULL, 0, ZeroLocation, 0.f, 0.f, 0.f );
	}
};
