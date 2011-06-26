#pragma once

#include "MBGlobal.h"

#ifdef MB_USE_D2D

#include "MGraphicsData.h"
#include "MGraphicsResource.h"
#include <d2d1.h>
#include <list>

namespace MetalBone
{
    class MD2DGraphicsData : public MGraphicsData
    {
        public:
            MD2DGraphicsData(HWND hwnd, long w, long h):
              renderTarget(0),gdiRT(0),
              wndHandle(hwnd),width(w),height(h){ createRenderTarget(); }

            virtual void beginDraw();
            virtual bool endDraw(const MRect& updatedRect);

            virtual void clear();
            virtual void clear(const MRect&);

            virtual void pushClip(const MRect&);
            virtual void popClip();

            virtual HDC  getDC();
            virtual void releaseDC(const MRect& updatedRect);

            virtual void drawRenderRule(CSS::RenderRuleData*,
                const MRect& widgetRect, const MRect& clipRect,
                const std::wstring& text, CSS::FixGDIAlpha fix,
                unsigned int frameIndex);

            virtual void drawImage     (MImageHandle& i, const MRect& drawRect);
            virtual void fill9PatchRect(MBrushHandle& b, const MRect& drawRect,
                bool scaleX, bool scaleY, const MRect* clipRect);

            virtual void resize(long w,long h)
                { width = w; height = h; renderTarget->Resize(D2D1::SizeU(w,h)); } 

            inline ID2D1RenderTarget* getRenderTarget() { return renderTarget; }
            inline static ID2D1RenderTarget* getRecentRenderTarget()
                { M_ASSERT(rtStack.size() != 0); return *rtStack.rbegin(); }

        private:
            void createRenderTarget();
            void drawGdiText(CSS::RenderRuleData*, const MRect& borderRect, const MRect& clipRect,
                const std::wstring& text, CSS::FixGDIAlpha);
            void drawD2DText(CSS::RenderRuleData*, const D2D1_RECT_F& borderRect,
                const std::wstring& text);

            static std::list<ID2D1RenderTarget*> rtStack;
            ID2D1HwndRenderTarget* renderTarget;
            ID2D1GdiInteropRenderTarget* gdiRT;
            HWND wndHandle;
            unsigned int width;
            unsigned int height;
    };
}

#endif
