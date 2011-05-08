#include "MToolTip.h"
#include "MStyleSheet.h"
#include "MApplication.h"
#include "MWidget.h"
#include <CommCtrl.h>
#include <Windows.h>
namespace MetalBone
{
	class ToolTipWidget
	{
		public:
			inline ToolTipWidget();
			~ToolTipWidget();
			void show(const std::wstring&, long x, long y);
			void hide();
			inline bool hasRenderRule();
		private:
			bool isShown;
			HWND winHandle;
			ID2D1HwndRenderTarget* rt;
			CSS::RenderRuleQuerier rrQuerier;
			long width;
			long height;
	};

	extern wchar_t gMWidgetClassName[];
	inline ToolTipWidget::ToolTipWidget():
		isShown(false), winHandle(NULL), rt(0),
		rrQuerier(L"ToolTipWidget"), width(1), height(1)
	{
		winHandle = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST |
			WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
			gMWidgetClassName, L"", WS_POPUP,
			200,200,200,200, NULL,NULL, mApp->getAppHandle(), NULL);

		D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
		p.usage  = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
		p.type   = mApp->isHardwareAccerated() ? 
				D2D1_RENDER_TARGET_TYPE_HARDWARE : D2D1_RENDER_TARGET_TYPE_SOFTWARE;
		p.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
		p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
		mApp->getD2D1Factory()->CreateHwndRenderTarget(p,
			D2D1::HwndRenderTargetProperties(winHandle,D2D1::SizeU(width,height)), &rt);
	}
	ToolTipWidget::~ToolTipWidget()
	{
		SafeRelease(rt);
		::DestroyWindow(winHandle);
	}
	bool ToolTipWidget::hasRenderRule()
		{ return mApp->getStyleSheet()->getRenderRule(&rrQuerier).isValid(); }

	void ToolTipWidget::show(const std::wstring& tip, long x, long y)
	{
		if(isShown)
		{
			::SetWindowPos(winHandle, NULL, x, y, 0, 0,SWP_NOSIZE |SWP_NOZORDER);
		} else
		{
			isShown = true;
			CSS::RenderRule rr = mApp->getStyleSheet()->getRenderRule(&rrQuerier);
			RECT rect;
			rr.getContentMargin(rect);
			SIZE size = rr.getStringSize(tip,350);
			size.cx += (rect.left + rect.right);
			size.cy += (rect.top  + rect.bottom);

			bool sizeChanged = (size.cx != width || size.cy != height);
			if(sizeChanged)
			{
				width = size.cx;
				height = size.cy;
				rt->Resize(D2D1::SizeU(width,height));
			}

			rt->BeginDraw();
			RECT r = {0,0,width,height};
			rr.draw(rt,r,r,tip);

			ID2D1GdiInteropRenderTarget* gdiRT;
			HDC dc;
			rt->QueryInterface(&gdiRT);
			HRESULT result = gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,&dc);

			BLENDFUNCTION blend;
			blend.BlendOp     = AC_SRC_OVER;
			blend.AlphaFormat = AC_SRC_ALPHA;
			blend.BlendFlags  = 0;
			blend.SourceConstantAlpha = 255;
			POINT sourcePos = {0, 0};
			POINT destPos   = {x, y};
			SIZE windowSize = {width, height};

			UPDATELAYEREDWINDOWINFO info = {};
			info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
			info.dwFlags  = ULW_ALPHA;
			info.hdcDst   = NULL;
			info.hdcSrc   = dc;
			info.pblend   = &blend;
			info.psize    = &windowSize; // NULL
			info.pptSrc   = &sourcePos;
			info.pptDst   = &destPos;

			::ShowWindow(winHandle,SW_NORMAL);
			::UpdateLayeredWindowIndirect(winHandle,&info);
			::SetWindowPos(winHandle, HWND_TOPMOST, x, y, width, height, sizeChanged ? 0 : SWP_NOSIZE);

			gdiRT->ReleaseDC(0);
			gdiRT->Release();
			result = rt->EndDraw();
		}
	}

	void ToolTipWidget::hide()
	{
		::ShowWindow(winHandle, SW_HIDE);
		isShown = false;
	}

	static ToolTipWidget* toolTipWidget = 0;
	struct ToolTipHelper
	{
		enum TipType { System, Custom };
		ToolTipHelper():type(System),lastTip(0){}
		~ToolTipHelper() { delete toolTipWidget; }
		HWND      toolTipWnd;
		TipType   type;
		MToolTip* lastTip;
	};

	static ToolTipHelper toolTipHelper;

	MToolTip::MToolTip(const std::wstring& tip, MWidget* w, HidePolicy h)
		:hp(h),tipString(tip),icon(NULL),iconEnum(None),customWidget(w)
	{
		if(toolTipWidget == 0)
		{
			toolTipWidget = new ToolTipWidget();
			toolTipHelper.toolTipWnd = ::CreateWindowExW(WS_EX_TOPMOST,
				TOOLTIPS_CLASSW,0,WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
				CW_USEDEFAULT, CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
				0,0,mApp->getAppHandle(),0);

			TOOLINFOW ti = {};
			ti.cbSize = sizeof(TOOLINFOW);
			ti.uFlags = TTF_ABSOLUTE | TTF_IDISHWND;
			ti.hwnd   = toolTipHelper.toolTipWnd;
			ti.hinst  = NULL;
			ti.uId    = (UINT)toolTipHelper.toolTipWnd;
			ti.lpszText = L"";

			::SendMessageW(toolTipHelper.toolTipWnd, TTM_ADDTOOLW, 0, (LPARAM)&ti);
			::SendMessageW(toolTipHelper.toolTipWnd, TTM_SETMAXTIPWIDTH,0, (LPARAM)350);
		}
	}

	MToolTip::~MToolTip()
	{
		if(toolTipHelper.lastTip == this)
			hide();
		if(icon != NULL)
			::DeleteObject(icon);
		if(customWidget)
			delete customWidget;
	}

	bool MToolTip::isShowing() const
		{ return toolTipHelper.lastTip == this; }

	void MToolTip::hideAll()
	{
		if(toolTipHelper.lastTip != 0)
			toolTipHelper.lastTip->hide();
	}

	void MToolTip::hide()
	{
		if(toolTipHelper.lastTip != this)
			return;
		toolTipHelper.lastTip = 0;
		if(customWidget)
			customWidget->hide();
		else if(toolTipHelper.type == ToolTipHelper::Custom)
			toolTipWidget->hide();
		else
		{
			TOOLINFOW ti = {};
			ti.cbSize = sizeof(TOOLINFOW);
			ti.hwnd   = toolTipHelper.toolTipWnd;
			ti.uId    = (UINT)toolTipHelper.toolTipWnd;
			::SendMessageW(toolTipHelper.toolTipWnd,TTM_TRACKACTIVATE,(WPARAM)FALSE,(LPARAM)&ti);
			if(!title.empty() || icon != NULL || iconEnum != None)
				::SendMessageW(toolTipHelper.toolTipWnd,TTM_SETTITLEW,(WPARAM)TTI_NONE,(LPARAM)L"");
		}
	}

	void MToolTip::show(long x, long y)
	{
		if(toolTipHelper.lastTip != 0 && toolTipHelper.lastTip != this)
			toolTipHelper.lastTip->hide();

		if(customWidget)
		{
			customWidget->move(x,y);
			customWidget->show();
		} else
		{
			if(toolTipWidget->hasRenderRule())
			{
				toolTipHelper.type = ToolTipHelper::Custom;
				toolTipWidget->show(tipString,x,y);
			} else
			{
				toolTipHelper.type = ToolTipHelper::System;
				if(toolTipHelper.lastTip != this)
				{
					TOOLINFOW ti = {};
					ti.cbSize = sizeof(TOOLINFOW);
					ti.hwnd   = toolTipHelper.toolTipWnd;
					ti.uId    = (UINT)toolTipHelper.toolTipWnd;
					ti.lpszText = (LPWSTR)tipString.c_str();

					if(!title.empty() || icon != NULL || iconEnum != None)
					{
						WPARAM p = (WPARAM)icon;
						if(icon == NULL)
							p = (WPARAM)iconEnum;
						::SendMessageW(toolTipHelper.toolTipWnd, TTM_SETTITLEW, p, (LPARAM)title.c_str());
					}
					::SendMessageW(toolTipHelper.toolTipWnd,TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
					::SendMessageW(toolTipHelper.toolTipWnd,TTM_TRACKACTIVATE, (WPARAM)TRUE, (LPARAM)&ti);
				} 
				::SendMessageW(toolTipHelper.toolTipWnd, TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(x,y));
			}
		}
		toolTipHelper.lastTip = this;
	}
}