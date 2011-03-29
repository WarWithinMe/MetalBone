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
	struct WidgetExtraData
	{
		WidgetExtraData(MWidget* p)
			: parent(p),
			  widgetState(MWS_HIDDEN),
			  attributes(0),
			  windowFlags(WF_Widget),
			  windowState(WindowNoState),
			  winHandle(NULL){}

		MWidget* parent;
		unsigned int widgetState;
		unsigned int attributes;
		unsigned int windowFlags;
		unsigned int windowState;

		int maxWidth;
		int maxHeight;
		int minWidth;
		int minHeight;

		// 包含这个widget的Window Handle
		HWND winHandle;

		ID2D1HwndRenderTarget* renderTarget;

		std::wstring windowTitle;
		std::wstring objectName;
		// index == 0 表示是最下方的子显示对象
		std::list<MWidget*> children;

		enum WidgetState
		{
			MWS_POLISHED = 1,
			MWS_HIDDEN = 2
		};

		void setWinHandleRecursively(HWND handle);
	};

	void WidgetExtraData::setWinHandleRecursively(HWND handle)
	{
		winHandle = handle;
		std::list<MWidget*>::iterator iter = children.begin();
		std::list<MWidget*>::iterator iterEnd = children.end();
		while(iter != iterEnd)
		{
			(*iter)->data->setWinHandleRecursively(handle);
			++iter;
		}
	}

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

	MWidget::MWidget(MWidget* parent)
		: x(200),y(200),width(480),height(640),data(new WidgetExtraData(parent))
	{
		M_ASSERT_X(mApp!=0, "MApplication must be created first.", "MWidget constructor");
		ENSURE_IN_MAIN_THREAD;
	}

	MWidget::~MWidget()
	{
		if(data->parent == 0)
		{
			if(data->winHandle != NULL)
			{
				DestroyWindow(data->winHandle);
				MApplicationData::instance->removeTopLevelWindows(this);
			}
		}else
			setParent(0);

		MApplicationData::instance->ssstyle.setWidgetSS(this,std::wstring());
		MApplicationData::instance->ssstyle.removeCache(this);

		std::list<MWidget*>::iterator iter = data->children.begin();
		std::list<MWidget*>::iterator iterEnd = data->children.end();
		while(iter != iterEnd)
		{
			delete (*iter);
			++iter;
		}
		delete data;
	}

	void MWidget::setObjectName(const std::wstring& name)
	{
		data->objectName = name;
	}

	const std::wstring& MWidget::objectName() const
	{
		return data->objectName;
	}

	HWND MWidget::windowHandle() const
	{
		return data->winHandle;
	}

	MWidget* MWidget::parent() const
	{
		return data->parent;
	}

	ID2D1RenderTarget* MWidget::getRenderTarget()
	{
		return windowWidget()->data->renderTarget;
	}

	MWidget* MWidget::windowWidget() const
	{
		MWidget* widget = const_cast<MWidget*>(this);
		while(widget->parent() != 0)
			widget = widget->parent();
		return widget;
	}

	bool MWidget::isWindow() const
	{
		if(data->windowFlags & WF_Window)
			return true;
		else if(data->parent == 0 && data->winHandle != NULL)
			return true;

		return false;
	}

	void MWidget::setWindowTitle(const std::wstring& title)
	{
		data->windowTitle = title;
		if(isWindow())
			SetWindowText(data->winHandle,title.c_str());
	}

	const std::wstring& MWidget::windowTitle() const
	{
		return data->windowTitle;
	}

	const std::list<MWidget*>& MWidget::children() const
	{
		return data->children;
	}

	unsigned int MWidget::windowFlags() const
	{
		return data->windowFlags;
	}

	void MWidget::setWindowFlags(unsigned int flags)
	{
		data->windowFlags = flags;
		if(!isWindow())
			return;

		DWORD winStyle = 0;
		DWORD winExStyle = 0;
		generateStyleFlags(flags,&winStyle,&winExStyle);

		SetWindowLongPtrW(data->winHandle,GWL_STYLE,winStyle);
		if(winExStyle)
			SetWindowLongPtrW(data->winHandle,GWL_EXSTYLE,winExStyle);

		HWND zpos = HWND_NOTOPMOST;
		if(flags & WF_AlwaysOnTop)
			zpos = HWND_TOPMOST;
		else if(flags & WF_AlwaysOnBottom)
			zpos = HWND_BOTTOM;

		SetWindowPos(data->winHandle,zpos,0,0,0,0,
					 SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | (zpos == HWND_NOTOPMOST ? SWP_NOZORDER : 0));
	}

	unsigned int MWidget::attributes() const
	{
		return data->attributes;
	}

	bool MWidget::testAttributes(WidgetAttributes attr) const
	{
		return data->attributes & attr;
	}

	void MWidget::setAttributes(WidgetAttributes attr, bool on)
	{
		if(on)
			data->attributes |= attr;
		else
			data->attributes &= (~attr);
	}

	void MWidget::setStyleSheet(const std::wstring& css)
	{
		MApplicationData::instance->ssstyle.setWidgetSS(this,css);
	}

	void MWidget::ensurePolished()
	{
		if(data->widgetState & WidgetExtraData::MWS_POLISHED)
			return;

		data->widgetState |= WidgetExtraData::MWS_POLISHED;
//		MApplicationData::instance->ssstyle.polish(this);
	}

	void MWidget::setParent(MWidget* parent)
	{
		if(data->parent == parent)
			return;

		if(data->windowFlags & WF_Window) // 用户希望设置Window Owner
		{
			// 如果窗口已经建立，则必须要先删掉这个窗口，再重新创建。
			if(data->winHandle != NULL)
			{
				DestroyWindow(data->winHandle);

				DWORD winStyle = 0;
				DWORD winExStyle = 0;
				generateStyleFlags(data->windowFlags,&winStyle,&winExStyle);

				HWND winHandle = parent ? parent->windowHandle() : NULL;
				winHandle = CreateWindowExW(winExStyle,
											gMWidgetClassName,
											data->windowTitle.c_str(),
											winStyle,
											x,y,height,width,winHandle,NULL,
											mApp->getAppHandle(), NULL);
				data->setWinHandleRecursively(winHandle);

				data->widgetState &= WidgetExtraData::MWS_HIDDEN;
				data->widgetState &= (~WidgetExtraData::MWS_POLISHED);
			}
			data->parent = parent;
			return;
		}

		if(data->parent == 0)
		{
			if(data->winHandle != NULL) // 没有parent，并且已经创建了窗口，则要删除
			{
				DestroyWindow(data->winHandle);
				data->winHandle = NULL;
				MApplicationData::instance->topLevelWindows.erase(this);
			}
		}else
		{
			// 如果是普通的widget
			data->parent->data->children.remove(this);

			// TODO：更新旧的父亲的显示内容
		}

		HWND winHandle = NULL;
		if(parent != 0)
		{
			parent->data->children.push_back(this);
			winHandle = parent->data->winHandle;
		}
		data->setWinHandleRecursively(winHandle);

		data->widgetState &= WidgetExtraData::MWS_HIDDEN;
		data->widgetState &= (~WidgetExtraData::MWS_POLISHED);
		data->parent = parent;
	}

	void MWidget::show()
	{
		// 如果不是被隐藏的话，返回。
		if(isWindow())
		{
			if(IsWindowVisible(data->winHandle))
				return;
		}else if(!(data->widgetState & WidgetExtraData::MWS_HIDDEN))
			return;

		if(data->winHandle == NULL)
		{
			// 表明这个MWidget是一个窗口，同时，没有为其创建window。
			DWORD winStyle = 0;
			DWORD winExStyle = 0;
			HWND winHandle = NULL;
			if(data->parent)
				winHandle = data->parent->windowHandle();

			generateStyleFlags(data->windowFlags,&winStyle,&winExStyle);
			winHandle = CreateWindowExW(winExStyle,
										gMWidgetClassName,
										data->windowTitle.c_str(),
										winStyle,
										x,y,height,width,winHandle,NULL,
										mApp->getAppHandle(), NULL);
			data->setWinHandleRecursively(winHandle);

			ShowWindow(data->winHandle,SW_SHOW);
			UpdateWindow(data->winHandle);
			if(data->windowFlags & WF_AlwaysOnBottom)
				SetWindowPos(data->winHandle,HWND_BOTTOM,0,0,0,0,SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);

			MApplicationData::instance->topLevelWindows.insert(this);

			D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
			D2D1_SIZE_U s;
			s.width = width;
			s.height = height;
			p.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
			mApp->getD2D1Factory()->CreateHwndRenderTarget(&p,
				&(D2D1::HwndRenderTargetProperties(winHandle, s)),
				&(data->renderTarget));
		}

		// TODO：如果这个Widget还没有获取相关资源，则在这里获取。
		// TODO：处理显示
		data->widgetState &= (~WidgetExtraData::MWS_HIDDEN);
	}

	void MWidget::hide()
	{
		if(data->widgetState & WidgetExtraData::MWS_HIDDEN)
			return;

		data->widgetState &= WidgetExtraData::MWS_HIDDEN;

		if(isWindow())
		{
			ShowWindow(data->winHandle,SW_HIDE);
		}else
		{

		}
	}

	bool MWidget::isHidden() const
	{
		return data->widgetState & WidgetExtraData::MWS_HIDDEN;
	}

	void MWidget::closeWindow()
	{
		if(isWindow())
			SendMessage(data->winHandle,WM_CLOSE,0,0);
	}

	void MWidget::showMinimized()
	{
		if(isWindow())
		{
			data->windowState = WindowMinimized;
			ShowWindow(data->winHandle,SW_MINIMIZE);
		}
	}

	void MWidget::showMaximized()
	{
		if(isWindow())
		{
			data->windowState = WindowMaximized;
			ShowWindow(data->winHandle,SW_MAXIMIZE);
		}
	}

	void MWidget::setGeometry(int x, int y, int width, int height)
	{

	}
}
