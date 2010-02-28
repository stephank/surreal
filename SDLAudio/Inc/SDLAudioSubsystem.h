/*=============================================================================
	SDLAudioSubsystem.h: Unreal SDL audio interface object.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jeremy Muhlich, based on GenericAudioSubsystem.
=============================================================================*/

/*------------------------------------------------------------------------------------
	Helpers
------------------------------------------------------------------------------------*/

// Constants.
#define MAX_EFFECTS_CHANNELS 32
#define MUSIC_CHANNELS       32
#define EFFECT_FACTOR        0.25

// Utility Macros.
#define safecall(f) \
{ \
	guard(f); \
	INT Error=f; \
	if( Error==0 ) \
		debugf( NAME_Warning, TEXT("%s failed: %i"), TEXT(#f), Error ); \
	unguard; \
}
#define silentcall(f) \
{ \
	guard(f); \
	f; \
	unguard; \
}

/*------------------------------------------------------------------------------------
	USDLAudioSubsystem.
------------------------------------------------------------------------------------*/

//
// Information about a playing sound.
//
class FPlayingSound
{
public:
  //Voice*		Channel;
	INT		PhysChannel;
	AActor*		Actor;
	INT		Id;
	UBOOL		Is3D;
	USound*		Sound;
	FVector		Location;
	FLOAT		Volume;
	FLOAT		Radius;
	FLOAT		Pitch;
	FLOAT		Priority;
	FPlayingSound()
	:/*       Channel (NULL)
		  ,*/
	        PhysChannel (-1)
	,       Actor   (NULL)
	,       Id	      (0)
	,       Is3D    (0)
	,       Sound   (0)
	,       Priority(0)
	,       Volume  (0)
	,       Radius  (0)
	,       Pitch   (0)
	{}
	FPlayingSound( AActor* InActor, INT InId, USound* InSound, FVector InLocation, FLOAT InVolume, FLOAT InRadius, FLOAT InPitch, FLOAT InPriority )
	:/*       Channel (NULL)
		  ,*/
	        PhysChannel (-1)
	,       Actor   (InActor)
	,       Id	      (InId)
	,       Is3D    (0)
	,       Sound   (InSound)
	,       Location(InLocation)
	,       Volume  (InVolume)
	,       Radius  (InRadius)
	,       Pitch   (InPitch)
	,       Priority(InPriority)
	{}
};

//
// The SDL_mixer implementation of UAudioSubsystem.
//
class DLL_EXPORT_CLASS USDLAudioSubsystem : public UAudioSubsystem
{
	DECLARE_CLASS(USDLAudioSubsystem,UAudioSubsystem,CLASS_Config)

	// Configuration.
	BITFIELD		UseFilter;
	BITFIELD		UseSurround;
	BITFIELD		UseStereo;
	BITFIELD		UseCDMusic;
	BITFIELD		UseDigitalMusic;
	BITFIELD		ReverseStereo;
	BITFIELD		Initialized;
	FLOAT			AmbientFactor;
	FLOAT			DopplerSpeed;
	INT			Latency;
	INT			Channels;
	BYTE			OutputRate;
	BYTE			MusicVolume;
	BYTE			SoundVolume;
	UBOOL			AudioStats;
	UBOOL			DetailStats;
	_WORD			Format;
	INT                     PhysChannels;

	// Variables.
	UViewport*		Viewport;
	FPlayingSound           PlayingSounds[MAX_EFFECTS_CHANNELS];
	DOUBLE			LastTime;
	UMusic*			CurrentMusic;
	BYTE			CurrentCDTrack;
	INT			FreeSlot;
	FLOAT			MusicFade;
	
	// Constructor.
	USDLAudioSubsystem();
	void StaticConstructor();

	// UObject interface.
	void Destroy();
	void PostEditChange();
	void ShutdownAfterError();

	// UAudioSubsystem interface.
	UBOOL Init();
	void SetViewport( UViewport* Viewport );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void Update( FPointRegion Region, FCoords& Coords );
	void UnregisterSound( USound* Sound );
	void UnregisterMusic( UMusic* Music );
	UBOOL PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch );
	void NoteDestroy( AActor* Actor );
	void RegisterSound( USound* Sound );
	void RegisterMusic( UMusic* Music );
	UBOOL GetLowQualitySetting() {return 0;}
	UViewport* GetViewport();
	void RenderAudioGeometry( FSceneNode* Frame );
	void PostRender( FSceneNode* Frame );
 
	// Internal functions.
	void SetVolumes();
	void StopSound( INT Index );
 
        // Inlines.
        Sample* GetSound( USound* Sound )
        {
                check(Sound);
                if( !Sound->Handle )
                        RegisterSound( Sound );
                return (Sample*)Sound->Handle;
        }
        FLOAT SoundPriority( UViewport* Viewport, FVector Location, FLOAT Volume, FLOAT Radius )
        {
                return Volume * (1.0 - (Location - (Viewport->Actor->ViewTarget ?
						    Viewport->Actor->ViewTarget :
						    Viewport->Actor)->Location).Size()/Radius);
        }
};

