#include "MWidget.h"
#include "MApplication.h"
#include "MStyleSheet.h"
#include "MResource.h"
#include "MEvent.h"
#include "MRegion.h"
#include "MToolTip.h"

#include <GdiPlus.h>
#include <list>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <tchar.h>
#include <WinError.h>
#include <WindowsX.h>
#include <ObjBase.h>
#include <dwmapi.h>
#include <limits>

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
	typedef std::tr1::unordered_map<MWidget*,RECT>    DrawRectHash;
	typedef std::tr1::unordered_map<MWidget*,MRegion> DrawRegionHash;
	typedef std::tr1::unordered_map<MWidget*,bool>    DirtyChildrenHash;
	struct WindowExtras {
		inline WindowExtras();
		~WindowExtras();

		// If m_wndHandle is a layered window, then we create a dummy window.
		// We send WM_PAINT messages to that window.
		// Remark: Can we receive the message if the dummy window is hidden?
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
	WindowExtras::~WindowExtras()
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
			RECT& updateRect  = iter->second;
			if(updateRect.left   > left  ) updateRect.left   = left;
			if(updateRect.top    > top   ) updateRect.top    = top;
			if(updateRect.right  < right ) updateRect.right  = right;
			if(updateRect.bottom < bottom) updateRect.bottom = bottom;
		} else {
			RECT& updateRect  = updateWidgets[w];
			updateRect.left   = left;
			updateRect.right  = right;
			updateRect.top    = top;
			updateRect.bottom = bottom;
		}
	}



	extern wchar_t gMWidgetClassName[] = L"MetalBone Widget";
	extern MCursor gArrowCursor = MCursor(MCursor::ArrowCursor);

	// ========== MApplicationData ==========
	struct MApplicationData
	{
		inline MApplicationData(bool hwAccelerated);
		// Window procedure
		static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);

		template<bool matchDummy>
		MWidget* findWidgetByHandle(HWND handle) const;
		static inline void removeTLW(MWidget* w);
		static inline void insertTLW(MWidget* w);

		std::set<MWidget*>  topLevelWindows;
		bool                quitOnLastWindowClosed;
		bool                hardwareAccelerated;
		HINSTANCE           appHandle;
		MStyleSheetStyle    ssstyle;

		ID2D1Factory*       d2d1Factory;
		IDWriteFactory*     dwriteFactory;
		IWICImagingFactory* wicFactory;

		ULONG_PTR gdiPlusToken;

		static MApplication::WinProc customWndProc;
		static MApplicationData*     instance;
		static void setFocusWidget(MWidget*);
	};



	// ========== MApplication Impl ==========
	MApplication* MApplication::s_instance = 0;
	MApplication::MApplication(bool hwAccelerated):
	mImpl(new MApplicationData(hwAccelerated))
	{
		ENSURE_IN_MAIN_THREAD;
		M_ASSERT_X(s_instance == 0, "Only one MApplication instance allowed",
			"MApplication::MApplication()");

		s_instance = this;
		mImpl->appHandle = GetModuleHandleW(NULL);

		// DPI
		HDC dc     = ::GetDC(0);
		windowsDPI = ::GetDeviceCaps(dc, LOGPIXELSY);
		::ReleaseDC(0,dc);

		// Register Window Class 
		WNDCLASSW wc;
		setupRegisterClass(wc);
		RegisterClassW(&wc);

		// COM
		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		M_ASSERT_X(SUCCEEDED(hr), "Cannot initialize COM!", "MApplicationData()");

		// D2D
#ifdef MB_DEBUG_D2D
		D2D1_FACTORY_OPTIONS opts;
		opts.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, opts, &(mImpl->d2d1Factory));
#else
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &(mImpl->d2d1Factory));
#endif
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create D2D1Factory. FATAL!", "MApplication()");

		// DWrite
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory),
			reinterpret_cast<IUnknown**>(&mImpl->dwriteFactory));

		// WIC
		hr = CoCreateInstance(CLSID_WICImagingFactory,NULL,
			CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&(mImpl->wicFactory)));
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create WIC Component. FATAL!", "MApplication()");

		// GDI+
		Gdiplus::GdiplusStartupInput input;
		Gdiplus::GdiplusStartup(&mImpl->gdiPlusToken,&input,0);
	}

	MApplication::~MApplication()
	{
		Gdiplus::GdiplusShutdown(mImpl->gdiPlusToken);
		SafeRelease(mImpl->wicFactory);
		SafeRelease(mImpl->d2d1Factory);
		SafeRelease(mImpl->dwriteFactory);
		delete mImpl;
		MApplicationData::instance = 0;
		s_instance = 0;
		CoUninitialize();
	}

	const std::set<MWidget*>& MApplication::topLevelWindows() const { return mImpl->topLevelWindows; }
	HINSTANCE            MApplication::getAppHandle()         const { return mImpl->appHandle;       }
	MStyleSheetStyle*    MApplication::getStyleSheet()              { return &(mImpl->ssstyle);      }
	ID2D1Factory*        MApplication::getD2D1Factory()             { return mImpl->d2d1Factory;     }
	IDWriteFactory*      MApplication::getDWriteFactory()           { return mImpl->dwriteFactory;   }
	IWICImagingFactory*  MApplication::getWICImagingFactory()       { return mImpl->wicFactory;      }
	bool  MApplication::isHardwareAccerated()                 const { return mImpl->hardwareAccelerated; }
	void  MApplication::setStyleSheet(const std::wstring& css)      { mImpl->ssstyle.setAppSS(css);  }
	void  MApplication::setCustomWindowProc(WinProc proc)           { mImpl->customWndProc = proc;   }

	void MApplication::setupRegisterClass(WNDCLASSW& wc)
	{
		wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = MApplicationData::windowProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = mImpl->appHandle;
		wc.hIcon         = 0;
		wc.hCursor       = gArrowCursor.getHandle();
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszMenuName  = 0;
		wc.lpszClassName = gMWidgetClassName;
	}

	int MApplication::exec()
	{
		MSG msg;
		int result;

		while( (result = GetMessageW(&msg, 0, 0, 0)) != 0)
		{
			if(result == -1) break; // GetMessage Error

			if(msg.message == WM_HOTKEY)
			{
				std::vector<MShortCut*> scs = MShortCut::getMachedShortCuts(
					LOWORD(msg.lParam),HIWORD(msg.lParam));
				for(int i = scs.size() - 1; i >= 0; --i)
					scs.at(i)->invoked();

			} else {
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		aboutToQuit();
		return result;
	}



	// ========== MApplicationData Impl ==========
	inline MApplicationData::MApplicationData(bool hwAccelerated):
		quitOnLastWindowClosed(true),
		hardwareAccelerated(hwAccelerated)
		{ instance = this; }
	inline void MApplicationData::removeTLW(MWidget* w)
		{ if(instance) instance->topLevelWindows.erase(w); }
	inline void MApplicationData::insertTLW(MWidget* w)
		{ if(instance) instance->topLevelWindows.insert(w); }

	MApplication::WinProc MApplicationData::customWndProc = 0;
	MApplicationData* MApplicationData::instance = 0;
	template<bool matchDummy>
	MWidget* MApplicationData::findWidgetByHandle(HWND handle) const
	{
		std::set<MWidget*>::const_iterator it    = topLevelWindows.begin();
		std::set<MWidget*>::const_iterator itEnd = topLevelWindows.end();
		while(it != itEnd)
		{
			MWidget* w = *it;
			if(matchDummy)
			{
				if(w->windowHandle() == handle || w->m_windowExtras->m_dummyHandle == handle)
					return w;
			} else
			{
				if(w->windowHandle() == handle)
					return w;
			}
			
			++it;
		}
		return 0;
	}

	void MApplicationData::setFocusWidget(MWidget* w)
	{
		WindowExtras* xtr = w->windowWidget()->m_windowExtras;
		MWidget* oldFocus = xtr->focusedWidget;
		if(oldFocus == w)
			return;
		xtr->focusedWidget = w;

		if(oldFocus != 0) {
			oldFocus->setWidgetState(MWS_Focused,false);
			instance->ssstyle.updateWidgetAppearance(oldFocus);
		}

		if(w != 0) {
			w->setWidgetState(MWS_Focused,true);
			instance->ssstyle.updateWidgetAppearance(w);
			w->focusEvent();
		}
	}

	void generateStyleFlags(unsigned int, DWORD*, DWORD*);
	unsigned int mapKeyState()
	{
		unsigned int result = NoModifier;
		if(::GetKeyState(VK_CONTROL)< 0) result |= CtrlModifier;
		if(::GetKeyState(VK_SHIFT)  < 0) result |= ShiftModifier;
		if(::GetKeyState(VK_MENU)   < 0) result |= AltModifier;
		if(::GetKeyState(VK_LWIN)   < 0 ||
		   ::GetKeyState(VK_RWIN)   < 0) result |= WinModifier;
		return result;
	}
	
	LRESULT MApplicationData::windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if(!instance)
			return DefWindowProcW(hwnd,msg,wparam,lparam);
		if(customWndProc)
		{
			LRESULT result;
			if(customWndProc(hwnd,msg,wparam,lparam,&result)) return result;
		}

		switch(msg)
		{
			case WM_DESTROY:
				// Check if the user wants to quit when the last window is closed.
				if(instance->quitOnLastWindowClosed && instance->topLevelWindows.size() == 0)
					mApp->exit(0);
				return 0;
		}
		
		MWidget* window;
		if(msg == WM_PAINT)
		{
			window = instance->findWidgetByHandle<true>(hwnd);
			if(window != 0)
			{
				RECT updateRect;
				GetUpdateRect(hwnd,&updateRect,false);
				if(updateRect.right > 1 && updateRect.bottom > 1)
				{
					RECT clientRect;
					GetClientRect(hwnd,&clientRect);
					// If we have to update the whole window,
					// we should ignore the update request make by the child widgets.
					if(memcmp(&updateRect,&clientRect,sizeof(updateRect)) == 0)
						window->m_windowExtras->clearUpdateQueue();

					window->m_windowExtras->addToRepaintMap(window, 0,0,window->width,window->height);

				}
				window->drawWindow();
				ValidateRect(hwnd,0);
				return 0;
			}
			return DefWindowProcW(hwnd,msg,wparam,lparam);
		} else if(msg == WM_MOVE)
		{
			window = instance->findWidgetByHandle<true>(hwnd);
			long newX = LOWORD(lparam);
			long newY = HIWORD(lparam);
			if(window == 0 || (window->x == newX && window->y == newY))
				return 0;
			window->x = newX;
			window->y = newY;
			if(window->m_windowFlags & WF_AllowTransparency)
			{
				BLENDFUNCTION blend = {};
				blend.AlphaFormat = AC_SRC_ALPHA;
				blend.SourceConstantAlpha = 255;
				POINT windowPos = {window->x, window->y};

				UPDATELAYEREDWINDOWINFO info = {};
				info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
				info.dwFlags  = ULW_ALPHA;
				info.pblend   = &blend;
				info.pptDst   = &windowPos;
				::UpdateLayeredWindowIndirect(window->m_windowExtras->m_wndHandle,&info);
			}
		}

		window = instance->findWidgetByHandle<false>(hwnd);
		if(window == 0)
			return DefWindowProcW(hwnd,msg,wparam,lparam);
		WindowExtras* xtr = window->m_windowExtras;

		switch(msg) {

		case WM_SETCURSOR:
			return (LOWORD(lparam) == HTCLIENT) ? TRUE : DefWindowProcW(hwnd,msg,wparam,lparam);
		case WM_MOUSEHOVER:
			{
				if(xtr->widgetUnderMouse != 0)
				{
					MToolTip* tip = xtr->widgetUnderMouse->getToolTip();
					if(tip)
					{
						RECT rect;
						GetWindowRect(hwnd,&rect);
						tip->show(rect.left + xtr->lastMouseX + 20,
							rect.top + xtr->lastMouseY + 20);
					}
				}
				xtr->bTrackingMouse = false;
			}
			break;
		case WM_MOUSEMOVE:
			{
				int xpos = GET_X_LPARAM(lparam);
				int ypos = GET_Y_LPARAM(lparam);

				if(xpos == xtr->lastMouseX && ypos == xtr->lastMouseY)
					return 0;

				xtr->lastMouseX = xpos;
				xtr->lastMouseY = ypos;

				if(!window->m_windowExtras->bTrackingMouse)
				{
					TRACKMOUSEEVENT tme = {0};
					tme.cbSize = sizeof(TRACKMOUSEEVENT);
					tme.dwFlags = TME_HOVER | TME_LEAVE;
					tme.hwndTrack = hwnd;
					tme.dwHoverTime = 400; // Remark: Set this value to a proper one.
					::TrackMouseEvent(&tme);
					window->m_windowExtras->bTrackingMouse = true;
				}

				// We could first check if the mouse is still over the last widget.
				MWidget* cw = 0;
				if(xpos >= 0 && ypos >= 0 && 
					xpos <= window->width && ypos <= window->height)
				{ cw = window->findWidget(xpos, ypos); }

				MWidget* lastWidget = xtr->widgetUnderMouse;
				bool mouseMove = true;
				if(lastWidget != cw)
				{
					if(lastWidget != 0)
					{
						lastWidget->leaveEvent();
						lastWidget->setWidgetState(MWS_Pressed | MWS_UnderMouse, false);
						instance->ssstyle.updateWidgetAppearance(lastWidget);

						MToolTip* tip = lastWidget->getToolTip();
						if(tip) tip->hide();
					} else {
						// The mouse moves into the window, we set it to the default.
						gArrowCursor.show(true);
					}

					if(cw != 0) {
						MEvent enter(false);
						cw->enterEvent(&enter);
						mouseMove = !enter.isAccepted();
						if(cw->testAttributes(WA_Hover))
							cw->setWidgetState(MWS_UnderMouse,true);
						instance->ssstyle.updateWidgetAppearance(cw);
					}
				}
				if(cw != 0) {
					if(cw->focusPolicy() == MoveOverFocus)
						cw->setFocus();

					if(mouseMove && cw->testAttributes(WA_TrackMouseMove))
					{
						RECT rect;
						GetWindowRect(hwnd,&rect);
						MMouseEvent me(xpos, ypos, rect.left + xtr->lastMouseX,
							rect.top + xtr->lastMouseY, NoButton);
						do 
						{
							if(cw->testAttributes(WA_TrackMouseMove))
								cw->mouseMoveEvent(&me);
							me.offsetPos(cw->x,cw->y);
							cw = cw->m_parent;
						} while (cw && !me.isAccepted() &&
								!cw->testAttributes(WA_NoMousePropagation));
					}
				}
				xtr->widgetUnderMouse = cw;

				MToolTip* thisTT = cw == 0 ? 0 : cw->getToolTip();
				if(thisTT && thisTT->isShowing())
				{
					if(thisTT->hidePolicy() == MToolTip::WhenMove)
						thisTT->hide();
					else
					{
						RECT rect;
						GetWindowRect(hwnd,&rect);
						thisTT->show(rect.left + xtr->lastMouseX + 20,
							rect.top + xtr->lastMouseY + 20);
					}
				}
			}
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			{
				// When we press down keys, we need to check the shortcut first.
				std::vector<MShortCut*> scs = MShortCut::getMachedShortCuts(mapKeyState(),wparam);
				MWidget* fw = xtr->focusedWidget;
				if(fw == 0) fw = window;
				MShortCut* invoker = 0;
				for(int i = scs.size() - 1; i>=0; --i) {
					MShortCut* sc = scs.at(i);
					if(sc->getTarget() == fw) {
						invoker = sc;
						break;
					}
				}
				bool accepted = false;
				if(invoker == 0)
				{
					while(fw != 0 && !accepted) {
						if(fw->focusPolicy() != NoFocus)
						{
							unsigned int mod = (wparam > VK_NUMPAD0 && wparam < VK_DIVIDE) ?
									KeypadModifier : NoModifier;
							mod |= mapKeyState();
							MKeyEvent ev(wparam,mod);
							fw->keyPressEvent(&ev);
							accepted = ev.isAccepted();
						}
						fw = fw->m_parent;
					}
				}
				if(!accepted)
				{
					if(invoker == 0) {
						for(int i = scs.size() - 1; i>=0; --i) {
							MShortCut* sc = scs.at(i);
							if(sc->getTarget() == window)
								{ invoker = sc; break; }
						}
					}
					if(invoker == 0) {
						for(int i = scs.size() - 1; i>=0; --i) {
							MShortCut* sc = scs.at(i);
							if(sc->getTarget() == 0)
								{ invoker = sc; break; }
						}
					}
				}
				if(invoker != 0) invoker->invoked();
			}
			if(msg == WM_SYSKEYDOWN)
				return DefWindowProcW(hwnd,msg,wparam,lparam);
			break;
		case WM_KEYUP:
			{
				MWidget* fw = xtr->focusedWidget;
				if(fw == 0) fw = window;
				while(fw != 0)
				{
					if(fw->focusPolicy() != NoFocus)
					{
						// We don't check the modifiers when receive keyup event.
						// But we still want to know if the key is in the keypad.
						KeyboardModifier mod = (wparam > VK_NUMPAD0 && wparam < VK_DIVIDE) ?
							KeypadModifier : NoModifier;
						MKeyEvent ev(wparam,mod);
						fw->keyUpEvent(&ev);
						if(ev.isAccepted())
							break;
					}
					fw = fw->m_parent;
				}
			}
			break;
		case WM_CHAR:
		case WM_SYSCHAR:
			{
				MWidget* fw = xtr->focusedWidget;
				if(fw == 0) fw = window;
				while(fw != 0)
				{
					if(fw->focusPolicy() != NoFocus)
					{
						MCharEvent ev(wparam);
						fw->charEvent(&ev);
						if(ev.isAccepted())
							break;
					}
					fw = fw->m_parent;
				}
				if(msg == WM_SYSCHAR)
					return DefWindowProcW(hwnd,msg,wparam,lparam);
			}
			break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			{
				SetFocus(hwnd);
				int xpos = GET_X_LPARAM(lparam);
				int ypos = GET_Y_LPARAM(lparam);
				MWidget* cw = 0;
				if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
					SendMessage(hwnd,WM_MOUSEMOVE,wparam,lparam);
				
				cw = xtr->widgetUnderMouse;
				if(cw != 0)
				{
					if(cw->focusPolicy() == ClickFocus)
						cw->setFocus();
					MouseButton btn = (msg == WM_LBUTTONDOWN ? LeftButton :
						(msg == WM_RBUTTONDOWN ? RightButton : MiddleButton));
					if(btn == LeftButton)
					{
						// Only change the state if the user press left button
						cw->setWidgetState(MWS_Pressed,true);
						instance->ssstyle.updateWidgetAppearance(cw);
					}
					RECT rect;
					GetWindowRect(hwnd,&rect);
					MWidget* pc = cw;
					MWidget* pp = cw->m_parent;
					while(pp != 0)
					{
						xpos -= pc->x;
						ypos -= pc->y;
						pc = pp; pp = pc->m_parent;
					}
					MMouseEvent me(xpos, ypos, rect.left + xtr->lastMouseX,
						rect.top + xtr->lastMouseY, btn);
					me.ignore();
					do 
					{
						cw->mousePressEvent(&me);
						me.offsetPos(cw->x,cw->y);
						cw = cw->m_parent;
					} while (cw && !me.isAccepted() &&
						cw->testAttributes(WA_NoMousePropagation));

					SetCapture(hwnd);
				}
				MToolTip::hideAll();
			}
			break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
			{
				int xpos = GET_X_LPARAM(lparam);
				int ypos = GET_Y_LPARAM(lparam);
				MWidget* cw = 0;
				if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
					SendMessage(hwnd,WM_MOUSEMOVE,wparam,lparam);

				cw = xtr->widgetUnderMouse;
				if(cw != 0)
				{
					MouseButton btn = (msg == WM_LBUTTONUP ? LeftButton :
						(msg == WM_RBUTTONUP ? RightButton : MiddleButton));
					if(btn == LeftButton) {
						cw->setWidgetState(MWS_Pressed, false);
						instance->ssstyle.updateWidgetAppearance(cw);
					}
					RECT rect;
					GetWindowRect(hwnd,&rect);
					MMouseEvent me(xpos, ypos, rect.left + xtr->lastMouseX,
						rect.top + xtr->lastMouseY, btn);
					me.ignore();
					do 
					{
						cw->mouseReleaseEvent(&me);
						me.offsetPos(cw->x,cw->y);
						cw = cw->m_parent;
					} while (cw && !me.isAccepted() &&
						cw->testAttributes(WA_NoMousePropagation));

					ReleaseCapture();
				}
				MToolTip::hideAll();
			}
			break;
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
			{
				int xpos = GET_X_LPARAM(lparam);
				int ypos = GET_Y_LPARAM(lparam);
				MWidget* cw = 0;
				if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
					SendMessage(hwnd,WM_MOUSEMOVE,wparam,lparam);

				cw = xtr->widgetUnderMouse;
				if(cw != 0)
				{
					MouseButton btn = (msg == WM_LBUTTONDBLCLK ? LeftButton :
						(msg == WM_RBUTTONDBLCLK ? RightButton : MiddleButton));
					if(btn == LeftButton) {
						cw->setWidgetState(MWS_Pressed,true);
						instance->ssstyle.updateWidgetAppearance(cw);
					}
					RECT rect;
					GetWindowRect(hwnd,&rect);
					MMouseEvent me(xpos, ypos, rect.left + xtr->lastMouseX,
						rect.top + xtr->lastMouseY, btn);
					me.ignore();
					do 
					{
						cw->mouseDClickEvent(&me);
						me.offsetPos(cw->x,cw->y);
						cw = cw->m_parent;
					} while (cw && !me.isAccepted() &&
						cw->testAttributes(WA_NoMousePropagation));
				}
			}
			break;
		case WM_MOUSELEAVE:
			if(window->m_windowExtras->bTrackingMouse)
				SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(-1,-1));
			window->m_windowExtras->bTrackingMouse = false;
			break;
		case WM_CLOSE:
			{
				MEvent closeEvent;
				window->closeEvent(&closeEvent);
				if(closeEvent.isAccepted()) {
					if(window->testAttributes(WA_DeleteOnClose))
						delete window;
					else
						window->destroyWnd();
				}
			}
			break;
		case WM_SIZE:
			{
				if(wparam == SIZE_MINIMIZED)
				{
					window->m_windowState = WindowMinimized;
					return 0;
				} else if(wparam == SIZE_MAXIMIZED)
					window->m_windowState = WindowMaximized;

				if(window->width == LOWORD(lparam) && window->height == HIWORD(lparam))
					return 0;

				MResizeEvent ev(window->width,window->height,LOWORD(lparam),HIWORD(lparam));
				window->width  = LOWORD(lparam);
				window->height = HIWORD(lparam);
				window->resizeEvent(&ev);
				xtr->m_renderTarget->Resize(D2D1::SizeU(LOWORD(lparam),HIWORD(lparam)));
			}
			break;
		case WM_NCHITTEST:
			{
				LRESULT result;
				if(!::DwmDefWindowProc(hwnd,msg,wparam,lparam,&result))
					result = DefWindowProcW(hwnd,msg,wparam,lparam);
				switch(result)
				{
					case HTTOP:
					case HTBOTTOM:
						if(window->maxHeight == window->minHeight)
							return HTBORDER;
					case HTLEFT:
					case HTRIGHT:
						if(window->maxWidth == window->minWidth)
							return HTBORDER;
					case HTBOTTOMLEFT:
					case HTBOTTOMRIGHT:
					case HTTOPLEFT:
					case HTTOPRIGHT:
						if(window->maxHeight == window->minHeight ||
							window->maxWidth == window->minWidth)
							return HTBORDER;
					default: return result;
				}
			}
			
		case WM_GETMINMAXINFO:
			{
				DWORD winStyle    = 0;
				DWORD winExStyle  = 0;
				// We don't set parent of layered window.
				RECT rect = {0,0,window->width,window->height};
				generateStyleFlags(window->m_windowFlags,&winStyle,&winExStyle);
				AdjustWindowRectEx(&rect,winStyle,false,winExStyle);
				int dx = rect.right - rect.left - window->width;
				int dy = rect.bottom - rect.top - window->height;

				MINMAXINFO* info = (MINMAXINFO*)lparam;
				info->ptMaxSize.x = window->maxWidth;
				info->ptMaxSize.y = window->maxHeight;
				long sum = window->maxWidth + dx;
				info->ptMaxTrackSize.x = sum > 0 ? sum : window->maxWidth;
				sum = window->maxHeight + dy;
				info->ptMaxTrackSize.y = sum > 0 ? sum : window->maxHeight;
				info->ptMinTrackSize.x = window->minWidth  + dx;
				info->ptMinTrackSize.y = window->minHeight + dy;
				break;
			}
		case WM_SHOWWINDOW:
			{
				if(lparam == 0)
					wparam == TRUE ? window->show() : window->hide();
				return DefWindowProcW(hwnd,msg,wparam,lparam);
			}
		case WM_SYSCOMMAND:
			// Only intercept this message when the window is layered window.
			if(window->m_windowFlags & WF_AllowTransparency)
			{
				switch(wparam)
				{
				case SC_MINIMIZE:
					window->showMinimized();
					return 0;
				case SC_MAXIMIZE:
					window->showMaximized();
					return 0;
				case SC_RESTORE:
					window->show();
					return 0;
				}
			}
			return DefWindowProcW(hwnd,msg,wparam,lparam);
		default: return DefWindowProcW(hwnd,msg,wparam,lparam);
		}

		return 0;
	}







	// ========== MWidget ==========
	MWidget::MWidget(MWidget* parent)
		: m_parent(parent),
		m_windowExtras(0),
		x(200),y(200),
		width(640),height(480),
		minWidth(0),minHeight(0),
		maxWidth(LONG_MAX),maxHeight(LONG_MAX),
		m_attributes(WA_AutoBG | WA_NonChildOverlap),
		m_windowFlags(WF_Widget),
		m_windowState(WindowNoState),
		m_widgetState(MWS_Hidden),lastPseudo(0),
		fp(NoFocus),m_toolTip(0),m_cursor(0)
	{
		M_ASSERT_X(mApp!=0, "MApplication must be created first.", "MWidget constructor");
		ENSURE_IN_MAIN_THREAD;

		m_topLevelParent = (parent != 0) ? parent->m_topLevelParent : this;
	}

	MWidget* MWidget::findWidget(int& px, int& py, bool ignoreMouse)
	{
		MWidgetList::reverse_iterator it = m_children.rbegin();
		MWidgetList::reverse_iterator itEnd = m_children.rend();
		MWidget* result = this;
		while(it != itEnd)
		{
			MWidget* child = *(it++);
			if(child->isHidden() || (ignoreMouse && child->testAttributes(WA_MouseThrough)))
				continue;

			if(px >= child->x && py >= child->y &&
				px <= child->x + child->width && py <= child->y + child->height)
			{
				px -= child->x;
				py -= child->y;
				result = child->findWidget(px,py,ignoreMouse);
				break;
			}
		}
		return result;
	}
	void MWidget::setCursor(MCursor* c)
		{ delete m_cursor; m_cursor = c; }
	void MWidget::setFocus()
		{ MApplicationData::setFocusWidget(this); }
	void MWidget::setToolTip(MToolTip* tip)
		{ delete m_toolTip; m_toolTip = tip; }
	void MWidget::setToolTip(const std::wstring& tip)
	{ 
		if(m_toolTip)
			m_toolTip->setText(tip);
		else
			m_toolTip = new MToolTip(tip);
	}

	unsigned int MWidget::getWidgetPseudo(bool mark)
	{
		unsigned int pseudo = CSS::PC_Default;
		// Only set pseudo to hover if it's not pressed.
		if(testWidgetState(MWS_Pressed))
			pseudo |= CSS::PC_Pressed;
		else if(testWidgetState(MWS_UnderMouse))
			pseudo |= CSS::PC_Hover;

		if(testWidgetState(MWS_Focused))
			pseudo |= CSS::PC_Focus;

		if(mark) lastPseudo = pseudo;
		return pseudo;
	}

	MWidget::~MWidget()
	{
		if(m_topLevelParent->m_windowExtras)
		{
			if(m_topLevelParent->m_windowExtras->focusedWidget == this)
				m_topLevelParent->m_windowExtras->focusedWidget = 0;
		}

		// Delete children
		MWidgetList::iterator iter    = m_children.begin();
		MWidgetList::iterator iterEnd = m_children.end();
		while(iter != iterEnd) {
			delete (*iter);
			iter = m_children.begin();
		}

		// After setting the parent, the m_topLevelParent will change.
		if(m_parent != 0)
			setParent(0);

		delete m_toolTip;
		delete m_cursor;

		if(mApp)
		{
			mApp->getStyleSheet()->setWidgetSS(this,std::wstring());
			mApp->getStyleSheet()->removeCache(this);
		}

		if(m_windowExtras)
		{
			MApplicationData::removeTLW(this);
			delete m_windowExtras;
		}
	}

	bool MWidget::hasWindow() const
	{
		if(m_windowExtras != 0 && m_windowExtras->m_wndHandle != NULL)
			return true;

		return false;
	}

	const std::wstring& MWidget::windowTitle() const
	{
		if(m_windowExtras == 0)
			return std::wstring();
		return m_windowExtras->m_windowTitle;
	}

	void MWidget::setWindowTitle(const std::wstring& t)
	{
		if(m_windowExtras == 0)
			return;
		m_windowExtras->m_windowTitle = t;
		if(m_windowExtras->m_wndHandle != NULL)
			::SetWindowTextW(m_windowExtras->m_wndHandle,t.c_str());
	}

	HWND MWidget::windowHandle() const 
	{
		if(this == m_topLevelParent) {
			return m_windowExtras ? m_windowExtras->m_wndHandle : NULL;
		} else {
			return m_topLevelParent->windowHandle();
		}
	}

	ID2D1RenderTarget* MWidget::getRenderTarget() 
	{
		if(this == m_topLevelParent) {
			if(m_windowExtras == 0) return 0;

			return m_windowExtras->m_rtHook ? m_windowExtras->m_rtHook :
				m_windowExtras->m_renderTarget;
		} else {
			return m_topLevelParent->getRenderTarget();
		}
	}

	void MWidget::closeWindow() 
	{
		if(hasWindow())
			::SendMessage(m_windowExtras->m_wndHandle,WM_CLOSE,0,0);
	}

	void generateStyleFlags(unsigned int flags, DWORD* winStyleOut, DWORD* winExStyleOut)
	{
		M_ASSERT(winStyleOut != 0);
		M_ASSERT(winExStyleOut != 0);

		DWORD winStyle = 0;
		DWORD winExStyle = 0;
		bool customized = ((flags >> 8) & 0xFF) != 0;

		if(customized)
		{
			if(flags & WF_HelpButton)
				winExStyle |= WS_EX_CONTEXTHELP;
			else
			{
				if(flags & WF_MaximizeButton)
					winStyle |= (WS_MAXIMIZEBOX | WS_SYSMENU);
				if(flags & WF_MinimizeButton)
					winStyle |= (WS_MINIMIZEBOX | WS_SYSMENU);
			}

			if(flags & WF_TitleBar)
				winStyle |= WS_CAPTION;
			if(flags & WF_CloseButton)
				winStyle |= WS_SYSMENU;

			if(flags & WF_Border)
				winStyle |= WS_THICKFRAME;
			else if(flags & WF_ThinBorder)
				winStyle |= WS_BORDER;
		}


		if((flags & 0xFE) == 0)
		{ // WF_Widget
			if(flags & WF_AllowTransparency)
				winStyle |= WS_POPUP;
			else if(!customized)
				winStyle |= WS_TILEDWINDOW;
		}else if(flags & WF_Popup)
			winStyle |= WS_POPUP;
		else // tool & dialog
		{
			winStyle |= WS_SYSMENU;
			if(flags & WF_Tool)
				winExStyle |= WS_EX_TOOLWINDOW;
		}

		if(flags & WF_AllowTransparency)
			winExStyle |= WS_EX_LAYERED;
		if(flags & WF_NoActivate)
			winExStyle |= WS_EX_NOACTIVATE;
		if(flags & WF_AlwaysOnTop)
			winExStyle |= WS_EX_TOPMOST;

		*winStyleOut = winStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		*winExStyleOut = flags & WF_DontShowOnTaskbar ? winExStyle : (winExStyle | WS_EX_APPWINDOW);
	}

	void MWidget::setWindowFlags(unsigned int flags)
	{
		m_windowFlags = flags;
		if(!hasWindow())
			return;

		DWORD winStyle = 0;
		DWORD winExStyle = 0;
		generateStyleFlags(flags,&winStyle,&winExStyle);

		HWND wndHandle = m_windowExtras->m_wndHandle;
		SetWindowLongPtrW(wndHandle,GWL_STYLE,winStyle);
		if(winExStyle != 0)
			SetWindowLongPtrW(wndHandle,GWL_EXSTYLE,winExStyle);

		HWND zpos = HWND_NOTOPMOST;
		if(flags & WF_AlwaysOnTop)
			zpos = HWND_TOPMOST;
		else if(flags & WF_AlwaysOnBottom)
			zpos = HWND_BOTTOM;

		::SetWindowPos(wndHandle,zpos,0,0,0,0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (zpos == HWND_NOTOPMOST ? SWP_NOZORDER : 0));
	}

	void MWidget::destroyWnd()
	{
		if(!hasWindow()) return;

		if(m_windowExtras->m_dummyHandle != NULL)
		{
			::DestroyWindow(m_windowExtras->m_dummyHandle);
			m_windowExtras->m_dummyHandle = NULL;
		}

		SafeRelease(m_windowExtras->m_renderTarget);

		// We destroy the window first, then remove it from the
		// TLW collection, so that MApplication won't exit even if
		// this is the last opened window.
		::DestroyWindow(m_windowExtras->m_wndHandle);
		m_windowExtras->m_wndHandle = NULL;
		MApplicationData::removeTLW(this);

		m_widgetState |= MWS_Hidden;
	}

	void MWidget::createWnd()
	{
		if(hasWindow()) {
			mWarning(true, L"A window for this widget already exists! In MWidget::createWnd()");
			return;
		}

		MApplicationData::insertTLW(this);
		setTopLevelParentRecursively(this);

		if(m_windowExtras == 0)
			m_windowExtras = new WindowExtras();

		bool isLayered    = (m_windowFlags & WF_AllowTransparency) != 0;
		DWORD winStyle    = 0;
		DWORD winExStyle  = 0;
		HWND parentHandle = NULL;
		// We don't set parent of layered window.
		if(!isLayered && m_parent) {
			parentHandle = m_parent->windowHandle();
			m_parent = 0;
		}

		RECT rect = {0,0,width,height};
		generateStyleFlags(m_windowFlags,&winStyle,&winExStyle);

		::AdjustWindowRectEx(&rect,winStyle,false,winExStyle);
		m_windowExtras->m_wndHandle = CreateWindowExW(winExStyle,
			gMWidgetClassName,
			m_windowExtras->m_windowTitle.c_str(),
			winStyle,
			x,y, // Remark: we should offset a bit, due to the border frame.
			rect.right - rect.left, // Width
			rect.bottom - rect.top, // Height
			parentHandle,NULL,
			mApp->getAppHandle(), NULL);

		if(isLayered)
		{
			m_windowExtras->m_dummyHandle = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE,
				gMWidgetClassName,
				m_windowExtras->m_windowTitle.c_str(),
				WS_POPUP,
				0, 0, 50, 50,
				parentHandle,NULL,
				mApp->getAppHandle(), NULL);
			::SetLayeredWindowAttributes(m_windowExtras->m_dummyHandle,0,0,LWA_ALPHA);
		}

		createRenderTarget();
	}

	void MWidget::setTopLevelParentRecursively(MWidget* w)
	{
		m_topLevelParent = w;
		MWidgetList::iterator it    = m_children.begin();
		MWidgetList::iterator itEnd = m_children.end();
		while(it != itEnd)
		{
			(*it)->setTopLevelParentRecursively(w);
			++it;
		}
	}

	void MWidget::setParent(MWidget* p)
	{
		if(p == m_parent)
			return;
		if(m_windowFlags & WF_Window)
		{
			mDebug(L"This is a Window. If you want to set the owner of this window, use setWindowOwner()");
			return;
		}

		if(m_parent == 0)
		{
			// If a window is created, destroy it.
			if(m_windowExtras != 0)
			{
				if(m_windowExtras->m_wndHandle != NULL)
					::DestroyWindow(m_windowExtras->m_wndHandle);
				if(m_windowExtras->m_dummyHandle != NULL)
					::DestroyWindow(m_windowExtras->m_dummyHandle);
				MApplicationData::removeTLW(this);
				delete m_windowExtras;
				m_windowExtras = 0;
			}
		} else {
			m_parent->repaint(x,y,width,height); // Update the region of this widget inside the old parent.
			m_parent->m_children.remove(this);
		}

		// Setting a widget's parent to 0 makes it hide.
		MWidget* tlp = p;
		if(p == 0)
		{
			m_widgetState |= MWS_Hidden;
			if(!testAttributes(WA_ConstStyleSheet))
				m_widgetState &= (~MWS_Polished); // We need to repolish the widget.
			tlp = this;
		} else {
			// We don't modify the widget's hidden state if it changes parent.
			p->m_children.push_back(this);
			repaint(); // Update the region of this widget inside the new parent.
		}

		setTopLevelParentRecursively(tlp);
		m_parent = p;
	}

	void MWidget::setWindowOwner(MWidget* parent)
	{
		if(!isWindow() && (m_windowFlags & WF_AllowTransparency))
			return;

		HWND parentWnd = (HWND)GetWindowLong(windowHandle(), GWL_HWNDPARENT);
		HWND toSetParentWnd = (parent == 0) ? NULL : parent->windowHandle();
		if(parentWnd == toSetParentWnd)
			return;

		m_parent = parent;

		if(hasWindow())
		{
			::DestroyWindow(m_windowExtras->m_wndHandle);
			m_windowExtras->m_wndHandle = NULL;
			SafeRelease(m_windowExtras->m_renderTarget);
			createWnd();

			if(!isHidden())
			{
				::ShowWindow(m_windowExtras->m_wndHandle,SW_SHOW);
				::UpdateWindow(m_windowExtras->m_wndHandle);
			}
		}
	}

	void MWidget::setMinimumSize(long w, long h)
	{
		if(w > maxWidth)  w = maxWidth;
		else if(w < 0)    w = 0;
		if(h > maxHeight) h = maxHeight;
		else if(h < 0)    h = 0;

		minWidth  = w;
		minHeight = h;

		w = width  < minWidth  ? minWidth  : width;
		h = height < minHeight ? minHeight : height;
		resize(w,h);
	}

	void MWidget::setMaximumSize(long w, long h)
	{
		if(w < minWidth)  w = minWidth;
		else if(w < 0)    w = LONG_MAX;
		if(h < minHeight) h = minHeight;
		else if(h < 0)    h = LONG_MAX;

		maxWidth  = w;
		maxHeight = h;

		w = width  > maxWidth  ? maxWidth  : width;
		h = height > maxHeight ? maxHeight : height;
		resize(w,h);
	}

	bool MWidget::isWindow() const
	{
		if(m_windowFlags & WF_Window)
			return true;
		else if(m_parent == 0 && hasWindow())
			return true;

		return false;
	}

	bool MWidget::isHidden() const
		{ return (m_widgetState & MWS_Hidden) != 0; }
	void MWidget::setStyleSheet(const std::wstring& css)
		{ mApp->getStyleSheet()->setWidgetSS(this,css); }
	void MWidget::ensurePolished()
	{
		if((m_widgetState & MWS_Polished) ||
			(m_attributes & WA_NoStyleSheet)) return;

		mApp->getStyleSheet()->polish(this);
		m_widgetState |= MWS_Polished;
	}

	void MWidget::ssSetOpaque(bool opaque)
		{ setWidgetState(MWS_CSSOpaqueBG,opaque); }

	void MWidget::showMinimized()
	{
		if(hasWindow() && m_windowState != WindowMinimized) {
			m_windowState = WindowMinimized;
			setWidgetState(MWS_Hidden,true);
			::ShowWindow(m_windowExtras->m_wndHandle, SW_MINIMIZE);
		}
	}

	void MWidget::showMaximized()
	{
		if(isWindow() && m_windowState != WindowMaximized) {
			m_windowState = WindowMaximized;
			setWidgetState(MWS_Hidden,false);
			::ShowWindow(m_windowExtras->m_wndHandle, SW_MAXIMIZE);
		}
	}

	void MWidget::show()
	{
		if(!isHidden()) return;

		// When polishing stylesheet, this is still hidden,
		// so even if this widget's size changed, it won't repaint itself.
		ensurePolished();
		setWidgetState(MWS_Hidden,false);

		// If has window handle (i.e. WF_Window or parentless shown WF_Widget )
		if(hasWindow())
		{
			if(m_windowState == WindowMinimized || m_windowState == WindowMaximized)
			{
				m_windowState = WindowNoState;
				::ShowWindow(m_windowExtras->m_wndHandle, SW_SHOWNORMAL);
				return;
			}

			if(::IsWindowVisible(m_windowExtras->m_wndHandle))
				return;

			HWND hwnd = m_windowExtras->m_dummyHandle;

			if(hwnd == NULL) {
				hwnd = m_windowExtras->m_wndHandle;
			} else {
				::ShowWindow(m_windowExtras->m_wndHandle, SW_SHOW);
			}
			::ShowWindow(hwnd, SW_SHOW);
			if(m_windowFlags & WF_AlwaysOnBottom)
				::SetWindowPos(m_windowExtras->m_wndHandle,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED);
			if(!m_windowExtras->updateWidgets.empty())
				::UpdateWindow(hwnd);
			return;
		}
		
		// If this is a window, we have to create the window first
		// before polishing stylesheet
		if((m_windowFlags & WF_Window) || m_parent == 0)
		{
			createWnd();
			HWND hwnd = m_windowExtras->m_wndHandle;
			::ShowWindow(hwnd, SW_SHOW);
			if(m_windowFlags & WF_AllowTransparency)
			{
				::ShowWindow(m_windowExtras->m_dummyHandle, SW_SHOW);
				::UpdateWindow(m_windowExtras->m_dummyHandle);
			} else
				::UpdateWindow(hwnd);
			
			return;
		}

		repaint();
	}

	void MWidget::hide()
	{
		if(isHidden()) return;

		m_widgetState |= MWS_Hidden;
		m_widgetState &= (~MWS_Visible);
		if(hasWindow()) {
			::ShowWindow(m_windowExtras->m_wndHandle, SW_HIDE);
			if(m_windowExtras->m_dummyHandle != NULL)
				::ShowWindow(m_windowExtras->m_dummyHandle, SW_HIDE);

			::SendMessage(m_windowExtras->m_wndHandle, WM_MOUSEMOVE, 0, MAKELPARAM(-1,-1));
			if(m_windowExtras->bTrackingMouse)
			{
				TRACKMOUSEEVENT tme = {};
				tme.cbSize      = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags     = TME_HOVER | TME_LEAVE | TME_CANCEL;
				tme.hwndTrack   = m_windowExtras->m_wndHandle;
				tme.dwHoverTime = 400;
				::TrackMouseEvent(&tme);
				m_windowExtras->bTrackingMouse = false;
			}
		} else {
			m_parent->repaint(x,y,width,height);
		}
	}

	void MWidget::setGeometry(long vx, long vy, long vwidth, long vheight)
	{
		// Ensure the size is in a valid range.
		if     (vwidth  < minWidth ) { vwidth  = minWidth;  }
		else if(vwidth  > maxWidth ) { vwidth  = maxWidth;  }
		if     (vheight < minHeight) { vheight = minHeight; } 
		else if(vheight > maxHeight) { vheight = maxHeight; }

		bool sizeChanged = (vwidth != width || vheight != height);

		if(!sizeChanged && vx == x && vy == y)
			return;

		MResizeEvent ev(width,height,vwidth,vheight);

		if(isHidden()) {
			x = vx;
			y = vy;

			if(sizeChanged)
			{
				width  = vwidth;
				height = vheight;
				resizeEvent(&ev);
			}
		} else
		{
			if(m_parent != 0) {
				// If is a child widget, we need to update the old region within the parent.
				m_parent->repaint(x,y,width,height);
				x = vx;
				y = vy;
				if(sizeChanged)
				{
					width  = vwidth;
					height = vheight;
					resizeEvent(&ev);
				}
				// Update the new rect.
				repaint();
			}
		}
		if(m_windowExtras != 0)
		{
			// It's a window. We change the size of it. And Windows will repaint it automatically.
			RECT rect = { 0, 0, width, height };
			DWORD winStyle   = 0;
			DWORD winExStyle = 0;
			generateStyleFlags(m_windowFlags, &winStyle, &winExStyle);
			::AdjustWindowRectEx(&rect, winStyle, false, winExStyle);
			::MoveWindow(m_windowExtras->m_wndHandle, x, y, rect.right - rect.left, rect.bottom - rect.top, true);
		}
	}

	bool MWidget::isVisible() const
	{
		bool selfVisible = 0 != (m_widgetState & MWS_Visible);
		if(!selfVisible)  return false;
		if(m_parent == 0) return true;

		return m_parent->isVisible();
	}

	bool MWidget::isOpaqueDrawing() const
	{
		if(m_attributes & WA_AutoBG)
			return (m_widgetState & MWS_CSSOpaqueBG) != 0;

		return (m_attributes & WA_OpaqueBG) == 0;
	}

	void MWidget::repaint(int ax, int ay, unsigned int aw, unsigned int ah)
	{
		if(windowHandle() == NULL) return;
		if(isHidden()) return;

		int right  = ax + aw;
		int bottom = ay + ah;
		if(ax >= (int)width || ay >= (int)height || right <= 0 || bottom <= 0)
			return;

		right  = min(right, (int)width);
		bottom = min(bottom,(int)height);

		// If the widget needs to draw itself, we marks it invisible at this time,
		// And if it does draw itself during next redrawing. It's visible.
		m_widgetState &= (~MWS_Visible);

		if(m_attributes & WA_DontShowOnScreen)
			return;

		MWidget* parent = m_parent;
		while(parent != 0)
		{
			if(parent->isHidden()) return;
			parent = parent->m_parent;
		}
		m_topLevelParent->m_windowExtras->addToRepaintMap(this, ax, ay, right, bottom);

		// Tells Windows that we need to update, we don't care the clip region.
		// We do it in our own way.
		RECT rect = { 0, 0, 1, 1 };
		HWND hwnd = m_topLevelParent->m_windowExtras->m_dummyHandle;
		if(hwnd == NULL) hwnd = windowHandle();
		::InvalidateRect(hwnd, &rect, false);
	}

	inline void markParentToBeChecked(MWidget* start, DirtyChildrenHash& h)
	{
		MWidget* ucp = start->parent();
		while(ucp != 0) {
			bool& marked = h[ucp];
			if(marked) break;
			marked = true;
			ucp = ucp->parent();
		}
	}

	void MWidget::createRenderTarget()
	{
		M_ASSERT(m_windowExtras != 0);
		mWarning(m_windowExtras->m_renderTarget != 0,
			L"We don't remember to release the created renderTarget");
		SafeRelease(m_windowExtras->m_renderTarget);

		D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
		p.usage  = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
		p.type   = mApp->isHardwareAccerated() ? 
				D2D1_RENDER_TARGET_TYPE_HARDWARE : D2D1_RENDER_TARGET_TYPE_SOFTWARE;
		p.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
		p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

		mApp->getD2D1Factory()->CreateHwndRenderTarget(p,
			D2D1::HwndRenderTargetProperties(m_windowExtras->m_wndHandle,D2D1::SizeU(width,height)),
			&m_windowExtras->m_renderTarget);
	}

	// Windows has told us that we can now repaint the window.
	void MWidget::drawWindow()
	{
		M_ASSERT(isWindow());

		bool layeredWindow    = (m_windowFlags & WF_AllowTransparency) != 0;
		bool fullWindowUpdate = false;
		RECT windowUpdateRect = {width,height,0,0};

		// Calculate which widget needs to be redraw.
		DirtyChildrenHash& childUpdatedHash  = m_windowExtras->childUpdatedHash;
		DrawRegionHash& passiveUpdateWidgets = m_windowExtras->passiveUpdateWidgets;
		DrawRectHash& updateWidgets          = m_windowExtras->updateWidgets;
		DrawRectHash::iterator drIter        = updateWidgets.begin();
		DrawRectHash::iterator drIterEnd     = updateWidgets.end();
		while(drIterEnd != drIter)
		{
			MWidget* uTarget = drIter->first;
			RECT&    uRect   = drIter->second;
			++drIter;

			// If we are dealing with the topLevelWindow itself,
			// we only have to add it the passiaveUpdateWidgets.
			if(uTarget == this) {
				passiveUpdateWidgets[this].addRect(uRect);
				if(layeredWindow && uRect.left == 0 &&
					uRect.right == width && uRect.top == 0 && uRect.bottom == height)
				{
					windowUpdateRect.left   = 0;
					windowUpdateRect.right  = width;
					windowUpdateRect.top    = 0;
					windowUpdateRect.bottom = height;
					fullWindowUpdate = true;
				}
				continue;
			}

			RECT urForNonOpaque = uRect;

			MWidget* pc = uTarget;
			MWidget* pp = pc->m_parent;
			bool outsideOfParent = false;
			DrawRegionHash tempPassiveWidgets;
			MRegion uTargetUR(uRect);

			// First, map the update rect to the Window's coordinate. If the 
			// drawing rect is outside of the parent's visible rect, we simply
			// ingore its update request. 
			// We also check if there might be overlap, and if there's something
			// above uTarget, we need to update those things too.
			while(pp != 0)
			{
				OffsetRect(&uRect,pc->x,pc->y);
				uTargetUR.offset(pc->x,pc->y);
				DrawRegionHash::iterator tpwIter    = tempPassiveWidgets.begin();
				DrawRegionHash::iterator tpwIterEnd = tempPassiveWidgets.end();
				for(; tpwIterEnd != tpwIter; ++tpwIter)
					tpwIter->second.offset(pc->x,pc->y);

				if(uRect.right  <= 0)              { outsideOfParent = true; break; }
				if(uRect.bottom <= 0)              { outsideOfParent = true; break; }
				if(uRect.left   > (int)pp->width ) { outsideOfParent = true; break; }
				if(uRect.top    > (int)pp->height) { outsideOfParent = true; break; } 
				// Clip to parent.
				if(uRect.right  > (int)pp->width ) { uRect.right  = pp->width;  }
				if(uRect.bottom > (int)pp->height) { uRect.bottom = pp->height; }
				if(uRect.left   < 0)               { uRect.left   = 0;          }
				if(uRect.top    < 0)               { uRect.top    = 0;          }

				// If the parent specifies that its children may overlap each
				// other. We have to test if the current draw region intersects
				// its children. If it does, we have to update those children.
				if(!pp->testAttributes(WA_NonChildOverlap))
				{
					MWidgetList::iterator chIter    = pp->m_children.begin();
					MWidgetList::iterator chIterEnd = pp->m_children.end();
					while(chIter != chIterEnd && *(chIter++) != pc) {}

					while(chIter != chIterEnd)
					{
						MWidget* cw = *(chIter++);
						if(cw->isHidden() || cw->testAttributes(WA_DontShowOnScreen)) continue;

						RECT childRect = {cw->x, cw->y, cw->x + cw->width, cw->y + cw->height};
						MRegion childR(childRect);
						if(cw->isOpaqueDrawing())
						{
							if(uTargetUR.isRectPartlyInside(childRect))
							{
								uTargetUR.subtract(childR);
								DrawRegionHash::iterator tpwIter    = tempPassiveWidgets.begin();
								DrawRegionHash::iterator tpwIterEnd = tempPassiveWidgets.end();
								for(; tpwIterEnd != tpwIter; ++tpwIter)
									tpwIter->second.subtract(childR);

								if(uTargetUR.isEmpty()) break;
							}
						} else {
							childR.intersect(uTargetUR);
							if(!childR.isEmpty())
								tempPassiveWidgets[cw].combine(childR);
						}
					}
				} // testAttributes(WA_NonChildOverlap)

				pc = pp;
				pp = pp->m_parent;
			}
			// The uTarget is not updated, thus, it won't affect anything.
			if(outsideOfParent || uTargetUR.isEmpty()) { continue; }

			// Combine the temp with the passiveUpdateWidgets.
			DrawRegionHash::iterator tempIter    = tempPassiveWidgets.begin();
			DrawRegionHash::iterator tempIterEnd = tempPassiveWidgets.end();
			for(; tempIter != tempIterEnd; ++tempIter)
			{
				MWidget* uc = tempIter->first;
				passiveUpdateWidgets[uc].combine(tempIter->second);
				// Mark the parent to be check later.
				markParentToBeChecked(uc, childUpdatedHash);
			}
			
			// Add the uTarget to the passiaveUpdateWidgets.
			passiveUpdateWidgets[uTarget].combine(uTargetUR);
			markParentToBeChecked(uTarget, childUpdatedHash);

			if(layeredWindow && !fullWindowUpdate)
			{
				RECT wur;
				uTargetUR.getBounds(wur);
				if(windowUpdateRect.left   > wur.left  ) windowUpdateRect.left   = wur.left;
				if(windowUpdateRect.right  < wur.right ) windowUpdateRect.right  = wur.right;
				if(windowUpdateRect.top    > wur.top   ) windowUpdateRect.top    = wur.top;
				if(windowUpdateRect.bottom < wur.bottom) windowUpdateRect.bottom = wur.bottom;
			}

			// Second, if this widget is not opaque, and there's widget under it
			// we have to paint that widget too.
			bool isPcOpaque = uTarget->isOpaqueDrawing();
			if(isPcOpaque) { continue; }

			MRegion underneathUR(uTargetUR);
			pp = uTarget->m_parent;
			pc = uTarget;

			while(pp != 0 && !isPcOpaque)
			{
				OffsetRect(&urForNonOpaque,pc->x,pc->y);
				DrawRegionHash::iterator tpwIter    = tempPassiveWidgets.begin();
				DrawRegionHash::iterator tpwIterEnd = tempPassiveWidgets.end();
				for(; tpwIterEnd != tpwIter; ++tpwIter)
					tpwIter->second.offset(pc->x,pc->y);

				// Clip to parent.
				if(urForNonOpaque.right  > (int)pp->width ) { urForNonOpaque.right  = pp->width;  }
				if(urForNonOpaque.bottom > (int)pp->height) { urForNonOpaque.bottom = pp->height; }
				if(urForNonOpaque.left   < 0)               { urForNonOpaque.left   = 0;          }
				if(urForNonOpaque.top    < 0)               { urForNonOpaque.top    = 0;          }

				MWidgetList::reverse_iterator chrIter    = pp->m_children.rbegin();
				MWidgetList::reverse_iterator chrIterEnd = pp->m_children.rend();
				while(chrIter != chrIterEnd && *(chrIter++) != pc) {}

				while(chrIter != chrIterEnd)
				{
					MWidget* cw = *(chrIter++);
					if(cw->isHidden() || cw->testAttributes(WA_DontShowOnScreen)) continue;

					RECT siblingRect = {cw->x, cw->y, cw->x + cw->width, cw->y + cw->height};
					MRegion siblingR(siblingRect);
					siblingR.intersect(underneathUR);
					if(!siblingR.isEmpty())
					{
						passiveUpdateWidgets[cw].combine(siblingR);
						markParentToBeChecked(cw, childUpdatedHash);

						if(cw->isOpaqueDrawing())
						{
							underneathUR.subtract(siblingR);
							if(underneathUR.isEmpty()) break;
						}
					}
				}

				if(underneathUR.isEmpty()) break;
				passiveUpdateWidgets[pp].combine(underneathUR);

				pc = pp;
				pp = pc->m_parent;
				isPcOpaque = pc->isOpaqueDrawing();
			}
		}

		HRESULT result;
		int retryTimes = 3;
		do
		{
			getRenderTarget()->BeginDraw();
			draw(0,0,passiveUpdateWidgets.find(this) != passiveUpdateWidgets.end());
			if(layeredWindow)
			{
				ID2D1GdiInteropRenderTarget* gdiRT;
				HDC dc;
				m_windowExtras->m_renderTarget->QueryInterface(&gdiRT);
				gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,&dc);

				BLENDFUNCTION blend;
				blend.BlendOp     = AC_SRC_OVER;
				blend.AlphaFormat = AC_SRC_ALPHA;
				blend.BlendFlags  = 0;
				blend.SourceConstantAlpha = 255;
				POINT sourcePos = {0, 0};
				SIZE windowSize = {width, height};

				UPDATELAYEREDWINDOWINFO info = {};
				info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
				info.crKey    = 0;
				info.dwFlags  = ULW_ALPHA;
				info.hdcDst   = NULL;
				info.hdcSrc   = dc;
				info.pblend   = &blend;
				info.psize    = &windowSize; // NULL
				info.pptDst   = NULL;
				info.pptSrc   = &sourcePos;
				info.prcDirty = &windowUpdateRect;
				::UpdateLayeredWindowIndirect(m_windowExtras->m_wndHandle,&info);

				gdiRT->ReleaseDC(0);
				gdiRT->Release();
			}
			result = getRenderTarget()->EndDraw();

			if(result == D2DERR_RECREATE_TARGET)
			{
				SafeRelease(m_windowExtras->m_renderTarget);
				createRenderTarget();
				mApp->getStyleSheet()->discardResource(m_windowExtras->m_renderTarget);
			}
		} while (FAILED(result) && (--retryTimes >= 0));

		m_windowExtras->clearUpdateQueue();
	}

	void MWidget::doStyleSheetDraw(ID2D1RenderTarget* rt,const RECT& widgetRectInRT, const RECT& clipRectInRT)
		{ mApp->getStyleSheet()->draw(this,rt,widgetRectInRT,clipRectInRT); }

	void MWidget::draw(int xOffsetInWnd, int yOffsetInWnd, bool drawMySelf)
	{
		WindowExtras* tlpWE = m_topLevelParent->m_windowExtras;
		if(drawMySelf)
		{
			ID2D1RenderTarget* rt = getRenderTarget();
			MRegion& updateRegion = tlpWE->passiveUpdateWidgets[this];

			// Draw the background with CSS
			if(!testAttributes(WA_NoStyleSheet))
			{
				MRegion::Iterator iter = updateRegion.begin();
				while(iter) {
					// Draw the background with CSS
					RECT clipRect;
					iter.getRect(clipRect);
					rt->PushAxisAlignedClip(D2D1::RectF((FLOAT)clipRect.left,(FLOAT)clipRect.top,
						(FLOAT)clipRect.right, (FLOAT)clipRect.bottom), D2D1_ANTIALIAS_MODE_ALIASED);

					RECT widgetRect;
					widgetRect.left   = xOffsetInWnd;
					widgetRect.right  = widgetRect.left + width;
					widgetRect.top    = yOffsetInWnd;
					widgetRect.bottom = widgetRect.top + height;
					doStyleSheetDraw(rt,widgetRect,clipRect);
					++iter;
					rt->PopAxisAlignedClip();
				}
			}

			// Tell the user that their widget should be updated.
			MPaintEvent pe(updateRegion);
			paintEvent(&pe);
			if(!pe.isAccepted())
				return;

			// Process the children.
			// Remark: When drawing the children, we could start from
			// the top, if we meet a opaque child, we immediately draw it
			// and exclude its rectangle from the update region. Maybe we
			// should try this.
			MWidgetList::iterator chIter = m_children.begin();
			MWidgetList::iterator chIterEnd = m_children.end();
			RECT childRectInWnd;
			while(chIter != chIterEnd)
			{
				MWidget* child = *chIter;
				++chIter;
				if(child->isHidden()) continue;

				childRectInWnd.left   = xOffsetInWnd + child->x;
				childRectInWnd.top    = yOffsetInWnd + child->y;
				childRectInWnd.right  = childRectInWnd.left + child->width;
				childRectInWnd.bottom = childRectInWnd.top + child->height;

				MRegion childRegion(childRectInWnd);
				childRegion.intersect(updateRegion);

				if(childRegion.isEmpty()) {
					if(tlpWE->passiveUpdateWidgets.find(child) != tlpWE->passiveUpdateWidgets.end())
						child->draw(childRectInWnd.left,childRectInWnd.top,true);
				} else {
					tlpWE->passiveUpdateWidgets[child].combine(childRegion);
					child->draw(childRectInWnd.left,childRectInWnd.top,true);
				}
			}
		} else {
			MWidgetList::iterator chIter = m_children.begin();
			MWidgetList::iterator chIterEnd = m_children.end();
			while(chIter != chIterEnd) {
				MWidget* child = *chIter;
				++chIter;
				if(child->isHidden()) continue;
				if(tlpWE->passiveUpdateWidgets.find(child) != tlpWE->passiveUpdateWidgets.end())
					child->draw(xOffsetInWnd+child->x,yOffsetInWnd+child->y,true);
				else if(tlpWE->childUpdatedHash.find(child) != tlpWE->childUpdatedHash.end())
					child->draw(xOffsetInWnd+child->x,yOffsetInWnd+child->y,false);
			}
		}
	}
}
