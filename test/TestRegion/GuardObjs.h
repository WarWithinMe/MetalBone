// GuardObjs.h
//////////////////////////////////////////////////////////////////////
#include "GObjBase.h"

namespace GObj
{
// Specialized tempate classes, for specific guard types.

#if ((defined _WINDOWS_) && (!defined GOBJ_WIN32))
#	define GOBJ_WIN32

	// Incapsulates Win32 handles, that must be closed with CloseHandle
	struct GBaseH_HandleClose :public GBaseH_Core<HANDLE> {
		void Destroy() { VERIFY(CloseHandle(m_Value)); }
	};
	// Set of standard Win32 handles
	struct GBaseH_HandleCloseStd :public GBaseH_HandleClose, public GBaseH_Null<HANDLE> {
	};
	// Set of Win32 file handles
	struct GBaseH_HandleCloseFile :public GBaseH_HandleClose, public GBaseH_Minus1<HANDLE> {
	};
	// Applicable for FindFirstFile
	struct GBaseH_HandleFindClose :public GBaseH_Core<HANDLE>, public GBaseH_Minus1<HANDLE> {
		void Destroy() { VERIFY(FindClose(m_Value)); }
	};

	typedef GObj_T<GBaseH_HandleCloseFile>	HFile_G;
	typedef GObj_T<GBaseH_HandleFindClose>	HFileFind_G;
	typedef GObj_T<GBaseH_HandleCloseStd>	Handle_G;

	// HGlobal
	struct GBaseH_HGlobal : public GBaseH_CoreNull<HGLOBAL> {
		void Destroy() { VERIFY(!GlobalFree(m_Value)); }
	};
	typedef GObj_T<GBaseH_HGlobal>		HGlobal_G;

	// Virtual memory
	struct GBaseH_VMem : public GObj::GBaseH_CoreNull<PBYTE> {
		void Destroy() { VERIFY(VirtualFree(m_Value, 0, MEM_RELEASE)); }
	};
	typedef GObj_T<GBaseH_VMem> VMem_G;

	// GDI objects.
	struct GBaseH_GdiObj : public GBaseH_CoreNull<HGDIOBJ> {
		void Destroy() { VERIFY(DeleteObject(m_Value)); }
	};
	typedef GObj_T<GBaseH_GdiObj>			HGdiObj_G;

	// File mapping
	struct GBaseH_UnmapFile : public GBaseH_CoreNull<PBYTE> {
		void Destroy() { VERIFY(UnmapViewOfFile(m_Value)); }
	};
	typedef GObj_T<GBaseH_UnmapFile>		HFileMapping_G;

	// Module loaded via LoadLibrary
	struct GBaseH_Module : public GBaseH_CoreNull<HMODULE> {
		void Destroy() { VERIFY(FreeLibrary(m_Value)); }
	};
	typedef GObj_T<GBaseH_Module>			HModule_G;

	// Window handle
	struct GBaseH_DestroyWindow :public GBaseH_CoreNull<HWND> {
		void Destroy() { VERIFY(DestroyWindow(m_Value)); }
	};
	typedef GObj_T<GBaseH_DestroyWindow> HWnd_G;

	// BSTRs
	struct GBaseH_Bstr : public GBaseH_CoreNull<BSTR> {
		void Destroy() { SysFreeString(m_Value); }
	};
	typedef GRef_T<GBaseH_Bstr>				Bstr_G;

	// Some types of HDCs
	typedef GBaseH_CoreNull<HDC> GBaseH_Hdc;

	struct GBaseH_HdcDelete :public GBaseH_Hdc {
		void Destroy() { VERIFY(DeleteDC(m_Value)); }
	};
	typedef GObj_T<GBaseH_HdcDelete>		Hdc_G;

	struct GBaseH_HdcRelease :public GBaseH_Hdc {
		HWND m_hWnd; // extra member added.
		void Destroy() { ReleaseDC(m_hWnd, m_Value); }
	};
	class HWinDc_G :public GObj_T<GBaseH_HdcRelease> {
		INHERIT_GUARD_OBJ_BASE(HWinDc_G, GObj_T<GBaseH_HdcRelease>, HDC)
		HWinDc_G(HWND hWnd)
		{
			m_Object.m_hWnd = hWnd;
		}
	};

	struct GBaseH_HdcEndPaint :public GBaseH_Hdc {
		HWND m_hWnd;
		PAINTSTRUCT m_PS;
		void Destroy() { VERIFY(EndPaint(m_hWnd, &m_PS)); }
	};
	class HPaintDc_G :public GObj_T<GBaseH_HdcEndPaint> {
		INHERIT_GUARD_OBJ_BASE(HPaintDc_G, GObj_T<GBaseH_HdcEndPaint>, HDC)
		HPaintDc_G(HWND hWnd)
		{
			m_Object.m_hWnd = hWnd;
		}
		bool Begin()
		{
			DoDestroy();
			m_Object.m_Value = BeginPaint(m_Object.m_hWnd, &m_Object.m_PS);
			return IsValid();
		}
	};

	// Selecting an HGDIOBJ into an HDC.
	struct GBaseH_HdcSelect : public GBaseH_CoreNull<HGDIOBJ> {
		HDC m_hDC;
		void Destroy() { VERIFY(SelectObject(m_hDC, m_Value)); }
	};
	class HSelectDc_G : public GObj_T<GBaseH_HdcSelect>
	{
		INHERIT_GUARD_OBJ_BASE(HSelectDc_G, GObj_T<GBaseH_HdcSelect>, HGDIOBJ)
		HSelectDc_G(HDC hDC)
		{
			m_Object.m_hDC = hDC;
		}
	};


#endif // GOBJ_WIN32

#if (((defined _WINSOCK2API_) || (defined _WINSOCKAPI_)) && (!defined GOBJ_SOCKET))
#	define GOBJ_SOCKET

	struct GBaseH_Socket : public GBaseH_CoreMinus1<SOCKET> {
		void Destroy() { VERIFY(!closesocket(m_Value)); }
	};
	typedef GObj_T<GBaseH_Socket>			Socket_G;

#endif // GOBJ_SOCKET

#if ((defined __WINCRYPT_H__) && (!defined GOBJ_WCRYPT))
#	define GOBJ_WCRYPT

	struct GBaseH_CryptHash : public GBaseH_CoreNull<HCRYPTHASH> {
		void Destroy() { VERIFY(CryptDestroyHash(m_Value)); }
	};
	struct GBaseH_CryptKey : public GBaseH_CoreNull<HCRYPTKEY> {
		void Destroy() { VERIFY(CryptDestroyKey(m_Value)); }
	};
	typedef GObj_T<GBaseH_CryptHash>		HCryptHash_G;
	typedef GObj_T<GBaseH_CryptKey>			HCryptKey_G;

#endif // GOBJ_WCRYPT

#if ((defined _INC_MMSYSTEM) && (!defined GOBJ_MMSYSTEM))
#	define GOBJ_MMSYSTEM

	struct GBaseH_HMMIO : public GObj::GBaseH_CoreNull<HMMIO> {
		void Destroy() { VERIFY(!mmioClose(m_Value, 0)); }
	};
	typedef GObj::GObj_T<GBaseH_HMMIO> HmmIo_G;

	struct GBaseH_HWAVEOUT : public GObj::GBaseH_CoreNull<HWAVEOUT> {
		void Destroy() { VERIFY(!waveOutClose(m_Value)); }
	};
	typedef GObj::GObj_T<GBaseH_HWAVEOUT> HWaveOut_G;

	struct GBaseH_HWAVEIN : public GObj::GBaseH_CoreNull<HWAVEIN> {
		void Destroy() { VERIFY(!waveInClose(m_Value)); }
	};
	typedef GObj::GObj_T<GBaseH_HWAVEIN> HWaveIn_G;

#endif // GOBJ_MMSYSTEM

#if ((defined _INC_ACM) && (!defined GOBJ_ACM))
#	define GOBJ_ACM

	struct GBaseH_HACMSTREAM : public GBaseH_CoreNull<HACMSTREAM> {
		void Destroy() { VERIFY(!acmStreamClose(m_Value, 0)); }
	};
	typedef GObj_T<GBaseH_HACMSTREAM> HacmStream_G;

#endif // GOBJ_ACM

#if ((defined _WININET_) && (!defined GOBJ_WININET))
#	define GOBJ_WININET

	struct GBaseH_HINet : public GBaseH_CoreNull<HINTERNET> {
		void Destroy() { VERIFY(InternetCloseHandle(m_Value)); }
	};
	typedef GObj_T<GBaseH_HINet>		HINet_G;

#endif // GOBJ_WININET

}; // namespace GObj

