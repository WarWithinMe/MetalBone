#include "MApplication.h"
#include "MWidget.h"
#include "MResource.h"
#include "private/MWidget_p.h"
#include "private/MApp_p.h"

#ifdef MB_USE_D2D
#  include <d2d1.h>
#  include <dwrite.h>
#  include <wincodec.h>
#endif

#include <fstream>

namespace MetalBone
{
    extern wchar_t gMWidgetClassName[] = L"MetalBone Widget";
    extern MCursor gArrowCursor(MCursor::ArrowCursor);

    // ========== MApplication Impl ==========
    MApplication* MApplication::s_instance = 0;
    MApplication::MApplication(GraphicsBackend backend, bool hwAccelerated):
        mImpl(new MApplicationData()),
        graphicsBackend(backend),
        hardwareAccelerated(hwAccelerated)
    {
        ENSURE_IN_MAIN_THREAD;
        M_ASSERT_X(s_instance == 0, "Only one MApplication instance allowed",
            "MApplication::MApplication()");

#ifndef MB_USE_D2D
        // Make sure the user do not select Direct2D when compiling without it
        if(backend == Direct2D) graphicsBackend = GDI;
#endif
#ifndef MB_USE_SKIA
        if(backend == Skia) graphicsBackend = GDI;
#endif

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

#ifdef MB_USE_D2D
        if(graphicsBackend == Direct2D)
        {
            // COM
            HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
            M_ASSERT_X(SUCCEEDED(hr), "Cannot initialize COM!", "MApplicationData()");
            // D2D
#  ifdef MB_DEBUG_D2D
            D2D1_FACTORY_OPTIONS opts;
            opts.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
            hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, opts, &(mImpl->d2d1Factory));
#  else
            hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &(mImpl->d2d1Factory));
#  endif
            M_ASSERT_X(SUCCEEDED(hr), "Cannot create D2D1Factory. FATAL!", "MApplication()");
            // DWrite
            DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(&mImpl->dwriteFactory));
            // WIC
            hr = CoCreateInstance(CLSID_WICImagingFactory,NULL,
                CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&(mImpl->wicFactory)));
            M_ASSERT_X(SUCCEEDED(hr), "Cannot create WIC Component. FATAL!", "MApplication()");
        }
#endif
    }

    MApplication::~MApplication()
    {
#ifdef MB_USE_D2D
        if(graphicsBackend == Direct2D)
        {
            SafeRelease(mImpl->wicFactory);
            SafeRelease(mImpl->d2d1Factory);
            SafeRelease(mImpl->dwriteFactory);
        }
#endif

        delete mImpl;
        MApplicationData::instance = 0;
        s_instance = 0;

#ifdef MB_USE_D2D
        CoUninitialize();
#endif
    }

#ifdef MB_USE_D2D
    ID2D1Factory*             MApplication::getD2D1Factory()        { return mImpl->d2d1Factory;     }
    IDWriteFactory*           MApplication::getDWriteFactory()      { return mImpl->dwriteFactory;   }
    IWICImagingFactory*       MApplication::getWICImagingFactory()  { return mImpl->wicFactory;      }
#endif

    const std::set<MWidget*>& MApplication::topLevelWindows() const { return mImpl->topLevelWindows; }
    HINSTANCE                 MApplication::getAppHandle()    const { return mImpl->appHandle;       }
    MStyleSheetStyle*         MApplication::getStyleSheet()         { return &(mImpl->ssstyle);      }
    void  MApplication::setStyleSheet(const std::wstring& css)      { mImpl->ssstyle.setAppSS(css);  }
    void  MApplication::setCustomWindowProc(WinProc proc)           { mImpl->customWndProc = proc;   }

    void MApplication::loadStyleSheetFromFile(const std::wstring& path, bool isAscii)
    {
        if(isAscii)
        {
            std::wifstream cssReader;
            cssReader.imbue(std::locale(".936"));
            cssReader.open(path.c_str(),std::ios_base::in);
            std::wstring wss((std::istreambuf_iterator<wchar_t>(cssReader)),
                std::istreambuf_iterator<wchar_t>());
            setStyleSheet(wss);
        } else
        {
            // UTF-8
            HANDLE specFile = ::CreateFileW(path.c_str(),GENERIC_READ,
                0,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);

            if(specFile == INVALID_HANDLE_VALUE) return;

            DWORD size = ::GetFileSize(specFile, 0);
            DWORD bytesRead = 0;
            char* buffer = new char[size];

            ::ReadFile(specFile, buffer, size, &bytesRead, 0);
            ::CloseHandle(specFile);

            bool hasBOM = false;
            if(bytesRead >= 3 && buffer[0] == (char)0xEF &&
                buffer[1] == (char)0xBB && buffer[2] == (char)0xBF)
            { hasBOM = true; bytesRead -= 3; }

            if(bytesRead == 0) return;

            wchar_t* strBuf = new wchar_t[bytesRead];
            bytesRead = ::MultiByteToWideChar(CP_UTF8,0, hasBOM ? buffer+3 : buffer,
                bytesRead, strBuf, bytesRead);
            std::wstring wss(strBuf, bytesRead);
            setStyleSheet(wss);
        }
        
    }

    void MApplication::setupRegisterClass(WNDCLASSW& wc)
    {
        wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = MApplicationData::windowProc;
        wc.hInstance     = mImpl->appHandle;
        wc.hCursor       = gArrowCursor.getHandle();
        wc.hbrBackground = NULL;
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
            oldFocus->updateSSAppearance();
        }

        if(w != 0) {
            w->setWidgetState(MWS_Focused,true);
            w->updateSSAppearance();
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

    void mapWindowCoorToChild(MWidget* child, int* xpos, int* ypos)
    {
        MWidget* w = child;
        while(w->parent() != 0)
        {
            *xpos -= w->x();
            *ypos -= w->y();
            w = w->parent();
        }
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

        case WM_ERASEBKGND:
            return 1;
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

                    window->m_windowExtras->addToRepaintMap(window,0,0,window->l_width,window->l_height);
                }
            }

            window->drawWindow();
            ::ValidateRect(hwnd,0);
            return 0;
        case WM_MOVE:
            window = instance->findWidgetByHandle<true>(hwnd);
            if (window == 0) RET_DEFPROC;

            // These are client-rect screen coordinates.
            long newX = (short)LOWORD(lparam);
            long newY = (short)HIWORD(lparam);

            if(window->m_windowFlags & WF_AllowTransparency)
            {
                window->l_x = newX;
                window->l_y = newY;

                BLENDFUNCTION blend = {AC_SRC_OVER,0,255,AC_SRC_ALPHA};
                POINT windowPos = { newX, newY };
                UPDATELAYEREDWINDOWINFO info = {};
                info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
                info.dwFlags  = ULW_ALPHA;
                info.pblend   = &blend;
                info.pptDst   = &windowPos;
                ::UpdateLayeredWindowIndirect(window->m_windowExtras->m_wndHandle,&info);
            } else
            {
                RECT  wndRect     = { newX, newY, newX, newY};
                DWORD winStyle    = 0;
                DWORD winExStyle  = 0;
                generateStyleFlags(window->m_windowFlags, &winStyle, &winExStyle);
                ::AdjustWindowRectEx(&wndRect, winStyle, false, winExStyle);
                window->l_x = wndRect.left;
                window->l_y = wndRect.right;
            }
            return 0;
        }

        window = instance->findWidgetByHandle<false>(hwnd);
        if(window == 0) RET_DEFPROC;
        WindowExtras* xtr = window->m_windowExtras;

        switch(msg) {

        case WM_NCHITTEST:
            {
                LRESULT result = ::DefWindowProcW(hwnd,msg,wparam,lparam);
                switch(result)
                {
                case HTTOP:
                case HTBOTTOM:
                    if(window->l_maxHeight == window->l_minHeight)
                        return HTBORDER;
                    break;
                case HTLEFT:
                case HTRIGHT:
                    if(window->l_maxWidth == window->l_minWidth)
                        return HTBORDER;
                    break;
                case HTBOTTOMLEFT:
                case HTBOTTOMRIGHT:
                case HTTOPLEFT:
                case HTTOPRIGHT:
                    if(window->l_maxHeight == window->l_minHeight ||
                        window->l_maxWidth == window->l_minWidth)
                        return HTBORDER;
                    break;
                case HTCLIENT:
                    {
                        // Check if the widget under the mouse is to
                        // simulate the Non-Client area.
                        
                        MWidget* currWidgetUnderMouse = xtr->widgetUnderMouse;
                        int xpos = GET_X_LPARAM(lparam) - window->x();
                        int ypos = GET_Y_LPARAM(lparam) - window->y();

                        // We will have to find the widget every time, if the
                        // widget is not WR_Normal. Because we do not store its
                        // position.
                        if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
                            { currWidgetUnderMouse = window->findWidget(xpos, ypos); }

                        if(currWidgetUnderMouse == 0)
                        {
                            xtr->currWidgetUnderMouse = 0;
                            return HTNOWHERE;
                        }

                        if(currWidgetUnderMouse->widgetRole() == WR_Normal)
                        {
                            xtr->currWidgetUnderMouse = currWidgetUnderMouse;
                            return HTCLIENT;
                        } else
                        {
                            // We will receive WM_MOUSELEAVE if we were in the
                            // client area before.
                            // *WidgetRole is minus 2 to fit in the size of CHAR.
                            xtr->currWidgetUnderMouse = 0;
                            return 2 + currWidgetUnderMouse->widgetRole();
                        }
                    }
                }
                return result;
            }
        case WM_MOUSEMOVE:
            {
                // At first, I check the widget under mouse in responding
                // to this message. Then I found that I have to check the
                // widget in WM_NCHITTEST. So now, the widget under mouse
                // must be check before processing this message.
                int xpos = GET_X_LPARAM(lparam);
                int ypos = GET_Y_LPARAM(lparam);

                if(xtr->mouseGrabber)
                {
                    // The xpos and ypos is in the Window Client Area Coordinate.
                    xtr->lastMouseX = xpos;
                    xtr->lastMouseY = ypos;
                    xtr->currWidgetUnderMouse = xtr->mouseGrabber;
                }

                if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
                {
                    xtr->lastMouseX = xpos;
                    xtr->lastMouseY = ypos;
                    xtr->currWidgetUnderMouse = window->findWidget(xpos, ypos);
                }

                // Remark: Sometimes when the mouse is out of the window
                // we will keep receiving WM_SYSTIMER. I suspect that it
                // was caused by TrackMouseEvent. But I don't know how to
                // fix it, or even how it happens.
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

                MWidget* lastWidget = xtr->widgetUnderMouse;
                MWidget* currWidget = xtr->currWidgetUnderMouse;
                if(lastWidget != 0)
                {
                    if(lastWidget != currWidget)
                    {
                        lastWidget->leaveEvent();
                        lastWidget->setWidgetState(MWS_Pressed | MWS_UnderMouse, false);
                        lastWidget->updateSSAppearance();

                        MToolTip* tip = lastWidget->getToolTip();
                        if(tip) tip->hide();
                    }
                } else
                {
                    // The mouse moves into the window, we set it to the default.
                    gArrowCursor.show(true);
                }

                if(currWidget != 0) {

                    int localPosX = xtr->lastMouseX;
                    int localPosY = xtr->lastMouseY;
                    mapWindowCoorToChild(currWidget, &localPosX, &localPosY);

                    bool mouseMove    = true;
                    bool crossBounds  = lastWidget != currWidget;
                    bool insideWidget = (localPosX >=0 && localPosY >=0 &&
                        localPosX <= currWidget->width() && localPosY <= currWidget->height());

                    if(!crossBounds)
                    {
                        if(currWidget->testWidgetState(MWS_UnderMouse))
                            { if(!insideWidget) crossBounds = true; }
                        else if(insideWidget) { crossBounds = true; }
                    }
                    if(crossBounds)
                    {
                        if(insideWidget)
                        {
                            MEvent enter(false);
                            currWidget->enterEvent(&enter);
                            mouseMove = !enter.isAccepted();
                            unsigned int state = MWS_UnderMouse;
                            if((wparam & MK_LBUTTON) != 0) state |= MWS_Pressed;
                            currWidget->setWidgetState(state, true);
                        } else
                        {
                            currWidget->leaveEvent();
                            currWidget->setWidgetState(MWS_UnderMouse | MWS_Pressed, false);
                            MToolTip* tip = currWidget->getToolTip();
                            if(tip) tip->hide();
                        }
                    }
                    currWidget->updateSSAppearance();

                    if(currWidget->focusPolicy() == MoveOverFocus)
                        currWidget->setFocus();

                    if(mouseMove && (wparam != 0 || currWidget->testAttributes(WA_TrackMouseMove)))
                    {
                        MRect rect;
                        ::GetWindowRect(hwnd,&rect);
                        rect.offset(xtr->lastMouseX, xtr->lastMouseY);

                        MMouseEvent me(localPosX, localPosY, rect.left, rect.top, wparam);
                        MWidget* eventWidget = currWidget;
                        do
                        {
                            if(wparam != 0 || eventWidget->testAttributes(WA_TrackMouseMove))
                                { eventWidget->mouseMoveEvent(&me); }

                            me.offsetPos(eventWidget->l_x,eventWidget->l_y);
                            eventWidget = eventWidget->m_parent;

                        } while (eventWidget && !me.isAccepted() &&
                            !eventWidget->testAttributes(WA_NoMousePropagation));
                    }
                }

                MToolTip* thisTT = xtr->widgetUnderMouse == 0 ? 0 : xtr->widgetUnderMouse->getToolTip();
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

                xtr->widgetUnderMouse = currWidget;
                xtr->currWidgetUnderMouse = 0;
            }
            break;
        case WM_SETCURSOR:
            if(LOWORD(lparam) != HTCLIENT)
                ::DefWindowProcW(hwnd,msg,wparam,lparam);
            return TRUE;
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
                // Send NCHITTEST to determine the widget under mouse
                if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
                    ::SendMessage(hwnd,WM_NCHITTEST,wparam,lparam);

                MWidget* cw = xtr->mouseGrabber ? xtr->mouseGrabber : xtr->widgetUnderMouse;
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
                        cw->updateSSAppearance();
                    }

                    MRect rect;
                    ::GetWindowRect(hwnd,&rect);
                    mapWindowCoorToChild(cw, &xpos, &ypos);
                    MMouseEvent me(xpos, ypos,
                        rect.left + xtr->lastMouseX,
                        rect.top  + xtr->lastMouseY, wparam);
                    me.ignore();
                    do {
                        cw->mousePressEvent(&me);
                        me.offsetPos(cw->l_x,cw->l_y);

                        if(me.isAccepted())
                        {
                            cw->grabMouse();
                            break;
                        }
                        cw = cw->m_parent;

                    } while (cw && cw->testAttributes(WA_NoMousePropagation));
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
                // Send NCHITTEST to determine the widget under mouse
                if(xpos != xtr->lastMouseX || ypos != xtr->lastMouseY)
                    ::SendMessage(hwnd,WM_NCHITTEST,wparam,lparam);

                MWidget* cw = xtr->mouseGrabber ? xtr->mouseGrabber : xtr->widgetUnderMouse;
                if(cw != 0)
                {
                    cw->releaseMouse();

                    MouseButton btn = (msg == WM_LBUTTONUP ? LeftButton :
                        (msg == WM_RBUTTONUP ? RightButton : MiddleButton));
                    if(btn == LeftButton) {
                        cw->setWidgetState(MWS_Pressed, false);
                        cw->updateSSAppearance();
                    }

                    MRect rect;
                    ::GetWindowRect(hwnd,&rect);
                    mapWindowCoorToChild(cw, &xpos, &ypos);
                    MMouseEvent me(xpos, ypos,
                        rect.left + xtr->lastMouseX,
                        rect.top  + xtr->lastMouseY, wparam);
                    me.ignore();
                    do {
                        cw->mouseReleaseEvent(&me);
                        me.offsetPos(cw->l_x,cw->l_y);
                        
                        if(me.isAccepted()) break;
                        cw = cw->m_parent;

                    } while (cw && cw->testAttributes(WA_NoMousePropagation));
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

                MWidget* cw = xtr->mouseGrabber ? xtr->mouseGrabber : xtr->widgetUnderMouse;
                if(cw != 0)
                {
                    MouseButton btn = (msg == WM_LBUTTONDBLCLK ? LeftButton :
                        (msg == WM_RBUTTONDBLCLK ? RightButton : MiddleButton));
                    if(btn == LeftButton) {
                        cw->setWidgetState(MWS_Pressed,true);
                        cw->updateSSAppearance();
                    }

                    RECT rect;
                    ::GetWindowRect(hwnd,&rect);
                    mapWindowCoorToChild(cw, &xpos, &ypos);
                    MMouseEvent me(xpos, ypos,
                        rect.left + xtr->lastMouseX,
                        rect.top  + xtr->lastMouseY, wparam);
                    me.ignore();
                    do {
                        cw->mouseDClickEvent(&me);
                        me.offsetPos(cw->l_x,cw->l_y);
                        cw = cw->m_parent;
                    } while (cw && !me.isAccepted() &&
                        cw->testAttributes(WA_NoMousePropagation));
                }
            }
            break;

        case WM_ACTIVATEAPP:
        case WM_MOUSELEAVE:
            {
                bool leaved = msg == WM_MOUSEMOVE ? true : wparam == FALSE;
                if(leaved)
                {
                    if(window->m_windowExtras->bTrackingMouse)
                    {
                        TRACKMOUSEEVENT tme = {0};
                        tme.cbSize      = sizeof(TRACKMOUSEEVENT);
                        tme.dwFlags     = TME_HOVER | TME_LEAVE | TME_CANCEL;
                        tme.hwndTrack   = hwnd;
                        ::TrackMouseEvent(&tme);
                    }
                    window->m_windowExtras->bTrackingMouse = true;

                    xtr->lastMouseX = -1;
                    xtr->lastMouseY = -1;
                    xtr->currWidgetUnderMouse = 0;
                    if(xtr->mouseGrabber) xtr->mouseGrabber->releaseMouse();
                    ::SendMessage(hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(-1,-1));

                    window->m_windowExtras->bTrackingMouse = false;
                }

                if(msg == WM_ACTIVATEAPP) RET_DEFPROC;
            }
            RET_DEFPROC;
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
                window->setWidgetState(MWS_Hidden, true);
                window->m_windowState = WindowMinimized;
                return 0;
            } else if(wparam == SIZE_MAXIMIZED)
                window->m_windowState = WindowMaximized;

            if(window->l_width == LOWORD(lparam) && window->l_height == HIWORD(lparam))
                return 0;
            window->resize(LOWORD(lparam), HIWORD(lparam));
            break;
        case WM_GETMINMAXINFO:
            {
                DWORD winStyle    = 0;
                DWORD winExStyle  = 0;
                // We don't set parent of layered window.
                MRect wndRect(0,0,window->l_width,window->l_height);
                generateStyleFlags(window->m_windowFlags,&winStyle,&winExStyle);
                ::AdjustWindowRectEx(&wndRect,winStyle,false,winExStyle);
                long dx = wndRect.width() - window->l_width;
                long dy = wndRect.height()- window->l_height;

                MINMAXINFO* info  = (MINMAXINFO*)lparam;
                info->ptMaxSize.x = window->l_maxWidth;
                info->ptMaxSize.y = window->l_maxHeight;
                long sum = window->l_maxWidth + dx;
                info->ptMaxTrackSize.x = sum > 0 ? sum : window->l_maxWidth;
                sum = window->l_maxHeight + dy;
                info->ptMaxTrackSize.y = sum > 0 ? sum : window->l_maxHeight;
                info->ptMinTrackSize.x = window->l_minWidth  + dx;
                info->ptMinTrackSize.y = window->l_minHeight + dy;
                break;
            }
        case WM_SHOWWINDOW:
            if(lparam == 0)
                wparam == TRUE ? window->show() : window->hide();
            RET_DEFPROC;
        case WM_NCCALCSIZE:
            // Remark: This message is only received when the size of the
            // window changes. But if we move the window without receiving
            // this message, the window will jump after we stop moving it.
            if(window->windowFlags() & WF_AllowTransparency)
                return 0;

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