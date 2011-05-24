#pragma once

#include "MStyleSheet.h"

namespace MetalBone
{
    struct MApplicationData
    {
        inline MApplicationData(bool hwAccelerated);
        // Window procedure
        static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);

        template<bool matchDummy>
        MWidget* findWidgetByHandle(HWND handle) const;

        static inline void removeTLW(MWidget* w);
        static inline void insertTLW(MWidget* w);

        std::set<MWidget*>  topLevelWindows;
        bool                quitOnLastWindowClosed;
        bool                hardwareAccelerated;
        HINSTANCE           appHandle;
        MStyleSheetStyle    ssstyle;

        ID2D1Factory*       d2d1Factory;
        IDWriteFactory*     dwriteFactory;
        IWICImagingFactory* wicFactory;

        ULONG_PTR           gdiPlusToken;

        static MApplication::WinProc customWndProc;
        static MApplicationData*     instance;

        static void setFocusWidget(MWidget*);
    };
    
    extern wchar_t gMWidgetClassName[];
    extern MCursor gArrowCursor;

    inline MApplicationData::MApplicationData(bool hwAccelerated):
        quitOnLastWindowClosed(true), hardwareAccelerated(hwAccelerated)
        { instance = this; }
    inline void MApplicationData::removeTLW(MWidget* w)
        { if(instance) instance->topLevelWindows.erase(w); }
    inline void MApplicationData::insertTLW(MWidget* w)
        { if(instance) instance->topLevelWindows.insert(w); }
}
