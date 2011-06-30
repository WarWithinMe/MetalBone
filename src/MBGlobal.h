#pragma once

/* ++++++++++ Switches ++++++++++ */

// -Define METALBONE_LIBRARY and MB_DECL_EXPORT to build metalbone
// -as a dll.
// -Define METALBONE_LIBRARY only to link with that dll.
// -If you are building as a static lib, do not define METALBONE_LIBRARY
//#define METALBONE_LIBRARY
//#define MB_DECL_EXPORT

#define STRIP_METALBONE_NAMESPACE
#define MB_DEBUGBREAK_INSTEADOF_ABORT

#define SK_IGNORE_STDINT_DOT_H // See 3rd/skia/core/SkTypes.h

// Use MSVC to compile, currently only MSVC is supported
#ifndef MSVC
#  define MSVC
#endif

// === Something about Direct2D ===
// -1. When using Direct2D to draw a bitmap repeatly, you should put
//     this bitmap in a single image file. For example, In "Demo", I
//     want the bitmap in ThumbBG.jpg, vertically repeating. So I take
//     it out of Elements.png
//     
#define MB_USE_D2D

// === How to build MetalBone with Skia? ===
// -1. Clone the "skia_lib_build" hub.
// -2. Navigate to skia_lib_build/project to build skia as static libs.
// -3. You need to add these path to your project's include path: 
//     metalbone/src/3rd/skia/config
//     metalbone/src/3rd/skia/core
//     ...
//     metalbone/src/3rd/skia/....
//
// === Something about Skia ===
// -1. Seems like Skia doesn't deal with wchar_t. So we may draw the text
//     using GDI only.
#define MB_USE_SKIA

/* ---------- Switches ---------- */

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

#include <windows.h>

#ifdef METALBONE_LIBRARY
#  ifdef MB_DECL_EXPORT
#    define METALBONE_EXPORT __declspec(dllexport)
#  else
#    define METALBONE_EXPORT __declspec(dllimport)
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

#ifndef GET_X_LPARAM
#  define GET_X_LPARAM(lp)  ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#  define GET_Y_LPARAM(lp)  ((int)(short)HIWORD(lp))
#endif

#include "3rd/Signal.h"
#ifdef STRIP_METALBONE_NAMESPACE
  using namespace MetalBone;
  using namespace MetalBone::ThirdParty::Gallant;
#endif

#define MB_DISABLE_COPY(TypeName)          \
    TypeName(const TypeName&);             \
    const TypeName& operator=(const TypeName&)
