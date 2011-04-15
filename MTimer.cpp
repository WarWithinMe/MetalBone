#include "MTimer.h"
namespace MetalBone
{
	MTimer::TimerHash MTimer::timerHash;
	MTimer::TimerIdMap MTimer::timerIdMap;
	void MTimer::start()
	{
		if(b_active) return;
		b_active = true;
		TimerHash::iterator it = timerHash.find(m_interval);
		TimerHash::iterator itEnd = timerHash.end();
		if(it == itEnd) {
			m_id = SetTimer(NULL,0,m_interval,timerProc);
			timerIdMap.insert(TimerIdMap::value_type(m_id,m_interval));
		} else { m_id = it->second->m_id; }
		timerHash.insert(TimerHash::value_type(m_interval,this));
	}
	void MTimer::stop()
	{
		if(!b_active) return;
		b_active = false;
		TimerHash::iterator it = timerHash.find(m_interval);
		TimerHash::iterator itEnd = timerHash.end();
		while(it != itEnd) { if(it->second == this) break; }
		timerHash.erase(it);
		it = timerHash.find(m_interval);
		if(it == itEnd) { 
			timerIdMap.erase(m_id);
			KillTimer(NULL,m_id);
		}
		m_id = 0;
	}

	void __stdcall MTimer::timerProc(HWND, UINT, UINT_PTR id, DWORD)
	{
		TimerIdMap::iterator it = timerIdMap.find(id);
		TimerIdMap::iterator itEnd = timerIdMap.end();
		if(it == itEnd)
			return;
		unsigned int interval = it->second;
		std::pair<TimerHash::iterator,TimerHash::iterator> range =
			timerHash.equal_range(interval);
		while(range.first != range.second)
		{
			MTimer* timer = range.first->second;
			if(timer->isSingleShot()) {
				timer->stop();
				range = timerHash.equal_range(interval);
			} else
				++range.first;
#ifdef METALBONE_USE_SIGSLOT
			timer->timeout.emit();
#else
			timer->timeout();
#endif
		}
	}

	void MTimer::cleanUp()
	{
		TimerIdMap::iterator it = timerIdMap.begin();
		TimerIdMap::iterator itEnd = timerIdMap.end();
		while(it != itEnd)
		{
			KillTimer(NULL,it->first);
			++it;
		}
		timerIdMap.clear();
		TimerHash::iterator tit = timerHash.begin();
		TimerHash::iterator titEnd = timerHash.end();
		while(tit != titEnd)
		{
			tit->second->b_active = false;
			tit->second->m_id = 0;
		}
		timerHash.clear();
	}

	void MTimer::setInterval(unsigned int msec)
	{
		m_interval = msec;
		if(!b_active)
			return;
		stop();
		start();
	}
}