#include "MBGlobal.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <WinBase.h>
#include <sstream>
#include <string>

#ifdef MB_DEBUG
inline void InlDebugBreak() { __asm { int 3 }; }
void mb_debug(const wchar_t* what)
{
	OutputDebugStringW(what);
}

double get_time()
{
	LARGE_INTEGER t, f;
	QueryPerformanceCounter(&t);
	QueryPerformanceFrequency(&f);
	return double(t.QuadPart)/double(f.QuadPart);
}

void mb_warning(bool cond, const wchar_t* what)
{
	if(!cond)
		return;
	std::wstringstream ss;
	ss << L"[Warning] ";
	ss << what;
	OutputDebugStringW(ss.str().c_str());
}

void mb_assert(const char* assertion, const char* file, unsigned int line)
{
	std::stringstream ss;
	ss << "\nFailed assertion \"";
	ss << assertion;
	ss << "\".\nFile: ";
	ss << file;
	ss << ". Line: ";
	ss << line;
	OutputDebugStringA(ss.str().c_str());
#ifdef MB_DEBUGBREAK_INSTEADOF_ABORT
	InlDebugBreak();
#else
	abort();
#endif
}

void mb_assert_xw(const wchar_t* what, const wchar_t* where, const char* file, unsigned int line)
{
	std::wstringstream ss;
	ss << L"\nFailed assertion \"";
	ss << what;
	ss << L"\" in \"";
	ss << where;
	ss << L"\".\nFile: ";
	ss << file;
	ss << L". Line: ";
	ss << line;
	OutputDebugStringW(ss.str().c_str());
#ifdef MB_DEBUGBREAK_INSTEADOF_ABORT
	InlDebugBreak();
#else
	abort();
#endif
}

void ensureInMainThread()
{
	static DWORD mainThreadId = GetCurrentThreadId();
	M_ASSERT_X(mainThreadId == GetCurrentThreadId(), "Widgets must created in the main thread.", "Widget constructor");
}

void dumpMemory(LPCVOID address, SIZE_T size)
{
	BYTE   byte;
	SIZE_T byteindex;
	SIZE_T bytesdone;
	SIZE_T dumplen;
	WCHAR  formatbuf [4];
	WCHAR  hexdump [58] = {0};
	SIZE_T hexindex;
	WCHAR  unidump [18] = {0};
	SIZE_T uniindex;

	// Each line of output is 16 bytes.
	if ((size % 16) == 0)
		dumplen = size;// No padding needed.
	else
		dumplen = size + (16 - (size % 16));// We'll need to pad the last line out to 16 bytes.

	// For each word of data, get both the Unicode equivalent and the hex
	// representation.
	bytesdone = 0;
	for (byteindex = 0; byteindex < dumplen; byteindex++) {
		hexindex = 3 * ((byteindex % 16) + ((byteindex % 16) / 4));   // 3 characters per byte, plus a 3-character space after every 4 bytes.
//		uniindex = ((byteindex / 2) % 8) + ((byteindex / 2) % 8) / 8; // 1 character every other byte, plus a 1-character space after every 8 bytes.
		uniindex = (byteindex % 16) + (byteindex % 16) / 8; // 1 character every other byte, plus a 1-character space after every 8 bytes.
		if (byteindex < size) {
			byte = ((PBYTE)address)[byteindex];
			_snwprintf_s(formatbuf, 4, _TRUNCATE, L"%.2X ", byte);
			formatbuf[4 - 1] = '\0';
			wcsncpy_s(hexdump + hexindex, 58 - hexindex, formatbuf, 4);

			if (isgraph(byte))
				unidump[uniindex] = (WCHAR)byte;
			else
				unidump[uniindex] = L'.';
//			if (((byteindex % 2) == 0) && ((byteindex + 1) < dumplen)) {
//				// On every even byte, print one character.
//				word = ((PWORD)address)[byteindex / 2];
//				if ((word == 0x0000) || (word == 0x0020))
//					unidump[uniindex] = L'.';
//				else
//					unidump[uniindex] = word;
//			}
		} else {
			// Add padding to fill out the last line to 16 bytes.
			wcsncpy_s(hexdump + hexindex, 58 - hexindex, L"   ", 4);
			unidump[uniindex] = L'.';
		}
		bytesdone++;
		if ((bytesdone % 16) == 0) {
			// Print one line of data for every 16 bytes. Include the
			// ASCII dump and the hex dump side-by-side.
//			report(L"    %s    %s\n", hexdump, unidump);
			std::wstringstream stream;
			stream << L"    ";
			stream << hexdump;
			stream << L"    ";
			stream << unidump;
			stream << L"\n";
			OutputDebugStringW(stream.str().c_str());
		} else {
			if ((bytesdone % 8) == 0) {
				// Add a spacer in the ASCII dump after every 8 bytes.
				unidump[uniindex + 1] = L' ';
			}
			if ((bytesdone % 4) == 0) {
				// Add a spacer in the hex dump after every 4 bytes.
				wcsncpy_s(hexdump + hexindex + 3, 58 - hexindex - 3, L"   ", 4);
			}
		}
	}
}
#endif
