#include "MResource.h"
#include "3rd/XUnzip.h"
namespace MetalBone
{
	// ========== MCursor ==========
	MCursor* MCursor::appCursor = 0;
	const MCursor* MCursor::showingCursor = 0;
	void MCursor::setType(CursorType t)
	{
		M_ASSERT(t != CustomCursor);
		if(e_type == t)
			return;

		HCURSOR newHandle = NULL;
		if(t == BlankCursor)
		{
			BYTE andMask[] = { 0xFF };
			BYTE xorMask[] = { 0 };
			newHandle = ::CreateCursor(mApp->getAppHandle(),0,0,8,1,andMask,xorMask);
		} else {
			LPWSTR c;
			switch(t)
			{
				case ArrowCursor:       c = IDC_ARROW;       break;
				case CrossCursor:       c = IDC_CROSS;       break;
				case HandCursor:        c = IDC_HAND;        break;
				case HelpCursor:        c = IDC_HELP;        break;
				case IBeamCursor:       c = IDC_IBEAM;       break;
				case WaitCursor:        c = IDC_WAIT;        break;
				case SizeAllCursor:     c = IDC_SIZEALL;     break;
				case SizeVerCursor:     c = IDC_SIZENS;      break;
				case SizeHorCursor:     c = IDC_SIZEWE;      break;
				case SizeBDiagCursor:   c = IDC_SIZENESW;    break;
				case SizeFDiagCursor:   c = IDC_SIZENWSE;    break;
				case AppStartingCursor: c = IDC_APPSTARTING; break;
				case ForbiddenCursor:   c = IDC_NO;          break;
				case UpArrowCursor:     c = IDC_UPARROW;     break;
			}
			newHandle = ::LoadCursorW(NULL, c);
		}

		updateCursor(newHandle, t);
	}

	void MCursor::updateCursor(HCURSOR h, CursorType t)
	{
		if(h == NULL)
		{
			h = ::LoadCursorW(NULL,IDC_ARROW);
			t = ArrowCursor; 
		}

		HCURSOR oldCursor = handle;
		if(showingCursor == this)
			::SetCursor(h);
		if(e_type == BlankCursor || e_type == CustomCursor)
			::DestroyCursor(oldCursor);

		handle = h;
		e_type = t;
	}

	MCursor::~MCursor()
	{
		if(showingCursor == this)
		{
			showingCursor = 0;
			if(e_type != ArrowCursor)
				::SetCursor(::LoadCursorW(NULL,IDC_ARROW));
		}

		if(e_type == BlankCursor || e_type == CustomCursor)
			::DestroyCursor(handle);
	}

	void MCursor::show(bool forceUpdate) const
	{
		if(appCursor != 0)
			return;

		if(!forceUpdate)
		{
			if(showingCursor == this)
				return;

			if(showingCursor != 0)
			{
				if(showingCursor->e_type != CustomCursor)
				{
					if(showingCursor->e_type == e_type)
						return;
				} else if(showingCursor->handle == handle)
					return;
			}
		}

		showingCursor = this;
		::SetCursor(handle);
	}




	// ========== MResource ==========
	MResource::ResourceCache MResource::cache;
	bool MResource::open(HINSTANCE hInstance, const wchar_t* name, const wchar_t* type)
	{
		HRSRC hrsrc = ::FindResourceW(hInstance,name,type);
		if(hrsrc) {
			HGLOBAL hglobal = ::LoadResource(hInstance,hrsrc);
			if(hglobal)
			{
				buffer = (const BYTE*)::LockResource(hglobal);
				if(buffer) {
					size = ::SizeofResource(hInstance,hrsrc);
					return true;
				}
			}
		}
		return false;
	}

	bool MResource::open(const std::wstring& fileName)
	{
		ResourceCache::const_iterator iter;
		if(fileName.at(0) == L':' && fileName.at(1) == L'/')
		{
			std::wstring temp(fileName,2,fileName.length() - 2);
			iter = cache.find(temp);
		} else {
			iter = cache.find(fileName);
		}
		if(iter == cache.cend())
			return false;

		buffer = iter->second->buffer;
		size   = iter->second->length;
		return true;
	}

	void MResource::clearCache()
	{
		ResourceCache::const_iterator it    = cache.cbegin();
		ResourceCache::const_iterator itEnd = cache.cend();
		while(it != itEnd) { delete it->second; ++it; }
		cache.clear();
	}

	bool MResource::addFileToCache(const std::wstring& filePath)
	{
		if(cache.find(filePath) != cache.cend())
			return false;

		HANDLE resFile = ::CreateFileW(filePath.c_str(),GENERIC_READ,
			FILE_SHARE_READ,0,OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,0);
		if(resFile == INVALID_HANDLE_VALUE)
			return false;

		ResourceEntry* newEntry = new ResourceEntry();

		DWORD size = ::GetFileSize(resFile,0);
		newEntry->length = size;
		newEntry->unity  = true;
		newEntry->buffer = new BYTE[size];

		DWORD readSize;
		if(::ReadFile(resFile,newEntry->buffer,size,&readSize,0))
		{
			if(readSize != size) {
				delete newEntry;
			} else {
				cache.insert(ResourceCache::value_type(filePath, newEntry));
				return true;
			}
		} else {
			delete newEntry;
		}
		return false;
	}

	bool MResource::addZipToCache(const std::wstring& zipPath)
	{
		HZIP zip = OpenZip((void*)zipPath.c_str(), 0, ZIP_FILENAME);
		if(zip == 0)
			return false;

		bool success = true;
		ZIPENTRYW zipEntry;
		if(ZR_OK == GetZipItem(zip, -1, &zipEntry))
		{
			std::vector<std::pair<int,ResourceEntry*> > tempCache;

			int totalItemCount = zipEntry.index;
			int buffer = 0;
			// Get the size of every item.
			for(int i = 0; i < totalItemCount; ++i)
			{
				GetZipItem(zip,i,&zipEntry);
				if(!(zipEntry.attr & FILE_ATTRIBUTE_DIRECTORY)) // ignore every folder
				{
					ResourceEntry* entry = new ResourceEntry();
					entry->buffer += buffer;
					entry->length = zipEntry.unc_size;

					cache.insert(ResourceCache::value_type(zipEntry.name,entry));
					tempCache.push_back(std::make_pair(i,entry));

					buffer += zipEntry.unc_size;
				}
			}

			BYTE* memBlock = new BYTE[buffer]; // buffer is the size of the zip file.

			// Uncompress every item.
			totalItemCount = tempCache.size();
			for(int i = 0; i < totalItemCount; ++i)
			{
				ResourceEntry* re = tempCache.at(i).second;
				re->buffer = memBlock + (int)re->buffer;

				UnzipItem(zip, tempCache.at(i).first,
					re->buffer, re->length, ZIP_MEMORY);
			}

			tempCache.at(0).second->unity = true;
		} else {
			success = false;
		}

		CloseZip(zip);
		return success;
	}
}