#ifndef MBGLOBAL_H
#define MBGLOBAL_H
#include "mb_switch.h"
#include <Windows.h>

#ifdef _DEBUG
#ifndef MB_DEBUG
#define MB_DEBUG
#endif
#ifndef MB_DEBUG_D2D
#define MB_DEBUG_D2D
#endif
#endif

#ifndef _UNICODE
#  define _UNICODE
#endif

#ifndef MB_DECL_EXPORT
#define MB_DECL_EXPORT __declspec(dllexport)
#endif // MB_DECL_EXPORT
#ifndef MB_DECL_IMPORT
#define MB_DECL_IMPORT __declspec(dllimport)
#endif // MB_DECL_IMPORT

#ifndef METALBONE_EXPORT
#  if defined(METALBONE_LIBRARY)
#    define METALBONE_EXPORT MB_DECL_EXPORT
#  else
#    define METALBONE_EXPORT MB_DECL_IMPORT
#  endif
#endif // METALBONE_EXPORT

#pragma comment(lib, "gdiplus.lib")       // Gdi+
#pragma comment(lib, "windowscodecs.lib") // WIC
#pragma comment(lib, "d2d1.lib")          // Direct2D
#pragma comment(lib, "dwrite.lib")        // DirectWrite
#pragma comment(lib, "dwmapi.lib")        // DWM

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace MetalBone{}
#ifdef STRIP_METALBONE_NAMESPACE
using namespace MetalBone;
#endif // STRIP_METALBONE_NAMESPACE


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
inline void SafeRelease(Interface*& ppInterfaceToRelease) {
	if (ppInterfaceToRelease != 0) {
		ppInterfaceToRelease->Release();
		ppInterfaceToRelease = 0;
	}
}

#include "3rd/Signal.h"
#ifdef STRIP_METALBONE_NAMESPACE
using namespace MetalBone::ThirdParty::Gallant;
#endif

#endif // MBGLOBAL_H
