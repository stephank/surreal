/*=============================================================================
	SDLClient.cpp: USDLClient code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel (based on XDrv).

		4/2/00 John Fulmer<jfulmer@appin.org>
			-Changed 'defaultres' to a more default 640x480
=============================================================================*/

#include "SDLDrv.h"

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USDLClient);

/*-----------------------------------------------------------------------------
	UWindowsClient implementation.
-----------------------------------------------------------------------------*/

//
// USDLClient constructor.
//
USDLClient::USDLClient()
{
	guard(USDLClient::USDLClient);

	// Zero out Keysym map.
	for (INT i=0; i<65536; i++)
		KeysymMap[i] = IK_None;

	// Remap important keys.
	KeysymMap[SDL_SCANCODE_BACKSPACE]= IK_Backspace;
	KeysymMap[SDL_SCANCODE_TAB]		= IK_Tab;
	KeysymMap[SDL_SCANCODE_RETURN]	= IK_Enter;
	KeysymMap[SDL_SCANCODE_ESCAPE]	= IK_Escape;
	KeysymMap[SDL_SCANCODE_SPACE]	= IK_Space;

	// Special remaps.
	KeysymMap[SDL_SCANCODE_GRAVE]	= IK_Tilde;

	// The following are all the SDL keycodes that don't generate characters,
	// that we can map to something in the engine.
	KeysymMap[SDL_SCANCODE_CAPSLOCK]= IK_CapsLock;

	KeysymMap[SDL_SCANCODE_F1]		= IK_F1;
	KeysymMap[SDL_SCANCODE_F2]		= IK_F2;
	KeysymMap[SDL_SCANCODE_F3]		= IK_F3;
	KeysymMap[SDL_SCANCODE_F4]		= IK_F4;
	KeysymMap[SDL_SCANCODE_F5]		= IK_F5;
	KeysymMap[SDL_SCANCODE_F6]		= IK_F6;
	KeysymMap[SDL_SCANCODE_F7]		= IK_F7;
	KeysymMap[SDL_SCANCODE_F8]		= IK_F8;
	KeysymMap[SDL_SCANCODE_F9]		= IK_F9;
	KeysymMap[SDL_SCANCODE_F10]		= IK_F10;
	KeysymMap[SDL_SCANCODE_F11]		= IK_F11;
	KeysymMap[SDL_SCANCODE_F12]		= IK_F12;

	KeysymMap[SDL_SCANCODE_PRINTSCREEN]= IK_PrintScrn;
	KeysymMap[SDL_SCANCODE_SCROLLLOCK]= IK_ScrollLock;
	KeysymMap[SDL_SCANCODE_PAUSE]	= IK_Pause;
	KeysymMap[SDL_SCANCODE_INSERT]  = IK_Insert;
	KeysymMap[SDL_SCANCODE_HOME]	= IK_Home;
	KeysymMap[SDL_SCANCODE_PAGEUP]	= IK_PageUp;
	KeysymMap[SDL_SCANCODE_DELETE]	= IK_Delete;
	KeysymMap[SDL_SCANCODE_END]		= IK_End;
	KeysymMap[SDL_SCANCODE_PAGEDOWN]= IK_PageDown;
	KeysymMap[SDL_SCANCODE_RIGHT]	= IK_Right;
	KeysymMap[SDL_SCANCODE_LEFT]	= IK_Left;
	KeysymMap[SDL_SCANCODE_DOWN]	= IK_Down;
	KeysymMap[SDL_SCANCODE_UP]		= IK_Up;

	KeysymMap[SDL_SCANCODE_NUMLOCKCLEAR]= IK_NumLock;
	KeysymMap[SDL_SCANCODE_KP_DIVIDE]= IK_GreySlash;
	KeysymMap[SDL_SCANCODE_KP_MULTIPLY]= IK_GreyStar;
	KeysymMap[SDL_SCANCODE_KP_MINUS]= IK_GreyMinus;
	KeysymMap[SDL_SCANCODE_KP_PLUS]	= IK_GreyPlus;
	KeysymMap[SDL_SCANCODE_KP_ENTER]= IK_Enter;
	KeysymMap[SDL_SCANCODE_KP_1]	= IK_NumPad1;
	KeysymMap[SDL_SCANCODE_KP_2]	= IK_NumPad2;
	KeysymMap[SDL_SCANCODE_KP_3]	= IK_NumPad3;
	KeysymMap[SDL_SCANCODE_KP_4]	= IK_NumPad4;
	KeysymMap[SDL_SCANCODE_KP_5]	= IK_NumPad5;
	KeysymMap[SDL_SCANCODE_KP_6]	= IK_NumPad6;
	KeysymMap[SDL_SCANCODE_KP_7]	= IK_NumPad7;
	KeysymMap[SDL_SCANCODE_KP_8]	= IK_NumPad8;
	KeysymMap[SDL_SCANCODE_KP_9]	= IK_NumPad9;
	KeysymMap[SDL_SCANCODE_KP_0]	= IK_NumPad0;
	KeysymMap[SDL_SCANCODE_KP_PERIOD]= IK_NumPadPeriod;

	KeysymMap[SDL_SCANCODE_F13]		= IK_F13;
	KeysymMap[SDL_SCANCODE_F14]		= IK_F14;
	KeysymMap[SDL_SCANCODE_F15]		= IK_F15;
	KeysymMap[SDL_SCANCODE_F16]		= IK_F16;
	KeysymMap[SDL_SCANCODE_F17]		= IK_F17;
	KeysymMap[SDL_SCANCODE_F18]		= IK_F18;
	KeysymMap[SDL_SCANCODE_F19]		= IK_F19;
	KeysymMap[SDL_SCANCODE_F20]		= IK_F20;
	KeysymMap[SDL_SCANCODE_F21]		= IK_F21;
	KeysymMap[SDL_SCANCODE_F22]		= IK_F22;
	KeysymMap[SDL_SCANCODE_F23]		= IK_F23;
	KeysymMap[SDL_SCANCODE_F24]		= IK_F24;
	KeysymMap[SDL_SCANCODE_EXECUTE]	= IK_Execute;
	KeysymMap[SDL_SCANCODE_HELP]	= IK_Help;
	KeysymMap[SDL_SCANCODE_SELECT]	= IK_Select;

	KeysymMap[SDL_SCANCODE_CANCEL]	= IK_Cancel;
	KeysymMap[SDL_SCANCODE_SEPARATOR]= IK_Separator;

	KeysymMap[SDL_SCANCODE_LCTRL]	= IK_LControl;
	KeysymMap[SDL_SCANCODE_LSHIFT]	= IK_LShift;
	KeysymMap[SDL_SCANCODE_LALT]	= IK_Alt;
	KeysymMap[SDL_SCANCODE_RCTRL]	= IK_RControl;
	KeysymMap[SDL_SCANCODE_RSHIFT]	= IK_RShift;
	KeysymMap[SDL_SCANCODE_RALT]	= IK_Alt;

	unguard;
}

//
// Static init.
//
void USDLClient::StaticConstructor()
{
	guard(USDLClient::StaticConstructor);
	new(GetClass(),TEXT("StartupFullscreen"), RF_Public)UBoolProperty(CPP_PROPERTY(StartupFullscreen), TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("SlowVideoBuffering"),RF_Public)UBoolProperty(CPP_PROPERTY(SlowVideoBuffering),TEXT("Display"),  CPF_Config );
	unguard;
}

//
// Initialize the platform-specific viewport manager subsystem.
// Must be called after the Unreal object manager has been initialized.
// Must be called before any viewports are created.
//
void USDLClient::Init( UEngine* InEngine )
{
	guard(USDLClient::Init);

	// Init base.
	UClient::Init( InEngine );

	// Note configuration.
	PostEditChange();

	// Default res option.
	if( ParseParam(appCmdLine(),TEXT("defaultres")) )
	{
		WindowedViewportX = FullscreenViewportX = 640;
		WindowedViewportY = FullscreenViewportY = 480;
	}

	// Success.
	debugf( NAME_Init, TEXT("SDLClient initialized.") );
	unguard;
}

//
// Shut down the platform-specific viewport manager subsystem.
//
void USDLClient::Destroy()
{
	guard(USDLClient::Destroy);

	for( INT i=Viewports.Num()-1; i>=0; i-- )
		Viewports( i )->CloseWindow();

	SDL_Quit();
	debugf( NAME_Exit, TEXT("SDL client shut down.") );

	Super::Destroy();

	unguard;
}

//
// Failsafe routine to shut down viewport manager subsystem
// after an error has occured. Not guarded.
//
void USDLClient::ShutdownAfterError()
{
	debugf( NAME_Exit, TEXT("Executing USDLClient::ShutdownAfterError") );

	// Kill the audio subsystem.
	if( Engine && Engine->Audio )
		Engine->Audio->ConditionalShutdownAfterError();

	// Release all viewports.
	for( INT i=Viewports.Num()-1; i>=0; i-- )
		Viewports( i )->ConditionalShutdownAfterError();

	Super::ShutdownAfterError();
}

void USDLClient::NotifyDestroy( void* Src )
{
	guard(USDLClient::NotifyDestroy);

	unguard;
}

//
// Command line.
//
UBOOL USDLClient::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
       guard(USDLClient::Exec);
       if( UClient::Exec( Cmd, Ar ) )
       {
               return 1;
       }
       return 0;
       unguard;
}

//
// Perform timer-tick processing on all visible viewports.  This causes
// all realtime viewports, and all non-realtime viewports which have been
// updated, to be blitted.
//
void USDLClient::Tick()
{
	guard(USDLClient::Tick);
	INT i;
	USDLViewport* Viewport;
	EInputKey Key, InputKey;
	EInputAction Action;

	SDL_Event Event;
	while( SDL_PollEvent( &Event ) )
	{
		switch( Event.type )
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:

			// Find the viewport.
			for( i=0; i<Viewports.Num(); i++ ){
				Viewport = CastChecked<USDLViewport>(Viewports(i));
				if( SDL_GetWindowID(Viewport->Window) == Event.key.windowID )
					break;
			}
			if( i == Viewports.Num() )
				break;

			// Determine the action.
			if( Event.key.state == SDL_PRESSED )
				Action = Event.key.repeat ? IST_Hold : IST_Press;
			else
				Action = IST_Release;

			// See if it's a scancode we've mapped.
			if( (Key = KeysymMap[Event.key.keysym.scancode]) != IK_None ){
				InputKey = Key;
			}
			else{
				// Try to handle as a basic ASCII value.
				Sint32 Keysym = Event.key.keysym.sym;
				Key = (EInputKey) Keysym;

				// Always send upper case letters to the input system.
				// The engine NEVER processes lowercase letters,
				// since lowercase keycodes are mapped to the F* keys.
				if( Keysym >= 'a' && Keysym <= 'z' )
					InputKey = (EInputKey) (Keysym - 32);
				else
					InputKey = (EInputKey) Keysym;
			}

			// Send key to input system.
			if ( Key > IK_None && Key < IK_MAX )
				Engine->InputEvent( Viewport, InputKey, Action );

			// Send to text processor
			Engine->Key( Viewport, Key );

			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:

			// Find the viewport.
			for( i=0; i<Viewports.Num(); i++ ){
				Viewport = CastChecked<USDLViewport>(Viewports(i));
				if( SDL_GetWindowID(Viewport->Window) == Event.button.windowID )
					break;
			}
			if( i == Viewports.Num() )
				break;

			// Determine the action.
			if( Event.button.state == SDL_PRESSED )
				Action = IST_Press;
			else
				Action = IST_Release;

			// Map the button to the engine key.
			switch (Event.button.button)
			{
				case SDL_BUTTON_LEFT:
					InputKey = IK_LeftMouse;
					break;
				case SDL_BUTTON_MIDDLE:
					InputKey = IK_MiddleMouse;
					break;
				case SDL_BUTTON_RIGHT:
					InputKey = IK_RightMouse;
					break;
			}

			// Send to input system.
			Engine->InputEvent( Viewport, InputKey, Action );

			break;

		case SDL_MOUSEWHEEL:

			// Find the viewport.
			for( i=0; i<Viewports.Num(); i++ ){
				Viewport = CastChecked<USDLViewport>(Viewports(i));
				if( SDL_GetWindowID(Viewport->Window) == Event.wheel.windowID )
					break;
			}
			if( i == Viewports.Num() )
				break;

			// Regardless of amount scrolled, we emit just one mouse wheel event
			// to the engine per tick. Collect the total scroll here.
			Viewport->MouseWheelDY += Event.wheel.y;

		case SDL_MOUSEMOTION:

			// Find the viewport.
			for( i=0; i<Viewports.Num(); i++ ){
				Viewport = CastChecked<USDLViewport>(Viewports(i));
				if( SDL_GetWindowID(Viewport->Window) == Event.motion.windowID )
					break;
			}
			if( i == Viewports.Num() )
				break;

			// Collect data for a combined mouse motion event to the engine.
			Viewport->MouseMoved = 1;
			Viewport->MouseX = Event.motion.x;
			Viewport->MouseY = Event.motion.y;
			Viewport->MouseDX += Event.motion.xrel;
			Viewport->MouseDY -= Event.motion.yrel;

			break;

		default:
			break;
		}
	}

	USDLViewport* BestViewport = NULL;
	for( i=0; i<Viewports.Num(); i++ )
	{
		Viewport = CastChecked<USDLViewport>(Viewports(i));

		// Flush collected motion events.
		if( Viewport->MouseWheelDY != 0 ) {
			if( Viewport->MouseWheelDY > 0 )
				Engine->InputEvent( Viewport, IK_MouseWheelUp, IST_Press );
			else
				Engine->InputEvent( Viewport, IK_MouseWheelDown, IST_Press );
			Viewport->MouseWheelDY = 0;
		}
		if( Viewport->MouseMoved ){
			Engine->MouseDelta( Viewport, 0, Viewport->MouseDX, Viewport->MouseDY );
			Engine->MousePosition( Viewport, 0, Viewport->MouseX, Viewport->MouseY );
			Viewport->MouseMoved = 0;
			Viewport->MouseDX = Viewport->MouseDY = 0;
		}

		// Blit any viewports that need blitting.
		check(!Viewport->HoldCount);
		if
		(	Viewport->IsRealtime()
		&&	Viewport->RenDev
		&&	(!BestViewport || Viewport->LastUpdateTime<BestViewport->LastUpdateTime) )
		{
			BestViewport = Viewport;
		}
	}
	if( BestViewport )
		BestViewport->Repaint( 1 );

	unguard;
}

//
// Create a new viewport.
//
UViewport* USDLClient::NewViewport( const FName Name )
{
	guard(USDLClient::NewViewport);
	return new( this, Name )USDLViewport();
	unguard;
}

//
// Enable or disable all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void USDLClient::EnableViewportWindows( DWORD ShowFlags, int DoEnable )
{
	guard(USDLClient::EnableViewportWindows);
	unguard;
}

//
// Show or hide all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void USDLClient::ShowViewportWindows( DWORD ShowFlags, int DoShow )
{
	guard(USDLClient::ShowViewportWindows);
	for( int i=0; i<Viewports.Num(); i++ )
	{
		USDLViewport* Viewport = (USDLViewport*)Viewports(i);
		SDL_ShowWindow( Viewport->Window );
	}
	unguard;
}

//
// Make this viewport the current one.
// If Viewport=0, makes no viewport the current one.
//
void USDLClient::MakeCurrent( UViewport* InViewport )
{
	guard(USDLViewport::MakeCurrent);
	for( INT i=0; i<Viewports.Num(); i++ )
	{
		UViewport* OldViewport = Viewports(i);
		if( OldViewport->Current && OldViewport!=InViewport )
		{
			OldViewport->Current = 0;
			OldViewport->UpdateWindowFrame();
		}
	}
	if( InViewport )
	{
		InViewport->Current = 1;
		InViewport->UpdateWindowFrame();
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	End.
-----------------------------------------------------------------------------*/
