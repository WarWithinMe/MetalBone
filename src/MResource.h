#pragma once

#include "MBGlobal.h"
#include "MApplication.h"

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <unordered_map>
#include <map>

// MIcon
// MSysTrayIcon
// MToolTip
// MFont
// MCursor
// MTimer
// MResource
// MShortcut
namespace MetalBone
{
    class METALBONE_EXPORT MIcon
    {
        public:
            // This class is a wrapper of the HICON handle.
            // You have to create the icon using Win32 API.

            // Construct a empty icon.
            MIcon():data(0){}
            ~MIcon();

            // If autoDestroy is true, the HICON icon will be deleted
            // when every copy of this MIcon object is deleted.
            explicit MIcon(HICON icon, bool autoDestroy = false);

            // The MIcon objects will share the same HICON if they're 
            // copyed or constructed using copy-construtor.
            MIcon(const MIcon&);
            const MIcon& operator=(const MIcon&);

            void setHandle(HICON icon, bool autoDestroy);
            operator HICON() const;
            operator bool()  const;
            bool operator==(const MIcon&) const;

        private:
            struct MIconData* data;
    };

    class METALBONE_EXPORT MSysTrayIcon
    {
        public:
            enum BalloonIcon
            {
                None    = 0,
                Info    = 1,
                Warning = 2,
                Error   = 3,
                Custom  = 4
            };

            MSysTrayIcon();
            virtual ~MSysTrayIcon();

            void setName(const std::wstring&);
            inline const std::wstring& getName() const;
            // Return the previous set icon.
            void setIcon(MIcon);
            inline MIcon getIcon() const;

            void show();
            void hide();

            bool showBalloonMessage(const std::wstring& message,
                const std::wstring& title = std::wstring(),
                BalloonIcon iconEnum = None,
                MIcon customIcon = MIcon());

        protected:
            virtual void event(unsigned int msg){}

        private:
            std::wstring   name;
            MIcon          icon;
            unsigned short id;
            bool           isVisible;

            typedef std::map<unsigned short, MSysTrayIcon*> TrayIconMap;

            static TrayIconMap    trayIconMap;
            static unsigned short cid;
            static HWND           trayWnd;
            static LRESULT CALLBACK trayWndProc(HWND, UINT, WPARAM, LPARAM);
            void reshow();
            MB_DISABLE_COPY(MSysTrayIcon);
    };

    class METALBONE_EXPORT MToolTip
    {
        public:
            enum HidePolicy { WhenMoveOut, WhenMove };

            enum Icon
            {
                None         = TTI_NONE,
                Info         = TTI_INFO,
                Warning      = TTI_WARNING,
                Error        = TTI_ERROR,
                LargeInfo    = TTI_INFO_LARGE,
                LargeWarning = TTI_WARNING_LARGE,
                LargeError   = TTI_ERROR_LARGE
            };

            // MToolTip do not take control of the MWidget,
            // It's the responsibility of the user to delete the widget.
            explicit MToolTip(const std::wstring& tip = std::wstring(),
                MWidget* w = 0, HidePolicy hp = WhenMoveOut);
            ~MToolTip();

            inline void       setHidePolicy(HidePolicy);
            inline HidePolicy hidePolicy() const;

            // If the toolTip is shown when you are setting the data for it,
            // you must hide and show it again to take effect.
            inline void setText(const std::wstring&);
            // If there's any HICON/MWidget previous set, this function will return
            // that HICON/MWidget, you must free the returned HICON/MWidget yourself.
            // The HICON/MWidget parameter passed in this function is deleted when
            // this class is deleted.
            // If you set the delegate to w, whenever the tooltip shows up, it won't
            // show up, instead, it calls MWidget::show() for the w.
            inline MWidget* setDelegate(MWidget* w);
            inline HICON    setExtra(const std::wstring& title, HICON icon = NULL);
            inline HICON    setExtra(const std::wstring& title, Icon);

            inline const std::wstring& getTitle()  const;
            inline const std::wstring& getText()   const;
            inline MWidget*            getWidget() const;

            void show(long x, long y);
            void hide();
            bool isShowing() const;

            // If there's a visible tooltip, this function will hide it.
            static void hideAll();

        private:
            HidePolicy   hp;
            std::wstring tipString;
            std::wstring title;
            HICON        icon;
            Icon         iconEnum;
            MWidget*     customWidget;
            MB_DISABLE_COPY(MToolTip);
    };


    struct MFontData;
    class METALBONE_EXPORT MFont
    {
        // A wrapper class for HFONT
        public:
            MFont(unsigned int      size,
                const std::wstring& faceName,
                bool bold      = false,
                bool italic    = false,
                bool pixelUnit = true);
            explicit MFont(const LOGFONTW&);
            MFont(const MFont&);
            MFont();
            ~MFont();

            HFONT getHandle();
            bool  isBold()   const;
            bool  isItalic() const;
            const std::wstring& getFaceName() const;
            unsigned int pointSize() const;

            operator HFONT() const;
            const MFont& operator=(const MFont&);

        private:
            MFontData* data;
    };


    class METALBONE_EXPORT MCursor
    {
        // TODO: Add support for creating cursor dynamically.
        public:
            enum CursorType
            {
                ArrowCursor,
                AppStartingCursor,
                CrossCursor,
                HandCursor,
                HelpCursor,
                IBeamCursor,
                WaitCursor,
                ForbiddenCursor,
                UpArrowCursor,

                SizeAllCursor,
                SizeVerCursor,
                SizeHorCursor,
                SizeBDiagCursor,
                SizeFDiagCursor,

                BlankCursor,

                CustomCursor
            };

            inline explicit MCursor(CursorType t = ArrowCursor);
            ~MCursor();

            void setType(CursorType t);
            inline CursorType type() const;

            inline HCURSOR getHandle() const;

            inline void loadCursorFromFile(const std::wstring&);
            // Loads a cursor from a resource embeded into this exe.
            inline void loadCursorFromRes(LPWSTR lpCursorName);

            // Sets the app's cursor to c, return the previous app's cursor or 0.
            // If the app's cursor doesn't change, it returns 0.
            // The MCursor c will be deleted when the application exits.
            // But you must free the returned cursor yourself.
            static MCursor* setAppCursor(MCursor* c);
            inline static MCursor* getAppCursor();
            inline static void     setPos(int x, int y);
            inline static POINT    pos();

            void show(bool forceUpdate = false) const;

        private:

            CursorType e_type;
            HCURSOR    handle;

            static MCursor*       appCursor;
            static const MCursor* showingCursor;

            void updateCursor(HCURSOR, CursorType);
            MB_DISABLE_COPY(MCursor);
    };

    class METALBONE_EXPORT MTimer
    {
        // A wrapper class for Win32 SetTimer and KillTimer
        // The interface is a copy of Qt's timer class - QTimer
        public:
            inline MTimer();
            inline ~MTimer();

            // Stops every MTimer
            static void cleanUp();

            inline bool isActive()         const;
            inline bool isSingleShot()     const;
            inline unsigned int timerId()  const;
            inline unsigned int interval() const;

            inline void setSingleShot(bool);
            void setInterval(unsigned int msec);

            // Starts the timer with msec as the interval.
            inline void start(unsigned int msec);
            void start();
            void stop();

            Signal0<> timeout;


        private:
            static void __stdcall timerProc(HWND, UINT, UINT_PTR, DWORD);
            unsigned int m_interval;
            unsigned int m_id;
            bool         b_active;
            bool         b_singleshot;

            MB_DISABLE_COPY(MTimer);
        friend struct TimerHelper;
    };


    class METALBONE_EXPORT MResource
    {
        // Use for small resource files like theme images.
        public:
            inline MResource();

            inline const BYTE*  byteBuffer() const;
            inline unsigned int length()     const;

            // Open a file from the cache, if a file is already opened,
            // it will be closed.
            bool open(const std::wstring& fileName);
            // Open a file from a module, if a file is already opened,
            // it will be closed.
            bool open(HINSTANCE hInstance,
                    const wchar_t* name,
                    const wchar_t* type);

            static void clearCache();
            static bool addFileToCache(const std::wstring& filePath);
            static bool addZipToCache(const std::wstring& zipPath);

#ifdef MB_DEBUG
            static void dumpCache();
#endif

        private:
            const BYTE*  buffer;
            unsigned int size;
            struct ResourceEntry
            {
                BYTE* buffer;
                DWORD length;
                bool  unity;
                ResourceEntry():buffer(0),length(0),unity(false){}
                ~ResourceEntry() { if(unity) delete[] buffer; }
            };
            typedef std::tr1::unordered_multimap<std::wstring,ResourceEntry*> ResourceCache;
            static ResourceCache cache;
    };


    class METALBONE_EXPORT MShortCut
    {
        public:
            inline MShortCut(unsigned int modifier,
                unsigned int virtualKey,
                MWidget*     target = 0,
                bool         global = false);
            inline MShortCut();
            inline ~MShortCut();

            void setKey(unsigned int modifier, unsigned int virtualKey);
            void setGlobal(bool);
            // Setting the target to a widget will make shortcut invoke
            // only when the target has focus.
            // Setting it to 0 will invoke the shortcut when any window
            // of this app is actived.
            inline void setTarget(MWidget*);
            inline void setEnabled(bool);

            inline bool isGlobal() const;
            inline bool isEnabled() const;
            inline unsigned int getModifier()   const;
            inline unsigned int getVirtualKey() const;
            inline MWidget* getTarget() const;

            static void getMachedShortCuts(unsigned int modifier,
                unsigned int virtualKey, std::vector<MShortCut*>&);

            Signal0<> invoked;


        private:
            unsigned int modifier;
            unsigned int virtualKey;
            MWidget*     target;
            bool         global;
            bool         enabled;

            typedef std::tr1::unordered_multimap<unsigned int, MShortCut*> ShortCutMap;
            static ShortCutMap scMap;

            inline void removeFromMap();
            inline void insertToMap();
            inline unsigned int getKey();

            MB_DISABLE_COPY(MShortCut);
    };



    inline MIcon MSysTrayIcon::getIcon() const
        { return icon; }



    inline void MToolTip::setHidePolicy(HidePolicy p)
        { hp = p; }
    inline MToolTip::HidePolicy MToolTip::hidePolicy() const
        { return hp; }
    inline const std::wstring& MToolTip::getText() const
        { return tipString; }
    inline const std::wstring& MToolTip::getTitle() const
        { return title; }
    inline MWidget* MToolTip::getWidget() const
        { return customWidget; }
    inline void MToolTip::setText(const std::wstring& t)
        { tipString = t; }
    inline HICON MToolTip::setExtra(const std::wstring& t, Icon i)
    {
        title = t;
        iconEnum = i;
        icon = NULL;
        return icon;
    }
    inline HICON MToolTip::setExtra(const std::wstring& t, HICON i)
    {
        HICON oldIcon = icon;
        title = t;
        icon  = i;
        return oldIcon;
    }
    inline MWidget* MToolTip::setDelegate(MWidget* w)
    { 
        hide();
        MWidget* oldW = customWidget;
        customWidget = w;
        return oldW;
    }




    MShortCut::MShortCut():modifier(0),virtualKey(0),target(0),global(false),enabled(false){}
    MShortCut::MShortCut(unsigned int m, unsigned int vk, MWidget* t, bool g):
        modifier(m),virtualKey(vk),target(t),global(false),enabled(true)
        { insertToMap(); if(g) setGlobal(true); }
    MShortCut::~MShortCut()
        { removeFromMap(); }
    inline bool MShortCut::isGlobal() const
        { return global; }
    inline bool MShortCut::isEnabled() const
        { return enabled; }
    inline unsigned int MShortCut::getModifier() const
        { return modifier; }
    inline unsigned int MShortCut::getVirtualKey() const
        { return virtualKey; }
    inline MWidget* MShortCut::getTarget() const
        { return target; }
    inline void MShortCut::setTarget(MWidget* t)
        { target = t; }
    inline void MShortCut::setEnabled(bool on)
        { enabled = on; }
    inline void MShortCut::insertToMap()
        { scMap.insert(ShortCutMap::value_type(getKey(),this)); }
    inline unsigned int MShortCut::getKey()
        { return (modifier << 26) | virtualKey; }



    inline MResource::MResource():buffer(0),size(0){}
    inline const BYTE*  MResource::byteBuffer() const
        { return buffer; }
    inline unsigned int MResource::length() const
        { return size; }



    inline MCursor::MCursor(CursorType t):e_type(CustomCursor)
        { setType(t); }
    inline MCursor::CursorType MCursor::type() const
        { return e_type; }
    inline void MCursor::setPos(int x, int y)
        { ::SetCursorPos(x,y); }
    inline POINT MCursor::pos()
        { POINT p; ::GetCursorPos(&p); return p; }
    inline HCURSOR MCursor::getHandle() const
        { return handle; }
    inline MCursor* MCursor::getAppCursor()
        { return appCursor; }
    inline void MCursor::loadCursorFromFile(const std::wstring& path)
        { updateCursor(::LoadCursorFromFileW(path.c_str()),CustomCursor); }
    inline void MCursor::loadCursorFromRes(LPWSTR res)
        { updateCursor(::LoadCursorW(mApp->getAppHandle(),res),CustomCursor); }



    inline MTimer::MTimer():m_interval(0),m_id(0),
        b_active(false),b_singleshot(false){}
    inline MTimer::~MTimer() { stop(); }
    inline bool MTimer::isActive() const
        { return b_active; }
    inline bool MTimer::isSingleShot() const
        { return b_singleshot; }
    inline unsigned int MTimer::interval() const
        { return m_interval; }
    inline unsigned int MTimer::timerId() const
        { return m_id; } 
    inline void MTimer::setSingleShot(bool single)
        { b_singleshot = single; }
    inline void MTimer::start(unsigned int msec)
        { m_interval = msec; start(); }
}
