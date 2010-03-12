/*=============================================================================
	OpenGL.h: Unreal OpenGL support header.
	Portions copyright 1999 Epic Games, Inc. All Rights Reserved.

	Revision history:

=============================================================================*/

#include <map>
#pragma warning(disable : 4663)
#pragma warning(disable : 4018)
#include <vector>

#pragma warning(disable : 4244)
#pragma warning(disable : 4245)

#pragma warning(disable : 4146)
#include <sstream>
#include <iostream>
#include <iomanip>

//Optional ASM code
#define UTGLR_USE_ASM_CODE

//Optional SSE code
#ifndef __GNUC__
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
	#ifdef WIN32
	#define FASTCALL	__fastcall
	#else
	#define FASTCALL
	#endif
#else
#define FASTCALL
#endif


//Debug defines
#ifdef __GNUC__
#define UTGLR_DONT_DEBUG_AT_ALL
#endif
//#ifndef UTGLR_DONT_DEBUG_AT_ALL
//#define UTGLR_DEBUG_SHOW_TEX_CONVERT_COUNTS
//#define UTGLR_DEBUG_SHOW_CALL_COUNTS
//#define UTGLR_DEBUG_WORLD_WIREFRAME
//#define UTGLR_DEBUG_ACTOR_WIREFRAME
//#define UTGLR_DEBUG_Z_RANGE_HACK_WIREFRAME
//#endif

//Controls the loading of a subset of OpenGL procs
//#define UTGLR_ALL_GL_PROCS


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

//If exceeds 7, various things will break
#define MAX_TMUNITS			4		// vogel: maximum number of texture mapping units supported

//Must be at least 2000
#define VERTEX_ARRAY_SIZE	4000	// vogel: better safe than sorry

#ifdef WIN32
#define STDGL 1					// Use standard GL driver or minidriver by default
#define DYNAMIC_BIND 1			// If 0, must static link to OpenGL32, Gdi32
#define GL_DLL (STDGL ? "OpenGL32.dll" : "3dfxgl.dll")
#endif


/*-----------------------------------------------------------------------------
	OpenGLDrv.
-----------------------------------------------------------------------------*/

class UOpenGLRenderDevice;


template <class ClassT> class rbtree_allocator {
public:
	typedef typename ClassT::node_t node_t;

public:
	rbtree_allocator() {
	}
	~rbtree_allocator() {
	}

	node_t *alloc_node(void) {
		return new node_t;
	}

	void free_node(node_t *pNode) {
		delete pNode;
	}
};

template <class KeyT, class DataT> class rbtree {
private:
	enum { RED_NODE, BLACK_NODE };
	enum { Left = 0, Right = 1 };

public:
	struct node_t {
		struct node_t *pC[2], *pParent;
		unsigned char color;
		KeyT key;
		DataT data;
	};

private:
	rbtree(const rbtree &);
	rbtree &operator=(const rbtree &);

public:
	rbtree() {
		m_pRoot = &m_NIL;
		m_NIL.color = BLACK_NODE;
		m_NIL.pParent = 0;
		m_NIL.pC[Left] = 0;
		m_NIL.pC[Right] = 0;
	}
	~rbtree() {
	}

	node_t *find(KeyT key) {
		node_t *px;

		px = m_pRoot;
		while (px != &m_NIL) {
			if (key == px->key) {
				return px;
			}
			px = px->pC[(key < px->key) ? Left : Right];
		}

		return 0;
	}

	bool insert(node_t *pNode) {
		//Attempt to insert the new node
		if (m_pRoot == &m_NIL) {
			m_pRoot = pNode;
			pNode->pParent = &m_NIL;
			pNode->pC[Left] = &m_NIL;
			pNode->pC[Right] = &m_NIL;
			pNode->color = BLACK_NODE;

			return true;
		}

		node_t *px, *py;
		unsigned int compLR = Left;

		px = m_pRoot;
		py = 0;

		while (px != &m_NIL) {
			py = px;

			if (pNode->key == px->key) {
				return false;
			}

			compLR = (pNode->key < px->key) ? Left : Right;
			px = px->pC[compLR];
		}

		py->pC[compLR] = pNode;
		pNode->pParent = py;
		pNode->pC[Left] = &m_NIL;
		pNode->pC[Right] = &m_NIL;
		pNode->color = RED_NODE;

		//Rebalance the tree
		px = pNode;
		node_t *pxParent;
		while (((pxParent = px->pParent) != &m_NIL) && (pxParent->color == RED_NODE)) {
			node_t *pxParentParent;

			pxParentParent = pxParent->pParent;
			pxParentParent->color = RED_NODE;
			if (pxParent == pxParentParent->pC[Left]) {
				py = pxParentParent->pC[Right];
				if (py->color == RED_NODE) {
					py->color = BLACK_NODE;
					pxParent->color = BLACK_NODE;
					px = pxParentParent;
				}
				else {
					if (px == pxParent->pC[Right]) {
						px = pxParent;
						left_rotate(px);
						pxParent = px->pParent;
					}
					pxParent->color = BLACK_NODE;
					right_rotate(pxParentParent);
				}
			}
			else {
				py = pxParentParent->pC[Left];
				if (py->color == RED_NODE) {
					py->color = BLACK_NODE;
					pxParent->color = BLACK_NODE;
					px = pxParentParent;
				}
				else {
					if (px == pxParent->pC[Left]) {
						px = pxParent;
						right_rotate(px);
						pxParent = px->pParent;
					}
					pxParent->color = BLACK_NODE;
					left_rotate(pxParentParent);
				}
			}
		}

		m_pRoot->color = BLACK_NODE;

		return true;
	}

	void remove(node_t *pNode) {
		node_t *px, *py;

		py = pNode;
		if (py->pC[Left] == &m_NIL) {
			px = py->pC[Right];
		}
		else if (py->pC[Right] == &m_NIL) {
			px = py->pC[Left];
		}
		else {
			py = py->pC[Right];
			while (py->pC[Left] != &m_NIL) {
				py = py->pC[Left];
			}
			px = py->pC[Right];
		}

		if (py != pNode) {
			pNode->pC[Left]->pParent = py;
			py->pC[Left] = pNode->pC[Left];
			if (py != pNode->pC[Right]) {
				px->pParent = py->pParent;
				py->pParent->pC[Left] = px;
				py->pC[Right] = pNode->pC[Right];
				pNode->pC[Right]->pParent = py;
			}
			else {
				px->pParent = py;
			}

			if (m_pRoot == pNode) {
				m_pRoot = py;
			}
			else {
				pNode->pParent->pC[(pNode->pParent->pC[Left] == pNode) ? Left : Right] = py;
			}
			py->pParent = pNode->pParent;

			unsigned char tempColor = pNode->color;
			pNode->color = py->color;
			py->color = tempColor;

			py = pNode;
		}
		else {
			px->pParent = py->pParent;

			if (m_pRoot == pNode) {
				m_pRoot = px;
			}
			else {
				py->pParent->pC[(py->pParent->pC[Left] == py) ? Left : Right] = px;
			}
		}

		if (py->color == BLACK_NODE) {
			node_t *pxp;

			while (((pxp = px->pParent) != &m_NIL) && (px->color == BLACK_NODE)) {
				if (px == pxp->pC[Left]) {
					node_t *pw;

					pw = pxp->pC[Right];
					if (pw->color == RED_NODE) {
						pw->color = BLACK_NODE;
						pxp->color = RED_NODE;
						left_rotate(pxp);
						pw = pxp->pC[Right];
					}
					//if ((pw->pC[Left]->color == BLACK_NODE) & (pw->pC[Right]->color == BLACK_NODE))
					if (((pw->pC[Left]->color ^ BLACK_NODE) | (pw->pC[Right]->color ^ BLACK_NODE)) == 0) {
						pw->color = RED_NODE;
						px = pxp;
					}
					else {
						if (pw->pC[Right]->color == BLACK_NODE) {
							pw->pC[Left]->color = BLACK_NODE;
							pw->color = RED_NODE;
							right_rotate(pw);
							pw = pxp->pC[Right];
						}
						pw->color = pxp->color;
						pxp->color = BLACK_NODE;
						pw->pC[Right]->color = BLACK_NODE;
						left_rotate(pxp);
						break;
					}
				}
				else {
					node_t *pw;

					pw = pxp->pC[Left];
					if (pw->color == RED_NODE) {
						pw->color = BLACK_NODE;
						pxp->color = RED_NODE;
						right_rotate(pxp);
						pw = pxp->pC[Left];
					}
					//if ((pw->pC[Right]->color == BLACK_NODE) & (pw->pC[Left]->color == BLACK_NODE))
					if (((pw->pC[Right]->color ^ BLACK_NODE) | (pw->pC[Left]->color ^ BLACK_NODE)) == 0) {
						pw->color = RED_NODE;
						px = pxp;
					}
					else {
						if (pw->pC[Left]->color == BLACK_NODE) {
							pw->pC[Right]->color = BLACK_NODE;
							pw->color = RED_NODE;
							left_rotate(pw);
							pw = pxp->pC[Left];
						}
						pw->color = pxp->color;
						pxp->color = BLACK_NODE;
						pw->pC[Left]->color = BLACK_NODE;
						right_rotate(pxp);
						break;
					}
				}
			}
			px->color = BLACK_NODE;
		}

		return;
	}

	node_t * FASTCALL next_node(node_t *pNode) {
		node_t *px;

		if (pNode->pC[Right] != &m_NIL) {
			pNode = pNode->pC[Right];
			while (pNode->pC[Left] != &m_NIL) {
				pNode = pNode->pC[Left];
			}
			return pNode;
		}

		if (pNode->pParent == &m_NIL) {
			return &m_NIL;
		}

		px = pNode->pParent;
		while (pNode == px->pC[Right]) {
			pNode = px;
			if (pNode->pParent == &m_NIL) {
				return &m_NIL;
			}
			px = pNode->pParent;
		}

		return px;
	}

	node_t *begin(void) {
		node_t *px;

		if (m_pRoot == &m_NIL) {
			return &m_NIL;
		}

		for (px = m_pRoot; px->pC[Left] != &m_NIL; px = px->pC[Left]);

		return px;
	}

	node_t *end(void) {
		return &m_NIL;
	}

	void FASTCALL clear(rbtree_allocator<rbtree<KeyT, DataT> > *pAllocator) {
		node_t *px;

		px = begin();
		while (px != end()) {
			node_t *py = next_node(px);
			remove(px);
			pAllocator->free_node(px);
			px = py;
		}

		return;
	}

	unsigned int calc_size(void) {
		node_t *px;
		unsigned int size;

		px = begin();
		size = 0;
		while (px != end()) {
			px = next_node(px);
			size++;
		}

		return size;
	}

private:
	void FASTCALL left_rotate(node_t *px) {
		node_t *py;

		py = px->pC[Right];

		node_t *pyLeft = py->pC[Left];
		px->pC[Right] = pyLeft;
		if (pyLeft != &m_NIL) {
			pyLeft->pParent = px;
		}

		node_t *pxParent = px->pParent;
		py->pParent = pxParent;

		if (pxParent == &m_NIL) {
			m_pRoot = py;
		}
		else {
			pxParent->pC[(px == pxParent->pC[Left]) ? Left : Right] = py;
		}

		py->pC[Left] = px;
		px->pParent = py;

		return;
	}

	void FASTCALL right_rotate(node_t *px) {
		node_t *py;

		py = px->pC[Left];

		node_t *pyRight = py->pC[Right];
		px->pC[Left] = pyRight;
		if (pyRight != &m_NIL) {
			pyRight->pParent = px;
		}

		node_t *pxParent = px->pParent;
		py->pParent = pxParent;

		if (pxParent == &m_NIL) {
			m_pRoot = py;
		}
		else {
			pxParent->pC[(px == pxParent->pC[Right]) ? Right : Left] = py;
		}

		py->pC[Right] = px;
		px->pParent = py;

		return;
	}

private:
	node_t m_NIL;
	node_t *m_pRoot;
};


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

#define CT_GENERATE_MIPMAPS_BIT					0x20

#define CT_HAS_MIPMAPS_BIT						0x40

#define CT_DEFAULT_TEXTURE_MAX_LEVEL			1000

struct tex_params_t {
	BYTE first;
	BYTE second;
	_WORD maxLevel;
};

//Default texture parameters for new OpenGL texture
const tex_params_t CT_DEFAULT_TEX_PARAMS = { CT_MIN_FILTER_NEAREST_MIPMAP_LINEAR | CT_MAG_FILTER_NEAREST_OR_LINEAR_BIT, 0, CT_DEFAULT_TEXTURE_MAX_LEVEL };

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

	inline void unlink(FCachedTexture *pCT) {
		pCT->pPrev->pNext = pCT->pNext;
		pCT->pNext->pPrev = pCT->pPrev;
	}
	inline void link_to_tail(FCachedTexture *pCT) {
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

	inline void add(node_t *pNode) {
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

// Geometry
struct FGLVertex {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

// Normals
struct FGLNormal {
	FLOAT x;
	FLOAT y;
	FLOAT z;
};

// Texcoords
struct FGLTexCoord {
	FLOAT u;
	FLOAT v;
};

// Primary and secondary (specular) color
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
#if defined UTGLR_DX_BUILD || defined UTGLR_UNREAL_BUILD
	DECLARE_CLASS(UOpenGLRenderDevice, URenderDevice, CLASS_Config)
#else
	DECLARE_CLASS(UOpenGLRenderDevice, URenderDevice, CLASS_Config, OpenGLDrv)
#endif

#ifndef UTGLR_DONT_DEBUG_AT_ALL
	class ods_buf : public std::basic_stringbuf<TCHAR, std::char_traits<TCHAR> > {
	public:
		virtual ~ods_buf() {
			sync();
		}

	protected:
		int sync() {
#ifdef WIN32
			//Output the string
			TCHAR_CALL_OS(OutputDebugStringW(str().c_str()), OutputDebugStringA(appToAnsi(str().c_str())));
#else
			//Add non-win32 debug output code here
#endif

			//Clear the buffer
			str(std::basic_string<TCHAR>());
			
			return 0;
		}
	};
	class ods_stream : public std::basic_ostream<TCHAR, std::char_traits<TCHAR> > {
	public:
		ods_stream() : std::basic_ostream<TCHAR, std::char_traits<TCHAR> >(new ods_buf()) {
		}
		~ods_stream() {
			delete rdbuf();
		}
	};

	std::basic_string<TCHAR> HexString(DWORD data, DWORD numBits = 4) {
		std::basic_ostringstream<TCHAR> strHexNum;

		strHexNum << std::hex;
		strHexNum.fill('0');
		strHexNum << std::uppercase;
		strHexNum << std::setw(((numBits + 3) & -4) / 4);
		strHexNum << data;

		return strHexNum.str();
	}

	//Debug stream
	ods_stream dout;

	//Debug bits
	DWORD m_debugBits;
	inline bool DebugBit(DWORD debugBit) {
		return ((m_debugBits & debugBit) != 0);
	}
	enum {
		DEBUG_BIT_BASIC		= 0x00000001,
		DEBUG_BIT_GL_ERROR	= 0x00000002,
		DEBUG_BIT_ANY		= 0xFFFFFFFF
	};
#endif

	//Fixed texture cache ids
	#define TEX_CACHE_ID_UNUSED		0xFFFFFFFFFFFFFFFFLL
	#define TEX_CACHE_ID_NO_TEX		0xFFFFFFFF00000010LL
	#define TEX_CACHE_ID_ALPHA_TEX	0xFFFFFFFF00000020LL

	//Texture cache id flags
	enum {
		TEX_CACHE_ID_FLAG_MASKED	= 0x1,
		TEX_CACHE_ID_FLAG_16BIT		= 0x2
	};

	//Mask for poly flags that impact texture object state
	#define TEX_DYNAMIC_POLY_FLAGS_MASK		(PF_NoSmooth)

	// Information about a cached texture.
	enum tex_type_t {
		TEX_TYPE_COMPRESSED_DXT1,
		TEX_TYPE_COMPRESSED_DXT1_TO_DXT3,
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


	inline void *AlignMemPtr(void *ptr, DWORD align) {
		return (void *)(((DWORD)ptr + (align - 1)) & -align);
	}
	enum { VERTEX_ARRAY_ALIGN = 64 };	//Must be even multiple of 16B for SSE
	enum { VERTEX_ARRAY_TAIL_PADDING = 72 };	//Must include 8B for half SSE tail

	// Geometry
	FGLVertex *VertexArray;
	BYTE m_VertexArrayMem[(sizeof(FGLVertex) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	// Normals
	FGLNormal *NormalArray;
	BYTE m_NormalArrayMem[(sizeof(FGLNormal) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	// Texcoords
	FGLTexCoord *TexCoordArray[MAX_TMUNITS];
	BYTE m_TexCoordArrayMem[MAX_TMUNITS][(sizeof(FGLTexCoord) * VERTEX_ARRAY_SIZE) + VERTEX_ARRAY_ALIGN + VERTEX_ARRAY_TAIL_PADDING];

	// Primary and secondary (specular) color
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

	//Vertex program cache information
	bool m_vpModeEnabled;
	GLuint m_vpCurrent;
	//Vertex program ids
	bool m_allocatedVertexProgramNames;
	GLuint m_vpDefaultRenderingState;
	GLuint m_vpDefaultRenderingStateWithFog;
#ifdef UTGLR_RUNE_BUILD
	GLuint m_vpDefaultRenderingStateWithLinearFog;
#endif
	GLuint m_vpComplexSurface[MAX_TMUNITS];
	GLuint m_vpComplexSurfaceDetailAlpha;
	GLuint m_vpComplexSurfaceSingleTextureAndDetailTexture;
	GLuint m_vpComplexSurfaceDualTextureAndDetailTexture;
	GLuint m_vpComplexSurfaceSingleTextureWithPos;
	GLuint m_vpComplexSurfaceDualTextureWithPos;
	GLuint m_vpComplexSurfaceTripleTextureWithPos;

	//Fragment program cache information
	bool m_fpModeEnabled;
	GLuint m_fpCurrent;
	//Fragment program ids
	bool m_allocatedFragmentProgramNames;
	GLuint m_fpDefaultRenderingState;
	GLuint m_fpDefaultRenderingStateWithFog;
#ifdef UTGLR_RUNE_BUILD
	GLuint m_fpDefaultRenderingStateWithLinearFog;
#endif
	GLuint m_fpComplexSurfaceSingleTexture;
	GLuint m_fpComplexSurfaceDualTextureModulated;
	GLuint m_fpComplexSurfaceDualTextureModulated2X;
	GLuint m_fpComplexSurfaceSingleTextureWithFog;
	GLuint m_fpComplexSurfaceDualTextureModulatedWithFog;
	GLuint m_fpComplexSurfaceDualTextureModulated2XWithFog;
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

	bool m_prevSwapBuffersStatus;

	typedef rbtree<DWORD, FCachedTexture> DWORD_CTTree_t;
	typedef rbtree_allocator<DWORD_CTTree_t> DWORD_CTTree_Allocator_t;
	typedef rbtree<QWORD, FCachedTexture> QWORD_CTTree_t;
	typedef rbtree_allocator<QWORD_CTTree_t> QWORD_CTTree_Allocator_t;
	typedef rbtree_node_pool<QWORD_CTTree_t> QWORD_CTTree_NodePool_t;
	typedef DWORD TexPoolMapKey_t;
	typedef rbtree<TexPoolMapKey_t, QWORD_CTTree_NodePool_t> TexPoolMap_t;
	typedef rbtree_allocator<TexPoolMap_t> TexPoolMap_Allocator_t;

	enum { NUM_CTTree_TREES = 16 }; //Must be a power of 2
	inline DWORD CTZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 12) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD CTNonZeroPrefixCacheIDSuffixToTreeIndex(DWORD CacheIDSuffix) {
		return ((CacheIDSuffix >> 20) & (NUM_CTTree_TREES - 1));
	}
	inline DWORD MakeTexPoolMapKey(DWORD UBits, DWORD VBits) {
		return ((UBits << 16) | VBits);
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
		UBOOL UseVertexProgram;
		UBOOL UseFragmentProgram;
	} DCV;

#ifdef UTGLR_UNREAL_BUILD
	UBOOL DetailTextures;
#endif
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
	UBOOL AutoGenerateMipmaps;
	UBOOL UseVertexSpecular;
	UBOOL UseAlphaPalette;
	UBOOL UseS3TC;
	UBOOL Use16BitTextures;
	UBOOL UseTNT; 					// vogel: REMOVE ME - HACK!
	UBOOL UseCVA;
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
	UBOOL UseVertexProgram;
	UBOOL UseFragmentProgram;
	INT SwapInterval;
#if defined UTGLR_DX_BUILD || defined UTGLR_UNREAL_BUILD || defined UTGLR_RUNE_BUILD
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
	UBOOL PL_AutoGenerateMipmaps;
	INT PL_MaxAnisotropy;
	UBOOL PL_UseTNT;
	UBOOL PL_SmoothMaskedTextures;
	UBOOL PL_MaskedTextureHack;
	FLOAT PL_LODBias;
	UBOOL PL_UsePalette;
	UBOOL PL_UseAlphaPalette;
	UBOOL PL_UseDetailAlpha;
	UBOOL PL_SinglePassDetail;
	UBOOL PL_UseVertexProgram;
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
	BYTE* HitData;
	INT* HitSize;

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

	enum {
		PF2_NEAR_Z_RANGE_HACK	= 0x01
	};
	DWORD m_curBlendFlags;
	DWORD m_smoothMaskedTexturesBit;
	DWORD m_curPolyFlags;
	DWORD m_curPolyFlags2;
	INT m_bufferActorTrisCutoff;

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
#ifdef UTGLR_RUNE_BUILD
	BYTE m_gpAlpha;
	bool m_gpFogEnabled;
#endif

	DWORD m_curTexEnvFlags[MAX_TMUNITS];
	FTexInfo TexInfo[MAX_TMUNITS];

	void (FASTCALL *m_pBuffer3BasicVertsProc)(UOpenGLRenderDevice *, FTransTexture **);
	void (FASTCALL *m_pBuffer3ColoredVertsProc)(UOpenGLRenderDevice *, FTransTexture **);
	void (FASTCALL *m_pBuffer3FoggedVertsProc)(UOpenGLRenderDevice *, FTransTexture **);

	void (FASTCALL *m_pBuffer3VertsProc)(UOpenGLRenderDevice *, FTransTexture **);

	TArray<INT> GLHitData;

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

	static bool g_gammaFirstTime;
	static bool g_haveOriginalGammaRamp;

	// GL functions.
	#define GL_EXT(name) static bool SUPPORTS##name;
	#define GL_PROC(ext,ret,func,parms) static ret (STDCALL *func)parms;
#ifdef UTGLR_ALL_GL_PROCS
	#define GL_PROX(ext,ret,func,parms) static ret (STDCALL *func)parms;
#else
	#define GL_PROX(ext,ret,func,parms)
#endif
	#include "OpenGLFuncs.h"
	#undef GL_EXT
	#undef GL_PROC
	#undef GL_PROX


#ifdef RGBA_MAKE
#undef RGBA_MAKE
#endif
	static inline DWORD RGBA_MAKE(BYTE r, BYTE g, BYTE b, BYTE a) {	// vogel: I hate macros...
		return (a << 24) | (b << 16) | (g << 8) | r;
	}																// vogel: ... and macros hate me


#if defined WIN32 && defined UTGLR_USE_ASM_CODE
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
	static inline DWORD FPlaneTo_RGB_A255(const FPlane *pPlane) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					255);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
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
	static inline DWORD FPlaneTo_RGBClamped_A255(const FPlane *pPlane) {
		return RGBA_MAKE(
					Clamp(appRound(pPlane->X * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
					255);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
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
	static inline DWORD FPlaneTo_RGB_A0(const FPlane *pPlane) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					0);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
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
	static inline DWORD FPlaneTo_RGB_Aub(const FPlane *pPlane, BYTE alpha) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					alpha);
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
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
	static inline DWORD FPlaneTo_RGBA(const FPlane *pPlane) {
		return RGBA_MAKE(
					appRound(pPlane->X * 255.0f),
					appRound(pPlane->Y * 255.0f),
					appRound(pPlane->Z * 255.0f),
					appRound(pPlane->W * 255.0f));
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
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
	static inline DWORD FPlaneTo_RGBAClamped(const FPlane *pPlane) {
		return RGBA_MAKE(
					Clamp(appRound(pPlane->X * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Y * 255.0f), 0, 255),
					Clamp(appRound(pPlane->Z * 255.0f), 0, 255),
					Clamp(appRound(pPlane->W * 255.0f), 0, 255));
	}
#endif

#if defined WIN32 && defined UTGLR_USE_ASM_CODE
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
	static inline DWORD FPlaneTo_RGBScaled_A255(const FPlane *pPlane, FLOAT rgbScale) {
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

	void FASTCALL DbgPrintInitParam(const TCHAR *pName, INT value);
	void FASTCALL DbgPrintInitParam(const TCHAR *pName, FLOAT value);

#ifdef UTGLR_INCLUDE_SSE_CODE
	static bool CPU_DetectCPUID(void);
	static bool CPU_DetectSSE(void);
	static bool CPU_DetectSSE2(void);
#endif //UTGLR_INCLUDE_SSE_CODE

	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FGammaRamp &ramp);
	void BuildGammaRamp(float redGamma, float greenGamma, float blueGamma, int brightness, FByteGammaRamp &ramp);
	void SetGamma(FLOAT GammaCorrection);

	static bool FASTCALL IsGLExtensionSupported(const char *pExtensionsString, const char *pExtensionName);
	bool FASTCALL FindExt(const char *pName);
	void FASTCALL FindProc(void*& ProcAddress, const char *pName, const char *pSupportName, bool& Supports, bool AllowExt);
	void FASTCALL FindProcs(bool AllowExt);

	UBOOL FailedInitf(const TCHAR* Fmt, ...);
	void Exit();
	void ShutdownAfterError();

	UBOOL SetRes(INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);
	void UnsetRes();

	void CheckGLErrorFlag(const TCHAR *pTag);

	void ConfigValidate_RefreshDCV(void);
	void ConfigValidate_RequiredExtensions(void);
	void ConfigValidate_Main(void);

	UBOOL Init(UViewport* InViewport, INT NewX, INT NewY, INT NewColorBytes, UBOOL Fullscreen);

	static QSORT_RETURN CDECL CompareRes(const FPlane* A, const FPlane* B) {
		return (QSORT_RETURN) (((A->X - B->X) != 0.0) ? (A->X - B->X) : (A->Y - B->Y));
	}

	UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);
	void Lock(FPlane InFlashScale, FPlane InFlashFog, FPlane ScreenClear, DWORD RenderLockFlags, BYTE* InHitData, INT* InHitSize);
	void SetSceneNode(FSceneNode* Frame);
	void Unlock(UBOOL Blit);
#ifdef UTGLR_UNREAL_BUILD
	void Flush();
	inline void Flush(UBOOL AllowPrecache) { Flush(); };
#else
	void Flush(UBOOL AllowPrecache);
#endif

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
	void Draw2DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
	void Draw3DLine(FSceneNode* Frame, FPlane Color, DWORD LineFlags, FVector P1, FVector P2);
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

	void ScanForOldTextures(void);

	inline void SetNoTexture(INT Multi) {
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

	inline void SetTexture(INT Multi, FTextureInfo& Info, DWORD PolyFlags, FLOAT PanBias) {
		FTexInfo& Tex = TexInfo[Multi];
		QWORD CacheID;
		DWORD DynamicPolyFlags;

		// Set panning.
		Tex.UPan = Info.Pan.X + PanBias * Info.UScale;
		Tex.VPan = Info.Pan.Y + PanBias * Info.VScale;

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

	inline void SetTextureNoPanBias(INT Multi, FTextureInfo& Info, DWORD PolyFlags) {
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

	void FASTCALL SetTextureNoCheck(FTexInfo& Tex, FTextureInfo& Info, DWORD PolyFlags);
	void FASTCALL CacheTextureInfo(FCachedTexture *pBind, const FTextureInfo &Info, DWORD PolyFlags);

	void FASTCALL ConvertDXT1_DXT3(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_P8(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_P8_NoStep(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertP8_RGBA8888(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertP8_RGBA8888_NoStep(const FMipmapBase *Mip, const FColor *Palette, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_BGRA8888_NoClamp(const FMipmapBase *Mip, INT Level);
	void FASTCALL ConvertBGRA7777_RGBA8888(const FMipmapBase *Mip, INT Level);

	inline void SetBlend(DWORD PolyFlags) {
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
#else
		DWORD blendFlags = PolyFlags & (PF_Translucent | PF_Modulated | PF_Invisible | PF_Occlude | PF_Masked | PF_Highlighted);
#endif
		if (m_curBlendFlags != blendFlags) {
			SetBlendNoCheck(blendFlags);
		}
	}
	void FASTCALL SetBlendNoCheck(DWORD blendFlags);

	inline void SetTexEnv(DWORD texUnit, DWORD PolyFlags) {
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

	inline void DisableSubsequentTextures(DWORD firstTexUnit) {
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

	inline void DisableSubsequentClientTextures(DWORD firstTexUnit) {
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

	inline void SetDefaultVertexProgramState(void) {
		//Keep vertex programs enabled if using vertex program mode
		if (UseVertexProgram) {
			SetVertexProgram(m_vpDefaultRenderingState);
			return;
		}

		//See if vertex program mode is enabled
		if (m_vpModeEnabled == true) {
			//Mark vertex program mode as disabled
			m_vpModeEnabled = false;

			//Disable vertex program mode
			glDisable(GL_VERTEX_PROGRAM_ARB);
		}

		return;
	}

	inline void SetVertexProgram(GLuint program) {
		//See if vertex program mode is disabled
		if (m_vpModeEnabled == false) {
			//Mark vertex program mode as enabled
			m_vpModeEnabled = true;

			//Enable vertex program mode
			glEnable(GL_VERTEX_PROGRAM_ARB);

			m_vpEnableCount++;
		}

		//See if the requested program is not already bound
		if (program != m_vpCurrent) {
			//Save the new current vertex program
			m_vpCurrent = program;

			//Bind the vertex program
			glBindProgramARB(GL_VERTEX_PROGRAM_ARB, program);

			m_vpSwitchCount++;
		}

		return;
	}

	inline void SetDisabledFragmentProgramState(void) {
		//See if fragment program mode is enabled
		if (m_fpModeEnabled == true) {
			//Mark fragment program mode as disabled
			m_fpModeEnabled = false;

			//Disable fragment program mode
			glDisable(GL_FRAGMENT_PROGRAM_ARB);
		}

		return;
	}

	inline void SetDefaultFragmentProgramState(void) {
		//Keep fragment programs enabled if using fragment program mode
		if (UseFragmentProgram) {
			SetFragmentProgram(m_fpDefaultRenderingState);
			return;
		}

		//See if fragment program mode is enabled
		if (m_fpModeEnabled == true) {
			//Mark fragment program mode as disabled
			m_fpModeEnabled = false;

			//Disable fragment program mode
			glDisable(GL_FRAGMENT_PROGRAM_ARB);
		}

		return;
	}

	inline void SetFragmentProgram(GLuint program) {
		//See if fragment program mode is disabled
		if (m_fpModeEnabled == false) {
			//Mark fragment program mode as enabled
			m_fpModeEnabled = true;

			//Enable fragment program mode
			glEnable(GL_FRAGMENT_PROGRAM_ARB);

			m_fpEnableCount++;
		}

		//See if the requested program is not already bound
		if (program != m_fpCurrent) {
			//Save the new current fragment program
			m_fpCurrent = program;

			//Bind the fragment program
			glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, program);

			m_fpSwitchCount++;
		}

		return;
	}

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

	inline void SetProjectionState(bool requestNearZRangeHackProjection) {
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

	void AllocateVertexProgramNamesSafe(void);
	void FreeVertexProgramNamesSafe(void);
	bool InitializeVertexPrograms(void);
	bool FASTCALL LoadVertexProgram(GLuint, const char *, const TCHAR *);
	void TryInitializeVertexProgramMode(void);
	void ShutdownVertexProgramMode(void);

	void AllocateFragmentProgramNamesSafe(void);
	void FreeFragmentProgramNamesSafe(void);
	bool InitializeFragmentPrograms(void);
	bool FASTCALL LoadFragmentProgram(GLuint, const char *, const TCHAR *);
	void TryInitializeFragmentProgramMode(void);
	void ShutdownFragmentProgramMode(void);

	void FASTCALL SetProjectionStateNoCheck(bool);
	void SetOrthoProjection(void);

	inline void RenderPasses(void) {
		if (m_rpPassCount != 0) {
			RenderPassesExec();
		}
	}
	inline void RenderPasses_SingleOrDualTextureAndDetailTexture(FTextureInfo &DetailTextureInfo) {
		RenderPassesExec_SingleOrDualTextureAndDetailTexture(DetailTextureInfo);
	}

	inline void AddRenderPass(FTextureInfo* Info, DWORD PolyFlags, FLOAT PanBias) {
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
	void RenderPassesNoCheckSetup_VP(void);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture(FTextureInfo &);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_VP(FTextureInfo &);
	void FASTCALL RenderPassesNoCheckSetup_SingleOrDualTextureAndDetailTexture_FP(FTextureInfo &);

	INT FASTCALL BufferStaticComplexSurfaceGeometry(const FSurfaceFacet&);
	INT FASTCALL BufferStaticComplexSurfaceGeometry_VP(const FSurfaceFacet&);
	DWORD FASTCALL BufferDetailTextureData(FLOAT);
#ifdef UTGLR_INCLUDE_SSE_CODE
	DWORD FASTCALL BufferDetailTextureData_SSE2(FLOAT);
#endif //UTGLR_INCLUDE_SSE_CODE

	void FASTCALL DrawDetailTexture(FTextureInfo &, INT, bool);
	void FASTCALL DrawDetailTexture_VP(FTextureInfo &);
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

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
