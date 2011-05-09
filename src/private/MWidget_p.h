#pragma once
namespace MetalBone
{
	// ========== WindowExtras ==========
	enum WidgetState
	{
		MWS_Polished    = 0x1,
		MWS_Hidden      = 0x2,
		MWS_CSSOpaqueBG = 0x4,
		MWS_Visible     = 0x8,
		MWS_UnderMouse  = 0x10,
		MWS_Focused     = 0x20,
		MWS_Pressed     = 0x40
	};

	typedef std::tr1::unordered_map<MWidget*,MRect>   DrawRectHash;
	typedef std::tr1::unordered_map<MWidget*,MRegion> DrawRegionHash;
	typedef std::tr1::unordered_map<MWidget*,bool>    DirtyChildrenHash;

	struct WindowExtras
	{
		inline WindowExtras();
		inline ~WindowExtras();

		// If m_wndHandle is a layered window, then we create a dummy window.
		// We send WM_PAINT messages to that window.
		HWND m_wndHandle;
		HWND m_dummyHandle;
		ID2D1HwndRenderTarget* m_renderTarget;

		ID2D1RenderTarget* m_rtHook;

		bool bTrackingMouse;
		MWidget* widgetUnderMouse;
		MWidget* focusedWidget;
		int lastMouseX;
		int lastMouseY;

		std::wstring      m_windowTitle;
		DrawRectHash      updateWidgets;
		DrawRegionHash    passiveUpdateWidgets;
		DirtyChildrenHash childUpdatedHash;

		inline void clearUpdateQueue();
		inline void addToRepaintMap(MWidget* w,int left,int top,int right,int bottom);
	};

	inline WindowExtras::WindowExtras():
		m_wndHandle(NULL), m_dummyHandle(NULL),
		m_renderTarget(0), m_rtHook(0), bTrackingMouse(false),
		widgetUnderMouse(0), focusedWidget(0),
		lastMouseX(0), lastMouseY(0){}
	inline WindowExtras::~WindowExtras()
	{
		SafeRelease(m_renderTarget);
		if(m_dummyHandle != NULL)
			::DestroyWindow(m_dummyHandle);
		::DestroyWindow(m_wndHandle);
	}
	inline void WindowExtras::clearUpdateQueue()
	{
		updateWidgets.clear();
		passiveUpdateWidgets.clear();
		childUpdatedHash.clear();
	}
	inline void WindowExtras::addToRepaintMap(MWidget* w, int left, int top, int right, int bottom)
	{
		DrawRectHash::iterator iter = updateWidgets.find(w);
		if(iter != updateWidgets.end()) {
			MRect& updateRect  = iter->second;
			if(updateRect.left   > left  ) updateRect.left   = left;
			if(updateRect.top    > top   ) updateRect.top    = top;
			if(updateRect.right  < right ) updateRect.right  = right;
			if(updateRect.bottom < bottom) updateRect.bottom = bottom;
		} else {
			MRect& updateRect  = updateWidgets[w];
			updateRect.left   = left;
			updateRect.right  = right;
			updateRect.top    = top;
			updateRect.bottom = bottom;
		}
	}

	extern void generateStyleFlags(unsigned int flags, DWORD* winStyleOut, DWORD* winExStyleOut);
}
