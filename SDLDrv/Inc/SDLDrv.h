/*=============================================================================
	SDLDrv.h: Unreal SDL viewport and platform driver.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel (based on XDrv).

=============================================================================*/

#ifndef _INC_SDLDRV
#define _INC_SDLDRV

/*----------------------------------------------------------------------------
	Dependencies.
----------------------------------------------------------------------------*/

// System includes.
#include <stdlib.h>
#ifdef WIN32
#include <windows.h>
#endif

// Unreal includes.
#include "Engine.h"
#include "UnRender.h"

// SDL includes
#include "SDL.h"
#include "SDL_syswm.h"


/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Classes.
class USDLViewport;
class USDLClient;

/*-----------------------------------------------------------------------------
	USDLClient.
-----------------------------------------------------------------------------*/

//
// SDL implementation of the client.
//
class USDLClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS(USDLClient,UClient,CLASS_Transient|CLASS_Config,SDLDrv)

	// Configuration.
	BITFIELD	StartupFullscreen;
	BITFIELD	SlowVideoBuffering;

	// Variables.
	UBOOL		InMenuLoop;
	UBOOL		ConfigReturnFullscreen;
	INT			NormalMouseInfo[3];
	INT			CaptureMouseInfo[3];
	UBOOL		LastKey;
	FTime		RepeatTimer;
	EInputKey	KeysymMap[65536];

	// Constructors.
	USDLClient();
	void StaticConstructor();

	// FNotifyHook interface.
	void NotifyDestroy( void* Src );

	// UObject interface.
	void Destroy();
	void ShutdownAfterError();

	// UClient interface.
	void Init( UEngine* InEngine );
	void ShowViewportWindows( DWORD ShowFlags, INT DoShow );
	void EnableViewportWindows( DWORD ShowFlags, INT DoEnable );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void Tick();
	void MakeCurrent( UViewport* InViewport );
	class UViewport* NewViewport( const FName Name );
};

/*-----------------------------------------------------------------------------
	USDLViewport.
-----------------------------------------------------------------------------*/

//
// A SDL viewport.
//
class USDLViewport : public UViewport
{
	DECLARE_CLASS(USDLViewport,UViewport,CLASS_Transient,SDLDrv)
	DECLARE_WITHIN(USDLClient)

	// Variables.
	SDL_Window*		Window;
	SDL_Renderer*	Renderer;
	UClass*			RenDevOverride;
	INT				CurrentColorBytes;
	INT				HoldCount;
	DWORD			BlitFlags;

	// Mouse motion events.
	// These variables are only used by USDLClient.
	INT				MouseX;
	INT				MouseY;
	INT				MouseWheelY;

	// SDL Keysym to EInputKey map.
	
	// Constructor.
	USDLViewport();

	// UObject interface.
	void Destroy();
	void ShutdownAfterError();

	// UViewport interface.
	UBOOL ResizeViewport( DWORD BlitFlags, INT NewX=INDEX_NONE, INT NewY=INDEX_NONE, INT NewColorBytes=INDEX_NONE );
	UBOOL IsFullscreen();
	void Repaint( UBOOL Blit );
	void SetModeCursor();
	void UpdateWindowFrame();
	void OpenWindow( DWORD ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY );
	void CloseWindow();
	void UpdateInput( UBOOL Reset );
	void* GetWindow();
	void* GetServer();
	void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly );
};

#endif //_INC_SDLDRV
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
