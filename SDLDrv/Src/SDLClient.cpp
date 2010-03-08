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
		WindowedViewportX  = FullscreenViewportX  = 640;
		WindowedViewportY  = FullscreenViewportY  = 480;
	}

	// Init SDL
	if( !SDL_WasInit( SDL_INIT_VIDEO ) ) {
		if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
			appErrorf( TEXT("Couldn't initialize SDL: %s\n"), SDL_GetError() );
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

	// End mouse capture.

	// Kill the audio subsystem.
	if( Engine && Engine->Audio )
	{
		Engine->Audio->ConditionalShutdownAfterError();
	}

	// Release all viewports.
	for( INT i=Viewports.Num()-1; i>=0; i-- )
	{
		USDLViewport* Viewport = (USDLViewport*)Viewports( i );
		Viewport->ConditionalShutdownAfterError();
	}

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

	// Tick the viewports.
  	for( INT i=0; i<Viewports.Num(); i++ )
	{
		USDLViewport* Viewport = CastChecked<USDLViewport>(Viewports(i));
		Viewport->Tick();
	}

	// Blit any viewports that need blitting.
	USDLViewport* BestViewport = NULL;
  	for( i=0; i<Viewports.Num(); i++ )
	{
		USDLViewport* Viewport = CastChecked<USDLViewport>(Viewports(i));
		check(!Viewport->HoldCount);
		/*		
		//dv: FIXME???
		if( !(Viewport->XWindow) )
		{
			// Window was closed via close button.
			delete Viewport;
			return;
		}
		*/
  		//else
		if
		(	Viewport->IsRealtime()
		&&	Viewport->SizeX
		&&	Viewport->SizeY
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
// Configuration change.
//
void USDLClient::PostEditChange()
{
	guard(USDLClient::PostEditChange);

	Super::PostEditChange();

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
		if( (Viewport->Actor->ShowFlags & ShowFlags)==ShowFlags )
			;// XMapWindow(XDisplay, Viewport->XWindow);
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
