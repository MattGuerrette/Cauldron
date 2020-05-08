// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _WIN32

#include <SDKDDKVer.h>

#define NOMINMAX    
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers

// Windows Header Files:
#include <windows.h>
#include <windowsx.h>
#include <wrl.h>

// math API
#include <DirectXMath.h>
using namespace DirectX;

#include <tchar.h>


#else

#include <xcb/xcb.h>

#endif

// C RunTime Header Files
#include <malloc.h>
#include <cassert>
#include <string>
#include <map>
#include <iostream>
#include <fstream>
#include <vector>
#include <mutex>
#include <limits>
#include <algorithm>
#include <mutex>



// TODO: reference additional headers your program requires here
