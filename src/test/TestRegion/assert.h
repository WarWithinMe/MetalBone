// Assert.h
//
//////////////////////////////////////////////////////////////////////
#pragma once

inline void InlDebugBreak() { __asm { int 3 }; }

#ifndef _DEBUG
#	define NOTRACE
#endif // _DEBUG

#ifdef NOTRACE
# if !defined(TRACE0)
#	define TRACE0(exp) do {} while(false)
# endif
#endif // NOTRACE

#ifdef _DEBUG
#	ifndef ASSERT
#		define ASSERT(x) do { if (!(x)) InlDebugBreak(); } while (false)
#	endif // ASSERT
#	ifndef VERIFY
#		define VERIFY(x) ASSERT(x)
#	endif
#	ifndef TRACE0
#		define TRACE0(exp) OutputDebugString(exp _T("\n"))
#	endif // TRACE0
#else // _DEBUG
#	ifndef ASSERT
#		define ASSERT(x)
#	endif // ASSERT
#	ifndef VERIFY
#		define VERIFY(x) (x)
#	endif
#endif // _DEBUG
