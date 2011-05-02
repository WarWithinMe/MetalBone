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
	typedef std::tr1::unordered_map<MWidget*,RECT>   DrawRectHash;
	typedef std::tr1::unordered_map<MWidget*,MRegion> DrawRegionHash;
	typedef std::tr1::unordered_map<MWidget*,bool>    DirtyChildrenHash;
	struct WindowExtras {
		WindowExtras():m_wndHandle(NULL),m_layeredWndHandle(NULL),
			m_renderTarget(0),m_rtHook(0),bTrackingMouse(false),
			widgetUnderMouse(0),focusedWidget(0),lastMouseX(0),lastMouseY(0){}
		HWND m_wndHandle;
		HWND m_layeredWndHandle;
		ID2D1HwndRenderTarget* m_renderTarget;
		ID2D1RenderTarget*     m_rtHook;

		bool bTrackingMouse;
		MWidget* widgetUnderMouse;
		MWidget* focusedWidget;
		int lastMouseX;
		int lastMouseY;

		std::wstring      m_windowTitle;
		DrawRectHash      updateWidgets;
		DrawRegionHash    passiveUpdateWidgets;
		DirtyChildrenHash childUpdatedHash;

		inline HWND getRealHwnd() const
		{
			if(m_layeredWndHandle != NULL)
				return m_layeredWndHandle;
			return m_wndHandle;
		}

		void clearUpdateQueue()
		{
			updateWidgets.clear();
			passiveUpdateWidgets.clear();
			childUpdatedHash.clear();
		}

		void addToRepaintMap(MWidget* w,int left,int right,int top,int bottom)
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
	};





	// ========== MApplicationData ==========
	wchar_t gMWidgetClassName[] = L"MetalBone Widget";
	extern MCursor gArrowCursor = MCursor(MCursor::ArrowCursor);
	struct MApplicationData
	{
		MApplicationData(bool hwAccelerated):
				quitOnLastWindowClosed(true),
				hardwareAccelerated(hwAccelerated),
				currentToolTip(0) { instance = this; }
		// Window procedure
		static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);

		std::set<MWidget*> topLevelWindows;

		MWidget* findWidgetByHandle(HWND handle) const;
		MWidget* findWidgetWithLayeredHandle(HWND handle) const;
		static inline void removeTLW(MWidget* w)
		{ instance->topLevelWindows.erase(w); }
		static inline void insertTLW(MWidget* w)
		{ instance->topLevelWindows.insert(w); }

		bool                quitOnLastWindowClosed;
		bool                hardwareAccelerated;
		HINSTANCE           appHandle;
		MStyleSheetStyle    ssstyle;

		ID2D1Factory*       d2d1Factory;
		IDWriteFactory*     dwriteFactory;
		IWICImagingFactory* wicFactory;

		ULONG_PTR gdiPlusToken;

		static MApplication::WinProc customWndProc;
		static MApplicationData* instance;
		static void setFocusWidget(MWidget*);
		static void showToolTip(MToolTip*, int xInDesktop, int yInDesktop);
		static void hideToolTip(MToolTip* tooltip = 0);
		
		MToolTip* currentToolTip;
	};

	MApplication::WinProc MApplicationData::customWndProc = 0;
	MApplicationData* MApplicationData::instance = 0;
	MWidget* MApplicationData::findWidgetByHandle(HWND handle) const
	{
		std::set<MWidget*>::const_iterator iter = topLevelWindows.begin();
		std::set<MWidget*>::const_iterator iterEnd = topLevelWindows.end();
		while(iter != iterEnd)
		{
			MWidget* w = *iter;
			if(w->windowHandle() == handle)
				return w;
			++iter;
		}
		return 0;
	}

	void MApplicationData::showToolTip(MToolTip* tip, int xInDesktop, int yInDesktop)
	{
		if(instance->currentToolTip != 0)
			instance->currentToolTip->hide();

		instance->currentToolTip = tip;
		tip->show(xInDesktop,yInDesktop);
	}

	void MApplicationData::hideToolTip(MToolTip* tooltip)
	{
		if(!instance->currentToolTip)
			return;

		if(tooltip != 0 && instance->currentToolTip != tooltip)
			return;

		instance->currentToolTip->hide();
		instance->currentToolTip = 0;
	}

	MWidget* MApplicationData::findWidgetWithLayeredHandle(HWND handle) const
	{
		std::set<MWidget*>::const_iterator iter = topLevelWindows.begin();
		std::set<MWidget*>::const_iterator iterEnd = topLevelWindows.end();
		while(iter != iterEnd)
		{
			MWidget* w = *iter;
			M_ASSERT(w->m_windowExtras != 0);
			if(w->m_windowExtras->m_wndHandle == handle ||
			   w->m_windowExtras->m_layeredWndHandle == handle)
				return w;
			++iter;
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
		if(::GetKeyState(VK_LWIN) < 0 ||
			::GetKeyState(VK_RWIN) < 0)  result |= WinModifier;
		return result;
	}
	
	void generateStyleFlags(unsigned int flags, DWORD* winStyleOut, DWORD* winExStyleOut);
	LRESULT MApplicationData::windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if(customWndProc)
		{
			LRESULT result;
			if(customWndProc(hwnd,msg,wparam,lparam,&result))
				return result;
		}

		switch(msg)
		{
			case WM_DESTROY:
				// Check if the user wants to quit when the last window is closed.
				if(instance->topLevelWindows.size() == 0 && instance->quitOnLastWindowClosed)
					mApp->exit(0);
				return 0;
		}

		MWidget* window = instance->findWidgetByHandle(hwnd);
		if(window == 0) return DefWindowProcW(hwnd,msg,wparam,lparam);
		WindowExtras* xtr = window->m_windowExtras;
		switch(msg) {

		case WM_SETCURSOR:
			return (LOWORD(lparam) == HTCLIENT) ? TRUE : DefWindowProcW(hwnd,msg,wparam,lparam);
		case WM_MOUSEHOVER:
			{
				if(xtr->widgetUnderMouse != 0)
				{
					MToolTip* tip = xtr->widgetUnderMouse->getToolTip();
					if(tip && tip != instance->currentToolTip)
					{
						RECT rect;
						GetWindowRect(hwnd,&rect);
						showToolTip(tip, rect.left + xtr->lastMouseX, rect.right + xtr->lastMouseY);
					}
				}
				xtr->bTrackingMouse = false;
			}
			break;
		case WM_MOUSEMOVE:
			{
				if(!window->m_windowExtras->bTrackingMouse)
				{
					TRACKMOUSEEVENT tme = {0};
					tme.cbSize = sizeof(TRACKMOUSEEVENT);
					tme.dwFlags = TME_HOVER | TME_LEAVE;
					tme.hwndTrack = hwnd;
					tme.dwHoverTime = 400; // Remark: Set this value to a proper one.
					TrackMouseEvent(&tme);
					window->m_windowExtras->bTrackingMouse = true;
				}

				int xpos = xtr->lastMouseX = GET_X_LPARAM(lparam);
				int ypos = xtr->lastMouseY = GET_Y_LPARAM(lparam);

				// We could first check if the mouse is still over the last widget.
				MWidget* cw = 0;
				if(xpos >= 0 && ypos >= 0 && 
					(unsigned int)xpos <= window->width && 
					(unsigned int)ypos <= window->height)
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
							cw->mouseMoveEvent(&me);
							me.offsetPos(cw->x,cw->y);
							cw = cw->m_parent;
						} while (cw && !me.isAccepted() &&
								cw->testAttributes(WA_NoMousePropagation));
					}
				}
				xtr->widgetUnderMouse = cw;
				// Hide tooltip.
				MToolTip* thisTT = cw == 0 ? 0 : cw->getToolTip();
				if(instance->currentToolTip != thisTT)
					hideToolTip();
				else if(thisTT != 0 && thisTT->hidePolicy() == MToolTip::WhenMove)
					hideToolTip();
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
				if(!invoker)
				{
					while(fw != 0 && !accepted) {
						if(fw->focusPolicy() != NoFocus)
						{
							// We don't check the modifiers when receive keyup event.
							// But we still want to know if the key is in the keypad.
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
				if(!accepted) {
					if(!invoker) {
						for(int i = scs.size() - 1; i>=0; --i) {
							MShortCut* sc = scs.at(i);
							if(sc->getTarget() == window) {
								invoker = sc;
								break;
							}
						}
					}
					if(!invoker) {
						for(int i = scs.size() - 1; i>=0; --i) {
							MShortCut* sc = scs.at(i);
							if(sc->getTarget() == 0) {
								invoker = sc;
								break;
							}
						}
					}
				}
				if(invoker)
					invoker->invoked();
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
		case WM_PAINT:
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

					window->m_windowExtras->addToRepaintMap(window,
						updateRect.left,updateRect.right,updateRect.top,updateRect.bottom);

				}
				window->drawWindow();
				ValidateRect(hwnd,0);
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
				hideToolTip();
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
				hideToolTip();
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
		case WM_MOVE:
			window->x = LOWORD(lparam);
			window->y = HIWORD(lparam);
			break;
		case WM_SIZE:
			if(wparam == SIZE_MAXIMIZED)
				window->m_windowState = WindowMaximized;
			else if(wparam == SIZE_MINIMIZED)
				window->m_windowState = WindowMinimized;
			window->width = LOWORD(lparam);
			window->height = HIWORD(lparam);
			xtr->m_renderTarget->Resize(D2D1::SizeU(LOWORD(lparam),HIWORD(lparam)));
			break;
		case WM_NCHITTEST:
			{
				LRESULT result;
				if(!::DwmDefWindowProc(hwnd,msg,wparam,lparam, &result))
					result = DefWindowProcW(hwnd,msg,wparam,lparam);
				switch(result)
				{
					case HTTOP:
					case HTBOTTOM:
						if(window->maxHeight == window->minHeight)
							return HTBORDER;
					case HTLEFT:
					case HTRIGHT:
						if(window->maxWidth == window->maxHeight)
							return HTBORDER;
					case HTBOTTOMLEFT:
					case HTBOTTOMRIGHT:
					case HTTOPLEFT:
					case HTTOPRIGHT:
						if(window->maxHeight == window->minHeight ||
							window->maxWidth == window->maxHeight)
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
				info->ptMaxTrackSize.x = window->maxWidth + dx;
				info->ptMaxTrackSize.y = window->maxHeight + dy;
				info->ptMinTrackSize.x = window->minWidth + dx;
				info->ptMinTrackSize.y = window->maxHeight + dy;
			}
			break;

		default: return DefWindowProcW(hwnd,msg,wparam,lparam);
		}

		return 0;
	}







	// ========== MApplicationd ==========
	MApplication* MApplication::s_instance = 0;
	MApplication::MApplication(bool hwAccelerated):
		mImpl(new MApplicationData(hwAccelerated))
	{
		M_ASSERT_X(s_instance==0, "Only one MApplication instance allowed", "MApplication::MApplication()");
		ENSURE_IN_MAIN_THREAD;

		s_instance = this;
		mImpl->appHandle = GetModuleHandleW(NULL);

		// DPI
		HDC dc = ::GetDC(0);
		windowsDPI = ::GetDeviceCaps(dc,LOGPIXELSY);
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
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create D2D1Factory. This is a fatal problem.", "MApplicationData()");

		// DWrite
		DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
				__uuidof(IDWriteFactory),
				reinterpret_cast<IUnknown**>(&mImpl->dwriteFactory));
		
		// WIC
		hr = CoCreateInstance(CLSID_WICImagingFactory,NULL,
							  CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&(mImpl->wicFactory)));
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create WIC Component. This is a fatal problem.", "MApplicationData()");

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
		CoUninitialize();
		s_instance = 0;
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
		wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = MApplicationData::windowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = mImpl->appHandle;
		wc.hIcon = 0;
		wc.hCursor = gArrowCursor.getHandle();
		wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
		wc.lpszMenuName = 0;
		wc.lpszClassName = gMWidgetClassName;
	}

	int MApplication::exec()
	{
		MSG msg;
		int result;

		while( (result = GetMessageW(&msg, 0, 0, 0)) != 0)
		{
			if(result == -1) // GetMessage Error 
				break;

			if(msg.message == WM_HOTKEY)
			{
				std::vector<MShortCut*> scs = MShortCut::getMachedShortCuts(
					LOWORD(msg.lParam),HIWORD(msg.lParam));
				for(int i = scs.size() - 1; i >= 0; --i)
				{
					scs.at(i)->invoked();
				}
			} else
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		aboutToQuit();
		return result;
	}













	// ========== MWidget ==========
	MWidget::MWidget(MWidget* parent)
		: m_parent(parent),
		m_windowExtras(0),
		x(200),y(200),
		width(640),height(480),
		minWidth(0),minHeight(0),
		maxWidth(0xffffffff),maxHeight(0xffffffff),
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
				(unsigned int)px <= child->x + child->width &&
				(unsigned int)py <= child->y + child->height)
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
		if(m_topLevelParent->m_windowExtras->focusedWidget == this)
			m_topLevelParent->m_windowExtras->focusedWidget = 0;

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

		if(m_toolTip)
		{
			MApplicationData::hideToolTip(m_toolTip);
			delete m_toolTip;
		}
		delete m_cursor;

		mApp->getStyleSheet()->setWidgetSS(this,std::wstring());
		mApp->getStyleSheet()->removeCache(this);


		// Destroy window
		if(hasWindow())
		{
			if(m_windowExtras->m_layeredWndHandle != NULL)
				DestroyWindow(m_windowExtras->m_layeredWndHandle);
			MApplicationData::removeTLW(this);
			DestroyWindow(m_windowExtras->m_wndHandle);
		}
		delete m_windowExtras;
	}

	bool MWidget::hasWindow() const
	{
		if(m_windowExtras != 0)
			if(m_windowExtras->m_wndHandle != NULL)
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
			SetWindowTextW(m_windowExtras->m_wndHandle,t.c_str());
	}

	HWND MWidget::windowHandle() const 
	{
		if(m_topLevelParent == this) {
			if(m_windowExtras == 0)
				return NULL;
			else
				return m_windowExtras->m_wndHandle;
		} else {
			return m_topLevelParent->windowHandle();
		}
	}

	ID2D1RenderTarget* MWidget::getRenderTarget() 
	{
		if(m_topLevelParent == this) {
			if(m_windowExtras != 0) {
				return m_windowExtras->m_rtHook ? 
					m_windowExtras->m_rtHook : m_windowExtras->m_renderTarget;
			} else
				return 0;
		} else {
			return m_topLevelParent->getRenderTarget();
		}
	}

	void MWidget::closeWindow() 
	{
		if(hasWindow())
			SendMessage(m_windowExtras->m_wndHandle,WM_CLOSE,0,0);
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

		if((flags &= 0xFE) == 0) // WF_Widget || WF_Window
		{
			if(!customized)
				winStyle |= WS_TILEDWINDOW;
		}else if(flags & WF_Popup)
			winStyle |= WS_POPUP;
		else // tool & dialog
		{
			winStyle |= WS_SYSMENU;
			if(flags & WF_Tool)
				winExStyle |= WS_EX_TOOLWINDOW;
		}

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

		SetWindowPos(wndHandle,zpos,0,0,0,0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (zpos == HWND_NOTOPMOST ? SWP_NOZORDER : 0));

		// Set the layeredWindow if there's any.
		wndHandle = m_windowExtras->m_layeredWndHandle;
		if(wndHandle != NULL) {
			SetWindowLongPtrW(wndHandle,GWL_STYLE,winStyle);
			if(winExStyle != 0)
				SetWindowLongPtrW(wndHandle,GWL_EXSTYLE,winExStyle);

			SetWindowPos(wndHandle,zpos,0,0,0,0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (zpos == HWND_NOTOPMOST ? SWP_NOZORDER : 0));
		}
	}

	void MWidget::createRenderTarget()
	{
		M_ASSERT(m_windowExtras != 0);
		mWarning(m_windowExtras->m_renderTarget != 0, L"We don't remember to release the created renderTarget");
		SafeRelease(m_windowExtras->m_renderTarget);
		// Create renderTarget for this window.
		D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
		D2D1_SIZE_U s;
		s.width  = width;
		s.height = height;
		p.usage  = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
		p.type   = mApp->isHardwareAccerated() ? 
					D2D1_RENDER_TARGET_TYPE_HARDWARE : D2D1_RENDER_TARGET_TYPE_SOFTWARE;
		p.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
		p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

		mApp->getD2D1Factory()->CreateHwndRenderTarget(p,
					D2D1::HwndRenderTargetProperties(m_windowExtras->m_wndHandle,s),
					&(m_windowExtras->m_renderTarget));
	}

	void MWidget::destroyWnd()
	{
		if(!hasWindow())
			return;

		if(m_windowExtras->m_layeredWndHandle != NULL) {
			DestroyWindow(m_windowExtras->m_layeredWndHandle);
			m_windowExtras->m_layeredWndHandle = NULL;
		}

		SafeRelease(m_windowExtras->m_renderTarget);

		// We destroy the window first, then remove it from the
		// TLW collection, so that MApplication won't exit even if
		// this is the last opened window.
		DestroyWindow(m_windowExtras->m_wndHandle);
		m_windowExtras->m_wndHandle = NULL;
		MApplicationData::removeTLW(this);

		m_widgetState |= MWS_Hidden;
	}

	void MWidget::createWnd()
	{
		if(hasWindow()) {
			mWarning(true,L"A window for this widget already exists! In MWidget::createWnd()");
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

		AdjustWindowRectEx(&rect,winStyle,false,winExStyle);
		m_windowExtras->m_wndHandle = CreateWindowExW(winExStyle,
			gMWidgetClassName,
			m_windowExtras->m_windowTitle.c_str(),
			winStyle,
			x,y, // Remark: we should offset a bit, due to the border frame.
			rect.right - rect.left, // Width
			rect.bottom - rect.top, // Height
			parentHandle,NULL,
			mApp->getAppHandle(), NULL);

		if(isLayered) {
			m_windowExtras->m_layeredWndHandle = CreateWindowExW(winExStyle & WS_EX_LAYERED,
				gMWidgetClassName,
				m_windowExtras->m_windowTitle.c_str(),
				winStyle,
				x,y,
				rect.right - rect.left, // Width
				rect.bottom - rect.top, // Height
				NULL,NULL,
				mApp->getAppHandle(), NULL);
		}

		createRenderTarget();
	}

	void MWidget::setTopLevelParentRecursively(MWidget* w)
	{
		m_topLevelParent = w;
		MWidgetList::iterator iter = m_children.begin();
		MWidgetList::iterator iterEnd = m_children.end();
		while(iter != iterEnd)
		{
			(*iter)->setTopLevelParentRecursively(w);
			++iter;
		}
	}

	void MWidget::setParent(MWidget* parent)
	{
		if(parent == m_parent)
			return;
		if(m_windowFlags & WF_Window)
		{
			mDebug(L"This is a Window. If you want to set the owner of this window, use setWindowOwner()");
			return;
		}

		if(m_parent == 0)
		{
			// We have created a Window for this widget, we need to destroy it.
			if(m_windowExtras != 0)
			{
				if(m_windowExtras->m_layeredWndHandle != NULL)
					DestroyWindow(m_windowExtras->m_layeredWndHandle);
				if(m_windowExtras->m_wndHandle != NULL)
					DestroyWindow(m_windowExtras->m_wndHandle);
				MApplicationData::removeTLW(this);
				delete m_windowExtras;
				m_windowExtras = 0;
			}
		} else {
			m_parent->repaint(x,y,width,height); // Update the region of this widget inside the old parent.
			m_parent->m_children.remove(this);
		}

		// Setting a widget's parent to 0 makes it hide.
		MWidget* tlp = parent;
		if(parent == 0)
		{
			m_widgetState |= MWS_Hidden;
			if(!testAttributes(WA_ConstStyleSheet))
				m_widgetState &= (~MWS_Polished); // We need to repolish the widget.
			tlp = this;
		} else {
			// We don't modify the widget's hidden state if it changes parent.
			parent->m_children.push_back(this);
			repaint(); // Update the region of this widget inside the new parent.
		}

		setTopLevelParentRecursively(tlp);
		m_parent = parent;
	}

	void MWidget::setMinimumSize(unsigned int w, unsigned int h)
	{
		if(w > maxWidth)
			w = maxWidth;
		if(h > maxHeight)
			h = maxHeight;

		minWidth = w;
		minHeight = h;

		w = width  < minWidth  ? minWidth  : width;
		h = height < minHeight ? minHeight : height;
		resize(w,h);
	}

	void MWidget::setMaximumSize(unsigned int w, unsigned int h)
	{
		if(w < minWidth)
			w = minWidth;
		if(h < minHeight)
			w = minHeight;

		maxWidth = w;
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
		if(m_widgetState & MWS_Polished)
			return;
		m_widgetState |= MWS_Polished;

		if(m_attributes & WA_NoStyleSheet)
			return;

		MStyleSheetStyle* sss = mApp->getStyleSheet();
		sss->polish(this);
	}

	void MWidget::ssSetOpaque(bool opaque)
		{ setWidgetState(MWS_CSSOpaqueBG,opaque); }

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
			DestroyWindow(m_windowExtras->m_wndHandle);
			m_windowExtras->m_wndHandle = NULL;
			SafeRelease(m_windowExtras->m_renderTarget);
			createWnd();
			
			if(!isHidden())
			{
				ShowWindow(m_windowExtras->m_wndHandle,SW_SHOW);
				UpdateWindow(m_windowExtras->m_wndHandle);
			}
		}
	}

	void MWidget::showMinimized()
	{
		if(hasWindow() && m_windowState != WindowMinimized) {
			m_windowState = WindowMinimized;
			ShowWindow(m_windowExtras->getRealHwnd(), SW_MINIMIZE);
		}
	}

	void MWidget::showMaximized()
	{
		if(isWindow() && m_windowState != WindowMaximized) {
			m_windowState = WindowMaximized;
			ShowWindow(m_windowExtras->m_wndHandle,SW_MAXIMIZE);
			if(m_windowFlags & WF_AllowTransparency) {
				RECT rect = {0,0,width,height};
				DWORD winStyle = 0;
				DWORD winExStyle = 0;
				generateStyleFlags(m_windowFlags,&winStyle,&winExStyle);
				AdjustWindowRectEx(&rect,winStyle,false,winExStyle);
				MoveWindow(m_windowExtras->m_layeredWndHandle,
							x,y,rect.right - rect.left,rect.bottom - rect.top,false);
			}
		}
	}

	void MWidget::show()
	{
		// If this has window handle (i.e. WF_Window or parentless shown WF_Widget )
		if(hasWindow())
		{
			HWND hwnd = m_windowExtras->getRealHwnd();
			if(IsWindowVisible(hwnd))
				return;
			ShowWindow(hwnd,SW_SHOW);
			if(m_windowFlags & WF_AlwaysOnBottom)
				SetWindowPos(hwnd,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
			UpdateWindow(m_windowExtras->m_wndHandle);

			return;
		}
		
		if(!isHidden())
			return;

		// If this is a window, we have to create the window first
		// before polishing stylesheet
		if((m_windowFlags & WF_Window) || m_parent == 0) {
			createWnd();

			// In case the stylesheet changed the window's geometry.
			RECT windowGeo = {x,y,width,height};
			ensurePolished();
			RECT newWindowGeo = {x,y,width,height};

			if(hasWindow()) {
				HWND hwnd = windowHandle();
				if(memcmp(&windowGeo,&newWindowGeo,sizeof(RECT))) {
					RECT currentRect;
					GetWindowRect(hwnd,&currentRect);
					int xOffset = newWindowGeo.left - windowGeo.left;
					int yOffset = newWindowGeo.top - windowGeo.top;
					int wdelta  = newWindowGeo.right - windowGeo.right;
					int ydelta  = newWindowGeo.bottom - windowGeo.bottom;
					currentRect.right  = currentRect.right - currentRect.left + wdelta;
					currentRect.bottom = currentRect.bottom - currentRect.top + ydelta;
					currentRect.left  += xOffset;
					currentRect.top   += yOffset;
					SetWindowPos(hwnd,HWND_NOTOPMOST,currentRect.left,
						currentRect.top,currentRect.right,currentRect.bottom, SWP_NOZORDER);
					if(wdelta != 0 || ydelta != 0)
						m_windowExtras->m_renderTarget->Resize(D2D1::SizeU(width,height));
				}
				ShowWindow(windowHandle(),SW_SHOW);
			}
		} else {
			ensurePolished();
		}

		// Mark the widget visible. When polishing stylesheet, this is still 
		// hidden, so even if this widget's size changed, it won't repaint itself.
		m_widgetState &= (~MWS_Hidden);
		repaint();
	}

	void MWidget::hide()
	{
		if(isHidden())
			return;
		m_widgetState |= MWS_Hidden;
		m_widgetState &= (~MWS_Visible);
		if(hasWindow())
			ShowWindow(m_windowExtras->getRealHwnd(),SW_HIDE);
		else
			m_parent->repaint(x,y,width,height);
	}

	void MWidget::setGeometry(int vx, int vy, unsigned int vwidth, unsigned int vheight)
	{
		if(vx == x && vy == y && vwidth == width && vheight == height)
			return;

		// Ensure the size is in a valid range.
		if(vwidth < minWidth)
			vwidth = minWidth;
		else if(vwidth > maxWidth)
			vwidth = maxWidth;
		if(vheight < minHeight)
			vheight = minHeight;
		else if(vheight > maxHeight)
			vheight = maxHeight;

		if(isHidden()) {
			x      = vx;
			y      = vy;
			width  = vwidth;
			height = vheight;
			return;
		}

		if(m_parent != 0) {
			// If is a child widget, we need to update the old region within the parent.
			m_parent->repaint(x,y,width,height);
			x      = vx;
			y      = vy;
			width  = vwidth;
			height = vheight;
			// Update the new rect.
			repaint();
		} else {
			// It's a window. We change the size of it. And Windows will repaint it automatically.
			RECT rect = {0,0,width,height};
			DWORD winStyle = 0;
			DWORD winExStyle = 0;
			generateStyleFlags(m_windowFlags,&winStyle,&winExStyle);
			AdjustWindowRectEx(&rect,winStyle,false,winExStyle);
			if(m_windowExtras->m_layeredWndHandle != NULL)
				MoveWindow(m_windowExtras->m_layeredWndHandle,x,y,rect.right - rect.left,rect.bottom - rect.top,true);
			MoveWindow(m_windowExtras->m_wndHandle,x,y,rect.right - rect.left,rect.bottom - rect.top,true);
		}
	}

	bool MWidget::isVisible() const
	{
		bool selfVisible = 0 != (m_widgetState & MWS_Visible);
		if(!selfVisible)
			return false;

		if(m_parent == 0)
			return true;

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
		if(windowHandle() == NULL)
			return;

		if(isHidden())
			return;

		int right  = ax + aw;
		int bottom = ay + ah;
		if(ax >= (int)width || ay >= (int)height || right <= 0 || bottom <= 0)
			return;

		right  = min(right, (int)width);
		bottom = min(bottom,(int)height);

		// If the widget needs to draw itself, we marks it invisible at this time,
		// And if it does draw itself during next redrawing. It's visible.
		m_widgetState &= (~MWS_Visible);

		MWidget* parent = m_parent;
		while(parent != 0)
		{
			if(parent->isHidden())
				return;
			parent = parent->m_parent;
		}
		m_topLevelParent->m_windowExtras->addToRepaintMap(this, ax, right, ay, bottom);

		// Tells Windows that we need to update, we don't care the clip region.
		// We do it in our own way.
		RECT rect = {0,0,1,1};
		InvalidateRect(windowHandle(),&rect,false);
		GetUpdateRect(windowHandle(),&rect,false);
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

	// Windows has told us that we can now repaint the window.
	void MWidget::drawWindow()
	{
		M_ASSERT(isWindow());

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
				continue;
			}

			RECT urForNonOpaque = uRect;

			MWidget* pc = uTarget;
			MWidget* pp = pc->m_parent;
			bool outsideOfParent          = false;
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
						if(cw->isHidden()) continue;

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
				if(urForNonOpaque.right  > (int)pp->width)  { urForNonOpaque.right  = pp->width;  }
				if(urForNonOpaque.bottom > (int)pp->height) { urForNonOpaque.bottom = pp->height; }
				if(urForNonOpaque.left   < 0)               { urForNonOpaque.left   = 0;          }
				if(urForNonOpaque.top    < 0)               { urForNonOpaque.top    = 0;          }

				MWidgetList::reverse_iterator chrIter    = pp->m_children.rbegin();
				MWidgetList::reverse_iterator chrIterEnd = pp->m_children.rend();
				while(chrIter != chrIterEnd && *(chrIter++) != pc) {}

				while(chrIter != chrIterEnd)
				{
					MWidget* cw      = *(chrIter++);
					if(cw->isHidden()) continue;

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
// 		ID2D1BitmapRenderTarget* layeredWndRT = 0;
// 		if(m_windowExtras->m_layeredWndHandle != NULL)
// 		{
// 			result = m_windowExtras->m_renderTarget->CreateCompatibleRenderTarget(&layeredWndRT);
// 			if(FAILED(result)) {
// 				passiveUpdateWidgets.clear();
// 				childUpdatedHash.clear();
// 				return;
// 			}
// 			layeredWndRT->QueryInterface(&(m_windowExtras->m_rtHook));
// 		}

		int retryTimes = 3;
		do
		{
			getRenderTarget()->BeginDraw();
			draw(0,0,passiveUpdateWidgets.find(this) != passiveUpdateWidgets.end());
			result = getRenderTarget()->EndDraw();

			if(result == D2DERR_RECREATE_TARGET)
			{
// 				createRenderTarget();
// 				mApp->getStyleSheet()->recreateResources(this);
			}
		} while (FAILED(result) && (--retryTimes >= 0));


// 		if(layeredWndRT) {
// 			ID2D1GdiInteropRenderTarget* gdiRT;
// 			layeredWndRT->QueryInterface(&gdiRT);
// 			HDC dc;
// 			gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,&dc);
// 			UpdateLayeredWindow(m_windowExtras->getRealHwnd(),NULL,NULL,NULL,dc,NULL,RGB(0,0,0),NULL,ULW_OPAQUE);
// 			gdiRT->ReleaseDC(NULL);
// 			gdiRT->Release();
// 			layeredWndRT->Release();
// 			m_windowExtras->m_rtHook->Release();
// 			m_windowExtras->m_rtHook = NULL;
// 		}

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
