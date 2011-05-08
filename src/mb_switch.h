#pragma once

// MetalBone is not exported, if defined.
//#define METALBONE_LIBRARY
//#define MB_DECL_EXPORT

#define STRIP_METALBONE_NAMESPACE

#define MB_DEBUGBREAK_INSTEADOF_ABORT

// Use MSVC to compile, currently only MSVC is supported
#ifndef MSVC
#  define MSVC
#endif
