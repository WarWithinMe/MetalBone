#include "MApplication.h"
#include "MWidget.h"
#include "MResource.h"
#include "MRegion.h"
#include "private/MWidget_p.h"
#include "private/MApp_p.h"

#include <windows.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

namespace MetalBone
{
	extern wchar_t gMWidgetClassName[] = L"MetalBone Widget";
	extern MCursor gArrowCursor = MCursor(MCursor::ArrowCursor);

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
		WNDCLASSW wc = {};
		setupRegisterClass(wc);
		wc.lpszClassName = gMWidgetClassName;
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
	HINSTANCE                 MApplication::getAppHandle()    const { return mImpl->appHandle;       }
	MStyleSheetStyle*         MApplication::getStyleSheet()         { return &(mImpl->ssstyle);      }
	ID2D1Factory*             MApplication::getD2D1Factory()        { return mImpl->d2d1Factory;     }
	IDWriteFactory*           MApplication::getDWriteFactory()      { return mImpl->dwriteFactory;   }
	IWICImagingFactory*       MApplication::getWICImagingFactory()  { return mImpl->wicFactory;      }
	bool  MApplication::isHardwareAccerated()                 const { return mImpl->hardwareAccelerated; }
	void  MApplication::setStyleSheet(const std::wstring& css)      { mImpl->ssstyle.setAppSS(css);  }
	void  MApplication::setCustomWindowProc(WinProc proc)           { mImpl->customWndProc = proc;   }

	void MApplication::setupRegisterClass(WNDCLASSW& wc)
	{
		wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc   = MApplicationData::windowProc;
		wc.hInstance     = mImpl->appHandle;
		wc.hCursor       = gArrowCursor.getHandle();
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	}

	int MApplication::exec()
	{
		MSG msg;
		int result;

		while( (result = ::GetMessageW(&msg, 0, 0, 0)) != 0)
		{
			if(result == -1) break; // GetMessage Error

			if(msg.message == WM_HOTKEY)
			{
				std::vector<MShortCut*> scs;
				MShortCut::getMachedShortCuts(LOWORD(msg.lParam),HIWORD(msg.lParam),scs);
				for(int i = scs.size() - 1; i >= 0; --i)
					scs.at(i)->invoked();

			} else {
				::TranslateMessage(&msg);
				::DispatchMessageW(&msg);
			}
		}

		aboutToQuit();
		return result;
	}



	// ========== MApplicationData Impl ==========
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

#define RET_DEFPROC return ::DefWindowProcW(hwnd,msg,wparam,lparam)

	LRESULT MApplicationData::windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if(!instance) RET_DEFPROC;

		if(customWndProc)
		{
			LRESULT result;
			if(customWndProc(hwnd,msg,wparam,lparam,&result)) return result;
		}

		MWidget* window = 0;
		switch(msg) {

		case WM_DESTROY:
			// Check if the user wants to quit when the last window is closed.
			if(instance->quitOnLastWindowClosed && instance->topLevelWindows.size() == 0)
				mApp->exit(0);
			return 0;
		case WM_PAINT:
			window = instance->findWidgetByHandle<true>(hwnd);
			if(window == 0) RET_DEFPROC;

			{
				MRect updateRect;
				::GetUpdateRect(hwnd,&updateRect,false);
				if(updateRect.right > 1 && updateRect.bottom > 1)
				{
					MRect clientRect;
					::GetClientRect(hwnd,&clientRect);
					// If we have to update the whole window,
					// we should ignore the update request make by the child widgets.
					if(updateRect == clientRect)
						window->m_windowExtras->clearUpdateQueue();

					window->m_windowExtras->addToRepaintMap(window,0,0,window->width,window->height);
				}
			}

			window->drawWindow();
			::ValidateRect(hwnd,0);
			return 0;
		case WM_MOVE:
			window = instance->findWidgetByHandle<true>(hwnd);
			if (window == 0) RET_DEFPROC;

			long newX = LOWORD(lparam);
			long newY = HIWORD(lparam);
			if(window->x != newX || window->y != newY)
			{
				window->x = newX;
				window->y = newY;
				if(window->m_windowFlags & WF_AllowTransparency)
				{
					BLENDFUNCTION blend = {AC_SRC_OVER,0,255,AC_SRC_ALPHA};
					POINT windowPos = {window->x, window->y};
					UPDATELAYEREDWINDOWINFO info = {};
					info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
					info.dwFlags  = ULW_ALPHA;
					info.pblend   = &blend;
					info.pptDst   = &windowPos;
					::UpdateLayeredWindowIndirect(window->m_windowExtras->m_wndHandle,&info);
				}
			}
			return 0;
		}

		window = instance->findWidgetByHandle<false>(hwnd);
		if(window == 0) RET_DEFPROC;
		WindowExtras* xtr = window->m_windowExtras;

		switch(msg) {

		case WM_NCHITTEST:
			{
				LRESULT result;
				if(!::DwmDefWindowProc(hwnd,msg,wparam,lparam,&result))
					result = ::DefWindowProcW(hwnd,msg,wparam,lparam);
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
				case HTCLIENT:
					// Check if the widget under the mouse is to
					// simulate the Non-Client area.
// 					{
// 						MWidget* widgetUnderMouse = xtr->widgetUnderMouse;
// 						int xpos = GET_X_LPARAM(lparam);
// 						int ypos = GET_Y_LPARAM(lparam);
// 
// 						if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
// 							{ widgetUnderMouse = window->findWidget(xpos, ypos); }
// 
// 						if(widgetUnderMouse->widgetRole() == WR_Normal)
// 						{
// 
// 						}
// 					}
				default: return result;
				}
			}
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
					tme.cbSize      = sizeof(TRACKMOUSEEVENT);
					tme.dwFlags     = TME_HOVER | TME_LEAVE;
					tme.hwndTrack   = hwnd;
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
						MRect rect;
						::GetWindowRect(hwnd,&rect);
						rect.offset(xtr->lastMouseX, xtr->lastMouseY);
						MMouseEvent me(xpos, ypos, rect.left, rect.top, NoButton);
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
						MRect rect;
						::GetWindowRect(hwnd,&rect);
						thisTT->show(rect.left + xtr->lastMouseX + 20,
							rect.top + xtr->lastMouseY + 20);
					}
				}
			}
			break;
		case WM_SETCURSOR:
			return (LOWORD(lparam) == HTCLIENT) ? TRUE : ::DefWindowProcW(hwnd,msg,wparam,lparam);
		case WM_MOUSEHOVER:
			{
				if(xtr->widgetUnderMouse != 0)
				{
					MToolTip* tip = xtr->widgetUnderMouse->getToolTip();
					if(tip)
					{
						MRect rect;
						::GetWindowRect(hwnd,&rect);
						tip->show(rect.left + xtr->lastMouseX + 20,
								  rect.top  + xtr->lastMouseY + 20);
					}
				}
				xtr->bTrackingMouse = false;
			}
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			{
				// When we press down keys, we need to check the shortcut first.
				std::vector<MShortCut*> scs;
				MShortCut::getMachedShortCuts(mapKeyState(),wparam,scs);

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

			if(msg == WM_SYSKEYDOWN) RET_DEFPROC;
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
				if(msg == WM_SYSCHAR) RET_DEFPROC;
			}
			break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			{
				::SetFocus(hwnd);
				int xpos = GET_X_LPARAM(lparam);
				int ypos = GET_Y_LPARAM(lparam);
				if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
					::SendMessage(hwnd,WM_MOUSEMOVE,wparam,lparam);

				MWidget* cw = xtr->widgetUnderMouse;
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

					MWidget* pc = cw;
					MWidget* pp = cw->m_parent;
					while(pp != 0)
					{
						xpos -= pc->x;
						ypos -= pc->y;
						pc = pp; pp = pc->m_parent;
					}

					MRect rect;
					::GetWindowRect(hwnd,&rect);
					MMouseEvent me(xpos, ypos,
						rect.left + xtr->lastMouseX,
						rect.top  + xtr->lastMouseY, btn);
					me.ignore();
					do {
						cw->mousePressEvent(&me);
						me.offsetPos(cw->x,cw->y);
						cw = cw->m_parent;
					} while (cw && !me.isAccepted() &&
						cw->testAttributes(WA_NoMousePropagation));

					::SetCapture(hwnd);
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
				if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
					::SendMessage(hwnd,WM_MOUSEMOVE,wparam,lparam);

				MWidget* cw = xtr->widgetUnderMouse;
				if(cw != 0)
				{
					MouseButton btn = (msg == WM_LBUTTONUP ? LeftButton :
						(msg == WM_RBUTTONUP ? RightButton : MiddleButton));
					if(btn == LeftButton) {
						cw->setWidgetState(MWS_Pressed, false);
						instance->ssstyle.updateWidgetAppearance(cw);
					}

					MRect rect;
					::GetWindowRect(hwnd,&rect);
					MMouseEvent me(xpos, ypos,
						rect.left + xtr->lastMouseX,
						rect.top  + xtr->lastMouseY, btn);
					me.ignore();
					do {
						cw->mouseReleaseEvent(&me);
						me.offsetPos(cw->x,cw->y);
						cw = cw->m_parent;
					} while (cw && !me.isAccepted() &&
						cw->testAttributes(WA_NoMousePropagation));
					::ReleaseCapture();
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
				if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
					::SendMessage(hwnd,WM_MOUSEMOVE,wparam,lparam);

				MWidget* cw = xtr->widgetUnderMouse;
				if(cw != 0)
				{
					MouseButton btn = (msg == WM_LBUTTONDBLCLK ? LeftButton :
						(msg == WM_RBUTTONDBLCLK ? RightButton : MiddleButton));
					if(btn == LeftButton) {
						cw->setWidgetState(MWS_Pressed,true);
						instance->ssstyle.updateWidgetAppearance(cw);
					}

					RECT rect;
					::GetWindowRect(hwnd,&rect);
					MMouseEvent me(xpos, ypos,
						rect.left + xtr->lastMouseX,
						rect.top  + xtr->lastMouseY, btn);
					me.ignore();
					do {
						cw->mouseDClickEvent(&me);
						me.offsetPos(cw->x,cw->y);
						cw = cw->m_parent;
					} while (cw && !me.isAccepted() &&
						cw->testAttributes(WA_NoMousePropagation));
				}
			}
			break;

		case WM_ACTIVATEAPP:
			::SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(-1,-1));
			break;
		case WM_MOUSELEAVE:
			if(window->m_windowExtras->bTrackingMouse)
				::SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(-1,-1));
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
			if(wparam == SIZE_MINIMIZED)
			{
				window->m_windowState = WindowMinimized;
				return 0;
			} else if(wparam == SIZE_MAXIMIZED)
				window->m_windowState = WindowMaximized;

			if(window->width == LOWORD(lparam) && window->height == HIWORD(lparam))
				return 0;
			{
				MResizeEvent ev(window->width,window->height,LOWORD(lparam),HIWORD(lparam));
				window->width  = LOWORD(lparam);
				window->height = HIWORD(lparam);
				window->resizeEvent(&ev);
				xtr->m_renderTarget->Resize(D2D1::SizeU(LOWORD(lparam),HIWORD(lparam)));
			}
			break;
		case WM_GETMINMAXINFO:
			{
				DWORD winStyle    = 0;
				DWORD winExStyle  = 0;
				// We don't set parent of layered window.
				MRect wndRect(0,0,window->width,window->height);
				generateStyleFlags(window->m_windowFlags,&winStyle,&winExStyle);
				::AdjustWindowRectEx(&wndRect,winStyle,false,winExStyle);
				long dx = wndRect.width() - window->width;
				long dy = wndRect.height()- window->height;

				MINMAXINFO* info  = (MINMAXINFO*)lparam;
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
			if(lparam == 0)
				wparam == TRUE ? window->show() : window->hide();
			RET_DEFPROC;
		case WM_SYSCOMMAND:
			// Only intercept this message when the window is layered window.
			if(window->m_windowFlags & WF_AllowTransparency)
			{
				switch(wparam) {
					case SC_MINIMIZE: window->showMinimized(); return 0;
					case SC_MAXIMIZE: window->showMaximized(); return 0;
					case SC_RESTORE:  window->show();          return 0;
				}
			}
			RET_DEFPROC;

		default: RET_DEFPROC;
		}

		return 0;
	}
}