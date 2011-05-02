#ifndef GUI_MRESOURCE_H
#define GUI_MRESOURCE_H
#include "MBGlobal.h"
#include "MApplication.h"

#include <D2d1helper.h>
#include <Windows.h>
#include <string>
#include <unordered_map>

namespace MetalBone
{
	struct MFontData;
	// A wrapper class for HFONT
	class MFont
	{
		public:
			~MFont();
			MFont();
			MFont(const MFont&);
			explicit MFont(const LOGFONTW&);

			MFont(unsigned int size, const std::wstring& faceName,
				bool bold = false, bool italic = false,
				bool pixelUnit = true);

			HFONT getHandle();
			bool isBold()   const;
			bool isItalic() const;
			const std::wstring& getFaceName() const;
			unsigned int pointSize() const;

			operator HFONT() const;
			const MFont& operator=(const MFont&);
		private:
			MFontData* data;
	};



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
			inline ~MTimer();

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

			Signal0<> timeout;
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
			unsigned int size;


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

			Signal0<> invoked;
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



	// A class for storing ARGB(0xAARRGGBB) color value.
	class MColor
	{
		public:
			inline MColor();
			inline explicit MColor(unsigned int argb);
			inline MColor(const MColor&);

			inline void setRed  (BYTE red);
			inline void setGreen(BYTE green);
			inline void setBlue (BYTE blue);
			inline void setAlpha(BYTE alpha);

			inline BYTE getRed()   const;
			inline BYTE getGreen() const;
			inline BYTE getBlue()  const;
			inline BYTE getAlpha() const;
			inline bool isTransparent() const;

			inline unsigned int getARGB() const;
			inline unsigned int getRGB()  const;
			inline COLORREF getCOLORREF() const;

// 			inline static MColor fromRGBA(unsigned int);
			inline static MColor fromCOLORREF(COLORREF color, BYTE alpha);

			inline const MColor& operator=(const MColor&);
			inline bool operator==(const MColor&) const;

			inline operator D2D1_COLOR_F() const;
		private:
			unsigned int argb;
	};


// 	inline MColor MColor::fromRGBA(unsigned int rgba)
// 		{ return MColor(((rgba & 0xFF) << 24) | (rgba & 0xFFFFFF)); }
	inline MColor MColor::fromCOLORREF(COLORREF color, BYTE alpha)
	{
		unsigned int argb = alpha;
		argb = argb << 24 | ((color & 0xFF) << 16) | (color & 0xFF00) | (color >> 16) & 0xFF;
		return MColor(argb);
	}
	inline COLORREF MColor::getCOLORREF() const // ColorRef is bgr format.
		{ return ((argb & 0xFF) << 16) | (argb & 0xFF00) | (argb >> 16 & 0xFF); }
	inline MColor::MColor():argb(0xFF000000){}
	inline MColor::MColor(unsigned int c):argb(c){}
	inline MColor::MColor(const MColor& rhs):argb(rhs.argb){}
	inline void MColor::setRed(BYTE red)
		{ argb = argb & 0xFF00FFFF | (red << 16); }
	inline void MColor::setGreen(BYTE green)
		{ argb = argb & 0xFFFF00FF | (green << 8); }
	inline void MColor::setBlue(BYTE blue)
		{ argb = argb & 0xFFFFFF00 | blue; }
	inline void MColor::setAlpha(BYTE alpha)
		{ argb = argb & 0xFFFFFF | (alpha << 24); }
	inline BYTE MColor::getRed() const
		{ return BYTE(argb >> 16); }
	inline BYTE MColor::getGreen() const
		{ return BYTE(argb >> 8); }
	inline BYTE MColor::getBlue() const
		{ return (BYTE)argb; }
	inline BYTE MColor::getAlpha() const
		{ return BYTE(argb >> 24); }
	inline bool MColor::isTransparent() const
		{ return argb <= 0x00FFFFFF; }
	inline const MColor& MColor::operator=(const MColor& rhs)
		{ argb = rhs.argb; return *this; }
	inline bool MColor::operator==(const MColor& rhs) const
		{ return argb == rhs.argb; }
	inline unsigned int MColor::getARGB() const
		{ return argb; }
	inline unsigned int MColor::getRGB() const
		{ return argb & 0xFFFFFF; }
	inline MColor::operator D2D1_COLOR_F() const
		{ return D2D1::ColorF(argb & 0xFFFFFF, float(argb >> 24 & 0xFF) / 255); }



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
	inline MTimer::~MTimer() { stop(); }
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


template<>
class std::tr1::hash<MColor>
{
	public:
		size_t operator()(const MColor& color) const
			{ return std::tr1::hash<unsigned int>().operator()(color.getARGB()); }
};
#endif // MRESOURCE_H
