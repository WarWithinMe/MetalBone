#include "MWidget.h"
#include "MApplication.h"
#include "MStyleSheet.h"
#include "MResource.h"
#include "MRegion.h"
#include "private/MApp_p.h"
#include "private/MWidget_p.h"

#include <list>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <tchar.h>
#include <winerror.h>

namespace MetalBone
{
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

		MRect rect(0,0,width,height);
		generateStyleFlags(m_windowFlags,&winStyle,&winExStyle);

		::AdjustWindowRectEx(&rect,winStyle,false,winExStyle);
		m_windowExtras->m_wndHandle = CreateWindowExW(winExStyle,
			gMWidgetClassName,
			m_windowExtras->m_windowTitle.c_str(),
			winStyle,
			x,y, // Remark: we should offset a bit, due to the border frame.
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
			MRect rect(0, 0, width, height);
			DWORD winStyle   = 0;
			DWORD winExStyle = 0;
			generateStyleFlags(m_windowFlags, &winStyle, &winExStyle);
			::AdjustWindowRectEx(&rect, winStyle, false, winExStyle);
			::MoveWindow(m_windowExtras->m_wndHandle, x, y, rect.width(), rect.height(), true);
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
		MRect windowUpdateRect(width,height,0,0);

		ID2D1RenderTarget* rt = getRenderTarget();
		rt->BeginDraw();

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
					uRect.right == width && uRect.top == 0 && uRect.bottom == height)
				{
					windowUpdateRect.left   = 0;
					windowUpdateRect.right  = width;
					windowUpdateRect.top    = 0;
					windowUpdateRect.bottom = height;
					fullWindowUpdate = true;
					rt->Clear();
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
				uRect.offset(pc->x,pc->y);
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

						MRect childRect(cw->x, cw->y, cw->x + cw->width, cw->y + cw->height);
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
				MRect wur;
				uTargetUR.getBounds(wur);
				if(windowUpdateRect.left   > wur.left  ) windowUpdateRect.left   = wur.left;
				if(windowUpdateRect.right  < wur.right ) windowUpdateRect.right  = wur.right;
				if(windowUpdateRect.top    > wur.top   ) windowUpdateRect.top    = wur.top;
				if(windowUpdateRect.bottom < wur.bottom) windowUpdateRect.bottom = wur.bottom;

				rt->PushAxisAlignedClip(wur, D2D1_ANTIALIAS_MODE_ALIASED);
				rt->Clear();
				rt->PopAxisAlignedClip();
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

					MRect siblingRect(cw->x, cw->y, cw->x + cw->width, cw->y + cw->height);
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
			draw(0,0,passiveUpdateWidgets.find(this) != passiveUpdateWidgets.end());
			if(layeredWindow)
			{
				ID2D1GdiInteropRenderTarget* gdiRT;
				HDC dc;
				m_windowExtras->m_renderTarget->QueryInterface(&gdiRT);
				gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,&dc);

				BLENDFUNCTION blend = {AC_SRC_OVER,0,255,AC_SRC_ALPHA};
				POINT sourcePos = {0, 0};
				SIZE windowSize = {width, height};

				UPDATELAYEREDWINDOWINFO info = {};
				info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
				info.dwFlags  = ULW_ALPHA;
				info.hdcSrc   = dc;
				info.pblend   = &blend;
				info.psize    = &windowSize;
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

	void MWidget::doStyleSheetDraw(ID2D1RenderTarget* rt,const MRect& wr, const MRect& cr)
		{ mApp->getStyleSheet()->draw(this,rt,wr,cr); }

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
					MRect clipRect;
					iter.getRect(clipRect);

					rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_ALIASED);
					MRect widgetRect(xOffsetInWnd,yOffsetInWnd,
						xOffsetInWnd+width, yOffsetInWnd+height);
					doStyleSheetDraw(rt,widgetRect,clipRect);
					rt->PopAxisAlignedClip();

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
