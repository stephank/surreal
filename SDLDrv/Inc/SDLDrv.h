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

// Unreal includes.
#include "Engine.h"
#include "UnRender.h"

// SDL includes
#include <SDL/SDL.h>


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
	DECLARE_CLASS(USDLClient,UClient,CLASS_Transient|CLASS_Config)

	// Configuration.
	BITFIELD			StartupFullscreen;
	BITFIELD			SlowVideoBuffering;

	// Variables.
	UBOOL				InMenuLoop;
	UBOOL				ConfigReturnFullscreen;
	INT				NormalMouseInfo[3];
	INT				CaptureMouseInfo[3];

	// Constructors.
	USDLClient();
	void StaticConstructor();

	// FNotifyHook interface.
	void NotifyDestroy( void* Src );

	// UObject interface.
	void Destroy();
	void PostEditChange();
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
// Viewport window status.
//
enum ESDLViewportStatus
{
	SDL_ViewportOpening	= 0, // Viewport is opening.
	SDL_ViewportNormal	= 1, // Viewport is operating normally.
	SDL_ViewportClosing	= 2, // Viewport is closing and CloseViewport has been called.
};

//
// A SDL viewport.
//
class USDLViewport : public UViewport
{
	DECLARE_CLASS(USDLViewport,UViewport,CLASS_Transient)
	DECLARE_WITHIN(USDLClient)

	// Variables.
	ESDLViewportStatus		ViewportStatus;
	INT				HoldCount;
	DWORD				BlitFlags;
	UBOOL				Borderless;
	UBOOL				RestoreAutoRepeat;
	UBOOL				LastKey;
	DOUBLE				RepeatTimer;
	UBOOL				Mapped;
	UBOOL				Iconified;
	
	// Mouse.
	//INT				MouseAccel_N;
	//INT				MouseAccel_D;
	//INT				MouseThreshold;

	// Info saved during captures and fullscreen sessions.
	INT				SavedColorBytes;
	INT				SavedCaps;

	// SDL Keysym to EInputKey map.
	BYTE				KeysymMap[65536];

	// ShiftMask map.
	BYTE				ShiftMaskMap[256];

	// KeyRepeat map.
	BYTE				KeyRepeatMap[256];

	// Map of keys that can be WM_CHAR'd
	BYTE				WMCharMap[256];
	
	// Constructor.
	USDLViewport();

	// UObject interface.
	void Destroy();
	void ShutdownAfterError();

	// UViewport interface.
	UBOOL Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData=NULL, INT* HitSize=0 );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );
	UBOOL ResizeViewport( DWORD BlitFlags, INT NewX=INDEX_NONE, INT NewY=INDEX_NONE, INT NewColorBytes=INDEX_NONE );
	UBOOL IsFullscreen();
	void Unlock( UBOOL Blit );
	void Repaint( UBOOL Blit );
	void SetModeCursor();
	void UpdateWindowFrame();
	void OpenWindow( DWORD ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY );
	void CloseWindow();
	void UpdateInput( UBOOL Reset );
	void* GetWindow();
	void* GetServer();
	void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly );

	// UXViewport interface.
	void ToggleFullscreen();
	void EndFullscreen();
	void FindAvailableModes();
	void TryHardware3D( USDLViewport* Viewport );
	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
	void TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen );
	void Tick();
	void Iconify();
	//	Cursor GetNullCursor();
	void CaptureInputs();
	void ReleaseInputs();
};

#endif //_INC_SDLDRV
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
