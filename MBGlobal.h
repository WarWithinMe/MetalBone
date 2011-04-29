#ifndef MBGLOBAL_H
#define MBGLOBAL_H
#include "mb_switch.h"
#include <Windows.h>

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

#pragma comment(lib, "d2d1.lib")          // Direct2D
#pragma comment(lib, "windowscodecs.lib") // WIC
#pragma comment(lib, "gdiplus.lib")       // Gdi+

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



#ifdef METALBONE_USE_SIGSLOT
// Signal/Slot classes，作了适量改动
// 作者：Sarah Thompson (sarah@telergy.com) 2002.
// 完整文档： http://sigslot.sourceforge.net/
//
// 线程模式：
//   单线程：          Your program is assumed to be single threaded from the point of view
//                     of signal/slot usage (i.e. all objects using signals and slots are
//                     created and destroyed from a single thread). Behaviour if objects are
//                     destroyed concurrently is undefined (i.e. you'll get the occasional
//                     segmentation fault/memory exception).
//
//   全局多线程：      Your program is assumed to be multi threaded. Objects using signals and
//                     slots can be safely created and destroyed from any thread, even when
//                     connections exist. In multi_threaded_global mode, this is achieved by a
//                     single global mutex (actually a critical section on Windows because they
//                     are faster). This option uses less OS resources, but results in more
//                     opportunities for contention, possibly resulting in more context switches
//                     than are strictly necessary.
//
//   局部多线程：      Behaviour in this mode is essentially the same as multi_threaded_global,
//                     except that each signal, and each object that inherits has_slots, all
//                     have their own mutex/critical section. In practice, this means that
//                     mutex collisions (and hence context switches) only happen if they are
//                     absolutely essential. However, on some platforms, creating a lot of
//                     mutexes can slow down the whole OS, so use this option with care.
//
// 使用例子：
//  class Slot : public has_slots { void testFunc(int){} }
//  void test() {
//    Signal1<int> mySignal;
//    Slot mySlot;
//    mySignal.connect(&mySlot,&Slot::testFunc);
//    mySignal.emit(5); // 调用了一次mySlot.testFunc(5);
//    mySignal.connect(&mySlot,&Slot::testFunc);
//    mySignal(5); // 调用了两次mySlot.testFunc(5);，因为链接了两次
//    mySignal.disconnect(&mySlot); // 断开了与mySlot的所有链接
//    mySignal.disconnect_all(); // 断开了所有链接
//  }
#include <set>
#include <list>

#if defined(SIGSLOT_PURE_ISO)
#  define SIGSLOT_DEFAULT_MT_POLICY single_threaded
#  define MT_POLICY single_threaded
#else
#  include <windows.h>
#ifdef SIGSLOT_MT_GLOBAL
#    define SIGSLOT_DEFAULT_MT_POLICY multi_threaded_global
#    define MT_POLICY multi_threaded_global
#  else
#    define SIGSLOT_DEFAULT_MT_POLICY multi_threaded_local
#    define MT_POLICY multi_threaded_local
#  endif
#endif

namespace MetalBone
{
	class single_threaded
	{
		public:
			single_threaded(){}
			~single_threaded(){}
			void lock(){}
			void unlock(){}
	};

#ifndef SIGSLOT_PURE_ISO
	class multi_threaded_global
	{
		public:
			multi_threaded_global()
			{
				static bool isinitialised = false;

				if(!isinitialised)
				{
					InitializeCriticalSection(get_critsec());
					isinitialised = true;
				}
			}

			~multi_threaded_global(){}
			void lock() { EnterCriticalSection(get_critsec()); }
			void unlock() { LeaveCriticalSection(get_critsec()); }
		private:
			inline CRITICAL_SECTION* get_critsec()
			{
				static CRITICAL_SECTION g_critsec;
				return &g_critsec;
			}
	};

	class multi_threaded_local
	{
		// TODO: Remove
			multi_threaded_local(const multi_threaded_local&) { InitializeCriticalSection(&m_critsec); }
		public:
			multi_threaded_local() { InitializeCriticalSection(&m_critsec); }
			~multi_threaded_local(){ DeleteCriticalSection(&m_critsec); }
			void lock() { EnterCriticalSection(&m_critsec); }
			void unlock() { LeaveCriticalSection(&m_critsec); }
		private:
			CRITICAL_SECTION m_critsec;
	};
#endif

	class lock_block
	{
		public:
			MT_POLICY* m_mutex;
			lock_block(MT_POLICY* mtx) : m_mutex(mtx) { m_mutex->lock(); }
			~lock_block() { m_mutex->unlock(); }
	};

	class SignalBase;
	class has_slots
	{
		private:
			typedef std::set<SignalBase*>::const_iterator const_iterator;

		public:
			has_slots(){}
			// 在multi_threaded_local的情况下，线程不安全
			// 因为没有锁上hs的m_mutex。不建议使用复制构造函数
			has_slots(const has_slots& hs);
			virtual ~has_slots();
			void disconnect_all();

			// 内部函数，应调用Signal::connect()，直接调用会导致undefined behavior
			void signal_connect(SignalBase* sender);
			// 内部函数，应调用Signal::disconnect()，直接调用会导致undefined behavior
			void signal_disconnect(SignalBase* sender);

		private:
			std::set<SignalBase*> m_senders;
			MT_POLICY m_mutex;
	};

	class ConnectionBase0
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit() = 0;
			virtual ConnectionBase0* clone() = 0;
			virtual ConnectionBase0* duplicate(has_slots*) = 0;
	};

	template<class T1>
	class ConnectionBase1
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1) = 0;
			virtual ConnectionBase1<T1>* clone() = 0;
			virtual ConnectionBase1<T1>* duplicate(has_slots*) = 0;
	};

	template<class T1, class T2>
	class ConnectionBase2
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1, T2) = 0;
			virtual ConnectionBase2<T1, T2>* clone() = 0;
			virtual ConnectionBase2<T1, T2>* duplicate(has_slots*) = 0;
	};

	template<class T1, class T2, class T3>
	class ConnectionBase3
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1, T2, T3) = 0;
			virtual ConnectionBase3<T1, T2, T3>* clone() = 0;
			virtual ConnectionBase3<T1, T2, T3>* duplicate(has_slots*) = 0;
	};

	template<class T1, class T2, class T3, class T4>
	class ConnectionBase4
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1, T2, T3, T4) = 0;
			virtual ConnectionBase4<T1, T2, T3, T4>* clone() = 0;
			virtual ConnectionBase4<T1, T2, T3, T4>* duplicate(has_slots*) = 0;
	};

	template<class T1, class T2, class T3, class T4, class T5>
	class ConnectionBase5
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1, T2, T3, T4, T5) = 0;
			virtual ConnectionBase5<T1, T2, T3, T4, T5>* clone() = 0;
			virtual ConnectionBase5<T1, T2, T3, T4, T5>* duplicate(has_slots*) = 0;
	};

	template<class T1, class T2, class T3, class T4, class T5, class T6>
	class ConnectionBase6
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1, T2, T3, T4, T5, T6) = 0;
			virtual ConnectionBase6<T1, T2, T3, T4, T5, T6>* clone() = 0;
			virtual ConnectionBase6<T1, T2, T3, T4, T5, T6>* duplicate(has_slots*) = 0;
	};

	template<class T1, class T2, class T3, class T4, class T5, class T6, class T7>
	class ConnectionBase7
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1, T2, T3, T4, T5, T6, T7) = 0;
			virtual ConnectionBase7<T1, T2, T3, T4, T5, T6, T7>* clone() = 0;
			virtual ConnectionBase7<T1, T2, T3, T4, T5, T6, T7>* duplicate(has_slots*) = 0;
	};

	template<class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
	class ConnectionBase8
	{
		public:
			virtual has_slots* getdest() const = 0;
			virtual void emit(T1, T2, T3, T4, T5, T6, T7, T8) = 0;
			virtual ConnectionBase8<T1, T2, T3, T4, T5, T6, T7, T8>* clone() = 0;
			virtual ConnectionBase8<T1, T2, T3, T4, T5, T6, T7, T8>* duplicate(has_slots*) = 0;
	};

	template<class DestT>
	class Connection0 : public ConnectionBase0
	{
		public:
			Connection0() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection0(DestT* pobject, void (DestT::*pmemfun)()) : m_pobject(pobject), m_pmemfun(pmemfun){}

			virtual ConnectionBase0* clone()
			{
				return new Connection0<DestT>(*this);
			}
			virtual ConnectionBase0* duplicate(has_slots* pnewdest)
			{
				return new Connection0<DestT>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit()
			{
				(m_pobject->*m_pmemfun)();
			}
			virtual has_slots* getdest() const { return m_pobject; }

		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)();
	};

	template<class DestT, class T1>
	class Connection1 : public ConnectionBase1<T1>
	{
		public:
			Connection1() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection1(DestT* pobject, void (DestT::*pmemfun)(T1)) : m_pobject(pobject), m_pmemfun(pmemfun){}

			virtual ConnectionBase1<T1>* clone()
			{
				return new Connection1<DestT, T1>(*this);
			}
			virtual ConnectionBase1<T1>* duplicate(has_slots* pnewdest)
			{
				return new Connection1<DestT, T1>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1)
			{
				(m_pobject->*m_pmemfun)(a1);
			}
			virtual has_slots* getdest() const { return m_pobject; }

		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1);
	};

	template<class DestT, class T1, class T2>
	class Connection2 : public ConnectionBase2<T1, T2>
	{
		public:
			Connection2() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection2(DestT* pobject, void (DestT::*pmemfun)(T1,T2))
				: m_pobject(pobject), m_pmemfun(pmemfun){}

			virtual ConnectionBase2<T1, T2>* clone()
			{
				return new Connection2<DestT, T1, T2>(*this);
			}
			virtual ConnectionBase2<T1, T2>* duplicate(has_slots* pnewdest)
			{
				return new Connection2<DestT, T1, T2>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1, T2 a2)
			{
				(m_pobject->*m_pmemfun)(a1, a2);
			}
			virtual has_slots* getdest() const { return m_pobject; }

		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1, T2);
	};

	template<class DestT, class T1, class T2, class T3>
	class Connection3 : public ConnectionBase3<T1, T2, T3>
	{
		public:
			Connection3() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection3(DestT* pobject, void (DestT::*pmemfun)(T1, T2, T3))
				: m_pobject(pobject),m_pmemfun(pmemfun){}

			virtual ConnectionBase3<T1, T2, T3>* clone()
			{
				return new Connection3<DestT, T1, T2, T3>(*this);
			}
			virtual ConnectionBase3<T1, T2, T3>* duplicate(has_slots* pnewdest)
			{
				return new Connection3<DestT, T1, T2, T3>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1, T2 a2, T3 a3)
			{
				(m_pobject->*m_pmemfun)(a1, a2, a3);
			}
			virtual has_slots* getdest() const { return m_pobject; }
		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1, T2, T3);
	};

	template<class DestT, class T1, class T2, class T3, class T4>
	class Connection4 : public ConnectionBase4<T1, T2, T3, T4>
	{
		public:
			Connection4() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection4(DestT* pobject, void (DestT::*pmemfun)(T1, T2, T3, T4))
				: m_pobject(pobject),m_pmemfun(pmemfun){}

			virtual ConnectionBase4<T1, T2, T3, T4>* clone()
			{
				return new Connection4<DestT, T1, T2, T3, T4>(*this);
			}
			virtual ConnectionBase4<T1, T2, T3, T4>* duplicate(has_slots* pnewdest)
			{
				return new Connection4<DestT, T1, T2, T3, T4>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1, T2 a2, T3 a3, T4 a4)
			{
				(m_pobject->*m_pmemfun)(a1, a2, a3, a4);
			}
			virtual has_slots* getdest() const { return m_pobject; }
		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1, T2, T3, T4);
	};

	template<class DestT, class T1, class T2, class T3, class T4, class T5>
	class Connection5 : public ConnectionBase5<T1, T2, T3, T4, T5>
	{
		public:
			Connection5() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection5(DestT* pobject, void (DestT::*pmemfun)(T1, T2, T3, T4, T5))
				: m_pobject(pobject),m_pmemfun(pmemfun){}

			virtual ConnectionBase5<T1, T2, T3, T4, T5>* clone()
			{
				return new Connection5<DestT, T1, T2, T3, T4, T5>(*this);
			}
			virtual ConnectionBase5<T1, T2, T3, T4, T5>* duplicate(has_slots* pnewdest)
			{
				return new Connection5<DestT, T1, T2, T3, T4, T5>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5)
			{
				(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5);
			}
			virtual has_slots* getdest() const { return m_pobject; }
		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1, T2, T3, T4, T5);
	};

	template<class DestT, class T1, class T2, class T3, class T4, class T5, class T6>
	class Connection6 : public ConnectionBase6<T1, T2, T3, T4, T5, T6>
	{
		public:
			Connection6() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection6(DestT* pobject, void (DestT::*pmemfun)(T1, T2, T3, T4, T5, T6))
				: m_pobject(pobject),m_pmemfun(pmemfun){}

			virtual ConnectionBase6<T1, T2, T3, T4, T5, T6>* clone()
			{
				return new Connection6<DestT, T1, T2, T3, T4, T5, T6>(*this);
			}
			virtual ConnectionBase6<T1, T2, T3, T4, T5, T6>* duplicate(has_slots* pnewdest)
			{
				return new Connection6<DestT, T1, T2, T3, T4, T5, T6>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6)
			{
				(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6);
			}
			virtual has_slots* getdest() const { return m_pobject; }

		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1, T2, T3, T4, T5, T6);
	};

	template<class DestT, class T1, class T2, class T3, class T4, class T5, class T6, class T7>
	class Connection7 : public ConnectionBase7<T1, T2, T3, T4, T5, T6, T7>
	{
		public:
			Connection7() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection7(DestT* pobject, void (DestT::*pmemfun)(T1, T2, T3, T4, T5, T6, T7))
				: m_pobject(pobject),m_pmemfun(pmemfun){}

			virtual ConnectionBase7<T1, T2, T3, T4, T5, T6, T7>* clone()
			{
				return new Connection7<DestT, T1, T2, T3, T4, T5, T6, T7>(*this);
			}
			virtual ConnectionBase7<T1, T2, T3, T4, T5, T6, T7>* duplicate(has_slots* pnewdest)
			{
				return new Connection7<DestT, T1, T2, T3, T4, T5, T6, T7>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7)
			{
				(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6, a7);
			}
			virtual has_slots* getdest() const { return m_pobject; }

		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1, T2, T3, T4, T5, T6, T7);
	};

	template<class DestT, class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
	class Connection8 : public ConnectionBase8<T1, T2, T3, T4, T5, T6, T7, T8>
	{
		public:
			Connection8() : m_pobject(NULL), m_pmemfun(NULL){}
			Connection8(DestT* pobject, void (DestT::*pmemfun)(T1, T2, T3, T4, T5, T6, T7, T8))
				: m_pobject(pobject),m_pmemfun(pmemfun){}

			virtual ConnectionBase8<T1, T2, T3, T4, T5, T6, T7, T8>* clone()
			{
				return new Connection8<DestT, T1, T2, T3, T4, T5, T6, T7, T8>(*this);
			}

			virtual ConnectionBase8<T1, T2, T3, T4, T5, T6, T7, T8>* duplicate(has_slots* pnewdest)
			{
				return new Connection8<DestT, T1, T2, T3, T4, T5, T6, T7, T8>((DestT *)pnewdest, m_pmemfun);
			}
			virtual void emit(T1 a1, T2 a2, T3 a3, T4 a4, T5 a5, T6 a6, T7 a7, T8 a8)
			{
				(m_pobject->*m_pmemfun)(a1, a2, a3, a4, a5, a6, a7, a8);
			}
			virtual has_slots* getdest() const { return m_pobject; }

		private:
			DestT* m_pobject;
			void (DestT::* m_pmemfun)(T1, T2, T3, T4, T5, T6, T7, T8);
	};

	class SignalBase
	{
		public:
			SignalBase(){}
			SignalBase(const SignalBase&){}
			virtual void disconnect_slot(has_slots* pslot) = 0;
			virtual void duplicate_slot(const has_slots* poldslot, has_slots* pnewslot) = 0;

		protected:
			template <typename ListT>
			void copyConnections(const ListT& src, ListT& tgt)
			{
				// 使用multi_threaded_local的情况下，线程不安全
				// 因为没有锁上src的m_mutex。但是如果锁上src的m_mutex
				// 又可能会导致死锁。
				lock_block lock(&m_mutex);

				typename ListT::const_iterator it = src.begin();
				typename ListT::const_iterator itEnd = src.end();
				typedef typename ListT::value_type ConT;

				ConT clone = 0;
				while(it != itEnd)
				{
					clone = (*it)->clone();
					if(clone)
					{
						clone->getdest()->signal_connect(this);
						tgt.push_back(clone);
					}
					++it;
				}
			}

			template <typename ListT>
			void freeConnections(ListT& list)
			{
				lock_block lock(&m_mutex);
				typename ListT::const_iterator it = list.begin();
				typename ListT::const_iterator itEnd = list.end();
				while(it != itEnd)
				{
					(*it)->getdest()->signal_disconnect(this);
					delete *it;
					++it;
				}
				list.clear();
			}

			template <typename ListT>
			void disconnect_helper(has_slots* pclass,ListT& list,bool signalDisconect)
			{
				lock_block lock(&m_mutex);
				typename ListT::iterator it = list.begin();
				typename ListT::iterator itEnd = list.end();

				bool diconnected = false;
				while(it != itEnd)
				{
					if((*it)->getdest() == pclass)
					{
						// 如果一个slot多次链接到同一个signal，则在
						// 这个slot被删除，从而导致要移除signal的记录时，
						// 要删除所有这个slot的记录，而不是一次
						diconnected = true;
						delete *it;
						it = list.erase(it);
						continue;
					}
					++it;
				}

				if(signalDisconect && diconnected)
					pclass->signal_disconnect(this);
			}

			template <typename ListT>
			void duplicate_helper(const has_slots* oldtarget, has_slots* newtarget, ListT& list)
			{
				lock_block lock(&m_mutex);
				typename ListT::iterator it = list.begin();
				typename ListT::iterator itEnd = list.end();

				while(it != itEnd)
				{
					if((*it)->getdest() == oldtarget)
						list.push_back((*it)->duplicate(newtarget));

					++it;
				}
			}

			mutable MT_POLICY m_mutex;
	};

	class Signal0 : public SignalBase
	{
		typedef std::list<ConnectionBase0*>::const_iterator const_iterator;
		typedef std::list<ConnectionBase0*>::iterator iterator;
		std::list<ConnectionBase0*> m_connected_slots;

		public:
			~Signal0() { disconnect_all(); }
			Signal0(){}
			// 不建议使用Signal的复制构造函数，因为它调用SignalBase的copyConnections()，
			// 在multi_threaded_local的情况下，线程不安全
			Signal0(const Signal0& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)())
			{
				lock_block lock(&m_mutex);
				Connection0<DestT>* conn = new Connection0<DestT>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()() const{ emit(); }
			void emit() const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit();
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }
			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }

			// 内部函数
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1>
	class Signal1 : public SignalBase
	{
		typedef typename std::list<ConnectionBase1<T1>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase1<T1>*>::iterator iterator;
		std::list<ConnectionBase1<T1>*> m_connected_slots;

		public:
			~Signal1() { disconnect_all(); }
			Signal1(){}
			Signal1(const Signal1<T1>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1))
			{
				lock_block lock(&m_mutex);
				Connection1<DestT,T1>* conn = new Connection1<DestT,T1>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1) const{ emit(t1); }
			void emit(T1 t1) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1,class T2>
	class Signal2 : public SignalBase
	{
		typedef typename std::list<ConnectionBase2<T1,T2>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase2<T1,T2>*>::iterator iterator;
		std::list<ConnectionBase2<T1,T2>*> m_connected_slots;

		public:
			~Signal2() { disconnect_all(); }
			Signal2(){}
			Signal2(const Signal2<T1,T2>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1,T2))
			{
				lock_block lock(&m_mutex);
				Connection2<DestT,T1,T2>* conn = new Connection2<DestT,T1,T2>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1,T2 t2) const{ emit(t1,t2); }
			void emit(T1 t1,T2 t2) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1,t2);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1,class T2,class T3>
	class Signal3 : public SignalBase
	{
		typedef typename std::list<ConnectionBase3<T1,T2,T3>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase3<T1,T2,T3>*>::iterator iterator;
		std::list<ConnectionBase3<T1,T2,T3>*> m_connected_slots;

		public:
			~Signal3() { disconnect_all(); }
			Signal3(){}
			Signal3(const Signal3<T1,T2,T3>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1,T2,T3))
			{
				lock_block lock(&m_mutex);
				Connection3<DestT,T1,T2,T3>* conn = new Connection3<DestT,T1,T2,T3>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1,T2 t2,T3 t3) const{ emit(t1,t2,t3); }
			void emit(T1 t1,T2 t2,T3 t3) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1,t2,t3);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1,class T2,class T3,class T4>
	class Signal4 : public SignalBase
	{
		typedef typename std::list<ConnectionBase4<T1,T2,T3,T4>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase4<T1,T2,T3,T4>*>::iterator iterator;
		std::list<ConnectionBase4<T1,T2,T3,T4>*> m_connected_slots;

		public:
			~Signal4() { disconnect_all(); }
			Signal4(){}
			Signal4(const Signal4<T1,T2,T3,T4>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1,T2,T3,T4))
			{
				lock_block lock(&m_mutex);
				Connection4<DestT,T1,T2,T3,T4>* conn = new Connection4<DestT,T1,T2,T3,T4>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1,T2 t2,T3 t3,T4 t4) const{ emit(t1,t2,t3,t4); }
			void emit(T1 t1,T2 t2,T3 t3,T4 t4) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1,t2,t3,t4);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1,class T2,class T3,class T4,class T5>
	class Signal5 : public SignalBase
	{
		typedef typename std::list<ConnectionBase5<T1,T2,T3,T4,T5>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase5<T1,T2,T3,T4,T5>*>::iterator iterator;
		std::list<ConnectionBase5<T1,T2,T3,T4,T5>*> m_connected_slots;

		public:
			~Signal5() { disconnect_all(); }
			Signal5(){}
			Signal5(const Signal5<T1,T2,T3,T4,T5>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1,T2,T3,T4,T5))
			{
				lock_block lock(&m_mutex);
				Connection5<DestT,T1,T2,T3,T4,T5>* conn = new Connection5<DestT,T1,T2,T3,T4,T5>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5) const{ emit(t1,t2,t3,t4,t5); }
			void emit(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1,t2,t3,t4,t5);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1,class T2,class T3,class T4,class T5,class T6>
	class Signal6 : public SignalBase
	{
		typedef typename std::list<ConnectionBase6<T1,T2,T3,T4,T5,T6>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase6<T1,T2,T3,T4,T5,T6>*>::iterator iterator;
		std::list<ConnectionBase6<T1,T2,T3,T4,T5,T6>*> m_connected_slots;

		public:
			~Signal6() { disconnect_all(); }
			Signal6(){}
			Signal6(const Signal6<T1,T2,T3,T4,T5,T6>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1,T2,T3,T4,T5,T6))
			{
				lock_block lock(&m_mutex);
				Connection6<DestT,T1,T2,T3,T4,T5,T6>* conn = new Connection6<DestT,T1,T2,T3,T4,T5,T6>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6) const{ emit(t1,t2,t3,t4,t5,t6); }
			void emit(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1,t2,t3,t4,t5,t6);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7>
	class Signal7 : public SignalBase
	{
		typedef typename std::list<ConnectionBase7<T1,T2,T3,T4,T5,T6,T7>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase7<T1,T2,T3,T4,T5,T6,T7>*>::iterator iterator;
		std::list<ConnectionBase7<T1,T2,T3,T4,T5,T6,T7>*> m_connected_slots;

		public:
			~Signal7() { disconnect_all(); }
			Signal7(){}
			Signal7(const Signal7<T1,T2,T3,T4,T5,T6,T7>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1,T2,T3,T4,T5,T6,T7))
			{
				lock_block lock(&m_mutex);
				Connection7<DestT,T1,T2,T3,T4,T5,T6,T7>* conn = new Connection7<DestT,T1,T2,T3,T4,T5,T6,T7>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6,T7 t7) const{ emit(t1,t2,t3,t4,t5,t6,t7); }
			void emit(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6,T7 t7) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1,t2,t3,t4,t5,t6,t7);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};

	template<class T1,class T2,class T3,class T4,class T5,class T6,class T7,class T8>
	class Signal8 : public SignalBase
	{
		typedef typename std::list<ConnectionBase8<T1,T2,T3,T4,T5,T6,T7,T8>*>::const_iterator const_iterator;
		typedef typename std::list<ConnectionBase8<T1,T2,T3,T4,T5,T6,T7,T8>*>::iterator iterator;
		std::list<ConnectionBase8<T1,T2,T3,T4,T5,T6,T7,T8>*> m_connected_slots;

		public:
			~Signal8() { disconnect_all(); }
			Signal8(){}
			Signal8(const Signal8<T1,T2,T3,T4,T5,T6,T7,T8>& s) : SignalBase(s)
			{
				copyConnections(s.m_connected_slots,m_connected_slots);
			}
			template<class DestT>
			void connect(DestT* pclass, void (DestT::*pmemfun)(T1,T2,T3,T4,T5,T6,T7,T8))
			{
				lock_block lock(&m_mutex);
				Connection8<DestT,T1,T2,T3,T4,T5,T6,T7,T8>* conn = new Connection8<DestT,T1,T2,T3,T4,T5,T6,T7,T8>(pclass, pmemfun);
				m_connected_slots.push_back(conn);
				pclass->signal_connect(this);
			}

			void operator()(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6,T7 t7,T8 t8) const{ emit(t1,t2,t3,t4,t5,t6,t7,t8); }
			void emit(T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6,T7 t7,T8 t8) const
			{
				lock_block lock(&m_mutex);
				const_iterator it = m_connected_slots.begin();
				const_iterator itEnd = m_connected_slots.end();
				while(it != itEnd)
				{
					(*it)->emit(t1,t2,t3,t4,t5,t6,t7,t8);
					++it;
				}
			}

			void disconnect_all() { freeConnections(m_connected_slots); }

			void disconnect(has_slots* pclass) { disconnect_helper(pclass,m_connected_slots,true); }
			void disconnect_slot(has_slots *pclass) { disconnect_helper(pclass,m_connected_slots,false); }
			void duplicate_slot(const has_slots *olds, has_slots *news) { duplicate_helper(olds,news,m_connected_slots); }
	};
} // namespace MetalBone
#endif // METALBONE_USE_SIGSLOT

#endif // MBGLOBAL_H
