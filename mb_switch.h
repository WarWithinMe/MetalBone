#ifndef MB_SWITCH_H
#define MB_SWITCH_H

// ����METALBONE_EXPORT��ʹMetalBone�ⲻ����
#define METALBONE_EXPORT

// ����STRIP_METALBONE_NAMESPACE���Զ�ʹ��MetalBone�����ռ�
#define STRIP_METALBONE_NAMESPACE

// ����METALBONE_NO_SIGSLOT��ʹ��Signal/Slot��
// ������ã�����ܻ��ö�̬��callback�������沿�ֹ���
#define METALBONE_USE_SIGSLOT

// ʹ�õ��̵߳�Signal/Slot
//#define SIGSLOT_PURE_ISO

// ʹ��ȫ�ֶ��̵߳�Signal/Slot
//#define SIGSLOT_MT_GLOBAL

#define MB_DEBUG

// ʹ��MSVC����
#ifndef MSVC
#  define MSVC
#endif

#endif // MB_SWITCH_H
