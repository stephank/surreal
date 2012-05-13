/*=============================================================================
	XLaunchPrivate.h: Unreal launcher for SDL.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#if defined(WIN32)
	#include <windows.h>
#else
	#include <errno.h>
	#include <sys/stat.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#if !defined(__APPLE__)
	#include <malloc.h>
#endif
#include <fcntl.h>
#include "SDL.h"
#include "Engine.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
