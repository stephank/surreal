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

#ifdef WIN32
#include <windows.h>
#endif

#include <SDL.h>
#include <GL/gl.h>
#ifdef WIN32
#include "glext.h"
#include "wglext.h"
#else
#include <GL/glext.h>
#endif

#define UTGLR_NO_APP_MALLOC
#include <stdlib.h>

#include "Engine.h"
#include "UnRender.h"

#ifdef UTGLR_UNREAL_227_BUILD
// Hack so I don't have to move UOpenGLRenderDevice's interface into a header.  --ryan.
//#define AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV UOpenGLRenderDevice::StaticClass();
extern "C" { void autoInitializeRegistrantsOpenGLDrv(void); }
// #define AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV autoInitializeRegistrantsOpenGLDrv();
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
