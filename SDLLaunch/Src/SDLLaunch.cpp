/*=============================================================================
	Launch.cpp: Game launcher.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

// System includes
#include "SDLLaunchPrivate.h"

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

extern "C" {TCHAR GPackage[64]=TEXT("SDLLaunch");}

#include "FNativeTypes.h"

// Memory allocator.
FMallocNative Malloc;

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceAnsiError.h"
FOutputDeviceAnsiError Error;

// Feedback.
#include "FFeedbackContextAnsi.h"
FFeedbackContextAnsi Warn;

// File manager.
FFileManagerNative FileManager;

// Config.
#include "FConfigCacheIni.h"

// Splash
static const TCHAR* SplashPath = TEXT("..") PATH_SEPARATOR TEXT("Help") PATH_SEPARATOR TEXT("Logo.bmp");
static SDL_Window* SplashWindow = NULL;
static SDL_Renderer* SplashRenderer = NULL;

/*-----------------------------------------------------------------------------
	Splash
-----------------------------------------------------------------------------*/

static void CloseSplash()
{
	if ( SplashRenderer != NULL ) {
		SDL_DestroyRenderer( SplashRenderer );
		SplashRenderer = NULL;
	}
	if ( SplashWindow != NULL ) {
		SDL_DestroyWindow( SplashWindow );
		SplashWindow = NULL;
	}
}

static void OpenSplash()
{
	// Init SDL.
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
		appErrorf( TEXT("Couldn't initialize SDL: %s\n"), SDL_GetError() );

	SDL_Surface* SplashSurface = NULL;
	SDL_Texture* SplashTexture = NULL;

	// Load the splash.
	SplashSurface = SDL_LoadBMP( appToAnsi(SplashPath) );
	if( SplashSurface == NULL )
		return;

	// Create a centered frameless window.
	SplashWindow = SDL_CreateWindow(
		"Unreal Tournament",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		SplashSurface->w, SplashSurface->h,
		SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS
	);
	if( SplashWindow == NULL )
		goto splash_fail;

	SplashRenderer = SDL_CreateRenderer(
		SplashWindow, -1, SDL_RENDERER_SOFTWARE
	);
	if( SplashRenderer == NULL )
		goto splash_fail;

	SplashTexture = SDL_CreateTextureFromSurface(
		SplashRenderer, SplashSurface
	);
	SDL_FreeSurface( SplashSurface );
	SplashSurface = NULL;
	if( SplashTexture == NULL )
		goto splash_fail;

	SDL_RenderCopy( SplashRenderer, SplashTexture, NULL, NULL );
	SDL_RenderPresent( SplashRenderer );
	return;

splash_fail:
	CloseSplash();
	if( SplashSurface != NULL ) {
		SDL_FreeSurface( SplashSurface );
		SplashSurface = NULL;
	}
}

/*-----------------------------------------------------------------------------
	Initialization
-----------------------------------------------------------------------------*/

//
// Creates a UEngine object.
//
static UEngine* InitEngine()
{
	guard(InitEngine);
	FTime LoadTime = appSeconds();

	// Set exec hook.
	GExec = NULL;

	// Update first-run.
	INT FirstRun=0;
	if (FirstRun<ENGINE_VERSION)
		FirstRun = ENGINE_VERSION;
	GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );

	// Create the global engine object.
	UClass* EngineClass;
	EngineClass = UObject::StaticLoadClass(
		UGameEngine::StaticClass(), NULL, 
		TEXT("ini:Engine.Engine.GameEngine"), 
		NULL, LOAD_NoFail, NULL 
	);
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	Engine->Init();
	debugf( TEXT("Startup time: %f seconds."), appSeconds()-LoadTime );

	// Don't use the TSC, which is a broken timing mechanism on modern PCs.
	GTimestamp = 0;

	return Engine;
	unguard;
}

/*-----------------------------------------------------------------------------
	Main Loop
-----------------------------------------------------------------------------*/

//
// X game message loop.
//
static void MainLoop( UEngine* Engine )
{
	guard(MainLoop);
	check(Engine);

	// Loop while running.
	GIsRunning = 1;
	FTime OldTime = appSeconds();
	FTime SecondStartTime = OldTime;
	INT TickCount = 0;
	while( GIsRunning && !GIsRequestingExit )
	{
		// Update the world.
		guard(UpdateWorld);
		FTime NewTime   = appSeconds();
		FLOAT  DeltaTime = NewTime - OldTime;
		Engine->Tick( DeltaTime );
		if( GWindowManager )
			GWindowManager->Tick( DeltaTime );
		OldTime = NewTime;
		TickCount++;
		if( OldTime > SecondStartTime + 1 )
		{
			Engine->CurrentTickRate = (FLOAT)TickCount / (OldTime - SecondStartTime);
			SecondStartTime = OldTime;
			TickCount = 0;
		}
		unguard;

		// Enforce optional maximum tick rate.
		guard(EnforceTickRate);
		FLOAT MaxTickRate = Engine->GetMaxTickRate();
		if( MaxTickRate>0.0 )
		{
			FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
		unguard;
	}
	GIsRunning = 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	Main.
-----------------------------------------------------------------------------*/

//
// Entry point.
//
int main( int argc, char* argv[] )
{
	#if !_MSC_VER
		__Context::StaticInit();
	#endif

	INT ErrorLevel = 0;
	GIsStarted     = 1;
#ifndef _DEBUG
	try
#endif
	{
		GIsGuarded = 1;

		#if !_MSC_VER
		// Set module name.
		appStrcpy( GModule, argv[0] );
		#endif

		// Set the package name.
		appStrcpy( GPackage, appPackage() );

		// Get the command line.
		TCHAR CmdLine[1024], *CmdLinePtr=CmdLine;
		*CmdLinePtr = 0;
		for( INT i=1; i<argc; i++ )
		{
			if( i>1 )
				appStrcat( CmdLine, TEXT(" ") );
			appStrcat( CmdLine, appFromAnsi(argv[i]) );
		}

		// Init core.
		GIsClient = 1;
		GIsGuarded = 1;
		appInit( TEXT("UnrealTournament"), CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );

		// Init mode.
		GIsServer		= 1;
		GIsClient		= !ParseParam(appCmdLine(), TEXT("SERVER"));
		GIsEditor		= 0;
		GIsScriptable	= 1;
		GLazyLoad		= !GIsClient || ParseParam(appCmdLine(), TEXT("LAZY"));

		// Init console log.
		if (ParseParam(CmdLine, TEXT("LOG")))
		{
			Warn.AuxOut = GLog;
			GLog		= &Warn;
		}

		// Open splash screen.
		OpenSplash();

		// Init engine.
		UEngine* Engine = InitEngine();
		if( Engine )
		{
			debugf( NAME_Title, LocalizeGeneral("Run") );

			// Optionally Exec and exec file.
			FString Temp;
			if( Parse(CmdLine, TEXT("EXEC="), Temp) )
			{
				Temp = FString(TEXT("exec ")) + Temp;
				if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
					Engine->Client->Viewports(0)->Exec( *Temp, *GLog );
			}

			// Clean up the splash screen.
			CloseSplash();

			// Start main engine loop.
			debugf( TEXT("Entering main loop.") );
			if ( !GIsRequestingExit )
				MainLoop( Engine );
		}
		appPreExit();
		GIsGuarded = 0;
	}
#ifndef _DEBUG
	catch( ... )
	{
		// Crashed.
		ErrorLevel = 1;
		GIsGuarded = 0;
		Error.HandleError();
	}
#endif
	appExit();
	GIsStarted = 0;
	return ErrorLevel;
}
