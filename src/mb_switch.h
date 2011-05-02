#ifndef MB_SWITCH_H
#define MB_SWITCH_H

// MetalBone is not exported, if defined.
#define METALBONE_EXPORT

#define STRIP_METALBONE_NAMESPACE

#define MB_DEBUGBREAK_INSTEADOF_ABORT

#define MB_DEBUG
#define MB_DEBUG_D2D

// Use MSVC to compile, currently only MSVC is supported
#ifndef MSVC
#  define MSVC
#endif

#endif // MB_SWITCH_H
