/*=============================================================================
	OpenGLDrv.h: Unreal OpenGL support header.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Tim Sweeney
	* Multitexture and context support - Andy Hanson (hanson@3dfx.com) and
	  Jack Mathews (jack@3dfx.com)
	* Unified by Daniel Vogel

=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>
#else
#include "SDL.h"
#endif
#include <GL/gl.h>
#ifdef WIN32
#include "glext.h"
#include "wglext.h"
#else
#include <GL/glext.h>
#endif

#ifndef UTGLR_DONT_DEBUG_AT_ALL
#define UTGLR_NO_APP_MALLOC
#endif

#include "Engine.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
