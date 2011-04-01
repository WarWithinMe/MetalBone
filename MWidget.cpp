#include "MApplication.h"
#include "MStyleSheet.h"
#include "MWidget.h"
#include "MEvent.h"
#include <list>
#include <set>
#include <tchar.h>
#include <WinError.h>
#include <ObjBase.h>

namespace MetalBone
{
	// ========== MApplication ==========
	wchar_t gMWidgetClassName[] = L"MetalBone Widget";
	struct MApplicationData
	{
		MApplicationData();
		// Window procedure
		static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);

		// 这个set存放窗口指针，对于类型是WF_Widget并且父亲为0的MWidget，
		// 只有调用过MWidget::show()之后，才会被加入到这里。
		std::set<MWidget*> topLevelWindows;

		MWidget* findWidgetByHandle(HWND handle) const;
		void removeTopLevelWindows(MWidget*);

		bool				quitOnLastWindowClosed;
		HINSTANCE			appHandle;
		MStyleSheetStyle		ssstyle;
		ID2D1Factory*		d2d1Factory;
		IWICImagingFactory*	wicFactory;

		static MApplication::WinProc customWndProc;
		static MApplicationData* instance;
	};

	MApplication::WinProc MApplicationData::customWndProc = 0;
	MApplicationData* MApplicationData::instance = 0;
	MApplicationData::MApplicationData()
		: quitOnLastWindowClosed(true)
	{
		instance = this;
	}

	MWidget* MApplicationData::findWidgetByHandle(HWND handle) const
	{
		std::set<MWidget*>::const_iterator iter = topLevelWindows.begin();
		std::set<MWidget*>::const_iterator iterEnd = topLevelWindows.end();
		while(iter != iterEnd)
		{
			MWidget* w = (*iter);
			if(w->windowHandle() == handle)
				return w;
			++iter;
		}
		return 0;
	}

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
			case WM_CLOSE:
			{
				MWidget* window = instance->findWidgetByHandle(hwnd);
				M_ASSERT(window != 0);
				MEvent closeEvent;
				window->closeEvent(&closeEvent);
				if(closeEvent.isAccepted())
				{
					if(window->testAttributes(WA_DeleteOnClose))
						delete window;
					else
						window->hide();
				}
				break;
			}
			default:
				return DefWindowProcW(hwnd,msg,wparam,lparam);
		}

		return 0;
	}

	void MApplicationData::removeTopLevelWindows(MWidget* w)
	{
		topLevelWindows.erase(w);
		if(topLevelWindows.size() == 0 && quitOnLastWindowClosed)
			mApp->exit(0);
	}

	MApplication* MApplication::s_instance = 0;
	MApplication::MApplication():
		mImpl(new MApplicationData())
	{
		M_ASSERT_X(s_instance==0, "Only one MApplication instance allowed", "MApplication::MApplication()");
		ENSURE_IN_MAIN_THREAD;

		s_instance = this;
		mImpl->appHandle = GetModuleHandleW(NULL);

		// 注册窗口类
		WNDCLASS wc;
		setupRegisterClass(wc);
		RegisterClassW(&wc);

		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
#ifdef MB_DEBUG_D2D
		D2D1_FACTORY_OPTIONS opts;
		opts.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, opts, &(mImpl->d2d1Factory));
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create D2D1Factory. This is a fatal problem.", "MApplicationData()");
#else
		HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &(mImpl->d2d1Factory));
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create D2D1Factory. This is a fatal problem.", "MApplicationData()");
#endif
		
		hr = CoCreateInstance(CLSID_WICImagingFactory,NULL,
							  CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&(mImpl->wicFactory)));
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create WIC Component. This is a fatal problem.", "MApplicationData()");
	}

	MApplication::~MApplication()
	{
		 s_instance = 0;
		 delete mImpl;
		 MApplicationData::instance = 0;
		 CoUninitialize();
	}

	const std::set<MWidget*>& MApplication::topLevelWindows() const
	{
		return mImpl->topLevelWindows;
	}

	HINSTANCE MApplication::getAppHandle() const
	{
		return mImpl->appHandle;
	}

	void MApplication::setStyleSheet(const std::wstring& css)
	{
		mImpl->ssstyle.setAppSS(css);
		// TODO: Repolish
	}

	void MApplication::setupRegisterClass(WNDCLASS& wc)
	{
		wc.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = MApplicationData::windowProc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = mImpl->appHandle;
		wc.hIcon = 0;
		wc.hCursor = LoadCursorW(0,IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
		wc.lpszMenuName = 0;
		wc.lpszClassName = gMWidgetClassName;
	}

	void MApplication::setCustomWindowProc(WinProc proc)
	{
		mImpl->customWndProc = proc;
	}

	MStyleSheetStyle* MApplication::getStyleSheet()
	{
		return &(mImpl->ssstyle);
	}

	ID2D1Factory* MApplication::getD2D1Factory()
	{
		return mImpl->d2d1Factory;
	}

	IWICImagingFactory* MApplication::getWICImagingFactory()
	{
		return mImpl->wicFactory;
	}

	int MApplication::exec()
	{
		MSG msg;
		int result;

		while( (result = GetMessageW(&msg, 0, 0, 0)) != 0)
		{
			if(result == -1) // GetMessage 出错
				break;

			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

#ifdef METALBONE_USE_SIGSLOT
		aboutToQuit.emit();
#else
		aboutToQuit();
#endif
		return result;
	}















	// ========== MWidget ==========
	enum WidgetState
	{
		MWS_POLISHED = 1,
		MWS_HIDDEN   = 2,
		MWS_OpaqueBG = 4
	};

	void generateStyleFlags(unsigned int flags, DWORD* winStyleOut, DWORD* winExStyleOut)
	{
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

		M_ASSERT(winStyleOut != 0);
		M_ASSERT(winExStyleOut != 0);
		*winStyleOut = winStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
		*winExStyleOut = flags & WF_DontShowOnTaskbar ? winExStyle : (winExStyle | WS_EX_APPWINDOW);
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
		else if(m_parent == 0 && m_winHandle != NULL)
			return true;

		return false;
	}

	bool MWidget::isHidden() const { return m_widgetState & MWS_HIDDEN; }

	void MWidget::setOpaqueBackground(bool on)
	{
		on ? (m_widgetState |= MWS_OpaqueBG) : (m_widgetState &= (~MWS_OpaqueBG));
	}

	void MWidget::setWindowFlags(unsigned int flags)
	{
		m_windowFlags = flags;
		if(!isWindow())
			return;

		DWORD winStyle = 0;
		DWORD winExStyle = 0;
		generateStyleFlags(flags,&winStyle,&winExStyle);

		SetWindowLongPtrW(m_winHandle,GWL_STYLE,winStyle);
		if(winExStyle != 0)
			SetWindowLongPtrW(m_winHandle,GWL_EXSTYLE,winExStyle);

		HWND zpos = HWND_NOTOPMOST;
		if(flags & WF_AlwaysOnTop)
			zpos = HWND_TOPMOST;
		else if(flags & WF_AlwaysOnBottom)
			zpos = HWND_BOTTOM;

		SetWindowPos(m_winHandle,zpos,0,0,0,0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (zpos == HWND_NOTOPMOST ? SWP_NOZORDER : 0));
	}

	void MWidget::ensurePolished()
	{
		if(m_widgetState & MWS_POLISHED)
			return;

		m_widgetState |= MWS_POLISHED;
		mApp->getStyleSheet()->polish(this);
	}

	void MWidget::setTopLevelParentRecursively(MWidget* w)
	{
		m_topLevelParent = w;
		std::list<MWidget*>::iterator iter = m_children.begin();
		std::list<MWidget*>::iterator iterEnd = m_children.end();
		while(iter != iterEnd)
		{
			(*iter)->setTopLevelParentRecursively(w);
			++iter;
		}
	}

	void MWidget::setWindowOwner(MWidget* parent)
	{
		if(!isWindow())
			return;

		HWND parentWnd = (HWND)GetWindowLong(windowHandle(), GWL_HWNDPARENT);
		HWND toSetParentWnd = (parent == 0) ? NULL : parent->windowHandle();
		if(parentWnd == toSetParentWnd)
			return;

		if(m_winHandle != NULL)
		{
			DestroyWindow(m_winHandle);

			DWORD winStyle = 0;
			DWORD winExStyle = 0;
			generateStyleFlags(m_windowFlags,&winStyle,&winExStyle);
			m_winHandle = CreateWindowExW(winExStyle,
				gMWidgetClassName,
				m_windowTitle.c_str(),
				winStyle,
				x,y,height,width,toSetParentWnd,NULL,
				mApp->getAppHandle(), NULL);

			if(!isHidden())
			{
				ShowWindow(m_winHandle,SW_SHOW);
				UpdateWindow(m_winHandle);
			}
		} else {
			// We take down the parent, and use it when we construct the window
			m_parent = parent;
		}
	}

	void MWidget::closeWindow()
	{
		if(isWindow())
			SendMessage(m_winHandle,WM_CLOSE,0,0);
	}

	void MWidget::showMinimized()
	{
		if(isWindow())
		{
			m_windowState = WindowMinimized;
			ShowWindow(m_winHandle,SW_MINIMIZE);
		}
	}

	void MWidget::showMaximized()
	{
		if(isWindow())
		{
			m_windowState = WindowMaximized;
			ShowWindow(m_winHandle,SW_MAXIMIZE);
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
			if(m_winHandle != NULL)
			{
				DestroyWindow(m_winHandle);
				m_winHandle = NULL;
				MApplicationData::instance->topLevelWindows.erase(this);
			}
		} else {
			m_parent->m_children.remove(this);
			m_parent->repaint(x,y,width,height);
		}

		// Setting a widget's parent to 0 makes it hide.
		MWidget* tlp = parent;
		if(parent == 0)
		{
			m_widgetState |= MWS_HIDDEN;
			if(!testAttributes(WA_ConstStyleSheet))
				m_widgetState &= (~MWS_POLISHED); // We need to repolish the widget.
			tlp = this;
		} else {
			// We don't modify the widget's hidden state if it changes parent.
			parent->m_children.push_back(this);
			if(!isHidden())
				parent->repaint(x,y,width,height); // We repaint the rect of this widget inside parent.
		}

		setTopLevelParentRecursively(tlp);
		m_parent = parent;
	}

	void MWidget::show()
	{
		// If this has window handle (i.e. WF_Window or parentless shown WF_Widget )
		if(m_winHandle != NULL)
		{
			if(IsWindowVisible(m_winHandle))
				return;
			else
				ShowWindow(m_winHandle,SW_SHOW);
		} else if(!isHidden())
			return;

		// Polish stylesheet 
		if(!(m_widgetState & MWS_POLISHED))
			mApp->getStyleSheet()->polish(this);

		// Mark the widget visible. When polishing stylesheet, this is still 
		// hidden, so even if this widget's size changed, it won't repaint itself.
		m_widgetState &= (~MWS_HIDDEN);

		// This is a window or not?
		if((m_windowFlags & WF_Window) || m_parent == 0)
		{
			// Create a window for it.
			if(m_winHandle == NULL)
			{
				DWORD winStyle = 0;
				DWORD winExStyle = 0;
				HWND parentHandle = NULL;
				if(m_parent) {
					parentHandle = m_parent->windowHandle();
					m_parent = 0;
				}

				RECT rect = {0,0,width,height};
				AdjustWindowRectEx(&rect,winStyle,false,winExStyle);

				generateStyleFlags(m_windowFlags,&winStyle,&winExStyle);
				m_winHandle = CreateWindowExW(winExStyle,
					gMWidgetClassName,
					m_windowTitle.c_str(),
					winStyle,
					x,y,
					rect.right - rect.left, // Width
					rect.bottom - rect.top, // Height
					parentHandle,NULL,
					mApp->getAppHandle(), NULL);

				setTopLevelParentRecursively(this);

				ShowWindow(m_winHandle,SW_SHOW);
				UpdateWindow(m_winHandle);
				if(m_windowFlags & WF_AlwaysOnBottom)
					SetWindowPos(m_winHandle,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

				MApplicationData::instance->topLevelWindows.insert(this);

				// Create renderTarget for this window.
				D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
				D2D1_SIZE_U s;
				s.width = width;
				s.height = height;
				p.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
				mApp->getD2D1Factory()->CreateHwndRenderTarget(&p,
					&(D2D1::HwndRenderTargetProperties(m_winHandle, s)),
					&m_renderTarget);
			}
		} else {
			m_parent->repaint(x,y,width,height);
		}
	}

	void MWidget::hide()
	{
		if(isHidden())
			return;
		m_widgetState |= MWS_HIDDEN;
		if(isWindow())
			ShowWindow(m_winHandle,SW_HIDE);
		else {
			m_parent->repaint(x,y,width,height);
		}
	}

	MWidget::MWidget(MWidget* parent)
		: m_parent(parent),
		m_winHandle(NULL),
		m_renderTarget(0),
		x(200),y(200),
		width(480),height(640),
		minWidth(0),minHeight(0),
		maxWidth(0xffffffff),maxHeight(0xffffffff),
		m_attributes(0),
		m_windowFlags(WF_Widget),
		m_windowState(WindowNoState),
		m_widgetState(MWS_HIDDEN)
	{
		M_ASSERT_X(mApp!=0, "MApplication must be created first.", "MWidget constructor");
		ENSURE_IN_MAIN_THREAD;

		m_topLevelParent = (parent != 0) ? parent->m_topLevelParent : this;
	}

	MWidget::~MWidget()
	{
		if(m_parent == 0)
		{
			if(m_winHandle != NULL)
			{
				DestroyWindow(m_winHandle);
				MApplicationData::instance->removeTopLevelWindows(this);
			}
		}else
			setParent(0);

		mApp->getStyleSheet()->setWidgetSS(this,std::wstring());
		mApp->getStyleSheet()->removeCache(this);

		std::list<MWidget*>::iterator iter = m_children.begin();
		std::list<MWidget*>::iterator iterEnd = m_children.end();
		while(iter != iterEnd)
		{
			delete (*iter);
			++iter;
		}
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

		if(isHidden())
		{
			x      = vx;
			y      = vy;
			width  = vwidth;
			height = vheight;
			return;
		}

		if(m_parent != 0) 
		{
			// If is a child widget, we need to update the old region within the parent.
			m_parent->repaint(x,y,width,height);
			x      = vx;
			y      = vy;
			width  = vwidth;
			height = vheight;
			// Update the new rect.
			m_parent->repaint(x,y,width,height);
		} else {
			// It's a window. We change the size of it. And Windows will repaint it automatically.
			RECT rect = {0,0,width,height};
			DWORD winStyle = 0;
			DWORD winExStyle = 0;
			generateStyleFlags(m_windowFlags,&winStyle,&winExStyle);
			AdjustWindowRectEx(&rect,winStyle,false,winExStyle);
			MoveWindow(m_winHandle,x,y,rect.right - rect.left,rect.bottom - rect.top,true);
		}
	}
}
