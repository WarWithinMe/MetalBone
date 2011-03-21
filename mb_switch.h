#ifndef MB_SWITCH_H
#define MB_SWITCH_H

// 定义METALBONE_EXPORT，使MetalBone库不导出
#define METALBONE_EXPORT

// 定义STRIP_METALBONE_NAMESPACE，自动使用MetalBone命名空间
#define STRIP_METALBONE_NAMESPACE

// 定义METALBONE_NO_SIGSLOT，使用Signal/Slot。
// 如果禁用，则可能会用多态或callback函数代替部分功能
#define METALBONE_USE_SIGSLOT

// 使用单线程的Signal/Slot
//#define SIGSLOT_PURE_ISO

// 使用全局多线程的Signal/Slot
//#define SIGSLOT_MT_GLOBAL

#define MB_DEBUG

// 使用MSVC编译
#ifndef MSVC
#  define MSVC
#endif

#endif // MB_SWITCH_H
