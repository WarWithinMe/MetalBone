#ifndef GUI_MRESOURCE_H
#define GUI_MRESOURCE_H
#include "MBGlobal.h"

#include <Windows.h>
#include <string>
#ifdef MSVC
#  include <unordered_map>
#else
#  include <tr1/unordered_map>
#endif

namespace MetalBone
{
	// Use for small resource files like theme images.
	class MResource
	{
		public:
			MResource():buffer(0),size(0){}

			const BYTE*		byteBuffer() const	{ return buffer; }
			unsigned int	length() const		{ return size; }

			// Open a file from the cache, if a file is already opened,
			// it will be closed.
			bool open(const std::wstring& fileName);
			// Open a file from a module, if a file is already opened,
			// it will be closed.
			bool open(HINSTANCE hInstance,
					  const wchar_t* name,
					  const wchar_t* type);

			static void clearCache();
			static bool addFileToCache(const std::wstring& filePath);
			static bool addZipToCache(const std::wstring& zipPath);

#ifdef MB_DEBUG
			static void dumpCache();
#endif

		private:
			const BYTE*	buffer;
			DWORD		size;

			struct ResourceEntry
			{
				BYTE*			buffer;
				DWORD			length;
				bool			unity;

				ResourceEntry():buffer(0),length(0),unity(false){}
				~ResourceEntry()
				{
					if(unity)
						delete[] buffer;
				}
			};

			typedef std::tr1::unordered_multimap<std::wstring,ResourceEntry*> ResourceCache;
			static ResourceCache cache;
	};
}
#endif // MRESOURCE_H
