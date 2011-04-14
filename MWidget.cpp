#include "MWidget.h"
#include "MApplication.h"
#include "MStyleSheet.h"
#include "MEvent.h"

#include <list>
#include <set>
#include <unordered_map>
#include <tchar.h>
#include <WinError.h>
#include <ObjBase.h>

namespace MetalBone
{
	// ========== MApplication ==========
	wchar_t gMWidgetClassName[] = L"MetalBone Widget";
	MApplication::WinProc MApplicationData::customWndProc = 0;
	MApplicationData* MApplicationData::instance = 0;

	struct MApplicationData
	{
		MApplicationData(bool hwAccelerated):
				quitOnLastWindowClosed(true),
				hardwareAccelerated(hwAccelerated)
		{ instance = this; }
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
		IWICImagingFactory* wicFactory;

		static MApplication::WinProc customWndProc;
		static MApplicationData* instance;
	};

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

	MWidget* MApplicationData::findWidgetWithLayeredHandle(HWND handle) const
	{
		std::set<MWidget*>::const_iterator iter = topLevelWindows.begin();
		std::set<MWidget*>::const_iterator iterEnd = topLevelWindows.end();
		while(iter != iterEnd)
		{
			MWidget* w = *iter;
			if(w->windowHandle() == handle || w->m_layeredWndHandle == handle)
				return w;
			++iter;
		}
		return 0;
	}


	MApplication* MApplication::s_instance = 0;
	MApplication::MApplication(bool hwAccelerated):
		mImpl(new MApplicationData(hwAccelerated))
	{
		M_ASSERT_X(s_instance==0, "Only one MApplication instance allowed", "MApplication::MApplication()");
		ENSURE_IN_MAIN_THREAD;

		s_instance = this;
		mImpl->appHandle = GetModuleHandleW(NULL);

		// ×¢²á´°¿ÚÀà
		WNDCLASS wc;
		setupRegisterClass(wc);
		RegisterClassW(&wc);

		HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		M_ASSERT_X(SUCCEEDED(hr), "Cannot initialize COM!", "MApplicationData()");

#ifdef MB_DEBUG_D2D
		D2D1_FACTORY_OPTIONS opts;
		opts.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, opts, &(mImpl->d2d1Factory));
#else
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &(mImpl->d2d1Factory));
#endif
		M_ASSERT_X(SUCCEEDED(hr), "Cannot create D2D1Factory. This is a fatal problem.", "MApplicationData()");
		
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

	const std::set<MWidget*>& MApplication::topLevelWindows() const { return mImpl->topLevelWindows; }
	HINSTANCE            MApplication::getAppHandle()         const { return mImpl->appHandle;       }
	MStyleSheetStyle*    MApplication::getStyleSheet()              { return &(mImpl->ssstyle);      }
	ID2D1Factory*        MApplication::getD2D1Factory()             { return mImpl->d2d1Factory;     }
	IWICImagingFactory*  MApplication::getWICImagingFactory()       { return mImpl->wicFactory;      }
	bool  MApplication::isHardwareAccerated()                 const { return mImpl->hardwareAccelerated; }
	void  MApplication::setStyleSheet(const std::wstring& css)      { mImpl->ssstyle.setAppSS(css);  }
	void  MApplication::setCustomWindowProc(WinProc proc)           { mImpl->customWndProc = proc;   }

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

	int MApplication::exec()
	{
		MSG msg;
		int result;

		while( (result = GetMessageW(&msg, 0, 0, 0)) != 0)
		{
			if(result == -1) // GetMessage ³ö´í
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
				if(window == 0)
					break;
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
			case WM_PAINT:
			{
				MWidget* window = instance->findWidgetByHandle(hwnd);
				if(window == 0) {
					mDebug(L"[Warning] We can find a widget corresponds to the HWND when processing WM_PAINT.");	
					break;
				}
				window->drawWindow();
				ValidateRect(hwnd,0);
			}
				break;
			case WM_DESTROY:
				// Check if the user wants to quit when the last window is closed.
				if(instance->topLevelWindows.size() == 0 && instance->quitOnLastWindowClosed)
					mApp->exit(0);
				break;
			default:
				return DefWindowProcW(hwnd,msg,wparam,lparam);
		}

		return 0;
	}













	// ========== MWidget ==========
	enum WidgetState
	{
		MWS_POLISHED    = 1,
		MWS_HIDDEN      = 2,
		MWS_CSSOpaqueBG = 4,
		MWS_VISIBLE     = 8
	};
	
	struct WindowExtras {
		WindowExtras():m_wndHandle(NULL),m_layeredWndHandle(NULL),m_renderTarget(0){}
		HWND m_wndHandle;
		HWND m_layeredWndHandle;
		ID2D1HwndRenderTarget* m_renderTarget;

		std::wstring m_windowTitle;
		typedef std::tr1::unordered_map<MWidget*,RECT> RepaintHash;
		RepaintHash repaintWidgets;

		inline HWND getRealHwnd() const
		{
			if(m_layeredWndHandle != NULL)
				return m_layeredWndHandle;
			return m_wndHandle;
		}

		void addToRepaintMap(MWidget*,int left,int right,int top,int bottom);
	};

	MWidget::MWidget(MWidget* parent)
		: m_parent(parent),
		m_windowExtras(0),
		x(200),y(200),
		width(640),height(480),
		minWidth(0),minHeight(0),
		maxWidth(0xffffffff),maxHeight(0xffffffff),
		m_attributes(WA_AutoBG),
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
		if(m_parent != 0)
			setParent(0);

		mApp->getStyleSheet()->setWidgetSS(this,std::wstring());
		mApp->getStyleSheet()->removeCache(this);

		// Delete children
		MWidgetList::iterator iter = m_children.begin();
		MWidgetList::iterator iterEnd = m_children.end();
		while(iter != iterEnd) {
			delete (*iter);
			++iter;
		}

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
			if(m_windowExtras != 0)
				return m_windowExtras->m_renderTarget;
			else
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

		m_widgetState |= MWS_HIDDEN;
	}

	void MWidget::createWnd()
	{
		if(hasWindow()) {
			mWarning(true,L"A window for this widget already exists! In MWidget::createWnd()");
			return;
		}

		if(m_windowExtras == 0)
			m_windowExtras = new WindowExtras();

		bool isLayered    = m_windowFlags & WF_AllowTransparency;
		DWORD winStyle    = 0;
		DWORD winExStyle  = 0;
		HWND parentHandle = NULL;
		// We don't set parent of layered window.
		if(!isLayered && m_parent)
			parentHandle = m_parent->windowHandle();
		m_parent = 0;

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

		setTopLevelParentRecursively(this);

		// We have to remember this is a TopLevel Window. And
		// we also have to create a renderTarget for it.
		MApplicationData::insertTLW(this);
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
			m_widgetState |= MWS_HIDDEN;
			if(!testAttributes(WA_ConstStyleSheet))
				m_widgetState &= (~MWS_POLISHED); // We need to repolish the widget.
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

	bool MWidget::isHidden() const { return (m_widgetState & MWS_HIDDEN) != 0; }
	void MWidget::setStyleSheet(const std::wstring& css) { mApp->getStyleSheet()->setWidgetSS(this,css); }
	void MWidget::ensurePolished()
	{
		if(m_widgetState & MWS_POLISHED)
			return;
		m_widgetState |= MWS_POLISHED;

		if(m_attributes & WA_NoStyleSheet)
			return;

		MStyleSheetStyle* sss = mApp->getStyleSheet();
		sss->polish(this);
		setWidgetState(MWS_CSSOpaqueBG,sss->getRenderRule(this,0).opaqueBackground());
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
		if((m_windowFlags & WF_Window) || m_parent == 0)
			createWnd();
		ensurePolished();

		// Mark the widget visible. When polishing stylesheet, this is still 
		// hidden, so even if this widget's size changed, it won't repaint itself.
		m_widgetState &= (~MWS_HIDDEN);
		repaint();
	}

	void MWidget::hide()
	{
		if(isHidden())
			return;
		m_widgetState |= MWS_HIDDEN;
		m_widgetState &= (~MWS_VISIBLE);
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
			m_parent->repaint(x,y,width,height);
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
		bool selfVisible = 0 != (m_widgetState & MWS_VISIBLE);
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

		return m_attributes & WA_OpaqueBG == 0;
	}

	void WindowExtras::addToRepaintMap(MWidget* w,int left,int right,int top,int bottom)
	{
		RepaintHash::iterator iter = repaintWidgets.find(w);
		if(iter != repaintWidgets.end()) {
			RECT& updateRect  = iter->second;
			if(updateRect.left   > left  ) updateRect.left   = left;
			if(updateRect.top    > top   ) updateRect.top    = top;
			if(updateRect.right  < right ) updateRect.right  = right;
			if(updateRect.bottom < bottom) updateRect.bottom = bottom;
		} else {
			iter = repaintWidgets.insert(w).first;
			RECT& updateRect  = iter->second;
			updateRect.left   = left;
			updateRect.right  = right;
			updateRect.top    = top;
			updateRect.bottom = bottom;
		}
	}

	void MWidget::repaint(int x, int y, unsigned int width, unsigned int height)
	{
		M_ASSERT_X(windowHandle() != NULL, "We don't have a window yet when trying to repaint this widget.", "MWidget::repaint()");

		if(isHidden())
			return;

		// If the widget needs to draw itself, we marks it invisible at this time,
		// And if it does draw itself during next redrawing. It's visible.
		m_widgetState &= (~MWS_VISIBLE);
		m_topLevelParent->m_windowExtras->addToRepaintMap(this,x,y,width,height);

		// Tells Windows that we need to update, we don't care the clip. We do it
		// in our own way.
		UpdateWindow(windowHandle());

		/*MWidget* p = m_parent;
		bool outsideOfParent = false;
		int right  = x + width;
		int bottom = y + height;
		while(p != 0) {
			if(0 >= right   ) { outsideOfParent = true; break; }
			if(0 >= bottom  ) { outsideOfParent = true; break; }
			if(x > p->width ) { outsideOfParent = true; break; }
			if(y + p->height) { outsideOfParent = true; break; }

			if(right  > p->width ) right  = p->width;
			if(bottom > p->height) bottom = p->height;
			if(x < 0) x=0;
			if(y < 0) y=0;

			x += p->x;
			y += p->y;
			right  += p->x;
			bottom += p->y;
		}
		if(outsideOfParent) return; */
	}

	void MWidget::drawWindow()
	{
		M_ASSERT(isWindow());











		int retryTimes = 3;
		HRESULT result;

		do {
			RECT boundingRect;
			GetRgnBox(updateRgn,&boundingRect);
			m_renderTarget->BeginDraw();

			MWidgetList::reverse_iterator childIter    = m_children.rbegin();
			MWidgetList::reverse_iterator childIterEnd = m_children.rend();
			MWidgetList drawLaterList;
			while(childIter != childIterEnd)
			{
				// We start with the topmost child, if we found a child which meets 
				// demands, we immediately draw it, and exclude it from the updateRegion.
				MWidget* child = *childIter;
				if(child->isHidden()) {
					++childIter;
					continue;
				}
				if(child->isOpaqueDrawing())
				{
					RECT drawRect = child->geometry();
					if(RectVisible(dc,&drawRect))
					{
						RECT intersect;
						IntersectRect(&intersect,&drawRect,&rect);
						child->draw(intersect);
						if(ExcludeClipRect(dc, drawRect.left, drawRect.top,
									drawRect.right,drawRect.bottom) == NULLREGION) { break; }
						GetClipBox(dc,&rect);
					}
				} else {
					nonOpaqueChildren.push_back(child);
				}
				++childIter;
			}

			// We draw the window itself
			mApp->getStyleSheet()->draw(this,0,0,rect);

			// Now we draw the non-opaque children.
			childIter = nonOpaqueChildren.rbegin();
			childIterEnd = nonOpaqueChildren.rend();
			while(childIter != childIterEnd)
			{
				MWidget* child = *childIter;
				RECT drawRect = {child->x, child->y, child->width + child->x, child->height + child->y};
				if(RectVisible(dc,&drawRect))
				{
					RECT intersect;
					IntersectRect(&intersect,&drawRect,&rect);
					child->draw(intersect);
					if(ExcludeClipRect(dc, drawRect.left, drawRect.top,
						drawRect.right,drawRect.bottom) == NULLREGION) { break; }
				}
				(*childIter)->draw(drawRect);
				++childIter;
			}
			MPaintEvent p;
			p.setUpdateRect(&rect);
			paintEvent(&p);

			result = m_renderTarget->EndDraw();
			if(result == D2DERR_RECREATE_TARGET)
			{
				createRenderTarget();
				mApp->getStyleSheet()->recreateResources(this);
			}
		}while(FAILED(result) && (--retryTimes > 0));
	}

	void MWidget::draw(RECT& rect)
	{

	}
}
