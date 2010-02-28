/*=============================================================================
	OpenGL.cpp: Unreal OpenGL support implementation for Windows and Linux.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.

	OpenGL renderer by Daniel Vogel <vogel@lokigames.com>
	Loki Software, Inc.

	Other URenderDevice subclasses include:
	* USoftwareRenderDevice: Software renderer.
	* UGlideRenderDevice: 3dfx Glide renderer.
	* UDirect3DRenderDevice: Direct3D renderer.
	* UOpenGLRenderDevice: OpenGL renderer.

	Revision history:
	* Created by Daniel Vogel based on XMesaGLDrv
	* Changes (John Fulmer, Jeroen Janssen)
	* Major changes (Daniel Vogel)
	* Ported back to Win32 (Fredrik Gustafsson)
	* Unification and addition of vertex arrays (Daniel Vogel)
	* Actor triangle caching (Steve Sinclair)
	* One pass fogging (Daniel Vogel)
	* Windows gamma support (Daniel Vogel)
	* 2X blending support (Daniel Vogel)
	* Better detail texture handling (Daniel Vogel)
	* Scaleability (Daniel Vogel)
	* Texture LOD bias (Daniel Vogel)
	* RefreshRate support on Windows (Jason Dick)
	* Finer control over gamma (Daniel Vogel)
	* (NOT ALWAYS) Fixed Windows bitdepth switching (Daniel Vogel)

	* Various modifications and additions by Chris Dohnal
	* Initial TruForm based on TruForm renderer modifications by NitroGL
	* Additional TruForm and Deus Ex updates by Leonhard Gruenschloss


	UseTNT			workaround for buggy TNT/TNT2 drivers
	UseTrilinear	whether to use trilinear filtering
	UseAlphaPalette	set to 0 for buggy drivers (GeForce)
	UseS3TC			whether to use compressed textures
	MaxAnisotropy	maximum level of anisotropy used
	MaxTMUnits		maximum number of TMUs UT will try to use
	LODBias			texture lod bias
	RefreshRate		requested refresh rate (Windows only)
	GammaOffset		offset for the gamma correction


TODO:
	- DOCUMENTATION!!! (especially all subtle assumptions)

=============================================================================*/

#include "OpenGLDrv.h"
#include "OpenGL.h"

#ifdef WIN32
#include <mmsystem.h>
#endif


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

#ifdef UTGLR_UNREAL_BUILD
const DWORD GUglyHackFlags = 0;
#endif

static const TCHAR *g_pSection = TEXT("OpenGLDrv.OpenGLRenderDevice");


/*-----------------------------------------------------------------------------
	Vertex programs.
-----------------------------------------------------------------------------*/

static const char *g_vpDefaultRenderingState =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"MOV result.color, vertex.color;\n"
	"MOV result.texcoord[0], vertex.texcoord[0];\n"

	"END\n"
;

static const char *g_vpDefaultRenderingStateWithFog =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"MOV result.color, vertex.color;\n"
	"MOV result.color.secondary, vertex.color.secondary;\n"
	"MOV result.texcoord[0], vertex.texcoord[0];\n"

	"END\n"
;

#ifdef UTGLR_RUNE_BUILD
static const char *g_vpDefaultRenderingStateWithLinearFog =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"MOV result.color, vertex.color;\n"
	"MOV result.color.secondary, vertex.color.secondary;\n"
	"MOV result.texcoord[0], vertex.texcoord[0];\n"
	"MOV result.fogcoord.x, vertex.position.z;\n"

	"END\n"
;
#endif

static const char *g_vpComplexSurfaceSingleTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceDualTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceTripleTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceQuadTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"ATTRIB texInfo3 = vertex.attrib[11];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"
	"OUTPUT oTex3 = result.texcoord[3];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo3.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo3.zwzw;\n"
	"MOV oTex3, t2;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceDetailAlpha =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"PARAM defTexCoord = { 0, 0.5, 0, 1 };\n" //Shared with initializing V coord for alpha texture
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"MUL t2.x, iPos.z, RNearZ;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceSingleTextureAndDetailTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"PARAM defTexCoord = { 0, 0.5, 0, 1 };\n" //Shared with initializing V coord for alpha texture
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"MUL t2.x, iPos.z, RNearZ;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceDualTextureAndDetailTexture =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"ATTRIB texInfo3 = vertex.attrib[11];\n"
	"PARAM defTexCoord = { 0, 0.5, 0, 1 };\n" //Shared with initializing V coord for alpha texture
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"
	"OUTPUT oTex3 = result.texcoord[3];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"MUL t2.x, iPos.z, RNearZ;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo3.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo3.zwzw;\n"
	"MOV oTex3, t2;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceSingleTextureWithPos =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV oTex1, iPos;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceDualTextureWithPos =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV oTex2, iPos;\n"

	"END\n"
;

static const char *g_vpComplexSurfaceTripleTextureWithPos =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"ATTRIB iPos = vertex.position;\n"
	"ATTRIB fxInfo = vertex.attrib[6];\n"
	"ATTRIB fyInfo = vertex.attrib[7];\n"
	"ATTRIB texInfo0 = vertex.attrib[8];\n"
	"ATTRIB texInfo1 = vertex.attrib[9];\n"
	"ATTRIB texInfo2 = vertex.attrib[10];\n"
	"PARAM defTexCoord = { 0, 0, 0, 1 };\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"OUTPUT oTex0 = result.texcoord[0];\n"
	"OUTPUT oTex1 = result.texcoord[1];\n"
	"OUTPUT oTex2 = result.texcoord[2];\n"
	"OUTPUT oTex3 = result.texcoord[3];\n"

	"MOV result.color, vertex.color;\n"

	"DPH t1.x, iPos, fxInfo;\n"
	"DPH t1.y, iPos, fyInfo;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo0.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo0.zwzw;\n"
	"MOV oTex0, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo1.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo1.zwzw;\n"
	"MOV oTex1, t2;\n"

	"MOV t2, defTexCoord;\n"
	"SUB t2.xy, t1.xyxy, texInfo2.xyxy;\n"
	"MUL t2.xy, t2.xyxy, texInfo2.zwzw;\n"
	"MOV oTex2, t2;\n"

	"MOV oTex3, iPos;\n"

	"END\n"
;


/*-----------------------------------------------------------------------------
	Fragment programs.
-----------------------------------------------------------------------------*/

static const char *g_fpDefaultRenderingState =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"TEMP t0;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"MUL result.color, t0, iColor;\n"

	"END\n"
;

static const char *g_fpDefaultRenderingStateWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"TEMP t0;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"MUL result.color.a, t0.a, iColor.a;\n"
	"MAD result.color.rgb, t0, iColor, fragment.color.secondary;\n"

	"END\n"
;

#ifdef UTGLR_RUNE_BUILD
static const char *g_fpDefaultRenderingStateWithLinearFog =
	"!!ARBfp1.0\n"
	"OPTION ARB_fog_linear;\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"TEMP t0;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"MUL result.color, t0, iColor;\n"

	"END\n"
;
#endif

static const char *g_fpComplexSurfaceSingleTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"

	"TEX result.color, iTC0, texture[0], 2D;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceDualTextureModulated =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MUL result.color, t0, t1;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceDualTextureModulated2X =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceSingleTextureWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"SUB t1.a, 1.0, t1.a;\n"

	"MOV result.color.a, t0.a;\n"

	"MAD result.color.rgb, t0, t1.aaaa, t1;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceDualTextureModulatedWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"SUB t2.a, 1.0, t2.a;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"

	"MAD result.color.rgb, t0, t2.aaaa, t2;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceDualTextureModulated2XWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"SUB t2.a, 1.0, t2.a;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"
	"ADD t0, t0, t0;\n"

	"MAD result.color.rgb, t0, t2.aaaa, t2;\n"

	"END\n"
;

static const char *g_fpDetailTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t0;\n"
	"TEMP tDB;\n"

	"MUL_SAT tDB.x, iTC1.z, RNearZ;\n"
	"SUB t0.x, 0.999, tDB.x;\n"
	"KIL t0.xxxx;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"LRP result.color, tDB.xxxx, iColor, t0;\n"

	"END\n"
;

static const char *g_fpDetailTextureTwoLayer =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"PARAM DetailScale = { 4.223, 4.223, 0, 1 };\n"
	"TEMP t0;\n"
	"TEMP tDB;\n"
	"TEMP tAcc;\n"

	"MUL_SAT tDB.x, iTC1.z, RNearZ;\n"
	"SUB t0.x, 0.999, tDB.x;\n"
	"KIL t0.xxxx;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"

	"LRP tAcc, tDB.xxxx, iColor, t0;\n"

	"MUL t0, iTC0, DetailScale;\n"
	"TEX t0, t0, texture[0], 2D;\n"

	"MUL_SAT tDB.x, tDB.x, DetailScale.x;\n"
	"LRP t0, tDB.xxxx, iColor, t0;\n"

	"MUL tAcc, tAcc, t0;\n"
	"ADD result.color, tAcc, tAcc;\n"

	"END\n"
;

static const char *g_fpSingleTextureAndDetailTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MOV result.color.a, t0.a;\n"

	"MUL_SAT tDB.x, iTC2.z, RNearZ;\n"
	"LRP t1, tDB.xxxx, iColor, t1;\n"

	"MUL t0, t0, t1;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n"
;

static const char *g_fpSingleTextureAndDetailTextureTwoLayer =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"PARAM DetailScale = { 4.223, 4.223, 0, 1 };\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MOV result.color.a, t0.a;\n"

	"MUL_SAT tDB.x, iTC2.z, RNearZ;\n"
	"LRP t1, tDB.xxxx, iColor, t1;\n"

	"MUL t0, t0, t1;\n"
	"ADD t0, t0, t0;\n"

	"MUL t1, iTC1, DetailScale;\n"
	"TEX t1, t1, texture[1], 2D;\n"

	"MUL_SAT tDB.x, tDB.x, DetailScale.x;\n"
	"LRP t1, tDB.xxxx, iColor, t1;\n"

	"MUL t0, t0, t1;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n"
;

static const char *g_fpDualTextureAndDetailTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"ATTRIB iTC3 = fragment.texcoord[3];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"
	"MAD t0, t0, iColor.aaaa, t0;\n"

	"MUL_SAT tDB.x, iTC3.z, RNearZ;\n"
	"LRP t2, tDB.xxxx, iColor, t2;\n"

	"MUL t0, t0, t2;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n"
;

static const char *g_fpDualTextureAndDetailTextureTwoLayer =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"ATTRIB iTC3 = fragment.texcoord[3];\n"
	"PARAM RNearZ = 0.002631578947;\n"
	"PARAM DetailScale = { 4.223, 4.223, 0, 1 };\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"TEMP tDB;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MOV result.color.a, t0.a;\n"
	"MAD t0, t0, iColor.aaaa, t0;\n"

	"MUL_SAT tDB.x, iTC3.z, RNearZ;\n"
	"LRP t2, tDB.xxxx, iColor, t2;\n"

	"MUL t0, t0, t2;\n"
	"ADD t0, t0, t0;\n"

	"MUL t2, iTC2, DetailScale;\n"
	"TEX t2, t2, texture[2], 2D;\n"

	"MUL_SAT tDB.x, tDB.x, DetailScale.x;\n"
	"LRP t2, tDB.xxxx, iColor, t2;\n"

	"MUL t0, t0, t2;\n"
	"ADD result.color.rgb, t0, t0;\n"

	"END\n"
;


/*-----------------------------------------------------------------------------
	OpenGLDrv.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UOpenGLRenderDevice);


void UOpenGLRenderDevice::StaticConstructor() {
	guard(UOpenGLRenderDevice::StaticConstructor);

#ifdef UTGLR_DX_BUILD
	const UBOOL UTGLR_DEFAULT_OneXBlending = 1;
#else
	const UBOOL UTGLR_DEFAULT_OneXBlending = 0;
#endif

#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
	const UBOOL UTGLR_DEFAULT_UseS3TC = 0;
#else
	const UBOOL UTGLR_DEFAULT_UseS3TC = 1;
#endif

#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
	const UBOOL UTGLR_DEFAULT_ZRangeHack = 0;
#else
	const UBOOL UTGLR_DEFAULT_ZRangeHack = 1;
#endif

#define CPP_PROPERTY_LOCAL(_name) _name, CPP_PROPERTY(_name)
#define CPP_PROPERTY_LOCAL_DCV(_name) DCV._name, CPP_PROPERTY(DCV._name)

	//Set parameter defaults and add parameters
#ifdef UTGLR_UNREAL_BUILD
	SC_AddBoolConfigParam(0,  TEXT("DetailTextures"), CPP_PROPERTY_LOCAL(DetailTextures), 0);
#endif
	SC_AddFloatConfigParam(TEXT("LODBias"), CPP_PROPERTY_LOCAL(LODBias), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffset"), CPP_PROPERTY_LOCAL(GammaOffset), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetRed"), CPP_PROPERTY_LOCAL(GammaOffsetRed), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetGreen"), CPP_PROPERTY_LOCAL(GammaOffsetGreen), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetBlue"), CPP_PROPERTY_LOCAL(GammaOffsetBlue), 0.0f);
	SC_AddBoolConfigParam(1,  TEXT("GammaCorrectScreenshots"), CPP_PROPERTY_LOCAL(GammaCorrectScreenshots), 0);
	SC_AddBoolConfigParam(0,  TEXT("OneXBlending"), CPP_PROPERTY_LOCAL(OneXBlending), UTGLR_DEFAULT_OneXBlending);
	SC_AddIntConfigParam(TEXT("MinLogTextureSize"), CPP_PROPERTY_LOCAL(MinLogTextureSize), 0);
	SC_AddIntConfigParam(TEXT("MaxLogTextureSize"), CPP_PROPERTY_LOCAL(MaxLogTextureSize), 8);
	SC_AddBoolConfigParam(10, TEXT("UseZTrick"), CPP_PROPERTY_LOCAL(UseZTrick), 0);
	SC_AddBoolConfigParam(9,  TEXT("UseBGRATextures"), CPP_PROPERTY_LOCAL(UseBGRATextures), 1);
	SC_AddBoolConfigParam(8,  TEXT("UseMultiTexture"), CPP_PROPERTY_LOCAL(UseMultiTexture), 1);
	SC_AddBoolConfigParam(7,  TEXT("UsePalette"), CPP_PROPERTY_LOCAL(UsePalette), 0);
	SC_AddBoolConfigParam(6,  TEXT("ShareLists"), CPP_PROPERTY_LOCAL(ShareLists), 0);
	SC_AddBoolConfigParam(5,  TEXT("UsePrecache"), CPP_PROPERTY_LOCAL(UsePrecache), 0);
	SC_AddBoolConfigParam(4,  TEXT("UseTrilinear"), CPP_PROPERTY_LOCAL(UseTrilinear), 0);
	SC_AddBoolConfigParam(3,  TEXT("UseAlphaPalette"), CPP_PROPERTY_LOCAL(UseAlphaPalette), 0);
	SC_AddBoolConfigParam(2,  TEXT("UseS3TC"), CPP_PROPERTY_LOCAL(UseS3TC), UTGLR_DEFAULT_UseS3TC);
	SC_AddBoolConfigParam(1,  TEXT("Use16BitTextures"), CPP_PROPERTY_LOCAL(Use16BitTextures), 0);
	SC_AddBoolConfigParam(0,  TEXT("UseTNT"), CPP_PROPERTY_LOCAL(UseTNT), 0);
	SC_AddIntConfigParam(TEXT("MaxAnisotropy"), CPP_PROPERTY_LOCAL(MaxAnisotropy), 0);
	SC_AddBoolConfigParam(0,  TEXT("NoFiltering"), CPP_PROPERTY_LOCAL(NoFiltering), 0);
	SC_AddIntConfigParam(TEXT("MaxTMUnits"), CPP_PROPERTY_LOCAL(MaxTMUnits), 0);
	SC_AddIntConfigParam(TEXT("RefreshRate"), CPP_PROPERTY_LOCAL(RefreshRate), 0);
	SC_AddIntConfigParam(TEXT("DetailMax"), CPP_PROPERTY_LOCAL(DetailMax), 0);
	SC_AddBoolConfigParam(9,  TEXT("DetailClipping"), CPP_PROPERTY_LOCAL(DetailClipping), 0);
	SC_AddBoolConfigParam(8,  TEXT("ColorizeDetailTextures"), CPP_PROPERTY_LOCAL(ColorizeDetailTextures), 0);
	SC_AddBoolConfigParam(7,  TEXT("SinglePassFog"), CPP_PROPERTY_LOCAL_DCV(SinglePassFog), 1);
	SC_AddBoolConfigParam(6,  TEXT("SinglePassDetail"), CPP_PROPERTY_LOCAL_DCV(SinglePassDetail), 0);
	SC_AddBoolConfigParam(5,  TEXT("BufferTileQuads"), CPP_PROPERTY_LOCAL(BufferTileQuads), 0);
	SC_AddBoolConfigParam(4,  TEXT("UseSSE"), CPP_PROPERTY_LOCAL(UseSSE), 1);
	SC_AddBoolConfigParam(3,  TEXT("UseSSE2"), CPP_PROPERTY_LOCAL(UseSSE2), 1);
	SC_AddBoolConfigParam(2,  TEXT("UseTexIdPool"), CPP_PROPERTY_LOCAL(UseTexIdPool), 1);
	SC_AddBoolConfigParam(1,  TEXT("UseTexPool"), CPP_PROPERTY_LOCAL(UseTexPool), 1);
	SC_AddBoolConfigParam(0,  TEXT("CacheStaticMaps"), CPP_PROPERTY_LOCAL(CacheStaticMaps), 0);
	SC_AddIntConfigParam(TEXT("DynamicTexIdRecycleLevel"), CPP_PROPERTY_LOCAL(DynamicTexIdRecycleLevel), 100);
	SC_AddBoolConfigParam(4,  TEXT("TexDXT1ToDXT3"), CPP_PROPERTY_LOCAL(TexDXT1ToDXT3), 0);
	SC_AddBoolConfigParam(3,  TEXT("UseMultiDrawArrays"), CPP_PROPERTY_LOCAL(UseMultiDrawArrays), 0);
	SC_AddBoolConfigParam(2,  TEXT("UseCVA"), CPP_PROPERTY_LOCAL(UseCVA), 0);
	SC_AddBoolConfigParam(1,  TEXT("UseVertexProgram"), CPP_PROPERTY_LOCAL_DCV(UseVertexProgram), 0);
	SC_AddBoolConfigParam(0,  TEXT("UseFragmentProgram"), CPP_PROPERTY_LOCAL_DCV(UseFragmentProgram), 0);
	SC_AddIntConfigParam(TEXT("SwapInterval"), CPP_PROPERTY_LOCAL(SwapInterval), -1);
	SC_AddIntConfigParam(TEXT("FrameRateLimit"), CPP_PROPERTY_LOCAL(FrameRateLimit), 0);
	SC_AddBoolConfigParam(3,  TEXT("SceneNodeHack"), CPP_PROPERTY_LOCAL(SceneNodeHack), 1);
	SC_AddBoolConfigParam(2,  TEXT("SmoothMaskedTextures"), CPP_PROPERTY_LOCAL(SmoothMaskedTextures), 0);
	SC_AddBoolConfigParam(1,  TEXT("MaskedTextureHack"), CPP_PROPERTY_LOCAL(MaskedTextureHack), 1);
	SC_AddBoolConfigParam(0,  TEXT("UseAA"), CPP_PROPERTY_LOCAL(UseAA), 0);
	SC_AddIntConfigParam(TEXT("NumAASamples"), CPP_PROPERTY_LOCAL(NumAASamples), 4);
	SC_AddBoolConfigParam(1,  TEXT("NoAATiles"), CPP_PROPERTY_LOCAL(NoAATiles), 1);
	SC_AddBoolConfigParam(0,  TEXT("ZRangeHack"), CPP_PROPERTY_LOCAL(ZRangeHack), UTGLR_DEFAULT_ZRangeHack);

#undef CPP_PROPERTY_LOCAL
#undef CPP_PROPERTY_LOCAL_DCV

	//Driver flags
	SpanBased				= 0;
	SupportsFogMaps			= 1;
#ifdef UTGLR_RUNE_BUILD
	SupportsDistanceFog		= 1;
#else
	SupportsDistanceFog		= 0;
#endif
	FullscreenOnly			= 0;

	SupportsLazyTextures	= 0;
	PrefersDeferredLoad		= 0;

	//Invalidate fixed texture ids
	m_noTextureId = 0;
	m_alphaTextureId = 0;

	//Mark vertex program names as not allocated
	m_allocatedVertexProgramNames = false;

	//Mark fragment program names as not allocated
	m_allocatedFragmentProgramNames = false;

	//Frame rate limit timer not yet initialized
	m_frameRateLimitTimerInitialized = false;

	unguard;
}


void UOpenGLRenderDevice::SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue) {
	param = (((defaultValue) != 0) ? 1 : 0) << BitMaskOffset; //Doesn't exactly work like a UBOOL "// Boolean 0 (false) or 1 (true)."
	new(GetClass(), pName, RF_Public)UBoolProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

void UOpenGLRenderDevice::SC_AddIntConfigParam(const TCHAR *pName, INT &param, ECppProperty EC_CppProperty, INT InOffset, INT defaultValue) {
	param = defaultValue;
	new(GetClass(), pName, RF_Public)UIntProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}

void UOpenGLRenderDevice::SC_AddFloatConfigParam(const TCHAR *pName, FLOAT &param, ECppProperty EC_CppProperty, INT InOffset, FLOAT defaultValue) {
	param = defaultValue;
	new(GetClass(), pName, RF_Public)UFloatProperty(EC_CppProperty, InOffset, TEXT("Options"), CPF_Config);
}


void UOpenGLRenderDevice::DbgPrintInitParam(const TCHAR *pName, INT value) {
	dout << TEXT("utglr: ") << pName << TEXT(" = ") << value << std::endl;
	return;
}

void UOpenGLRenderDevice::DbgPrintInitParam(const TCHAR *pName, FLOAT value) {
	dout << TEXT("utglr: ") << pName << TEXT(" = ") << value << std::endl;
	return;
}


#ifdef UTGLR_INCLUDE_SSE_CODE
bool UOpenGLRenderDevice::CPU_DetectCPUID(void) {
	//Check for cpuid instruction support
	__try {
		__asm {
			//CPUID function 0
			xor eax, eax
			cpuid
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}

	return true;
}

bool UOpenGLRenderDevice::CPU_DetectSSE(void) {
	bool bSupportsSSE;

	//Check for cpuid instruction support
	if (CPU_DetectCPUID() != true) {
		return false;
	}

	//Check for SSE support
	bSupportsSSE = false;
	__asm {
		//CPUID function 1
		mov eax, 1
		cpuid

		//Check the SSE bit
		test edx, 0x02000000
		jz l_no_sse

		//Set bSupportsSSE to true
		mov bSupportsSSE, 1

l_no_sse:
	}

	//Return if no CPU SSE support
	if (bSupportsSSE == false) {
		return bSupportsSSE;
	}

	//Check for SSE OS support
	__try {
		__asm {
			//Execute SSE instruction
			xorps xmm0, xmm0
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		//Clear SSE support flag
		bSupportsSSE = false;
	}

	return bSupportsSSE;
}

bool UOpenGLRenderDevice::CPU_DetectSSE2(void) {
	bool bSupportsSSE2;

	//Check for cpuid instruction support
	if (CPU_DetectCPUID() != true) {
		return false;
	}

	//Check for SSE2 support
	bSupportsSSE2 = false;
	__asm {
		//CPUID function 1
		mov eax, 1
		cpuid

		//Check the SSE2 bit
		test edx, 0x04000000
		jz l_no_sse2

		//Set bSupportsSSE2 to true
		mov bSupportsSSE2, 1

l_no_sse2:
	}

	//Return if no CPU SSE2 support
	if (bSupportsSSE2 == false) {
		return bSupportsSSE2;
	}

	//Check for SSE2 OS support
	__try {
		__asm {
			//Execute SSE2 instruction
			xorpd xmm0, xmm0
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		//Clear SSE2 support flag
		bSupportsSSE2 = false;
	}

	return bSupportsSSE2;
}
#endif //UTGLR_INCLUDE_SSE_CODE


void UOpenGLRenderDevice::InitFrameRateLimitTimerSafe(void) {
	//Only initialize once
	if (m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = true;

#ifdef WIN32
	//Request high resolution timer
	timeBeginPeriod(1);
#endif

	return;
}

void UOpenGLRenderDevice::ShutdownFrameRateLimitTimer(void) {
	//Only shutdown once
	if (!m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = false;

#ifdef WIN32
	//Release high resolution timer
	timeEndPeriod(1);
#endif

	return;
}


static void FASTCALL Buffer3Verts(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLNormal *pNormalArray = &pRD->NormalArray[pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	FGLSingleColor *pSingleColorArray = &pRD->SingleColorArray[pRD->BufferedVerts];
	FGLDoubleColor *pDoubleColorArray = &pRD->DoubleColorArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		if (pRD->m_requestedColorFlags & UOpenGLRenderDevice::CF_NORMAL_ARRAY) {
			pNormalArray->x = P->Normal.X;
			pNormalArray->y = P->Normal.Y;
			pNormalArray->z = P->Normal.Z;
			pNormalArray++;
		}

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;

		if (pRD->m_requestedColorFlags & UOpenGLRenderDevice::CF_DUAL_COLOR_ARRAY) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			pDoubleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGBScaled_A255(&P->Light, f255_Times_One_Minus_FogW);
			pDoubleColorArray->specular = UOpenGLRenderDevice::FPlaneTo_RGB_A0(&P->Fog);
			pDoubleColorArray++;
		}
		else if (pRD->m_requestedColorFlags & UOpenGLRenderDevice::CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			pSingleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGB_Aub(&P->Light, pRD->m_gpAlpha);
#else
			pSingleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGB_A255(&P->Light);
#endif
			pSingleColorArray++;
		}
	}
}

static void FASTCALL Buffer3BasicVerts(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	FLOAT UMult = pRD->TexInfo[0].UMult;
	FLOAT VMult = pRD->TexInfo[0].VMult;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * UMult;
		pTexCoordArray->v = P->V * VMult;
		pTexCoordArray++;

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;
	}
}

static void FASTCALL Buffer3ColoredVerts(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	FGLSingleColor *pSingleColorArray = &pRD->SingleColorArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;

		pSingleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGB_A255(&P->Light);
		pSingleColorArray++;
	}
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) static void FASTCALL Buffer3ColoredVerts_SSE(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	static float f255 = 255.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*4]
		add esi, [ecx]UOpenGLRenderDevice.SingleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		//BufferedVerts += 3
		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movss xmm2, f255

		//Pts in edx
		//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm3, [eax]FTransTexture.U
			mulss xmm3, xmm0
			movss [ebx]FGLTexCoord.u, xmm3
			movss xmm3, [eax]FTransTexture.V
			mulss xmm3, xmm1
			movss [ebx]FGLTexCoord.v, xmm3
			add ebx, TYPE FGLTexCoord

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			and eax, 255
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			shl ecx, 16
			or ecx, 0xFF000000
			or eax, ecx
			mov [esi]FGLSingleColor.color, eax
			add esi, TYPE FGLSingleColor

			cmp edx, ebp
			jne v_loop

		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}

__declspec(naked) static void FASTCALL Buffer3ColoredVerts_SSE2(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*4]
		add esi, [ecx]UOpenGLRenderDevice.SingleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		//BufferedVerts += 3
		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movaps xmm2, fColorMul
		movd xmm3, alphaOr

		//Pts in edx
		//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm4, [eax]FTransTexture.U
			mulss xmm4, xmm0
			movss [ebx]FGLTexCoord.u, xmm4
			movss xmm4, [eax]FTransTexture.V
			mulss xmm4, xmm1
			movss [ebx]FGLTexCoord.v, xmm4
			add ebx, TYPE FGLTexCoord

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movups xmm4, [eax]FTransSample.Light
			mulps xmm4, xmm2
			cvtps2dq xmm4, xmm4
			packssdw xmm4, xmm4
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movd [esi]FGLSingleColor.color, xmm4
			add esi, TYPE FGLSingleColor

			cmp edx, ebp
			jne v_loop

		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}
#endif

static void FASTCALL Buffer3FoggedVerts(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	FGLTexCoord *pTexCoordArray = &pRD->TexCoordArray[0][pRD->BufferedVerts];
	FGLVertex *pVertexArray = &pRD->VertexArray[pRD->BufferedVerts];
	FGLDoubleColor *pDoubleColorArray = &pRD->DoubleColorArray[pRD->BufferedVerts];
	pRD->BufferedVerts += 3;
	for (INT i = 0; i < 3; i++) {
		const FTransTexture* P = *Pts++;

		pTexCoordArray->u = P->U * pRD->TexInfo[0].UMult;
		pTexCoordArray->v = P->V * pRD->TexInfo[0].VMult;
		pTexCoordArray++;

		pVertexArray->x = P->Point.X;
		pVertexArray->y = P->Point.Y;
		pVertexArray->z = P->Point.Z;
		pVertexArray++;

		FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
		pDoubleColorArray->color = UOpenGLRenderDevice::FPlaneTo_RGBScaled_A255(&P->Light, f255_Times_One_Minus_FogW);
		pDoubleColorArray->specular = UOpenGLRenderDevice::FPlaneTo_RGB_A0(&P->Fog);
		pDoubleColorArray++;
	}
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) static void FASTCALL Buffer3FoggedVerts_SSE(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	static float f255 = 255.0f;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp
		sub esp, 4

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*8]
		add esi, [ecx]UOpenGLRenderDevice.DoubleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		//BufferedVerts += 3
		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movss xmm2, f255

		//Pts in edx
		//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm3, [eax]FTransTexture.U
			mulss xmm3, xmm0
			movss [ebx]FGLTexCoord.u, xmm3
			movss xmm3, [eax]FTransTexture.V
			mulss xmm3, xmm1
			movss [ebx]FGLTexCoord.v, xmm3
			add ebx, TYPE FGLTexCoord

			mov [esp], ebx

			movss xmm6, f1
			subss xmm6, [eax]FTransSample.Fog + 12
			mulss xmm6, xmm2

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movss xmm3, [eax]FTransSample.Light + 0
			mulss xmm3, xmm6
			movss xmm4, [eax]FTransSample.Light + 4
			mulss xmm4, xmm6
			movss xmm5, [eax]FTransSample.Light + 8
			mulss xmm5, xmm6
			cvtss2si ebx, xmm3
			and ebx, 255
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or ebx, ecx
			cvtss2si ecx, xmm5
			shl ecx, 16
			or ecx, 0xFF000000
			or ebx, ecx
			mov [esi]FGLDoubleColor.color, ebx

			mov ebx, [esp]

			movss xmm3, [eax]FTransSample.Fog + 0
			mulss xmm3, xmm2
			movss xmm4, [eax]FTransSample.Fog + 4
			mulss xmm4, xmm2
			movss xmm5, [eax]FTransSample.Fog + 8
			mulss xmm5, xmm2
			cvtss2si eax, xmm3
			and eax, 255
			cvtss2si ecx, xmm4
			and ecx, 255
			shl ecx, 8
			or eax, ecx
			cvtss2si ecx, xmm5
			and ecx, 255
			shl ecx, 16
			or eax, ecx
			mov [esi]FGLDoubleColor.specular, eax
			add esi, TYPE FGLDoubleColor

			cmp edx, ebp
			jne v_loop

		add esp, 4
		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}

__declspec(naked) static void FASTCALL Buffer3FoggedVerts_SSE2(UOpenGLRenderDevice *pRD, FTransTexture** Pts) {
	static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
	static DWORD alphaOr = 0xFF000000;
	static float f1 = 1.0f;
	__asm {
		//pRD is in ecx
		//Pts is in edx

		push ebx
		push esi
		push edi
		push ebp

		mov eax, [ecx]UOpenGLRenderDevice.BufferedVerts

		lea ebx, [eax*8]
		add ebx, [ecx]UOpenGLRenderDevice.TexCoordArray[0]

		lea esi, [eax*8]
		add esi, [ecx]UOpenGLRenderDevice.DoubleColorArray

		lea edi, [eax + eax*2]
		lea edi, [edi*4]
		add edi, [ecx]UOpenGLRenderDevice.VertexArray

		//BufferedVerts += 3
		add eax, 3
		mov [ecx]UOpenGLRenderDevice.BufferedVerts, eax

		lea eax, [ecx]UOpenGLRenderDevice.TexInfo
		movss xmm0, [eax]FTexInfo.UMult
		movss xmm1, [eax]FTexInfo.VMult
		movaps xmm2, fColorMul
		movd xmm3, alphaOr

		//Pts in edx
		//Get PtsPlus12B
		lea ebp, [edx + 12]

v_loop:
			mov eax, [edx]
			add edx, 4

			movss xmm4, [eax]FTransTexture.U
			mulss xmm4, xmm0
			movss [ebx]FGLTexCoord.u, xmm4
			movss xmm4, [eax]FTransTexture.V
			mulss xmm4, xmm1
			movss [ebx]FGLTexCoord.v, xmm4
			add ebx, TYPE FGLTexCoord

			movss xmm6, f1
			subss xmm6, [eax]FTransSample.Fog + 12
			mulss xmm6, xmm2
			shufps xmm6, xmm6, 0x00

			mov ecx, [eax]FOutVector.Point.X
			mov [edi]FGLVertex.x, ecx
			mov ecx, [eax]FOutVector.Point.Y
			mov [edi]FGLVertex.y, ecx
			mov ecx, [eax]FOutVector.Point.Z
			mov [edi]FGLVertex.z, ecx
			add edi, TYPE FGLVertex

			movups xmm4, [eax]FTransSample.Light
			mulps xmm4, xmm6
			cvtps2dq xmm4, xmm4

			movups xmm5, [eax]FTransSample.Fog
			mulps xmm5, xmm2
			cvtps2dq xmm5, xmm5
			packssdw xmm4, xmm5
			packuswb xmm4, xmm4
			por xmm4, xmm3
			movq QWORD PTR ([esi]FGLDoubleColor.color), xmm4
			add esi, TYPE FGLDoubleColor

			cmp edx, ebp
			jne v_loop

		pop ebp
		pop edi
		pop esi
		pop ebx

		ret
	}
}
#endif


//Must be called with (NumPts > 3)
void UOpenGLRenderDevice::BufferAdditionalClippedVerts(FTransTexture** Pts, INT NumPts) {
	INT FirstVert = BufferedVerts - 3;
	INT i;

	i = 3;
	do {
		TexCoordArray[0][BufferedVerts] = TexCoordArray[0][FirstVert];
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			DoubleColorArray[BufferedVerts] = DoubleColorArray[FirstVert];
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			SingleColorArray[BufferedVerts] = SingleColorArray[FirstVert];
		}
		VertexArray[BufferedVerts] = VertexArray[FirstVert];
		BufferedVerts++;

		TexCoordArray[0][BufferedVerts] = TexCoordArray[0][BufferedVerts - 2];
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			DoubleColorArray[BufferedVerts] = DoubleColorArray[BufferedVerts - 2];
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			SingleColorArray[BufferedVerts] = SingleColorArray[BufferedVerts - 2];
		}
		VertexArray[BufferedVerts] = VertexArray[BufferedVerts - 2];
		BufferedVerts++;

		const FTransTexture* P = Pts[i];
		FGLTexCoord &destTexCoordArray = TexCoordArray[0][BufferedVerts];
		destTexCoordArray.u = P->U * TexInfo[0].UMult;
		destTexCoordArray.v = P->V * TexInfo[0].VMult;
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			FLOAT f255_Times_One_Minus_FogW = 255.0f * (1.0f - P->Fog.W);
			DoubleColorArray[BufferedVerts].color = FPlaneTo_RGBScaled_A255(&P->Light, f255_Times_One_Minus_FogW);
			DoubleColorArray[BufferedVerts].specular = FPlaneTo_RGB_A0(&P->Fog);
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			SingleColorArray[BufferedVerts].color = FPlaneTo_RGB_Aub(&P->Light, m_gpAlpha);
#else
			SingleColorArray[BufferedVerts].color = FPlaneTo_RGB_A255(&P->Light);
#endif
		}
		FGLVertex &destVertexArray = VertexArray[BufferedVerts];
		destVertexArray.x = P->Point.X;
		destVertexArray.y = P->Point.Y;
		destVertexArray.z = P->Point.Z;
		BufferedVerts++;
	} while (++i < NumPts);

	return;
}


void UOpenGLRenderDevice::BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FGammaRamp &ramp) {
	unsigned int u;

	//Parameter clamping
	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	float oneOverRedGamma = 1.0f / (2.5f * redGamma);
	float oneOverGreenGamma = 1.0f / (2.5f * greenGamma);
	float oneOverBlueGamma = 1.0f / (2.5f * blueGamma);
	for (u = 0; u < 256; u++) {
		int iVal;
		int iValRed, iValGreen, iValBlue;

		//Initial value
		iVal = u;

		//Brightness
		iVal += brightness;
		//Clamping
		if (iVal < 0) iVal = 0;
		if (iVal > 255) iVal = 255;

		//Gamma
		iValRed = (int)appRound((float)appPow(iVal / 255.0f, oneOverRedGamma) * 65535.0f);
		iValGreen = (int)appRound((float)appPow(iVal / 255.0f, oneOverGreenGamma) * 65535.0f);
		iValBlue = (int)appRound((float)appPow(iVal / 255.0f, oneOverBlueGamma) * 65535.0f);

		//Save results
		ramp.red[u] = (_WORD)iValRed;
		ramp.green[u] = (_WORD)iValGreen;
		ramp.blue[u] = (_WORD)iValBlue;
	}

	return;
}

void UOpenGLRenderDevice::BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FByteGammaRamp &ramp) {
	unsigned int u;

	//Parameter clamping
	if (brightness < -50) brightness = -50;
	if (brightness > 50) brightness = 50;

	float oneOverRedGamma = 1.0f / (2.5f * redGamma);
	float oneOverGreenGamma = 1.0f / (2.5f * greenGamma);
	float oneOverBlueGamma = 1.0f / (2.5f * blueGamma);
	for (u = 0; u < 256; u++) {
		int iVal;
		int iValRed, iValGreen, iValBlue;

		//Initial value
		iVal = u;

		//Brightness
		iVal += brightness;
		//Clamping
		if (iVal < 0) iVal = 0;
		if (iVal > 255) iVal = 255;

		//Gamma
		iValRed = (int)appRound((float)appPow(iVal / 255.0f, oneOverRedGamma) * 255.0f);
		iValGreen = (int)appRound((float)appPow(iVal / 255.0f, oneOverGreenGamma) * 255.0f);
		iValBlue = (int)appRound((float)appPow(iVal / 255.0f, oneOverBlueGamma) * 255.0f);

		//Save results
		ramp.red[u] = (BYTE)iValRed;
		ramp.green[u] = (BYTE)iValGreen;
		ramp.blue[u] = (BYTE)iValBlue;
	}

	return;
}

void UOpenGLRenderDevice::SetGamma(FLOAT GammaCorrection) {
	FGammaRamp gammaRamp;

	GammaCorrection += GammaOffset;

	//Do not attempt to set gamma if <= zero
	if (GammaCorrection <= 0.0f) {
		return;
	}

	BuildGammaRamp(GammaCorrection + GammaOffsetRed, GammaCorrection + GammaOffsetGreen, GammaCorrection + GammaOffsetBlue, Brightness, gammaRamp);

#ifdef __LINUX__
	// vogel: FIXME (talk to Sam)
	// SDL_SetGammaRamp( Ramp.red, Ramp.green, Ramp.blue );
	FLOAT gamma = 0.4 + 2 * GammaCorrection; 
	SDL_SetGamma(gamma, gamma, gamma);
#else
	if (g_gammaFirstTime) {
		if (GetDeviceGammaRamp(m_hDC, &g_originalGammaRamp)) {
			g_haveOriginalGammaRamp = true;
		}
		g_gammaFirstTime = false;
	}

	m_setGammaRampSucceeded = false;
	if (SetDeviceGammaRamp(m_hDC, &gammaRamp)) {
		m_setGammaRampSucceeded = true;
		SavedGammaCorrection = GammaCorrection;
	}
#endif

	return;
}

void UOpenGLRenderDevice::ResetGamma(void) {
#ifdef __LINUX__

#else
	//Restore gamma ramp if original was successfully saved
	if (g_haveOriginalGammaRamp) {
		HWND hDesktopWnd;
		HDC hDC;

		hDesktopWnd = GetDesktopWindow();
		hDC = GetDC(hDesktopWnd);

		// vogel: grrr, UClient::destroy is called before this gets called so hDC is invalid
		SetDeviceGammaRamp(hDC, &g_originalGammaRamp);

		ReleaseDC(hDesktopWnd, hDC);
	}
#endif

	return;
}


bool UOpenGLRenderDevice::IsGLExtensionSupported(const char *pExtensionsString, const char *pExtensionName) {
	const char *pStart;
	const char *pWhere, *pTerminator;

	pStart = pExtensionsString;
	while (1) {
		pWhere = strstr(pStart, pExtensionName);
		if (pWhere == NULL) {
			break;
		}
		pTerminator = pWhere + strlen(pExtensionName);
		if ((pWhere == pStart) || (*(pWhere - 1) == ' ')) {
			if ((*pTerminator == ' ') || (*pTerminator == '\0')) {
				return true;
			}
		}
		pStart = pTerminator;
	}

	return false;
}

bool UOpenGLRenderDevice::FindExt(const char *pName) {
	guard(UOpenGLRenderDevice::FindExt);

	if (strcmp(pName, "GL") == 0) {
		return true;
	}

	bool bRet = IsGLExtensionSupported((const char *)glGetString(GL_EXTENSIONS), pName);
	if (bRet) {
		debugf(NAME_Init, TEXT("Device supports: %s"), appFromAnsi(pName));
	}
	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utglr: GL_EXT: ") << appFromAnsi(pName) << TEXT(" = ") << bRet << std::endl;
	}

	return bRet;
	unguard;
}

void UOpenGLRenderDevice::FindProc(void*& ProcAddress, const char *pName, const char *pSupportName, bool& Supports, bool AllowExt) {
	guard(UOpenGLRenderDevice::FindProc);
#ifdef __LINUX__
	if (!ProcAddress) {
		ProcAddress = (void*)SDL_GL_GetProcAddress(pName);
	}
#else
#if DYNAMIC_BIND
	if (!ProcAddress) {
		ProcAddress = GetProcAddress(hModuleGlMain, pName);
	}
	if (!ProcAddress) {
		ProcAddress = GetProcAddress(hModuleGlGdi, pName);
	}
#endif
	if (!ProcAddress && Supports && AllowExt) {
		ProcAddress = wglGetProcAddress(pName);
	}
#endif
	if (!ProcAddress) {
		if (Supports) {
			debugf(TEXT("   Missing function '%s' for '%s' support"), appFromAnsi(pName), appFromAnsi(pSupportName));
		}
		Supports = false;
	}
	unguard;
}

void UOpenGLRenderDevice::FindProcs(bool AllowExt) {
	guard(UOpenGLRenderDevice::FindProcs);
	#define GL_EXT(name) if (AllowExt) SUPPORTS##name = FindExt(#name+1);
	#define GL_PROC(ext,ret,func,parms) FindProc(*(void**)&func, #func, #ext, SUPPORTS##ext, AllowExt);
#ifdef UTGLR_ALL_GL_PROCS
	#define GL_PROX(ext,ret,func,parms) FindProc(*(void**)&func, #func, #ext, SUPPORTS##ext, AllowExt);
#else
	#define GL_PROX(ext,ret,func,parms)
#endif
	#include "OpenGLFuncs.h"
	#undef GL_EXT
	#undef GL_PROC
	#undef GL_PROX
	unguard;
}


UBOOL UOpenGLRenderDevice::FailedInitf(const TCHAR* Fmt, ...) {
	TCHAR TempStr[4096];
	GET_VARARGS(TempStr, ARRAY_COUNT(TempStr), Fmt);
	debugf(NAME_Init, TempStr);
	Exit();
	return 0;
}

void UOpenGLRenderDevice::Exit() {
	guard(UOpenGLRenderDevice::Exit);
	check(NumDevices > 0);

#ifdef __LINUX__
	UnsetRes();

	// Shut down global GL.
	if (--NumDevices == 0) {
		{
			unsigned int u;

			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedZeroPrefixBindTrees[u].clear(&m_DWORD_CTTree_Allocator);
			}
			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedNonZeroPrefixBindTrees[u].clear(&m_QWORD_CTTree_Allocator);
			}
		}
		m_sharedNonZeroPrefixBindChain.mark_as_clear();
		//Texture ids and memory for non zero prefix tex id pool are not freed on exit
		m_sharedRGBA8TexPool.clear(&m_TexPoolMap_Allocator);
	}
#else
	// Shut down RC.
	if (m_hRC) {
		UnsetRes();
	}

	//Reset gamma ramp
	ResetGamma();

	//Timer shutdown
	ShutdownFrameRateLimitTimer();

	// Shut down this GL context. May fail if window was already destroyed.
	if (m_hDC) {
		ReleaseDC(m_hWnd, m_hDC);
	}

	// Shut down global GL.
	if (--NumDevices == 0) {
#if DYNAMIC_BIND && 0 /* Broken on some drivers */
		// Free modules.
		if (hModuleGlMain)
			verify(FreeLibrary(hModuleGlMain));
		if (hModuleGlGdi)
			verify(FreeLibrary(hModuleGlGdi));
#endif
		{
			unsigned int u;

			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedZeroPrefixBindTrees[u].clear(&m_DWORD_CTTree_Allocator);
			}
			for (u = 0; u < NUM_CTTree_TREES; u++) {
				m_sharedNonZeroPrefixBindTrees[u].clear(&m_QWORD_CTTree_Allocator);
			}
		}
		m_sharedNonZeroPrefixBindChain.mark_as_clear();
		//Texture ids and memory for non zero prefix tex id pool are not freed on exit
		m_sharedRGBA8TexPool.clear(&m_TexPoolMap_Allocator);
		AllContexts.~TArray<HGLRC>();
	}
#endif
	unguard;
}

void UOpenGLRenderDevice::ShutdownAfterError() {
	guard(UOpenGLRenderDevice::ShutdownAfterError);

	debugf(NAME_Exit, TEXT("UOpenGLRenderDevice::ShutdownAfterError"));

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utglr: ShutdownAfterError") << std::endl;
	}

	//ChangeDisplaySettings(NULL, 0);

	//Reset gamma ramp
	ResetGamma();

	unguard;
}


UBOOL UOpenGLRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	unsigned int u;

	guard(UOpenGlRenderDevice::SetRes);

	//Get debug bits
	{
		INT i = 0;
		if (!GConfig->GetInt(g_pSection, TEXT("DebugBits"), i)) i = 0;
		m_debugBits = i;
	}
	//Display debug bits
	if (DebugBit(DEBUG_BIT_ANY)) dout << TEXT("utglr: DebugBits = ") << m_debugBits << std::endl;

#ifdef __LINUX__
	UnsetRes();

	INT MinDepthBits;
	
	m_usingAA = false;
	m_curAAEnable = true;
	m_defAAEnable = true;

	// Minimum size of the depth buffer
	if (!GConfig->GetInt(g_pSection, TEXT("MinDepthBits"), MinDepthBits)) MinDepthBits = 16;
	//debugf( TEXT("MinDepthBits = %i"), MinDepthBits );
	// 16 is the bare minimum.
	if (MinDepthBits < 16) MinDepthBits = 16; 

	INT RequestDoubleBuffer;
	if (!GConfig->GetInt(g_pSection, TEXT("RequestDoubleBuffer"), RequestDoubleBuffer)) RequestDoubleBuffer = 1;

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, (NewColorBytes <= 2) ? 5 : 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, (NewColorBytes <= 2) ? 5 : 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, (NewColorBytes <= 2) ? 5 : 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, MinDepthBits);
	if (RequestDoubleBuffer) {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	}
	else {
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
	}

	//TODO: Set this correctly
	m_numDepthBits = MinDepthBits;

	// Change window size.
	Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes);
#else
	debugf(TEXT("Enter SetRes()"));

	// If not fullscreen, and color bytes hasn't changed, do nothing.
	if (m_hRC && !Fullscreen && !WasFullscreen && NewColorBytes == Viewport->ColorBytes) {
		if (!Viewport->ResizeViewport(BLIT_HardwarePaint | BLIT_OpenGL, NewX, NewY, NewColorBytes)) {
			return 0;
		}
		glViewport(0, 0, NewX, NewY);

		return 1;
	}

	// Exit res.
	if (m_hRC) {
		debugf(TEXT("UnSetRes() -> hRC != NULL"));
		UnsetRes();
	}

#if !STDGL
	Fullscreen = 1; /* Minidrivers are fullscreen only!! */
#endif

	// Change display settings.
	if (Fullscreen && STDGL) {
		INT FindX = NewX, FindY = NewY, BestError = MAXINT;
		for (INT i = 0; i < Modes.Num(); i++) {
			if (Modes(i).Z==NewColorBytes*8) {
				INT Error
				=	(Modes(i).X-FindX)*(Modes(i).X-FindX)
				+	(Modes(i).Y-FindY)*(Modes(i).Y-FindY);
				if (Error < BestError) {
					NewX      = Modes(i).X;
					NewY      = Modes(i).Y;
					BestError = Error;
				}
			}
		}
		{
			DEVMODEA dma;
			DEVMODEW dmw;
			bool tryNoRefreshRate = true;

			ZeroMemory(&dma, sizeof(dma));
			ZeroMemory(&dmw, sizeof(dmw));
			dma.dmSize = sizeof(dma);
			dmw.dmSize = sizeof(dmw);
			dma.dmPelsWidth = dmw.dmPelsWidth = NewX;
			dma.dmPelsHeight = dmw.dmPelsHeight = NewY;
			dma.dmBitsPerPel = dmw.dmBitsPerPel = NewColorBytes * 8;
			dma.dmFields = dmw.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;// | DM_BITSPERPEL;
			if (RefreshRate) {
				dma.dmDisplayFrequency = dmw.dmDisplayFrequency = RefreshRate;
				dma.dmFields |= DM_DISPLAYFREQUENCY;
				dmw.dmFields |= DM_DISPLAYFREQUENCY;

				if (TCHAR_CALL_OS(ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN), ChangeDisplaySettingsA(&dma, CDS_FULLSCREEN)) != DISP_CHANGE_SUCCESSFUL) {
					debugf(TEXT("ChangeDisplaySettings failed: %ix%i, %i Hz"), NewX, NewY, RefreshRate);
					dma.dmFields &= ~DM_DISPLAYFREQUENCY;
					dmw.dmFields &= ~DM_DISPLAYFREQUENCY;
				}
				else {
					tryNoRefreshRate = false;
				}
			}
			if (tryNoRefreshRate) {
				if (TCHAR_CALL_OS(ChangeDisplaySettingsW(&dmw, CDS_FULLSCREEN), ChangeDisplaySettingsA(&dma, CDS_FULLSCREEN)) != DISP_CHANGE_SUCCESSFUL) {
					debugf(TEXT("ChangeDisplaySettings failed: %ix%i"), NewX, NewY);

					return 0;
				}
			}
		}
	}

	// Change window size.
	UBOOL Result = Viewport->ResizeViewport(Fullscreen ? (BLIT_Fullscreen | BLIT_OpenGL) : (BLIT_HardwarePaint | BLIT_OpenGL), NewX, NewY, NewColorBytes);
	if (!Result) {
		if (Fullscreen) {
			TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL,0), ChangeDisplaySettingsA(NULL,0));
		}

		return 0;
	}


	//Set default numDepthBits in case any failures might prevent it from being set later
	m_numDepthBits = 16;

	//Restrict NumAASamples range
	if (NumAASamples < 1) NumAASamples = 1;

	m_usingAA = false;
	m_curAAEnable = true;
	m_defAAEnable = true;

	bool doBasicInit = true;
	if (UseAA) {
		if (SetAAPixelFormat(NewColorBytes)) {
			doBasicInit = false;
			m_usingAA = true;
		}
	}
	if (doBasicInit) {
		SetBasicPixelFormat(NewColorBytes);
	}

	if (ShareLists && AllContexts.Num()) {
		verify(wglShareLists(AllContexts(0), m_hRC) == 1);
	}
	AllContexts.AddItem(m_hRC);
#endif

	//Reset previous SwapBuffers status to okay
	m_prevSwapBuffersStatus = true;

	// Get info and extensions.

	//PrintFormat( hDC, nPixelFormat );
	debugf(NAME_Init, TEXT("GL_VENDOR     : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_VENDOR)));
	debugf(NAME_Init, TEXT("GL_RENDERER   : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_RENDERER)));
	debugf(NAME_Init, TEXT("GL_VERSION    : %s"), appFromAnsi((const ANSICHAR *)glGetString(GL_VERSION)));

	// vogel: logging of more than 1024 characters is dangerous at the moment.
	const char *pGLExtensions = (const char *)glGetString(GL_EXTENSIONS);
	if (strlen(pGLExtensions) < 1024) {
		debugf(NAME_Init, TEXT("GL_EXTENSIONS : %s"), appFromAnsi(pGLExtensions));
	}
	else {
#ifdef __LINUX__
		printf("GL_EXTENSIONS : %s\n", pGLExtensions);
#endif
	}

	FindProcs(true);

	debugf(NAME_Init, TEXT("Depth bits: %u"), m_numDepthBits);

	// Set modelview.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1.0f, -1.0f, -1.0f);

	//Get other defaults
	if (!GConfig->GetInt(g_pSection, TEXT("Brightness"), Brightness)) Brightness = 0;

	//Debug parameter listing
	if (DebugBit(DEBUG_BIT_BASIC)) {
		#define UTGLR_DEBUG_SHOW_PARAM_REG(_name) DbgPrintInitParam(TEXT(#_name), _name)
		#define UTGLR_DEBUG_SHOW_PARAM_DCV(_name) DbgPrintInitParam(TEXT(#_name), DCV._name)

		UTGLR_DEBUG_SHOW_PARAM_REG(LODBias);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffset);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetRed);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetGreen);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaOffsetBlue);
		UTGLR_DEBUG_SHOW_PARAM_REG(Brightness);
		UTGLR_DEBUG_SHOW_PARAM_REG(GammaCorrectScreenshots);
		UTGLR_DEBUG_SHOW_PARAM_REG(OneXBlending);
		UTGLR_DEBUG_SHOW_PARAM_REG(MinLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxLogTextureSize);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxAnisotropy);
		UTGLR_DEBUG_SHOW_PARAM_REG(TMUnits);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaxTMUnits);
		UTGLR_DEBUG_SHOW_PARAM_REG(RefreshRate);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseZTrick);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseBGRATextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseMultiTexture);
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePalette);
		UTGLR_DEBUG_SHOW_PARAM_REG(ShareLists);
//		UTGLR_DEBUG_SHOW_PARAM_REG(AlwaysMipmap);
		UTGLR_DEBUG_SHOW_PARAM_REG(UsePrecache);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTrilinear);
//		UTGLR_DEBUG_SHOW_PARAM_REG(AutoGenerateMipmaps);
//		UTGLR_DEBUG_SHOW_PARAM_REG(UseVertexSpecular);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseAlphaPalette);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseS3TC);
		UTGLR_DEBUG_SHOW_PARAM_REG(Use16BitTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTNT);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseCVA);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoFiltering);
		UTGLR_DEBUG_SHOW_PARAM_REG(DetailMax);
//		UTGLR_DEBUG_SHOW_PARAM_REG(UseDetailAlpha);
		UTGLR_DEBUG_SHOW_PARAM_REG(DetailClipping);
		UTGLR_DEBUG_SHOW_PARAM_REG(ColorizeDetailTextures);
		UTGLR_DEBUG_SHOW_PARAM_DCV(SinglePassFog);
		UTGLR_DEBUG_SHOW_PARAM_DCV(SinglePassDetail);
//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferActorTris);
//		UTGLR_DEBUG_SHOW_PARAM_REG(BufferClippedActorTris);
		UTGLR_DEBUG_SHOW_PARAM_REG(BufferTileQuads);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseSSE2);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexIdPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseTexPool);
		UTGLR_DEBUG_SHOW_PARAM_REG(CacheStaticMaps);
		UTGLR_DEBUG_SHOW_PARAM_REG(DynamicTexIdRecycleLevel);
		UTGLR_DEBUG_SHOW_PARAM_REG(TexDXT1ToDXT3);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseMultiDrawArrays);
		UTGLR_DEBUG_SHOW_PARAM_DCV(UseVertexProgram);
		UTGLR_DEBUG_SHOW_PARAM_DCV(UseFragmentProgram);
		UTGLR_DEBUG_SHOW_PARAM_REG(SwapInterval);
		UTGLR_DEBUG_SHOW_PARAM_REG(FrameRateLimit);
		UTGLR_DEBUG_SHOW_PARAM_REG(SceneNodeHack);
		UTGLR_DEBUG_SHOW_PARAM_REG(SmoothMaskedTextures);
		UTGLR_DEBUG_SHOW_PARAM_REG(MaskedTextureHack);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseAA);
		UTGLR_DEBUG_SHOW_PARAM_REG(NumAASamples);
		UTGLR_DEBUG_SHOW_PARAM_REG(NoAATiles);
		UTGLR_DEBUG_SHOW_PARAM_REG(ZRangeHack);

		#undef UTGLR_DEBUG_SHOW_PARAM_REG
		#undef UTGLR_DEBUG_SHOW_PARAM_DCV
	}

	//Special handling for WGL_EXT_swap_control
	//Restricted to a maximum of 10
	if ((SwapInterval >= 0) && (SwapInterval <= 10)) {
#ifdef WIN32
		const char *pWGLExtensions = NULL;
		PFNWGLGETEXTENSIONSSTRINGARBPROC p_wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));
		if (p_wglGetExtensionsStringARB != NULL) {
			pWGLExtensions = p_wglGetExtensionsStringARB(m_hDC);
		}

		//See if WGL extension string was retrieved
		if (pWGLExtensions != NULL) {
			//See if WGL_EXT_swap_control is supported
			if (IsGLExtensionSupported(pWGLExtensions, "WGL_EXT_swap_control")) {
				//Get the extension function pointer and use it if successful
				PFNWGLSWAPINTERVALEXTPROC p_wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));
				if (p_wglSwapIntervalEXT != NULL) {
					//Set the swap interval
					p_wglSwapIntervalEXT(SwapInterval);
				}
			}
		}
#else
		//Add non-win32 swap control code here
#endif
	}


#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE) {
		if (!CPU_DetectSSE()) {
			UseSSE = 0;
		}
	}
	if (UseSSE2) {
		if (!CPU_DetectSSE2()) {
			UseSSE2 = 0;
		}
	}
#else
	UseSSE = 0;
	UseSSE2 = 0;
#endif
	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utglr: UseSSE = ") << UseSSE << std::endl;
	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utglr: UseSSE2 = ") << UseSSE2 << std::endl;

	SetGamma(Viewport->GetOuterUClient()->Brightness);

	//Restrict dynamic tex id recycle level range
	if (DynamicTexIdRecycleLevel < 10) DynamicTexIdRecycleLevel = 10;

	AlwaysMipmap = 0;
	AutoGenerateMipmaps = 0;
	UseVertexSpecular = 1;

	SupportsTC = UseS3TC;

	BufferClippedActorTris = 1;

	UseDetailAlpha = 1;


	//Dependent extensions

	//ARB extension uses similar tokens
	SUPPORTS_GL_EXT_texture_env_combine |= SUPPORTS_GL_ARB_texture_env_combine;

	//Fragment program mode uses vertex program defines
	if (!SUPPORTS_GL_ARB_vertex_program) SUPPORTS_GL_ARB_fragment_program = 0;


	// Validate flags.

	//Special extensions validation for init only config pass
	if (!SUPPORTS_GL_ARB_multitexture) UseMultiTexture = 0;
	if (!SUPPORTS_GL_EXT_secondary_color) UseVertexSpecular = 0;
	if (!SUPPORTS_GL_EXT_texture_compression_s3tc || !SUPPORTS_GL_ARB_texture_compression) SupportsTC = 0;

	//DCV refresh
	ConfigValidate_RefreshDCV();

	//Required extensions config validation pass
	ConfigValidate_RequiredExtensions();


	if (!MaxTMUnits || (MaxTMUnits > MAX_TMUNITS)) {
		MaxTMUnits = MAX_TMUNITS;
	}

	if (UseMultiTexture) {
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &TMUnits);
		debugf(TEXT("%i Texture Mapping Units found"), TMUnits);
		if (TMUnits > MaxTMUnits) {
			TMUnits = MaxTMUnits;
		}
	}
	else {
		TMUnits = 1;
	}


	//Main config validation pass (after set TMUnits)
	ConfigValidate_Main();


	//Reset vertex program state tracking variables
	m_vpModeEnabled = false;
	m_vpCurrent = 0;

	//Mark vertex program names as not allocated
	m_allocatedVertexProgramNames = false;
	//Setup vertex programs
	if (UseVertexProgram) {
		//Attempt to initialize vertex program mode
		TryInitializeVertexProgramMode();
	}

	//Fragment program mode requires vertex program mode
	if (!UseVertexProgram) UseFragmentProgram = 0;


	//Reset fragment program state tracking variables
	m_fpModeEnabled = false;
	m_fpCurrent = 0;

	//Mark fragment program names as not allocated
	m_allocatedFragmentProgramNames = false;
	//Setup fragment programs
	if (UseFragmentProgram) {
		//Attempt to initialize fragment program mode
		TryInitializeFragmentProgramMode();
	}


	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy) {
		GLint iMaxAnisotropyLimit = 1;
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iMaxAnisotropyLimit);
		debugf(TEXT("MaxAnisotropy = %i"), iMaxAnisotropyLimit); 
		if (MaxAnisotropy > iMaxAnisotropyLimit) {
			MaxAnisotropy = iMaxAnisotropyLimit;
		}
	}

	BufferActorTris = UseVertexSpecular; // Only buffer when we can use 1 pass fogging

	if (SupportsTC) {
		debugf(TEXT("Trying to use S3TC extension."));
	}

	//Use default if MaxLogTextureSize <= 0
	if (MaxLogTextureSize <= 0) MaxLogTextureSize = 12;

	INT MaxTextureSize;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &MaxTextureSize);
	INT Dummy = -1;
	while (MaxTextureSize > 0) {
		MaxTextureSize >>= 1;
		Dummy++;
	}

	if ((MaxLogTextureSize > Dummy) || (SupportsTC)) MaxLogTextureSize = Dummy;
	if ((MinLogTextureSize < 2) || (SupportsTC)) MinLogTextureSize = 2;

	MaxLogUOverV = MaxLogTextureSize;
	MaxLogVOverU = MaxLogTextureSize;

	debugf(TEXT("MinLogTextureSize = %i"), MinLogTextureSize);
	debugf(TEXT("MaxLogTextureSize = %i"), MaxLogTextureSize);

	debugf(TEXT("BufferActorTris = %i"), BufferActorTris);

	debugf(TEXT("UseDetailAlpha = %i"), UseDetailAlpha);


	//Set pointers to aligned memory
	VertexArray = (FGLVertex *)AlignMemPtr(m_VertexArrayMem, VERTEX_ARRAY_ALIGN);
	NormalArray = (FGLNormal *)AlignMemPtr(m_NormalArrayMem, VERTEX_ARRAY_ALIGN);
	for (u = 0; u < MAX_TMUNITS; u++) {
		TexCoordArray[u] = (FGLTexCoord *)AlignMemPtr(m_TexCoordArrayMem[u], VERTEX_ARRAY_ALIGN);
	}
	MapDotArray = (FGLMapDot *)AlignMemPtr(m_MapDotArrayMem, VERTEX_ARRAY_ALIGN);
	SingleColorArray = (FGLSingleColor *)AlignMemPtr(m_ColorArrayMem, VERTEX_ARRAY_ALIGN);
	DoubleColorArray = (FGLDoubleColor *)AlignMemPtr(m_ColorArrayMem, VERTEX_ARRAY_ALIGN);


	// Verify hardware defaults.
	check(MinLogTextureSize >= 0);
	check(MaxLogTextureSize >= 0);
	check(MinLogTextureSize <= MaxLogTextureSize);

	// Flush textures.
	Flush(1);

	//Invalidate fixed texture ids
	m_noTextureId = 0;
	m_alphaTextureId = 0;

	// Bind little white RGBA texture to ID 0.
	InitNoTextureSafe();

	// Set permanent state.
	glEnable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_TEXTURE_2D);
	glAlphaFunc(GL_GREATER, 0.5);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glPolygonOffset(-1.0f, -1.0f);
	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);
	glEnable(GL_DITHER);
#ifdef UTGLR_RUNE_BUILD
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0f);
	m_gpFogEnabled = false;
#endif

	if (LODBias) {
		SetTexLODBiasState(TMUnits);
	}

	if (UseDetailAlpha) {	// vogel: alpha texture for better detail textures (no vertex alpha)
		InitAlphaTextureSafe();
	}

	//Vertex array
	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_FLOAT, sizeof(FGLVertex), &VertexArray[0].x);

	//Normal array
	glNormalPointer(GL_FLOAT, sizeof(FGLNormal), &NormalArray[0].x);

	//TexCoord arrays
	if (UseMultiTexture) {
		//Potential driver bug workaround
		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glTexCoordPointer(2, GL_FLOAT, sizeof(FGLTexCoord), &TexCoordArray[0][0].u);
	if (UseMultiTexture) {
		glClientActiveTextureARB(GL_TEXTURE1_ARB);
		glTexCoordPointer(2, GL_FLOAT, sizeof(FGLTexCoord), &TexCoordArray[1][0].u);
		if (TMUnits > 2) {
			glClientActiveTextureARB(GL_TEXTURE2_ARB);
			glTexCoordPointer(2, GL_FLOAT, sizeof(FGLTexCoord), &TexCoordArray[2][0].u);
		}
		if (TMUnits > 3) {
			glClientActiveTextureARB(GL_TEXTURE3_ARB);
			glTexCoordPointer(2, GL_FLOAT, sizeof(FGLTexCoord), &TexCoordArray[3][0].u);
		}
		glClientActiveTextureARB(GL_TEXTURE0_ARB);
	}

	//Set primary color array for single color mode by default
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FGLSingleColor), &SingleColorArray[0].color);
	if (UseVertexSpecular) {
		//Secondary color array is always set for double color mode
		glSecondaryColorPointerEXT(3, GL_UNSIGNED_BYTE, sizeof(FGLDoubleColor), &DoubleColorArray[0].specular);
	}

	//Initialize texture state cache information
	m_texEnableBits = 0x1;
	m_clientTexEnableBits = 0x1;

	// Init variables.
	BufferedVerts = 0;
	BufferedTileVerts = 0;

	m_curBlendFlags = PF_Occlude;
	m_smoothMaskedTexturesBit = 0;
	m_curPolyFlags = 0;
	m_curPolyFlags2 = 0;

	//Initialize texture environment state
	InitOrInvalidateTexEnvState();
	SetPermanentTexEnvState(TMUnits);

	//Initialize color flags
	m_currentColorFlags = 0;
	m_requestedColorFlags = 0;

	//Initialize Z range hack state
	m_useZRangeHack = false;
	m_nearZRangeHackProjectionActive = false;
	m_requestNearZRangeHackProjection = false;


	//Initialize previous lock variables
	PL_DetailTextures = DetailTextures;
	PL_OneXBlending = OneXBlending;
	PL_MaxLogUOverV = MaxLogUOverV;
	PL_MaxLogVOverU = MaxLogVOverU;
	PL_MinLogTextureSize = MinLogTextureSize;
	PL_MaxLogTextureSize = MaxLogTextureSize;
	PL_NoFiltering = NoFiltering;
	PL_AlwaysMipmap = AlwaysMipmap;
	PL_UseTrilinear = UseTrilinear;
	PL_Use16BitTextures = Use16BitTextures;
	PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
	PL_AutoGenerateMipmaps = AutoGenerateMipmaps;
	PL_MaxAnisotropy = MaxAnisotropy;
	PL_UseTNT = UseTNT;
	PL_SmoothMaskedTextures = SmoothMaskedTextures;
	PL_MaskedTextureHack = MaskedTextureHack;
	PL_LODBias = LODBias;
	PL_UsePalette = UsePalette;
	PL_UseAlphaPalette = UseAlphaPalette;
	PL_UseDetailAlpha = UseDetailAlpha;
	PL_SinglePassDetail = SinglePassDetail;
	PL_UseVertexProgram = UseVertexProgram;
	PL_UseFragmentProgram = UseFragmentProgram;
	PL_UseSSE = UseSSE;
	PL_UseSSE2 = UseSSE2;


	//Reset current frame count
	m_currentFrameCount = 0;

	// Remember fullscreenness.
	WasFullscreen = Fullscreen;

	return 1;

	unguard;
}

void UOpenGLRenderDevice::UnsetRes() {
	guard(UOpenGLRenderDevice::UnsetRes);

#ifdef WIN32
	check(m_hRC)
#endif

	//Flush textures
	Flush(1);

	//Free fixed textures if they were allocated
	if (m_noTextureId != 0) {
		glDeleteTextures(1, &m_noTextureId);
		m_noTextureId = 0;
	}
	if (m_alphaTextureId != 0) {
		glDeleteTextures(1, &m_alphaTextureId);
		m_alphaTextureId = 0;
	}

	//Free vertex programs if they were allocated and leave vertex program mode if necessary
	ShutdownVertexProgramMode();

	//Free fragment programs if they were allocated and leave fragment program mode if necessary
	ShutdownFragmentProgramMode();

#ifdef WIN32
	hCurrentRC = NULL;
	wglMakeCurrent(NULL, NULL);
//	verify(wglDeleteContext(m_hRC));
	wglDeleteContext(m_hRC);
	verify(AllContexts.RemoveItem(m_hRC) == 1);
	m_hRC = NULL;
	if (WasFullscreen) {
		TCHAR_CALL_OS(ChangeDisplaySettingsW(NULL, 0), ChangeDisplaySettingsA(NULL, 0));
	}
#endif

	unguard;
}


void UOpenGLRenderDevice::MakeCurrent(void) {
	guard(UOpenGLRenderDevice::MakeCurrent);
#ifdef WIN32
	check(m_hRC);
	check(m_hDC);
	if (hCurrentRC != m_hRC) {
		verify(wglMakeCurrent(m_hDC, m_hRC));
		hCurrentRC = m_hRC;
	}
#endif
	unguard;
}

void UOpenGLRenderDevice::CheckGLErrorFlag(const TCHAR *pTag) {
	GLenum Error = glGetError();
	if ((Error != GL_NO_ERROR) && DebugBit(DEBUG_BIT_GL_ERROR)) {
		const TCHAR *pMsg;
		switch (Error) {
		case GL_INVALID_ENUM:
			pMsg = TEXT("GL_INVALID_ENUM");
			break;
		case GL_INVALID_VALUE:
			pMsg = TEXT("GL_INVALID_VALUE");
			break;
		case GL_INVALID_OPERATION:
			pMsg = TEXT("GL_INVALID_OPERATION");
			break;
		case GL_STACK_OVERFLOW:
			pMsg = TEXT("GL_STACK_OVERFLOW");
			break;
		case GL_STACK_UNDERFLOW:
			pMsg = TEXT("GL_STACK_UNDERFLOW");
			break;
		case GL_OUT_OF_MEMORY:
			pMsg = TEXT("GL_OUT_OF_MEMORY");
			break;
		default:
			pMsg = TEXT("UNKNOWN");
		}
		//appErrorf(TEXT("OpenGL Error: %s (%s)"), pMsg, pTag);
		debugf(TEXT("OpenGL Error: %s (%s)"), pMsg, pTag);
	}
}


void UOpenGLRenderDevice::ConfigValidate_RefreshDCV(void) {
	#define UTGLR_REFRESH_DCV(_name) _name = DCV._name

	UTGLR_REFRESH_DCV(SinglePassFog);
	UTGLR_REFRESH_DCV(SinglePassDetail);
	UTGLR_REFRESH_DCV(UseVertexProgram);
	UTGLR_REFRESH_DCV(UseFragmentProgram);

	#undef UTGLR_REFRESH_DCV

	return;
}

void UOpenGLRenderDevice::ConfigValidate_RequiredExtensions(void) {
	if (!SUPPORTS_GL_ARB_fragment_program) UseFragmentProgram = 0;
	if (!SUPPORTS_GL_ARB_vertex_program) UseVertexProgram = 0;
	if (!SUPPORTS_GL_EXT_bgra) UseBGRATextures = 0;
	if (!SUPPORTS_GL_EXT_compiled_vertex_array) UseCVA = 0;
	if (!SUPPORTS_GL_EXT_multi_draw_arrays) UseMultiDrawArrays = 0;
	if (!SUPPORTS_GL_EXT_paletted_texture) UsePalette = 0;
	if (!SUPPORTS_GL_EXT_texture_env_combine) DetailTextures = 0;
	if (!SUPPORTS_GL_EXT_texture_env_combine) UseDetailAlpha = 0;
	if (!SUPPORTS_GL_EXT_texture_filter_anisotropic) MaxAnisotropy  = 0;
	if (!SUPPORTS_GL_EXT_texture_lod_bias) LODBias = 0;
	if (!SUPPORTS_GL_SGIS_generate_mipmap) AutoGenerateMipmaps = 0;

	if (!SUPPORTS_GL_ATI_texture_env_combine3 && !SUPPORTS_GL_NV_texture_env_combine4) SinglePassFog = 0;

	//Force 1X blending if no tex env combine support
	if (!SUPPORTS_GL_EXT_texture_env_combine) OneXBlending = 0x1;	//Must use proper bit offset for Bool param

	return;
}

void UOpenGLRenderDevice::ConfigValidate_Main(void) {
	//Detail alpha requires at least two texture units
	if (TMUnits < 2) UseDetailAlpha = 0;

	//Single pass detail texturing requires at least 4 texture units
	if (TMUnits < 4) SinglePassDetail = 0;
	//Single pass detail texturing requires detail alpha
	if (!UseDetailAlpha) SinglePassDetail = 0;

	//Limit maximum DetailMax
	if (DetailMax > 3) DetailMax = 3;

	//Must use detail alpha for vertex program detail textures
	if (DetailTextures) {
		if (!UseDetailAlpha) {
			UseVertexProgram = 0;
		}
	}

	//Fragment program mode requires vertex program mode
	if (!UseVertexProgram) UseFragmentProgram = 0;

	//UseTNT cannot be used with texture compression
	if (SupportsTC) {
		UseTNT = 0;
	}

	return;
}


#ifdef WIN32
void UOpenGLRenderDevice::InitARBPixelFormat(INT NewColorBytes, InitARBPixelFormatRet_t *pRet) {
	guard(UOpenGLRenderDevice::InitARBPixelFormat);

	HINSTANCE hInstance = TCHAR_CALL_OS(GetModuleHandleW(NULL), GetModuleHandleA(NULL));
	struct tagWNDCLASSA wcA;
	struct tagWNDCLASSW wcW;
	const CHAR *pClassNameA = "UOpenGLRenderDevice::InitARBPixelFormat";
	const WCHAR *pClassNameW = L"UOpenGLRenderDevice::InitARBPixelFormat";
	InitARBPixelFormatWndProcParams_t initParams;
	HWND hWnd;

	wcA.style = wcW.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	wcA.lpfnWndProc = wcW.lpfnWndProc = UOpenGLRenderDevice::InitARBPixelFormatWndProc;
	wcA.cbClsExtra = wcW.cbClsExtra = 0;
	wcA.cbWndExtra = wcW.cbWndExtra = 0;
	wcA.hInstance = wcW.hInstance = hInstance;
	wcA.hIcon = wcW.hIcon = NULL;
	wcA.hCursor = wcW.hCursor = TCHAR_CALL_OS(LoadCursorW(NULL, MAKEINTRESOURCEW(32512)/*IDC_ARROW*/), LoadCursorA(NULL, MAKEINTRESOURCEA(32512)/*IDC_ARROW*/));
	wcA.hbrBackground = wcW.hbrBackground = NULL;
	wcA.lpszMenuName = NULL;
	wcW.lpszMenuName = NULL;
	wcA.lpszClassName = pClassNameA;
	wcW.lpszClassName = pClassNameW;

	if (TCHAR_CALL_OS(RegisterClassW(&wcW), RegisterClassA(&wcA)) == 0) {
		return;
	}

	initParams.NewColorBytes = NewColorBytes;
	initParams.p_wglChoosePixelFormatARB = NULL;
	initParams.haveWGLMultisampleARB = false;

	hWnd = TCHAR_CALL_OS(
		CreateWindowW(
		pClassNameW,
		pClassNameW,
		WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0, 0,
		20, 20,
		NULL,
		NULL,
		hInstance,
		&initParams),
		CreateWindowA(
		pClassNameA,
		pClassNameA,
		WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0, 0,
		20, 20,
		NULL,
		NULL,
		hInstance,
		&initParams)
		);
	if (hWnd == NULL) {
		TCHAR_CALL_OS(UnregisterClassW(pClassNameW, hInstance), UnregisterClassA(pClassNameA, hInstance));
		return;
	}

	DestroyWindow(hWnd);

	TCHAR_CALL_OS(UnregisterClassW(pClassNameW, hInstance), UnregisterClassA(pClassNameA, hInstance));

	pRet->p_wglChoosePixelFormatARB = initParams.p_wglChoosePixelFormatARB;
	pRet->haveWGLMultisampleARB = initParams.haveWGLMultisampleARB;

	return;

	unguard;
}

LRESULT CALLBACK UOpenGLRenderDevice::InitARBPixelFormatWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch(uMsg) {
	case WM_CREATE:
		{
			InitARBPixelFormatWndProcParams_t *pInitParams = (InitARBPixelFormatWndProcParams_t *)((LPCREATESTRUCT)lParam)->lpCreateParams;

			HDC hDC = GetDC(hWnd);

			INT NewColorBytes = pInitParams->NewColorBytes;
			INT nPixelFormat;
			BYTE DesiredColorBits   = (NewColorBytes <= 2) ? 16 : 24;
			BYTE DesiredDepthBits   = 32;
			BYTE DesiredStencilBits = 0;
			PIXELFORMATDESCRIPTOR pfd =
			{
				sizeof(PIXELFORMATDESCRIPTOR),
				1,
				PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
				PFD_TYPE_RGBA,
				DesiredColorBits,
				0,0,0,0,0,0,
				0,0,
				0,0,0,0,0,
				DesiredDepthBits,
				DesiredStencilBits,
				0,
				PFD_MAIN_PLANE,
				0,
				0,0,0
			};

			nPixelFormat = ChoosePixelFormat(hDC, &pfd);
			if (!nPixelFormat) {
				pfd.cDepthBits = 24;
				nPixelFormat = ChoosePixelFormat(hDC, &pfd);
			}
			if (!nPixelFormat) {
				pfd.cDepthBits = 16;
				nPixelFormat = ChoosePixelFormat(hDC, &pfd);
			}
			if (nPixelFormat == 0) {
				break;
			}

			if (SetPixelFormat(hDC, nPixelFormat, &pfd) == FALSE) {
				break;
			}
			HGLRC hGLRC = wglCreateContext(hDC);
			if (hGLRC == NULL) {
				break;
			}
			if (wglMakeCurrent(hDC, hGLRC) == FALSE) {
				wglDeleteContext(hGLRC);
				break;
			}

			PFNWGLGETEXTENSIONSSTRINGARBPROC p_wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));
			if (p_wglGetExtensionsStringARB != NULL) {
				const char *pWGLExtensions = p_wglGetExtensionsStringARB(hDC);
				if (pWGLExtensions != NULL) {
					pInitParams->haveWGLMultisampleARB = IsGLExtensionSupported(pWGLExtensions, "WGL_ARB_multisample");
				}
			}
			pInitParams->p_wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));

			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(hGLRC);
		}
		break;

	default:
		return TCHAR_CALL_OS(DefWindowProcW(hWnd, uMsg, wParam, lParam), DefWindowProcA(hWnd, uMsg, wParam, lParam));
	}

	return 0;
}


void UOpenGLRenderDevice::SetBasicPixelFormat(INT NewColorBytes) {
	// Set res.
	INT nPixelFormat;
	BYTE DesiredColorBits   = (NewColorBytes <= 2) ? 16 : 24;
	BYTE DesiredDepthBits   = 32;
	BYTE DesiredStencilBits = 0;
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		DesiredColorBits,
		0,0,0,0,0,0,
		0,0,
		0,0,0,0,0,
		DesiredDepthBits,
		DesiredStencilBits,
		0,
		PFD_MAIN_PLANE,
		0,
		0,0,0
	};

	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utglr: BasicInit") << std::endl;

	nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
	if (!nPixelFormat) {
		pfd.cDepthBits = 24;
		nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
	}
	if (!nPixelFormat) {
		pfd.cDepthBits = 16;
		nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);
	}

	Parse(appCmdLine(), TEXT("PIXELFORMAT="), nPixelFormat);
	debugf(NAME_Init, TEXT("Using pixel format %i"), nPixelFormat);
	check(nPixelFormat);

	verify(SetPixelFormat(m_hDC, nPixelFormat, &pfd));
	m_hRC = wglCreateContext(m_hDC);
	check(m_hRC);

	//Get actual number of depth bits
	if (DescribePixelFormat(m_hDC, nPixelFormat, sizeof(pfd), &pfd)) {
		m_numDepthBits = pfd.cDepthBits;
	}

	MakeCurrent();

	return;
}

bool UOpenGLRenderDevice::SetAAPixelFormat(INT NewColorBytes) {
	InitARBPixelFormatRet_t iapfRet;
	BOOL bRet;
	int iFormats[1];
	int iAttributes[30];
	UINT nNumFormats;
	PIXELFORMATDESCRIPTOR tempPfd;

	iapfRet.p_wglChoosePixelFormatARB = NULL;
	iapfRet.haveWGLMultisampleARB = false;
	InitARBPixelFormat(NewColorBytes, &iapfRet);

	if ((iapfRet.p_wglChoosePixelFormatARB == NULL) || (iapfRet.haveWGLMultisampleARB != true)) {
		return false;
	}

	if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utglr: AAInit") << std::endl;

	iAttributes[0] = WGL_SUPPORT_OPENGL_ARB;
	iAttributes[1] = GL_TRUE;
	iAttributes[2] = WGL_DRAW_TO_WINDOW_ARB;
	iAttributes[3] = GL_TRUE;
	iAttributes[4] = WGL_COLOR_BITS_ARB;
	iAttributes[5] = (NewColorBytes <= 2) ? 16 : 24;
	iAttributes[6] = WGL_DEPTH_BITS_ARB;
	iAttributes[7] = 32;
	iAttributes[8] = WGL_DOUBLE_BUFFER_ARB;
	iAttributes[9] = GL_TRUE;
	iAttributes[10] = WGL_SAMPLE_BUFFERS_ARB;
	iAttributes[11] = GL_TRUE;
	iAttributes[12] = WGL_SAMPLES_ARB;
	iAttributes[13] = NumAASamples;
	iAttributes[14] = 0;
	iAttributes[15] = 0;

	bRet = iapfRet.p_wglChoosePixelFormatARB(m_hDC, iAttributes, NULL, 1, iFormats, &nNumFormats);
	if ((bRet == FALSE) || (nNumFormats == 0)) {
		iAttributes[7] = 24;
		bRet = iapfRet.p_wglChoosePixelFormatARB(m_hDC, iAttributes, NULL, 1, iFormats, &nNumFormats);
	}
	if ((bRet == FALSE) || (nNumFormats == 0)) {
		iAttributes[7] = 16;
		bRet = iapfRet.p_wglChoosePixelFormatARB(m_hDC, iAttributes, NULL, 1, iFormats, &nNumFormats);
	}
	if ((bRet == FALSE) || (nNumFormats == 0)) {
		if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utglr: AAInit failed") << std::endl;
		return false;
	}

	appMemzero(&tempPfd, sizeof(tempPfd));
	tempPfd.nSize = sizeof(tempPfd);
	verify(SetPixelFormat(m_hDC, iFormats[0], &tempPfd));
	m_hRC = wglCreateContext(m_hDC);
	check(m_hRC);

	MakeCurrent();

	//Get actual number of depth bits
	PFNWGLGETPIXELFORMATATTRIBIVARBPROC p_wglGetPixelFormatAttribivARB = reinterpret_cast<PFNWGLGETPIXELFORMATATTRIBIVARBPROC>(wglGetProcAddress("wglGetPixelFormatAttribivARB"));
	if (p_wglGetPixelFormatAttribivARB != NULL) {
		int iAttribute = WGL_DEPTH_BITS_ARB;
		int iValue = m_numDepthBits;

		if (p_wglGetPixelFormatAttribivARB(m_hDC, iFormats[0], 0, 1, &iAttribute, &iValue)) {
			m_numDepthBits = iValue;
		}
	}

	return true;
}
#endif


UBOOL UOpenGLRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UOpenGLRenderDevice::Init);

	debugf(TEXT("Initializing OpenGLDrv..."));

	if (NumDevices == 0) {
		g_gammaFirstTime = true;
		g_haveOriginalGammaRamp = false;
	}

#ifdef __LINUX__
	// Init global GL.
	if (NumDevices == 0) {
		// Bind the library.
		FString OpenGLLibName;
		// Default to libGL.so.1 if not defined
		if (!GConfig->GetString(g_pSection, TEXT("OpenGLLibName"), OpenGLLibName)) {
			OpenGLLibName = TEXT("libGL.so.1");
		}

		if (!GLLoaded) {
			// Only call it once as succeeding calls will 'fail'.
			debugf(TEXT("binding %s"), *OpenGLLibName);
			if (SDL_GL_LoadLibrary(*OpenGLLibName) == -1) {
				appErrorf(TEXT(SDL_GetError()));
			}
			GLLoaded = true;
		}

		SUPPORTS_GL = 1;
		FindProcs(false);
		if (!SUPPORTS_GL) {
			return 0;
		}
	}
#else
	// Get list of device modes.
	for (INT i = 0; ; i++) {
		UBOOL UnicodeOS;

#if defined(NO_UNICODE_OS_SUPPORT) || !defined(UNICODE)
		UnicodeOS = 0;
#elif defined(NO_ANSI_OS_SUPPORT)
		UnicodeOS = 1;
#else
		UnicodeOS = GUnicodeOS;
#endif

		if (!UnicodeOS) {
#if defined(NO_UNICODE_OS_SUPPORT) || !defined(UNICODE) || !defined(NO_ANSI_OS_SUPPORT)
			DEVMODEA Tmp;
			appMemzero(&Tmp, sizeof(Tmp));
			Tmp.dmSize = sizeof(Tmp);
			if (!EnumDisplaySettingsA(NULL, i, &Tmp)) {
				break;
			}
			Modes.AddUniqueItem(FPlane(Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency));
#endif
		}
		else {
#if !defined(NO_UNICODE_OS_SUPPORT) && defined(UNICODE)
			DEVMODEW Tmp;
			appMemzero(&Tmp, sizeof(Tmp));
			Tmp.dmSize = sizeof(Tmp);
			if (!EnumDisplaySettingsW(NULL, i, &Tmp)) {
				break;
			}
			Modes.AddUniqueItem(FPlane(Tmp.dmPelsWidth, Tmp.dmPelsHeight, Tmp.dmBitsPerPel, Tmp.dmDisplayFrequency));
#endif
		}
	}

	// Init global GL.
	if (NumDevices == 0) {
#if DYNAMIC_BIND
		// Find DLL's.
		hModuleGlMain = LoadLibraryA(GL_DLL);
		if (!hModuleGlMain) {
			debugf(NAME_Init, LocalizeError("NoFindGL"), appFromAnsi(GL_DLL));
			return 0;
		}
		hModuleGlGdi = LoadLibraryA("GDI32.dll");
		check(hModuleGlGdi);

		// Find functions.
		SUPPORTS_GL = 1;
		FindProcs(false);
		if (!SUPPORTS_GL) {
			return 0;
		}
#endif
	}
#endif

	NumDevices++;

	// Init this GL rendering context.
	m_zeroPrefixBindTrees = ShareLists ? m_sharedZeroPrefixBindTrees : m_localZeroPrefixBindTrees;
	m_nonZeroPrefixBindTrees = ShareLists ? m_sharedNonZeroPrefixBindTrees : m_localNonZeroPrefixBindTrees;
	m_nonZeroPrefixBindChain = ShareLists ? &m_sharedNonZeroPrefixBindChain : &m_localNonZeroPrefixBindChain;
	m_nonZeroPrefixTexIdPool = ShareLists ? &m_sharedNonZeroPrefixTexIdPool : &m_localNonZeroPrefixTexIdPool;
	m_RGBA8TexPool = ShareLists ? &m_sharedRGBA8TexPool : &m_localRGBA8TexPool;

	Viewport = InViewport;

#ifndef __LINUX__
	m_hWnd = (HWND)InViewport->GetWindow();
	check(m_hWnd);
	m_hDC = GetDC(m_hWnd);
	check(m_hDC);
#endif

#if 0
	{
		//Print all PFD's exposed
		INT pf;
		INT pfCount = DescribePixelFormat(m_hDC, 0, 0, NULL);
		for (pf = 1; pf <= pfCount; pf++) {
			PrintFormat(m_hDC, pf);
		}
	}
#endif

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		return FailedInitf(LocalizeError("ResFailed"));
	}

	return 1;
	unguard;
}

UBOOL UOpenGLRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar) {
	guard(UOpenGLRenderDevice::Exec);

#ifndef UTGLR_UNREAL_BUILD
	if (URenderDevice::Exec(Cmd, Ar)) {
		return 1;
	}
#endif
	if (ParseCommand(&Cmd, TEXT("DGL"))) {
		if (ParseCommand(&Cmd, TEXT("BUFFERTRIS"))) {
			BufferActorTris = !BufferActorTris;
			if (!UseVertexSpecular) BufferActorTris = 0;
			debugf(TEXT("BUFFERTRIS [%i]"), BufferActorTris);
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("BUILD"))) {
#ifdef __LINUX__
			debugf(TEXT("OpenGL renderer built: %s %s"), __DATE__, __TIME__);
#else
			debugf(TEXT("OpenGL renderer built: VOGEL FIXME"));
#endif
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("AA"))) {
			if (m_usingAA) {
				m_defAAEnable = !m_defAAEnable;
				debugf(TEXT("AA Enable [%u]"), (m_defAAEnable) ? 1 : 0);
			}
			return 1;
		}

		return 0;
	}
	else if (ParseCommand(&Cmd, TEXT("GetRes"))) {
#ifdef __LINUX__
		// Changing Resolutions:
		// Entries in the resolution box in the console is
		// apparently controled building a string of relevant 
		// resolutions and sending it to the engine via Ar.Log()

		// Here I am querying SDL_ListModes for available resolutions,
		// and populating the dropbox with its output.
		FString Str = "";
		SDL_Rect **modes;
		INT i, j;

		// Available fullscreen video modes
		modes = SDL_ListModes(NULL, SDL_FULLSCREEN);

		if (modes == (SDL_Rect **)0) {
			debugf(NAME_Init, TEXT("No available fullscreen video modes"));
		}
		else if (modes == (SDL_Rect **)-1) {
			debugf(NAME_Init, TEXT("No special fullscreen video modes"));
		}
		else {
			// count the number of available modes
			for (i = 0, j = 0; modes[i]; ++i) {
				++j;
			}

			// Load the string with resolutions from smallest to 
			// largest. SDL_ListModes() provides them from lg
			// to sm...
			for (i = (j - 1); i >= 0; --i) {
				Str += FString::Printf(TEXT("%ix%i "), modes[i]->w, modes[i]->h);
			}
		}

		// Send the resolution string to the engine.
		Ar.Log(*Str.LeftChop(1));
		return 1;
#else
		TArray<FPlane> Relevant;
		INT i;
		for (i = 0; i < Modes.Num(); i++) {
			if (Modes(i).Z == (Viewport->ColorBytes * 8))
				if
				(	(Modes(i).X!=320 || Modes(i).Y!=200)
				&&	(Modes(i).X!=640 || Modes(i).Y!=400) )
				Relevant.AddUniqueItem(FPlane(Modes(i).X, Modes(i).Y, 0, 0));
		}
		appQsort(&Relevant(0), Relevant.Num(), sizeof(FPlane), (QSORT_COMPARE)CompareRes);
		FString Str;
		for (i = 0; i < Relevant.Num(); i++) {
			Str += FString::Printf(TEXT("%ix%i "), (INT)Relevant(i).X, (INT)Relevant(i).Y);
		}
		Ar.Log(*Str.LeftChop(1));
		return 1;
#endif
	}

	return 0;
	unguard;
}

void UOpenGLRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: Lock = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::Lock);
	check(LockCount == 0);
	++LockCount;


	//Reset stats
	BindCycles = ImageCycles = ComplexCycles = GouraudCycles = TileCycles = 0;

	m_vpEnableCount = 0;
	m_vpSwitchCount = 0;
	m_fpEnableCount = 0;
	m_fpSwitchCount = 0;
	m_AASwitchCount = 0;
	m_stat0Count = 0;
	m_stat1Count = 0;


	// Make this context current.
	MakeCurrent();

	// Clear the Z buffer if needed.
	if (!UseZTrick || GIsEditor || (RenderLockFlags & LOCKR_ClearScreen)) {
		glClearColor(ScreenClear.X, ScreenClear.Y, ScreenClear.Z, ScreenClear.W);
		glClearDepth(1.0);
		glDepthRange(0.0, 1.0);
		ZTrickFunc = GL_LEQUAL;
		glPolygonOffset(-1.0f, -1.0f);
		SetBlend(PF_Occlude);
		glClear(GL_DEPTH_BUFFER_BIT | ((RenderLockFlags & LOCKR_ClearScreen) ? GL_COLOR_BUFFER_BIT : 0));
	}
	else if (ZTrickToggle) {
		ZTrickToggle = 0;
		glClearDepth(0.5);
		glDepthRange(0.0, 0.5);
		ZTrickFunc = GL_LEQUAL;
		glPolygonOffset(-1.0f, -1.0f);
	}
	else {
		ZTrickToggle = 1;
		glClearDepth(0.5);
		glDepthRange(1.0, 0.5);
		ZTrickFunc = GL_GEQUAL;
		glPolygonOffset(1.0f, 1.0f);
	}
	glDepthFunc((GLenum)ZTrickFunc);


	bool flushTextures = false;
	bool needVertexProgramReload = false;
	bool needFragmentProgramReload = false;


	//DCV refresh
	ConfigValidate_RefreshDCV();

	//Required extensions config validation pass
	ConfigValidate_RequiredExtensions();


	//Main config validation pass
	ConfigValidate_Main();


	//Detect changes in 1X blending setting and force tex env flush if necessary
	if (OneXBlending != PL_OneXBlending) {
		PL_OneXBlending = OneXBlending;
		InitOrInvalidateTexEnvState();
	}

	//Prevent changes to these parameters
	MaxLogUOverV = PL_MaxLogUOverV;
	MaxLogVOverU = PL_MaxLogVOverU;
	MinLogTextureSize = PL_MinLogTextureSize;
	MaxLogTextureSize = PL_MaxLogTextureSize;

	//Detect changes in various texture related options and force texture flush if necessary
	if (NoFiltering != PL_NoFiltering) {
		PL_NoFiltering = NoFiltering;
		flushTextures = true;
	}
	if (AlwaysMipmap != PL_AlwaysMipmap) {
		PL_AlwaysMipmap = AlwaysMipmap;
		flushTextures = true;
	}
	if (UseTrilinear != PL_UseTrilinear) {
		PL_UseTrilinear = UseTrilinear;
		flushTextures = true;
	}
	if (Use16BitTextures != PL_Use16BitTextures) {
		PL_Use16BitTextures = Use16BitTextures;
		flushTextures = true;
	}
	if (TexDXT1ToDXT3 != PL_TexDXT1ToDXT3) {
		PL_TexDXT1ToDXT3 = TexDXT1ToDXT3;
		flushTextures = true;
	}
	if (AutoGenerateMipmaps != PL_AutoGenerateMipmaps) {
		PL_AutoGenerateMipmaps = AutoGenerateMipmaps;
		flushTextures = true;
	}
	//MaxAnisotropy cannot be negative
	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy != PL_MaxAnisotropy) {
		if (MaxAnisotropy) {
			GLint iMaxAnisotropyLimit = 1;
			glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iMaxAnisotropyLimit);
			if (MaxAnisotropy > iMaxAnisotropyLimit) {
				MaxAnisotropy = iMaxAnisotropyLimit;
			}
		}

		PL_MaxAnisotropy = MaxAnisotropy;
		flushTextures = true;
	}

	if (UseTNT != PL_UseTNT) {
		PL_UseTNT = UseTNT;
		flushTextures = true;
	}

	if (SmoothMaskedTextures != PL_SmoothMaskedTextures) {
		PL_SmoothMaskedTextures = SmoothMaskedTextures;
		//Clear masked blending state if set before adjusting smooth masked textures bit
		SetBlend(PF_Occlude);
	}
	//Set smooth masked textures bit
	m_smoothMaskedTexturesBit = (SmoothMaskedTextures != 0) ? PF_Masked : 0;

	if (MaskedTextureHack != PL_MaskedTextureHack) {
		PL_MaskedTextureHack = MaskedTextureHack;
		flushTextures = true;
	}

	if (LODBias != PL_LODBias) {
		PL_LODBias = LODBias;
		SetTexLODBiasState(TMUnits);
	}

	if (UsePalette != PL_UsePalette) {
		PL_UsePalette = UsePalette;
		flushTextures = true;
	}
	if (UseAlphaPalette != PL_UseAlphaPalette) {
		PL_UseAlphaPalette = UseAlphaPalette;
		flushTextures = true;
	}

	if (DetailTextures != PL_DetailTextures) {
		PL_DetailTextures = DetailTextures;
		flushTextures = true;
		if (DetailTextures) {
			needFragmentProgramReload = true;
		}
	}

	if (UseDetailAlpha != PL_UseDetailAlpha) {
		PL_UseDetailAlpha = UseDetailAlpha;
		if (UseDetailAlpha) {
			InitAlphaTextureSafe();
			needVertexProgramReload = true;
		}
	}

	if (SinglePassDetail != PL_SinglePassDetail) {
		PL_SinglePassDetail = SinglePassDetail;
		if (SinglePassDetail) {
			needVertexProgramReload = true;
		}
	}

	//Extra vertex programs needed in fragment program mode
	if (UseFragmentProgram != PL_UseFragmentProgram) {
		if (UseFragmentProgram) {
			needVertexProgramReload = true;
		}
	}


	if (UseVertexProgram != PL_UseVertexProgram) {
		PL_UseVertexProgram = UseVertexProgram;
		if (UseVertexProgram) {
			//Attempt to initialize vertex program mode
			TryInitializeVertexProgramMode();
			needVertexProgramReload = false;
		}
		else {
			//Free vertex programs if they were allocated and leave vertex program mode if necessary
			ShutdownVertexProgramMode();
		}
	}

	//Check if vertex program reload is necessary
	if (UseVertexProgram) {
		if (needVertexProgramReload) {
			//Attempt to initialize vertex program mode
			TryInitializeVertexProgramMode();
		}
	}

	//Fragment program mode requires vertex program mode
	if (!UseVertexProgram) UseFragmentProgram = 0;


	if (UseFragmentProgram != PL_UseFragmentProgram) {
		PL_UseFragmentProgram = UseFragmentProgram;
		if (UseFragmentProgram) {
			//Attempt to initialize fragment program mode
			TryInitializeFragmentProgramMode();
			needFragmentProgramReload = false;
		}
		else {
			//Free fragment programs if they were allocated and leave fragment program mode if necessary
			ShutdownFragmentProgramMode();
		}
	}

	//Check if fragment program reload is necessary
	if (UseFragmentProgram) {
		if (needFragmentProgramReload) {
			//Attempt to initialize fragment program mode
			TryInitializeFragmentProgramMode();
		}
	}


	if (UseSSE != PL_UseSSE) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		if (UseSSE) {
			if (!CPU_DetectSSE()) {
				UseSSE = 0;
			}
		}
#else
		UseSSE = 0;
#endif
		PL_UseSSE = UseSSE;
	}
	if (UseSSE2 != PL_UseSSE2) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		if (UseSSE2) {
			if (!CPU_DetectSSE2()) {
				UseSSE2 = 0;
			}
		}
#else
		UseSSE2 = 0;
#endif
		PL_UseSSE2 = UseSSE2;
	}


#ifdef UTGLR_UNREAL_BUILD
	ZRangeHack = 0;
#endif


	//Initialize buffer verts proc pointers
	m_pBuffer3BasicVertsProc = Buffer3BasicVerts;
	m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts;
	m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts;

#ifdef UTGLR_INCLUDE_SSE_CODE
	//Initialize SSE buffer verts proc pointers
	if (UseSSE) {
		m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts_SSE;
		m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts_SSE;
	}
	if (UseSSE2) {
		m_pBuffer3ColoredVertsProc = Buffer3ColoredVerts_SSE2;
		m_pBuffer3FoggedVertsProc = Buffer3FoggedVerts_SSE2;
	}
#endif //UTGLR_INCLUDE_SSE_CODE

	m_pBuffer3VertsProc = NULL;


	//Initialize render passes no check proc pointers
	if (UseVertexProgram) {
		m_pRenderPassesNoCheckSetupProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_VP;
		m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_VP;
		if (UseFragmentProgram) {
			m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP;
		}
	}
	else {
		m_pRenderPassesNoCheckSetupProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup;
		m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture;
	}

	//Initialize buffer detail texture data proc pointer
	m_pBufferDetailTextureDataProc = &UOpenGLRenderDevice::BufferDetailTextureData;
#ifdef UTGLR_INCLUDE_SSE_CODE
	if (UseSSE2) {
		m_pBufferDetailTextureDataProc = &UOpenGLRenderDevice::BufferDetailTextureData_SSE2;
	}
#endif //UTGLR_INCLUDE_SSE_CODE


	//Precalculate the cutoff for buffering actor triangles based on config settings
	if (!BufferActorTris) {
		m_bufferActorTrisCutoff = 0;
	}
	else if (!BufferClippedActorTris) {
		m_bufferActorTrisCutoff = 3;
	}
	else {
		m_bufferActorTrisCutoff = 10;
	}

	//Precalculate detail texture colors
	if (ColorizeDetailTextures) {
		m_detailTextureColor3f_1f[0] = 0.25f;
		m_detailTextureColor3f_1f[1] = 0.5f;
		m_detailTextureColor3f_1f[2] = 0.25f;
		m_detailTextureColor4ub = 0x00408040;
	}
	else {
		m_detailTextureColor3f_1f[0] = 0.5f;
		m_detailTextureColor3f_1f[1] = 0.5f;
		m_detailTextureColor3f_1f[2] = 0.5f;
		m_detailTextureColor4ub = 0x00808080;
	}

	//Precalculate mask for MaskedTextureHack based on if it's enabled
	m_maskedTextureHackMask = (MaskedTextureHack) ? TEX_CACHE_ID_FLAG_MASKED : 0;

	// Remember stuff.
	FlashScale = InFlashScale;
	FlashFog   = InFlashFog;
	HitData    = InHitData;
	HitSize    = InHitSize;
	if (HitData) {
		*HitSize = 0;
		if (!GLHitData.Num()) {
			GLHitData.Add(16384);
		}
		glSelectBuffer(GLHitData.Num(), (GLuint*)&GLHitData(0));
		glRenderMode(GL_SELECT);
		glInitNames();
	}

	//Flush textures if necessary due to config change
	if (flushTextures) {
		Flush(1);
	}

	unguard;
}

void UOpenGLRenderDevice::SetSceneNode(FSceneNode* Frame) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: SetSceneNode = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::SetSceneNode);

	EndBuffering();		// Flush vertex array before changing the projection matrix!

	m_sceneNodeCount++;

	//No need to set default AA state here
	//No need to set default projection state as this function always sets/initializes it
	SetDefaultColorState();
	SetDefaultVertexProgramState();
	SetDefaultFragmentProgramState();
	SetDefaultTextureState();

	// Precompute stuff.
	FLOAT One_Over_FX = 1.0f / Frame->FX;
	m_Aspect = Frame->FY * One_Over_FX;
	m_RProjZ = appTan(Viewport->Actor->FovAngle * PI / 360.0);
	m_RFX2 = 2.0f * m_RProjZ * One_Over_FX;
	m_RFY2 = 2.0f * m_RProjZ * One_Over_FX;

	//Remember Frame->X and Frame->Y for scene node hack
	m_sceneNodeX = Frame->X;
	m_sceneNodeY = Frame->Y;

	// Set viewport.
	glViewport(Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y);

	//Decide whether or not to use Z range hack
	m_useZRangeHack = false;
	if (ZRangeHack) {
		m_useZRangeHack = true;
	}

	// Set projection.
	if (Frame->Viewport->IsOrtho()) {
		//Don't use Z range hack if ortho projection
		m_useZRangeHack = false;

		SetOrthoProjection();
	}
	else {
		SetProjectionStateNoCheck(false);
	}

	// Set clip planes.
	if (HitData) {
		if (Frame->Viewport->IsOrtho()) {
			double cp[4];
			FLOAT nX = Viewport->HitX - Frame->FX2;
			FLOAT pX = nX + Viewport->HitXL;
			FLOAT nY = Viewport->HitY - Frame->FY2;
			FLOAT pY = nY + Viewport->HitYL;

			nX *= m_RFX2 * 0.5f;
			pX *= m_RFX2 * 0.5f;
			nY *= m_RFY2 * 0.5f;
			pY *= m_RFY2 * 0.5f;

			cp[0] = 1.0; cp[1] = 0.0; cp[2] = 0.0; cp[3] = -nX;
			glClipPlane(GL_CLIP_PLANE0, cp);
			glEnable(GL_CLIP_PLANE0);

			cp[0] = 0.0; cp[1] = 1.0; cp[2] = 0.0; cp[3] = -nY;
			glClipPlane(GL_CLIP_PLANE1, cp);
			glEnable(GL_CLIP_PLANE1);

			cp[0] = -1.0; cp[1] = 0.0; cp[2] = 0.0; cp[3] = pX;
			glClipPlane(GL_CLIP_PLANE2, cp);
			glEnable(GL_CLIP_PLANE2);

			cp[0] = 0.0; cp[1] = -1.0; cp[2] = 0.0; cp[3] = pY;
			glClipPlane(GL_CLIP_PLANE3, cp);
			glEnable(GL_CLIP_PLANE3);
		}
		else {
			FVector N[4];
			N[0] = (FVector((Viewport->HitX-Frame->FX2)*Frame->RProj.Z,0,1)^FVector(0,-1,0)).SafeNormal();
			N[1] = (FVector((Viewport->HitX+Viewport->HitXL-Frame->FX2)*Frame->RProj.Z,0,1)^FVector(0,+1,0)).SafeNormal();
			N[2] = (FVector(0,(Viewport->HitY-Frame->FY2)*Frame->RProj.Z,1)^FVector(+1,0,0)).SafeNormal();
			N[3] = (FVector(0,(Viewport->HitY+Viewport->HitYL-Frame->FY2)*Frame->RProj.Z,1)^FVector(-1,0,0)).SafeNormal();
			for (INT i = 0; i < 4; i++) {
				double D0[4] = {N[i].X, N[i].Y, N[i].Z, 0};
				glClipPlane((GLenum)(GL_CLIP_PLANE0 + i), D0);
				glEnable((GLenum)(GL_CLIP_PLANE0 + i));
			}
		}
	}

	unguard;
}

void UOpenGLRenderDevice::Unlock(UBOOL Blit) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: Unlock = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::Unlock);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultVertexProgramState();
	SetDefaultFragmentProgramState();
	SetDefaultTextureState();

	// Unlock and render.
	check(LockCount == 1);

	//glFlush();

	if (Blit) {
		CheckGLErrorFlag(TEXT("please report this bug"));
#ifdef __LINUX__
		SDL_GL_SwapBuffers();
#else
		{
			bool SwapBuffersStatus;

			SwapBuffersStatus = (SwapBuffers(m_hDC)) ? true : false;

			if (!m_prevSwapBuffersStatus) {
				check(SwapBuffersStatus);
			}
			m_prevSwapBuffersStatus = SwapBuffersStatus;
		}
#endif
	}

	--LockCount;

	// Hits.
	if (HitData) {
		INT Records = glRenderMode(GL_RENDER);
		INT* Ptr = &GLHitData(0);
		DWORD BestDepth = MAXDWORD;
		INT i;
		for (i = 0; i < Records; i++) {
			INT   NameCount = *Ptr++;
			DWORD MinDepth  = *Ptr++;
			DWORD MaxDepth  = *Ptr++;
			if (MinDepth <= BestDepth) {
				BestDepth = MinDepth;
				*HitSize = 0;
				INT CurName;
				for (CurName = 0; CurName < NameCount; ) {
					INT Count = Ptr[CurName++];
					for (INT j = 0; j < Count; j += 4) {
						*(INT*)(HitData + *HitSize + j) = Ptr[CurName++];
					}
					*HitSize += Count;
				}
				check(CurName == NameCount);
			}
			Ptr += NameCount;
			(void)MaxDepth;
		}
		for (i = 0; i < 4; i++) {
			glDisable((GLenum)(GL_CLIP_PLANE0 + i));
		}
	}

	//Scan for old textures
	if (UseTexIdPool) {
		//Scan for old textures
		ScanForOldTextures();
	}

	//Increment current frame count
	m_currentFrameCount++;

	//Check for optional frame rate limit
	if (FrameRateLimit >= 20) {
#if defined UTGLR_DX_BUILD || defined UTGLR_UNREAL_BUILD || defined UTGLR_RUNE_BUILD
		FLOAT curFrameTimestamp;
#else
		FTime curFrameTimestamp;
#endif
		float timeDiff;
		float rcpFrameRateLimit;

		//First time timer init if necessary
		InitFrameRateLimitTimerSafe();

		curFrameTimestamp = appSeconds();
		timeDiff = curFrameTimestamp - m_prevFrameTimestamp;
		m_prevFrameTimestamp = curFrameTimestamp;

		rcpFrameRateLimit = 1.0f / FrameRateLimit;
		if (timeDiff < rcpFrameRateLimit) {
			float sleepTime;

			sleepTime = rcpFrameRateLimit - timeDiff;
			appSleep(sleepTime);

			m_prevFrameTimestamp = appSeconds();
		}
	}


#if 0
	dout << TEXT("VP enable count = ") << m_vpEnableCount << std::endl;
	dout << TEXT("VP switch count = ") << m_vpSwitchCount << std::endl;
	dout << TEXT("FP enable count = ") << m_fpEnableCount << std::endl;
	dout << TEXT("FP switch count = ") << m_fpSwitchCount << std::endl;
	dout << TEXT("AA switch count = ") << m_AASwitchCount << std::endl;
	dout << TEXT("Scene node count = ") << m_sceneNodeCount << std::endl;
	dout << TEXT("Scene node hack count = ") << m_sceneNodeHackCount << std::endl;
	dout << TEXT("Stat 0 count = ") << m_stat0Count << std::endl;
	dout << TEXT("Stat 1 count = ") << m_stat1Count << std::endl;
#endif


	unguard;
}

#ifdef UTGLR_UNREAL_BUILD
void UOpenGLRenderDevice::Flush() {
	const UBOOL AllowPrecache = 1;
#else
void UOpenGLRenderDevice::Flush(UBOOL AllowPrecache) {
#endif
	guard(UOpenGLRenderDevice::Flush);
	unsigned int u;
	TArray<GLuint> Binds;

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[u];
		for (DWORD_CTTree_t::node_t *zpbmPtr = zeroPrefixBindTree->begin(); zpbmPtr != zeroPrefixBindTree->end(); zpbmPtr = zeroPrefixBindTree->next_node(zpbmPtr)) {
			Binds.AddItem(zpbmPtr->data.Id);
		}
		zeroPrefixBindTree->clear(&m_DWORD_CTTree_Allocator);
	}

	for (u = 0; u < NUM_CTTree_TREES; u++) {
		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[u];
		for (QWORD_CTTree_t::node_t *nzpbmPtr = nonZeroPrefixBindTree->begin(); nzpbmPtr != nonZeroPrefixBindTree->end(); nzpbmPtr = nonZeroPrefixBindTree->next_node(nzpbmPtr)) {
			Binds.AddItem(nzpbmPtr->data.Id);
		}
		nonZeroPrefixBindTree->clear(&m_QWORD_CTTree_Allocator);
	}

	m_nonZeroPrefixBindChain->mark_as_clear();

	while (QWORD_CTTree_NodePool_t::node_t *nzptipPtr = m_nonZeroPrefixTexIdPool->try_remove()) {
		Binds.AddItem(nzptipPtr->data.Id);
		m_QWORD_CTTree_Allocator.free_node(nzptipPtr);
	}

	for (TexPoolMap_t::node_t *RGBA8TpPtr = m_RGBA8TexPool->begin(); RGBA8TpPtr != m_RGBA8TexPool->end(); RGBA8TpPtr = m_RGBA8TexPool->next_node(RGBA8TpPtr)) {
		while (QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr = RGBA8TpPtr->data.try_remove()) {
			Binds.AddItem(texPoolNodePtr->data.Id);
			m_QWORD_CTTree_Allocator.free_node(texPoolNodePtr);
		}
	}
	m_RGBA8TexPool->clear(&m_TexPoolMap_Allocator);

	while (QWORD_CTTree_NodePool_t::node_t *nzpnpPtr = m_nonZeroPrefixNodePool.try_remove()) {
		m_QWORD_CTTree_Allocator.free_node(nzpnpPtr);
	}

	if (Binds.Num()) {
		glDeleteTextures(Binds.Num(), (GLuint*)&Binds(0));
	}
	AllocatedTextures = 0;

	//Reset current texture ids to hopefully unused values
	for (u = 0; u < MAX_TMUNITS; u++) {
		TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
		TexInfo[u].pBind = NULL;
	}

	if (AllowPrecache && UsePrecache && !GIsEditor) {
		PrecacheOnFlip = 1;
	}

	SetGamma(Viewport->GetOuterUClient()->Brightness);

	unguard;
}


void UOpenGLRenderDevice::DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: DrawComplexSurface = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::DrawComplexSurface);

	EndBuffering();		// vogel: might have still been locked (can happen!)

	if (SceneNodeHack) {
		if ((Frame->X != m_sceneNodeX) || (Frame->Y != m_sceneNodeY)) {
			m_sceneNodeHackCount++;
			SetSceneNode(Frame);
		}
	}

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	//This function uses cached vertex program state information
	//This function uses cached fragment program state information
	//This function uses cached texture state information

	check(Surface.Texture);

	clock(ComplexCycles);

	//Calculate UDot and VDot intermediates for complex surface
	m_csUDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	m_csVDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	// Buffer "static" geometry.
	INT Index;
	if (UseVertexProgram) {
		Index = BufferStaticComplexSurfaceGeometry_VP(Facet);
	}
	else {
		Index = BufferStaticComplexSurfaceGeometry(Facet);
	}

	//Reject invalid surfaces early
	if (Index == 0) {
		return;
	}

	//Save number of points
	m_csPtCount = Index;

	//See if detail texture should be drawn
	//FogMap and DetailTexture are mutually exclusive effects
	bool drawDetailTexture = false;
	if ((DetailTextures != 0) && Surface.DetailTexture && !Surface.FogMap) {
		drawDetailTexture = true;
	}

	//Check for detail texture
	if (drawDetailTexture == true) {
		DWORD anyIsNearBits;

		//Buffer detail texture data
		anyIsNearBits = (this->*m_pBufferDetailTextureDataProc)(380.0f);

		//Do not draw detail texture if no vertices are near
		if (anyIsNearBits == 0) {
			drawDetailTexture = false;
		}
	}


	if (UseCVA) {
		DWORD texUnit;
		DWORD texBit;
		DWORD clientTexEnableBitsCopy = m_clientTexEnableBits;

		//Disable client texture arrays
		for (texUnit = 0, texBit = 1U; texBit <= clientTexEnableBitsCopy; texUnit++, texBit <<= 1) {
			//See if the client texture is enabled
			if (texBit & clientTexEnableBitsCopy) {
				//Disable the client texture
				if (SUPPORTS_GL_ARB_multitexture) {
					glClientActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
				}
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}

		//Lock arrays
		glLockArraysEXT(0, Index);

		//Enable client texture arrays
		for (texUnit = 0, texBit = 1U; texBit <= clientTexEnableBitsCopy; texUnit++, texBit <<= 1) {
			//See if the client texture is enabled
			if (texBit & clientTexEnableBitsCopy) {
				//Enable the client texture
				if (SUPPORTS_GL_ARB_multitexture) {
					glClientActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
				}
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}
	}


	DWORD PolyFlags = Surface.PolyFlags;

	//Initialize render passes state information
	m_rpPassCount = 0;
	m_rpTMUnits = TMUnits;
	m_rpForceSingle = false;
	m_rpMasked = ((PolyFlags & PF_Masked) == 0) ? false : true;
	m_rpSetDepthEqual = false;


	//Do static render passes state setup
	if (UseVertexProgram) {
		const FVector &XAxis = Facet.MapCoords.XAxis;
		const FVector &YAxis = Facet.MapCoords.YAxis;

		glVertexAttrib4fARB(6, XAxis.X, XAxis.Y, XAxis.Z, -m_csUDot);
		glVertexAttrib4fARB(7, YAxis.X, YAxis.Y, YAxis.Z, -m_csVDot);
	}
	glColor3f(1.0f, 1.0f, 1.0f);

	AddRenderPass(Surface.Texture, PolyFlags & ~PF_FlatShaded, 0.0f);

	if (Surface.MacroTexture) {
		AddRenderPass(Surface.MacroTexture, PF_Modulated, -0.5f);
	}

	if (Surface.LightMap) {
		AddRenderPass(Surface.LightMap, PF_Modulated, -0.5f);
	}

	if (Surface.FogMap) {
		bool useFragmentProgramSinglePassFog = false;

		//Check if can do single pass fragment program fog
		//Fog must always be the last layer as it currently is (no detail texture allowed if fog map texture)
		if (UseFragmentProgram && DCV.SinglePassFog) {
			if ((m_rpPassCount == 1) || (m_rpPassCount == 2)) {
				useFragmentProgramSinglePassFog = true;
			}
		}

		if (!useFragmentProgramSinglePassFog) {
			//Make fog first pass if no SetTexEnv support for it, or if single pass mode is disabled
			if (!SinglePassFog) {
				RenderPasses();
			}
		}

		AddRenderPass(Surface.FogMap, PF_Highlighted, -0.5f);
	}

	// Draw detail texture overlaid, in a separate pass.
	if (drawDetailTexture == true) {
		bool singlePassDetail = false;

		//Check if single pass detail mode is enabled and if can use it
		if (SinglePassDetail) {
			//Only attempt single pass detail if single texture rendering wasn't forced earlier
			if (!m_rpForceSingle) {
				//Single pass detail must be done with one or two normal passes
				if ((m_rpPassCount == 1) || (m_rpPassCount == 2)) {
					singlePassDetail = true;
				}
			}
		}

		if (singlePassDetail) {
			RenderPasses_SingleOrDualTextureAndDetailTexture(*Surface.DetailTexture);

			if (UseCVA) {
				//Unlock arrays
				glUnlockArraysEXT();
			}
		}
		else {
			RenderPasses();

			if (UseCVA) {
				//Unlock arrays
				glUnlockArraysEXT();
			}

			bool clipDetailTexture = (DetailClipping != 0);

			if (m_rpMasked) {
				//Cannot use detail texture clipping with masked mode
				//It will not work with glDepthFunc(GL_EQUAL)
				clipDetailTexture = false;

				if (m_rpSetDepthEqual == false) {
					glDepthFunc(GL_EQUAL);
					m_rpSetDepthEqual = true;
				}
			}

			//This function should only be called if at least one polygon will be detail textured
			if (UseFragmentProgram) {
				DrawDetailTexture_FP(*Surface.DetailTexture);
			}
			else if (UseVertexProgram) {
				DrawDetailTexture_VP(*Surface.DetailTexture);
			}
			else {
				DrawDetailTexture(*Surface.DetailTexture, Index, clipDetailTexture);
			}
		}
	}
	else {
		RenderPasses();

		if (UseCVA) {
			//Unlock arrays
			glUnlockArraysEXT();
		}
	}

	// UnrealEd selection.
	if (GIsEditor && (PolyFlags & (PF_Selected | PF_FlatShaded))) {
		//No need to set default AA state here as it is always set on entry to DrawComplexSurface
		//No need to set default projection state here as it is always set on entry to DrawComplexSurface
		//No need to set default color state here as it is always set on entry to DrawComplexSurface
		SetDefaultVertexProgramState();
		SetDefaultFragmentProgramState();
		SetDefaultTextureState();

		SetNoTexture(0);
		SetBlend(PF_Highlighted);
		if (PolyFlags & PF_FlatShaded) {
			glColor4f(Surface.FlatColor.R / 255.0f, Surface.FlatColor.G / 255.0f, Surface.FlatColor.B / 255.0f, 1.0f);
		}
		else {
			glColor4f(0.0f, 0.0f, 0.5f, 0.5f);
		}
		for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			glBegin(GL_TRIANGLE_FAN);
			for (INT i = 0; i < Poly->NumPts; i++) {
				glVertex3fv(&Poly->Pts[i]->Point.X);
			}
			glEnd();
		}
	}

	if (m_rpSetDepthEqual == true) {
		glDepthFunc((GLenum)ZTrickFunc);
	}

	unclock(ComplexCycles);
	unguard;
}

#ifdef UTGLR_RUNE_BUILD
void UOpenGLRenderDevice::PreDrawFogSurface() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: PreDrawFogSurface = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::PreDrawFogSurface);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultVertexProgramState();
	SetDefaultFragmentProgramState();
	SetDefaultTextureState();

	SetBlend(PF_AlphaBlend);
//	glDisable(GL_TEXTURE_2D);
	SetNoTexture(0);

	unguard;
}

void UOpenGLRenderDevice::PostDrawFogSurface() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: PostDrawFogSurface = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::PostDrawFogSurface);

//	glEnable(GL_TEXTURE_2D);
	SetBlend(0);

	unguard;
}

void UOpenGLRenderDevice::DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: DrawFogSurface = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::DrawFogSurface);

	FPlane Modulate(FogSurf.FogColor.X, FogSurf.FogColor.Y, FogSurf.FogColor.Z, 0.0f);

	FLOAT RFogDistance = 1.0f / FogSurf.FogDistance;

	if (FogSurf.PolyFlags & PF_Masked) {
		glDepthFunc(GL_EQUAL);
	}

	for (FSavedPoly* Poly = FogSurf.Polys; Poly; Poly = Poly->Next) {
		glBegin(GL_TRIANGLE_FAN);
		for (INT i = 0; i < Poly->NumPts; i++) {
			Modulate.W = Poly->Pts[i]->Point.Z * RFogDistance;
			if (Modulate.W > 1.0f) {
				Modulate.W = 1.0f;
			}
			else if (Modulate.W < 0.0f) {
				Modulate.W = 0.0f;
			}
			glColor4fv(&Modulate.X);
			glVertex3fv(&Poly->Pts[i]->Point.X);
		}
		glEnd();
	}

	if (FogSurf.PolyFlags & PF_Masked) {
		glDepthFunc(ZTrickFunc);
	}

	unguard;
}

void UOpenGLRenderDevice::PreDrawGouraud(FSceneNode* Frame, FLOAT FogDistance, FPlane FogColor) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: PreDrawGouraud = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::PreDrawGouraud);

	if (FogDistance > 0.0f) {
		EndBuffering();

		//Enable fog
		m_gpFogEnabled = true;
		glEnable(GL_FOG);

//		glFogi(GL_FOG_MODE, GL_LINEAR);
		glFogfv(GL_FOG_COLOR, &FogColor.X);
//		glFogf(GL_FOG_START, 0.0f);
		glFogf(GL_FOG_END, FogDistance);
	}

	unguard;
}

void UOpenGLRenderDevice::PostDrawGouraud(FLOAT FogDistance) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: PostDrawGouraud = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::PostDrawGouraud);

	if (FogDistance > 0.0f) {
		EndBuffering();

		//Disable fog
		m_gpFogEnabled = false;
		glDisable(GL_FOG);
	}

	unguard;
}
#endif

void UOpenGLRenderDevice::DrawGouraudPolygonOld(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: DrawGouraudPolygonOld = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::DrawGouraudPolygonOld);
	clock(GouraudCycles);

	//Decide if should request near Z range hack projection
	bool requestNearZRangeHackProjection = false;
	if (m_useZRangeHack && (GUglyHackFlags & 0x1)) {
		requestNearZRangeHackProjection = true;
	}
	//Set projection state
	SetProjectionState(requestNearZRangeHackProjection);

	SetBlend(PolyFlags);
	SetTextureNoPanBias(0, Info, PolyFlags);

#ifdef UTGLR_RUNE_BUILD
	BYTE alpha = 255;
	if (PolyFlags & PF_AlphaBlend) {
		alpha = appRound(Info.Texture->Alpha * 255.0f);
	}
#endif
	if (PolyFlags & PF_Modulated) {
		glColor3f(1.0f, 1.0f, 1.0f);
		m_requestedColorFlags = 0;
	}
	else {
		m_requestedColorFlags = CF_COLOR_ARRAY;
	}

	//Set color state
	SetColorState();

	INT Index = 0;
	for (INT i = 0; i < NumPts; i++) {
		FTransTexture* P = Pts[i];
		FGLTexCoord &destTexCoord = TexCoordArray[0][Index];
		destTexCoord.u = P->U * TexInfo[0].UMult;
		destTexCoord.v = P->V * TexInfo[0].VMult;
		if (m_requestedColorFlags & CF_COLOR_ARRAY) {
#ifdef UTGLR_RUNE_BUILD
			SingleColorArray[Index].color = FPlaneTo_RGB_Aub(&P->Light, alpha);
#else
			SingleColorArray[Index].color = FPlaneTo_RGB_A255(&P->Light);
#endif
		}
		FGLVertex &destVertex = VertexArray[Index];
		destVertex.x = P->Point.X;
		destVertex.y = P->Point.Y;
		destVertex.z = P->Point.Z;
		Index++;
	}

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

	glDrawArrays(GL_TRIANGLE_FAN, 0, Index);

#ifdef UTGLR_RUNE_BUILD
	if ((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) {
#else
	if ((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) {
#endif
		//Additional pass fogging uses the color array
		m_requestedColorFlags = CF_COLOR_ARRAY;

		//Set color state
		SetColorState();

		SetNoTexture(0);
		SetBlend(PF_Highlighted);
		Index = 0;
		for (INT i = 0; i < NumPts; i++) {
			const FTransTexture* P = Pts[i];
			SingleColorArray[Index].color = FPlaneTo_RGBA(&P->Fog);
			Index++;
		}

		glDrawArrays(GL_TRIANGLE_FAN, 0, Index);
	}

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	unclock(GouraudCycles);
	unguard;
}

void UOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: DrawGouraudPolygon = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::DrawGouraudPolygon);

	//DrawGouraudPolygon only uses PolyFlags2 locally
	DWORD PolyFlags2 = 0;

	EndTileBuffering();

	if (SceneNodeHack) {
		if ((Frame->X != m_sceneNodeX) || (Frame->Y != m_sceneNodeY)) {
			m_sceneNodeHackCount++;
			SetSceneNode(Frame);
		}
	}

	//Reject invalid polygons early so that other parts of the code do not have to deal with them
	if (NumPts < 3) {
		return;
	}

	if (NumPts > m_bufferActorTrisCutoff) {
		EndBuffering();

		SetDefaultAAState();
		//No need to set default projection state here as DrawGouraudPolygonOld will set its own projection state
		//No need to set default color state here as DrawGouraudPolygonOld will set its own color state
#ifdef UTGLR_RUNE_BUILD
		if (UseVertexProgram && m_gpFogEnabled) {
			SetVertexProgram(m_vpDefaultRenderingStateWithLinearFog);
		}
		else {
			SetDefaultVertexProgramState();
		}
		if (UseFragmentProgram && m_gpFogEnabled) {
			SetFragmentProgram(m_fpDefaultRenderingStateWithLinearFog);
		}
		else {
			SetDefaultFragmentProgramState();
		}
#else
		SetDefaultVertexProgramState();
		SetDefaultFragmentProgramState();
#endif
		SetDefaultTextureState();

		DrawGouraudPolygonOld(Frame, Info, Pts, NumPts, PolyFlags, Span);

		return;
	}

	//Load texture cache id
	QWORD CacheID = Info.CacheID;

	//Only attempt to alter texture cache id on certain textures
	if ((CacheID & 0xFF) == 0xE0) {
		//Alter texture cache id if masked texture hack is enabled and texture is masked
		CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

		//Check for 16 bit texture option
		if (Use16BitTextures) {
			if (Info.Palette && (Info.Palette[128].A == 255)) {
				CacheID |= TEX_CACHE_ID_FLAG_16BIT;
			}
		}
	}

	//Decide if should request near Z range hack projection
	if (m_useZRangeHack && (GUglyHackFlags & 0x1)) {
		PolyFlags2 |= PF2_NEAR_Z_RANGE_HACK;
	}

	if ((m_curPolyFlags != PolyFlags) ||
		(m_curPolyFlags2 != PolyFlags2) ||
		(TexInfo[0].CurrentCacheID != CacheID) ||
		((BufferedVerts + NumPts) >= (VERTEX_ARRAY_SIZE - 14)) ||
		(BufferedVerts == 0))
	// flush drawing and set the state!
	{
		EndGouraudPolygonBuffering(); //Flush the vertex array

		//Update current poly flags
		m_curPolyFlags = PolyFlags;
		m_curPolyFlags2 = PolyFlags2;

		//Set request near Z range hack projection flag
		m_requestNearZRangeHackProjection = (PolyFlags2 & PF2_NEAR_Z_RANGE_HACK) ? true : false;

		//Set default texture state
		SetDefaultTextureState();

		SetBlend(PolyFlags);
		SetTextureNoPanBias(0, Info, PolyFlags);

		if (PolyFlags & PF_Modulated) {
			glColor3f(1.0f, 1.0f, 1.0f);
			m_requestedColorFlags = 0;
		}
		else {
			m_requestedColorFlags = CF_COLOR_ARRAY;

#ifdef UTGLR_RUNE_BUILD
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) {
#else
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) && UseVertexSpecular) {
#endif
				m_requestedColorFlags = CF_COLOR_ARRAY | CF_DUAL_COLOR_ARRAY | CF_COLOR_SUM;
			}
		}

#ifdef UTGLR_RUNE_BUILD
		if (UseVertexProgram && m_gpFogEnabled) {
			SetVertexProgram(m_vpDefaultRenderingStateWithLinearFog);
		}
		else {
			SetDefaultVertexProgramState();
		}
		if (UseFragmentProgram && m_gpFogEnabled) {
			SetFragmentProgram(m_fpDefaultRenderingStateWithLinearFog);
		}
		else {
			SetDefaultFragmentProgramState();
		}
#else
		//May need to set a fog vertex program if vertex program mode is enabled
		if (UseVertexProgram && (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY)) {
			SetVertexProgram(m_vpDefaultRenderingStateWithFog);
		}
		else {
			SetDefaultVertexProgramState();
		}
		//May need to set a fog fragment program if fragment program mode is enabled
		if (UseFragmentProgram && (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY)) {
			//Leave color sum off if using fragment program
			m_requestedColorFlags &= ~CF_COLOR_SUM;
			SetFragmentProgram(m_fpDefaultRenderingStateWithFog);
		}
		else {
			SetDefaultFragmentProgramState();
		}
#endif

		//Select a buffer verts proc
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			m_pBuffer3VertsProc = m_pBuffer3FoggedVertsProc;
		}
		else if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			m_pBuffer3VertsProc = m_pBuffer3ColoredVertsProc;
		}
		else {
			m_pBuffer3VertsProc = m_pBuffer3BasicVertsProc;
		}
#ifdef UTGLR_RUNE_BUILD
		m_gpAlpha = 255;
		if (PolyFlags & PF_AlphaBlend) {
			m_gpAlpha = appRound(Info.Texture->Alpha * 255.0f);
			m_pBuffer3VertsProc = Buffer3Verts;
		}
#endif
	}

	//Buffer 3 vertices from the first (and perhaps only) triangle
	(m_pBuffer3VertsProc)(this, Pts);

	if (NumPts > 3) {
		//Buffer additional vertices from a clipped triangle
		BufferAdditionalClippedVerts(Pts, NumPts);
	}

	unguard;
}

void UOpenGLRenderDevice::DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: DrawTile = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::DrawTile);

	//DrawTile does not use PolyFlags2
	const DWORD PolyFlags2 = 0;

	EndGouraudPolygonBuffering();

	if (SceneNodeHack) {
		if ((Frame->X != m_sceneNodeX) || (Frame->Y != m_sceneNodeY)) {
			m_sceneNodeHackCount++;
			SetSceneNode(Frame);
		}
	}

	//Adjust Z coordinate if Z range hack is active
	if (m_useZRangeHack) {
		if ((Z >= 0.5f) && (Z < 8.0f)) {
			Z = (((Z - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}
	}

	FLOAT PX1 = X - Frame->FX2;
	FLOAT PX2 = PX1 + XL;
	FLOAT PY1 = Y - Frame->FY2;
	FLOAT PY2 = PY1 + YL;

	FLOAT RPX1 = m_RFX2 * PX1;
	FLOAT RPX2 = m_RFX2 * PX2;
	FLOAT RPY1 = m_RFY2 * PY1;
	FLOAT RPY2 = m_RFY2 * PY2;
	if (!Frame->Viewport->IsOrtho()) {
		RPX1 *= Z;
		RPX2 *= Z;
		RPY1 *= Z;
		RPY2 *= Z;
	}

	if (BufferTileQuads) {
		//Load texture cache id
		QWORD CacheID = Info.CacheID;

		//Only attempt to alter texture cache id on certain textures
		if ((CacheID & 0xFF) == 0xE0) {
			//Alter texture cache id if masked texture hack is enabled and texture is masked
			CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

			//Check for 16 bit texture option
			if (Use16BitTextures) {
				if (Info.Palette && (Info.Palette[128].A == 255)) {
					CacheID |= TEX_CACHE_ID_FLAG_16BIT;
				}
			}
		}

		//Check if need to start new tile buffering
		if ((m_curPolyFlags != PolyFlags) ||
			(m_curPolyFlags2 != PolyFlags2) ||
			(TexInfo[0].CurrentCacheID != CacheID) ||
			(BufferedTileVerts >= (VERTEX_ARRAY_SIZE - 4)) ||
			(BufferedTileVerts == 0))
		{
			//Flush any previously buffered tiles
			EndTileBuffering();

			SetDefaultVertexProgramState();
			SetDefaultFragmentProgramState();

			//Update current poly flags (before possible local modification)
			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

#ifdef UTGLR_RUNE_BUILD
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#else
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent)) {
#endif
				PolyFlags |= PF_Highlighted | PF_Occlude;
			}

			//Set default texture state
			SetDefaultTextureState();

			SetBlend(PolyFlags);
			SetTextureNoPanBias(0, Info, PolyFlags);

			if (PolyFlags & PF_Modulated) {
				glColor3f(1.0f, 1.0f, 1.0f);
				m_requestedColorFlags = 0;
			}
			else {
				m_requestedColorFlags = CF_COLOR_ARRAY;
			}
		}

		//Buffer the tile
		FGLTexCoord *pTexCoordArray = &TexCoordArray[0][BufferedTileVerts];
		FGLVertex *pVertexArray = &VertexArray[BufferedTileVerts];

		FLOAT TexInfoUMult = TexInfo[0].UMult;
		FLOAT TexInfoVMult = TexInfo[0].VMult;

		FLOAT SU1 = (U) * TexInfoUMult;
		FLOAT SU2 = (U + UL) * TexInfoUMult;
		FLOAT SV1 = (V) * TexInfoVMult;
		FLOAT SV2 = (V + VL) * TexInfoVMult;

		pTexCoordArray[0].u = SU1;
		pTexCoordArray[0].v = SV1;

		pTexCoordArray[1].u = SU2;
		pTexCoordArray[1].v = SV1;

		pTexCoordArray[2].u = SU2;
		pTexCoordArray[2].v = SV2;

		pTexCoordArray[3].u = SU1;
		pTexCoordArray[3].v = SV2;

		pVertexArray[0].x = RPX1;
		pVertexArray[0].y = RPY1;
		pVertexArray[0].z = Z;

		pVertexArray[1].x = RPX2;
		pVertexArray[1].y = RPY1;
		pVertexArray[1].z = Z;

		pVertexArray[2].x = RPX2;
		pVertexArray[2].y = RPY2;
		pVertexArray[2].z = Z;

		pVertexArray[3].x = RPX1;
		pVertexArray[3].y = RPY2;
		pVertexArray[3].z = Z;

		//Optionally buffer color information
		if (!(PolyFlags & PF_Modulated)) {
			if (UseSSE2) {
#ifdef UTGLR_INCLUDE_SSE_CODE
				FGLSingleColor *pSingleColorArray = &SingleColorArray[BufferedTileVerts];
				static __m128 fColorMul = { 255.0f, 255.0f, 255.0f, 0.0f };
				__m128 fColorMulReg;
				__m128 fColor;
				__m128 fAlpha;
				__m128i iColor;

				fColorMulReg = fColorMul;
				fColor = _mm_loadu_ps(&Color.X);
				fColor = _mm_mul_ps(fColor, fColorMulReg);

				fAlpha = _mm_setzero_ps();
				fAlpha = _mm_move_ss(fAlpha, fColorMulReg);
#ifdef UTGLR_RUNE_BUILD
				if (PolyFlags & PF_AlphaBlend) {
					fAlpha = _mm_mul_ss(fAlpha, _mm_load_ss(&Info.Texture->Alpha));
				}
#endif
				fAlpha = _mm_shuffle_ps(fAlpha, fAlpha,  _MM_SHUFFLE(0, 1, 1, 1));

				fColor = _mm_or_ps(fColor, fAlpha);

				iColor = _mm_cvtps_epi32(fColor);
				iColor = _mm_packs_epi32(iColor, iColor);
				iColor = _mm_packus_epi16(iColor, iColor);

				//Always buffer 4 colors and color array starts out 16B aligned
				_mm_store_si128((__m128i *)pSingleColorArray, iColor);
#endif
			}
			else {
				DWORD dwColor;
				FGLSingleColor *pSingleColorArray = &SingleColorArray[BufferedTileVerts];

#ifdef UTGLR_RUNE_BUILD
				if (PolyFlags & PF_AlphaBlend) {
					Color.W = Info.Texture->Alpha;
					dwColor = FPlaneTo_RGBAClamped(&Color);
				}
				else {
					dwColor = FPlaneTo_RGBClamped_A255(&Color);
				}
#else
				dwColor = FPlaneTo_RGBClamped_A255(&Color);
#endif

				pSingleColorArray[0].color = dwColor;
				pSingleColorArray[1].color = dwColor;
				pSingleColorArray[2].color = dwColor;
				pSingleColorArray[3].color = dwColor;
			}
		}

		BufferedTileVerts += 4;
	}
	else {
		EndTileBuffering();

		clock(TileCycles);

		if (NoAATiles) {
			SetDisabledAAState();
		}
		else {
			SetDefaultAAState();
		}
		SetDefaultProjectionState();
		SetDefaultColorState();
		SetDefaultVertexProgramState();
		SetDefaultFragmentProgramState();
		SetDefaultTextureState();

#ifdef UTGLR_RUNE_BUILD
		if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#else
		if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & PF_Translucent)) {
#endif
			PolyFlags |= PF_Highlighted | PF_Occlude;
		}

		SetBlend(PolyFlags);
		SetTextureNoPanBias(0, Info, PolyFlags);

		if (PolyFlags & PF_Modulated) {
			glColor3f(1.0f, 1.0f, 1.0f);
		}
		else {
#ifdef UTGLR_RUNE_BUILD
			Color.W = 1.0f;
			if (PolyFlags & PF_AlphaBlend) {
				Color.W = Info.Texture->Alpha;
			}
			glColor4fv(&Color.X);
#else
			glColor3fv(&Color.X);
#endif
		}

		FLOAT TexInfoUMult = TexInfo[0].UMult;
		FLOAT TexInfoVMult = TexInfo[0].VMult;

		FLOAT SU1 = (U) * TexInfoUMult;
		FLOAT SU2 = (U + UL) * TexInfoUMult;
		FLOAT SV1 = (V) * TexInfoVMult;
		FLOAT SV2 = (V + VL) * TexInfoVMult;

		glBegin(GL_TRIANGLE_FAN);

		glTexCoord2f(SU1, SV1);
		glVertex3f(RPX1, RPY1, Z);

		glTexCoord2f(SU2, SV1);
		glVertex3f(RPX2, RPY1, Z);

		glTexCoord2f(SU2, SV2);
		glVertex3f(RPX2, RPY2, Z);

		glTexCoord2f(SU1, SV2);
		glVertex3f(RPX1, RPY2, Z);

		glEnd();

		unclock(TileCycles);
	}

	unguard;
}

void UOpenGLRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: Draw2DLine = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::Draw2DLine);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultVertexProgramState();
	SetDefaultFragmentProgramState();
	SetDefaultTextureState();

	SetNoTexture(0);
	SetBlend(PF_Highlighted | PF_Occlude);
	glColor3fv(&Color.X);

	FLOAT X1Pos = m_RFX2 * (P1.X - Frame->FX2);
	FLOAT Y1Pos = m_RFY2 * (P1.Y - Frame->FY2);
	FLOAT X2Pos = m_RFX2 * (P2.X - Frame->FX2);
	FLOAT Y2Pos = m_RFY2 * (P2.Y - Frame->FY2);
	if (!Frame->Viewport->IsOrtho()) {
		X1Pos *= P1.Z;
		Y1Pos *= P1.Z;
		X2Pos *= P2.Z;
		Y2Pos *= P2.Z;
	}
	glBegin(GL_LINES);
	glVertex3f(X1Pos, Y1Pos, P1.Z);
	glVertex3f(X2Pos, Y2Pos, P2.Z);
	glEnd();

	unguard;
}

void UOpenGLRenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: Draw3DLine = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::Draw3DLine);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultVertexProgramState();
	SetDefaultFragmentProgramState();
	SetDefaultTextureState();

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho()) {
		// Zoom.
		P1.X = (P1.X) / Frame->Zoom + Frame->FX2;
		P1.Y = (P1.Y) / Frame->Zoom + Frame->FY2;
		P2.X = (P2.X) / Frame->Zoom + Frame->FX2;
		P2.Y = (P2.Y) / Frame->Zoom + Frame->FY2;
		P1.Z = P2.Z = 1;

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2) {
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else if (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL) {
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1, P1.Y - 1, P1.X + 1, P1.Y + 1, P1.Z);
		}
	}
	else {
		SetNoTexture(0);
		SetBlend(PF_Highlighted | PF_Occlude);
		glColor3fv(&Color.X);

		glBegin(GL_LINES);
		glVertex3fv(&P1.X);
		glVertex3fv(&P2.X);
		glEnd();
	}
	unguard;
}

void UOpenGLRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: Draw2DPoint = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::Draw2DPoint);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultVertexProgramState();
	SetDefaultFragmentProgramState();
	SetDefaultTextureState();

	SetBlend(PF_Highlighted | PF_Occlude);
	SetNoTexture(0);
	glColor3fv(&Color.X); // vogel: was 4 - ONLY FOR UT!

	FLOAT X1Pos = m_RFX2 * (X1 - Frame->FX2);
	FLOAT Y1Pos = m_RFY2 * (Y1 - Frame->FY2);
	FLOAT X2Pos = m_RFX2 * (X2 - Frame->FX2);
	FLOAT Y2Pos = m_RFY2 * (Y2 - Frame->FY2);
	if (!Frame->Viewport->IsOrtho()) {
		X1Pos *= Z;
		Y1Pos *= Z;
		X2Pos *= Z;
		Y2Pos *= Z;
	}
	glBegin(GL_TRIANGLE_FAN);
	glVertex3f(X1Pos, Y1Pos, Z);
	glVertex3f(X2Pos, Y1Pos, Z);
	glVertex3f(X2Pos, Y2Pos, Z);
	glVertex3f(X1Pos, Y2Pos, Z);
	glEnd();

	unguard;
}


void UOpenGLRenderDevice::ClearZ(FSceneNode* Frame) {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: ClearZ = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::ClearZ);

	EndBuffering();

	//Default AA state not required for glClear
	//Default projection state not required for glClear
	//Default color state not required for glClear
	//Default vertex program state not required for glClear
	//Default fragment program state not required for glClear
	//Default texture state not required for glClear

	SetBlend(PF_Occlude);
	glClear(GL_DEPTH_BUFFER_BIT);

	unguard;
}

void UOpenGLRenderDevice::PushHit(const BYTE* Data, INT Count) {
	guard(UOpenGLRenderDevice::PushHit);

	EndBuffering();

	glPushName(Count);
	for (INT i = 0; i < Count; i += 4) {
		glPushName(*(INT*)(Data + i));
	}

	unguard;
}

void UOpenGLRenderDevice::PopHit(INT Count, UBOOL bForce) {
	guard(UOpenGLRenderDevice::PopHit);

	EndBuffering();

	glPopName();
	for (INT i = 0; i < Count; i += 4) {
		glPopName();
	}
	//!!implement bforce

	unguard;
}

void UOpenGLRenderDevice::GetStats(TCHAR* Result) {
	guard(UOpenGLRenderDevice::GetStats);

	double msPerCycle = GSecondsPerCycle * 1000.0f;
	appSprintf(
		Result,
		TEXT("OpenGL stats: Bind=%04.1f Image=%04.1f Complex=%04.1f Gouraud=%04.1f Tile=%04.1f"),
		msPerCycle * BindCycles,
		msPerCycle * ImageCycles,
		msPerCycle * ComplexCycles,
		msPerCycle * GouraudCycles,
		msPerCycle * TileCycles
	);

	unguard;
}

void UOpenGLRenderDevice::ReadPixels(FColor* Pixels) {
	guard(UOpenGLRenderDevice::ReadPixels);

	INT x, y;
	INT SizeX, SizeY;

	SizeX = Viewport->SizeX;
	SizeY = Viewport->SizeY;

	glReadPixels(0, 0, SizeX, SizeY, GL_RGBA, GL_UNSIGNED_BYTE, Pixels);
	for (y = 0; y < SizeY / 2; y++) {
		for (x = 0; x < SizeX; x++) {
			Exchange(Pixels[x + y * SizeX].R, Pixels[x + (SizeY - 1 - y) * SizeX].B);
			Exchange(Pixels[x + y * SizeX].G, Pixels[x + (SizeY - 1 - y) * SizeX].G);
			Exchange(Pixels[x + y * SizeX].B, Pixels[x + (SizeY - 1 - y) * SizeX].R);
		}
	}

	//Gamma correct screenshots if the option is true and the gamma ramp was set successfully
	if (GammaCorrectScreenshots && m_setGammaRampSucceeded) {
		FByteGammaRamp gammaByteRamp;
		BuildGammaRamp(SavedGammaCorrection, SavedGammaCorrection, SavedGammaCorrection, Brightness, gammaByteRamp);
		for (y = 0; y < SizeY; y++) {
			for (x = 0; x < SizeX; x++) {
				Pixels[x + y * SizeX].R = gammaByteRamp.red[Pixels[x + y * SizeX].R];
				Pixels[x + y * SizeX].G = gammaByteRamp.green[Pixels[x + y * SizeX].G];
				Pixels[x + y * SizeX].B = gammaByteRamp.blue[Pixels[x + y * SizeX].B];
			}
		}
	}

	unguard;
}

void UOpenGLRenderDevice::EndFlash() {
#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
{
	static int si;
	dout << L"utglr: EndFlash = " << si++ << std::endl;
}
#endif
	guard(UOpenGLRenderDevice::EndFlash);
	if (FlashScale != FPlane(.5,.5,.5,0) || FlashFog != FPlane(0,0,0,0)) {
		EndBuffering();

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultColorState();
		SetDefaultVertexProgramState();
		SetDefaultFragmentProgramState();
		SetDefaultTextureState();

		SetBlend(PF_Highlighted);
		SetNoTexture(0);

		glColor4f(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0 - Min(FlashScale.X * 2.0f, 1.0f));

		FLOAT RFX2 = m_RProjZ;
		FLOAT RFY2 = m_RProjZ * m_Aspect;

		//Adjust Z coordinate if Z range hack is active
		FLOAT ZCoord = 1.0f;
		if (m_useZRangeHack) {
			ZCoord = (((ZCoord - 0.5f) / 7.5f) * 4.0f) + 4.0f;
		}

		glBegin(GL_TRIANGLE_FAN);
		glVertex3f(RFX2 * (-1.0f * ZCoord), RFY2 * (-1.0f * ZCoord), ZCoord);
		glVertex3f(RFX2 * (+1.0f * ZCoord), RFY2 * (-1.0f * ZCoord), ZCoord);
		glVertex3f(RFX2 * (+1.0f * ZCoord), RFY2 * (+1.0f * ZCoord), ZCoord);
		glVertex3f(RFX2 * (-1.0f * ZCoord), RFY2 * (+1.0f * ZCoord), ZCoord);
		glEnd();
	}
	unguard;
}

void UOpenGLRenderDevice::PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags) {
	guard(UOpenGLRenderDevice::PrecacheTexture);
	SetTextureNoPanBias(0, Info, PolyFlags);
	unguard;
}


//This function is safe to call multiple times to initialize once
void UOpenGLRenderDevice::InitNoTextureSafe(void) {
	guard(UOpenGLRenderDevice::InitNoTexture);
	unsigned int u;
	DWORD Data[4*4];

	//Return early if already initialized
	if (m_noTextureId != 0) {
		return;
	}

	for (u = 0; u < (4*4); u++) {
		Data[u] = 0xFFFFFFFF;
	}

	glGenTextures(1, &m_noTextureId);

	SetNoTexture(0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, Data);

	return;
	unguard;
}

//This function is safe to call multiple times to initialize once
void UOpenGLRenderDevice::InitAlphaTextureSafe(void) {
	guard(UOpenGLRenderDevice::InitAlphaTexture);
	unsigned int u;
	BYTE AlphaData[256];

	//Return early if already initialized
	if (m_alphaTextureId != 0) {
		return;
	}

	for (u = 0; u < 256; u++) {
		AlphaData[u] = 255 - u;
	}

	glGenTextures(1, &m_alphaTextureId);

	SetAlphaTexture(0);

	// vogel: could use 1D texture but opted against (for minor reasons)
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 256, 1, 0, GL_ALPHA, GL_UNSIGNED_BYTE, AlphaData);

	return;
	unguard;
}

void UOpenGLRenderDevice::ScanForOldTextures(void) {
	guard(UOpenGLRenderDevice::ScanForOldTextures);

	unsigned int u;
	FCachedTexture *pCT;

	//Prevent currently bound textures from being recycled
	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if (pBind != NULL) {
			//Update last used frame count so that the texture will not be recycled
			pBind->LastUsedFrameCount = m_currentFrameCount;

			//Move node to tail of linked list if in LRU list
			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				m_nonZeroPrefixBindChain->unlink(pBind);
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}
		}
	}

	pCT = m_nonZeroPrefixBindChain->begin();
	while (pCT != m_nonZeroPrefixBindChain->end()) {
		DWORD numFramesSinceUsed = m_currentFrameCount - pCT->LastUsedFrameCount;
		if (numFramesSinceUsed > DynamicTexIdRecycleLevel) {
			//See if the tex pool is not enabled or the internal format is not RGBA8
			if (!UseTexPool || (pCT->texInternalFormat != GL_RGBA8)) {
				//Remove node from linked list
				m_nonZeroPrefixBindChain->unlink(pCT);

				//Get pointer to node in bind map
				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (DWORD)&(((QWORD_CTTree_t::node_t *)0)->data));
				//Extract tree index
				BYTE treeIndex = pCT->treeIndex;
				//Advanced cached texture pointer to next entry in linked list
				pCT = pCT->pNext;

				//Remove node from bind map
				m_nonZeroPrefixBindTrees[treeIndex].remove(pNode);

				//Add node plus texture id to texture id pool
				m_nonZeroPrefixTexIdPool->add(pNode);

				continue;
			}
			else {
				TexPoolMap_t::node_t *texPoolPtr;

#if 0
{
	static int si;
	dout << L"utglr: TexPool free = " << si++ << L", Id = " << pCT->Id
		<< L", u = " << pCT->UBits << L", v = " << pCT->VBits << std::endl;
}
#endif

				//Remove node from linked list
				m_nonZeroPrefixBindChain->unlink(pCT);

				//Create a key from the lg2 width and height of the texture object
				TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pCT->UBits, pCT->VBits);

				//Get pointer to node in bind map
				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (DWORD)&(((QWORD_CTTree_t::node_t *)0)->data));
				//Extract tree index
				BYTE treeIndex = pCT->treeIndex;
				//Advanced cached texture pointer to next entry in linked list
				pCT = pCT->pNext;

				//Remove node from bind map
				m_nonZeroPrefixBindTrees[treeIndex].remove(pNode);

				//See if the key does not yet exist
				texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
				//If the key does not yet exist, add an empty vector in its place
				if (texPoolPtr == 0) {
					texPoolPtr = m_TexPoolMap_Allocator.alloc_node();
					texPoolPtr->key = texPoolKey;
					texPoolPtr->data = QWORD_CTTree_NodePool_t();
					m_RGBA8TexPool->insert(texPoolPtr);
				}

				//Add node plus texture id to a list in the tex pool based on its dimensions
				texPoolPtr->data.add(pNode);

				continue;
			}
		}

		//The list is sorted
		//Stop searching on first one not to be recycled
		break;

		pCT = pCT->pNext;
	}

	unguard;
}

void UOpenGLRenderDevice::SetNoTextureNoCheck(INT Multi) {
	guard(UOpenGLRenderDevice::SetNoTexture);

	// Set small white texture.
	clock(BindCycles);

	glBindTexture(GL_TEXTURE_2D, m_noTextureId);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_NO_TEX;
	TexInfo[Multi].pBind = NULL;

	unclock(BindCycles);

	unguard;
}

void UOpenGLRenderDevice::SetAlphaTextureNoCheck(INT Multi) {
	guard(UOpenGLRenderDevice::SetAlphaTexture);

	// Set alpha gradient texture.
	clock(BindCycles);

	glBindTexture(GL_TEXTURE_2D, m_alphaTextureId);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_ALPHA_TEX;
	TexInfo[Multi].pBind = NULL;

	unclock(BindCycles);

	unguard;
}

//This function must use Tex.CurrentCacheID and NEVER use Info.CacheID to reference the texture cache id
//This makes it work with the masked texture hack code
void UOpenGLRenderDevice::SetTextureNoCheck(FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags) {
	guard(UOpenGLRenderDevice::SetTexture);

	// Make current.
	clock(BindCycles);

	bool isZeroPrefixCacheID = ((Tex.CurrentCacheID & 0xFFFFFFFF00000000LL) == 0) ? true : false;

	FCachedTexture *pBind = NULL;
	bool existingBind = false;
	bool needTexAllocate = true;

	if (isZeroPrefixCacheID) {
		DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFFLL);

		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
		DWORD_CTTree_t::node_t *bindTreePtr = zeroPrefixBindTree->find(CacheIDSuffix);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
			existingBind = true;
			needTexAllocate = false;
		}
		else {
			DWORD_CTTree_t::node_t *pNewNode;

			//Insert new texture info
			pNewNode = m_DWORD_CTTree_Allocator.alloc_node();
			pNewNode->key = CacheIDSuffix;
			zeroPrefixBindTree->insert(pNewNode);
			pBind = &pNewNode->data;

			//Set bind type
			pBind->bindType = BIND_TYPE_ZERO_PREFIX;

			//Set default tex params
			pBind->texParams = CT_DEFAULT_TEX_PARAMS;
			pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

			//Cache texture info for the new texture
			CacheTextureInfo(pBind, Info, PolyFlags);

			//Allocate a new texture Id
			glGenTextures(1, &pBind->Id);
			AllocatedTextures++;
		}
	}
	else {
		DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFFLL);
		DWORD treeIndex = CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);

		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[treeIndex];
		QWORD_CTTree_t::node_t *bindTreePtr = nonZeroPrefixBindTree->find(Tex.CurrentCacheID);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
			pBind->LastUsedFrameCount = m_currentFrameCount;

			//Check if texture is in LRU list
			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				//Move node to tail of linked list
				m_nonZeroPrefixBindChain->unlink(pBind);
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}

			existingBind = true;
			needTexAllocate = false;
		}
		else {
			QWORD_CTTree_t::node_t *pNewNode;

			//Allocate a new node
			//Use the node pool if it is not empty
			pNewNode = m_nonZeroPrefixNodePool.try_remove();
			if (!pNewNode) {
				pNewNode = m_QWORD_CTTree_Allocator.alloc_node();
			}

			//Insert new texture info
			pNewNode->key = Tex.CurrentCacheID;
			nonZeroPrefixBindTree->insert(pNewNode);
			pBind = &pNewNode->data;
			pBind->LastUsedFrameCount = m_currentFrameCount;

			//Set bind type
			pBind->bindType = BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST;
			if (CacheStaticMaps && ((Tex.CurrentCacheID & 0xFF) == 0x18)) {
				pBind->bindType = BIND_TYPE_NON_ZERO_PREFIX;
			}

			//Save tree index
			pBind->treeIndex = (BYTE)treeIndex;

			//Set default tex params
			pBind->texParams = CT_DEFAULT_TEX_PARAMS;
			pBind->dynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;

			//Check if texture should be in LRU list
			if (pBind->bindType == BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST) {
				//Add node to linked list
				m_nonZeroPrefixBindChain->link_to_tail(pBind);
			}

			//Cache texture info for the new texture
			CacheTextureInfo(pBind, Info, PolyFlags);

			//See if the tex pool is enabled
			bool needTexIdAllocate = true;
			if (UseTexPool) {
				//See if the format will be RGBA8
				if (pBind->texType == TEX_TYPE_NORMAL) {
					TexPoolMap_t::node_t *texPoolPtr;

					//Create a key from the lg2 width and height of the texture object
					TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pBind->UBits, pBind->VBits);

					//Search for the key in the map
					texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
					if (texPoolPtr != 0) {
						QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr;

						//Get a reference to the pool of nodes with tex ids of the right dimension
						QWORD_CTTree_NodePool_t &texPool = texPoolPtr->data;

						//Attempt to get a texture id for the tex pool
						if ((texPoolNodePtr = texPool.try_remove()) != 0) {
							//Use texture id from node in tex pool
							pBind->Id = texPoolNodePtr->data.Id;

							//Use tex params from node in tex pool
							pBind->texParams = texPoolNodePtr->data.texParams;
							pBind->dynamicTexBits = texPoolNodePtr->data.dynamicTexBits;

							//Then add node to free list
							m_nonZeroPrefixNodePool.add(texPoolNodePtr);

#if 0
{
	static int si;
	dout << L"utglr: TexPool retrieve = " << si++ << L", Id = " << pBind->Id
		<< L", u = " << pBind->UBits << L", v = " << pBind->VBits << std::endl;
}
#endif

							//Clear the need tex id allocate flag
							needTexIdAllocate = false;

							//Clear the need tex allocate flag
							needTexAllocate = false;
						}
					}
				}
			}
			if (needTexIdAllocate) {
				QWORD_CTTree_NodePool_t::node_t *nzptipPtr;

				//Allocate a new texture id
				if (UseTexIdPool && ((nzptipPtr = m_nonZeroPrefixTexIdPool->try_remove()) != 0)) {
					//Use texture id from node in tex id pool
					pBind->Id = nzptipPtr->data.Id;

					//Use tex params from node in tex id pool
					pBind->texParams = nzptipPtr->data.texParams;
					pBind->dynamicTexBits = nzptipPtr->data.dynamicTexBits;

					//Then add node to free list
					m_nonZeroPrefixNodePool.add(nzptipPtr);
				}
				else {
					glGenTextures(1, &pBind->Id);
					AllocatedTextures++;
				}
			}
		}
	}

	//Save pointer to current texture bind for current texture unit
	Tex.pBind = pBind;

	glBindTexture(GL_TEXTURE_2D, pBind->Id);

	unclock(BindCycles);

	// Account for all the impact on scale normalization.
	Tex.UMult = pBind->UMult;
	Tex.VMult = pBind->VMult;

	//Check for any changes to dynamic texture object parameters
	{
		BYTE desiredDynamicTexBits;

		desiredDynamicTexBits = (PolyFlags & PF_NoSmooth) ? DT_NO_SMOOTH_BIT : 0;
		if (desiredDynamicTexBits != pBind->dynamicTexBits) {
			BYTE dynamicTexBitsXor;

			dynamicTexBitsXor = desiredDynamicTexBits ^ pBind->dynamicTexBits;

			//Update dynamic tex bits early as there are no subsequent dependencies
			pBind->dynamicTexBits = desiredDynamicTexBits;

			if (dynamicTexBitsXor & DT_NO_SMOOTH_BIT) {
				BYTE desiredTexParamsFirst;
				BYTE texParamsFirstXor;

				//Set partial desired first tex params
				desiredTexParamsFirst = 0;
				if (NoFiltering) {
					desiredTexParamsFirst |= CT_MIN_FILTER_NEAREST;
				}
				else if (PolyFlags & PF_NoSmooth) {
					desiredTexParamsFirst |= ((pBind->texParams.first & CT_HAS_MIPMAPS_BIT) == 0) ? CT_MIN_FILTER_NEAREST : CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST;
				}
				else {
					desiredTexParamsFirst |= ((pBind->texParams.first & CT_HAS_MIPMAPS_BIT) == 0) ? CT_MIN_FILTER_LINEAR : (UseTrilinear ? CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR : CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST);
					desiredTexParamsFirst |= CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT;
					if (MaxAnisotropy) {
						desiredTexParamsFirst |= CT_ANISOTROPIC_FILTER_BIT;
					}
				}

				//Update partial texture state
				texParamsFirstXor = desiredTexParamsFirst ^ pBind->texParams.first;
				if (texParamsFirstXor & CT_MIN_FILTER_MASK) {
					GLint intParam = GL_NEAREST;

					switch (desiredTexParamsFirst & CT_MIN_FILTER_MASK) {
					case CT_MIN_FILTER_NEAREST: intParam = GL_NEAREST; break;
					case CT_MIN_FILTER_LINEAR: intParam = GL_LINEAR; break;
					case CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST: intParam = GL_NEAREST_MIPMAP_NEAREST; break;
					case CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST: intParam = GL_LINEAR_MIPMAP_NEAREST; break;
					case CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR: intParam = GL_NEAREST_MIPMAP_LINEAR; break;
					case CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR: intParam = GL_LINEAR_MIPMAP_LINEAR; break;
					default:
						;
					}

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, intParam);
				}
				if (texParamsFirstXor & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) {
					GLint intParam;

					intParam = (desiredTexParamsFirst & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) ? GL_LINEAR : GL_NEAREST;

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, intParam);
				}
				if (texParamsFirstXor & CT_ANISOTROPIC_FILTER_BIT) {
					GLfloat floatParam;

					floatParam = (desiredTexParamsFirst & CT_ANISOTROPIC_FILTER_BIT) ? (GLfloat)MaxAnisotropy : 1.0f;

					glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, floatParam);
				}

				//Store partial updated texture parameter state in cached texture object
				const BYTE MODIFIED_TEX_PARAMS_FIRST_BITS = CT_MIN_FILTER_MASK | CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT | CT_ANISOTROPIC_FILTER_BIT;
				pBind->texParams.first = (pBind->texParams.first & ~MODIFIED_TEX_PARAMS_FIRST_BITS) | desiredTexParamsFirst;
			}
		}
	}

	// Upload if needed.
	if (!existingBind || Info.bRealtimeChanged) {
		FColor paletteIndex0;

		// Cleanup texture flags.
		if (SupportsLazyTextures) {
			Info.Load();
		}
		Info.bRealtimeChanged = 0;

		//Set palette index 0 to black for masked paletted textures
		if (Info.Palette && (PolyFlags & PF_Masked)) {
			paletteIndex0 = Info.Palette[0];
			Info.Palette[0] = FColor(0,0,0,0);
		}

		// Download the texture.
		clock(ImageCycles);

		if (pBind->texType == TEX_TYPE_PALETTED) {
			glColorTableEXT(GL_TEXTURE_2D, GL_RGBA, 256, GL_RGBA, GL_UNSIGNED_BYTE, Info.Palette);
		}

		// width * height * 4 bytes per pixel
		DWORD memAllocSize = 1 << (pBind->UBits + pBind->VBits + 2);
		if (memAllocSize > LOCAL_TEX_COMPOSE_BUFFER_SIZE) {
			m_texComposeMemMark = FMemMark(GMem);
			m_texConvertCtx.pCompose = New<BYTE>(GMem, memAllocSize);
		}
		else {
			m_texConvertCtx.pCompose = m_localTexComposeBuffer;
		}

		m_texConvertCtx.pBind = pBind;

		UBOOL SkipMipmaps = (Info.NumMips == 1) && !AlwaysMipmap;
		INT MaxLevel = pBind->MaxLevel;

		//Only update texture state for new textures
		if (!existingBind) {
			tex_params_t desiredTexParams;
			BYTE texParamsFirstXor;

			//Set desired first tex params
			desiredTexParams.first = 0;
			if (NoFiltering) {
				desiredTexParams.first |= CT_MIN_FILTER_NEAREST;
			}
			else if (PolyFlags & PF_NoSmooth) {
				desiredTexParams.first |= SkipMipmaps ? CT_MIN_FILTER_NEAREST : CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST;
			}
			else {
				desiredTexParams.first |= SkipMipmaps ? CT_MIN_FILTER_LINEAR : (UseTrilinear ? CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR : CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST);
				desiredTexParams.first |= CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT;
				if (MaxAnisotropy) {
					desiredTexParams.first |= CT_ANISOTROPIC_FILTER_BIT;
				}
			}

			if (!SkipMipmaps) {
				if (AutoGenerateMipmaps && (pBind->texType == TEX_TYPE_NORMAL)) {
					desiredTexParams.first |= CT_GENERATE_MIPMAPS_BIT;
				}

				desiredTexParams.first |= CT_HAS_MIPMAPS_BIT;
			}

			//Set desired max level tex param
			desiredTexParams.maxLevel = CT_DEFAULT_TEXTURE_MAX_LEVEL;
			if (!UseTNT && !SkipMipmaps) {
				desiredTexParams.maxLevel = MaxLevel;
			}


			//Set texture state
			texParamsFirstXor = desiredTexParams.first ^ pBind->texParams.first;
			if (texParamsFirstXor & CT_MIN_FILTER_MASK) {
				GLint intParam = GL_NEAREST;

				switch (desiredTexParams.first & CT_MIN_FILTER_MASK) {
				case CT_MIN_FILTER_NEAREST: intParam = GL_NEAREST; break;
				case CT_MIN_FILTER_LINEAR: intParam = GL_LINEAR; break;
				case CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST: intParam = GL_NEAREST_MIPMAP_NEAREST; break;
				case CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST: intParam = GL_LINEAR_MIPMAP_NEAREST; break;
				case CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR: intParam = GL_NEAREST_MIPMAP_LINEAR; break;
				case CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR: intParam = GL_LINEAR_MIPMAP_LINEAR; break;
				default:
					;
				}

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, intParam);
			}
			if (texParamsFirstXor & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) {
				GLint intParam;

				intParam = (desiredTexParams.first & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) ? GL_LINEAR : GL_NEAREST;

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, intParam);
			}
			if (texParamsFirstXor & CT_ANISOTROPIC_FILTER_BIT) {
				GLfloat floatParam;

				floatParam = (desiredTexParams.first & CT_ANISOTROPIC_FILTER_BIT) ? (GLfloat)MaxAnisotropy : 1.0f;

				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, floatParam);
			}
			if (texParamsFirstXor & CT_GENERATE_MIPMAPS_BIT) {
				GLint intParam;

				intParam = (desiredTexParams.first & CT_GENERATE_MIPMAPS_BIT) ? GL_TRUE : GL_FALSE;

				glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, intParam);
			}

			if (desiredTexParams.maxLevel != pBind->texParams.maxLevel) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, desiredTexParams.maxLevel);
			}

			//Store updated texture parameter state in cached texture object
			pBind->texParams = desiredTexParams;
		}


		//Some textures only upload the base texture
		INT MaxUploadLevel = MaxLevel;
		if (SkipMipmaps) {
			MaxUploadLevel = 0;
		}
		if (AutoGenerateMipmaps && (pBind->texType == TEX_TYPE_NORMAL)) {
			MaxUploadLevel = 0;
		}


		//Set initial texture width and height in the context structure
		//Setup code must ensure that both UBits and VBits are greater than or equal to 0
		m_texConvertCtx.texWidthPow2 = 1 << pBind->UBits;
		m_texConvertCtx.texHeightPow2 = 1 << pBind->VBits;

		INT Level;
		for (Level = 0; Level <= MaxUploadLevel; Level++) {
			// Convert the mipmap.
			INT MipIndex = pBind->BaseMip + Level;
			INT stepBits = 0;
			if (MipIndex >= Info.NumMips) {
				stepBits = MipIndex - (Info.NumMips - 1);
				MipIndex = Info.NumMips - 1;
			}
			m_texConvertCtx.stepBits = stepBits;

			FMipmapBase* Mip = Info.Mips[MipIndex];
			if (!Mip->DataPtr) {
				//Skip looking at any subsequent mipmap pointers
				break;
			}
			else {
				switch (pBind->texType) {
				case TEX_TYPE_COMPRESSED_DXT1:
					//No conversion required for compressed DXT1 textures
					break;

				case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
					guard(ConvertDXT1_DXT3);
					ConvertDXT1_DXT3(Mip, Level);
					unguard;
					break;

				case TEX_TYPE_PALETTED:
					guard(ConvertP8_P8);
					if (stepBits == 0) {
						ConvertP8_P8_NoStep(Mip, Level);
					}
					else {
						ConvertP8_P8(Mip, Level);
					}
					unguard;
					break;

				case TEX_TYPE_HAS_PALETTE:
					guard(ConvertP8_RGBA8888);
					if (stepBits == 0) {
						ConvertP8_RGBA8888_NoStep(Mip, Info.Palette, Level);
					}
					else {
						ConvertP8_RGBA8888(Mip, Info.Palette, Level);
					}
					unguard;
					break;

				default:
					guard(ConvertBGRA7777);
					(this->*pBind->pConvertBGRA7777)(Mip, Level);
					unguard;
				}

				BYTE *Src = (BYTE*)m_texConvertCtx.pCompose;
				DWORD texWidth, texHeight;

				//Get current texture width and height
				texWidth = m_texConvertCtx.texWidthPow2;
				texHeight = m_texConvertCtx.texHeightPow2;

				//Calculate and save next texture width and height
				//Both are divided by two down to a floor of 1
				//Texture width and height must be even powers of 2 for the following code to work
				m_texConvertCtx.texWidthPow2 = (texWidth & 0x1) | (texWidth >> 1);
				m_texConvertCtx.texHeightPow2 = (texHeight & 0x1) | (texHeight >> 1);

				if (!needTexAllocate) {
					if ((pBind->texType == TEX_TYPE_COMPRESSED_DXT1) || (pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3)) {
						guard(glCompressedTexSubImage2D);
#if 0
{
	static int si;
	dout << L"utglr: glCompressedTexSubImage2DARB = " << si++ << std::endl;
}
#endif
						glCompressedTexSubImage2DARB(
							GL_TEXTURE_2D,
							Level,
							0,
							0,
							texWidth,
							texHeight,
							pBind->texInternalFormat,
							(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? (texWidth * texHeight) : (texWidth * texHeight / 2),
							(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? Src : Mip->DataPtr);
						unguard;
					}
					else {
						guard(glTexSubImage2D);
#if 0
{
	static int si;
	dout << L"utglr: glTexSubImage2D = " << si++ << std::endl;
}
#endif
						glTexSubImage2D(
							GL_TEXTURE_2D,
							Level,
							0,
							0,
							texWidth,
							texHeight,
							pBind->texSourceFormat,
							GL_UNSIGNED_BYTE,
							Src);
						unguard;
					}
				}
				else {
					if ((pBind->texType == TEX_TYPE_COMPRESSED_DXT1) || (pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3)) {
						guard(glCompressedTexImage2D);
#if 0
{
	static int si;
	dout << L"utglr: glCompressedTexImage2DARB = " << si++ << std::endl;
}
#endif
						glCompressedTexImage2DARB(
							GL_TEXTURE_2D,
							Level,
							pBind->texInternalFormat,
							texWidth,
							texHeight,
							0,
							(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? (texWidth * texHeight) : (texWidth * texHeight / 2),
							(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? Src : Mip->DataPtr);
						unguard;
					}
					else {
						guard(glTexImage2D);
#if 0
{
	static int si;
	dout << L"utglr: glTexImage2D = " << si++ << std::endl;
}
#endif
						glTexImage2D(
							GL_TEXTURE_2D,
							Level,
							pBind->texInternalFormat,
							texWidth,
							texHeight,
							0,
							pBind->texSourceFormat,
							GL_UNSIGNED_BYTE,
							Src);
						unguard;
					}
				}
			}
		}

		if (memAllocSize > LOCAL_TEX_COMPOSE_BUFFER_SIZE) {
			m_texComposeMemMark.Pop();
		}

		unclock(ImageCycles);

		//Restore palette index 0 for masked paletted textures
		if (Info.Palette && (PolyFlags & PF_Masked)) {
			Info.Palette[0] = paletteIndex0;
		}

		// Cleanup.
		if (SupportsLazyTextures) {
			Info.Unload();
		}
	}

	unguard;
}

void UOpenGLRenderDevice::CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags) {
#if 0
{
	dout << L"utglr: CacheId = "
		<< HexString((DWORD)((QWORD)Info.CacheID >> 32), 32) << L":"
		<< HexString((DWORD)((QWORD)Info.CacheID & 0xFFFFFFFF), 32) << std::endl;
}
{
	const UTexture *pTexture = Info.Texture;
	const TCHAR *pName = pTexture->GetFullName();
	if (pName) dout << L"utglr: TexName = " << pName << std::endl;
}
{
	dout << L"utglr: NumMips = " << Info.NumMips << std::endl;
}
{
	unsigned int u;

	dout << L"utglr: ZPBindTree Size = ";
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		dout << m_zeroPrefixBindTrees[u].calc_size();
		if (u != (NUM_CTTree_TREES - 1)) dout << L", ";
	}
	dout << std::endl;

	dout << L"utglr: NZPBindTree Size = ";
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		dout << m_nonZeroPrefixBindTrees[u].calc_size();
		if (u != (NUM_CTTree_TREES - 1)) dout << L", ";
	}
	dout << std::endl;
}
#endif

	// Figure out OpenGL-related scaling for the texture.
	DWORD texFlags = 0;
	INT BaseMip = 0;
	INT MaxLevel;
	INT UBits = Info.Mips[0]->UBits;
	INT VBits = Info.Mips[0]->VBits;
	INT UCopyBits = 0;
	INT VCopyBits = 0;
	if ((UBits - VBits) > MaxLogUOverV) {
		VCopyBits += (UBits - VBits) - MaxLogUOverV;
		VBits = UBits - MaxLogUOverV;
	}
	if ((VBits - UBits) > MaxLogVOverU) {
		UCopyBits += (VBits - UBits) - MaxLogVOverU;
		UBits = VBits - MaxLogVOverU;
	}
	if (UBits < MinLogTextureSize) {
		UCopyBits += MinLogTextureSize - UBits;
		UBits += MinLogTextureSize - UBits;
	}
	if (VBits < MinLogTextureSize) {
		VCopyBits += MinLogTextureSize - VBits;
		VBits += MinLogTextureSize - VBits;
	}
	if (UBits > MaxLogTextureSize) {
		BaseMip += UBits - MaxLogTextureSize;
		VBits -= UBits - MaxLogTextureSize;
		UBits = MaxLogTextureSize;
		if (VBits < 0) {
			VCopyBits = -VBits;
			VBits = 0;
		}
	}
	if (VBits > MaxLogTextureSize) {
		BaseMip += VBits - MaxLogTextureSize;
		UBits -= VBits - MaxLogTextureSize;
		VBits = MaxLogTextureSize;
		if (UBits < 0) {
			UCopyBits = -UBits;
			UBits = 0;
		}
	}

	pBind->BaseMip = BaseMip;
	if (UseTNT) {
		MaxLevel = Max(UBits, VBits);
	}
	else {
		MaxLevel = Min(UBits, VBits) - MinLogTextureSize;
		if (MaxLevel < 0) {
			MaxLevel = 0;
		}
	}
	pBind->MaxLevel = MaxLevel;
	pBind->UBits = UBits;
	pBind->VBits = VBits;

	pBind->UMult = 1.0f / (Info.UScale * (Info.USize << UCopyBits));
	pBind->VMult = 1.0f / (Info.VScale * (Info.VSize << VCopyBits));

	pBind->UClampVal = Info.UClamp - 1;
	pBind->VClampVal = Info.VClamp - 1;

	//Check for texture that does not require clamping
	//No clamp required if ((Info.UClamp == Info.USize) & (Info.VClamp == Info.VSize))
	if (((Info.UClamp ^ Info.USize) | (Info.VClamp ^ Info.VSize)) == 0) {
		texFlags |= TEX_FLAG_NO_CLAMP;
	}


	//Determine texture type
	//PolyFlags PF_Masked cannot change if existing texture is updated as it caches texture type information here
	bool paletted = false;
	if (UsePalette && Info.Palette) {
		paletted = true;
		if (!UseAlphaPalette) {
			if ((PolyFlags & PF_Masked) || (Info.Palette[0].A != 255)) {
				paletted = false;
			}
		}
	}

	if ((Info.Format == TEXF_DXT1) && SupportsTC) {
		if (TexDXT1ToDXT3 && (!(PolyFlags & PF_Masked))) {
			pBind->texType = TEX_TYPE_COMPRESSED_DXT1_TO_DXT3;
			//texSourceFormat not used for compressed textures
			pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
		}
		else {
			pBind->texType = TEX_TYPE_COMPRESSED_DXT1;
			//texSourceFormat not used for compressed textures
			pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		}
	}
	else if (paletted) {
		pBind->texType = TEX_TYPE_PALETTED;
		pBind->texSourceFormat = GL_COLOR_INDEX;
		pBind->texInternalFormat = GL_COLOR_INDEX8_EXT;
	}
	else if (Info.Palette) {
		pBind->texType = TEX_TYPE_HAS_PALETTE;
		pBind->texSourceFormat = GL_RGBA;
		pBind->texInternalFormat = GL_RGBA8;
		//Check if texture should be 16-bit
		if (PolyFlags & PF_Memorized) {
			pBind->texInternalFormat = (PolyFlags & PF_Masked) ? GL_RGB5_A1 : GL_RGB5;
		}
	}
	else {
		pBind->texType = TEX_TYPE_NORMAL;
		if (UseBGRATextures) {
			pBind->texSourceFormat = GL_BGRA_EXT;
			if (texFlags & TEX_FLAG_NO_CLAMP) {
				pBind->pConvertBGRA7777 = &UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888_NoClamp;
			}
			else {
				pBind->pConvertBGRA7777 = &UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888;
			}
		}
		else {
			pBind->texSourceFormat = GL_RGBA;
			pBind->pConvertBGRA7777 = &UOpenGLRenderDevice::ConvertBGRA7777_RGBA8888;
		}
		pBind->texInternalFormat = GL_RGBA8;
	}

	return;
}


void UOpenGLRenderDevice::ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level) {
	DWORD *pSrc = (DWORD *)Mip->DataPtr;
	DWORD *pDest = (DWORD *)m_texConvertCtx.pCompose;
	DWORD numBlocks = 1 << (Max(0, (INT)m_texConvertCtx.pBind->UBits - Level - 2) + Max(0, (INT)m_texConvertCtx.pBind->VBits - Level - 2));
	for (DWORD block = 0; block < numBlocks; block++) {
		*pDest = 0xFFFFFFFF;
		*(pDest + 1) = 0xFFFFFFFF;
		*(pDest + 2) = *pSrc;
		*(pDest + 3) = *(pSrc + 1);
		pSrc += 2;
		pDest += 4;
	}

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertDXT1_DXT3 = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::ConvertP8_P8(const FMipmapBase *Mip, INT Level) {
	BYTE* Ptr = (BYTE*)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		BYTE* Base = (BYTE*)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			*Ptr++ = Base[j & UMask];
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertP8_P8 = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::ConvertP8_P8_NoStep(const FMipmapBase *Mip, INT Level) {
	BYTE* Ptr = (BYTE*)m_texConvertCtx.pCompose;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		BYTE* Base = (BYTE*)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			*Ptr++ = Base[j & UMask];
		} while ((j += 1) < j_stop);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertP8_P8_NoStep = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	FColor* Ptr = (FColor*)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		BYTE* Base = (BYTE*)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			*Ptr++ = Palette[Base[j & UMask]];
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertP8_RGBA8888 = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level) {
	FColor* Ptr = (FColor*)m_texConvertCtx.pCompose;
	DWORD UMask = Mip->USize - 1;
	DWORD VMask = Mip->VSize - 1;
	INT i_stop = m_texConvertCtx.texHeightPow2;
	INT j_stop = m_texConvertCtx.texWidthPow2;
	INT i = 0;
	do { //i_stop always >= 1
		BYTE* Base = (BYTE*)Mip->DataPtr + (i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1
			*Ptr++ = Palette[Base[j & UMask]];
		} while ((j += 1) < j_stop);
	} while ((i += 1) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertP8_RGBA8888_NoStep = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level) {
	FColor* Ptr = (FColor*)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = Mip->VSize - 1;
	DWORD VClampVal = m_texConvertCtx.pBind->VClampVal;
	DWORD UMask = Mip->USize - 1;
	DWORD UClampVal = m_texConvertCtx.pBind->UClampVal;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		FColor* Base = (FColor*)Mip->DataPtr + Min<DWORD>(i & VMask, VClampVal) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1;
			const FColor& Src = Base[Min<DWORD>(j & UMask, UClampVal)];
			*(DWORD *)Ptr = *((DWORD *)&Src) * 2; // because of 7777
			Ptr++;
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertBGRA7777_BGRA8888 = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level) {
	FColor* Ptr = (FColor*)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = Mip->VSize - 1;
	DWORD UMask = Mip->USize - 1;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		FColor* Base = (FColor*)Mip->DataPtr + (DWORD)(i & VMask) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1;
			const FColor& Src = Base[(DWORD)(j & UMask)];
			*(DWORD *)Ptr = *((DWORD *)&Src) * 2; // because of 7777
			Ptr++;
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertBGRA7777_BGRA8888_NoClamp = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::ConvertBGRA7777_RGBA8888(const FMipmapBase *Mip, INT Level) {
	FColor* Ptr = (FColor*)m_texConvertCtx.pCompose;
	INT StepBits = m_texConvertCtx.stepBits;
	DWORD VMask = Mip->VSize - 1;
	DWORD VClampVal = m_texConvertCtx.pBind->VClampVal;
	DWORD UMask = Mip->USize - 1;
	DWORD UClampVal = m_texConvertCtx.pBind->UClampVal;
	INT ij_inc = 1 << StepBits;
	INT i_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->VBits - Level + StepBits);
	INT j_stop = 1 << Max(0, (INT)m_texConvertCtx.pBind->UBits - Level + StepBits);
	INT i = 0;
	do { //i_stop always >= 1
		FColor* Base = (FColor*)Mip->DataPtr + Min<DWORD>(i & VMask, VClampVal) * Mip->USize;
		INT j = 0;
		do { //j_stop always >= 1;
			const FColor& Src = Base[Min<DWORD>(j & UMask, UClampVal)];
			// vogel: optimize it.
			Ptr->R = 2 * Src.B;
			Ptr->G = 2 * Src.G;
			Ptr->B = 2 * Src.R;
			Ptr->A = 2 * Src.A; // because of 7777

			Ptr++;
		} while ((j += ij_inc) < j_stop);
	} while ((i += ij_inc) < i_stop);

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
	{
		static int si;
		dout << L"utglr: ConvertBGRA7777_RGBA8888 = " << si++ << std::endl;
	}
#endif
}

void UOpenGLRenderDevice::SetBlendNoCheck(DWORD blendFlags) {
	guardSlow(UOpenGLRenderDevice::SetBlend);

	// Detect changes in the blending modes.
	DWORD Xor = m_curBlendFlags ^ blendFlags;

	//Save copy of current blend flags
	DWORD curBlendFlags = m_curBlendFlags;

	//Update main copy of current blend flags early
	m_curBlendFlags = blendFlags;

#ifdef UTGLR_RUNE_BUILD
	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted | PF_AlphaBlend;
#else
	const DWORD GL_BLEND_FLAG_BITS = PF_Translucent | PF_Modulated | PF_Highlighted;
#endif
	DWORD relevantBlendFlagBits = GL_BLEND_FLAG_BITS | m_smoothMaskedTexturesBit;
	if (Xor & (relevantBlendFlagBits)) {
		if (!(blendFlags & (relevantBlendFlagBits))) {
			glDisable(GL_BLEND);
		}
		else {
			if (!(curBlendFlags & (relevantBlendFlagBits))) {
				glEnable(GL_BLEND);
			}
			if (blendFlags & PF_Translucent) {
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
			}
			else if (blendFlags & PF_Modulated) {
				glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
			}
			else if (blendFlags & PF_Highlighted) {
				glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
#ifdef UTGLR_RUNE_BUILD
			else if (blendFlags & PF_AlphaBlend) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
#endif
			else if (blendFlags & PF_Masked) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
	}
	if (Xor & PF_Masked) {
		if (blendFlags & PF_Masked) {
			glEnable(GL_ALPHA_TEST);
		}
		else {
			glDisable(GL_ALPHA_TEST);
		}
	}
	if (Xor & PF_Invisible) {
		UBOOL flag = ((blendFlags & PF_Invisible) == 0) ? GL_TRUE : GL_FALSE;
		glColorMask(flag, flag, flag, flag);
	}
	if (Xor & PF_Occlude) {
		UBOOL flag = ((blendFlags & PF_Occlude) == 0) ? GL_FALSE : GL_TRUE;
		glDepthMask(flag);
	}

	unguardSlow;
}

//This function will initialize or invalidate the texture environment state
//The current architecture allows both operations to be done in the same way
void UOpenGLRenderDevice::InitOrInvalidateTexEnvState(void) {
	INT TMU;

	//For initialization, flags for all texture units are cleared
	//For invalidation, flags for all texture units are also cleared as it is
	//fast enough and has no potential outside interaction side effects
	for (TMU = 0; TMU < MAX_TMUNITS; TMU++) {
		m_curTexEnvFlags[TMU] = 0;
	}

	//Set TexEnv 0 to modulated by default
	if (SUPPORTS_GL_ARB_multitexture) {
		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	SetTexEnv(0, PF_Modulated);

	return;
}

void UOpenGLRenderDevice::SetPermanentTexEnvState(INT TMUnits) {
	INT TMU;

	//Set permanent tex env state for all texture units
	for (TMU = 0; TMU < TMUnits; TMU++) {
		//Only switch texture units if multitexture support is present
		if (SUPPORTS_GL_ARB_multitexture) {
			glActiveTextureARB(GL_TEXTURE0_ARB + TMU);
		}

		//Set permanent tex env state for GL_EXT_texture_env_combine
		if (SUPPORTS_GL_EXT_texture_env_combine) {
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_PREVIOUS_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
		}
	}
	//Leave texture unit 0 active when finished
	if (SUPPORTS_GL_ARB_multitexture) {
		glActiveTextureARB(GL_TEXTURE0_ARB);
	}

	return;
}

void UOpenGLRenderDevice::SetTexLODBiasState(INT TMUnits) {
	INT TMU;

	//Set texture LOD bias for all texture units
	for (TMU = 0; TMU < TMUnits; TMU++) {
		//Only switch texture units if multitexture support is present
		if (SUPPORTS_GL_ARB_multitexture) {
			glActiveTextureARB(GL_TEXTURE0_ARB + TMU);
		}

		//Set texture LOD bias
		glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, LODBias);
	}
	//Leave texture unit 0 active when finished
	if (SUPPORTS_GL_ARB_multitexture) {
		glActiveTextureARB(GL_TEXTURE0_ARB);
	}

	return;
}

void UOpenGLRenderDevice::SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags) {
	guardSlow(UOpenGLRenderDevice::SetTexEnv);

	//Update current tex env flags early as there are no subsequent dependencies
	m_curTexEnvFlags[texUnit] = texEnvFlags;

	if (texEnvFlags & PF_Modulated) {
		if ((texEnvFlags & PF_FlatShaded) || (texUnit != 0) && !OneXBlending) {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE);

			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PREVIOUS_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 2.0f);
		}
		else {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		}
	}
	else if (texEnvFlags & PF_Memorized) {
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_INTERPOLATE_EXT);
		glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);

		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
		glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_ALPHA);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT);
		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_PREVIOUS_EXT);

		glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE);

		glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
	}
	else if (texEnvFlags & PF_Highlighted) {
		if (SUPPORTS_GL_ATI_texture_env_combine3) {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE_ADD_ATI);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_REPLACE);

			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_SRC_COLOR);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_TEXTURE);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_PREVIOUS_EXT);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
		}
		else {
			glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV);

			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_ADD);
			glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_ADD);

			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB_EXT, GL_ONE_MINUS_SRC_ALPHA);
			glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND2_RGB_EXT, GL_SRC_COLOR);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_TEXTURE);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_RGB_EXT, GL_TEXTURE);

			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_ZERO);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PREVIOUS_EXT);
			glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE2_ALPHA_EXT, GL_PREVIOUS_EXT);

			glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE_EXT, 1.0f);
		}
	}

	unguardSlow;
}


void UOpenGLRenderDevice::SetDefaultColorStateNoCheck(void) {
	//Check for normal array
	if (m_currentColorFlags & CF_NORMAL_ARRAY) {
		glDisableClientState(GL_NORMAL_ARRAY);
	}

	//Check for color sum
	if (m_currentColorFlags & CF_COLOR_SUM) {
		glDisable(GL_COLOR_SUM_EXT);
	}

	//Check for dual color array
	if (m_currentColorFlags & CF_DUAL_COLOR_ARRAY) {
		glDisableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);

		//Reset color array pointer to default when not using dual color array
		glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FGLSingleColor), &SingleColorArray[0].color);
	}

	//Check for color array
	if (m_currentColorFlags & CF_COLOR_ARRAY) {
		glDisableClientState(GL_COLOR_ARRAY);
	}

	m_currentColorFlags = 0;

	return;
}

void UOpenGLRenderDevice::SetColorStateNoCheck(void) {
	BYTE changedFlags;

	changedFlags = m_requestedColorFlags ^ m_currentColorFlags;

	//Check for normal array change
	if (changedFlags & CF_NORMAL_ARRAY) {
		if (m_requestedColorFlags & CF_NORMAL_ARRAY) {
			glEnableClientState(GL_NORMAL_ARRAY);
		}
		else {
			glDisableClientState(GL_NORMAL_ARRAY);
		}
	}

	//Check for color sum change
	if (changedFlags & CF_COLOR_SUM) {
		if (m_requestedColorFlags & CF_COLOR_SUM) {
			glEnable(GL_COLOR_SUM_EXT);
		}
		else {
			glDisable(GL_COLOR_SUM_EXT);
		}
	}

	//Check for dual color array change
	if (changedFlags & CF_DUAL_COLOR_ARRAY) {
		if (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY) {
			glEnableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);

			//Set up color array pointer for dual color array
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FGLDoubleColor), &DoubleColorArray[0].color);
		}
		else {
			glDisableClientState(GL_SECONDARY_COLOR_ARRAY_EXT);

			//Reset color array pointer to default when not using dual color array
			glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(FGLSingleColor), &SingleColorArray[0].color);
		}
	}

	//Check for color array change
	if (changedFlags & CF_COLOR_ARRAY) {
		if (m_requestedColorFlags & CF_COLOR_ARRAY) {
			glEnableClientState(GL_COLOR_ARRAY);
		}
		else {
			glDisableClientState(GL_COLOR_ARRAY);
		}
	}

	m_currentColorFlags = m_requestedColorFlags;

	return;
}


void UOpenGLRenderDevice::SetAAStateNoCheck(bool AAEnable) {
	//Save new AA state
	m_curAAEnable = AAEnable;

	m_AASwitchCount++;

	//Set new AA state
	if (AAEnable) {
		glEnable(GL_MULTISAMPLE_ARB);
	}
	else {
		glDisable(GL_MULTISAMPLE_ARB);
	}

	return;
}


void UOpenGLRenderDevice::AllocateVertexProgramNamesSafe(void) {
	//Do not allocate names if already allocated
	if (m_allocatedVertexProgramNames) {
		return;
	}

	//Allocate names
	glGenProgramsARB(1, &m_vpDefaultRenderingState);
	glGenProgramsARB(1, &m_vpDefaultRenderingStateWithFog);
#ifdef UTGLR_RUNE_BUILD
	glGenProgramsARB(1, &m_vpDefaultRenderingStateWithLinearFog);
#endif
	glGenProgramsARB(MAX_TMUNITS, m_vpComplexSurface);
	glGenProgramsARB(1, &m_vpComplexSurfaceDetailAlpha);
	glGenProgramsARB(1, &m_vpComplexSurfaceSingleTextureAndDetailTexture);
	glGenProgramsARB(1, &m_vpComplexSurfaceDualTextureAndDetailTexture);
	glGenProgramsARB(1, &m_vpComplexSurfaceSingleTextureWithPos);
	glGenProgramsARB(1, &m_vpComplexSurfaceDualTextureWithPos);
	glGenProgramsARB(1, &m_vpComplexSurfaceTripleTextureWithPos);

	//Mark names as allocated
	m_allocatedVertexProgramNames = true;

	return;
}

void UOpenGLRenderDevice::FreeVertexProgramNamesSafe(void) {
	//Do not free names if not allocated
	if (!m_allocatedVertexProgramNames) {
		return;
	}

	//Free names
	glDeleteProgramsARB(1, &m_vpDefaultRenderingState);
	glDeleteProgramsARB(1, &m_vpDefaultRenderingStateWithFog);
#ifdef UTGLR_RUNE_BUILD
	glDeleteProgramsARB(1, &m_vpDefaultRenderingStateWithLinearFog);
#endif
	glDeleteProgramsARB(MAX_TMUNITS, m_vpComplexSurface);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceDetailAlpha);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceSingleTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceDualTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceSingleTextureWithPos);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceDualTextureWithPos);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceTripleTextureWithPos);

	//Mark names as not allocated
	m_allocatedVertexProgramNames = false;

	return;
}

bool UOpenGLRenderDevice::InitializeVertexPrograms(void) {
	bool initOk = true;


	//Default rendering state
	initOk &= LoadVertexProgram(m_vpDefaultRenderingState, g_vpDefaultRenderingState,
		TEXT("Default rendering state"));

	//Default rendering state with fog
	initOk &= LoadVertexProgram(m_vpDefaultRenderingStateWithFog, g_vpDefaultRenderingStateWithFog,
		TEXT("Default rendering state with fog"));

#ifdef UTGLR_RUNE_BUILD
	//Default rendering state with linear fog
	initOk &= LoadVertexProgram(m_vpDefaultRenderingStateWithLinearFog, g_vpDefaultRenderingStateWithLinearFog,
		TEXT("Default rendering state with linear fog"));
#endif


	//Complex surface single texture
	initOk &= LoadVertexProgram(m_vpComplexSurface[0], g_vpComplexSurfaceSingleTexture,
		TEXT("Complex surface single texture"));

	if (TMUnits >= 2) {
		//Complex surface dual texture
		initOk &= LoadVertexProgram(m_vpComplexSurface[1], g_vpComplexSurfaceDualTexture,
			TEXT("Complex surface dual texture"));
	}

	if (TMUnits >= 3) {
		//Complex surface triple texture
		initOk &= LoadVertexProgram(m_vpComplexSurface[2], g_vpComplexSurfaceTripleTexture,
			TEXT("Complex surface triple texture"));
	}

	if (TMUnits >= 4) {
		//Complex surface quad texture
		initOk &= LoadVertexProgram(m_vpComplexSurface[3], g_vpComplexSurfaceQuadTexture,
			TEXT("Complex surface quad texture"));
	}


	if (UseDetailAlpha) {
		//Complex surface detail alpha
		initOk &= LoadVertexProgram(m_vpComplexSurfaceDetailAlpha, g_vpComplexSurfaceDetailAlpha,
			TEXT("Complex surface detail alpha"));
	}

	if (SinglePassDetail) {
		//Complex surface single texture and detail texture
		initOk &= LoadVertexProgram(m_vpComplexSurfaceSingleTextureAndDetailTexture, g_vpComplexSurfaceSingleTextureAndDetailTexture,
			TEXT("Complex surface single texture and detail texture"));

		//Complex surface dual texture and detail texture
		initOk &= LoadVertexProgram(m_vpComplexSurfaceDualTextureAndDetailTexture, g_vpComplexSurfaceDualTextureAndDetailTexture,
			TEXT("Complex surface dual texture and detail texture"));
	}


	if (UseFragmentProgram) {
		//Complex surface single texture with position
		initOk &= LoadVertexProgram(m_vpComplexSurfaceSingleTextureWithPos, g_vpComplexSurfaceSingleTextureWithPos,
			TEXT("Complex surface single texture with position"));

		//Complex surface dual texture with position
		initOk &= LoadVertexProgram(m_vpComplexSurfaceDualTextureWithPos, g_vpComplexSurfaceDualTextureWithPos,
			TEXT("Complex surface dual texture with position"));

		//Complex surface triple texture with position
		initOk &= LoadVertexProgram(m_vpComplexSurfaceTripleTextureWithPos, g_vpComplexSurfaceTripleTextureWithPos,
			TEXT("Complex surface triple texture with position"));
	}


	//Reset to default vertex program and update current vertex program variable
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
	m_vpCurrent = 0;

	return initOk;
}

bool UOpenGLRenderDevice::LoadVertexProgram(GLuint vpId, const char *pProgram, const TCHAR *pName) {
	GLint iErrorPos;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utglr: Loading vertex program \"") << pName << TEXT("\"") << std::endl;
	}

	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpId);
	glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(pProgram), pProgram);

	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);

	if (DebugBit(DEBUG_BIT_BASIC)) {
		if (iErrorPos != -1) {
			dout << TEXT("utglr: Vertex program error at offset ") << iErrorPos << std::endl;
			dout << TEXT("utglr: Vertex program text from error offset:\n") << appFromAnsi(pProgram + iErrorPos) << std::endl;
		}
	}

	if (iErrorPos != -1) {
		return false;
	}

	return true;
}

//Attempts to initializes vertex program mode
//Safe to call multiple times as all names are always allocated
//Will reload any existing programs if called multiple times
void UOpenGLRenderDevice::TryInitializeVertexProgramMode(void) {
	//Allocate vertex program names
	AllocateVertexProgramNamesSafe();

	//Initialize vertex programs
	if (InitializeVertexPrograms() == false) {
		//Free vertex program names
		FreeVertexProgramNamesSafe();

		//Disable vertex program mode
		DCV.UseVertexProgram = 0;
		UseVertexProgram = 0;
		PL_UseVertexProgram = 0;

		if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utglr: Vertex program initialization failed") << std::endl;
	}

	return;
}

//Shuts down vertex program mode if it is active
//Freeing the vertex program names takes care of releasing resources
//Safe to call even if vertex program mode is not supported or was never initialized
void UOpenGLRenderDevice::ShutdownVertexProgramMode(void) {
	//Free vertex program names
	FreeVertexProgramNamesSafe();

	//Disable vertex program mode if it was enabled
	if (m_vpModeEnabled == true) {
		//Mark vertex program mode as disabled
		m_vpModeEnabled = false;

		//Disable vertex program mode
		glDisable(GL_VERTEX_PROGRAM_ARB);
	}

	//The default vertex program is now current
	m_vpCurrent = 0;

	return;
}


void UOpenGLRenderDevice::AllocateFragmentProgramNamesSafe(void) {
	//Do not allocate names if already allocated
	if (m_allocatedFragmentProgramNames) {
		return;
	}

	//Allocate names
	glGenProgramsARB(1, &m_fpDefaultRenderingState);
	glGenProgramsARB(1, &m_fpDefaultRenderingStateWithFog);
#ifdef UTGLR_RUNE_BUILD
	glGenProgramsARB(1, &m_fpDefaultRenderingStateWithLinearFog);
#endif
	glGenProgramsARB(1, &m_fpComplexSurfaceSingleTexture);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated2X);
	glGenProgramsARB(1, &m_fpComplexSurfaceSingleTextureWithFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulatedWithFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated2XWithFog);
	glGenProgramsARB(1, &m_fpDetailTexture);
	glGenProgramsARB(1, &m_fpDetailTextureTwoLayer);
	glGenProgramsARB(1, &m_fpSingleTextureAndDetailTexture);
	glGenProgramsARB(1, &m_fpSingleTextureAndDetailTextureTwoLayer);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTexture);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayer);

	//Mark names as allocated
	m_allocatedFragmentProgramNames = true;

	return;
}

void UOpenGLRenderDevice::FreeFragmentProgramNamesSafe(void) {
	//Do not free names if not allocated
	if (!m_allocatedFragmentProgramNames) {
		return;
	}

	//Free names
	glDeleteProgramsARB(1, &m_fpDefaultRenderingState);
	glDeleteProgramsARB(1, &m_fpDefaultRenderingStateWithFog);
#ifdef UTGLR_RUNE_BUILD
	glDeleteProgramsARB(1, &m_fpDefaultRenderingStateWithLinearFog);
#endif
	glDeleteProgramsARB(1, &m_fpComplexSurfaceSingleTexture);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated2X);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceSingleTextureWithFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulatedWithFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated2XWithFog);
	glDeleteProgramsARB(1, &m_fpDetailTexture);
	glDeleteProgramsARB(1, &m_fpDetailTextureTwoLayer);
	glDeleteProgramsARB(1, &m_fpSingleTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_fpSingleTextureAndDetailTextureTwoLayer);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayer);

	//Mark names as not allocated
	m_allocatedFragmentProgramNames = false;

	return;
}

bool UOpenGLRenderDevice::InitializeFragmentPrograms(void) {
	bool initOk = true;


	//Default rendering state
	initOk &= LoadFragmentProgram(m_fpDefaultRenderingState, g_fpDefaultRenderingState,
		TEXT("Default rendering state"));

	//Default rendering state with fog
	initOk &= LoadFragmentProgram(m_fpDefaultRenderingStateWithFog, g_fpDefaultRenderingStateWithFog,
		TEXT("Default rendering state with fog"));

#ifdef UTGLR_RUNE_BUILD
	//Default rendering state with linear fog
	initOk &= LoadFragmentProgram(m_fpDefaultRenderingStateWithLinearFog, g_fpDefaultRenderingStateWithLinearFog,
		TEXT("Default rendering state with linear fog"));
#endif


	//Complex surface single texture
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceSingleTexture, g_fpComplexSurfaceSingleTexture,
		TEXT("Complex surface single texture"));

	//Complex surface dual texture modulated
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulated, g_fpComplexSurfaceDualTextureModulated,
		TEXT("Complex surface dual texture modulated"));

	//Complex surface dual texture modulated 2X
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulated2X, g_fpComplexSurfaceDualTextureModulated2X,
		TEXT("Complex surface dual texture modulated 2X"));


	//Complex surface single texture with fog
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceSingleTextureWithFog, g_fpComplexSurfaceSingleTextureWithFog,
		TEXT("Complex surface single texture with fog"));

	//Complex surface dual texture modulated with fog
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulatedWithFog, g_fpComplexSurfaceDualTextureModulatedWithFog,
		TEXT("Complex surface dual texture modulated with fog"));

	//Complex surface dual texture modulated 2X with fog
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulated2XWithFog, g_fpComplexSurfaceDualTextureModulated2XWithFog,
		TEXT("Complex surface dual texture modulated 2X with fog"));


	if (DetailTextures) {
		//Detail texture
		initOk &= LoadFragmentProgram(m_fpDetailTexture, g_fpDetailTexture,
			TEXT("Detail texture"));

		//Detail texture two layer
		initOk &= LoadFragmentProgram(m_fpDetailTextureTwoLayer, g_fpDetailTextureTwoLayer,
			TEXT("Detail texture two layer"));

		//Single texture and detail texture
		initOk &= LoadFragmentProgram(m_fpSingleTextureAndDetailTexture, g_fpSingleTextureAndDetailTexture,
			TEXT("Complex surface single texture and detail texture"));

		//Single texture and detail texture two layer
		initOk &= LoadFragmentProgram(m_fpSingleTextureAndDetailTextureTwoLayer, g_fpSingleTextureAndDetailTextureTwoLayer,
			TEXT("Complex surface single texture and detail texture two layer"));

		//Dual texture and detail texture
		initOk &= LoadFragmentProgram(m_fpDualTextureAndDetailTexture, g_fpDualTextureAndDetailTexture,
			TEXT("Complex surface dual texture and detail texture"));

		//Dual texture and detail texture two layer
		initOk &= LoadFragmentProgram(m_fpDualTextureAndDetailTextureTwoLayer, g_fpDualTextureAndDetailTextureTwoLayer,
			TEXT("Complex surface dual texture and detail texture two layer"));
	}


	//Reset to default fragment program and update current fragment program variable
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
	m_fpCurrent = 0;

	return initOk;
}

bool UOpenGLRenderDevice::LoadFragmentProgram(GLuint fpId, const char *pProgram, const TCHAR *pName) {
	GLint iErrorPos;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dout << TEXT("utglr: Loading fragment program \"") << pName << TEXT("\"") << std::endl;
	}

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpId);
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(pProgram), pProgram);

	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);

	if (DebugBit(DEBUG_BIT_BASIC)) {
		if (iErrorPos != -1) {
			dout << TEXT("utglr: Fragment program error at offset ") << iErrorPos << std::endl;
			dout << TEXT("utglr: Fragment program text from error offset:\n") << appFromAnsi(pProgram + iErrorPos) << std::endl;
		}
	}

	if (iErrorPos != -1) {
		return false;
	}

	return true;
}

//Attempts to initializes fragment program mode
//Safe to call multiple times as all names are always allocated
//Will reload any existing programs if called multiple times
void UOpenGLRenderDevice::TryInitializeFragmentProgramMode(void) {
	//Allocate fragment program names
	AllocateFragmentProgramNamesSafe();

	//Initialize fragment programs
	if (InitializeFragmentPrograms() == false) {
		//Free fragment program names
		FreeFragmentProgramNamesSafe();

		//Disable fragment program mode
		DCV.UseFragmentProgram = 0;
		UseFragmentProgram = 0;
		PL_UseFragmentProgram = 0;

		if (DebugBit(DEBUG_BIT_BASIC)) dout << TEXT("utglr: Fragment program initialization failed") << std::endl;
	}

	return;
}

//Shuts down fragment program mode if it is active
//Freeing the fragment program names takes care of releasing resources
//Safe to call even if fragment program mode is not supported or was never initialized
void UOpenGLRenderDevice::ShutdownFragmentProgramMode(void) {
	//Free fragment program names
	FreeFragmentProgramNamesSafe();

	//Disable fragment program mode if it was enabled
	if (m_fpModeEnabled == true) {
		//Mark fragment program mode as disabled
		m_fpModeEnabled = false;

		//Disable fragment program mode
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
	}

	//The default fragment program is now current
	m_fpCurrent = 0;

	return;
}


void UOpenGLRenderDevice::SetProjectionStateNoCheck(bool requestNearZRangeHackProjection) {
	//Save new Z range hack projection state
	m_nearZRangeHackProjectionActive = requestNearZRangeHackProjection;

	//Select projection matrix and reset to identity
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	//Set default zNear
	FLOAT zNear = 0.5f;

	if (requestNearZRangeHackProjection) {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

		glScalef(1, 1, 0.125 * 0.125);
		zNear = 4.0f;
	}
	else {
#ifdef UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

		if (m_useZRangeHack) {
			zNear = 4.0f;
		}
	}

	glFrustum(-m_RProjZ * zNear, +m_RProjZ * zNear, -m_Aspect*m_RProjZ * zNear, +m_Aspect*m_RProjZ * zNear, 1.0 * zNear, 32768.0);

	return;
}

void UOpenGLRenderDevice::SetOrthoProjection(void) {
	//Save new Z range hack projection state
	m_nearZRangeHackProjectionActive = false;

	//Select projection matrix and reset to identity
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glOrtho(-m_RProjZ * 0.5, +m_RProjZ * 0.5, -m_Aspect*m_RProjZ * 0.5, +m_Aspect*m_RProjZ * 0.5, 1.0 * 0.5, 32768.0);

	return;
}


void UOpenGLRenderDevice::RenderPassesExec(void) {
	guard(UOpenGLRenderDevice::RenderPassesExec);

	//Some render passes paths may use fragment program

	if (m_rpMasked && m_rpForceSingle && !m_rpSetDepthEqual) {
		glDepthFunc(GL_EQUAL);
		m_rpSetDepthEqual = true;
	}

	//Call the render passes no check setup proc
	(this->*m_pRenderPassesNoCheckSetupProc)();

	m_rpTMUnits = 1;
	m_rpForceSingle = true;


	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	}
	else {
		for (INT PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	SetBlend(PF_Modulated);

	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	}
	else {
		for (INT PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	if (SUPPORTS_GL_ARB_multitexture) {
		glActiveTextureARB(GL_TEXTURE0_ARB);
	}

#if 0
{
	dout << L"utglr: PassCount = " << m_rpPassCount << std::endl;
}
#endif
	m_rpPassCount = 0;


	unguard;
}

void UOpenGLRenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	guard(UOpenGLRenderDevice::RenderPassesExec_DualTextureAndDetailTexture);

	//Some render passes paths may use fragment program

	//The dual texture and detail texture path can never be executed if single pass rendering were forced earlier
	//The depth function will never need to be changed due to single pass rendering here

	//Call the render passes no check setup dual texture and detail texture proc
	(this->*m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc)(DetailTextureInfo);

	//Single texture rendering does not need to be forced here since the detail texture is always the last pass


	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	}
	else {
		for (INT PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

#ifdef UTGLR_DEBUG_WORLD_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	SetBlend(PF_Modulated);

	if (UseMultiDrawArrays && (m_csPolyCount > 1)) {
		glMultiDrawArraysEXT(GL_TRIANGLE_FAN, MultiDrawFirstArray, MultiDrawCountArray, m_csPolyCount);
	}
	else {
		for (INT PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++) {
			glDrawArrays(GL_TRIANGLE_FAN, MultiDrawFirstArray[PolyNum], MultiDrawCountArray[PolyNum]);
		}
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	glActiveTextureARB(GL_TEXTURE0_ARB);

#if 0
{
	dout << L"utglr: PassCount = " << m_rpPassCount << std::endl;
}
#endif
	m_rpPassCount = 0;


	unguard;
}

//Must be called with (m_rpPassCount > 0)
void UOpenGLRenderDevice::RenderPassesNoCheckSetup(void) {
	INT i;
	INT t;

	SetDefaultFragmentProgramState();

	SetBlend(MultiPass.TMU[0].PolyFlags);

	i = 0;
	do {
		if (i != 0) {
			DWORD texBit;

			glActiveTextureARB(GL_TEXTURE0_ARB + i);

			texBit = 1 << i;
			if ((m_texEnableBits & texBit) == 0) {
				m_texEnableBits |= texBit;

				glEnable(GL_TEXTURE_2D);
			}
			if ((m_clientTexEnableBits & texBit) == 0) {
				m_clientTexEnableBits |= texBit;

				glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}

			SetTexEnv(i, MultiPass.TMU[i].PolyFlags);
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);
	} while (++i < m_rpPassCount);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(m_rpPassCount);
	DisableSubsequentClientTextures(m_rpPassCount);

	if (UseSSE) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		t = 0;
		do {
			__m128 uvPan;
			__m128 uvMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			uvPan = _mm_setzero_ps();
			uvMult = _mm_setzero_ps();
			uvPan = _mm_loadl_pi(uvPan, (const __m64 *)&TexInfo[t].UPan);
			uvMult = _mm_loadl_pi(uvMult, (const __m64 *)&TexInfo[t].UMult);
			uvPan = _mm_movelh_ps(uvPan, uvPan);
			uvMult = _mm_movelh_ps(uvMult, uvMult);

			INT ptCounter = m_csPtCount;
			do {
				__m128 data;

				data = _mm_load_ps((const float *)pMapDot);
				data = _mm_sub_ps(data, uvPan);
				data = _mm_mul_ps(data, uvMult);
				_mm_store_ps((float *)pTexCoord, data);

				pMapDot += 2;
				pTexCoord += 2;
			} while ((ptCounter -= 2) > 0);
		} while (++t < m_rpPassCount);
#endif //UTGLR_INCLUDE_SSE_CODE
	}
	else {
		t = 0;
		do {
			FLOAT UPan = TexInfo[t].UPan;
			FLOAT VPan = TexInfo[t].VPan;
			FLOAT UMult = TexInfo[t].UMult;
			FLOAT VMult = TexInfo[t].VMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			INT ptCounter = m_csPtCount;
			do {
				pTexCoord->u = (pMapDot->u - UPan) * UMult;
				pTexCoord->v = (pMapDot->v - VPan) * VMult;

				pMapDot++;
				pTexCoord++;
			} while (--ptCounter != 0);
		} while (++t < m_rpPassCount);
	}

	return;
}

//Must be called with (m_rpPassCount > 0)
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_VP(void) {
	INT i;
	GLuint fpId = 0;

	SetBlend(MultiPass.TMU[0].PolyFlags);

	SetVertexProgram(m_vpComplexSurface[m_rpPassCount - 1]);

	//Look for a fragment program that can use if they're enabled
	if (UseFragmentProgram) {
		if (m_rpPassCount == 1) {
			fpId = m_fpComplexSurfaceSingleTexture;
		}
		else if (m_rpPassCount == 2) {
			if (MultiPass.TMU[1].PolyFlags == PF_Modulated) {
				if (OneXBlending) {
					fpId = m_fpComplexSurfaceDualTextureModulated;
				}
				else {
					fpId = m_fpComplexSurfaceDualTextureModulated2X;
				}
			}
			else if (MultiPass.TMU[1].PolyFlags == PF_Highlighted) {
				fpId = m_fpComplexSurfaceSingleTextureWithFog;
			}
		}
		else if (m_rpPassCount == 3) {
			if (MultiPass.TMU[2].PolyFlags == PF_Highlighted) {
				if (OneXBlending) {
					fpId = m_fpComplexSurfaceDualTextureModulatedWithFog;
				}
				else {
					fpId = m_fpComplexSurfaceDualTextureModulated2XWithFog;
				}
			}
		}
	}

	//Check if found a fragment program to use
	if (fpId == 0) {
		SetDisabledFragmentProgramState();
	}
	else {
		SetFragmentProgram(fpId);
	}

	i = 0;
	do {
		if (i != 0) {
			DWORD texBit;

			glActiveTextureARB(GL_TEXTURE0_ARB + i);

			texBit = 1 << i;
			if ((m_texEnableBits & texBit) == 0) {
				m_texEnableBits |= texBit;

				glEnable(GL_TEXTURE_2D);
			}

			//No TexEnv setup for fragment program
			if (fpId == 0) {
				SetTexEnv(i, MultiPass.TMU[i].PolyFlags);
			}
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);
		
		glVertexAttrib4fARB(i + 8, TexInfo[i].UPan, TexInfo[i].VPan, TexInfo[i].UMult, TexInfo[i].VMult);
	} while (++i < m_rpPassCount);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(m_rpPassCount);
	//Disable all client textures for this vertex program path
	DisableSubsequentClientTextures(0);

	return;
}

//Must be called with (m_rpPassCount > 0)
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	INT i;
	INT t;
	FLOAT NearZ  = 380.0f;
	FLOAT RNearZ = 1.0f / NearZ;

	//Two extra texture units used for detail texture
	m_rpPassCount += 2;

	SetDefaultFragmentProgramState();

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//Surface texture must be 2X blended
	//Also force PF_Modulated for the TexEnv stage
	MultiPass.TMU[0].PolyFlags |= (PF_Modulated | PF_FlatShaded);

	//Detail texture uses first two texture units
	//Other textures use last two texture units
	i = 2;
	do {
		if (i != 0) {
			DWORD texBit;

			glActiveTextureARB(GL_TEXTURE0_ARB + i);

			texBit = 1 << i;
			if ((m_texEnableBits & texBit) == 0) {
				m_texEnableBits |= texBit;

				glEnable(GL_TEXTURE_2D);
			}
			if ((m_clientTexEnableBits & texBit) == 0) {
				m_clientTexEnableBits |= texBit;

				glClientActiveTextureARB(GL_TEXTURE0_ARB + i);
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}

			SetTexEnv(i, MultiPass.TMU[i - 2].PolyFlags);
		}

		SetTexture(i, *MultiPass.TMU[i - 2].Info, MultiPass.TMU[i - 2].PolyFlags, MultiPass.TMU[i - 2].PanBias);
	} while (++i < m_rpPassCount);

	glColor3fv(m_detailTextureColor3f_1f);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	SetAlphaTexture(0);
	{
		DWORD texBit = 1 << 0;
		if ((m_texEnableBits & texBit) == 0) {
			m_texEnableBits |= texBit;

			glEnable(GL_TEXTURE_2D);
		}
		if ((m_clientTexEnableBits & texBit) == 0) {
			m_clientTexEnableBits |= texBit;

			glClientActiveTextureARB(GL_TEXTURE0_ARB);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	glActiveTextureARB(GL_TEXTURE1_ARB);
	SetTexEnv(1, PF_Memorized);
	SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);
	{
		DWORD texBit = 1 << 1;
		if ((m_texEnableBits & texBit) == 0) {
			m_texEnableBits |= texBit;

			glEnable(GL_TEXTURE_2D);
		}
		if ((m_clientTexEnableBits & texBit) == 0) {
			m_clientTexEnableBits |= texBit;

			glClientActiveTextureARB(GL_TEXTURE1_ARB);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
	}

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(m_rpPassCount);
	DisableSubsequentClientTextures(m_rpPassCount);

	//Alpha texture for detail texture uses texture unit 0
	{
		INT t = 0;
		const FGLVertex *pVertex = &VertexArray[0];
		FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

		INT ptCounter = m_csPtCount;
		do {
			pTexCoord->u = pVertex->z * RNearZ;
			pTexCoord->v = 0.5f;

			pVertex++;
			pTexCoord++;
		} while (--ptCounter != 0);
	}
	//Detail texture uses texture unit 1
	//Remaining 1 or 2 textures use texture units 2 and 3
	if (UseSSE) {
#ifdef UTGLR_INCLUDE_SSE_CODE
		t = 1;
		do {
			__m128 uvPan;
			__m128 uvMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			uvPan = _mm_setzero_ps();
			uvMult = _mm_setzero_ps();
			uvPan = _mm_loadl_pi(uvPan, (const __m64 *)&TexInfo[t].UPan);
			uvMult = _mm_loadl_pi(uvMult, (const __m64 *)&TexInfo[t].UMult);
			uvPan = _mm_movelh_ps(uvPan, uvPan);
			uvMult = _mm_movelh_ps(uvMult, uvMult);

			INT ptCounter = m_csPtCount;
			do {
				__m128 data;

				data = _mm_load_ps((const float *)pMapDot);
				data = _mm_sub_ps(data, uvPan);
				data = _mm_mul_ps(data, uvMult);
				_mm_store_ps((float *)pTexCoord, data);

				pMapDot += 2;
				pTexCoord += 2;
			} while ((ptCounter -= 2) > 0);
		} while (++t < m_rpPassCount);
#endif //UTGLR_INCLUDE_SSE_CODE
	}
	else {
		t = 1;
		do {
			FLOAT UPan = TexInfo[t].UPan;
			FLOAT VPan = TexInfo[t].VPan;
			FLOAT UMult = TexInfo[t].UMult;
			FLOAT VMult = TexInfo[t].VMult;
			const FGLMapDot *pMapDot = &MapDotArray[0];
			FGLTexCoord *pTexCoord = &TexCoordArray[t][0];

			INT ptCounter = m_csPtCount;
			do {
				pTexCoord->u = (pMapDot->u - UPan) * UMult;
				pTexCoord->v = (pMapDot->v - VPan) * VMult;

				pMapDot++;
				pTexCoord++;
			} while (--ptCounter != 0);
		} while (++t < m_rpPassCount);
	}

	return;
}

//Must be called with (m_rpPassCount > 0)
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_VP(FTextureInfo &DetailTextureInfo) {
	INT i;

	//Two extra texture units used for detail texture
	m_rpPassCount += 2;

	SetDefaultFragmentProgramState();

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//Surface texture must be 2X blended
	//Also force PF_Modulated for the TexEnv stage
	MultiPass.TMU[0].PolyFlags |= (PF_Modulated | PF_FlatShaded);

	if (m_rpPassCount == 3) {
		SetVertexProgram(m_vpComplexSurfaceSingleTextureAndDetailTexture);
	}
	else {
		SetVertexProgram(m_vpComplexSurfaceDualTextureAndDetailTexture);
	}

	//Detail texture uses first two texture units
	//Other textures use last two texture units
	i = 2;
	do {
		if (i != 0) {
			DWORD texBit;

			glActiveTextureARB(GL_TEXTURE0_ARB + i);

			texBit = 1 << i;
			if ((m_texEnableBits & texBit) == 0) {
				m_texEnableBits |= texBit;

				glEnable(GL_TEXTURE_2D);
			}

			SetTexEnv(i, MultiPass.TMU[i - 2].PolyFlags);
		}

		SetTexture(i, *MultiPass.TMU[i - 2].Info, MultiPass.TMU[i - 2].PolyFlags, MultiPass.TMU[i - 2].PanBias);

		glVertexAttrib4fARB(i + 8, TexInfo[i].UPan, TexInfo[i].VPan, TexInfo[i].UMult, TexInfo[i].VMult);
	} while (++i < m_rpPassCount);

	glColor3fv(m_detailTextureColor3f_1f);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	SetAlphaTexture(0);
	{
		DWORD texBit = 1 << 0;
		if ((m_texEnableBits & texBit) == 0) {
			m_texEnableBits |= texBit;

			glEnable(GL_TEXTURE_2D);
		}
	}

	glActiveTextureARB(GL_TEXTURE1_ARB);
	SetTexEnv(1, PF_Memorized);
	SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);
	{
		DWORD texBit = 1 << 1;
		if ((m_texEnableBits & texBit) == 0) {
			m_texEnableBits |= texBit;

			glEnable(GL_TEXTURE_2D);
		}
	}
	glVertexAttrib4fARB(9, TexInfo[1].UPan, TexInfo[1].VPan, TexInfo[1].UMult, TexInfo[1].VMult);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(m_rpPassCount);
	//Disable all client textures for this vertex program path
	DisableSubsequentClientTextures(0);

	return;
}

//Must be called with (m_rpPassCount > 0)
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	INT i;
	DWORD detailTexUnit;

	//One extra texture unit used for detail texture
	m_rpPassCount += 1;

	//Detail texture is in the last texture unit
	detailTexUnit = (m_rpPassCount - 1);

	if (m_rpPassCount == 2) {
		SetVertexProgram(m_vpComplexSurfaceDualTextureWithPos);
	}
	else {
		SetVertexProgram(m_vpComplexSurfaceTripleTextureWithPos);
	}
	if (DetailMax >= 2) {
		if (m_rpPassCount == 2) {
			SetFragmentProgram(m_fpSingleTextureAndDetailTextureTwoLayer);
		}
		else {
			SetFragmentProgram(m_fpDualTextureAndDetailTextureTwoLayer);
		}
	}
	else {
		if (m_rpPassCount == 2) {
			SetFragmentProgram(m_fpSingleTextureAndDetailTexture);
		}
		else {
			SetFragmentProgram(m_fpDualTextureAndDetailTexture);
		}
	}

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//First one or two textures in first two texture units
	i = 0;
	do {
		if (i != 0) {
			DWORD texBit;

			glActiveTextureARB(GL_TEXTURE0_ARB + i);

			texBit = 1 << i;
			if ((m_texEnableBits & texBit) == 0) {
				m_texEnableBits |= texBit;

				glEnable(GL_TEXTURE_2D);
			}

			//No TexEnv setup for fragment program
			//Only works with modulated
		}

		SetTexture(i, *MultiPass.TMU[i].Info, MultiPass.TMU[i].PolyFlags, MultiPass.TMU[i].PanBias);

		glVertexAttrib4fARB(i + 8, TexInfo[i].UPan, TexInfo[i].VPan, TexInfo[i].UMult, TexInfo[i].VMult);
	} while (++i < detailTexUnit);

	m_detailTextureColor3f_1f[3] = (OneXBlending) ? 0.0f : 1.0f;
	glColor4fv(m_detailTextureColor3f_1f);

	//Detail texture in second or third texture unit
	glActiveTextureARB(GL_TEXTURE0_ARB + detailTexUnit);
	//No TexEnv to set in fragment program mode
	SetTextureNoPanBias(detailTexUnit, DetailTextureInfo, PF_Modulated);
	{
		DWORD texBit = 1 << detailTexUnit;
		if ((m_texEnableBits & texBit) == 0) {
			m_texEnableBits |= texBit;

			glEnable(GL_TEXTURE_2D);
		}
	}
	glVertexAttrib4fARB(8 + detailTexUnit, TexInfo[detailTexUnit].UPan, TexInfo[detailTexUnit].VPan, TexInfo[detailTexUnit].UMult, TexInfo[detailTexUnit].VMult);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(m_rpPassCount);
	//Disable all client textures for this fragment program path
	DisableSubsequentClientTextures(0);

	return;
}

//Modified this routine to always set up detail texture state
//It should only be called if at least one polygon will be detail textured
void UOpenGLRenderDevice::DrawDetailTexture(FTextureInfo &DetailTextureInfo, INT BaseClipIndex, bool clipDetailTexture) {
	//Setup detail texture state
	SetBlend(PF_Modulated);

	//Set detail alpha mode flag
	bool detailAlphaMode = ((clipDetailTexture == false) && UseDetailAlpha) ? true : false;

	//Enable client texture zero if it is disabled
	if ((m_clientTexEnableBits & 0x1) == 0) {
		m_clientTexEnableBits |= 0x1;

		if (SUPPORTS_GL_ARB_multitexture) {
			glClientActiveTextureARB(GL_TEXTURE0_ARB);
		}
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if (detailAlphaMode) {
		glColor3fv(m_detailTextureColor3f_1f);

		glActiveTextureARB(GL_TEXTURE0_ARB);
		SetAlphaTexture(0);
		//TexEnv 0 is PF_Modulated by default

		glActiveTextureARB(GL_TEXTURE1_ARB);
		if ((m_texEnableBits & 0x2) == 0) {
			m_texEnableBits |= 0x2;

			glEnable(GL_TEXTURE_2D);
		}
		if ((m_clientTexEnableBits & 0x2) == 0) {
			m_clientTexEnableBits |= 0x2;

			glClientActiveTextureARB(GL_TEXTURE1_ARB);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		}
		SetTexEnv(1, PF_Memorized);
		SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);

		//Check for additional enabled texture units that should be disabled
		DisableSubsequentTextures(2);
		DisableSubsequentClientTextures(2);
	}
	else {
		if (SUPPORTS_GL_ARB_multitexture) {
			glActiveTextureARB(GL_TEXTURE0_ARB);
		}
		SetTexEnv(0, PF_Memorized);
		SetTextureNoPanBias(0, DetailTextureInfo, PF_Modulated);

		glEnableClientState(GL_COLOR_ARRAY);
		if (clipDetailTexture == true) {
			glEnable(GL_POLYGON_OFFSET_FILL);
		}

		//Check for additional enabled texture units that should be disabled
		DisableSubsequentTextures(1);
		DisableSubsequentClientTextures(1);
	}


	//Get detail texture color
	DWORD detailColor = m_detailTextureColor4ub;

	INT detailPassNum = 0;
	FLOAT NearZ  = 380.0f;
	FLOAT RNearZ = 1.0f / NearZ;
	FLOAT DetailScale = 1.0f;
	do {
		//Set up new NearZ and rescan points if subsequent pass
		if (detailPassNum > 0) {
			//Adjust NearZ and detail texture scaling
			NearZ /= 4.223f;
			RNearZ *= 4.223f;
			DetailScale *= 4.223f;

			//Rescan points
			(this->*m_pBufferDetailTextureDataProc)(NearZ);
		}

		//Calculate scaled UMult and VMult for detail texture based on mode
		FLOAT DetailUMult;
		FLOAT DetailVMult;
		if (detailAlphaMode) {
			DetailUMult = TexInfo[1].UMult * DetailScale;
			DetailVMult = TexInfo[1].VMult * DetailScale;
		}
		else {
			DetailUMult = TexInfo[0].UMult * DetailScale;
			DetailVMult = TexInfo[0].VMult * DetailScale;
		}

		INT Index = 0;
		INT NextClipIndex = BaseClipIndex;

		INT *pNumPts = &MultiDrawCountArray[0];
		DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
		for (DWORD PolyNum = 0; PolyNum < m_csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
			DWORD NumPts = *pNumPts;
			DWORD isNearBits = *pDetailTextureIsNear;

			//Skip the polygon if it will not be detail textured
			if (isNearBits == 0) {
				Index += NumPts;
				continue;
			}
			INT StartIndex = Index;

			DWORD allPtsBits = ~(~0U << NumPts);
			//Detail alpha mode
			if (detailAlphaMode) {
				for (INT i = 0; i < NumPts; i++) {
					const FGLVertex &Point = VertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					FGLTexCoord *pTexCoord0 = TexCoordArray[0];
					FGLTexCoord *pTexCoord1 = TexCoordArray[1];
					FLOAT PointZ_Times_RNearZ = Point.z * RNearZ;
					pTexCoord0[Index].u = PointZ_Times_RNearZ;
					pTexCoord0[Index].v = 0.5f;
					pTexCoord1[Index].u = (U - TexInfo[1].UPan) * DetailUMult;
					pTexCoord1[Index].v = (V - TexInfo[1].VPan) * DetailVMult;

					Index++;
				}

				glDrawArrays(GL_TRIANGLE_FAN, StartIndex, NumPts);
			}
			//Otherwise, no clipping required, or clipping required, but DetailClipping not enabled
			else if ((clipDetailTexture == false) || (isNearBits == allPtsBits)) {
				for (INT i = 0; i < NumPts; i++) {
					const FGLVertex &Point = VertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					FGLTexCoord *pTexCoord = TexCoordArray[0];
					pTexCoord[Index].u = (U - TexInfo[0].UPan) * DetailUMult;
					pTexCoord[Index].v = (V - TexInfo[0].VPan) * DetailVMult;
					DWORD alpha = appRound((1.0f - (Clamp(Point.z, 0.0f, NearZ) * RNearZ)) * 255.0f);
					SingleColorArray[Index].color = detailColor | (alpha << 24);

					Index++;
				}

				glDrawArrays(GL_TRIANGLE_FAN, StartIndex, NumPts);
			}
			//Otherwise, clipping required and DetailClipping enabled
			else {
				DWORD NextIndex = 0;
				GLuint IndexList[64];
				DWORD isNear_i_bit = 1U << (NumPts - 1);
				DWORD isNear_j_bit = 1U;
				for (INT i = 0, j = NumPts - 1; i < NumPts; j = i++, isNear_j_bit = isNear_i_bit, isNear_i_bit >>= 1) {
					const FGLVertex &Point = VertexArray[Index];
					FLOAT U = MapDotArray[Index].u;
					FLOAT V = MapDotArray[Index].v;

					if (((isNear_i_bit & isNearBits) != 0) && ((isNear_j_bit & isNearBits) == 0)) {
						const FGLVertex &PrevPoint = VertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = PrevPoint.z - Point.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - Point.z) / dist;
						}
						FGLVertex *pVertex = &VertexArray[NextClipIndex];
						FGLTexCoord *pTexCoord = &TexCoordArray[0][NextClipIndex];
						pVertex->x = (m * (PrevPoint.x - Point.x)) + Point.x;
						pVertex->y = (m * (PrevPoint.y - Point.y)) + Point.y;
						pVertex->z = NearZ;
						pTexCoord->u = ((m * (PrevU - U)) + U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord->v = ((m * (PrevV - V)) + V - TexInfo[0].VPan) * DetailVMult;
						DWORD alpha = 0;
						SingleColorArray[NextClipIndex].color = detailColor | (alpha << 24);
						IndexList[NextIndex++] = NextClipIndex++;
					}

					if ((isNear_i_bit & isNearBits) != 0) {
						FGLTexCoord *pTexCoord = &TexCoordArray[0][Index];
						pTexCoord->u = (U - TexInfo[0].UPan) * DetailUMult;
						pTexCoord->v = (V - TexInfo[0].VPan) * DetailVMult;
						DWORD alpha = appRound((1.0f - (Clamp(Point.z, 0.0f, NearZ) * RNearZ)) * 255.0f);
						SingleColorArray[Index].color = detailColor | (alpha << 24);
						IndexList[NextIndex++] = Index;
					}

					if (((isNear_i_bit & isNearBits) == 0) && ((isNear_j_bit & isNearBits) != 0)) {
						const FGLVertex &PrevPoint = VertexArray[StartIndex + j];
						FLOAT PrevU = MapDotArray[StartIndex + j].u;
						FLOAT PrevV = MapDotArray[StartIndex + j].v;

						FLOAT dist = Point.z - PrevPoint.z;
						FLOAT m = 1.0f;
						if (dist > 0.001f) {
							m = (NearZ - PrevPoint.z) / dist;
						}
						FGLVertex *pVertex = &VertexArray[NextClipIndex];
						FGLTexCoord *pTexCoord = &TexCoordArray[0][NextClipIndex];
						pVertex->x = (m * (Point.x - PrevPoint.x)) + PrevPoint.x;
						pVertex->y = (m * (Point.y - PrevPoint.y)) + PrevPoint.y;
						pVertex->z = NearZ;
						pTexCoord->u = ((m * (U - PrevU)) + PrevU - TexInfo[0].UPan) * DetailUMult;
						pTexCoord->v = ((m * (V - PrevV)) + PrevV - TexInfo[0].VPan) * DetailVMult;
						DWORD alpha = 0;
						SingleColorArray[NextClipIndex].color = detailColor | (alpha << 24);
						IndexList[NextIndex++] = NextClipIndex++;
					}
					Index++;
				}

				glDrawElements(GL_TRIANGLE_FAN, NextIndex, GL_UNSIGNED_INT, IndexList);
			}
		}
	} while (++detailPassNum < DetailMax);


	//Clear detail texture state
	if (SUPPORTS_GL_ARB_multitexture) {
		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	if (detailAlphaMode) {
		//TexEnv 0 was left in default state of PF_Modulated
	}
	else {
		SetTexEnv(0, PF_Modulated);

		glDisableClientState(GL_COLOR_ARRAY);
		if (clipDetailTexture == true) {
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
	}

	return;
}

//Modified this routine to always set up detail texture state
//It should only be called if at least one polygon will be detail textured
void UOpenGLRenderDevice::DrawDetailTexture_VP(FTextureInfo &DetailTextureInfo) {
	INT Index = 0;

	//Setup detail texture state
	SetBlend(PF_Modulated);

	SetVertexProgram(m_vpComplexSurfaceDetailAlpha);

	glColor3fv(m_detailTextureColor3f_1f);

	glActiveTextureARB(GL_TEXTURE0_ARB);
	SetAlphaTexture(0);
	//TexEnv 0 is PF_Modulated by default

	glActiveTextureARB(GL_TEXTURE1_ARB);
	SetTexEnv(1, PF_Memorized);
	SetTextureNoPanBias(1, DetailTextureInfo, PF_Modulated);
	if ((m_texEnableBits & 0x2) == 0) {
		m_texEnableBits |= 0x2;

		glEnable(GL_TEXTURE_2D);
	}
	glVertexAttrib4fARB(9, TexInfo[1].UPan, TexInfo[1].VPan, TexInfo[1].UMult, TexInfo[1].VMult);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(2);
	DisableSubsequentClientTextures(0);


	INT *pNumPts = &MultiDrawCountArray[0];
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD csPolyCount = m_csPolyCount;
	for (DWORD PolyNum = 0; PolyNum < csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
		DWORD NumPts = *pNumPts;
		DWORD isNearBits = *pDetailTextureIsNear;

		//Skip the polygon if it will not be detail textured
		if (isNearBits == 0) {
			Index += NumPts;
			continue;
		}

		glDrawArrays(GL_TRIANGLE_FAN, Index, NumPts);
		Index += NumPts;
	}


	//Clear detail texture state
	glActiveTextureARB(GL_TEXTURE0_ARB);
	//TexEnv 0 was left in default state of PF_Modulated

	return;
}

//Modified this routine to always set up detail texture state
//It should only be called if at least one polygon will be detail textured
void UOpenGLRenderDevice::DrawDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	INT Index = 0;

	//Setup detail texture state
	SetBlend(PF_Modulated);

	SetVertexProgram(m_vpComplexSurfaceSingleTextureWithPos);
	if (DetailMax >= 2) {
		SetFragmentProgram(m_fpDetailTextureTwoLayer);
	}
	else {
		SetFragmentProgram(m_fpDetailTexture);
	}

	glColor3fv(m_detailTextureColor3f_1f);

	if (SUPPORTS_GL_ARB_multitexture) {
		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	//No TexEnv to set in fragment program mode
	SetTextureNoPanBias(0, DetailTextureInfo, PF_Modulated);
	glVertexAttrib4fARB(8, TexInfo[0].UPan, TexInfo[0].VPan, TexInfo[0].UMult, TexInfo[0].VMult);

	//Check for additional enabled texture units that should be disabled
	DisableSubsequentTextures(1);
	DisableSubsequentClientTextures(0);


	INT *pNumPts = &MultiDrawCountArray[0];
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD csPolyCount = m_csPolyCount;
	for (DWORD PolyNum = 0; PolyNum < csPolyCount; PolyNum++, pNumPts++, pDetailTextureIsNear++) {
		DWORD NumPts = *pNumPts;
		DWORD isNearBits = *pDetailTextureIsNear;

		//Skip the polygon if it will not be detail textured
		if (isNearBits == 0) {
			Index += NumPts;
			continue;
		}

		glDrawArrays(GL_TRIANGLE_FAN, Index, NumPts);
		Index += NumPts;
	}


	//Clear detail texture state
	if (SUPPORTS_GL_ARB_multitexture) {
		glActiveTextureARB(GL_TEXTURE0_ARB);
	}
	//TexEnv 0 was left in default state of PF_Modulated

	return;
}

INT UOpenGLRenderDevice::BufferStaticComplexSurfaceGeometry(const FSurfaceFacet& Facet) {
	INT Index = 0;

	// Buffer "static" geometry.
	m_csPolyCount = 0;
	FGLMapDot *pMapDot = &MapDotArray[0];
	FGLVertex *pVertex = &VertexArray[0];
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
		//Skip if no points
		INT NumPts = Poly->NumPts;
		if (NumPts <= 0) {
			continue;
		}

		DWORD csPolyCount = m_csPolyCount;
		MultiDrawFirstArray[csPolyCount] = Index;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		Index += NumPts;
		if (Index > VERTEX_ARRAY_SIZE) {
			return 0;
		}
		FTransform **pPts = &Poly->Pts[0];
		do {
			const FVector &Point = (*pPts++)->Point;

			pMapDot->u = (Facet.MapCoords.XAxis | Point) - m_csUDot;
			pMapDot->v = (Facet.MapCoords.YAxis | Point) - m_csVDot;
			pMapDot++;

			pVertex->x = Point.X;
			pVertex->y = Point.Y;
			pVertex->z = Point.Z;
			pVertex++;
		} while (--NumPts != 0);
	}

	return Index;
}

INT UOpenGLRenderDevice::BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet& Facet) {
	INT Index = 0;

	// Buffer "static" geometry.
	m_csPolyCount = 0;
	FGLVertex *pVertex = &VertexArray[0];
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
		//Skip if no points
		INT NumPts = Poly->NumPts;
		if (NumPts <= 0) {
			continue;
		}

		DWORD csPolyCount = m_csPolyCount;
		MultiDrawFirstArray[csPolyCount] = Index;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		Index += NumPts;
		if (Index > VERTEX_ARRAY_SIZE) {
			return 0;
		}
		FTransform **pPts = &Poly->Pts[0];
		do {
			const FVector &Point = (*pPts++)->Point;

			pVertex->x = Point.X;
			pVertex->y = Point.Y;
			pVertex->z = Point.Z;
			pVertex++;
		} while (--NumPts != 0);
	}

	return Index;
}

DWORD UOpenGLRenderDevice::BufferDetailTextureData(FLOAT NearZ) {
	DWORD *pDetailTextureIsNear = DetailTextureIsNearArray;
	DWORD anyIsNearBits = 0;

	FGLVertex *pVertex = &VertexArray[0];
	INT *pNumPts = &MultiDrawCountArray[0];
	DWORD csPolyCount = m_csPolyCount;
	do {
		INT NumPts = *pNumPts++;
		DWORD isNear = 0;

		do {
			isNear <<= 1;
			if (pVertex->z < NearZ) {
				isNear |= 1;
			}
			pVertex++;
		} while (--NumPts != 0);

		*pDetailTextureIsNear++ = isNear;
		anyIsNearBits |= isNear;
	} while (--csPolyCount != 0);

	return anyIsNearBits;
}

#ifdef UTGLR_INCLUDE_SSE_CODE
__declspec(naked) DWORD UOpenGLRenderDevice::BufferDetailTextureData_SSE2(FLOAT NearZ) {
	__asm {
		movd xmm0, [esp+4]

		push esi
		push edi

		mov esi, [ecx]this.VertexArray
		lea edx, [ecx]this.MultiDrawCountArray
		lea edi, [ecx]this.DetailTextureIsNearArray

		pxor xmm1, xmm1

		mov ecx, [ecx]this.m_csPolyCount

		poly_count_loop:
			mov eax, [edx]
			add edx, 4

			pxor xmm2, xmm2

			num_pts_loop:
				movss xmm3, [esi+8]
				add esi, TYPE FGLVertex

				pslld xmm2, 1

				cmpltss xmm3, xmm0
				psrld xmm3, 31

				por xmm2, xmm3

				dec eax
				jne num_pts_loop

			movd [edi], xmm2
			add edi, 4

			por xmm1, xmm2

			dec ecx
			jne poly_count_loop

		movd eax, xmm1

		pop edi
		pop esi

		ret 4
	}
}
#endif //UTGLR_INCLUDE_SSE_CODE
	
void UOpenGLRenderDevice::EndGouraudPolygonBufferingNoCheck(void) {
	SetDefaultAAState();
	//EndGouraudPolygonBufferingNoCheck sets its own projection state
	//EndGouraudPolygonBufferingNoCheck sets its own color state
	//Vertex program state set when start buffering
	//Fragment program state set when start buffering
	//Default texture state set when start buffering

	clock(GouraudCycles);

	//Set projection state
	SetProjectionState(m_requestNearZRangeHackProjection);

	//Set color state
	SetColorState();

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

	// Actually render the triangles.
	glDrawArrays(GL_TRIANGLES, 0, BufferedVerts);

#ifdef UTGLR_DEBUG_ACTOR_WIREFRAME
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

	BufferedVerts = 0;

	unclock(GouraudCycles);
}

void UOpenGLRenderDevice::EndTileBufferingNoCheck(void) {
	if (NoAATiles) {
		SetDisabledAAState();
	}
	else {
		SetDefaultAAState();
	}
	SetDefaultProjectionState();
	//EndTileBufferingNoCheck sets its own color state
	//Vertex program state set when start buffering
	//Fragment program state set when start buffering
	//Default texture state set when start buffering

	clock(TileCycles);

	//Set color state
	SetColorState();

	//Draw the quads
	glDrawArrays(GL_QUADS, 0, BufferedTileVerts);

	BufferedTileVerts = 0;

	unclock(TileCycles);
}


// Static variables.
INT		UOpenGLRenderDevice::NumDevices		= 0;
INT		UOpenGLRenderDevice::LockCount		= 0;
#ifdef WIN32
HGLRC		UOpenGLRenderDevice::hCurrentRC		= NULL;
HMODULE		UOpenGLRenderDevice::hModuleGlMain	= NULL;
HMODULE		UOpenGLRenderDevice::hModuleGlGdi	= NULL;
TArray<HGLRC>	UOpenGLRenderDevice::AllContexts;
#else
UBOOL 		UOpenGLRenderDevice::GLLoaded	= false;
#endif

bool UOpenGLRenderDevice::g_gammaFirstTime = false;
bool UOpenGLRenderDevice::g_haveOriginalGammaRamp = false;
#ifdef WIN32
UOpenGLRenderDevice::FGammaRamp UOpenGLRenderDevice::g_originalGammaRamp;
#endif

UOpenGLRenderDevice::DWORD_CTTree_t UOpenGLRenderDevice::m_sharedZeroPrefixBindTrees[NUM_CTTree_TREES];
UOpenGLRenderDevice::QWORD_CTTree_t UOpenGLRenderDevice::m_sharedNonZeroPrefixBindTrees[NUM_CTTree_TREES];
CCachedTextureChain UOpenGLRenderDevice::m_sharedNonZeroPrefixBindChain;
UOpenGLRenderDevice::QWORD_CTTree_NodePool_t UOpenGLRenderDevice::m_sharedNonZeroPrefixTexIdPool;
UOpenGLRenderDevice::TexPoolMap_t UOpenGLRenderDevice::m_sharedRGBA8TexPool;

// OpenGL function pointers.
#define GL_EXT(name) bool UOpenGLRenderDevice::SUPPORTS##name = 0;
#define GL_PROC(ext,ret,func,parms) ret (STDCALL *UOpenGLRenderDevice::func)parms;
#ifdef UTGLR_ALL_GL_PROCS
#define GL_PROX(ext,ret,func,parms) ret (STDCALL *UOpenGLRenderDevice::func)parms;
#else
#define GL_PROX(ext,ret,func,parms)
#endif
#include "OpenGLFuncs.h"
#undef GL_EXT
#undef GL_PROC
#undef GL_PROX

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
