#include "MResource.h"
#include "MStyleSheet.h"
#include "MApplication.h"
#include "MWidget.h"
#include "3rd/XUnzip.h"

#include "MGraphics.h"
#include "private/MGraphicsData.h"

#define MAX_TOOLTIP_WIDTH 350
#define TRAY_ICON_MESSAGE (WM_USER+100)

namespace MetalBone
{
    struct MIconData
    {
        inline MIconData(HICON h, bool d):refCount(1),handle(h),autoDestroy(d){}
        inline ~MIconData(){ if(autoDestroy) ::DestroyIcon(handle); }

        HICON handle;
        unsigned int refCount;
        bool autoDestroy;
    };

    MIcon::~MIcon() { if(data != 0 && (--data->refCount == 0)) delete data; }
    MIcon::MIcon(HICON icon, bool ad):data(new MIconData(icon,ad)){}
    MIcon::MIcon(const MIcon& rhs)
    {
        if(rhs.data != 0) ++rhs.data->refCount;
        data = rhs.data;
    }
    void MIcon::setHandle(HICON icon, bool autoDestroy)
    {
        if(data == 0)
            data = new MIconData(icon,autoDestroy);
        else if(data->refCount > 1)
        {
            --data->refCount;
            data = new MIconData(icon,autoDestroy);
        } else
        {
            if(data->autoDestroy)
                ::DestroyIcon(data->handle);
            data->handle = icon;
            data->autoDestroy = autoDestroy;
        }
    }
    MIcon::operator HICON() const
        { return data == 0 ? NULL : data->handle; }
    MIcon::operator bool() const
        { return (data !=0) && data->handle != NULL; }
    const MIcon& MIcon::operator=(const MIcon& rhs)
    {
        if(data != 0 && (--data->refCount == 0))
            delete data;
        if(rhs.data != 0) ++rhs.data->refCount;
        data = rhs.data;
        return *this;
    }
    bool MIcon::operator==(const MIcon& rhs) const
    {
        if(data != rhs.data)
            return false;
        if(data == 0)
            return true;
        return data->handle == rhs.data->handle;
    }



    unsigned short MSysTrayIcon::cid = 0;
    HWND  MSysTrayIcon::trayWnd = NULL;
    std::map<unsigned short, MSysTrayIcon*> MSysTrayIcon::trayIconMap;
    static wchar_t gMSysTrayWndClassName[] = L"MetalBone SysTray Wnd";
    MSysTrayIcon::MSysTrayIcon():id(cid),isVisible(false)
    {
        ++cid;
        trayIconMap[id] = this;
        if(trayWnd == NULL)
        {
            WNDCLASSW wc = {};
            wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
            wc.hInstance     = (HINSTANCE)GetModuleHandle(NULL);
            wc.hCursor       = ::LoadCursorW(NULL, IDC_ARROW);
            wc.lpfnWndProc   = MSysTrayIcon::trayWndProc;
            wc.lpszClassName = gMSysTrayWndClassName;
            RegisterClassW(&wc);

            trayWnd = CreateWindowExW(0,gMSysTrayWndClassName,0,
                WS_POPUP,0,0,0,0,0,0,wc.hInstance,0);
        }
    }

    MSysTrayIcon::~MSysTrayIcon()
        { hide(); trayIconMap.erase(cid); }
    void MSysTrayIcon::setName(const std::wstring& n)
    {
        name = n;
        if(name.size() > 127)
            name.resize(127);
        if(isVisible)
        {
            NOTIFYICONDATAW data = {};
            data.cbSize = sizeof(NOTIFYICONDATAW);
            data.hWnd   = trayWnd;
            data.uID    = id;
            data.uFlags = NIF_TIP;
            wcscpy_s(data.szTip, min(n.size(), 127), n.c_str());
            ::Shell_NotifyIconW(NIM_MODIFY, &data);
        }
    }

    void MSysTrayIcon::setIcon(MIcon ic)
    {
        if(icon == ic) return;
        icon = ic;
        if(isVisible)
        {
            NOTIFYICONDATAW data = {};
            data.cbSize = sizeof(NOTIFYICONDATAW);
            data.hWnd   = trayWnd;
            data.uID    = id;
            data.uFlags = NIF_ICON;
            data.hIcon  = icon ? icon : ::LoadIconW(NULL, IDI_APPLICATION);
            ::Shell_NotifyIconW(NIM_MODIFY, &data);
        }
    }

    void MSysTrayIcon::show()
    {
        if(isVisible) return;
        isVisible = true;

        NOTIFYICONDATAW data = {};
        data.uCallbackMessage = TRAY_ICON_MESSAGE;
        data.cbSize = sizeof(NOTIFYICONDATAW);
        data.hWnd   = trayWnd;
        data.uID    = id;
        data.uFlags = NIF_MESSAGE|NIF_ICON|NIF_TIP;
        data.hIcon  = icon ? icon : ::LoadIconW(NULL, IDI_APPLICATION);
        wcscpy_s(data.szTip, 128, name.c_str());

        ::Shell_NotifyIconW(NIM_ADD, &data);
    }

    void MSysTrayIcon::hide()
    {
        if(!isVisible) return;
        isVisible = false;

        NOTIFYICONDATAW data = {};
        data.cbSize = sizeof(NOTIFYICONDATAW);
        data.hWnd   = trayWnd;
        data.uID    = id;
        ::Shell_NotifyIconW(NIM_DELETE, &data);
    }

    void MSysTrayIcon::reshow()
    {
        if(isVisible)
        {
            isVisible = false;
            show();
        }
    }

    static const UINT gTaskbarCreatedMsg = ::RegisterWindowMessage(TEXT("TaskbarCreated"));
    LRESULT CALLBACK MSysTrayIcon::trayWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if(msg == TRAY_ICON_MESSAGE)
        {
            TrayIconMap::iterator it = trayIconMap.find((unsigned short)wparam);
            if(it != trayIconMap.end())
                it->second->event((unsigned int)lparam);
            return 0;
        } else if(msg == gTaskbarCreatedMsg)
        {
            TrayIconMap::iterator it = trayIconMap.begin();
            TrayIconMap::iterator itEnd = trayIconMap.end();
            while(it != itEnd)
                it->second->reshow();
        }
        return ::DefWindowProc(hwnd, msg, wparam, lparam);
    }

    bool MSysTrayIcon::showBalloonMessage(const std::wstring& message,
        const std::wstring& title, BalloonIcon iconEnum, MIcon customIcon)
    {
        if(!isVisible) return false;

        if(message.size() > 0 && message.size() < 256)
        {
            NOTIFYICONDATA data = {};
            data.cbSize      = sizeof(NOTIFYICONDATAW);
            data.hWnd        = trayWnd;
            data.uID         = id;
            data.uFlags      = NIF_INFO;
            if(customIcon)
            {
                data.dwInfoFlags = NIIF_USER;
                data.hBalloonIcon = customIcon;
            } else
                data.dwInfoFlags = iconEnum;

            wcscpy_s(data.szInfo, 256, message.c_str());
            if(title.size() > 0 && title.size() < 64)
                wcscpy_s(data.szInfoTitle, 64, data.szInfoTitle);

            return (::Shell_NotifyIcon(NIM_MODIFY, &data) == TRUE);
        }

        return false;
    }



    // ========== MToolTip ==========
    extern wchar_t gMWidgetClassName[];
    class ToolTipWidget
    {
        public:
            inline ToolTipWidget();
            inline ~ToolTipWidget();

            inline bool hasRenderRule();
            inline void hide();
            void show(const std::wstring&, long x, long y);
            void createRenderTarget();

            void deleteThis() { delete this; }

        private:
            bool isShown;
            HWND winHandle;
            CSS::RenderRuleQuerier rrQuerier;
            MGraphicsData*         graphicsData;

            long width;
            long height;
    };

    inline ToolTipWidget::ToolTipWidget():
        isShown(false), winHandle(NULL), 
        rrQuerier(L"ToolTipWidget"), width(1), height(1)
    {
        winHandle = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            gMWidgetClassName, L"", WS_POPUP,
            0,0,1,1, NULL, NULL, mApp->getAppHandle(), NULL);

        graphicsData = MGraphicsData::create(winHandle,1,1);
        graphicsData->setLayeredWindow(true);

        mApp->aboutToQuit.Connect(this, &ToolTipWidget::deleteThis);
    }
    inline ToolTipWidget::~ToolTipWidget()
    {
        delete graphicsData;
        ::DestroyWindow(winHandle);
    }
    inline bool ToolTipWidget::hasRenderRule()
        { return mApp->getStyleSheet()->getRenderRule(&rrQuerier).isValid(); }
    inline void ToolTipWidget::hide()
    {
        ::ShowWindow(winHandle, SW_HIDE);
        isShown = false;
    }

    void ToolTipWidget::show(const std::wstring& tip, long x, long y)
    {
        if(isShown)
        {
            ::SetWindowPos(winHandle, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return;
        }

        isShown = true;
        CSS::RenderRule rule = mApp->getStyleSheet()->getRenderRule(&rrQuerier);

        MSize size = rule.getStringSize(tip, MAX_TOOLTIP_WIDTH);
        MRect marginRect = rule.getContentMargin();
        size.cx += (marginRect.left + marginRect.right);
        size.cy += (marginRect.top  + marginRect.bottom);

        bool sizeChanged = (size.cx != width || size.cy != height);
        if(sizeChanged)
        {
            width  = size.cx;
            height = size.cy;
            graphicsData->resize(width,height);
        }

        ::SetWindowPos(winHandle, HWND_TOPMOST, x, y, width, height, SWP_NOREDRAW | SWP_SHOWWINDOW);

        MRect drawRect(0,0,width,height);
        MGraphics graphics(graphicsData);
        graphicsData->beginDraw();
        graphicsData->clear();
        rule.draw(graphics, drawRect, drawRect, tip, CSS::QuickOpaque);
        graphicsData->endDraw(MRect(0,0,width,height));
    }

    static ToolTipWidget* toolTipWidget = 0;
    struct ToolTipHelper
    {
        enum TipType { System, Custom };
        ToolTipHelper():type(System),lastTip(0){}
        ~ToolTipHelper() {}
        HWND      toolTipWnd; // The Windows ToolTip Control Handle
        TipType   type;
        MToolTip* lastTip;
    };
    static ToolTipHelper toolTipHelper;

    MToolTip::MToolTip(const std::wstring& tip, MWidget* w, HidePolicy h)
        :hp(h),tipString(tip),icon(NULL),iconEnum(None),customWidget(w)
    {
        if(toolTipWidget == 0)
        {
            HWND tooltipWnd = ::CreateWindowExW(WS_EX_TOPMOST,
                TOOLTIPS_CLASSW, 0,
                WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
                CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,
                0,0,mApp->getAppHandle(),0);

            TOOLINFOW ti = {};
            ti.cbSize   = sizeof(TOOLINFOW);
            ti.uFlags   = TTF_ABSOLUTE | TTF_IDISHWND;
            ti.hwnd     = tooltipWnd;
            ti.uId      = (UINT_PTR)tooltipWnd;
            ti.lpszText = L"";

            ::SendMessageW(tooltipWnd, TTM_ADDTOOLW, 0, (LPARAM)&ti);
            ::SendMessageW(tooltipWnd, TTM_SETMAXTIPWIDTH, 0, (LPARAM)MAX_TOOLTIP_WIDTH);

            toolTipWidget = new ToolTipWidget();
            toolTipHelper.toolTipWnd = tooltipWnd;
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
            HWND ttWnd = toolTipHelper.toolTipWnd;

            TOOLINFOW ti = {};
            ti.cbSize = sizeof(TOOLINFOW);
            ti.hwnd   = ttWnd;
            ti.uId    = (UINT_PTR)ttWnd;

            ::SendMessageW(ttWnd, TTM_TRACKACTIVATE, (WPARAM)FALSE, (LPARAM)&ti);

            if(!title.empty() || icon != NULL || iconEnum != None)
                ::SendMessageW(ttWnd, TTM_SETTITLEW, (WPARAM)TTI_NONE, (LPARAM)L"");
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
            toolTipHelper.lastTip = this;
            return;
        }
        if(toolTipWidget->hasRenderRule())
        {
            toolTipWidget->show(tipString,x,y);
            toolTipHelper.type    = ToolTipHelper::Custom;
            toolTipHelper.lastTip = this;
            return;
        }

        HWND ttWnd = toolTipHelper.toolTipWnd;
        if(toolTipHelper.lastTip != this)
        {
            TOOLINFOW ti = {};
            ti.cbSize   = sizeof(TOOLINFOW);
            ti.hwnd     = ttWnd;
            ti.uId      = (UINT_PTR)ttWnd;
            ti.lpszText = (LPWSTR)tipString.c_str();

            if(!title.empty() || icon != NULL || iconEnum != None)
            {
                WPARAM ic = icon == NULL ? (WPARAM)iconEnum : (WPARAM)icon;
                ::SendMessageW(ttWnd, TTM_SETTITLEW, ic, (LPARAM)title.c_str());
            }

            ::SendMessageW(ttWnd, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
            ::SendMessageW(ttWnd, TTM_TRACKACTIVATE, (WPARAM)TRUE, (LPARAM)&ti);
        } 

        ::SendMessageW(ttWnd, TTM_TRACKPOSITION, 0, (LPARAM)MAKELONG(x,y));
        toolTipHelper.type    = ToolTipHelper::System;
        toolTipHelper.lastTip = this;
    }





    // ========== MFont ==========
    struct MFontData
    {
        enum FontStyle
        {
            None      = 0,
            Bold      = 0x1,
            Italic    = 0x2
        };
        inline MFontData();
        inline ~MFontData();

        HFONT fontHandle;
        int refCount;
        unsigned int ptSize;
        unsigned int style;
        
        std::wstring faceName;

        typedef std::tr1::unordered_multimap<std::wstring, MFont> FontCache;
        static FontCache cache;
    };
    inline MFontData::MFontData():fontHandle(NULL),refCount(0),style(None),ptSize(9){}
    inline MFontData::~MFontData(){ if(fontHandle != NULL) ::DeleteObject(fontHandle); }
    MFontData::FontCache MFontData::cache;
    
    MFont::~MFont()
    {
        --data->refCount;
        if(data->refCount == 0)
            delete data;
    }

    MFont::MFont()
    {
        static bool inited = false;
        static MFontData defaultData;
        if(inited == false)
        {
            LOGFONTW lf;
            memset(&lf,0,sizeof(LOGFONTW));
            lf.lfHeight = mApp->winDpi() / 72 * 9;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = ANTIALIASED_QUALITY;
            wcscpy_s(lf.lfFaceName,L"Arial");
            defaultData.fontHandle = ::CreateFontIndirectW(&lf);
            defaultData.refCount = 1;
            defaultData.faceName = L"Arial";
            inited = true;
        }

        data = &defaultData;
        ++defaultData.refCount;
    }

    MFont::MFont(const LOGFONTW& lf)
    {
        std::wstring faceName(lf.lfFaceName);
        unsigned int style = lf.lfWeight == FW_BOLD ? MFontData::Bold : MFontData::None;
        if(lf.lfItalic) style |= MFontData::Italic;
        unsigned int ptSize = lf.lfHeight * 72 / mApp->winDpi();

        std::pair<MFontData::FontCache::iterator, MFontData::FontCache::iterator>
            cIts = MFontData::cache.equal_range(faceName);
        MFontData::FontCache::iterator& it = cIts.first;
        while(it != cIts.second)
        {
            MFontData* d = it->second.data;
            if(d->style == style && d->ptSize == ptSize)
            {
                data = d;
                ++data->refCount;
                return;
            }
            ++it;
        }

        data = new MFontData();
        data->fontHandle = ::CreateFontIndirectW(&lf);
        data->refCount   = 1;
        data->style      = style;
        data->ptSize     = ptSize;
        data->faceName   = faceName;
        MFontData::cache.insert(MFontData::FontCache::value_type(faceName,*this));
    }

    MFont::MFont(const MFont& rhs)
    {
        data = rhs.data;
        ++data->refCount;
    }

    MFont::MFont(unsigned int size, const std::wstring& faceName,
        bool bold, bool italic, bool pixelUnit)
    {
        // Search for sharing.
        unsigned int style = bold ? MFontData::Bold : MFontData::None;
        if(italic) style |= MFontData::Italic;
        unsigned int ptSize = pixelUnit ? size * 72 / mApp->winDpi() : size;

        std::pair<MFontData::FontCache::iterator, MFontData::FontCache::iterator>
            cIts = MFontData::cache.equal_range(faceName);
        MFontData::FontCache::iterator& it = cIts.first;
        while(it != cIts.second)
        {
            MFontData* d = it->second.data;
            if(d->style == style && d->ptSize == ptSize)
            {
                data = d;
                ++data->refCount;
                return;
            }
            ++it;
        }

        LOGFONTW lf;
        memset(&lf,0,sizeof(LOGFONTW));
        lf.lfHeight = 0 - (pixelUnit ? size : size * mApp->winDpi() / 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
        lf.lfItalic = italic ? TRUE : FALSE;
        lf.lfQuality = ANTIALIASED_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH;
        wcscpy_s(lf.lfFaceName, faceName.c_str());

        data = new MFontData();
        data->fontHandle = ::CreateFontIndirectW(&lf);
        data->faceName   = faceName;
        data->refCount   = 1;
        data->style      = style;
        data->ptSize     = ptSize;

        MFontData::cache.insert(MFontData::FontCache::value_type(faceName,*this));
    }

    HFONT MFont::getHandle()
        { return data->fontHandle; }
    bool MFont::isBold() const
        { return (data->style & MFontData::Bold)   != 0; }
    bool MFont::isItalic() const
        { return (data->style & MFontData::Italic) != 0; }
    const std::wstring& MFont::getFaceName() const
        { return data->faceName; }
    unsigned int MFont::pointSize() const
        { return data->ptSize; }

    const MFont& MFont::operator=(const MFont& rhs)
    {
        --data->refCount;
        if(data->refCount == 0)
            delete data;
        data = rhs.data;
        ++data->refCount;
        return *this;
    }

    MFont::operator HFONT() const
        { return data->fontHandle; }





    // ========== MCursor ==========
    MCursor* MCursor::appCursor = 0;
    const  MCursor* MCursor::showingCursor = 0;
    extern MCursor gArrowCursor;
    MCursor* MCursor::setAppCursor(MCursor* c)
    {
        if(appCursor == c)
            return 0;
        MCursor* oldCursor = appCursor;
        appCursor = c;
        if(appCursor != 0)
            appCursor->show();
        else
            gArrowCursor.show();

        return oldCursor;
    }
    void MCursor::setType(CursorType t)
    {
        M_ASSERT(t != CustomCursor);
        if(e_type == t)
            return;

        HCURSOR newHandle = NULL;
        if(t == BlankCursor)
        {
            BYTE andMask[] = { 0xFF };
            BYTE xorMask[] = { 0 };
            newHandle = ::CreateCursor(mApp->getAppHandle(),0,0,8,1,andMask,xorMask);
        } else {
            LPWSTR c;
            switch(t)
            {
                case ArrowCursor:       c = IDC_ARROW;       break;
                case CrossCursor:       c = IDC_CROSS;       break;
                case HandCursor:        c = IDC_HAND;        break;
                case HelpCursor:        c = IDC_HELP;        break;
                case IBeamCursor:       c = IDC_IBEAM;       break;
                case WaitCursor:        c = IDC_WAIT;        break;
                case SizeAllCursor:     c = IDC_SIZEALL;     break;
                case SizeVerCursor:     c = IDC_SIZENS;      break;
                case SizeHorCursor:     c = IDC_SIZEWE;      break;
                case SizeBDiagCursor:   c = IDC_SIZENESW;    break;
                case SizeFDiagCursor:   c = IDC_SIZENWSE;    break;
                case AppStartingCursor: c = IDC_APPSTARTING; break;
                case ForbiddenCursor:   c = IDC_NO;          break;
                case UpArrowCursor:     c = IDC_UPARROW;     break;
            }
            newHandle = ::LoadCursorW(NULL, c);
        }

        updateCursor(newHandle, t);
    }

    void MCursor::updateCursor(HCURSOR h, CursorType t)
    {
        if(h == NULL)
        {
            h = ::LoadCursorW(NULL,IDC_ARROW);
            t = ArrowCursor; 
        }

        HCURSOR oldCursor = handle;
        if(showingCursor == this)
            ::SetCursor(h);
        if(e_type == BlankCursor || e_type == CustomCursor)
            ::DestroyCursor(oldCursor);

        handle = h;
        e_type = t;
    }

    MCursor::~MCursor()
    {
        if(showingCursor == this)
        {
            showingCursor = 0;
            if(e_type != ArrowCursor)
                ::SetCursor(::LoadCursorW(NULL,IDC_ARROW));
        }

        if(e_type == BlankCursor || e_type == CustomCursor)
            ::DestroyCursor(handle);
    }

    void MCursor::show(bool forceUpdate) const
    {
        if(appCursor != 0)
            return;

        if(!forceUpdate)
        {
            if(showingCursor == this)
                return;

            if(showingCursor != 0)
            {
                if(showingCursor->e_type != CustomCursor)
                {
                    if(showingCursor->e_type == e_type)
                        return;
                } else if(showingCursor->handle == handle)
                    return;
            }
        }

        showingCursor = this;
        ::SetCursor(handle);
    }





    // ========== MTimer ==========
    // TimerId - Interval
    typedef std::tr1::unordered_map<unsigned int, unsigned int> TimerIdMap;
    // Interval - MTimer
    typedef std::tr1::unordered_multimap<unsigned int, MTimer*> TimerHash;
    struct TimerHelper
    {
        ~TimerHelper() { cleanUp(); }
        void cleanUp();

        TimerHash  timerHash;
        TimerIdMap timerIdMap;
    };
    static TimerHelper timerHelper;

    void TimerHelper::cleanUp()
    {
        TimerIdMap::iterator it    = timerHelper.timerIdMap.begin();
        TimerIdMap::iterator itEnd = timerHelper.timerIdMap.end();
        while(it != itEnd)
        {
            ::KillTimer(NULL,it->first);
            ++it;
        }
        timerHelper.timerIdMap.clear();
        TimerHash::iterator tit    = timerHelper.timerHash.begin();
        TimerHash::iterator titEnd = timerHelper.timerHash.end();
        while(tit != titEnd)
        {
            tit->second->b_active = false;
            tit->second->m_id = 0;
            ++tit;
        }
        timerHelper.timerHash.clear();
    }

    void MTimer::cleanUp()
        { timerHelper.cleanUp(); }
    void MTimer::start()
    {
        if(b_active) return;
        b_active = true;
        TimerHash::iterator it    = timerHelper.timerHash.find(m_interval);
        TimerHash::iterator itEnd = timerHelper.timerHash.end();
        if(it == itEnd)
        {
            m_id = ::SetTimer(NULL,0,m_interval,timerProc);
            timerHelper.timerIdMap.insert(TimerIdMap::value_type(m_id,m_interval));
        } else
        {
            m_id = it->second->m_id;
        }
        timerHelper.timerHash.insert(TimerHash::value_type(m_interval,this));
    }
    void MTimer::stop()
    {
        if(!b_active) return;
        b_active = false;
        TimerHash::iterator it    = timerHelper.timerHash.find(m_interval);
        TimerHash::iterator itEnd = timerHelper.timerHash.end();
        while(it != itEnd) { if(it->second == this) break; }
        timerHelper.timerHash.erase(it);
        it = timerHelper.timerHash.find(m_interval);
        if(it == itEnd) { 
            timerHelper.timerIdMap.erase(m_id);
            ::KillTimer(NULL,m_id);
        }
        m_id = 0;
    }

    void __stdcall MTimer::timerProc(HWND, UINT, UINT_PTR id, DWORD)
    {
        TimerIdMap::iterator it    = timerHelper.timerIdMap.find(id);
        TimerIdMap::iterator itEnd = timerHelper.timerIdMap.end();
        if(it == itEnd)
            return;
        unsigned int interval = it->second;
        std::pair<TimerHash::iterator,TimerHash::iterator> range =
            timerHelper.timerHash.equal_range(interval);
        while(range.first != range.second)
        {
            MTimer* timer = range.first->second;
            if(timer->isSingleShot()) {
                timer->stop();
                range = timerHelper.timerHash.equal_range(interval);
            } else
                ++range.first;
            timer->timeout();
        }
    }

    void MTimer::setInterval(unsigned int msec)
    {
        m_interval = msec;
        if(!b_active) return;
        stop();
        start();
    }





    // ========== MShortCut ==========
    MShortCut::ShortCutMap MShortCut::scMap;
    inline void MShortCut::removeFromMap()
    {
        std::pair<ShortCutMap::iterator, ShortCutMap::iterator> range = scMap.equal_range(getKey());
        ShortCutMap::iterator it = range.first;
        while(it != range.second)
        {
            if(it->second == this)
            {
                scMap.erase(it);
                break;
            }
            ++it;
        }
    }
    void MShortCut::setGlobal(bool on)
    {
        if(global == on)
            return;
        global = on;

        if(global)
        {
            target = 0;
            if(modifier == 0 || virtualKey == 0)
                return;
            unsigned int mod = modifier & (~KeypadModifier) | MOD_NOREPEAT;
            ::RegisterHotKey(0,getKey(),mod,virtualKey);
        } else {
            ::UnregisterHotKey(0,getKey());
        }
    }
    void MShortCut::setKey(unsigned int m, unsigned int v)
    {
        if(m == modifier && v == virtualKey)
            return;
        enabled = true;
        if(global)
            ::UnregisterHotKey(0,getKey());
        removeFromMap();
        modifier = m;
        virtualKey = v;
        if(global) {
            global = false;
            setGlobal(true);
        }
        insertToMap();
    }

    void MShortCut::getMachedShortCuts(unsigned int modifier, unsigned int virtualKey,
        std::vector<MShortCut*>& scs)
    {
        scs.clear();
        unsigned int key = (modifier << 26) | virtualKey;
        std::pair<ShortCutMap::iterator, ShortCutMap::iterator> range = scMap.equal_range(key);
        ShortCutMap::iterator it = range.first;
        while(it != range.second)
        {
            if(it->second->isEnabled())
                scs.push_back(it->second);
            ++it;
        }
    }





    // ========== MResource ==========
    MResource::ResourceCache MResource::cache;
    bool MResource::open(HINSTANCE hInstance, const wchar_t* name, const wchar_t* type)
    {
        HRSRC hrsrc = ::FindResourceW(hInstance,name,type);
        if(hrsrc) {
            HGLOBAL hglobal = ::LoadResource(hInstance,hrsrc);
            if(hglobal)
            {
                buffer = (const BYTE*)::LockResource(hglobal);
                if(buffer) {
                    size = ::SizeofResource(hInstance,hrsrc);
                    return true;
                }
            }
        }
        return false;
    }

    bool MResource::open(const std::wstring& fileName)
    {
        ResourceCache::const_iterator iter;
        if(fileName.at(0) == L':' && fileName.at(1) == L'/')
        {
            std::wstring temp(fileName,2,fileName.length() - 2);
            iter = cache.find(temp);
        } else {
            iter = cache.find(fileName);
        }
        if(iter == cache.cend())
            return false;

        buffer = iter->second->buffer;
        size   = iter->second->length;
        return true;
    }

    void MResource::clearCache()
    {
        ResourceCache::const_iterator it    = cache.cbegin();
        ResourceCache::const_iterator itEnd = cache.cend();
        while(it != itEnd) { delete it->second; ++it; }
        cache.clear();
    }

    bool MResource::addFileToCache(const std::wstring& filePath)
    {
        if(cache.find(filePath) != cache.cend())
            return false;

        HANDLE resFile = ::CreateFileW(filePath.c_str(),GENERIC_READ,
            FILE_SHARE_READ,0,OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,0);
        if(resFile == INVALID_HANDLE_VALUE)
            return false;

        ResourceEntry* newEntry = new ResourceEntry();

        DWORD size = ::GetFileSize(resFile,0);
        newEntry->length = size;
        newEntry->unity  = true;
        newEntry->buffer = new BYTE[size];

        DWORD readSize;
        if(::ReadFile(resFile,newEntry->buffer,size,&readSize,0))
        {
            if(readSize != size) {
                delete newEntry;
            } else {
                cache.insert(ResourceCache::value_type(filePath, newEntry));
                return true;
            }
        } else {
            delete newEntry;
        }
        return false;
    }

    bool MResource::addZipToCache(const std::wstring& zipPath)
    {
        HZIP zip = OpenZip((void*)zipPath.c_str(), 0, ZIP_FILENAME);
        if(zip == 0)
            return false;

        bool success = true;
        ZIPENTRYW zipEntry;
        if(ZR_OK == GetZipItem(zip, -1, &zipEntry))
        {
            std::vector<std::pair<int,ResourceEntry*> > tempCache;

            int totalItemCount = zipEntry.index;
            int buffer = 0;
            // Get the size of every item.
            for(int i = 0; i < totalItemCount; ++i)
            {
                GetZipItem(zip,i,&zipEntry);
                if(!(zipEntry.attr & FILE_ATTRIBUTE_DIRECTORY)) // ignore every folder
                {
                    ResourceEntry* entry = new ResourceEntry();
                    entry->buffer += buffer;
                    entry->length = zipEntry.unc_size;

                    cache.insert(ResourceCache::value_type(zipEntry.name,entry));
                    tempCache.push_back(std::make_pair(i,entry));

                    buffer += zipEntry.unc_size;
                }
            }

            BYTE* memBlock = new BYTE[buffer]; // buffer is the size of the zip file.

            // Uncompress every item.
            totalItemCount = tempCache.size();
            for(int i = 0; i < totalItemCount; ++i)
            {
                ResourceEntry* re = tempCache.at(i).second;
                re->buffer = memBlock + (int)re->buffer;

                UnzipItem(zip, tempCache.at(i).first,
                    re->buffer, re->length, ZIP_MEMORY);
            }

            tempCache.at(0).second->unity = true;
        } else {
            success = false;
        }

        CloseZip(zip);
        return success;
    }
}