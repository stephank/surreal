/*=============================================================================
	FNativeTypes.h: Unreal typedefs for native OS abstraction classes.

	Revision history:
		* Created by Stéphan Kochen
=============================================================================*/

#if defined(WIN32)
	#include "FFileManagerWindows.h"
	typedef FFileManagerWindows FFileManagerNative;
	#include "FMallocWindows.h"
	typedef FMallocWindows FMallocNative;
#elif defined(__LINUX__) || defined(__APPLE__)
	#include "FFileManagerMmap.h"
	typedef FFileManagerMmap FFileManagerNative;
	#include "FMallocAnsi.h"
	typedef FMallocAnsi FMallocNative;
#else
	#include "FFileManagerAnsi.h"
	typedef FFileManagerAnsi FFileManagerNative;
	#include "FMallocAnsi.h"
	typedef FMallocAnsi FMallocNative;
#endif
