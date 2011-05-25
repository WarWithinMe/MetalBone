#pragma once
#include "MRegion.h"
namespace MetalBone
{
    enum WidgetState
    {
        MWS_Polished    = 0x1,
        MWS_Hidden      = 0x2,
        MWS_CSSOpaqueBG = 0x4,
        MWS_Visible     = 0x8,
        MWS_UnderMouse  = 0x10,
        MWS_Focused     = 0x20,
        MWS_Pressed     = 0x40
    };

    typedef std::tr1::unordered_map<MWidget*,MRect>   DrawRectHash;
    typedef std::tr1::unordered_map<MWidget*,MRegion> DrawRegionHash;
    typedef std::tr1::unordered_map<MWidget*,bool>    DirtyChildrenHash;

    class MD2DPaintContextData;
    struct WindowExtras
    {
        inline WindowExtras();
        ~WindowExtras();

        DrawRectHash           updateWidgets;
        DrawRegionHash         passiveUpdateWidgets;
        DirtyChildrenHash      childUpdatedHash;
        std::wstring           m_windowTitle;
        // If m_wndHandle is a layered window, then we create a dummy window.
        // We send WM_PAINT messages to that window.
        HWND                   m_wndHandle;
        HWND                   m_dummyHandle;
        MD2DPaintContextData*  m_pcData;

        MWidget*               focusedWidget;
        MWidget*               widgetUnderMouse;
        MWidget*               currWidgetUnderMouse;
        int                    lastMouseX;
        int                    lastMouseY;
        bool                   bTrackingMouse;


        inline void clearUpdateQueue();
        inline void addToRepaintMap(MWidget* w,int left,int top,int right,int bottom);
    };

    extern void generateStyleFlags(unsigned int flags, DWORD* winStyleOut, DWORD* winExStyleOut);

    inline WindowExtras::WindowExtras():
        m_wndHandle(NULL), m_dummyHandle(NULL),
        m_pcData(0),bTrackingMouse(false),
        widgetUnderMouse(0), currWidgetUnderMouse(0), focusedWidget(0),
        lastMouseX(0), lastMouseY(0){}
    inline void WindowExtras::clearUpdateQueue()
    {
        updateWidgets.clear();
        passiveUpdateWidgets.clear();
        childUpdatedHash.clear();
    }
    inline void WindowExtras::addToRepaintMap(MWidget* w, int left, int top, int right, int bottom)
    {
        DrawRectHash::iterator iter = updateWidgets.find(w);
        if(iter != updateWidgets.end()) {
            MRect& updateRect  = iter->second;
            if(updateRect.left   > left  ) updateRect.left   = left;
            if(updateRect.top    > top   ) updateRect.top    = top;
            if(updateRect.right  < right ) updateRect.right  = right;
            if(updateRect.bottom < bottom) updateRect.bottom = bottom;
        } else {
            MRect& updateRect  = updateWidgets[w];
            updateRect.left   = left;
            updateRect.right  = right;
            updateRect.top    = top;
            updateRect.bottom = bottom;
        }
    }
}
