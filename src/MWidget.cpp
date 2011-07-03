#include "MWidget.h"
#include "MApplication.h"
#include "MStyleSheet.h"
#include "MResource.h"
#include "MRegion.h"
#include "MGraphics.h"
#include "private/MApp_p.h"
#include "private/MWidget_p.h"
#include "private/MGraphicsData.h"

#include <list>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <tchar.h>
#include <winerror.h>

namespace MetalBone
{
    WindowExtras::~WindowExtras()
    {
        delete m_graphicsData;
        if(m_dummyHandle != NULL)
            ::DestroyWindow(m_dummyHandle);
        ::DestroyWindow(m_wndHandle);
    }

    // ========== MWidget ==========
    MWidget::MWidget(MWidget* parent)
        : m_parent(parent),
        m_windowExtras(0),
        l_x(0),l_y(0),
        l_width(640),l_height(480),
        l_minWidth(0),l_minHeight(0),
        l_maxWidth(LONG_MAX),l_maxHeight(LONG_MAX),
        m_attributes(WA_AutoBG | WA_NonChildOverlap),
        m_windowFlags(WF_Widget),
        m_windowState(WindowNoState),
        m_widgetState(MWS_Hidden),lastPseudo(CSS::PC_Default),
        e_widgetRole(WR_Normal),
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
            if(child->isDisabled() || child->isHidden() ||
                (ignoreMouse && child->testAttributes(WA_MouseThrough)))
                continue;

            if(px >= child->l_x && py >= child->l_y &&
                px <= child->l_x + child->l_width && py <= child->l_y + child->l_height)
            {
                px -= child->l_x;
                py -= child->l_y;
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

    unsigned int MWidget::getWidgetPseudo(bool mark, unsigned int initP)
    {
        unsigned int pseudo = initP;
        // Only set pseudo 'hover' if it's not pressed.
        if(testWidgetState(MWS_Pressed))
            pseudo |= CSS::PC_Pressed;
        else if(testWidgetState(MWS_UnderMouse))
            pseudo |= CSS::PC_Hover;

        if(testWidgetState(MWS_Disabled))
            pseudo |= CSS::PC_Disabled;

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

    std::wstring MWidget::windowTitle() const
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

    void MWidget::closeWindow() 
    {
        if(hasWindow())
            ::PostMessage(m_windowExtras->m_wndHandle,WM_CLOSE,0,0);
    }

    extern void generateStyleFlags(unsigned int flags, DWORD* winStyleOut, DWORD* winExStyleOut)
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
                winStyle |= (WS_POPUP | WS_SIZEBOX); // Must have WS_SIZEBOX to make WidgetRole work.
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
        if(!hasWindow()) return;

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

        m_windowExtras->m_graphicsData->setLayeredWindow((m_windowFlags & WF_AllowTransparency) != 0);
    }

    void MWidget::destroyWnd()
    {
        if(!hasWindow()) return;

        if(m_windowExtras->m_dummyHandle != NULL)
        {
            ::DestroyWindow(m_windowExtras->m_dummyHandle);
            m_windowExtras->m_dummyHandle = NULL;
        }

        delete m_windowExtras->m_graphicsData;
        m_windowExtras->m_graphicsData = 0;

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

        MRect rect(0, 0, l_width, l_height);
        generateStyleFlags(m_windowFlags, &winStyle, &winExStyle);
        if(!isLayered) {
            ::AdjustWindowRectEx(&rect, winStyle, false, winExStyle);
        }

        m_windowExtras->clientAreaPosX = -rect.left;
        m_windowExtras->clientAreaPosY = -rect.top;
        
        m_windowExtras->m_wndHandle = CreateWindowExW(winExStyle,
            gMWidgetClassName,
            m_windowExtras->m_windowTitle.c_str(),
            winStyle,
            l_x,l_y,
            rect.width(), rect.height(),
            parentHandle,NULL,
            mApp->getAppHandle(), NULL);

        if(isLayered)
        {
            m_windowExtras->m_dummyHandle = CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE,
                gMWidgetClassName,
                m_windowExtras->m_windowTitle.c_str(),
                WS_POPUP,
                0, 0, 10, 10,
                parentHandle,NULL,
                mApp->getAppHandle(), NULL);
            ::SetLayeredWindowAttributes(m_windowExtras->m_dummyHandle,0,0,LWA_ALPHA);
            ::SetWindowPos(m_windowExtras->m_wndHandle,HWND_NOTOPMOST,
                0,0,0,0,SWP_NOMOVE|SWP_NOZORDER|SWP_NOSIZE|SWP_FRAMECHANGED);
        }

        m_windowExtras->m_graphicsData = MGraphicsData::create(m_windowExtras->m_wndHandle, l_width, l_height);
        m_windowExtras->m_graphicsData->setLayeredWindow(isLayered);
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
            m_parent->repaint(l_x,l_y,l_width,l_height); // Update the region of this widget inside the old parent.
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
            delete m_windowExtras->m_graphicsData;
            m_windowExtras->m_graphicsData = 0;
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
        if(w > l_maxWidth)  w = l_maxWidth;
        else if(w < 0)      w = 0;
        if(h > l_maxHeight) h = l_maxHeight;
        else if(h < 0)      h = 0;

        l_minWidth  = w;
        l_minHeight = h;

        w = l_width  < l_minWidth  ? l_minWidth  : l_width;
        h = l_height < l_minHeight ? l_minHeight : l_height;
        resize(w,h);
    }

    void MWidget::setMaximumSize(long w, long h)
    {
        if(w < l_minWidth)  w = l_minWidth;
        else if(w < 0)      w = LONG_MAX;
        if(h < l_minHeight) h = l_minHeight;
        else if(h < 0)      h = LONG_MAX;

        l_maxWidth  = w;
        l_maxHeight = h;

        w = l_width  > l_maxWidth  ? l_maxWidth  : l_width;
        h = l_height > l_maxHeight ? l_maxHeight : l_height;
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

    void MWidget::setEnabled(bool enabled)
    {
        bool currentDisabled = (m_widgetState & MWS_Disabled) != 0;
        if(currentDisabled != enabled) return;

        if(enabled)
        {
            m_widgetState &= (~MWS_Disabled);
            updateSSAppearance();
        } else
        {
            unsigned int mask = MWS_UnderMouse | MWS_Focused | MWS_Pressed;
            m_widgetState &= (~mask);
            m_widgetState |= MWS_Disabled;
            updateSSAppearance();
        }
    }

    bool MWidget::isHidden() const
        { return (m_widgetState & MWS_Hidden) != 0; }
    bool MWidget::isDisabled() const
        { return (m_widgetState & MWS_Disabled) != 0; }
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
            m_parent->repaint(l_x,l_y,l_width,l_height);
        }
    }

    void MWidget::grabMouse()
    { 
        if(m_topLevelParent->m_windowExtras) 
            m_topLevelParent->m_windowExtras->grabMouse(this); 
    }
    void MWidget::releaseMouse()
    { 
        if(m_topLevelParent->m_windowExtras) 
            m_topLevelParent->m_windowExtras->releaseMouse(); 
    }

    void MWidget::raise()
    {
        if(m_parent == 0)
        {
            if(hasWindow() && ::IsWindowVisible(m_windowExtras->m_wndHandle))
            {
                ::SetWindowPos(m_windowExtras->m_wndHandle, HWND_TOPMOST, 0,0,0,0,
                    SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW);
            }
            return;
        }

        if(*(m_parent->m_children.begin()) != this)
        {
            m_parent->m_children.remove(this);
            m_parent->m_children.push_front(this);
            m_parent->repaint();
        }
    }

    void MWidget::lower()
    {
        if(m_parent == 0)
        {
            if(hasWindow() && ::IsWindowVisible(m_windowExtras->m_wndHandle))
            {
                ::SetWindowPos(m_windowExtras->m_wndHandle, HWND_BOTTOM, 0,0,0,0,
                    SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOREDRAW);
            }
            return;
        }

        if(*(m_parent->m_children.rbegin()) != this)
        {
            m_parent->m_children.remove(this);
            m_parent->m_children.push_back(this);
            m_parent->repaint();
        }
    }

    void MWidget::setGeometry(long vx, long vy, long vwidth, long vheight)
    {
        // Ensure the size is in a valid range.
        if     (vwidth  < l_minWidth ) { vwidth  = l_minWidth;  }
        else if(vwidth  > l_maxWidth ) { vwidth  = l_maxWidth;  }
        if     (vheight < l_minHeight) { vheight = l_minHeight; } 
        else if(vheight > l_maxHeight) { vheight = l_maxHeight; }

        bool sizeChanged = (vwidth != l_width || vheight != l_height);
        bool posChanged  = (vx != l_x || vy != l_y);

        if(sizeChanged)
        {
            MResizeEvent ev(l_width,l_height,vwidth,vheight);
            l_width  = vwidth;
            l_height = vheight;

            resizeEvent(&ev);
            if(!ev.isAccepted())
            {
                sizeChanged = false;
                l_width  = ev.getOldSize().width();
                l_height = ev.getOldSize().height();
            }
        }

        if(!sizeChanged && !posChanged)
            return;

        if(!isHidden() && m_parent != 0) {
            // If is a child widget, we need to update the old region within the parent.
            m_parent->repaint(l_x,l_y,l_width,l_height);
        }

        l_x = vx;
        l_y = vy;

        if(m_windowExtras != 0)
        {
            MRect rect(0, 0, l_width, l_height);

            if(m_windowFlags & WF_AllowTransparency)
            {
                if(sizeChanged)
                    repaint();
            } else
            {
                DWORD winStyle   = 0;
                DWORD winExStyle = 0;
                generateStyleFlags(m_windowFlags, &winStyle, &winExStyle);
                ::AdjustWindowRectEx(&rect, winStyle, false, winExStyle);

                // Moving a normal window will repaint it automatically.
            }

            ::MoveWindow(m_windowExtras->m_wndHandle, l_x, l_y, rect.width(), rect.height(), true);

            if(m_windowExtras->m_graphicsData)
                m_windowExtras->m_graphicsData->resize(vwidth,vheight);

        } else if(!isHidden())
            repaint(); // Update the new rect.
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
        if(ax >= (int)l_width || ay >= (int)l_height || right <= 0 || bottom <= 0)
            return;

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

        right  = min(right, (int)l_width);
        bottom = min(bottom,(int)l_height);
        m_topLevelParent->m_windowExtras->addToRepaintMap(this, ax, ay, right, bottom);

        // Tells Windows that we need to update, we don't care the clip region.
        // We do it in our own way.
        MRect rect(0,0,1,1);
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

    // Windows has told us that we can now repaint the window.
    void MWidget::drawWindow()
    {
        M_ASSERT(isWindow());

        bool layeredWindow    = (m_windowFlags & WF_AllowTransparency) != 0;
        bool fullWindowUpdate = false;
        MRect windowUpdateRect(l_width,l_height,0,0);

        MGraphics graphics(this);
        m_windowExtras->m_graphicsData->beginDraw();

        // Calculate which widget needs to be redraw.
        DirtyChildrenHash& childUpdatedHash  = m_windowExtras->childUpdatedHash;
        DrawRegionHash& passiveUpdateWidgets = m_windowExtras->passiveUpdateWidgets;
        DrawRectHash& updateWidgets          = m_windowExtras->updateWidgets;
        DrawRectHash::iterator drIter        = updateWidgets.begin();
        DrawRectHash::iterator drIterEnd     = updateWidgets.end();
        while(drIterEnd != drIter)
        {
            MWidget* uTarget = drIter->first;
            MRect&   uRect   = drIter->second;
            ++drIter;

            // If we are dealing with the topLevelWindow itself,
            // we only have to add it the passiaveUpdateWidgets.
            if(uTarget == this) {

                passiveUpdateWidgets[this].addRect(uRect);
                if(layeredWindow && uRect.left == 0 &&
                    uRect.right == l_width && uRect.top == 0 && uRect.bottom == l_height)
                {
                    windowUpdateRect.left   = 0;
                    windowUpdateRect.right  = l_width;
                    windowUpdateRect.top    = 0;
                    windowUpdateRect.bottom = l_height;
                    fullWindowUpdate = true;
                    graphics.clear();
                }
                continue;
            }

            MRect urForNonOpaque = uRect;

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
                uRect.offset(pc->l_x,pc->l_y);
                uTargetUR.offset(pc->l_x,pc->l_y);
                DrawRegionHash::iterator tpwIter    = tempPassiveWidgets.begin();
                DrawRegionHash::iterator tpwIterEnd = tempPassiveWidgets.end();
                for(; tpwIterEnd != tpwIter; ++tpwIter)
                    tpwIter->second.offset(pc->l_x,pc->l_y);

                if(uRect.right  <= 0)                { outsideOfParent = true; break; }
                if(uRect.bottom <= 0)                { outsideOfParent = true; break; }
                if(uRect.left   > (int)pp->l_width ) { outsideOfParent = true; break; }
                if(uRect.top    > (int)pp->l_height) { outsideOfParent = true; break; } 
                // Clip to parent.
                if(uRect.right  > (int)pp->l_width ) { uRect.right  = pp->l_width;  }
                if(uRect.bottom > (int)pp->l_height) { uRect.bottom = pp->l_height; }
                if(uRect.left   < 0)                 { uRect.left   = 0;          }
                if(uRect.top    < 0)                 { uRect.top    = 0;          }

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

                        MRect childRect(cw->l_x, cw->l_y, cw->l_x + cw->l_width, cw->l_y + cw->l_height);
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

            if(!this->isOpaqueDrawing() && !fullWindowUpdate)
            {
                MRect wur;
                uTargetUR.getBounds(wur);
                if(windowUpdateRect.left   > wur.left  ) windowUpdateRect.left   = wur.left;
                if(windowUpdateRect.right  < wur.right ) windowUpdateRect.right  = wur.right;
                if(windowUpdateRect.top    > wur.top   ) windowUpdateRect.top    = wur.top;
                if(windowUpdateRect.bottom < wur.bottom) windowUpdateRect.bottom = wur.bottom;

                graphics.clear(wur);
            }

            MRegion underneathUR(uTargetUR);
            pp = uTarget->m_parent;
            pc = uTarget;

            while(pp != 0 && !isPcOpaque)
            {
                OffsetRect(&urForNonOpaque,pc->l_x,pc->l_y);
                DrawRegionHash::iterator tpwIter    = tempPassiveWidgets.begin();
                DrawRegionHash::iterator tpwIterEnd = tempPassiveWidgets.end();
                for(; tpwIterEnd != tpwIter; ++tpwIter)
                    tpwIter->second.offset(pc->l_x,pc->l_y);

                // Clip to parent.
                if(urForNonOpaque.right  > (int)pp->l_width ) { urForNonOpaque.right  = pp->l_width;  }
                if(urForNonOpaque.bottom > (int)pp->l_height) { urForNonOpaque.bottom = pp->l_height; }
                if(urForNonOpaque.left   < 0)                 { urForNonOpaque.left   = 0;            }
                if(urForNonOpaque.top    < 0)                 { urForNonOpaque.top    = 0;            }

                MWidgetList::reverse_iterator chrIter    = pp->m_children.rbegin();
                MWidgetList::reverse_iterator chrIterEnd = pp->m_children.rend();
                while(chrIter != chrIterEnd && *(chrIter++) != pc) {}

                while(chrIter != chrIterEnd)
                {
                    MWidget* cw = *(chrIter++);
                    if(cw->isHidden() || cw->testAttributes(WA_DontShowOnScreen)) continue;

                    MRect siblingRect(cw->l_x, cw->l_y, cw->l_x + cw->l_width, cw->l_y + cw->l_height);
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

        bool result;
        int retryTimes = 3;
        do
        {
            draw(0,0,passiveUpdateWidgets.find(this) != passiveUpdateWidgets.end());

            // If the update rect is the whole window
            // we need to swap the left/top with right/bottom.
            if(windowUpdateRect.right == 0 && windowUpdateRect.bottom == 0)
            {
                windowUpdateRect.right = windowUpdateRect.left;
                windowUpdateRect.bottom= windowUpdateRect.top;
                windowUpdateRect.left = 0;
                windowUpdateRect.top  = 0;
            }
            result = m_windowExtras->m_graphicsData->endDraw(windowUpdateRect);
        } while (!result && (--retryTimes >= 0));

        m_windowExtras->clearUpdateQueue();
    }

    void MWidget::doStyleSheetDraw(const MRect& wr, const MRect& cr)
        { mApp->getStyleSheet()->draw(this,wr,cr); }

    void MWidget::draw(int xOffsetInWnd, int yOffsetInWnd, bool drawMySelf)
    {
        WindowExtras* tlpWE = m_topLevelParent->m_windowExtras;
        if(drawMySelf)
        {
            MRegion& updateRegion = tlpWE->passiveUpdateWidgets[this];

            // Draw the background with CSS
            if(!testAttributes(WA_NoStyleSheet))
            {
                MRegion::Iterator iter = updateRegion.begin();
                while(iter) {
                    // Draw the background with CSS
                    MRect clipRect;
                    iter.getRect(clipRect);
                    MRect widgetRect(xOffsetInWnd,yOffsetInWnd,
                        xOffsetInWnd+l_width, yOffsetInWnd+l_height);

                    doStyleSheetDraw(widgetRect,clipRect);
                    ++iter;
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
            MWidgetList::iterator chIter    = m_children.begin();
            MWidgetList::iterator chIterEnd = m_children.end();
            MRect childRectInWnd;
            while(chIter != chIterEnd)
            {
                MWidget* child = *chIter;
                ++chIter;
                if(child->isHidden()) continue;

                childRectInWnd.left   = xOffsetInWnd + child->l_x;
                childRectInWnd.top    = yOffsetInWnd + child->l_y;
                childRectInWnd.right  = childRectInWnd.left + child->l_width;
                childRectInWnd.bottom = childRectInWnd.top  + child->l_height;

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
                    child->draw(xOffsetInWnd+child->l_x,yOffsetInWnd+child->l_y,true);
                else if(tlpWE->childUpdatedHash.find(child) != tlpWE->childUpdatedHash.end())
                    child->draw(xOffsetInWnd+child->l_x,yOffsetInWnd+child->l_y,false);
            }
        }
    }
}
