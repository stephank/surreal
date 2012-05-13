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
	* Various modifications and additions by Smirftsch / Oldunreal


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

#include "OpenGL.h"


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

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

static const char *g_vpDefaultRenderingStateWithLinearFog =
	"!!ARBvp1.0\n"
	"OPTION ARB_position_invariant;\n"

	"MOV result.color, vertex.color;\n"
	"MOV result.color.secondary, vertex.color.secondary;\n"
	"MOV result.texcoord[0], vertex.texcoord[0];\n"
	"MOV result.fogcoord.x, vertex.position.z;\n"

	"END\n"
;

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

static const char *g_fpComplexSurfaceSingleTexture =
	"!!ARBfp1.0\n"

	"ATTRIB iTC0 = fragment.texcoord[0];\n"

	"TEX result.color, iTC0, texture[0], 2D;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceDualTextureModulated =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MOV result.color, t0;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceTripleTextureModulated =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MUL t0, t0, t2;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MOV result.color, t0;\n"

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

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"SUB t2.a, 1.0, t2.a;\n"
	"MAD t0.rgb, t0, t2.aaaa, t2;\n"

	"MOV result.color, t0;\n"

	"END\n"
;

static const char *g_fpComplexSurfaceTripleTextureModulatedWithFog =
	"!!ARBfp1.0\n"

	"ATTRIB iColor = fragment.color.primary;\n"
	"ATTRIB iTC0 = fragment.texcoord[0];\n"
	"ATTRIB iTC1 = fragment.texcoord[1];\n"
	"ATTRIB iTC2 = fragment.texcoord[2];\n"
	"ATTRIB iTC3 = fragment.texcoord[3];\n"
	"TEMP t0;\n"
	"TEMP t1;\n"
	"TEMP t2;\n"
	"TEMP t3;\n"

	"TEX t0, iTC0, texture[0], 2D;\n"
	"TEX t1, iTC1, texture[1], 2D;\n"
	"TEX t2, iTC2, texture[2], 2D;\n"
	"TEX t3, iTC3, texture[3], 2D;\n"

	"MUL t0, t0, t1;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"MUL t0, t0, t2;\n"
	"MAD t0.rgb, t0, iColor.aaaa, t0;\n"

	"SUB t3.a, 1.0, t3.a;\n"
	"MAD t0.rgb, t0, t3.aaaa, t3;\n"

	"MOV result.color, t0;\n"

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


int UOpenGLRenderDevice::dbgPrintf(const char *format, ...) {
	const unsigned int DBG_PRINT_BUF_SIZE = 1024;
	char dbgPrintBuf[DBG_PRINT_BUF_SIZE];
	va_list vaArgs;
	int iRet = 0;

	va_start(vaArgs, format);

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4996)
	iRet = vsnprintf(dbgPrintBuf, DBG_PRINT_BUF_SIZE, format, vaArgs);
	dbgPrintBuf[DBG_PRINT_BUF_SIZE - 1] = '\0';
#pragma warning(pop)

	TCHAR_CALL_OS(OutputDebugStringW(appFromAnsi(dbgPrintBuf)), OutputDebugStringA(dbgPrintBuf));
#endif

	va_end(vaArgs);

	return iRet;
}


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
	SC_AddFloatConfigParam(TEXT("LODBias"), CPP_PROPERTY_LOCAL(LODBias), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffset"), CPP_PROPERTY_LOCAL(GammaOffset), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetRed"), CPP_PROPERTY_LOCAL(GammaOffsetRed), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetGreen"), CPP_PROPERTY_LOCAL(GammaOffsetGreen), 0.0f);
	SC_AddFloatConfigParam(TEXT("GammaOffsetBlue"), CPP_PROPERTY_LOCAL(GammaOffsetBlue), 0.0f);
	SC_AddBoolConfigParam(1,  TEXT("GammaCorrectScreenshots"), CPP_PROPERTY_LOCAL(GammaCorrectScreenshots), 0);
	SC_AddBoolConfigParam(0,  TEXT("OneXBlending"), CPP_PROPERTY_LOCAL(OneXBlending), UTGLR_DEFAULT_OneXBlending);
	SC_AddIntConfigParam(TEXT("MinLogTextureSize"), CPP_PROPERTY_LOCAL(MinLogTextureSize), 0);
	SC_AddIntConfigParam(TEXT("MaxLogTextureSize"), CPP_PROPERTY_LOCAL(MaxLogTextureSize), 12);
	SC_AddBoolConfigParam(9,  TEXT("UseZTrick"), CPP_PROPERTY_LOCAL(UseZTrick), 0);
	SC_AddBoolConfigParam(8,  TEXT("UseBGRATextures"), CPP_PROPERTY_LOCAL(UseBGRATextures), 1);
	SC_AddBoolConfigParam(7,  TEXT("UseMultiTexture"), CPP_PROPERTY_LOCAL(UseMultiTexture), 1);
	SC_AddBoolConfigParam(6,  TEXT("UsePalette"), CPP_PROPERTY_LOCAL(UsePalette), 0);
	SC_AddBoolConfigParam(5,  TEXT("ShareLists"), CPP_PROPERTY_LOCAL(ShareLists), 0);
	SC_AddBoolConfigParam(4,  TEXT("UsePrecache"), CPP_PROPERTY_LOCAL(UsePrecache), 0);
	SC_AddBoolConfigParam(3,  TEXT("UseTrilinear"), CPP_PROPERTY_LOCAL(UseTrilinear), 0);
	SC_AddBoolConfigParam(2,  TEXT("UseAlphaPalette"), CPP_PROPERTY_LOCAL(UseAlphaPalette), 0);
	SC_AddBoolConfigParam(1,  TEXT("UseS3TC"), CPP_PROPERTY_LOCAL(UseS3TC), UTGLR_DEFAULT_UseS3TC);
	SC_AddBoolConfigParam(0,  TEXT("Use16BitTextures"), CPP_PROPERTY_LOCAL(Use16BitTextures), 0);
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
	SC_AddBoolConfigParam(0,  TEXT("CacheStaticMaps"), CPP_PROPERTY_LOCAL(CacheStaticMaps), 1);
	SC_AddIntConfigParam(TEXT("DynamicTexIdRecycleLevel"), CPP_PROPERTY_LOCAL(DynamicTexIdRecycleLevel), 100);
	SC_AddBoolConfigParam(2,  TEXT("TexDXT1ToDXT3"), CPP_PROPERTY_LOCAL(TexDXT1ToDXT3), 0);
	SC_AddBoolConfigParam(1,  TEXT("UseMultiDrawArrays"), CPP_PROPERTY_LOCAL(UseMultiDrawArrays), 0);
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

	//Mark shader names as not allocated
	m_allocatedShaderNames = false;

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


void UOpenGLRenderDevice::DbgPrintInitParam(const char *pName, INT value) {
	dbgPrintf("utglr: %s = %d\n", pName, value);
	return;
}

void UOpenGLRenderDevice::DbgPrintInitParam(const char *pName, FLOAT value) {
	dbgPrintf("utglr: %s = %f\n", pName, value);
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

	return;
}

void UOpenGLRenderDevice::ShutdownFrameRateLimitTimer(void) {
	//Only shutdown once
	if (!m_frameRateLimitTimerInitialized) {
		return;
	}
	m_frameRateLimitTimerInitialized = false;

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

	float rcpRedGamma = 1.0f / (2.5f * redGamma);
	float rcpGreenGamma = 1.0f / (2.5f * greenGamma);
	float rcpBlueGamma = 1.0f / (2.5f * blueGamma);
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
		iValRed = (int)appRound((float)appPow(iVal / 255.0f, rcpRedGamma) * 65535.0f);
		iValGreen = (int)appRound((float)appPow(iVal / 255.0f, rcpGreenGamma) * 65535.0f);
		iValBlue = (int)appRound((float)appPow(iVal / 255.0f, rcpBlueGamma) * 65535.0f);

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

	float rcpRedGamma = 1.0f / (2.5f * redGamma);
	float rcpGreenGamma = 1.0f / (2.5f * greenGamma);
	float rcpBlueGamma = 1.0f / (2.5f * blueGamma);
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
		iValRed = (int)appRound((float)appPow(iVal / 255.0f, rcpRedGamma) * 255.0f);
		iValGreen = (int)appRound((float)appPow(iVal / 255.0f, rcpGreenGamma) * 255.0f);
		iValBlue = (int)appRound((float)appPow(iVal / 255.0f, rcpBlueGamma) * 255.0f);

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

	SDL_SetWindowGammaRamp( GetWindow(), gammaRamp.red, gammaRamp.green, gammaRamp.blue );

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

bool UOpenGLRenderDevice::GetGL1Proc(void*& ProcAddress, const char *pName) {
	guard(UOpenGLRenderDevice::GetGL1Proc);

	ProcAddress = (void *)SDL_GL_GetProcAddress(pName);
	if (!ProcAddress) {
		debugf(TEXT("   Missing function '%s' for '%s' support"), appFromAnsi(pName), TEXT("OpenGL 1.x"));
		dbgPrintf("Missing function '%s' for '%s' support\n", pName, "OpenGL 1.x");
		return false;
	}

	return true;

	unguard;
}

bool UOpenGLRenderDevice::GetGL1Procs(void) {
	guard(UOpenGLRenderDevice::GetGL1Procs);

	bool loadOk = true;

	#define GL1_PROC(ret, func, params) loadOk &= GetGL1Proc(*(void**)&func, #func);
	#include "OpenGL1Funcs.h"
	#undef GL1_PROC

	return loadOk;

	unguard;
}

bool UOpenGLRenderDevice::FindGLExt(const char *pName) {
	guard(UOpenGLRenderDevice::FindGLExt);

	bool bRet = IsGLExtensionSupported((const char *)glGetString(GL_EXTENSIONS), pName);
	if (bRet) {
		debugf(NAME_Init, TEXT("Device supports: %s"), appFromAnsi(pName));
	}
	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: GL_EXT: %s = %d\n", pName, bRet);
	}

	return bRet;
	unguard;
}

void UOpenGLRenderDevice::GetGLExtProc(void*& ProcAddress, const char *pName, const char *pSupportName, bool& Supports) {
	guard(UOpenGLRenderDevice::GetGLExtProc);

	if (!Supports) {
		return;
	}

	ProcAddress = (void *)SDL_GL_GetProcAddress(pName);
	if (!ProcAddress) {
		Supports = false;

		debugf(TEXT("   Missing function '%s' for '%s' support"), appFromAnsi(pName), appFromAnsi(pSupportName));
		dbgPrintf("Missing function '%s' for '%s' support\n", pName, pSupportName);
	}

	unguard;
}

void UOpenGLRenderDevice::GetGLExtProcs(void) {
	guard(UOpenGLRenderDevice::GetGLExtProcs);
	#define GL_EXT_NAME(name) SUPPORTS##name = FindGLExt(#name + 1);
	#define GL_EXT_PROC(ext, ret, func, params) GetGLExtProc(*(void**)&func, #func, #ext + 1, SUPPORTS##ext);
	#include "OpenGLExtFuncs.h"
	#undef GL_EXT_NAME
	#undef GL_EXT_PROC
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
	check(SetContext() == 0);

	UnsetRes();
	SDL_GL_DeleteContext( Context );

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
	unguard;
}

void UOpenGLRenderDevice::ShutdownAfterError() {
	guard(UOpenGLRenderDevice::ShutdownAfterError);

	debugf(NAME_Exit, TEXT("UOpenGLRenderDevice::ShutdownAfterError"));

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: ShutdownAfterError\n");
	}

	//ChangeDisplaySettings(NULL, 0);

	unguard;
}


UBOOL UOpenGLRenderDevice::SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UOpenGLRenderDevice::SetRes);
	check(SetContext() == 0);

	unsigned int u;

	//Get debug bits
	{
		INT i = 0;
		if (!GConfig->GetInt(g_pSection, TEXT("DebugBits"), i)) i = 0;
		m_debugBits = i;
	}
	//Display debug bits
	if (DebugBit(DEBUG_BIT_ANY)) dbgPrintf("utglr: DebugBits = %u\n", m_debugBits);

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

	//Reset previous SwapBuffers status to okay
	m_prevSwapBuffersStatus = true;

	//Get OpenGL extension function pointers
	GetGLExtProcs();

	debugf(NAME_Init, TEXT("Depth bits: %u"), m_numDepthBits);
	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: Depth bits: %u\n", m_numDepthBits);
	if (m_usingAA) {
		debugf(NAME_Init, TEXT("AA samples: %u"), m_initNumAASamples);
		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: AA samples: %u\n", m_initNumAASamples);
	}

	// Set modelview.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glScalef(1.0f, -1.0f, -1.0f);

	//Get other defaults
	if (!GConfig->GetInt(g_pSection, TEXT("Brightness"), Brightness)) Brightness = 0;

	//Debug parameter listing
	if (DebugBit(DEBUG_BIT_BASIC)) {
		#define UTGLR_DEBUG_SHOW_PARAM_REG(name) DbgPrintInitParam(#name, name)
		#define UTGLR_DEBUG_SHOW_PARAM_DCV(name) DbgPrintInitParam(#name, DCV.name)

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
//		UTGLR_DEBUG_SHOW_PARAM_REG(UseVertexSpecular);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseAlphaPalette);
		UTGLR_DEBUG_SHOW_PARAM_REG(UseS3TC);
		UTGLR_DEBUG_SHOW_PARAM_REG(Use16BitTextures);
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
	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: UseSSE = %u\n", UseSSE);
	if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: UseSSE2 = %u\n", UseSSE2);

	SetGamma(Viewport->GetOuterUClient()->Brightness);

	//Restrict dynamic tex id recycle level range
	if (DynamicTexIdRecycleLevel < 10) DynamicTexIdRecycleLevel = 10;

	AlwaysMipmap = 0;
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


	//Reset vertex program and fragment program state tracking variables
	m_vpCurrent = 0;
	m_fpCurrent = 0;

	//Mark shader names as not allocated
	m_allocatedShaderNames = false;

	//Setup vertex programs and fragment programs
	if (UseFragmentProgram) {
		//Attempt to initialize vertex program and fragment program mode
		TryInitializeFragmentProgramMode();
	}


	if (MaxAnisotropy < 0) {
		MaxAnisotropy = 0;
	}
	if (MaxAnisotropy) {
		GLint iMaxAnisotropyLimit = 1;
		glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &iMaxAnisotropyLimit);
		debugf(TEXT("MaxAnisotropy: %i"), iMaxAnisotropyLimit);
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

	debugf(TEXT("MinLogTextureSize: %i"), MinLogTextureSize);
	debugf(TEXT("MaxLogTextureSize: %i"), MaxLogTextureSize);

	debugf(TEXT("BufferActorTris: %i"), BufferActorTris);

	debugf(TEXT("UseDetailAlpha: %i"), UseDetailAlpha);


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
	glAlphaFunc(GL_GREATER, 0.5f);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);
	glPolygonOffset(-1.0f, -1.0f);
	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);
	glEnable(GL_DITHER);

#ifdef UTGLR_RUNE_BUILD
	glFogi(GL_FOG_MODE, GL_LINEAR);
	glFogf(GL_FOG_START, 0.0f);
#endif
	m_gpFogEnabled = false;

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
	m_useAlphaToCoverageForMasked = false;
	m_alphaToCoverageEnabled = false;
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
	PL_MaxAnisotropy = MaxAnisotropy;
	PL_SmoothMaskedTextures = SmoothMaskedTextures;
	PL_MaskedTextureHack = MaskedTextureHack;
	PL_LODBias = LODBias;
	PL_UsePalette = UsePalette;
	PL_UseAlphaPalette = UseAlphaPalette;
	PL_UseDetailAlpha = UseDetailAlpha;
	PL_SinglePassDetail = SinglePassDetail;
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

	//Free vertex and fragment programs if they were allocated and leave vertex and fragment program modes if necessary
	ShutdownFragmentProgramMode();

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
	UTGLR_REFRESH_DCV(UseFragmentProgram);

	#undef UTGLR_REFRESH_DCV

	return;
}

void UOpenGLRenderDevice::ConfigValidate_RequiredExtensions(void) {
	if (!SUPPORTS_GL_ARB_vertex_program) UseFragmentProgram = 0;
	if (!SUPPORTS_GL_ARB_fragment_program) UseFragmentProgram = 0;
	if (!SUPPORTS_GL_EXT_bgra) UseBGRATextures = 0;
	if (!SUPPORTS_GL_EXT_multi_draw_arrays) UseMultiDrawArrays = 0;
	if (!SUPPORTS_GL_EXT_paletted_texture) UsePalette = 0;
	if (!SUPPORTS_GL_EXT_texture_env_combine) DetailTextures = 0;
	if (!SUPPORTS_GL_EXT_texture_env_combine) UseDetailAlpha = 0;
	if (!SUPPORTS_GL_EXT_texture_filter_anisotropic) MaxAnisotropy  = 0;
	if (!SUPPORTS_GL_EXT_texture_lod_bias) LODBias = 0;

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

	return;
}




UBOOL UOpenGLRenderDevice::Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen) {
	guard(UOpenGLRenderDevice::Init);

	// FIXME: Check viewport class is SDL.

	debugf(TEXT("Initializing OpenGLDrv..."));

	// Init global GL.
	if (NumDevices == 0) {
		bool loadRet;

		// Bind the library.
		const char* GLLibName = NULL;
		FString GLLibNameString;
		if (GConfig->GetString(g_pSection, TEXT("OpenGLLibName"), GLLibNameString)) {
			debugf(TEXT("binding %s"), *GLLibNameString);
			GLLibName = appToAnsi(*GLLibNameString);
		}

		if (!GLLoaded) {
			// Only call it once as succeeding calls will 'fail'.
			if (SDL_GL_LoadLibrary(GLLibName) == -1) {
				appErrorf(appFromAnsi(SDL_GetError()));
			}
			GLLoaded = true;
		}

		//Get OpenGL 1.x function pointers
		loadRet = GetGL1Procs();
		if (!loadRet) {
			return 0;
		}
	}

	NumDevices++;

	// Init this GL rendering context.
	m_zeroPrefixBindTrees = ShareLists ? m_sharedZeroPrefixBindTrees : m_localZeroPrefixBindTrees;
	m_nonZeroPrefixBindTrees = ShareLists ? m_sharedNonZeroPrefixBindTrees : m_localNonZeroPrefixBindTrees;
	m_nonZeroPrefixBindChain = ShareLists ? &m_sharedNonZeroPrefixBindChain : &m_localNonZeroPrefixBindChain;
	m_nonZeroPrefixTexIdPool = ShareLists ? &m_sharedNonZeroPrefixTexIdPool : &m_localNonZeroPrefixTexIdPool;
	m_RGBA8TexPool = ShareLists ? &m_sharedRGBA8TexPool : &m_localRGBA8TexPool;

	Viewport = InViewport;
	Context = SDL_GL_CreateContext( GetWindow() );
	if( !Context )
		return 0;

	if (!SetRes(NewX, NewY, NewColorBytes, Fullscreen)) {
		return FailedInitf(LocalizeError("ResFailed"));
	}

	return 1;
	unguard;
}

UBOOL UOpenGLRenderDevice::Exec(const TCHAR* Cmd, FOutputDevice& Ar) {
	guard(UOpenGLRenderDevice::Exec);

	if (URenderDevice::Exec(Cmd, Ar)) {
		return 1;
	}
	if (ParseCommand(&Cmd, TEXT("DGL"))) {
		if (ParseCommand(&Cmd, TEXT("BUFFERTRIS"))) {
			BufferActorTris = !BufferActorTris;
			if (!UseVertexSpecular) BufferActorTris = 0;
			debugf(TEXT("BUFFERTRIS [%i]"), BufferActorTris);
			return 1;
		}
		else if (ParseCommand(&Cmd, TEXT("BUILD"))) {
			debugf(TEXT("OpenGL renderer built: %s %s"), __DATE__, __TIME__);
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

	return 0;

	unguard;
}

void UOpenGLRenderDevice::Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize) {
	guard(UOpenGLRenderDevice::Lock);
	check(SetContext() == 0);
	check(LockCount == 0);
	++LockCount;
	UTGLR_DEBUG_CALL_COUNT(Lock);


	//Reset stats
	BindCycles = ImageCycles = ComplexCycles = GouraudCycles = TileCycles = 0;

	m_vpEnableCount = 0;
	m_vpSwitchCount = 0;
	m_fpEnableCount = 0;
	m_fpSwitchCount = 0;
	m_AASwitchCount = 0;
	m_sceneNodeCount = 0;
	m_sceneNodeHackCount = 0;
	m_stat0Count = 0;
	m_stat1Count = 0;


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


	//Scan for textures that may have been deleted by another context
	ScanForDeletedTextures();


	bool flushTextures = false;
	bool needShaderReload = false;


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

	if (SmoothMaskedTextures != PL_SmoothMaskedTextures) {
		PL_SmoothMaskedTextures = SmoothMaskedTextures;

		//Clear masked blending state if set before adjusting smooth masked textures bit
		SetBlend(PF_Occlude);

		//Disable alpha to coverage if currently enabled
		if (m_alphaToCoverageEnabled) {
			m_alphaToCoverageEnabled = false;
			glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		}
	}

	//Smooth masked textures bit controls alpha blend for masked textures
	m_smoothMaskedTexturesBit = 0;
	m_useAlphaToCoverageForMasked = false;
	if (SmoothMaskedTextures) {
		//Use alpha to coverage if using AA and enough samples
		if (m_usingAA && (m_initNumAASamples >= 4)) {
			m_useAlphaToCoverageForMasked = true;
		}
		else {
			m_smoothMaskedTexturesBit = PF_Masked;
		}
	}

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
	}

	if (UseDetailAlpha != PL_UseDetailAlpha) {
		PL_UseDetailAlpha = UseDetailAlpha;
		if (UseDetailAlpha) {
			InitAlphaTextureSafe();
		}
	}

	if (SinglePassDetail != PL_SinglePassDetail) {
		PL_SinglePassDetail = SinglePassDetail;
	}


	if (UseFragmentProgram != PL_UseFragmentProgram) {
		PL_UseFragmentProgram = UseFragmentProgram;
		if (UseFragmentProgram) {
			//Attempt to initialize fragment program mode
			TryInitializeFragmentProgramMode();
			needShaderReload = false;
		}
		else {
			//Free fragment programs if they were allocated and leave fragment program mode if necessary
			ShutdownFragmentProgramMode();
		}
	}

	//Check if fragment program reload is necessary
	if (UseFragmentProgram) {
		if (needShaderReload) {
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


	//Shared fragment program parameters
	if (UseFragmentProgram) {
		//Lightmap blend scale factor
	}

	//Precalculate complex surface color
	m_complexSurfaceColor3f_1f[0] = 1.0f;
	m_complexSurfaceColor3f_1f[1] = 1.0f;
	m_complexSurfaceColor3f_1f[2] = 1.0f;
	//Alpha used for fragment program lightmap blend scaling
	m_complexSurfaceColor3f_1f[3] = (OneXBlending) ? 0.0f : 1.0f;


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
	if (UseFragmentProgram) {
		m_pRenderPassesNoCheckSetupProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_FP;
		m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc = &UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP;
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
	//Alpha used for fragment program lightmap blend scaling
	m_detailTextureColor3f_1f[3] = (OneXBlending) ? 0.0f : 1.0f;

	//Precalculate mask for MaskedTextureHack based on if it's enabled
	m_maskedTextureHackMask = (MaskedTextureHack) ? TEX_CACHE_ID_FLAG_MASKED : 0;

	// Remember stuff.
	FlashScale = InFlashScale;
	FlashFog   = InFlashFog;

	//Selection setup
	m_HitData = InHitData;
	m_HitSize = InHitSize;
	m_HitCount = 0;
	if (m_HitData) {
		m_HitBufSize = *m_HitSize;
		*m_HitSize = 0;

		//Start selection
		m_gclip.SelectModeStart();
	}

	//Flush textures if necessary due to config change
	if (flushTextures) {
		Flush(1);
	}

	unguard;
}

void UOpenGLRenderDevice::SetSceneNode(FSceneNode* Frame) {
	guard(UOpenGLRenderDevice::SetSceneNode);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(SetSceneNode);

	EndBuffering();		// Flush vertex array before changing the projection matrix!

	m_sceneNodeCount++;

	//No need to set default AA state here
	//No need to set default projection state as this function always sets/initializes it
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	// Precompute stuff.
	FLOAT rcpFrameFX = 1.0f / Frame->FX;
	m_Aspect = Frame->FY * rcpFrameFX;
	m_RProjZ = appTan(Viewport->Actor->FovAngle * PI / 360.0);
	m_RFX2 = 2.0f * m_RProjZ * rcpFrameFX;
	m_RFY2 = 2.0f * m_RProjZ * rcpFrameFX;

	//Remember Frame->X and Frame->Y for scene node hack
	m_sceneNodeX = Frame->X;
	m_sceneNodeY = Frame->Y;

	// Set viewport.
	glViewport(Frame->XB, Viewport->SizeY - Frame->Y - Frame->YB, Frame->X, Frame->Y);

	//Decide whether or not to use Z range hack
	m_useZRangeHack = false;
	if (ZRangeHack && !GIsEditor) {
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

	//Set clip planes if doing selection
	if (m_HitData) {
		if (Frame->Viewport->IsOrtho()) {
			float cp[4];
			FLOAT nX = Viewport->HitX - Frame->FX2;
			FLOAT pX = nX + Viewport->HitXL;
			FLOAT nY = Viewport->HitY - Frame->FY2;
			FLOAT pY = nY + Viewport->HitYL;

			nX *= m_RFX2 * 0.5f;
			pX *= m_RFX2 * 0.5f;
			nY *= m_RFY2 * 0.5f;
			pY *= m_RFY2 * 0.5f;

			cp[0] = +1.0; cp[1] = 0.0; cp[2] = 0.0; cp[3] = -nX;
			m_gclip.SetCp(0, cp);
			m_gclip.SetCpEnable(0, true);

			cp[0] = 0.0; cp[1] = +1.0; cp[2] = 0.0; cp[3] = -nY;
			m_gclip.SetCp(1, cp);
			m_gclip.SetCpEnable(1, true);

			cp[0] = -1.0; cp[1] = 0.0; cp[2] = 0.0; cp[3] = +pX;
			m_gclip.SetCp(2, cp);
			m_gclip.SetCpEnable(2, true);

			cp[0] = 0.0; cp[1] = -1.0; cp[2] = 0.0; cp[3] = +pY;
			m_gclip.SetCp(3, cp);
			m_gclip.SetCpEnable(3, true);

			//Near clip plane
			cp[0] = 0.0f; cp[1] = 0.0f; cp[2] = 1.0f; cp[3] = -0.5f;
			m_gclip.SetCp(4, cp);
			m_gclip.SetCpEnable(4, true);
		}
		else {
			FVector N[4];
			float cp[4];
			INT i;

			FLOAT nX = Viewport->HitX - Frame->FX2;
			FLOAT pX = nX + Viewport->HitXL;
			FLOAT nY = Viewport->HitY - Frame->FY2;
			FLOAT pY = nY + Viewport->HitYL;

			N[0] = (FVector(nX * Frame->RProj.Z, 0, 1) ^ FVector(0, -1, 0)).SafeNormal();
			N[1] = (FVector(pX * Frame->RProj.Z, 0, 1) ^ FVector(0, +1, 0)).SafeNormal();
			N[2] = (FVector(0, nY * Frame->RProj.Z, 1) ^ FVector(+1, 0, 0)).SafeNormal();
			N[3] = (FVector(0, pY * Frame->RProj.Z, 1) ^ FVector(-1, 0, 0)).SafeNormal();

			for (i = 0; i < 4; i++) {
				cp[0] = N[i].X;
				cp[1] = N[i].Y;
				cp[2] = N[i].Z;
				cp[3] = 0.0f;
				m_gclip.SetCp(i, cp);
				m_gclip.SetCpEnable(i, true);
			}

			//Near clip plane
			cp[0] = 0.0f; cp[1] = 0.0f; cp[2] = 1.0f; cp[3] = -0.5f;
			m_gclip.SetCp(4, cp);
			m_gclip.SetCpEnable(4, true);
		}
	}

	unguard;
}

void UOpenGLRenderDevice::Unlock(UBOOL Blit) {
	guard(UOpenGLRenderDevice::Unlock);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(Unlock);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	// Unlock and render.
	check(LockCount == 1);

	//glFlush();

	if (Blit) {
		CheckGLErrorFlag(TEXT("please report this bug"));

		//Swap buffers
		SDL_GL_SwapWindow( GetWindow() );
	}

	--LockCount;

	//If doing selection, end and return hits
	if (m_HitData) {
		INT i;

		//End selection
		m_gclip.SelectModeEnd();

		*m_HitSize = m_HitCount;

		//Disable clip planes
		for (i = 0; i < 5; i++) {
			m_gclip.SetCpEnable(i, false);
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
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
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
	dbgPrintf("VP enable count = %u\n", m_vpEnableCount);
	dbgPrintf("VP switch count = %u\n", m_vpSwitchCount);
	dbgPrintf("FP enable count = %u\n", m_fpEnableCount);
	dbgPrintf("FP switch count = %u\n", m_fpSwitchCount);
	dbgPrintf("AA switch count = %u\n", m_AASwitchCount);
	dbgPrintf("Scene node count = %u\n", m_sceneNodeCount);
	dbgPrintf("Scene node hack count = %u\n", m_sceneNodeHackCount);
	dbgPrintf("Stat 0 count = %u\n", m_stat0Count);
	dbgPrintf("Stat 1 count = %u\n", m_stat1Count);
#endif


	unguard;
}

void UOpenGLRenderDevice::Flush(UBOOL AllowPrecache) {
	guard(UOpenGLRenderDevice::Flush);
	check(SetContext() == 0);

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
	guard(UOpenGLRenderDevice::DrawComplexSurface);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(DrawComplexSurface);

	EndBuffering();

	if (SceneNodeHack) {
		if ((Frame->X != m_sceneNodeX) || (Frame->Y != m_sceneNodeY)) {
			m_sceneNodeHackCount++;
			SetSceneNode(Frame);
		}
	}

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	//This function uses cached shader state information
	//This function uses cached texture state information

	check(Surface.Texture);

	//Hit select path
	if (m_HitData) {
		for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
			INT NumPts = Poly->NumPts;
			CGClip::vec3_t triPts[3];
			INT i;
			const FTransform* Pt;

			Pt = Poly->Pts[0];
			triPts[0].x = Pt->Point.X;
			triPts[0].y = Pt->Point.Y;
			triPts[0].z = Pt->Point.Z;

			for (i = 2; i < NumPts; i++) {
				Pt = Poly->Pts[i - 1];
				triPts[1].x = Pt->Point.X;
				triPts[1].y = Pt->Point.Y;
				triPts[1].z = Pt->Point.Z;

				Pt = Poly->Pts[i];
				triPts[2].x = Pt->Point.X;
				triPts[2].y = Pt->Point.Y;
				triPts[2].z = Pt->Point.Z;

				m_gclip.SelectDrawTri(triPts);
			}
		}

		return;
	}

	cycle(ComplexCycles);

	//Calculate UDot and VDot intermediates for complex surface
	m_csUDot = Facet.MapCoords.XAxis | Facet.MapCoords.Origin;
	m_csVDot = Facet.MapCoords.YAxis | Facet.MapCoords.Origin;

	//Buffer static geometry
	//Sets m_csPolyCount
	//m_csPtCount set later from return value
	//Sets MultiDrawFirstArray and MultiDrawCountArray
	INT numVerts;
	if (UseFragmentProgram) {
		numVerts = BufferStaticComplexSurfaceGeometry_VP(Facet);
	}
	else {
		numVerts = BufferStaticComplexSurfaceGeometry(Facet);
	}

	//Reject invalid surfaces early
	if (numVerts == 0) {
		return;
	}

	//Save number of points
	m_csPtCount = numVerts;

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


	DWORD PolyFlags = Surface.PolyFlags;

	//Initialize render passes state information
	m_rpPassCount = 0;
	m_rpTMUnits = TMUnits;
	m_rpForceSingle = false;
	m_rpMasked = ((PolyFlags & PF_Masked) == 0) ? false : true;
	m_rpSetDepthEqual = false;


	//Do static render passes state setup
	if (UseFragmentProgram) {
		const FVector &XAxis = Facet.MapCoords.XAxis;
		const FVector &YAxis = Facet.MapCoords.YAxis;

		glVertexAttrib4fARB(6, XAxis.X, XAxis.Y, XAxis.Z, -m_csUDot);
		glVertexAttrib4fARB(7, YAxis.X, YAxis.Y, YAxis.Z, -m_csVDot);
	}

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
		}
		else {
			RenderPasses();

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
			else {
				DrawDetailTexture(*Surface.DetailTexture, numVerts, clipDetailTexture);
			}
		}
	}
	else {
		RenderPasses();
	}

	// UnrealEd selection.
	if (GIsEditor && (PolyFlags & (PF_Selected | PF_FlatShaded))) {
		//No need to set default AA state here as it is always set on entry to DrawComplexSurface
		//No need to set default projection state here as it is always set on entry to DrawComplexSurface
		//No need to set default color state here as it is always set on entry to DrawComplexSurface
		SetDefaultShaderState();
		SetDefaultTextureState();

		SetNoTexture(0);
		SetBlend(PF_Highlighted);
		if (PolyFlags & PF_FlatShaded) {
			if (PolyFlags & PF_Selected) {
				glColor4f((Surface.FlatColor.R * 1.5f) / 255.0f, (Surface.FlatColor.G * 1.5f) / 255.0f, (Surface.FlatColor.B * 1.5f) / 255.0f, 1.0f);
			}
			else {
				glColor4f(Surface.FlatColor.R / 255.0f, Surface.FlatColor.G / 255.0f, Surface.FlatColor.B / 255.0f, 0.85f);
			}
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

	uncycle(ComplexCycles);
	unguard;
}

#ifdef UTGLR_RUNE_BUILD
void UOpenGLRenderDevice::PreDrawFogSurface() {
	guard(UOpenGLRenderDevice::PreDrawFogSurface);
	UTGLR_DEBUG_CALL_COUNT(PreDrawFogSurface);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	SetBlend(PF_AlphaBlend);
	SetNoTexture(0);

	unguard;
}

void UOpenGLRenderDevice::PostDrawFogSurface() {
	guard(UOpenGLRenderDevice::PostDrawFogSurface);
	UTGLR_DEBUG_CALL_COUNT(PostDrawFogSurface);

	SetBlend(0);

	unguard;
}

void UOpenGLRenderDevice::DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf) {
	guard(UOpenGLRenderDevice::DrawFogSurface);
	UTGLR_DEBUG_CALL_COUNT(DrawFogSurface);

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
	guard(UOpenGLRenderDevice::PreDrawGouraud);
	UTGLR_DEBUG_CALL_COUNT(PreDrawGouraud);

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
	guard(UOpenGLRenderDevice::PostDrawGouraud);
	UTGLR_DEBUG_CALL_COUNT(PostDrawGouraud);

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
	guard(UOpenGLRenderDevice::DrawGouraudPolygonOld);
	UTGLR_DEBUG_CALL_COUNT(DrawGouraudPolygonOld);
	cycle(GouraudCycles);

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
#elif UTGLR_UNREAL_227_BUILD
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

	uncycle(GouraudCycles);
	unguard;
}

void UOpenGLRenderDevice::DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span) {
	guard(UOpenGLRenderDevice::DrawGouraudPolygon);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(DrawGouraudPolygon);

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

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t triPts[3];
		const FTransTexture* Pt;
		INT i;

		Pt = Pts[0];
		triPts[0].x = Pt->Point.X;
		triPts[0].y = Pt->Point.Y;
		triPts[0].z = Pt->Point.Z;

		for (i = 2; i < NumPts; i++) {
			Pt = Pts[i - 1];
			triPts[1].x = Pt->Point.X;
			triPts[1].y = Pt->Point.Y;
			triPts[1].z = Pt->Point.Z;

			Pt = Pts[i];
			triPts[2].x = Pt->Point.X;
			triPts[2].y = Pt->Point.Y;
			triPts[2].z = Pt->Point.Z;

			m_gclip.SelectDrawTri(triPts);
		}

		return;
	}

	if (NumPts > m_bufferActorTrisCutoff) {
		EndBuffering();

		SetDefaultAAState();
		//No need to set default projection state here as DrawGouraudPolygonOld will set its own projection state
		//No need to set default color state here as DrawGouraudPolygonOld will set its own color state
#ifdef UTGLR_RUNE_BUILD
		if (UseFragmentProgram && m_gpFogEnabled) {
			SetShaderState(m_vpDefaultRenderingStateWithLinearFog, m_fpDefaultRenderingStateWithLinearFog);
		}
		else {
			SetDefaultShaderState();
		}
#else
		SetDefaultShaderState();
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
	{
		//Flush any previously buffered gouraud polys
		EndGouraudPolygonBuffering();

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
#elif UTGLR_UNREAL_227_BUILD
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated | PF_AlphaBlend)) == PF_RenderFog) && UseVertexSpecular) {
#else
			if (((PolyFlags & (PF_RenderFog | PF_Translucent | PF_Modulated)) == PF_RenderFog) && UseVertexSpecular) {
#endif
				m_requestedColorFlags = CF_COLOR_ARRAY | CF_DUAL_COLOR_ARRAY | CF_COLOR_SUM;
			}
		}

#ifdef UTGLR_RUNE_BUILD
		if (UseFragmentProgram && m_gpFogEnabled) {
			SetShaderState(m_vpDefaultRenderingStateWithLinearFog, m_fpDefaultRenderingStateWithLinearFog);
		}
		else {
			SetDefaultShaderState();
		}
#else
		//May need to set a fog vertex program and fragment program if fragment program mode is enabled
		if (UseFragmentProgram && (m_requestedColorFlags & CF_DUAL_COLOR_ARRAY)) {
			//Leave color sum off if using fragment program
			m_requestedColorFlags &= ~CF_COLOR_SUM;
			SetShaderState(m_vpDefaultRenderingStateWithFog, m_fpDefaultRenderingStateWithFog);
		}
		else {
			SetDefaultShaderState();
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
	guard(UOpenGLRenderDevice::DrawTile);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(DrawTile);

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

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t triPts[3];

		triPts[0].x = RPX1;
		triPts[0].y = RPY1;
		triPts[0].z = Z;

		triPts[1].x = RPX2;
		triPts[1].y = RPY1;
		triPts[1].z = Z;

		triPts[2].x = RPX2;
		triPts[2].y = RPY2;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		triPts[0].x = RPX1;
		triPts[0].y = RPY1;
		triPts[0].z = Z;

		triPts[1].x = RPX2;
		triPts[1].y = RPY2;
		triPts[1].z = Z;

		triPts[2].x = RPX1;
		triPts[2].y = RPY2;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		return;
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

			SetDefaultShaderState();

			//Update current poly flags (before possible local modification)
			m_curPolyFlags = PolyFlags;
			m_curPolyFlags2 = PolyFlags2;

#ifdef UTGLR_RUNE_BUILD
			if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#elif UTGLR_UNREAL_227_BUILD
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
#elif UTGLR_UNREAL_227_BUILD
				if (PolyFlags & PF_AlphaBlend) {
					fAlpha = _mm_mul_ss(fAlpha, _mm_load_ss(&Color.W));
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
#elif UTGLR_UNREAL_227_BUILD
				if (PolyFlags & PF_AlphaBlend) {
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

		cycle(TileCycles);

		if (NoAATiles) {
			SetDisabledAAState();
		}
		else {
			SetDefaultAAState();
		}
		SetDefaultProjectionState();
		SetDefaultColorState();
		SetDefaultShaderState();
		SetDefaultTextureState();

#ifdef UTGLR_RUNE_BUILD
		if (Info.Palette && Info.Palette[128].A != 255 && !(PolyFlags & (PF_Translucent | PF_AlphaBlend))) {
#elif UTGLR_UNREAL_227_BUILD
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
#elif UTGLR_UNREAL_227_BUILD
			if (PolyFlags & PF_AlphaBlend) {
				glColor4fv(&Color.X);
			}
			else {
				glColor3fv(&Color.X);
			}
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

		uncycle(TileCycles);
	}

	unguard;
}

void UOpenGLRenderDevice::Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
	guard(UOpenGLRenderDevice::Draw3DLine);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(Draw3DLine);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	P1 = P1.TransformPointBy(Frame->Coords);
	P2 = P2.TransformPointBy(Frame->Coords);
	if (Frame->Viewport->IsOrtho()) {
		// Zoom.
		FLOAT rcpZoom = 1.0f / Frame->Zoom;
		P1.X = (P1.X * rcpZoom) + Frame->FX2;
		P1.Y = (P1.Y * rcpZoom) + Frame->FY2;
		P2.X = (P2.X * rcpZoom) + Frame->FX2;
		P2.Y = (P2.Y * rcpZoom) + Frame->FY2;
		P1.Z = P2.Z = 1;

		// See if points form a line parallel to our line of sight (i.e. line appears as a dot).
		if (Abs(P2.X - P1.X) + Abs(P2.Y - P1.Y) >= 0.2f) {
			Draw2DLine(Frame, Color, LineFlags, P1, P2);
		}
		else if (Frame->Viewport->Actor->OrthoZoom < ORTHO_LOW_DETAIL) {
			Draw2DPoint(Frame, Color, LINE_None, P1.X - 1.0f, P1.Y - 1.0f, P1.X + 1.0f, P1.Y + 1.0f, P1.Z);
		}
	}
	else {
		//Hit select path
		if (m_HitData) {
			CGClip::vec3_t lnPts[2];

			lnPts[0].x = P1.X;
			lnPts[0].y = P1.Y;
			lnPts[0].z = P1.Z;

			lnPts[1].x = P2.X;
			lnPts[1].y = P2.Y;
			lnPts[1].z = P2.Z;

			m_gclip.SelectDrawLine(lnPts);

			return;
		}

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

void UOpenGLRenderDevice::Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2) {
	guard(UOpenGLRenderDevice::Draw2DLine);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(Draw2DLine);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	SetNoTexture(0);
	SetBlend(PF_Highlighted | PF_Occlude);
	glColor3fv(&Color.X);

	//Get line coordinates back in 3D
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

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t lnPts[2];

		lnPts[0].x = X1Pos;
		lnPts[0].y = Y1Pos;
		lnPts[0].z = P1.Z;

		lnPts[1].x = X2Pos;
		lnPts[1].y = Y2Pos;
		lnPts[1].z = P2.Z;

		m_gclip.SelectDrawLine(lnPts);

		return;
	}

	glBegin(GL_LINES);
	glVertex3f(X1Pos, Y1Pos, P1.Z);
	glVertex3f(X2Pos, Y2Pos, P2.Z);
	glEnd();

	unguard;
}

void UOpenGLRenderDevice::Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z) {
	guard(UOpenGLRenderDevice::Draw2DPoint);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(Draw2DPoint);

	EndBuffering();

	SetDefaultAAState();
	SetDefaultProjectionState();
	SetDefaultColorState();
	SetDefaultShaderState();
	SetDefaultTextureState();

	SetBlend(PF_Highlighted | PF_Occlude);
	SetNoTexture(0);
	glColor3fv(&Color.X); // vogel: was 4 - ONLY FOR UT!

	// Hack to fix UED selection problem with selection brush
	if (GIsEditor) {
		Z = 1.0f;
	}

	//Get point coordinates back in 3D
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

	//Hit select path
	if (m_HitData) {
		CGClip::vec3_t triPts[3];

		triPts[0].x = X1Pos;
		triPts[0].y = Y1Pos;
		triPts[0].z = Z;

		triPts[1].x = X2Pos;
		triPts[1].y = Y1Pos;
		triPts[1].z = Z;

		triPts[2].x = X2Pos;
		triPts[2].y = Y2Pos;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		triPts[0].x = X1Pos;
		triPts[0].y = Y1Pos;
		triPts[0].z = Z;

		triPts[1].x = X2Pos;
		triPts[1].y = Y2Pos;
		triPts[1].z = Z;

		triPts[2].x = X1Pos;
		triPts[2].y = Y2Pos;
		triPts[2].z = Z;

		m_gclip.SelectDrawTri(triPts);

		return;
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
	guard(UOpenGLRenderDevice::ClearZ);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(ClearZ);

	EndBuffering();

	//Default AA state not required for glClear
	//Default projection state not required for glClear
	//Default color state not required for glClear
	//Default shader state not required for glClear
	//Default texture state not required for glClear

	SetBlend(PF_Occlude);
	glClear(GL_DEPTH_BUFFER_BIT);

	unguard;
}

void UOpenGLRenderDevice::PushHit(const BYTE* Data, INT Count) {
	guard(UOpenGLRenderDevice::PushHit);

	INT i;

	EndBuffering();

	//Add to stack
	for (i = 0; i < Count; i += 4) {
		DWORD hitName = *(DWORD *)(Data + i);
		m_gclip.PushHitName(hitName);
	}

	unguard;
}

void UOpenGLRenderDevice::PopHit(INT Count, UBOOL bForce) {
	guard(UOpenGLRenderDevice::PopHit);

	EndBuffering();

	INT i;
	bool selHit;

	//Handle hit
	selHit = m_gclip.CheckNewSelectHit();
	if (selHit || bForce) {
		DWORD nHitNameBytes;

		nHitNameBytes = m_gclip.GetHitNameStackSize() * 4;
		if (nHitNameBytes <= m_HitBufSize) {
			m_gclip.GetHitNameStackValues((unsigned int *)m_HitData, nHitNameBytes / 4);
			m_HitCount = nHitNameBytes;
		}
		else {
			m_HitCount = 0;
		}
	}

	//Remove from stack
	for (i = 0; i < Count; i += 4) {
		m_gclip.PopHitName();
	}

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
	check(SetContext() == 0);

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
	guard(UOpenGLRenderDevice::EndFlash);
	check(SetContext() == 0);
	UTGLR_DEBUG_CALL_COUNT(EndFlash);

	if ((FlashScale != FPlane(0.5f, 0.5f, 0.5f, 0.0f)) || (FlashFog != FPlane(0.0f, 0.0f, 0.0f, 0.0f))) {
		EndBuffering();

		SetDefaultAAState();
		SetDefaultProjectionState();
		SetDefaultColorState();
		SetDefaultShaderState();
		SetDefaultTextureState();

		SetBlend(PF_Highlighted);
		SetNoTexture(0);

		glColor4f(FlashFog.X, FlashFog.Y, FlashFog.Z, 1.0f - Min(FlashScale.X * 2.0f, 1.0f));

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
	check(SetContext() == 0);

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

void UOpenGLRenderDevice::ScanForDeletedTextures(void) {
	guard(UOpenGLRenderDevice::ScanForDeletedTextures);

	unsigned int u;

	//Check if currently bound texture may have been removed by another context
	for (u = 0; u < MAX_TMUNITS; u++) {
		FCachedTexture *pBind = TexInfo[u].pBind;
		if (pBind != NULL) {
			//Check if the texture may have been removed by another context
			if (NULL == FindCachedTexture(TexInfo[u].CurrentCacheID)) {
				//Cause texture to be updated next time
				//Some parts of texture resource may not be released while still bound
				TexInfo[u].CurrentCacheID = TEX_CACHE_ID_UNUSED;
				TexInfo[u].pBind = NULL;
			}
		}
	}

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
			//See if the tex pool is not enabled, or the tex format is not RGBA8, or the texture has mipmaps
			if (!UseTexPool || (pCT->texInternalFormat != GL_RGBA8) || (pCT->texParams.hasMipmaps)) {
				//Remove node from linked list
				m_nonZeroPrefixBindChain->unlink(pCT);

				//Get pointer to node in bind map
				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (uintptr_t)&(((QWORD_CTTree_t::node_t *)0)->data));
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
	static unsigned int s_c;
	dbgPrintf("utglr: TexPool free = %u, Id = %u, u = %u, v = %u\n",
		s_c++, pCT->Id, pCT->UBits, pCT->VBits);
}
#endif

				//Remove node from linked list
				m_nonZeroPrefixBindChain->unlink(pCT);

				//Create a key from the lg2 width and height of the texture object
				TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pCT->UBits, pCT->VBits);

				//Get pointer to node in bind map
				QWORD_CTTree_t::node_t *pNode = (QWORD_CTTree_t::node_t *)((BYTE *)pCT - (uintptr_t)&(((QWORD_CTTree_t::node_t *)0)->data));
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
	cycle(BindCycles);

	//Set texture
	glBindTexture(GL_TEXTURE_2D, m_noTextureId);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_NO_TEX;
	TexInfo[Multi].pBind = NULL;

	uncycle(BindCycles);

	unguard;
}

void UOpenGLRenderDevice::SetAlphaTextureNoCheck(INT Multi) {
	guard(UOpenGLRenderDevice::SetAlphaTexture);

	// Set alpha gradient texture.
	cycle(BindCycles);

	//Set texture
	glBindTexture(GL_TEXTURE_2D, m_alphaTextureId);

	TexInfo[Multi].CurrentCacheID = TEX_CACHE_ID_ALPHA_TEX;
	TexInfo[Multi].pBind = NULL;

	uncycle(BindCycles);

	unguard;
}

FCachedTexture *UOpenGLRenderDevice::FindCachedTexture(QWORD CacheID) {
	bool isZeroPrefixCacheID = ((CacheID & 0xFFFFFFFF00000000ULL) == 0) ? true : false;
	FCachedTexture *pBind = NULL;

	if (isZeroPrefixCacheID) {
		DWORD CacheIDSuffix = (CacheID & 0x00000000FFFFFFFFULL);

		DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
		DWORD_CTTree_t::node_t *bindTreePtr = zeroPrefixBindTree->find(CacheIDSuffix);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
		}
	}
	else {
		DWORD CacheIDSuffix = (CacheID & 0x00000000FFFFFFFF);
		DWORD treeIndex = CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);

		QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[treeIndex];
		QWORD_CTTree_t::node_t *bindTreePtr = nonZeroPrefixBindTree->find(CacheID);
		if (bindTreePtr != 0) {
			pBind = &bindTreePtr->data;
		}
	}

	return pBind;
}

UOpenGLRenderDevice::QWORD_CTTree_NodePool_t::node_t *UOpenGLRenderDevice::TryAllocFromTexPool(TexPoolMapKey_t texPoolKey) {
	TexPoolMap_t::node_t *texPoolPtr;

	//Search for the key in the map
	texPoolPtr = m_RGBA8TexPool->find(texPoolKey);
	if (texPoolPtr != 0) {
		QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr;

		//Get a reference to the pool of nodes with tex ids of the right dimension
		QWORD_CTTree_NodePool_t &texPool = texPoolPtr->data;

		//Attempt to get a texture id from the tex pool
		if ((texPoolNodePtr = texPool.try_remove()) != 0) {
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: TexPool retrieve = %u, Id = %u, u = %u, v = %u\n",
		s_c++, texPoolNodePtr->data.Id, texPoolNodePtr->data.UBits, texPoolNodePtr->data.VBits);
}
#endif

			return texPoolNodePtr;
		}
	}

	return NULL;
}

BYTE UOpenGLRenderDevice::GenerateTexFilterParams(DWORD PolyFlags, FCachedTexture *pBind) {
	BYTE texFilter;

	//Generate tex filter params
	texFilter = 0;
	if (NoFiltering) {
		texFilter |= CT_MIN_FILTER_NEAREST;
	}
	else if (PolyFlags & PF_NoSmooth) {
		texFilter |= (!pBind->texParams.hasMipmaps) ? CT_MIN_FILTER_NEAREST : CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST;
	}
	else {
		texFilter |= (!pBind->texParams.hasMipmaps) ? CT_MIN_FILTER_LINEAR : (UseTrilinear ? CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR : CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST);
		texFilter |= CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT;
		if (MaxAnisotropy) {
			texFilter |= CT_ANISOTROPIC_FILTER_BIT;
		}
	}
	texFilter |= (pBind->texParams.texObjFilter & (CT_ADDRESS_U_CLAMP | CT_ADDRESS_V_CLAMP));

	return texFilter;
}

void UOpenGLRenderDevice::SetTexFilterNoCheck(FCachedTexture *pBind, BYTE texFilter) {
	BYTE texFilterXor;

	//Get changed tex filter params
	texFilterXor = pBind->texParams.filter ^ texFilter;

	//Update main copy of tex filter params early
	pBind->texParams.filter = texFilter;

	//Update changed params
	if (texFilterXor & CT_MIN_FILTER_MASK) {
		GLint intParam = GL_NEAREST;

		switch (texFilter & CT_MIN_FILTER_MASK) {
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
	if (texFilterXor & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) {
		GLint intParam;

		intParam = (texFilter & CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT) ? GL_LINEAR : GL_NEAREST;

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, intParam);
	}
	if (texFilterXor & CT_ANISOTROPIC_FILTER_BIT) {
		GLfloat floatParam;

		floatParam = (texFilter & CT_ANISOTROPIC_FILTER_BIT) ? (GLfloat)MaxAnisotropy : 1.0f;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, floatParam);
	}
	if (texFilterXor & CT_ADDRESS_U_CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (texFilter & CT_ADDRESS_U_CLAMP) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	}
	if (texFilterXor & CT_ADDRESS_V_CLAMP) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (texFilter & CT_ADDRESS_V_CLAMP) ? GL_CLAMP_TO_EDGE : GL_REPEAT);
	}

	return;
}

//This function must use Tex.CurrentCacheID and NEVER use Info.CacheID to reference the texture cache id
//This makes it work with the masked texture hack code
void UOpenGLRenderDevice::SetTextureNoCheck(FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags) {
	guard(UOpenGLRenderDevice::SetTexture);

	// Make current.
	cycle(BindCycles);

	//Search for texture in cache
	FCachedTexture *pBind;
	bool existingBind = false;
	bool needTexAllocate = true;
	pBind = FindCachedTexture(Tex.CurrentCacheID);

	if (pBind) {
		//Update when texture last referenced
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
		bool isZeroPrefixCacheID = ((Tex.CurrentCacheID & 0xFFFFFFFF00000000ULL) == 0) ? true : false;
		if (isZeroPrefixCacheID) {
			DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFFULL);
			DWORD_CTTree_t *zeroPrefixBindTree = &m_zeroPrefixBindTrees[CTZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix)];
			DWORD_CTTree_t::node_t *pNewNode;

			//Insert new texture info
			pNewNode = m_DWORD_CTTree_Allocator.alloc_node();
			pNewNode->key = CacheIDSuffix;
			zeroPrefixBindTree->insert(pNewNode);
			pBind = &pNewNode->data;
			pBind->LastUsedFrameCount = m_currentFrameCount;

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
		else {
			DWORD CacheIDSuffix = (Tex.CurrentCacheID & 0x00000000FFFFFFFF);
			DWORD treeIndex = CTNonZeroPrefixCacheIDSuffixToTreeIndex(CacheIDSuffix);
			QWORD_CTTree_t *nonZeroPrefixBindTree = &m_nonZeroPrefixBindTrees[treeIndex];
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
				//Only textures without mipmaps are stored in the tex pool
				if ((pBind->texType == TEX_TYPE_NORMAL) && (Info.NumMips == 1)) {
					QWORD_CTTree_NodePool_t::node_t *texPoolNodePtr;

					//Create a key from the lg2 width and height of the texture object
					TexPoolMapKey_t texPoolKey = MakeTexPoolMapKey(pBind->UBits, pBind->VBits);

					//Attempt to allocate from the tex pool
					texPoolNodePtr = TryAllocFromTexPool(texPoolKey);
					if (texPoolNodePtr) {
						//Use texture id from node in tex pool
						pBind->Id = texPoolNodePtr->data.Id;

						//Use tex params from node in tex pool
						pBind->texParams = texPoolNodePtr->data.texParams;
						pBind->dynamicTexBits = texPoolNodePtr->data.dynamicTexBits;

						//Then add node to free list
						m_nonZeroPrefixNodePool.add(texPoolNodePtr);

						//Clear the need tex id and tex allocate flags
						needTexIdAllocate = false;
						needTexAllocate = false;
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

	//Set texture
	glBindTexture(GL_TEXTURE_2D, pBind->Id);

	uncycle(BindCycles);

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
				BYTE texFilter;

				//Generate tex filter params
				texFilter = GenerateTexFilterParams(PolyFlags, pBind);

				//Set tex filter state
				SetTexFilter(pBind, texFilter);
			}
		}
	}

	// Upload if needed.
	if (!existingBind || Info.bRealtimeChanged) {
		UploadTextureExec(Info, PolyFlags, pBind, existingBind, needTexAllocate);
	}

	unguard;
}

void UOpenGLRenderDevice::UploadTextureExec(FTextureInfo& Info, DWORD PolyFlags, FCachedTexture *pBind, bool existingBind, bool needTexAllocate) {
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
	cycle(ImageCycles);

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
		DWORD texMaxLevel;
		BYTE texFilter;

		//Set tex max level param
		//This is set once for new textures and never changed afterwards
		texMaxLevel = 1000;
		if (!SkipMipmaps) {
			texMaxLevel = MaxLevel;
		}
		else {
			texMaxLevel = 0;
		}
		//Update if different than default for new textures
		if (texMaxLevel != 1000) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, texMaxLevel);
		}

		//Flag indicating if texture has mipmaps
		//This is set once for new textures and never changed afterwards
		pBind->texParams.hasMipmaps = (!SkipMipmaps) ? true : false;

		//Texture filter params set once for each texture
		pBind->texParams.texObjFilter = 0;
#ifdef UTGLR_UNREAL_227_BUILD
		// Check if clamped or wrapped (f.e. for Skyboxes). Smirftsch
		if (Info.UClampMode) {
			pBind->texParams.texObjFilter |= CT_ADDRESS_U_CLAMP;
		}
		if (Info.VClampMode) {
			pBind->texParams.texObjFilter |= CT_ADDRESS_V_CLAMP;
		}
#endif


		//Generate tex filter params
		texFilter = GenerateTexFilterParams(PolyFlags, pBind);

		//Set tex filter state
		SetTexFilter(pBind, texFilter);
	}


	//Some textures only upload the base texture
	INT MaxUploadLevel = MaxLevel;
	if (SkipMipmaps) {
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
		if (!Mip || !Mip->DataPtr) {
			//Skip looking at any subsequent mipmap pointers
			break;
		}
		else {
			//Texture data conversion if necessary
			switch (pBind->texType) {
			case TEX_TYPE_COMPRESSED_DXT1:
				//No conversion required for compressed DXT1 textures
				break;

			case TEX_TYPE_COMPRESSED_DXT3:
				//No conversion required for compressed DXT3 textures
				break;

			case TEX_TYPE_COMPRESSED_DXT5:
				//No conversion required for compressed DXT5 textures
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
				//Update existing texture
				switch (pBind->texType) {
				case TEX_TYPE_COMPRESSED_DXT1:
				case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
				case TEX_TYPE_COMPRESSED_DXT3:
				case TEX_TYPE_COMPRESSED_DXT5:
					guard(glCompressedTexSubImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glCompressedTexSubImage2DARB = %u\n", s_c++);
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
						(pBind->texType == TEX_TYPE_COMPRESSED_DXT1) ? (texWidth * texHeight / 2) : (texWidth * texHeight),
						(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? Src : Mip->DataPtr);
					unguard;

					break;

				default:
					guard(glTexSubImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glTexSubImage2D = %u\n", s_c++);
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
				//Create new texture and update
				switch (pBind->texType) {
				case TEX_TYPE_COMPRESSED_DXT1:
				case TEX_TYPE_COMPRESSED_DXT1_TO_DXT3:
				case TEX_TYPE_COMPRESSED_DXT3:
				case TEX_TYPE_COMPRESSED_DXT5:
					guard(glCompressedTexImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glCompressedTexImage2DARB = %u\n", s_c++);
}
#endif
					glCompressedTexImage2DARB(
						GL_TEXTURE_2D,
						Level,
						pBind->texInternalFormat,
						texWidth,
						texHeight,
						0,
						(pBind->texType == TEX_TYPE_COMPRESSED_DXT1) ? (texWidth * texHeight / 2) : (texWidth * texHeight),
						(pBind->texType == TEX_TYPE_COMPRESSED_DXT1_TO_DXT3) ? Src : Mip->DataPtr);
					unguard;

					break;

				default:
					guard(glTexImage2D);
#if 0
{
	static unsigned int s_c;
	dbgPrintf("utglr: glTexImage2D = %u\n", s_c++);
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

	uncycle(ImageCycles);

	//Restore palette index 0 for masked paletted textures
	if (Info.Palette && (PolyFlags & PF_Masked)) {
		Info.Palette[0] = paletteIndex0;
	}

	// Cleanup.
	if (SupportsLazyTextures) {
		Info.Unload();
	}

	return;
}

void UOpenGLRenderDevice::CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags) {
#if 0
{
	dbgPrintf("utglr: CacheId = %08X:%08X\n",
		(DWORD)((QWORD)Info.CacheID >> 32), (DWORD)((QWORD)Info.CacheID & 0xFFFFFFFF));
}
{
	const UTexture *pTexture = Info.Texture;
	const TCHAR *pName = pTexture->GetFullName();
	if (pName) dbgPrintf("utglr: TexName = %s\n", appToAnsi(pName));
}
{
	dbgPrintf("utglr: NumMips = %d\n", Info.NumMips);
}
{
	unsigned int u;
	TCHAR dbgStr[1024];
	TCHAR numStr[32];

	dbgStr[0] = _T('\0');
	appStrcat(dbgStr, TEXT("utglr: ZPBindTree Size = "));
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		appSprintf(numStr, TEXT("%u"), m_zeroPrefixBindTrees[u].calc_size());
		appStrcat(dbgStr, numStr);
		if (u != (NUM_CTTree_TREES - 1)) appStrcat(dbgStr, TEXT(", "));
	}
	dbgPrintf("%s\n", appToAnsi(dbgStr));

	dbgStr[0] = _T('\0');
	appStrcat(dbgStr, TEXT("utglr: NZPBindTree Size = "));
	for (u = 0; u < NUM_CTTree_TREES; u++) {
		appSprintf(numStr, TEXT("%u"), m_nonZeroPrefixBindTrees[u].calc_size());
		appStrcat(dbgStr, numStr);
		if (u != (NUM_CTTree_TREES - 1)) appStrcat(dbgStr, TEXT(", "));
	}
	dbgPrintf("%s\n", appToAnsi(dbgStr));
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
	MaxLevel = Min(UBits, VBits) - MinLogTextureSize;
	if (MaxLevel < 0) {
		MaxLevel = 0;
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

	pBind->texType = TEX_TYPE_NONE;
	if (SupportsTC) {
		switch (Info.Format) {
		case TEXF_DXT1:
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
			break;

#ifdef UTGLR_UNREAL_227_BUILD
		case TEXF_DXT3:
			pBind->texType = TEX_TYPE_COMPRESSED_DXT3;
			//texSourceFormat not used for compressed textures
			pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
			break;

		case TEXF_DXT5:
			pBind->texType = TEX_TYPE_COMPRESSED_DXT5;
			//texSourceFormat not used for compressed textures
			pBind->texInternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			break;
#endif

		default:
			;
		}
	}
	if (pBind->texType != TEX_TYPE_NONE) {
		//Using compressed texture
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertDXT1_DXT3);
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_P8);
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_P8_NoStep);
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_RGBA8888);
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertP8_RGBA8888_NoStep);
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertBGRA7777_BGRA8888);
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertBGRA7777_BGRA8888_NoClamp);
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

	UTGLR_DEBUG_TEX_CONVERT_COUNT(ConvertBGRA7777_RGBA8888);
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
#elif UTGLR_UNREAL_227_BUILD
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
#elif UTGLR_UNREAL_227_BUILD
			else if (blendFlags & PF_AlphaBlend) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
#endif
			else if (blendFlags & PF_Masked) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
		}
	}
#ifdef UTGLR_UNREAL_227_BUILD
	if (Xor & (PF_Masked | PF_AlphaBlend)) {
#else
	if (Xor & PF_Masked) {
#endif
		if (blendFlags & PF_Masked) {
#ifdef UTGLR_UNREAL_227_BUILD
			glAlphaFunc(GL_GREATER, 0.5f);
			glEnable(GL_ALPHA_TEST);
			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = true;
				glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			}
		}
		else if (blendFlags & PF_AlphaBlend) {
			glAlphaFunc(GL_GREATER, 0.01f);
#endif
			glEnable(GL_ALPHA_TEST);
			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = true;
				glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			}
		}
		else {
			glDisable(GL_ALPHA_TEST);
			if (m_useAlphaToCoverageForMasked) {
				m_alphaToCoverageEnabled = false;
				glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
			}
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
		if ((texEnvFlags & PF_FlatShaded) || ((texUnit != 0) && !OneXBlending)) {
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


void UOpenGLRenderDevice::SetVertexProgramNoCheck(GLuint vpId) {
	//Check if no vertex program and may need to disable vertex program mode
	if (vpId == 0) {
		if (m_vpCurrent != 0) {
			//Id of 0 marks vertex program mode as disabled
			m_vpCurrent = 0;

			//Disable vertex program mode
			glDisable(GL_VERTEX_PROGRAM_ARB);
		}

		return;
	}

	//Check if need to enable vertex program mode
	if (m_vpCurrent == 0) {
		//Id of not 0, to be set later, marks vertex program mode as enabled

		//Enable vertex program mode
		glEnable(GL_VERTEX_PROGRAM_ARB);

		m_vpEnableCount++;
	}

	//Save the new current vertex program
	m_vpCurrent = vpId;

	//Bind the vertex program
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpId);

	m_vpSwitchCount++;

	return;
}

void UOpenGLRenderDevice::SetFragmentProgramNoCheck(GLuint fpId) {
	//Check if no fragment program and may need to disable fragment program mode
	if (fpId == 0) {
		if (m_fpCurrent != 0) {
			//Id of 0 marks fragment program mode as disabled
			m_fpCurrent = 0;

			//Disable fragment program mode
			glDisable(GL_FRAGMENT_PROGRAM_ARB);
		}

		return;
	}

	//Check if need to enable fragment program mode
	if (m_fpCurrent == 0) {
		//Id of not 0, to be set later, marks fragment program mode as enabled

		//Enable fragment program mode
		glEnable(GL_FRAGMENT_PROGRAM_ARB);

		m_fpEnableCount++;
	}

	//Save the new current fragment program
	m_fpCurrent = fpId;

	//Bind the fragment program
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpId);

	m_fpSwitchCount++;

	return;
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


bool UOpenGLRenderDevice::LoadVertexProgram(GLuint vpId, const char *pProgram, const char *pName) {
	GLint iErrorPos;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: Loading vertex program \"%s\"\n", pName);
	}

	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, vpId);
	glProgramStringARB(GL_VERTEX_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(pProgram), pProgram);

	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);

	if (DebugBit(DEBUG_BIT_BASIC)) {
		if (iErrorPos != -1) {
			dbgPrintf("utglr: Vertex program error at offset %d\n", iErrorPos);
			dbgPrintf("utglr: Vertex program text from error offset:\n%s\n", pProgram + iErrorPos);
		}
	}

	if (iErrorPos != -1) {
		return false;
	}

	return true;
}

bool UOpenGLRenderDevice::LoadFragmentProgram(GLuint fpId, const char *pProgram, const char *pName) {
	GLint iErrorPos;

	if (DebugBit(DEBUG_BIT_BASIC)) {
		dbgPrintf("utglr: Loading fragment program \"%s\"\n", pName);
	}

	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, fpId);
	glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB, GL_PROGRAM_FORMAT_ASCII_ARB, strlen(pProgram), pProgram);

	glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &iErrorPos);

	if (DebugBit(DEBUG_BIT_BASIC)) {
		if (iErrorPos != -1) {
			dbgPrintf("utglr: Fragment program error at offset %d\n", iErrorPos);
			dbgPrintf("utglr: Fragment program text from error offset:\n%s\n", pProgram + iErrorPos);
		}
	}

	if (iErrorPos != -1) {
		return false;
	}

	return true;
}


void UOpenGLRenderDevice::AllocateFragmentProgramNamesSafe(void) {
	//Do not allocate names if already allocated
	if (m_allocatedShaderNames) {
		return;
	}

	//Allocate vertex program names
	glGenProgramsARB(1, &m_vpDefaultRenderingState);
	glGenProgramsARB(1, &m_vpDefaultRenderingStateWithFog);
	glGenProgramsARB(1, &m_vpDefaultRenderingStateWithLinearFog);
	glGenProgramsARB(MAX_TMUNITS, m_vpComplexSurface);
	glGenProgramsARB(1, &m_vpComplexSurfaceSingleTextureWithPos);
	glGenProgramsARB(1, &m_vpComplexSurfaceDualTextureWithPos);
	glGenProgramsARB(1, &m_vpComplexSurfaceTripleTextureWithPos);

	//Allocate fragment program names
	glGenProgramsARB(1, &m_fpDefaultRenderingState);
	glGenProgramsARB(1, &m_fpDefaultRenderingStateWithFog);
	glGenProgramsARB(1, &m_fpDefaultRenderingStateWithLinearFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceSingleTexture);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated);
	glGenProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulated);
	glGenProgramsARB(1, &m_fpComplexSurfaceSingleTextureWithFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceDualTextureModulatedWithFog);
	glGenProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulatedWithFog);
	glGenProgramsARB(1, &m_fpDetailTexture);
	glGenProgramsARB(1, &m_fpDetailTextureTwoLayer);
	glGenProgramsARB(1, &m_fpSingleTextureAndDetailTexture);
	glGenProgramsARB(1, &m_fpSingleTextureAndDetailTextureTwoLayer);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTexture);
	glGenProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayer);

	//Mark names as allocated
	m_allocatedShaderNames = true;

	return;
}

void UOpenGLRenderDevice::FreeFragmentProgramNamesSafe(void) {
	//Do not free names if not allocated
	if (!m_allocatedShaderNames) {
		return;
	}

	//Free vertex program names
	glDeleteProgramsARB(1, &m_vpDefaultRenderingState);
	glDeleteProgramsARB(1, &m_vpDefaultRenderingStateWithFog);
	glDeleteProgramsARB(1, &m_vpDefaultRenderingStateWithLinearFog);
	glDeleteProgramsARB(MAX_TMUNITS, m_vpComplexSurface);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceSingleTextureWithPos);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceDualTextureWithPos);
	glDeleteProgramsARB(1, &m_vpComplexSurfaceTripleTextureWithPos);

	//Free fragment program names
	glDeleteProgramsARB(1, &m_fpDefaultRenderingState);
	glDeleteProgramsARB(1, &m_fpDefaultRenderingStateWithFog);
	glDeleteProgramsARB(1, &m_fpDefaultRenderingStateWithLinearFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceSingleTexture);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulated);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulated);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceSingleTextureWithFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceDualTextureModulatedWithFog);
	glDeleteProgramsARB(1, &m_fpComplexSurfaceTripleTextureModulatedWithFog);
	glDeleteProgramsARB(1, &m_fpDetailTexture);
	glDeleteProgramsARB(1, &m_fpDetailTextureTwoLayer);
	glDeleteProgramsARB(1, &m_fpSingleTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_fpSingleTextureAndDetailTextureTwoLayer);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTexture);
	glDeleteProgramsARB(1, &m_fpDualTextureAndDetailTextureTwoLayer);

	//Mark names as not allocated
	m_allocatedShaderNames = false;

	return;
}

bool UOpenGLRenderDevice::InitializeFragmentPrograms(void) {
	bool initOk = true;


	//Vertex programs

	//Default rendering state
	initOk &= LoadVertexProgram(m_vpDefaultRenderingState, g_vpDefaultRenderingState,
		"Default rendering state");

	//Default rendering state with fog
	initOk &= LoadVertexProgram(m_vpDefaultRenderingStateWithFog, g_vpDefaultRenderingStateWithFog,
		"Default rendering state with fog");

	//Default rendering state with linear fog
	initOk &= LoadVertexProgram(m_vpDefaultRenderingStateWithLinearFog, g_vpDefaultRenderingStateWithLinearFog,
		"Default rendering state with linear fog");


	//Complex surface single texture
	initOk &= LoadVertexProgram(m_vpComplexSurface[0], g_vpComplexSurfaceSingleTexture,
		"Complex surface single texture");

	//Complex surface dual texture
	initOk &= LoadVertexProgram(m_vpComplexSurface[1], g_vpComplexSurfaceDualTexture,
		"Complex surface dual texture");

	//Complex surface triple texture
	initOk &= LoadVertexProgram(m_vpComplexSurface[2], g_vpComplexSurfaceTripleTexture,
		"Complex surface triple texture");

	//Complex surface quad texture
	initOk &= LoadVertexProgram(m_vpComplexSurface[3], g_vpComplexSurfaceQuadTexture,
		"Complex surface quad texture");


	//Complex surface single texture with position
	initOk &= LoadVertexProgram(m_vpComplexSurfaceSingleTextureWithPos, g_vpComplexSurfaceSingleTextureWithPos,
		"Complex surface single texture with position");

	//Complex surface dual texture with position
	initOk &= LoadVertexProgram(m_vpComplexSurfaceDualTextureWithPos, g_vpComplexSurfaceDualTextureWithPos,
		"Complex surface dual texture with position");

	//Complex surface triple texture with position
	initOk &= LoadVertexProgram(m_vpComplexSurfaceTripleTextureWithPos, g_vpComplexSurfaceTripleTextureWithPos,
		"Complex surface triple texture with position");


	//Reset to default vertex program and update current vertex program variable
	glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
	glDisable(GL_VERTEX_PROGRAM_ARB);
	m_vpCurrent = 0;


	//Fragment programs

	//Default rendering state
	initOk &= LoadFragmentProgram(m_fpDefaultRenderingState, g_fpDefaultRenderingState,
		"Default rendering state");

	//Default rendering state with fog
	initOk &= LoadFragmentProgram(m_fpDefaultRenderingStateWithFog, g_fpDefaultRenderingStateWithFog,
		"Default rendering state with fog");

	//Default rendering state with linear fog
	initOk &= LoadFragmentProgram(m_fpDefaultRenderingStateWithLinearFog, g_fpDefaultRenderingStateWithLinearFog,
		"Default rendering state with linear fog");


	//Complex surface single texture
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceSingleTexture, g_fpComplexSurfaceSingleTexture,
		"Complex surface single texture");

	//Complex surface dual texture modulated
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulated, g_fpComplexSurfaceDualTextureModulated,
		"Complex surface dual texture modulated");

	//Complex surface triple texture modulated
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceTripleTextureModulated, g_fpComplexSurfaceTripleTextureModulated,
		"Complex surface triple texture modulated");


	//Complex surface single texture with fog
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceSingleTextureWithFog, g_fpComplexSurfaceSingleTextureWithFog,
		"Complex surface single texture with fog");

	//Complex surface dual texture modulated with fog
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceDualTextureModulatedWithFog, g_fpComplexSurfaceDualTextureModulatedWithFog,
		"Complex surface dual texture modulated with fog");

	//Complex surface triple texture modulated with fog
	initOk &= LoadFragmentProgram(m_fpComplexSurfaceTripleTextureModulatedWithFog, g_fpComplexSurfaceTripleTextureModulatedWithFog,
		"Complex surface triple texture modulated with fog");


	//Detail texture
	initOk &= LoadFragmentProgram(m_fpDetailTexture, g_fpDetailTexture,
		"Detail texture");

	//Detail texture two layer
	initOk &= LoadFragmentProgram(m_fpDetailTextureTwoLayer, g_fpDetailTextureTwoLayer,
		"Detail texture two layer");

	//Single texture and detail texture
	initOk &= LoadFragmentProgram(m_fpSingleTextureAndDetailTexture, g_fpSingleTextureAndDetailTexture,
		"Complex surface single texture and detail texture");

	//Single texture and detail texture two layer
	initOk &= LoadFragmentProgram(m_fpSingleTextureAndDetailTextureTwoLayer, g_fpSingleTextureAndDetailTextureTwoLayer,
		"Complex surface single texture and detail texture two layer");

	//Dual texture and detail texture
	initOk &= LoadFragmentProgram(m_fpDualTextureAndDetailTexture, g_fpDualTextureAndDetailTexture,
		"Complex surface dual texture and detail texture");

	//Dual texture and detail texture two layer
	initOk &= LoadFragmentProgram(m_fpDualTextureAndDetailTextureTwoLayer, g_fpDualTextureAndDetailTextureTwoLayer,
		"Complex surface dual texture and detail texture two layer");


	//Reset to default fragment program and update current fragment program variable
	glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
	glDisable(GL_FRAGMENT_PROGRAM_ARB);
	m_fpCurrent = 0;

	return initOk;
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

		if (DebugBit(DEBUG_BIT_BASIC)) dbgPrintf("utglr: Fragment program initialization failed\n");
	}

	return;
}

//Shuts down fragment program mode if it is active
//Freeing the fragment program names takes care of releasing resources
//Safe to call even if fragment program mode is not supported or was never initialized
void UOpenGLRenderDevice::ShutdownFragmentProgramMode(void) {
	//Free fragment program names
	FreeFragmentProgramNamesSafe();

	//Disable vertex program mode if it was enabled
	if (m_vpCurrent != 0) {
		//Disable vertex program mode
		glBindProgramARB(GL_VERTEX_PROGRAM_ARB, 0);
		glDisable(GL_VERTEX_PROGRAM_ARB);
		m_vpCurrent = 0;
	}

	//Disable fragment program mode if it was enabled
	if (m_fpCurrent != 0) {
		//Disable fragment program mode
		glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
		glDisable(GL_FRAGMENT_PROGRAM_ARB);
		m_fpCurrent = 0;
	}

	return;
}


void UOpenGLRenderDevice::SetProjectionStateNoCheck(bool requestNearZRangeHackProjection) {
	FLOAT zNear;
	FLOAT zFar;

	//Save new Z range hack projection state
	m_nearZRangeHackProjectionActive = requestNearZRangeHackProjection;

	//Select projection matrix and reset to identity
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	//Set default zNear
	zNear = 0.5f;

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

	//Set zFar
#ifdef UTGLR_UNREAL_227_BUILD
	zFar = 49152.0f;
#else
	zFar = 32768.0f;
#endif

	glFrustum(-m_RProjZ * zNear, +m_RProjZ * zNear, -m_Aspect*m_RProjZ * zNear, +m_Aspect*m_RProjZ * zNear, 1.0 * zNear, zFar);

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
	dbgPrintf("utglr: PassCount = %d\n", m_rpPassCount);
}
#endif
	m_rpPassCount = 0;


	unguard;
}

void UOpenGLRenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
	guard(UOpenGLRenderDevice::RenderPassesExec_SingleOrDualTextureAndDetailTexture);

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
	dbgPrintf("utglr: PassCount = %d\n", m_rpPassCount);
}
#endif
	m_rpPassCount = 0;


	unguard;
}

//Must be called with (m_rpPassCount > 0)
void UOpenGLRenderDevice::RenderPassesNoCheckSetup(void) {
	INT i;
	INT t;

	SetDefaultShaderState();

	glColor3fv(m_complexSurfaceColor3f_1f);

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
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_FP(void) {
	INT i;
	GLuint fpId = 0;

	glColor4fv(m_complexSurfaceColor3f_1f);

	SetBlend(MultiPass.TMU[0].PolyFlags);

	//Look for a fragment program that can use if they're enabled
	if (UseFragmentProgram) {
		if (m_rpPassCount == 1) {
			fpId = m_fpComplexSurfaceSingleTexture;
		}
		else if (m_rpPassCount == 2) {
			if (MultiPass.TMU[1].PolyFlags == PF_Modulated) {
				fpId = m_fpComplexSurfaceDualTextureModulated;
			}
			else if (MultiPass.TMU[1].PolyFlags == PF_Highlighted) {
				fpId = m_fpComplexSurfaceSingleTextureWithFog;
			}
		}
		else if (m_rpPassCount == 3) {
			if (MultiPass.TMU[2].PolyFlags == PF_Modulated) {
				fpId = m_fpComplexSurfaceTripleTextureModulated;
			}
			else if (MultiPass.TMU[2].PolyFlags == PF_Highlighted) {
				fpId = m_fpComplexSurfaceDualTextureModulatedWithFog;
			}
		}
		else if (m_rpPassCount == 4) {
			if (MultiPass.TMU[3].PolyFlags == PF_Highlighted) {
				fpId = m_fpComplexSurfaceTripleTextureModulatedWithFog;
			}
		}
	}

	//Check if found a fragment program to use
	//All possible combinations are supposed to have a fragment program
	if (fpId == 0) {
		fpId = m_fpComplexSurfaceSingleTexture;
	}
	SetShaderState(m_vpComplexSurface[m_rpPassCount - 1], fpId);

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

	SetDefaultShaderState();

	glColor3fv(m_detailTextureColor3f_1f);

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
void UOpenGLRenderDevice::RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	INT i;
	DWORD detailTexUnit;
	GLuint vpId = 0;
	GLuint fpId = 0;

	//One extra texture unit used for detail texture
	m_rpPassCount += 1;

	//Detail texture is in the last texture unit
	detailTexUnit = (m_rpPassCount - 1);

	if (m_rpPassCount == 2) {
		vpId = m_vpComplexSurfaceDualTextureWithPos;
	}
	else {
		vpId = m_vpComplexSurfaceTripleTextureWithPos;
	}
	if (DetailMax >= 2) {
		if (m_rpPassCount == 2) {
			fpId = m_fpSingleTextureAndDetailTextureTwoLayer;
		}
		else {
			fpId = m_fpDualTextureAndDetailTextureTwoLayer;
		}
	}
	else {
		if (m_rpPassCount == 2) {
			fpId = m_fpSingleTextureAndDetailTexture;
		}
		else {
			fpId = m_fpDualTextureAndDetailTexture;
		}
	}
	SetShaderState(vpId, fpId);

	glColor4fv(m_detailTextureColor3f_1f);

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

	SetDefaultShaderState();

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
void UOpenGLRenderDevice::DrawDetailTexture_FP(FTextureInfo &DetailTextureInfo) {
	INT Index = 0;
	GLuint fpId;

	//Setup detail texture state
	SetBlend(PF_Modulated);

	fpId = m_fpDetailTexture;
	if (DetailMax >= 2) fpId = m_fpDetailTextureTwoLayer;
	SetShaderState(m_vpComplexSurfaceSingleTextureWithPos, fpId);

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
	INT numVerts = 0;

	//Buffer static geometry
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
		MultiDrawFirstArray[csPolyCount] = numVerts;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		numVerts += NumPts;
		if (numVerts > VERTEX_ARRAY_SIZE) {
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

	return numVerts;
}

INT UOpenGLRenderDevice::BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet& Facet) {
	INT numVerts = 0;

	//Buffer static geometry
	m_csPolyCount = 0;
	FGLVertex *pVertex = &VertexArray[0];
	for (FSavedPoly* Poly = Facet.Polys; Poly; Poly = Poly->Next) {
		//Skip if no points
		INT NumPts = Poly->NumPts;
		if (NumPts <= 0) {
			continue;
		}

		DWORD csPolyCount = m_csPolyCount;
		MultiDrawFirstArray[csPolyCount] = numVerts;
		MultiDrawCountArray[csPolyCount] = NumPts;
		m_csPolyCount = csPolyCount + 1;

		numVerts += NumPts;
		if (numVerts > VERTEX_ARRAY_SIZE) {
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

	return numVerts;
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
	//Shader state set when start buffering
	//Default texture state set when start buffering

	cycle(GouraudCycles);

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

	uncycle(GouraudCycles);
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
	//Shader state set when start buffering
	//Default texture state set when start buffering

	cycle(TileCycles);

	//Set color state
	SetColorState();

	//Draw the quads
	glDrawArrays(GL_QUADS, 0, BufferedTileVerts);

	BufferedTileVerts = 0;

	uncycle(TileCycles);
}


// Static variables.
INT UOpenGLRenderDevice::NumDevices = 0;
INT UOpenGLRenderDevice::LockCount  = 0;
UBOOL UOpenGLRenderDevice::GLLoaded = false;

UOpenGLRenderDevice::DWORD_CTTree_t UOpenGLRenderDevice::m_sharedZeroPrefixBindTrees[NUM_CTTree_TREES];
UOpenGLRenderDevice::QWORD_CTTree_t UOpenGLRenderDevice::m_sharedNonZeroPrefixBindTrees[NUM_CTTree_TREES];
CCachedTextureChain UOpenGLRenderDevice::m_sharedNonZeroPrefixBindChain;
UOpenGLRenderDevice::QWORD_CTTree_NodePool_t UOpenGLRenderDevice::m_sharedNonZeroPrefixTexIdPool;
UOpenGLRenderDevice::TexPoolMap_t UOpenGLRenderDevice::m_sharedRGBA8TexPool;

//OpenGL 1.x function pointers for remaining subset to be used with OpenGL 3.2
#define GL1_PROC(ret, func, params) ret (STDCALL *UOpenGLRenderDevice::func)params;
#include "OpenGL1Funcs.h"
#undef GL1_PROC

//OpenGL extension function pointers
#define GL_EXT_NAME(name) bool UOpenGLRenderDevice::SUPPORTS##name = 0;
#define GL_EXT_PROC(ext, ret, func, params) ret (STDCALL *UOpenGLRenderDevice::func)params;
#include "OpenGLExtFuncs.h"
#undef GL_EXT_NAME
#undef GL_EXT_PROC

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
