/*=============================================================================
	OpenGL.h: Unreal OpenGL support header.
	Portions copyright 1999 Epic Games, Inc. All Rights Reserved.

	Revision history:

=============================================================================*/


#include "OpenGLDrv.h"


//Make sure valid build config selected
#undef UTGLR_VALID_BUILD_CONFIG

#ifdef UTGLR_UT_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_DX_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_RUNE_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif
#ifdef UTGLR_UNREAL_227_BUILD
#define UTGLR_VALID_BUILD_CONFIG 1
#endif

#if !UTGLR_VALID_BUILD_CONFIG
#error Valid build config not selected.
#endif
#undef UTGLR_VALID_BUILD_CONFIG


#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>


#include <map>
#pragma warning(disable : 4663)
#pragma warning(disable : 4018)
#include <vector>

#pragma warning(disable : 4244)
#pragma warning(disable : 4245)

#pragma warning(disable : 4146)

#include "c_gclip.h"


#ifdef _WIN32

//Optional ASM code
#define UTGLR_USE_ASM_CODE

//Optional SSE code
#define UTGLR_INCLUDE_SSE_CODE

#endif

#ifdef UTGLR_INCLUDE_SSE_CODE
#include <xmmintrin.h>
#include <emmintrin.h>
#endif


//Optional opcode patch code
//#define UTGLR_INCLUDE_OPCODE_PATCH_CODE

#ifdef UTGLR_INCLUDE_OPCODE_PATCH_CODE
#include "OpcodePatch.h"
#endif


//Optional fastcall calling convention usage
#define UTGLR_USE_FASTCALL

#ifdef UTGLR_USE_FASTCALL
	#ifdef _WIN32
	#define FASTCALL	__fastcall
	#else
	#define FASTCALL
	#endif
#else
#define FASTCALL
#endif


//Debug defines
//#define UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
//#define UTGLR_DEBUG_SHOW_CALL_COUNTS
//#define UTGLR_DEBUG_WORLD_WIREFRAME
//#define UTGLR_DEBUG_ACTOR_WIREFRAME
//#define UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME


#ifdef UTGLR_DEBUG_SHOW_CALL_COUNTS
#define UTGLR_DEBUG_CALL_COUNT(name) \
	{ \
		static unsigned int s_c; \
		dbgPrintf("utglr: " #name " = %u\n", s_c); \
		s_c++; \
	}
#else
#define UTGLR_DEBUG_CALL_COUNT(name)
#endif

#ifdef UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
#define UTGLR_DEBUG_TEX_CONVERT_COUNT(name) \
	{ \
		static unsigned int s_c; \
		dbgPrintf("utglr: " #name " = %u\n", s_c); \
		s_c++; \
	}
#else
#define UTGLR_DEBUG_TEX_CONVERT_COUNT(name)
#endif


#include "c_rbtree.h"


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

//If exceeds 7, various things will break
#define MAX_TMUNITS			4		// vogel: maximum number of texture mapping units supported

//Must be at least 2000
#define VERTEX_ARRAY_SIZE	4000	// vogel: better safe than sorry


/*-----------------------------------------------------------------------------
	OpenGLDrv.
-----------------------------------------------------------------------------*/

class UOpenGLRenderDevice;


enum bind_type_t {
	BIND_TYPE_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX,
	BIND_TYPE_NON_ZERO_PREFIX_LRU_LIST
};

#define CT_MIN_FILTER_NEAREST					0x00
#define CT_MIN_FILTER_LINEAR					0x01
#define CT_MIN_FILTER_NEAREST_MIPMAP_NEAREST	0x02
#define CT_MIN_FILTER_LINEAR_MIPMAP_NEAREST		0x03
#define CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR		0x04
#define CT_MIN_FILTER_LINEAR_MIPMAP_LINEAR		0x05
#define CT_MIN_FILTER_MASK						0x07

#define CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT		0x08

#define CT_ANISOTROPIC_FILTER_BIT				0x10

#define CT_ADDRESS_U_CLAMP						0x20
#define CT_ADDRESS_V_CLAMP						0x40

struct tex_params_t {
	BYTE filter;
	bool hasMipmaps;
	BYTE texObjFilter;
	BYTE reserved3;
};

//Default texture parameters for new OpenGL texture
const tex_params_t CT_DEFAULT_TEX_PARAMS = { CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR | CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT, true, 0, 0 };

#define DT_NO_SMOOTH_BIT	0x01

struct FCachedTexture {
	GLuint Id;
	DWORD LastUsedFrameCount;
	BYTE BaseMip;
	BYTE MaxLevel;
	BYTE UBits, VBits;
	DWORD UClampVal, VClampVal;
	FLOAT UMult, VMult;
	BYTE texType;
	BYTE bindType;
	BYTE treeIndex;
	BYTE dynamicTexBits;
	tex_params_t texParams;
	GLenum texSourceFormat;
	GLenum texInternalFormat;
	union {
		void (FASTCALL UOpenGLRenderDevice::*pConvertBGRA7777)(const FMipmapBase *, INT);
	};
	FCachedTexture *pPrev;
	FCachedTexture *pNext;
};

class CCachedTextureChain {
public:
	CCachedTextureChain() {
		mark_as_clear();
	}
	~CCachedTextureChain() {
	}

	inline void mark_as_clear(void) {
		m_head.pNext = &m_tail;
		m_tail.pPrev = &m_head;
	}

	inline void FASTCALL unlink(FCachedTexture *pCT) {
		pCT->pPrev->pNext = pCT->pNext;
		pCT->pNext->pPrev = pCT->pPrev;
	}
	inline void FASTCALL link_to_tail(FCachedTexture *pCT) {
		pCT->pPrev = m_tail.pPrev;
		pCT->pNext = &m_tail;
		m_tail.pPrev->pNext = pCT;
		m_tail.pPrev = pCT;
	}

	inline FCachedTexture *begin(void) {
		return m_head.pNext;
	}
	inline FCachedTexture *end(void) {
		return &m_tail;
	}

private:
	FCachedTexture m_head;
	FCachedTexture m_tail;
};

template <class ClassT> class rbtree_node_pool {
public:
	typedef typename ClassT::node_t node_t;

public:
	rbtree_node_pool() {
		m_pTail = 0;
	}
	~rbtree_node_pool() {
	}

	inline void FASTCALL add(node_t *pNode) {
		pNode->pParent = m_pTail;

		m_pTail = pNode;

		return;
	}

	inline node_t *try_remove(void) {
		node_t *pNode;

		if (m_pTail == 0) {
			return 0;
		}

		pNode = m_pTail;

		m_pTail = pNode->pParent;

		return pNode;
	}

	unsigned int calc_size(void) {
		node_t *pNode;
		unsigned int size;

		pNode = m_pTail;
		size = 0;
		while (pNode != 0) {
			pNode = pNode->pParent;
			size++;
		}

		return size;
	}

private:
	node_t *m_pTail;
};

struct FTexInfo {
	QWORD CurrentCacheID;
	DWORD CurrentDynamicPolyFlags;
	FCachedTexture *pBind;
	FLOAT UMult;
	FLOAT VMult;
	FLOAT UPan;
	FLOAT VPan;
};

//Geometry
struct FGLVertex {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

//Normals
struct FGLNormal {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

//Tex coords
struct FGLTexCoord {
	FLOAT u;
	FLOAT v;
};

//Primary and secondary (specular) color
struct FGLSingleColor {
	DWORD color;
};
struct FGLDoubleColor {
	DWORD color;
	DWORD specular;
};
struct FGLColorAlloc {
	union {
		FGLSingleColor singleColor;
		FGLDoubleColor doubleColor;
	};
};

struct FGLMapDot {
	FLOAT u;
	FLOAT v;
};


//
// An OpenGL rendering device attached to a viewport.
//
class UOpenGLRenderDevice : public URenderDevice {
#if defined UTGLR_DX_BUILD
	DECLARE_CLASS(UOpenGLRenderDevice, URenderDevice, CLASS_Config)
#else
	DECLARE_CLASS(UOpenGLRenderDevice, URenderDevice, CLASS_Config, OpenGLDrv)
#endif

	//Debug print function
	int dbgPrintf(const char *format, ...);

	//Debug bits
	DWORD m_debugBits;
	inline bool FASTCALL DebugBit(DWORD debugBit) {
		return ((m_debugBits & debugBit) != 0);
	}
	enum {
		DEBUG_BIT_BASIC		= 0x00000001,
		DEBUG_BIT_GL_ERROR	= 0x00000002,
		DEBUG_BIT_ANY		= 0xFFFFFFFF
	};

	//Fixed texture cache ids
	#define TEX_CACHE_ID_UNUSED		0xFFFFFFFFFFFFFFFFULL
	#define TEX_CACHE_ID_NO_TEX		0xFFFFFFFF00000010ULL
	#define TEX_CACHE_ID_ALPHA_TEX	0xFFFFFFFF00000020ULL

	//Texture cache id flags
	enum {
		TEX_CACHE_ID_FLAG_MASKED	= 0x1,
		TEX_CACHE_ID_FLAG_16BIT		= 0x2
	};

	//Mask for poly flags that impact texture object state
	#define TEX_DYNAMIC_POLY_FLAGS_MASK		(PF_NoSmooth)

	// Information about a cached texture.
	enum tex_type_t {
		TEX_TYPE_NONE,
		TEX_TYPE_COMPRESSED_DXT1,
		TEX_TYPE_COMPRESSED_DXT1_TO_DXT3,
		TEX_TYPE_COMPRESSED_DXT3,
		TEX_TYPE_COMPRESSED_DXT5,
		TEX_TYPE_PALETTED,
		TEX_TYPE_HAS_PALETTE,
		TEX_TYPE_NORMAL
	};
	#define TEX_FLAG_NO_CLAMP	0x00000001

	struct FTexConvertCtx {
		BYTE *pCompose;
		INT stepBits;
		DWORD texWidthPow2;
		DWORD texHeightPow2;
		const FCachedTexture *pBind;
	} m_texConvertCtx;

	FMemMark m_texComposeMemMark;
	enum { LOCAL_TEX_COMPOSE_BUFFER_SIZE = 16384 };
	BYTE m_localTexComposeBuffer[LOCAL_TEX_COMPOSE_BUFFER_SIZE + 16];


	inline void * FASTCALL AlignMemPtr(void *ptr, size_t align) {
		return (void *)(((uintptr_t)ptr + (align - 1)) & -align);
	}
	enum { VERTEX_ARRAY_ALIGN = 64 };	//Must be even multiple of 16B for SSE
	enum { VERTEX_ARRAY_TAIL_PADDING = 72 };	//Must include 8B for half SSE tail

	//Geometry
	FGLVertex *VertexArray;
	BYTE m_VertexArrayMem[(sizeof(FGLVertex) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	//Normals
	FGLNormal *NormalArray;
	BYTE m_NormalArrayMem[(sizeof(FGLNormal) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	//Tex coords
	FGLTexCoord *TexCoordArray[MAX_TMUNITS];
	BYTE m_TexCoordArrayMem[MAX_TMUNITS][(sizeof(FGLTexCoord) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	//Primary and secondary (specular) color
	FGLSingleColor *SingleColorArray;
	FGLDoubleColor *DoubleColorArray;
	BYTE m_ColorArrayMem[(sizeof(FGLColorAlloc) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	FGLMapDot *MapDotArray;
	BYTE m_MapDotArrayMem[(sizeof(FGLMapDot) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	//IsNear bits for detail texturing
	DWORD DetailTextureIsNearArray[VERTEX_ARRAY_SIZE / 3];

	//First and count arrays for glMultiDrawArrays
	GLint MultiDrawFirstArray[VERTEX_ARRAY_SIZE / 3];
	GLsizei MultiDrawCountArray[VERTEX_ARRAY_SIZE / 3];

	DWORD m_csPolyCount;
	INT m_csPtCount;

	FLOAT m_csUDot;
	FLOAT m_csVDot;


	// MultiPass rendering information
	struct FGLRenderPass {
		struct FGLSinglePass {
			FTextureInfo* Info;
			DWORD PolyFlags;
			FLOAT PanBias;
		} TMU[MAX_TMUNITS];
	} MultiPass;				// vogel: MULTIPASS!!! ;)

	//Texture state cache information
	BYTE m_texEnableBits;
	BYTE m_clientTexEnableBits;

	//Vertex program and fragment program cache information
	GLuint m_vpCurrent;
	GLuint m_fpCurrent;

	//Vertex program and fragment program id tracking
	bool m_allocatedShaderNames;

	//Vertex program ids
	GLuint m_vpDefaultRenderingState;
	GLuint m_vpDefaultRenderingStateWithFog;
	GLuint m_vpDefaultRenderingStateWithLinearFog;
	GLuint m_vpComplexSurface[MAX_TMUNITS];
	GLuint m_vpComplexSurfaceSingleTextureWithPos;
	GLuint m_vpComplexSurfaceDualTextureWithPos;
	GLuint m_vpComplexSurfaceTripleTextureWithPos;

	//Fragment program ids
	GLuint m_fpDefaultRenderingState;
	GLuint m_fpDefaultRenderingStateWithFog;
	GLuint m_fpDefaultRenderingStateWithLinearFog;
	GLuint m_fpComplexSurfaceSingleTexture;
	GLuint m_fpComplexSurfaceDualTextureModulated;
	GLuint m_fpComplexSurfaceTripleTextureModulated;
	GLuint m_fpComplexSurfaceSingleTextureWithFog;
	GLuint m_fpComplexSurfaceDualTextureModulatedWithFog;
	GLuint m_fpComplexSurfaceTripleTextureModulatedWithFog;
	GLuint m_fpDetailTexture;
	GLuint m_fpDetailTextureTwoLayer;
	GLuint m_fpSingleTextureAndDetailTexture;
	GLuint m_fpSingleTextureAndDetailTextureTwoLayer;
	GLuint m_fpDualTextureAndDetailTexture;
	GLuint m_fpDualTextureAndDetailTextureTwoLayer;


	struct FGammaRamp {
		_WORD red[256];
		_WORD green[256];
		_WORD blue[256];
	};
	struct FByteGammaRamp {
		BYTE red[256];
		BYTE green[256];
		BYTE blue[256];
	};

	UBOOL WasFullscreen;

	bool m_frameRateLimitTimerInitialized;

	bool m_prevSwapBuffersStatus;

	typedef rbtree<DWORD, FCachedTexture> DWORD_CTTree_t;
	typedef rbtree_allocator<DWORD_CTTree_t> DWORD_CTTree_Allocator_t;
	typedef rbtree<QWORD, FCachedTexture> QWORD_CTTree_t;
	typedef rbtree_allocator<QWORD_CTTree_t> QWORD_CTTree_Allocator_t;
	typedef rbtree_node_pool<QWORD_CTTree_t> QWORD_CTTree_NodePool_t;
	typedef _WORD TexPoolMapKey_t;
	typedef rbtree<TexPoolMapKey_t, QWORD_CTTree_NodePool_t> TexPoolMap_t;
	typedef rbtree_allocator<TexPoolMap_t> TexPoolMap_Allocator_t;

	enum { NUM_CTTree_TREES = 16 }; //Must be a power of 2
	inline DWORD FASTCALL CTZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 12) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD FASTCALL CTNonZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 20) & (NUM_CTTree_TREES - 1));
	}
	inline _WORD FASTCALL MakeTexPoolMapKey(DWORD UBits, DWORD VBits) {
		return (((UBits << 8) | VBits) & 0xFFFF);
	}

	DWORD_CTTree_t m_localZeroPrefixBindTrees[NUM_CTTree_TREES], *m_zeroPrefixBindTrees;
	QWORD_CTTree_t m_localNonZeroPrefixBindTrees[NUM_CTTree_TREES], *m_nonZeroPrefixBindTrees;
	CCachedTextureChain m_localNonZeroPrefixBindChain, *m_nonZeroPrefixBindChain;
	QWORD_CTTree_NodePool_t m_localNonZeroPrefixTexIdPool, *m_nonZeroPrefixTexIdPool;
	TexPoolMap_t m_localRGBA8TexPool, *m_RGBA8TexPool;

	DWORD_CTTree_Allocator_t m_DWORD_CTTree_Allocator;
	QWORD_CTTree_Allocator_t m_QWORD_CTTree_Allocator;
	TexPoolMap_Allocator_t m_TexPoolMap_Allocator;

	QWORD_CTTree_NodePool_t m_nonZeroPrefixNodePool;

	TArray<FPlane> Modes;

	//Use UViewport* in URenderDevice
	//UViewport* Viewport;
	inline SDL_Window* GetWindow() { return (SDL_Window*) Viewport->GetWindow(); }
	SDL_GLContext Context;
	inline INT SetContext() { return SDL_GL_MakeCurrent( GetWindow(), Context ); }


	// Timing.
	DWORD BindCycles, ImageCycles, ComplexCycles, GouraudCycles, TileCycles;

	DWORD m_vpEnableCount;
	DWORD m_vpSwitchCount;
	DWORD m_fpEnableCount;
	DWORD m_fpSwitchCount;
	DWORD m_AASwitchCount;
	DWORD m_sceneNodeCount;
	DWORD m_sceneNodeHackCount;
	DWORD m_stat0Count;
	DWORD m_stat1Count;


	// Hardware constraints.
	struct {
		UBOOL SinglePassFog;
		UBOOL SinglePassDetail;
		UBOOL UseFragmentProgram;
	} DCV;

	FLOAT LODBias;
	FLOAT GammaOffset;
	FLOAT GammaOffsetRed;
	FLOAT GammaOffsetGreen;
	FLOAT GammaOffsetBlue;
	INT Brightness;
	UBOOL GammaCorrectScreenshots;
	UBOOL OneXBlending;
	INT MaxLogUOverV;
	INT MaxLogVOverU;
	INT MinLogTextureSize;
	INT MaxLogTextureSize;
	INT MaxAnisotropy;
	INT TMUnits;
	INT MaxTMUnits;
	INT RefreshRate;
	UBOOL UseZTrick;
	UBOOL UseBGRATextures;
	UBOOL UseMultiTexture;
	UBOOL UsePalette;
	UBOOL ShareLists;
	UBOOL AlwaysMipmap;
	UBOOL UsePrecache;
	UBOOL UseTrilinear;
	UBOOL UseVertexSpecular;
	UBOOL UseAlphaPalette;
	UBOOL UseS3TC;
	UBOOL Use16BitTextures;
	UBOOL NoFiltering;
	INT DetailMax;
	UBOOL UseDetailAlpha;
	UBOOL DetailClipping;
	UBOOL ColorizeDetailTextures;
	UBOOL SinglePassFog;
	UBOOL SinglePassDetail;
	UBOOL UseSSE;
	UBOOL UseSSE2;
	UBOOL UseTexIdPool;
	UBOOL UseTexPool;
	UBOOL CacheStaticMaps;
	INT DynamicTexIdRecycleLevel;
	UBOOL TexDXT1ToDXT3;
	UBOOL UseMultiDrawArrays;
	UBOOL UseFragmentProgram;
	INT SwapInterval;
	INT FrameRateLimit;
#if defined UTGLR_DX_BUILD || defined UTGLR_RUNE_BUILD
	FLOAT m_prevFrameTimestamp;
#else
	FTime m_prevFrameTimestamp;
#endif
	UBOOL SceneNodeHack;
	UBOOL SmoothMaskedTextures;
	UBOOL MaskedTextureHack;

	UBOOL UseAA;
	INT NumAASamples;
	UBOOL NoAATiles;

	UBOOL ZRangeHack;
	bool m_useZRangeHack;
	bool m_nearZRangeHackProjectionActive;
	bool m_requestNearZRangeHackProjection;

	UBOOL BufferActorTris;
	UBOOL BufferClippedActorTris;
	INT BufferedVerts;

	UBOOL BufferTileQuads;
	INT BufferedTileVerts;


	//Previous lock variables
	//Used to detect changes in settings
	BITFIELD PL_DetailTextures;
	UBOOL PL_OneXBlending;
	INT PL_MaxLogUOverV;
	INT PL_MaxLogVOverU;
	INT PL_MinLogTextureSize;
	INT PL_MaxLogTextureSize;
	UBOOL PL_NoFiltering;
	UBOOL PL_AlwaysMipmap;
	UBOOL PL_UseTrilinear;
	UBOOL PL_Use16BitTextures;
	UBOOL PL_TexDXT1ToDXT3;
	INT PL_MaxAnisotropy;
	UBOOL PL_SmoothMaskedTextures;
	UBOOL PL_MaskedTextureHack;
	FLOAT PL_LODBias;
	UBOOL PL_UsePalette;
	UBOOL PL_UseAlphaPalette;
	UBOOL PL_UseDetailAlpha;
	UBOOL PL_SinglePassDetail;
	UBOOL PL_UseFragmentProgram;
	UBOOL PL_UseSSE;
	UBOOL PL_UseSSE2;

	bool m_setGammaRampSucceeded;
	FLOAT SavedGammaCorrection;

	DWORD m_numDepthBits;

	INT AllocatedTextures;

	INT m_rpPassCount;
	INT m_rpTMUnits;
	bool m_rpForceSingle;
	bool m_rpMasked;
	bool m_rpSetDepthEqual;

	void (UOpenGLRenderDevice::*m_pRenderPassesNoCheckSetupProc)(void);
	void (FASTCALL UOpenGLRenderDevice::*m_pRenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTextureProc)(FTextureInfo &);

	DWORD (FASTCALL UOpenGLRenderDevice::*m_pBufferDetailTextureDataProc)(FLOAT);

	// Hit info.
	BYTE* m_HitData;
	INT* m_HitSize;
	INT m_HitBufSize;
	INT m_HitCount;
	CGClip m_gclip;


	DWORD m_currentFrameCount;

	// Lock variables.
	UBOOL ZTrickToggle;
	INT ZTrickFunc;
	FPlane FlashScale, FlashFog;
	FLOAT m_RProjZ, m_Aspect;
	FLOAT m_RFX2, m_RFY2;
	INT m_sceneNodeX, m_sceneNodeY;

	bool m_usingAA;
	bool m_curAAEnable;
	bool m_defAAEnable;
	INT m_initNumAASamples;

	enum {
		PF2_NEAR_Z_RANGE_HACK	= 0x01
	};
	DWORD m_curBlendFlags;
	DWORD m_smoothMaskedTexturesBit;
	bool m_useAlphaToCoverageForMasked;
	bool m_alphaToCoverageEnabled;
	DWORD m_curPolyFlags;
	DWORD m_curPolyFlags2;
	INT m_bufferActorTrisCutoff;

	FLOAT m_complexSurfaceColor3f_1f[4];
	FLOAT m_detailTextureColor3f_1f[4];
	DWORD m_detailTextureColor4ub;

	DWORD m_maskedTextureHackMask;

	enum {
		CF_COLOR_ARRAY			= 0x01,
		CF_DUAL_COLOR_ARRAY		= 0x02,
		CF_COLOR_SUM			= 0x04,
		CF_NORMAL_ARRAY			= 0x08
	};
	BYTE m_currentColorFlags;
	BYTE m_requestedColorFlags;

	BYTE m_gpAlpha;
	bool m_gpFogEnabled;

	DWORD m_curTexEnvFlags[MAX_TMUNITS];
	FTexInfo TexInfo[MAX_TMUNITS];

	void (FASTCALL *m_pBuffer3BasicVertsProc)(UOpenGLRenderDevice *, FTransTexture **);
	void (FASTCALL *m_pBuffer3ColoredVertsProc)(UOpenGLRenderDevice *, FTransTexture **);
	void (FASTCALL *m_pBuffer3FoggedVertsProc)(UOpenGLRenderDevice *, FTransTexture **);

	void (FASTCALL *m_pBuffer3VertsProc)(UOpenGLRenderDevice *, FTransTexture **);

	GLuint m_noTextureId;
	GLuint m_alphaTextureId;

	// Static variables.
	static DWORD_CTTree_t m_sharedZeroPrefixBindTrees[NUM_CTTree_TREES];
	static QWORD_CTTree_t m_sharedNonZeroPrefixBindTrees[NUM_CTTree_TREES];
	static CCachedTextureChain m_sharedNonZeroPrefixBindChain;
	static QWORD_CTTree_NodePool_t m_sharedNonZeroPrefixTexIdPool;
	static TexPoolMap_t m_sharedRGBA8TexPool;
	static INT NumDevices;
	static INT LockCount;

	static UBOOL GLLoaded;

	//OpenGL 1.x function pointers for remaining subset to be used with OpenGL 3.2
	#define GL1_PROC(ret, func, params) static ret (STDCALL *func)params;
	#include "OpenGL1Funcs.h"
	#undef GL1_PROC

	//OpenGL extension function pointers
	#define GL_EXT_NAME(name) static bool SUPPORTS##name;
	#define GL_EXT_PROC(ext, ret, func, params) static ret (STDCALL *func)params;
	#include "OpenGLExtFuncs.h"
	#undef GL_EXT_NAME
	#undef GL_EXT_PROC


#ifdef RGBA_MAKE
#undef RGBA_MAKE
#endif
#if __INTEL_BYTE_ORDER__
	static inline DWORD RGBA_MAKE(BYTE r, BYTE g, BYTE b, BYTE a) {	// vogel: I hate macros...
		return (a << 24) | (b << 16) | (g << 8) | r;
	}																// vogel: ... and macros hate me
#else
    static inline DWORD RGBA_MAKE(BYTE r, BYTE g, BYTE b, BYTE a){
		return (r << 24) | (g <<16) | ( b<< 8) | a;
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGB_A255(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return RGBA_MAKE(iR, iG, iB, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGB_A255(const FPlane *pPlane) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					255);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBClamped_A255(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return RGBA_MAKE(Clamp(iR, 0, 255), Clamp(iG, 0, 255), Clamp(iB, 0, 255), 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBClamped_A255(const FPlane *pPlane) {
		return RGBA_MAKE(
					Clamp(appRound(pPlane->X * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
					255);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGB_A0(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return RGBA_MAKE(iR, iG, iB, 0);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGB_A0(const FPlane *pPlane) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					0);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGB_Aub(const FPlane *pPlane, BYTE alpha) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
		}
		return RGBA_MAKE(iR, iG, iB, alpha);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGB_Aub(const FPlane *pPlane, BYTE alpha) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					alpha);
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBA(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB, iA;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
			fld [eax]FPlane.W
			fmul [f255]
			fistp [iA]
		}
		return RGBA_MAKE(iR, iG, iB, iA);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBA(const FPlane *pPlane) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					appRound(pPlane->W * 255.0f));
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBAClamped(const FPlane *pPlane) {
		static FLOAT f255 = 255.0f;
		INT iR, iG, iB, iA;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [f255]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [f255]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [f255]
			fistp [iB]
			fld [eax]FPlane.W
			fmul [f255]
			fistp [iA]
		}
		return RGBA_MAKE(Clamp(iR, 0, 255), Clamp(iG, 0, 255), Clamp(iB, 0, 255), Clamp(iA, 0, 255));
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBAClamped(const FPlane *pPlane) {
		return RGBA_MAKE(
					Clamp(appRound(pPlane->X * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
					Clamp(appRound(pPlane->W * 255.0f), 0, 255));
	}
#endif

#if defined _WIN32 && defined UTGLR_USE_ASM_CODE
	static inline DWORD FPlaneTo_RGBScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
		INT iR, iG, iB;
		__asm {
			mov eax, pPlane
			fld [eax]FVector.X
			fmul [rgbScale]
			fistp [iR]
			fld [eax]FVector.Y
			fmul [rgbScale]
			fistp [iG]
			fld [eax]FVector.Z
			fmul [rgbScale]
			fistp [iB]
		}
		return RGBA_MAKE(iR, iG, iB, 255);
	}
#else
	static inline DWORD FASTCALL FPlaneTo_RGBScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
		return RGBA_MAKE(
					appRound(pPlane->X * rgbScale),
					appRound(pPlane->Y * rgbScale),
					appRound(pPlane->Z * rgbScale),
					255);
	}
#endif


	// UObject interface.
	void StaticConstructor();


	// Implementation.
	void FASTCALL SC_AddBoolConfigParam(DWORD BitMaskOffset, const TCHAR *pName, UBOOL &param, ECppProperty EC_CppProperty, INT InOffset, UBOOL defaultValue);
	void FASTCALL SC_AddIntConfigParam(const TCHAR *pName, INT &param, ECppProperty EC_CppProperty, INT InOffset, INT defaultValue);
	void FASTCALL SC_AddFloatConfigParam(const TCHAR *pName, FLOAT &param, ECppProperty EC_CppProperty, INT InOffset, FLOAT defaultValue);

	void FASTCALL DbgPrintInitParam(const char *pName, INT value);
	void FASTCALL DbgPrintInitParam(const char *pName, FLOAT value);

#ifdef UTGLR_INCLUDE_SSE_CODE
	static bool CPU_DetectCPUID(void);
	static bool CPU_DetectSSE(void);
	static bool CPU_DetectSSE2(void);
#endif //UTGLR_INCLUDE_SSE_CODE

	void InitFrameRateLimitTimerSafe(void);
	void ShutdownFrameRateLimitTimer(void);

	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FGammaRamp &ramp);
	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FByteGammaRamp &ramp);
	void SetGamma(FLOAT GammaCorrection);
	void ResetGamma(void);

	static bool FASTCALL IsGLExtensionSupported(const char *pExtensionsString, const char *pExtensionName);
	bool FASTCALL GetGL1Proc(void*& ProcAddress, const char *pName);
	bool FASTCALL GetGL1Procs(void);
	bool FASTCALL FindGLExt(const char *pName);
	void FASTCALL GetGLExtProc(void*& ProcAddress, const char *pName, const char *pSupportName, bool& Supports);
	void FASTCALL GetGLExtProcs(void);

	UBOOL FailedInitf(const TCHAR* Fmt, ...);
	void Exit();
	void ShutdownAfterError();

	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void UnsetRes();

	void MakeCurrent(void);
	void CheckGLErrorFlag(const TCHAR *pTag);

	void ConfigValidate_RefreshDCV(void);
	void ConfigValidate_RequiredExtensions(void);
	void ConfigValidate_Main(void);

	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);

	static QSORT_RETURN CDECL CompareRes(const FPlane* A, const FPlane* B) {
		return (QSORT_RETURN) (((A->X - B->X) != 0.0f) ? (A->X - B->X) : (A->Y - B->Y));
	}

	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize);
	void SetSceneNode(FSceneNode* Frame);
	void Unlock(UBOOL Blit);
	void Flush(UBOOL AllowPrecache);

	void DrawComplexSurface(FSceneNode* Frame, FSurfaceInfo& Surface, FSurfaceFacet& Facet);
#ifdef UTGLR_RUNE_BUILD
	void PreDrawFogSurface();
	void PostDrawFogSurface();
	void DrawFogSurface(FSceneNode* Frame, FFogSurf &FogSurf);
	void PreDrawGouraud(FSceneNode* Frame, FLOAT FogDistance, FPlane FogColor);
	void PostDrawGouraud(FLOAT FogDistance);
#endif
	void DrawGouraudPolygonOld(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
	void DrawGouraudPolygon(FSceneNode* Frame, FTextureInfo& Info, FTransTexture** Pts, INT NumPts, DWORD PolyFlags, FSpanBuffer* Span);
	void DrawTile(FSceneNode* Frame, FTextureInfo& Info, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, class FSpanBuffer* Span, FLOAT Z, FPlane Color, FPlane Fog, DWORD PolyFlags);
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw2DPoint(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FLOAT X1, FLOAT Y1, FLOAT X2, FLOAT Y2, FLOAT Z);

	void ClearZ(FSceneNode* Frame);
	void PushHit(const BYTE* Data, INT Count);
	void PopHit(INT Count, UBOOL bForce);
	void GetStats(TCHAR* Result);
	void ReadPixels(FColor* Pixels);
	void EndFlash();
	void PrecacheTexture(FTextureInfo& Info, DWORD PolyFlags);


	void InitNoTextureSafe(void);
	void InitAlphaTextureSafe(void);

	void ScanForDeletedTextures(void);
	void ScanForOldTextures(void);

	inline void FASTCALL SetNoTexture(INT Multi) {
		if (TexInfo[Multi].CurrentCacheID != TEX_CACHE_ID_NO_TEX) {
			SetNoTextureNoCheck(Multi);
		}
	}
	inline void SetAlphaTexture(INT Multi) {
		if (TexInfo[Multi].CurrentCacheID != TEX_CACHE_ID_ALPHA_TEX) {
			SetAlphaTextureNoCheck(Multi);
		}
	}

	void FASTCALL SetNoTextureNoCheck(INT Multi);
	void FASTCALL SetAlphaTextureNoCheck(INT Multi);

	inline void FASTCALL SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias) {
		FTexInfo& Tex = TexInfo[Multi];
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		// Set panning.
		Tex.UPan = Info.Pan.X + (PanBias * Info.UScale);
		Tex.VPan = Info.Pan.Y + (PanBias * Info.VScale);

		//PF_Memorized used internally to indicate 16-bit texture
		PolyFlags &= ~PF_Memorized;

		//Load texture cache id
		CacheID = Info.CacheID;

		//Only attempt to alter texture cache id on certain textures
		if ((CacheID & 0xFF) == 0xE0) {
			//Alter texture cache id if masked texture hack is enabled and texture is masked
			CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

			//Check for 16 bit texture option
			if (Use16BitTextures) {
				if (Info.Palette && (Info.Palette[128].A == 255)) {
					CacheID |= TEX_CACHE_ID_FLAG_16BIT;
					PolyFlags |= PF_Memorized;
				}
			}
		}

		//Get dynamic poly flags
		DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

		// Find in cache.
		if ((CacheID == Tex.CurrentCacheID) && (DynamicPolyFlags == Tex.CurrentDynamicPolyFlags) && !Info.bRealtimeChanged) {
			return;
		}

		//Update soon to be current texture cache id
		Tex.CurrentCacheID = CacheID;
		Tex.CurrentDynamicPolyFlags = DynamicPolyFlags;

		SetTextureNoCheck(Tex, Info, PolyFlags);

		return;
	}

	inline void FASTCALL SetTextureNoPanBias(INT Multi, FTextureInfo& Info, DWORD PolyFlags) {
		FTexInfo& Tex = TexInfo[Multi];
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		// Set panning.
		Tex.UPan = Info.Pan.X;
		Tex.VPan = Info.Pan.Y;

		//PF_Memorized used internally to indicate 16-bit texture
		PolyFlags &= ~PF_Memorized;

		//Load texture cache id
		CacheID = Info.CacheID;

		//Only attempt to alter texture cache id on certain textures
		if ((CacheID & 0xFF) == 0xE0) {
			//Alter texture cache id if masked texture hack is enabled and texture is masked
			CacheID |= ((PolyFlags & PF_Masked) ? TEX_CACHE_ID_FLAG_MASKED : 0) & m_maskedTextureHackMask;

			//Check for 16 bit texture option
			if (Use16BitTextures) {
				if (Info.Palette && (Info.Palette[128].A == 255)) {
					CacheID |= TEX_CACHE_ID_FLAG_16BIT;
					PolyFlags |= PF_Memorized;
				}
			}
		}

		//Get dynamic poly flags
		DynamicPolyFlags = PolyFlags & TEX_DYNAMIC_POLY_FLAGS_MASK;

		// Find in cache.
		if ((CacheID == Tex.CurrentCacheID) && (DynamicPolyFlags == Tex.CurrentDynamicPolyFlags) && !Info.bRealtimeChanged) {
			return;
		}

		//Update soon to be current texture cache id
		Tex.CurrentCacheID = CacheID;
		Tex.CurrentDynamicPolyFlags = DynamicPolyFlags;

		SetTextureNoCheck(Tex, Info, PolyFlags);

		return;
	}

	inline void FASTCALL SetTexFilter(FCachedTexture *pBind, BYTE texFilter) {
		if (pBind->texParams.filter != texFilter) {
			SetTexFilterNoCheck(pBind, texFilter);
		}
	}

	FCachedTexture *FindCachedTexture(QWORD CacheID);
	QWORD_CTTree_NodePool_t::node_t * FASTCALL TryAllocFromTexPool(TexPoolMapKey_t texPoolKey);
	BYTE FASTCALL GenerateTexFilterParams(DWORD PolyFlags, FCachedTexture *pBind);
	void FASTCALL SetTexFilterNoCheck(FCachedTexture *pBind, BYTE texFilter);
	void FASTCALL SetTextureNoCheck(FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags);
	void FASTCALL UploadTextureExec(FTextureInfo& Info, DWORD PolyFlags, FCachedTexture *pBind, bool existingBind, bool needTexAllocate);
	void FASTCALL CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags);

	void FASTCALL ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_P8(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_P8_NoStep(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_RGBA8888(const FMipmapBase *Mip, INT Level);

	inline void FASTCALL SetBlend(DWORD PolyFlags) {
#ifdef UTGLR_RUNE_BUILD
		if (PolyFlags & PF_AlphaBlend) {
			if (!(PolyFlags & PF_Masked)) {
				PolyFlags |= PF_Occlude;
			}
			else {
				PolyFlags &= ~PF_Masked;
			}
		}
		else
#endif
		if (!(PolyFlags & (PF_Translucent | PF_Modulated | PF_Highlighted))) {
			PolyFlags |= PF_Occlude;
		}
		else if (PolyFlags & PF_Translucent) {
			PolyFlags &= ~PF_Masked;
		}

		//Only check relevant blend flags
#ifdef UTGLR_RUNE_BUILD
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted | PF_AlphaBlend);
#elif  UTGLR_UNREAL_227_BUILD
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted | PF_AlphaBlend);
#else
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted);
#endif
		if (m_curBlendFlags != blendFlags) {
			SetBlendNoCheck(blendFlags);
		}
	}
	void FASTCALL SetBlendNoCheck(DWORD blendFlags);

	inline void FASTCALL SetTexEnv(DWORD texUnit, DWORD PolyFlags) {
		//Only check relevant tex env flags
		DWORD texEnvFlags = PolyFlags & (PF_Modulated | PF_Highlighted | PF_Memorized | PF_FlatShaded);
		//Modulated by default
		if ((texEnvFlags & (PF_Modulated | PF_Highlighted | PF_Memorized)) == 0) {
			texEnvFlags |= PF_Modulated;
		}
		if (m_curTexEnvFlags[texUnit] != texEnvFlags) {
			SetTexEnvNoCheck(texUnit, texEnvFlags);
		}
	}
	void InitOrInvalidateTexEnvState(void);
	void FASTCALL SetPermanentTexEnvState(INT TMUnits);
	void FASTCALL SetTexLODBiasState(INT TMUnits);
	void FASTCALL SetTexEnvNoCheck(DWORD texUnit, DWORD texEnvFlags);


	inline void SetDefaultTextureState(void) {
		//Check if only texture unit zero is enabled
		if (m_texEnableBits != 0x1) {
			DWORD texUnit;
			DWORD texBit;

			//Disable all texture units except texture unit zero
			for (texUnit = 1, texBit = 0x2; m_texEnableBits != 0x1; texUnit++, texBit <<= 1) {
				//See if the texture unit is enabled
				if (texBit & m_texEnableBits) {
					//Update tex enable bits (sub to clear known set bit)
					m_texEnableBits -= texBit;

					//Disable the texture unit
					glActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
					glDisable(GL_TEXTURE_2D);
				}
			}

			//Leave texture unit zero active
			if (SUPPORTS_GL_ARB_multitexture) {
				glActiveTextureARB(GL_TEXTURE0_ARB);
			}
		}

		//Check if only client texture zero is enabled
		if (m_clientTexEnableBits != 0x1) {
			DWORD texUnit;
			DWORD texBit;

			//Enable client texture zero if it is disabled
			if ((m_clientTexEnableBits & 0x1) == 0) {
				//Update client tex enable bits
				m_clientTexEnableBits |= 0x1;

				//Enable client texture zero
				if (SUPPORTS_GL_ARB_multitexture) {
					glClientActiveTextureARB(GL_TEXTURE0_ARB);
				}
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			}

			//Disable all client textures except client texture zero
			for (texUnit = 1, texBit = 0x2; m_clientTexEnableBits != 0x1; texUnit++, texBit <<= 1) {
				//See if the client texture is enabled
				if (texBit & m_clientTexEnableBits) {
					//Update client tex enable bits (sub to clear known set bit)
					m_clientTexEnableBits -= texBit;

					//Disable the client texture
					glClientActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
					glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				}
			}
		}

		return;
	}

	inline void FASTCALL DisableSubsequentTextures(DWORD firstTexUnit) {
		DWORD texUnit;
		BYTE texBit = 1U << firstTexUnit;

		//Disable subsequent texture units
		for (texUnit = firstTexUnit; texBit <= m_texEnableBits; texUnit++, texBit <<= 1) {
			//See if the texture unit is enabled
			if (texBit & m_texEnableBits) {
				//Update tex enable bits (sub to clear known set bit)
				m_texEnableBits -= texBit;

				//Disable the texture unit
				if (SUPPORTS_GL_ARB_multitexture) {
					glActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
				}
				glDisable(GL_TEXTURE_2D);
			}
		}

		return;
	}

	inline void FASTCALL DisableSubsequentClientTextures(DWORD firstTexUnit) {
		DWORD texUnit;
		BYTE texBit = 1U << firstTexUnit;

		//Disable subsequent client textures
		for (texUnit = firstTexUnit; texBit <= m_clientTexEnableBits; texUnit++, texBit <<= 1) {
			//See if the client texture is enabled
			if (texBit & m_clientTexEnableBits) {
				//Update client tex enable bits (sub to clear known set bit)
				m_clientTexEnableBits -= texBit;

				//Disable the client texture
				if (SUPPORTS_GL_ARB_multitexture) {
					glClientActiveTextureARB(GL_TEXTURE0_ARB + texUnit);
				}
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			}
		}

		return;
	}

	inline void SetDefaultShaderState(void) {
		//Keep vertex programs enabled if using vertex program mode
		GLuint vpId = (UseFragmentProgram) ? m_vpDefaultRenderingState : 0;
		if (m_vpCurrent != vpId) {
			SetVertexProgramNoCheck(vpId);
		}

		//Keep fragment programs enabled if using fragment program mode
		GLuint fpId = (UseFragmentProgram) ? m_fpDefaultRenderingState : 0;
		if (m_fpCurrent != fpId) {
			SetFragmentProgramNoCheck(fpId);
		}
	}
	inline void FASTCALL SetShaderState(GLuint vpId, GLuint fpId) {
		if (m_vpCurrent != vpId) {
			SetVertexProgramNoCheck(vpId);
		}
		if (m_fpCurrent != fpId) {
			SetFragmentProgramNoCheck(fpId);
		}
	}

	void FASTCALL SetVertexProgramNoCheck(GLuint vpId);
	void FASTCALL SetFragmentProgramNoCheck(GLuint fpId);

	inline void SetDefaultColorState(void) {
		if (m_currentColorFlags != 0) {
			SetDefaultColorStateNoCheck();
		}
	}
	void SetDefaultColorStateNoCheck(void);

	inline void SetColorState(void) {
		if (m_currentColorFlags != m_requestedColorFlags) {
			SetColorStateNoCheck();
		}
	}
	void SetColorStateNoCheck(void);

	inline void SetDefaultProjectionState(void) {
		//See if non-default projection is active
		if (m_nearZRangeHackProjectionActive) {
			SetProjectionStateNoCheck(false);
		}
	}

	inline void FASTCALL SetProjectionState(bool requestNearZRangeHackProjection) {
		if (requestNearZRangeHackProjection != m_nearZRangeHackProjectionActive) {
			//Set requested projection state
			SetProjectionStateNoCheck(requestNearZRangeHackProjection);
		}
	}

	inline void SetDefaultAAState(void) {
		if (m_defAAEnable != m_curAAEnable) {
			SetAAStateNoCheck(m_defAAEnable);
		}
	}
	inline void SetDisabledAAState(void) {
		if (m_curAAEnable && m_usingAA) {
			SetAAStateNoCheck(false);
		}
	}
	void FASTCALL SetAAStateNoCheck(bool AAEnable);

	bool FASTCALL LoadVertexProgram(GLuint, const char *, const char *);
	bool FASTCALL LoadFragmentProgram(GLuint, const char *, const char *);

	void AllocateFragmentProgramNamesSafe(void);
	void FreeFragmentProgramNamesSafe(void);
	bool InitializeFragmentPrograms(void);
	void TryInitializeFragmentProgramMode(void);
	void ShutdownFragmentProgramMode(void);

	void FASTCALL SetProjectionStateNoCheck(bool);
	void SetOrthoProjection(void);

	inline void RenderPasses(void) {
		if (m_rpPassCount != 0) {
			RenderPassesExec();
		}
	}
	inline void FASTCALL RenderPasses_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
		RenderPassesExec_SingleOrDualTextureAndDetailTexture(DetailTextureInfo);
	}

	inline void FASTCALL AddRenderPass(FTextureInfo* Info, DWORD PolyFlags, FLOAT PanBias) {
		INT rpPassCount = m_rpPassCount;

		MultiPass.TMU[rpPassCount].Info      = Info;
		MultiPass.TMU[rpPassCount].PolyFlags = PolyFlags;
		MultiPass.TMU[rpPassCount].PanBias   = PanBias;

		//Single texture rendering forced here by setting m_rpTMUnits equal to 1
		rpPassCount++;
		m_rpPassCount = rpPassCount;
		if (rpPassCount >= m_rpTMUnits) {
			//m_rpPassCount will never be equal to 0 here
			RenderPassesExec();
		}
	}

	void RenderPassesExec(void);
	void FASTCALL RenderPassesExec_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo);

	void RenderPassesNoCheckSetup(void);
	void RenderPassesNoCheckSetup_FP(void);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &);

	INT FASTCALL BufferStaticComplexSurfaceGeometry(const FSurfaceFacet&);
	INT FASTCALL BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet&);
	DWORD FASTCALL BufferDetailTextureData(FLOAT);
#ifdef UTGLR_INCLUDE_SSE_CODE
	DWORD FASTCALL BufferDetailTextureData_SSE2(FLOAT);
#endif //UTGLR_INCLUDE_SSE_CODE

	void FASTCALL DrawDetailTexture(FTextureInfo &, INT, bool);
	void FASTCALL DrawDetailTexture_FP(FTextureInfo &);

	inline void EndGouraudPolygonBuffering(void) {
		if (BufferedVerts > 0) {
			EndGouraudPolygonBufferingNoCheck();
		}
	}
	inline void EndTileBuffering(void) {
		if (BufferedTileVerts > 0) {
			EndTileBufferingNoCheck();
		}
	}
	inline void EndBuffering(void) {
		if (BufferedVerts > 0) {
			EndGouraudPolygonBufferingNoCheck();
		}
		if (BufferedTileVerts > 0) {
			EndTileBufferingNoCheck();
		}
	}
	void EndGouraudPolygonBufferingNoCheck(void);
	void EndTileBufferingNoCheck(void);

	void FASTCALL BufferAdditionalClippedVerts(FTransTexture** Pts, INT NumPts);
};

#ifdef UTGLR_UNREAL_227_BUILD

#if __STATIC_LINK

/* No native execs. */

#define AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV \
	UOpenGLRenderDevice::StaticClass();
#endif

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
