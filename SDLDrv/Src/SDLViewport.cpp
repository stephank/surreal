/*=============================================================================
	SDLViewport.cpp: USDLViewport code.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel (based on XDrv)
		
		* 00/04/04 John Fulmer<jfulmer@appin.org>
			- Queries SDL_GetVideoInfo() for correct
			  color depth info and populates ColorBytes with it.
			
			NOTE: Apparently, GlideDrv also fixes ColorBytes
			with whatever depth Glide wants to use, so this
			should be safe to use with GlideDrv.
			
			- Locked console dropbox to match current bpp.
			- Fixed broken keyboard code to more closly follow
			  code in XDrv.

		* 00/04/05 Daniel Vogel
			- querying bitdepth now matches the XDrv's code
			- indentation fixes
		
		* 00/04/06 John Fulmer
			- SDL longer creates GLX context if Glide is
			  used as driver. 

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
,	ViewportStatus( SDL_ViewportOpening )
{
	guard(USDLViewport::USDLViewport);

	// Query current bit depth
	int depth = SDL_GetVideoInfo()->vfmt->BitsPerPixel;
	switch (depth)
	{
		case 8 :	
			ColorBytes = 2;
			break;

		case 15 :	
		case 16 :
			ColorBytes = 2;
			Caps |= CC_RGB565;
			break;

		case 24 :
		case 32 :
			ColorBytes = 4;
    			break;

		default :	
			ColorBytes = 2;
			Caps |= CC_RGB565;
	}
	
	// Zero out Keysym map.
	for (INT i=0; i<65536; i++)
		KeysymMap[i] = 0;
		
	// Remap important keys.

	// TTY Functions.
	KeysymMap[SDLK_BACKSPACE]= IK_Backspace;
	KeysymMap[SDLK_TAB]	= IK_Tab;
	KeysymMap[SDLK_RETURN]	= IK_Enter;
	KeysymMap[SDLK_PAUSE]	= IK_Pause;
	KeysymMap[SDLK_ESCAPE]	= IK_Escape;
	KeysymMap[SDLK_DELETE]	= IK_Delete;
	KeysymMap[SDLK_INSERT]  = IK_Insert;

	// Modifiers.
	KeysymMap[SDLK_LSHIFT]	= IK_LShift;
	KeysymMap[SDLK_RSHIFT]	= IK_RShift;
	KeysymMap[SDLK_LCTRL]	= IK_LControl;
	KeysymMap[SDLK_RCTRL]	= IK_RControl;
	KeysymMap[SDLK_LMETA]	= IK_Alt;
	KeysymMap[SDLK_RMETA]	= IK_Alt;
	KeysymMap[SDLK_LALT]	= IK_Alt;
	KeysymMap[SDLK_RALT]	= IK_Alt;
	
	// Special remaps.
	KeysymMap[SDLK_BACKQUOTE]= IK_Tilde;

	// Misc function keys.
	KeysymMap[SDLK_F1]	= IK_F1;
	KeysymMap[SDLK_F2]	= IK_F2;
	KeysymMap[SDLK_F3]	= IK_F3;
	KeysymMap[SDLK_F4]	= IK_F4;
	KeysymMap[SDLK_F5]	= IK_F5;
	KeysymMap[SDLK_F6]	= IK_F6;
	KeysymMap[SDLK_F7]	= IK_F7;
	KeysymMap[SDLK_F8]	= IK_F8;
	KeysymMap[SDLK_F9]	= IK_F9;
	KeysymMap[SDLK_F10]	= IK_F10;
	KeysymMap[SDLK_F11]	= IK_F11;
	KeysymMap[SDLK_F12]	= IK_F12;
	KeysymMap[SDLK_F13]	= IK_F13;
	KeysymMap[SDLK_F14]	= IK_F14;
	KeysymMap[SDLK_F15]	= IK_F15;

	// Cursor control and motion.
	KeysymMap[SDLK_HOME]	= IK_Home;
	KeysymMap[SDLK_LEFT]	= IK_Left;
	KeysymMap[SDLK_UP]	= IK_Up;
	KeysymMap[SDLK_RIGHT]	= IK_Right;
	KeysymMap[SDLK_DOWN]	= IK_Down;
	KeysymMap[SDLK_PAGEUP]	= IK_PageUp;
	KeysymMap[SDLK_PAGEDOWN]= IK_PageDown;
	KeysymMap[SDLK_END]	= IK_End;

	// Keypad functions and numbers.
	KeysymMap[SDLK_KP_ENTER]= IK_Enter;
	KeysymMap[SDLK_KP0]	= IK_NumPad0;
	KeysymMap[SDLK_KP1]	= IK_NumPad1;
	KeysymMap[SDLK_KP2]	= IK_NumPad2;
	KeysymMap[SDLK_KP3]	= IK_NumPad3;
	KeysymMap[SDLK_KP4]	= IK_NumPad4;
	KeysymMap[SDLK_KP5]	= IK_NumPad5;
	KeysymMap[SDLK_KP6]	= IK_NumPad6;
	KeysymMap[SDLK_KP7]	= IK_NumPad7;
	KeysymMap[SDLK_KP8]	= IK_NumPad8;
	KeysymMap[SDLK_KP9]	= IK_NumPad9;
	KeysymMap[SDLK_KP_MULTIPLY]= IK_GreyStar;
	KeysymMap[SDLK_KP_PLUS]	= IK_GreyPlus;
	// dv: TODO - what is KP_Separator in SDL???
	// KeysymMap[XK_KP_Separator]	= IK_Separator;
	KeysymMap[SDLK_KP_MINUS]= IK_GreyMinus;
	KeysymMap[SDLK_KP_PERIOD]= IK_NumPadPeriod;
	KeysymMap[SDLK_KP_DIVIDE]= IK_GreySlash;

	// Other
	KeysymMap[SDLK_MINUS]	= IK_Minus;
	KeysymMap[SDLK_EQUALS]	= IK_Equals;
       
	
	// Zero out ShiftMask map.
	for (i=0; i<256; i++)
		ShiftMaskMap[i] = 0;

	// ShiftMask map.	
	ShiftMaskMap['1']	= '!';
	ShiftMaskMap['2']	= '@';
	ShiftMaskMap['3']	= '#';
	ShiftMaskMap['4']	= '$';
	ShiftMaskMap['5']	= '%';
	ShiftMaskMap['6']	= '^';
	ShiftMaskMap['7']	= '&';
	ShiftMaskMap['8']	= '*';
	ShiftMaskMap['9']	= '(';
	ShiftMaskMap['0']	= ')';
	ShiftMaskMap['-']	= '_';
	ShiftMaskMap['=']	= '+';
	ShiftMaskMap['[']	= '{';
	ShiftMaskMap[']']	= '}';
	ShiftMaskMap['\\']	= '|';
	ShiftMaskMap[';']	= ':';
	ShiftMaskMap['\'']	= '\"'; //" 
	ShiftMaskMap[',']	= '<';
	ShiftMaskMap['.']	= '>';
	ShiftMaskMap['/']	= '?';

	// WM_CHAR allowables.
	for (i=0; i<256; i++)
		WMCharMap[i] = 0;
	for (i='A'; i<='Z'; i++)
		WMCharMap[i] = 1;
	for (i='a'; i<='z'; i++)
		WMCharMap[i] = 1;
	WMCharMap[IK_Backspace]	= 1;
	WMCharMap[IK_Space]	= 1;
	WMCharMap[IK_Tab]	= 1;
	WMCharMap[IK_Enter]	= 1;
	WMCharMap['1']		= 1;
	WMCharMap['2']		= 1;
	WMCharMap['3']		= 1;
	WMCharMap['4']		= 1;
	WMCharMap['5']		= 1;
	WMCharMap['6']		= 1;
	WMCharMap['7']		= 1;
	WMCharMap['8']		= 1;
	WMCharMap['9']		= 1;
	WMCharMap['0']		= 1;
	WMCharMap['-']		= 1;
	WMCharMap['=']		= 1;
	WMCharMap['[']		= 1;
	WMCharMap[']']		= 1;
	WMCharMap['\\']		= 1;
	WMCharMap[';']		= 1;
	WMCharMap['\'']		= 1;
	WMCharMap[',']		= 1;
	WMCharMap['.']		= 1;
	WMCharMap['/']		= 1;
	WMCharMap['!']		= 1;
	WMCharMap['@']		= 1;
	WMCharMap['#']		= 1;
	WMCharMap['$']		= 1;
	WMCharMap['%']		= 1;
	WMCharMap['^']		= 1;
	WMCharMap['&']		= 1;
	WMCharMap['*']		= 1;
	WMCharMap['(']		= 1;
	WMCharMap[')']		= 1;
	WMCharMap['_']		= 1;
	WMCharMap['+']		= 1;
	WMCharMap['{']		= 1;
	WMCharMap['}']		= 1;
	WMCharMap['|']		= 1;
	WMCharMap[':']		= 1;
	WMCharMap['\"']		= 1; //"
	WMCharMap['<']		= 1;
	WMCharMap['>']		= 1;
	WMCharMap['?']		= 1;
	
	// Zero out KeyRepeat map.
	for (i=0; i<256; i++)
		KeyRepeatMap[i] = 0;

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
	Command line.
-----------------------------------------------------------------------------*/

//
// Command line.
//
UBOOL USDLViewport::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(USDLViewport::Exec);
	if( UViewport::Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EndFullscreen")) )
	{
		EndFullscreen();
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("ToggleFullscreen")) )
	{
		ToggleFullscreen();
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("Iconify")) )
	{
		Iconify();
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetCurrentRes")) )
	{
		Ar.Logf( TEXT("%ix%i"), SizeX, SizeY, (ColorBytes?ColorBytes:2)*8 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetCurrentColorDepth")) )
	{
		Ar.Logf( TEXT("%i"), (ColorBytes?ColorBytes:2)*8 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetColorDepths")) )
	{
		Ar.Logf( TEXT("%i"), ColorBytes * 8 );	// Lock bpp to current
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetCurrentRenderDevice")) )
	{
		Ar.Log( RenDev->GetClass()->GetPathName() );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SetRenderDevice")) )
	{
		FString Saved = RenDev->GetClass()->GetPathName();
		INT SavedSizeX=SizeX, SavedSizeY=SizeY, SavedColorBytes=ColorBytes, SavedFullscreen=((BlitFlags & BLIT_Fullscreen)!=0);
		TryRenderDevice( Cmd, SizeX, SizeY, ColorBytes, SavedFullscreen );
		if( !RenDev )
		{
			TryRenderDevice( *Saved, SavedSizeX, SavedSizeY, SavedColorBytes, SavedFullscreen );
			check(RenDev);
			Ar.Log(TEXT("0"));
		}
		else Ar.Log(TEXT("1"));
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GetRes")) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SetRes")) )
	{
		INT X=appAtoi(Cmd);
		TCHAR* CmdTemp = (TCHAR*) (appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT(""));
		INT Y=appAtoi(CmdTemp);
		Cmd = CmdTemp;
		CmdTemp = (TCHAR*) (appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT(""));
		INT C=appAtoi(CmdTemp);
		INT NewColorBytes = C ? C/8 : ColorBytes;
		if( X && Y )
		{
			HoldCount++;
			UBOOL Result = RenDev->SetRes( X, Y, NewColorBytes, IsFullscreen() );
			HoldCount--;
			if( !Result )
				EndFullscreen();
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("Preferences")) )
	{
		// No preferences window.
	
		return 1;
	}
	else return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	Window openining and closing.
-----------------------------------------------------------------------------*/

//
// Open this viewport's window.
//
void USDLViewport::OpenWindow( DWORD InParentWindow, UBOOL IsTemporary, INT NewX, INT NewY, INT OpenX, INT OpenY )
{
	guard(USDLViewport::OpenWindow);
	check(Actor);
	check(!HoldCount);
	USDLClient* C = GetOuterUSDLClient();

	debugf( TEXT("Opening SDL viewport.") );

	// Create or update the window.
	SizeX = C->FullscreenViewportX;
	SizeY = C->FullscreenViewportY;

	FindAvailableModes();

	// Create rendering device.
	if( !RenDev && !GIsEditor && !ParseParam(appCmdLine(),TEXT("nohard")) )
		TryRenderDevice( TEXT("ini:Engine.Engine.GameRenderDevice"), NewX, NewY, ColorBytes, C->StartupFullscreen );
	check(RenDev);
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
	unguard;
}

/*-----------------------------------------------------------------------------
	USDLViewport operations.
-----------------------------------------------------------------------------*/

//
// Find all available DirectDraw modes for a certain number of color bytes.
//
void USDLViewport::FindAvailableModes()
{
	guard(UWindowsViewport::FindAvailableModes);
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
	// FIXME: Implement for X
	unguard;
}

//
// Update user viewport interface.
//
void USDLViewport::UpdateWindowFrame()
{
	guard(USDLViewport::UpdateWindowFrame);

	// If not a window, exit.
	if( HoldCount || (BlitFlags&BLIT_Fullscreen) || (BlitFlags&BLIT_Temporary) )
		return;

	unguard;
}

//
// Return the viewport's window.
//
void* USDLViewport::GetWindow()
{
	guard(USDLViewport::GetWindow);
     	return NULL;
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

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

//
// Input event router.
//
UBOOL USDLViewport::CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta )
{
	guard(USDLViewport::CauseInputEvent);

	// Route to engine if a valid key; some keyboards produce key
	// codes that go beyond IK_MAX.
	if( iKey>=0 && iKey<IK_MAX )
		return GetOuterUSDLClient()->Engine->InputEvent( this, (EInputKey)iKey, Action, Delta );
	else
		return 0;

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

	if( OnlyFocus )
		return;

	if( Capture )
	{
		CaptureInputs();
	} else {
		ReleaseInputs();
	}
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
	Lock and Unlock.
-----------------------------------------------------------------------------*/

//
// Lock the viewport window and set the approprite Screen and RealScreen fields
// of Viewport.  Returns 1 if locked successfully, 0 if failed.  Note that a
// lock failing is not a critical error; it's a sign that a DirectDraw mode
// has ended or the user has closed a viewport window.
//
UBOOL USDLViewport::Lock( FPlane FlashScale, FPlane FlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* HitData, INT* HitSize )
{
	guard(USDLViewport::LockWindow);
	USDLClient* Client = GetOuterUSDLClient();
	clock(Client->DrawCycles);

	// Success here, so pass to superclass.
	unclock(Client->DrawCycles);
	return UViewport::Lock(FlashScale,FlashFog,ScreenClear,RenderLockFlags,HitData,HitSize);

	unguard;
}

//
// Unlock the viewport window.  If Blit=1, blits the viewport's frame buffer.
//
void USDLViewport::Unlock( UBOOL Blit )
{
	guard(USDLViewport::Unlock);

	UViewport::Unlock( Blit );

	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport modes.
-----------------------------------------------------------------------------*/

//
// Try switching to a new rendering device.
//
void USDLViewport::TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen )
{
	guard(USDLViewport::TryRenderDevice);

	// Shut down current rendering device.
	if( RenDev )
	{
		RenDev->Exit();
		delete RenDev;
		RenDev = NULL;
	}

	// Use appropriate defaults.
	USDLClient* C = GetOuterUSDLClient();
	if( NewX==INDEX_NONE )
		NewX = Fullscreen ? C->FullscreenViewportX : C->WindowedViewportX;
	if( NewY==INDEX_NONE )
		NewY = Fullscreen ? C->FullscreenViewportY : C->WindowedViewportY;

	// Find device driver.
	UClass* RenderClass = UObject::StaticLoadClass( URenderDevice::StaticClass(), NULL, ClassName, NULL, 0, NULL );
	if( RenderClass )
	{
		debugf( TEXT("Loaded render device class.") );
		HoldCount++;
		RenDev = ConstructObject<URenderDevice>( RenderClass, this );
		if( RenDev->Init( this, NewX, NewY, NewColorBytes, Fullscreen ) )
		{
			if( GIsRunning )
				Actor->GetLevel()->DetailChange( RenDev->HighDetailActors );
		}
		else
		{
			debugf( NAME_Log, LocalizeError("Failed3D") );
			delete RenDev;
			RenDev = NULL;
		}
		HoldCount--;
	}
	GRenderDevice = RenDev;
	unguard;
}

//
// If in fullscreen mode, end it and return to Windows.
//
void USDLViewport::EndFullscreen()
{
	guard(USDLViewport::EndFullscreen);
	//USDLClient* Client = GetOuterUSDLClient();
	debugf(NAME_Log, TEXT("Ending fullscreen mode by request."));
	if( RenDev && RenDev->FullscreenOnly )
	{
		// This device doesn't support fullscreen, so use a window-capable rendering device.
		TryRenderDevice( TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 0 );
		check(RenDev);
	}
	else if( RenDev && (BlitFlags & BLIT_OpenGL) )
	{
		RenDev->SetRes( INDEX_NONE, INDEX_NONE, ColorBytes, 0 );
	}
	else
	{
		ResizeViewport( BLIT_DibSection );
	}
	UpdateWindowFrame();
	unguard;
}

//
// Toggle fullscreen.
//
void USDLViewport::ToggleFullscreen()
{
	guard(USDLViewport::ToggleFullscreen);
	if( BlitFlags & BLIT_Fullscreen )
	{
		EndFullscreen();
	}
	else if( !(Actor->ShowFlags & SHOW_ChildWindow) )
	{
		debugf(TEXT("AttemptFullscreen"));
		TryRenderDevice( TEXT("ini:Engine.Engine.GameRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1 );
		if( !RenDev )
			TryRenderDevice( TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 1 );
		if( !RenDev )
			TryRenderDevice( TEXT("ini:Engine.Engine.WindowedRenderDevice"), INDEX_NONE, INDEX_NONE, ColorBytes, 0 );
	}
	unguard;
}

//
// Resize the viewport.
//
UBOOL USDLViewport::ResizeViewport( DWORD NewBlitFlags, INT InNewX, INT InNewY, INT InNewColorBytes )
{
	guard(USDLViewport::ResizeViewport);
	USDLClient* Client = GetOuterUSDLClient();

	debugf( TEXT("Resizing SDL viewport. X: %i Y: %i"), InNewX, InNewY );

	// Remember viewport.
	UViewport* SavedViewport = NULL;
	if( Client->Engine->Audio && !GIsEditor && !(GetFlags() & RF_Destroyed) )
		SavedViewport = Client->Engine->Audio->GetViewport();

	// Accept default parameters.
	INT NewX          = InNewX         ==INDEX_NONE ? SizeX      : InNewX;
	INT NewY          = InNewY         ==INDEX_NONE ? SizeY      : InNewY;
	INT NewColorBytes = InNewColorBytes==INDEX_NONE ? ColorBytes : InNewColorBytes;

	// Default resolution handling.
	NewX = InNewX!=INDEX_NONE ? InNewX : (NewBlitFlags&BLIT_Fullscreen) ? Client->FullscreenViewportX : Client->WindowedViewportX;
	NewY = InNewX!=INDEX_NONE ? InNewY : (NewBlitFlags&BLIT_Fullscreen) ? Client->FullscreenViewportY : Client->WindowedViewportY;

	INT VideoFlags = SDL_SWSURFACE;

	// Pull current driver string 
	FString CurrentDriver = RenDev->GetClass()->GetPathName();

	// if ! Glide, Set up the SDL OpenGL contexts
	if (!( CurrentDriver == "GlideDrv.GlideRenderDevice" )) 
		VideoFlags = SDL_OPENGL;

	if( NewBlitFlags & BLIT_Fullscreen )
	{
		// Grab mouse and keyboard.
		SetMouseCapture( 1, 1, 0 );
		VideoFlags |= SDL_FULLSCREEN;
	} else {
		// Changing to windowed mode.
		SetMouseCapture( 0, 0, 0 );
	}

	if ( SDL_SetVideoMode( NewX, NewY, 0, VideoFlags ) == NULL ) {
			fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
			exit(1);
	}	

	if ( NewBlitFlags & BLIT_Fullscreen )
		SDL_WM_GrabInput(SDL_GRAB_ON);

	// Window has been created.
	ViewportStatus = SDL_ViewportNormal;

	// Make this viewport current and update its title bar.
	GetOuterUClient()->MakeCurrent( this );	

	// Update audio.
	if( SavedViewport && SavedViewport!=Client->Engine->Audio->GetViewport() )
		Client->Engine->Audio->SetViewport( SavedViewport );

	// Update the window.
	UpdateWindowFrame();

	// Set new info.
	// DWORD OldBlitFlags = BlitFlags;
	BlitFlags          = NewBlitFlags & ~BLIT_ParameterFlags;
	SizeX              = NewX;
	SizeY              = NewY;
	ColorBytes         = NewColorBytes ? NewColorBytes : ColorBytes;

	// Save info.
	if( RenDev && !GIsEditor )
	{
		if( NewBlitFlags & BLIT_Fullscreen )
		{
			if( NewX && NewY )
			{
				Client->FullscreenViewportX = NewX;
				Client->FullscreenViewportY = NewY;
			}
		}
		else
		{
			if( NewX && NewY )
			{
				Client->WindowedViewportX = NewX;
				Client->WindowedViewportY = NewY;
			}
		}
		Client->SaveConfig();
	}
	return 1;
	unguard;
}

void USDLViewport::Tick()
{
	guard(USDLViewport::Tick);
	USDLClient* Client = GetOuterUSDLClient();

	// Keyboard.
	EInputKey Key = IK_None;
	EInputKey UpperCase = IK_None;
	EInputKey LowerCase = IK_None;

	// Mouse movement management.
	UBOOL MouseMoved = false;
	INT DX = 0, DY = 0;

        // Give some time to gather input...
        // SDL_Delay(10);

	char* sdl_error;
	SDL_Event Event;
	while( SDL_PollEvent( &Event ) ) 
	{
		switch( Event.type )
		{
		case SDL_KEYDOWN:
			// Reset timer.
			RepeatTimer = appSeconds();
			LastKey = true;

			Key = (EInputKey) Event.key.keysym.sym;
			
			// Convert to UpperCase/LowerCase values. If not
			// a letter, leave alone
			if ((Key >= ((EInputKey) SDLK_a)) && (Key <= ((EInputKey) SDLK_z))) 
			{
				LowerCase = Key;
				UpperCase = (EInputKey) (Key - 32);
			} 
			else if ((Key >= ((EInputKey) (SDLK_a - 32))) && (Key <= ((EInputKey) (SDLK_z - 32)))) 
			{
				UpperCase = Key;
				LowerCase = (EInputKey) (Key + 32);
			} 
			else 	
			{
				UpperCase = Key;
				LowerCase = Key;
			}
			// Always send upper case letters to the system.
			// The engine NEVER processes lowercase letters, 
			// since lowercase keycodes are mapped to the F keys

			Key = UpperCase;

			// Check the Keysym map.
			if (KeysymMap[Key] != 0)
				Key = (EInputKey) KeysymMap[Key];
	
			// Send key to input system. 
			if ( Key != IK_None )
				CauseInputEvent( Key, IST_Press );

			// Check for shift modifier. Restore case value
			// of letters. This is used for text boxes and
			// chat modes.
			if (Event.key.keysym.mod & KMOD_SHIFT) 
			{
				Key = UpperCase;
				if (ShiftMaskMap[Key] != 0)
					// Shift it.
					Key = (EInputKey) ShiftMaskMap[Key];
			} 
			else // Restore it to lowercase
			{		
				Key = LowerCase;
			}

			// Reset to origional values. In case below gets 
			// munged by the above code. Shouldn't happen.
			if (Key == (EInputKey) SDLK_BACKSPACE)
				Key = IK_Backspace;
			if (Key ==(EInputKey) SDLK_TAB)
				Key = IK_Tab;
			if (Key == (EInputKey) SDLK_RETURN)
				Key = IK_Enter;

			if (WMCharMap[Key] == 1)
			{
				KeyRepeatMap[Key] = 1;
				// Send to text processor	
				Client->Engine->Key( this, Key );
			}
			break;

		case SDL_KEYUP:
			// Get key code.
			Key = (EInputKey) Event.key.keysym.sym;
			
			// Convert to UpperCase/LowerCase
			if ((Key >= ((EInputKey) SDLK_a)) && (Key <= ((EInputKey) SDLK_z))) 
			{
				LowerCase = Key;
				UpperCase = (EInputKey) (Key - 32);
			} 
			else if ((Key >= ((EInputKey) (SDLK_a - 32))) && (Key <= ((EInputKey) (SDLK_z - 32)))) 
			{
				UpperCase = Key;
				LowerCase = (EInputKey) (Key + 32);
			}
			else 
			{
				UpperCase = Key;
				LowerCase = Key;
			}
			
			// Always send upper case letters to the system.
			Key = UpperCase;

			// Check the Keysym map.
			if (KeysymMap[Key] != 0)
				Key = (EInputKey) KeysymMap[Key];

			// Send key to input system.
			CauseInputEvent( Key, IST_Release );

			// Release all types of this key.
			if (Key ==(EInputKey) SDLK_BACKSPACE)
				Key = IK_Backspace;
			if (Key == (EInputKey) SDLK_TAB)
				Key = IK_Tab;
			if (Key ==(EInputKey) SDLK_RETURN)
				Key = IK_Enter;

			// Turn off repeating. Needed for
			// chat modes.
			KeyRepeatMap[Key] = 0;
			KeyRepeatMap[ShiftMaskMap[Key]] = 0;
			KeyRepeatMap[ShiftMaskMap[LowerCase]] = 0;
			KeyRepeatMap[LowerCase] = 0;
			break;
		
		case SDL_MOUSEBUTTONDOWN:
			switch (Event.button.button)
			{
				case 1:
					Key = IK_LeftMouse;
					break;
				case 2:
					Key = IK_MiddleMouse;
					break;
				case 3:
					Key = IK_RightMouse;
					break;
				case 4:
					Key = IK_MouseWheelUp;
					break;
				case 5:
					Key = IK_MouseWheelDown;
					break;
			}
			// Send to input system.
			CauseInputEvent( Key, IST_Press );
			break;

		case SDL_MOUSEBUTTONUP:
			switch (Event.button.button)
			{
				case 1:
					Key = IK_LeftMouse;
					break;
				case 2:
					Key = IK_MiddleMouse;
					break;
				case 3:
					Key = IK_RightMouse;
					break;
				case 4:
					Key = IK_MouseWheelUp;
						break;
				case 5:
					Key = IK_MouseWheelDown;
					break;
			}
			// Send to input system.
			CauseInputEvent( Key, IST_Release );
			break;

		case SDL_MOUSEMOTION:
			MouseMoved = true;		
			DX += Event.motion.xrel;
			DY += Event.motion.yrel;
			break;
	
		default:;
		}
	}

	if (Iconified)
		return;

	// Deliver mouse behavior to the engine.
	if (MouseMoved)
	{
		// Send to input subsystem.
		if( DX )
			CauseInputEvent( IK_MouseX, IST_Axis, +DX );
		if( DY )
			CauseInputEvent( IK_MouseY, IST_Axis, -DY );
	}
	
	// Send WM_CHAR for down keys.
	if ( LastKey && (appSeconds() - RepeatTimer < 0.5) )
		return;
	LastKey = false;
	if ( appSeconds() - RepeatTimer < 0.1 )
		return;

	RepeatTimer = appSeconds();
	for (INT i=0; i<256; i++)
		if (KeyRepeatMap[i] != 0)
		{
			if (i == IK_Backspace)
			{
				CauseInputEvent( i, IST_Press );
				CauseInputEvent( i, IST_Release );				
			}
			else
				Client->Engine->Key( this, (EInputKey) i );
		}
	
	unguard;
}

void USDLViewport::Iconify()
{
	guard(USDLViewport::Iconify);

	if (Iconified)
		return;
		
	Iconified = true;

	// Pause the game if applicable.
	if( GIsRunning )
		Exec( TEXT("SETPAUSE 1"), *this );

	// Release the mouse.
	SetMouseCapture( 0, 0, 0 );
	SetDrag( 0 );

	// Reset the input buffer.
	Input->ResetInput();

	// End fullscreen.
	if( BlitFlags & BLIT_Fullscreen )
		EndFullscreen();
	GetOuterUClient()->MakeCurrent( NULL );

	// Iconify the window.
	SDL_WM_IconifyWindow();

	unguard;
}

void USDLViewport::CaptureInputs()
{
	guard(USDLViewport::CaptureInputs);
	
	SDL_ShowCursor( false );
	
	unguard;
}

void USDLViewport::ReleaseInputs()
{
	guard(USDLViewport::ReleaseInputs);
	
	SDL_ShowCursor( true );
	
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
