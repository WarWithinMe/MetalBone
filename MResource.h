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

	inline MResource::MResource():buffer(0),size(0){}
	inline const BYTE*  MResource::byteBuffer() const { return buffer; }
	inline unsigned int MResource::length()     const { return size; }
}
#endif // MRESOURCE_H
