#pragma once

#pragma comment(lib, "gdiplus.lib")       // Gdi+
#pragma comment(lib, "windowscodecs.lib") // WIC
#pragma comment(lib, "d2d1.lib")          // Direct2D
#pragma comment(lib, "dwrite.lib")        // DirectWrite
#pragma comment(lib, "dwmapi.lib")        // DWM
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include "mb_switch.h"
#include <Windows.h>

#ifdef _DEBUG
#  if !defined(MB_DEBUG)
#    define MB_DEBUG
#  endif
#  if !defined(MB_DEBUG_D2D)
#    define MB_DEBUG_D2D
#  endif
#endif

#ifndef _UNICODE
#  define _UNICODE
#endif

#ifdef METALBONE_LIBRARY
#  ifdef MB_DECL_EXPORT
#    define METALBONE_EXPORT __declspec(dllexport)
#  else
#    define METALBONE_EXPORT __declspec(dllexport)
#  endif
#else
#  define METALBONE_EXPORT
#endif

#define M_UNUSED(var) (void)var
inline void mb_noop(){}

// M_ASSERT / M_ASSERT_X / mDebug()
#ifdef MB_DEBUG
METALBONE_EXPORT void mb_assert(const char* assertion, const char* file, unsigned int line);
METALBONE_EXPORT void mb_assert_xw(const wchar_t* what, const wchar_t* where, const char* file, unsigned int line);
METALBONE_EXPORT void ensureInMainThread();
METALBONE_EXPORT void mb_debug(const wchar_t* what);
METALBONE_EXPORT void mb_warning(bool,const wchar_t* what);
METALBONE_EXPORT void dumpMemory(LPCVOID address, SIZE_T size); // dumpMemory() from VLD utility
#  define M_ASSERT(cond) ((!(cond)) ? mb_assert(#cond,__FILE__,__LINE__) : mb_noop())
#  define mDebug(message) mb_debug(message)
#  define mWarning(cond,message) mb_warning(cond,message)
#  define M_ASSERT_X(cond, what, where) ((!(cond)) ? mb_assert_xw(L##what,L##where,__FILE__,__LINE__) : mb_noop())
#  define ENSURE_IN_MAIN_THREAD ensureInMainThread()
#else
#  define M_ASSERT(cond) mb_noop()
#  define M_ASSERT_X(cond, what, where) mb_noop()
#  define mDebug(cond) mb_noop()
#  define mWarning(cond,m) mb_noop()
#  define ENSURE_IN_MAIN_THREAD mb_noop()
#endif // MB_DEBUG

template<class Interface>
inline void SafeRelease(Interface*& comObject) {
	if(comObject != 0) {
		comObject->Release();
		comObject = 0;
	}
}

namespace MetalBone{}
#include "3rd/Signal.h"
#ifdef STRIP_METALBONE_NAMESPACE
  using namespace MetalBone;
  using namespace MetalBone::ThirdParty::Gallant;
#endif
