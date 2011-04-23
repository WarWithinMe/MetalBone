#ifndef GUI_MRESOURCE_H
#define GUI_MRESOURCE_H
#include "MBGlobal.h"
#include "MApplication.h"

#include <Windows.h>
#include <string>
#include <unordered_map>

namespace MetalBone
{
	class MCursor
	{
		public:
			enum CursorType
			{
				ArrowCursor,
				AppStartingCursor,
				CrossCursor,
				HandCursor,
				HelpCursor,
				IBeamCursor,
				WaitCursor,
				ForbiddenCursor,
				UpArrowCursor,

				SizeAllCursor,
				SizeVerCursor,
				SizeHorCursor,
				SizeBDiagCursor,
				SizeFDiagCursor,

				BlankCursor,

				CustomCursor
			};

			inline explicit MCursor(CursorType t = ArrowCursor);
			~MCursor();

			void setType(CursorType t);
			inline CursorType type() const;

			inline HCURSOR getHandle() const;

			// TODO: Add support for creating cursor dynamically.

			inline void loadCursorFromFile(const std::wstring&);
			// Loads a cursor from a resource embeded into this exe.
			inline void loadCursorFromRes(LPWSTR lpCursorName);

			// This function does not take control of the MCursor.
			// It's the caller's responsibility to delete the MCursor.
			// Setting this to 0 will cancle the App's Cursor, so that
			// it can change when hovering over different widgets.
			inline static void     setAppCursor(MCursor*);
			inline static MCursor* getAppCursor();
			inline static void     setPos(int x, int y);
			inline static POINT    pos();


			void show(bool forceUpdate = false) const;
		private:
			CursorType e_type;
			HCURSOR handle;
			static MCursor* appCursor;
			static const MCursor* showingCursor;
			void updateCursor(HCURSOR, CursorType);
			const MCursor& operator=(const MCursor&);
	};



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
			inline void setSingleShot(bool);
			void setInterval(unsigned int msec);

			inline void start(unsigned int msec);
			static void cleanUp();
			void start();
			void stop();

	#ifdef METALBONE_USE_SIGSLOT
			Signal0 timeout;
	#else
		protected:
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



	// Use for small resource files like theme images.
	class MResource
	{
		public:
			inline MResource();

			inline const BYTE*  byteBuffer() const;
			inline unsigned int length() const;

			// Open a file from the cache, if a file is already opened,
			// it will be closed.
			bool open(const std::wstring& fileName);
			// Open a file from a module, if a file is already opened,
			// it will be closed.
			bool open(HINSTANCE hInstance, const wchar_t* name,
					  const wchar_t* type);

			static void clearCache();
			static bool addFileToCache(const std::wstring& filePath);
			static bool addZipToCache(const std::wstring& zipPath);


#ifdef MB_DEBUG
			static void dumpCache();
#endif
		private:
			const BYTE* buffer;
			DWORD       size;

			struct ResourceEntry
			{
				BYTE* buffer;
				DWORD length;
				bool  unity;
				ResourceEntry():buffer(0),length(0),unity(false){}
				~ResourceEntry() { if(unity) delete[] buffer; }
			};
			typedef std::tr1::unordered_multimap<std::wstring,ResourceEntry*> ResourceCache;
			static ResourceCache cache;
	};



	class MShortCut
	{
		public:
			inline MShortCut();
			inline MShortCut(unsigned int modifier, unsigned int virtualKey,
				MWidget* target = 0, bool global = false);
			inline ~MShortCut();

			void setKey(unsigned int modifier, unsigned int virtualKey);
			void setGlobal(bool);
			// Setting the target to a widget will make shortcut invoke
			// only when the target has focus.
			// Setting it to 0 will invoke the shortcut when any window
			// of this app is actived.
			inline void setTarget(MWidget*);
			inline void setEnabled(bool);

			inline bool isGlobal() const;
			inline bool isEnabled() const;
			inline unsigned int getModifier()   const;
			inline unsigned int getVirtualKey() const;
			inline MWidget* getTarget() const;

			static std::vector<MShortCut*> getMachedShortCuts(unsigned int modifier, unsigned int virtualKey);
#ifdef METALBONE_USE_SIGSLOT
			Signal0 invoked;
#else
		protected:
			virtual void invoked(){}
#endif

		private:
			MShortCut(const MShortCut&);
			const MShortCut& operator=(const MShortCut&);
			unsigned int modifier;
			unsigned int virtualKey;
			MWidget* target;
			bool global;
			bool enabled;

			typedef std::tr1::unordered_multimap<unsigned int, MShortCut*> ShortCutMap;
			static ShortCutMap scMap;

			inline void removeFromMap();
			inline void insertToMap();
			inline unsigned int getKey();

	};

	MShortCut::MShortCut():modifier(0),virtualKey(0),target(0),global(false),enabled(false){}
	MShortCut::MShortCut(unsigned int m, unsigned int vk, MWidget* t, bool g):
		modifier(m),virtualKey(vk),target(t),global(false),enabled(true){ insertToMap(); if(g) setGlobal(true); }
	MShortCut::~MShortCut() { removeFromMap(); }

	inline bool MShortCut::isGlobal() const
		{ return global; }
	inline bool MShortCut::isEnabled() const
		{ return enabled; }
	inline unsigned int MShortCut::getModifier() const
		{ return modifier; }
	inline unsigned int MShortCut::getVirtualKey() const
		{ return virtualKey; }
	inline MWidget* MShortCut::getTarget() const
		{ return target; }
	inline void MShortCut::setTarget(MWidget* t)
		{ target = t; }
	inline void MShortCut::setEnabled(bool on)
		{ enabled = on; }
	inline void MShortCut::insertToMap()
		{ scMap.insert(ShortCutMap::value_type(getKey(),this)); }
	inline unsigned int MShortCut::getKey()
		{ return (modifier << 26) | virtualKey; }




	inline MResource::MResource():buffer(0),size(0){}
	inline const BYTE*  MResource::byteBuffer() const
		{ return buffer; }
	inline unsigned int MResource::length() const
		{ return size; }

	inline MCursor::MCursor(CursorType t):e_type(CustomCursor)
		{ setType(t); }
	inline MCursor::CursorType MCursor::type() const
		{ return e_type; }
	inline void MCursor::setPos(int x, int y)
		{ ::SetCursorPos(x,y); }
	inline POINT MCursor::pos()
		{ POINT p; ::GetCursorPos(&p); return p; }
	inline HCURSOR MCursor::getHandle() const
		{ return handle; }
	inline MCursor* MCursor::getAppCursor()
		{ return appCursor; }
	inline void MCursor::loadCursorFromFile(const std::wstring& path)
		{ updateCursor(::LoadCursorFromFileW(path.c_str()),CustomCursor); }
	inline void MCursor::loadCursorFromRes(LPWSTR res)
		{ updateCursor(::LoadCursorW(mApp->getAppHandle(),res),CustomCursor); }
	void MCursor::setAppCursor(MCursor* c)
	{
		if(appCursor == c) return;

		appCursor = c;
		appCursor->show();
	}

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
	inline void MTimer::setSingleShot(bool single)
		{ b_singleshot = single; }
	inline void MTimer::start(unsigned int msec)
		{ m_interval = msec; start(); }
}
#endif // MRESOURCE_H
