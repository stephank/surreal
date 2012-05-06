/*=============================================================================
	SDLViewport.cpp: USDLViewport code.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "SDLDrv.h"

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USDLViewport);

/*-----------------------------------------------------------------------------
	USDLViewport Init/Exit.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
USDLViewport::USDLViewport()
:	UViewport()
{
	guard(USDLViewport::USDLViewport);

	// Initialize size to undefined.
	SizeX = INDEX_NONE;
	SizeY = INDEX_NONE;

	// Set initial fullscreen value.
	USDLClient* Client = GetOuterUSDLClient();
	if( !GIsEditor && Client->StartupFullscreen )
		BlitFlags |= BLIT_Fullscreen;

	// Set initial depth value.
	SDL_DisplayMode CurrentMode;
	if( SDL_GetCurrentDisplayMode( 0, &CurrentMode ) == 0 )
		ColorBytes = SDL_BYTESPERPIXEL( CurrentMode.format );
	else
		ColorBytes = 4;

	debugf( TEXT("Created and initialized a new SDL viewport.") );

	unguard;
}

//
// Destroy.
//
void USDLViewport::Destroy()
{
	guard(USDLViewport::Destroy);
	Super::Destroy();
	unguard;
}

//
// Error shutdown.
//
void USDLViewport::ShutdownAfterError()
{
	debugf( NAME_Exit, TEXT("Executing USDLViewport::ShutdownAfterError") );
	Super::ShutdownAfterError();
}

/*-----------------------------------------------------------------------------
	USDLViewport operations.
-----------------------------------------------------------------------------*/

//
// Open this viewport's window.
//
void USDLViewport::OpenWindow( DWORD InParentWindow, UBOOL IsTemporary, INT NewX, INT NewY, INT OpenX, INT OpenY )
{
	guard(USDLViewport::OpenWindow);

	check(Actor);
	check(!HoldCount);
	check(!Window);

	debugf( TEXT("Opening SDL viewport.") );
	UBOOL Fullscreen = IsFullscreen();

	// Window size defaults.
	USDLClient* Client = GetOuterUSDLClient();
	if( NewX!=INDEX_NONE )
		SizeX = NewX;
	if( NewY!=INDEX_NONE )
		SizeY = NewY;
	if( SizeX==INDEX_NONE )
		SizeX = Fullscreen ? Client->FullscreenViewportX : Client->WindowedViewportX;
	if( SizeY==INDEX_NONE )
		SizeY = Fullscreen ? Client->FullscreenViewportY : Client->WindowedViewportY;

	// Find device driver.
	// FIXME: Is this the correct way to handle GIsEditor?
	const TCHAR* IniPath;
	if( GIsEditor )
		IniPath = TEXT("ini:Engine.Engine.RenderDevice");
	else if( Fullscreen )
		IniPath = TEXT("ini:Engine.Engine.GameRenderDevice");
	else
		IniPath = TEXT("ini:Engine.Engine.WindowedRenderDevice");
	UClass* RenderClass = UObject::StaticLoadClass( URenderDevice::StaticClass(), NULL, IniPath, NULL, 0, NULL);
	check(RenderClass);
	debugf( TEXT("Loaded render device class.") );

	// Create the window.
	Uint32 WindowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN;
	if( Fullscreen )
		WindowFlags |= SDL_WINDOW_FULLSCREEN;
	// FIXME: Neater way to do this? (Ask the device, somehow?)
	// Does it at all matter if we say OpenGL, but then not actually use it?
	if( appStricmp(RenderClass->GetName(),TEXT("OpenGLRenderDevice")) == 0 )
		WindowFlags |= SDL_WINDOW_OPENGL;
	Window = SDL_CreateWindow(
		"Unreal Tournament",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		NewX, NewY,
		WindowFlags
	);
	debugf( TEXT("Created SDL window.") );

	// Create the render device.
	HoldCount++;
	RenDev = ConstructObject<URenderDevice>( RenderClass, this );
	if( RenDev->Init( this, NewX, NewY, ColorBytes, Fullscreen ) ){
		if( GIsRunning )
			Actor->GetLevel()->DetailChange( RenDev->HighDetailActors );
	}
	else{
		debugf( NAME_Log, LocalizeError("Failed3D") );
		delete RenDev;
		RenDev = NULL;
	}
	HoldCount--;
	check(RenDev);
	GRenderDevice = RenDev;
	debugf( TEXT("Created render device.") );

	// Finish up.
	UpdateWindowFrame();
	Repaint( 1 );

	unguard;
}

//
// Close a viewport window.  Assumes that the viewport has been openened with
// OpenViewportWindow.  Does not affect the viewport's object, only the
// platform-specific information associated with it.
//
void USDLViewport::CloseWindow()
{
	guard(USDLViewport::CloseWindow);

	if( RenDev )
	{
		RenDev->Exit();
		delete RenDev;
		RenDev = NULL;
	}

	if( Window )
	{
		SDL_DestroyWindow( Window );
		Window = NULL;
	}

	unguard;
}

//
// Repaint the viewport.
//
void USDLViewport::Repaint( UBOOL Blit )
{
	guard(USDLViewport::Repaint);
	GetOuterUSDLClient()->Engine->Draw( this, Blit );
	unguard;
}

//
// Return whether fullscreen.
//
UBOOL USDLViewport::IsFullscreen()
{
	guard(USDLViewport::IsFullscreen);
	return (BlitFlags & BLIT_Fullscreen)!=0;
	unguard;
}

//
// Set the mouse cursor according to Unreal or UnrealEd's mode, or to
// an hourglass if a slow task is active.
//
void USDLViewport::SetModeCursor()
{
	guard(USDLViewport::SetModeCursor);
	// FIXME: Implement for SDL
	unguard;
}

//
// Update user viewport interface.
//
void USDLViewport::UpdateWindowFrame()
{
	guard(USDLViewport::UpdateWindowFrame);

	// If not a window, exit.
	if( HoldCount || !Window || (BlitFlags&BLIT_Fullscreen) || (BlitFlags&BLIT_Temporary) )
		return;

	unguard;
}

//
// Return the viewport's window.
//
void* USDLViewport::GetWindow()
{
	guard(USDLViewport::GetWindow);
	return Window;
	unguard;
}

//
// Return the viewport's display.
//
void* USDLViewport::GetServer()
{
	guard(USDLViewport::GetServer);
	return NULL;
	unguard;
}

//
// If the cursor is currently being captured, stop capturing, clipping, and 
// hiding it, and move its position back to where it was when it was initially
// captured.
//
void USDLViewport::SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL OnlyFocus )
{
	guard(USDLViewport::SetMouseCapture);

	bWindowsMouseAvailable = !Capture;

	if( !OnlyFocus )
		SDL_SetWindowGrab( Window, Capture ? SDL_TRUE : SDL_FALSE );

	unguard;
}

//
// Update input for this viewport.
//
void USDLViewport::UpdateInput( UBOOL Reset )
{
	guard(USDLViewport::UpdateInput);
	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport modes.
-----------------------------------------------------------------------------*/

//
// Resize the viewport.
//
UBOOL USDLViewport::ResizeViewport( DWORD NewBlitFlags, INT NewX, INT NewY, INT NewColorBytes )
{
	guard(USDLViewport::ResizeViewport);
	USDLClient* Client = GetOuterUSDLClient();

	debugf( TEXT("Resizing SDL viewport. X: %i Y: %i"), NewX, NewY );

	// Save current viewport on audio device.
	UViewport* SavedViewport = NULL;
	if( Client->Engine->Audio && !GIsEditor && !(GetFlags() & RF_Destroyed) )
		SavedViewport = Client->Engine->Audio->GetViewport();

	// Apply new parameters.
	if( NewColorBytes!=INDEX_NONE )
		ColorBytes = NewColorBytes;
	BlitFlags = NewBlitFlags;

	// Grab mouse if fullscreen.
	if( IsFullscreen() ){
		// FIXME: SDL_SetWindowDisplayMode
		SDL_SetWindowFullscreen( Window, SDL_TRUE );
		SetMouseCapture( 1, 1, 0 );
	}
	else{
		SDL_SetWindowFullscreen( Window, SDL_FALSE );
		// FIXME: Check that ColorBytes matches the current mode.
		// SDL2 no longer allows changing modes when windowed.
		SDL_SetWindowSize( Window, SizeX, SizeY );
		SetMouseCapture( 0, 0, 0 );
	}

	// Make this viewport current and update its title bar.
	GetOuterUClient()->MakeCurrent( this );	

	// Restore saved viewport on audio device.
	if( SavedViewport && SavedViewport!=Client->Engine->Audio->GetViewport() )
		Client->Engine->Audio->SetViewport( SavedViewport );

	// Update the window.
	UpdateWindowFrame();

	// Save new config.
	if( RenDev && !GIsEditor )
	{
		if( IsFullscreen() )
		{
			Client->FullscreenViewportX = SizeX;
			Client->FullscreenViewportY = SizeY;
		}
		else
		{
			Client->WindowedViewportX = SizeX;
			Client->WindowedViewportY = SizeY;
		}
		Client->SaveConfig();
	}

	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
