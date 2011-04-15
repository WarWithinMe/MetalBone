#pragma once
#include "MBGlobal.h"
#include <unordered_map>
namespace MetalBone
{
	// A wrapper class for Win32 SetTimer and KillTimer
	// The interface is a copy of Qt's timer class - QTimer
	class MTimer
	{
		public:
			inline MTimer();
			~MTimer(){}

			inline bool isActive()         const;
			inline bool isSingleShot()     const;
			inline unsigned int timerId()  const;
			inline unsigned int interval() const;
			inline void setInterval(unsigned int msec);
			inline void setSingleShot(bool);

			inline void start(unsigned int msec);
			void start();
			void stop();

#ifdef METALBONE_USE_SIGSLOT
			Signal0 timeout;
#else
			virtual void timeout(){}
#endif
		private:
			static void __stdcall timerProc(HWND, UINT, UINT_PTR, DWORD);
			// TimerId - Interval
			typedef std::tr1::unordered_map<unsigned int, unsigned int> TimerIdMap;
			// Interval - MTimer
			typedef std::tr1::unordered_multimap<unsigned int, MTimer*> TimerHash;
			static TimerHash  timerHash;
			static TimerIdMap timerIdMap;
			unsigned int m_interval;
			unsigned int m_id;
			bool b_active;
			bool b_singleshot;
	};

	inline MTimer::MTimer():m_interval(0),m_id(0),
		b_active(false),b_singleshot(false){}
	inline bool MTimer::isActive() const
		{ return b_active; }
	inline bool MTimer::isSingleShot() const
		{ return b_singleshot; }
	inline unsigned int MTimer::interval() const
		{ return m_interval; }
	inline unsigned int MTimer::timerId() const
		{ return m_id; } 
	inline void MTimer::setInterval(unsigned int msec)
		{ m_interval = msec; }
	inline void MTimer::setSingleShot(bool single)
		{ b_singleshot = single; }
	inline void MTimer::start(unsigned int msec)
		{ m_interval = msec; start(); }
}


