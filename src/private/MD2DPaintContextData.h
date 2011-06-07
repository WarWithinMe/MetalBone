#pragma once

#include "MBGlobal.h"
#include <d2d1.h>
#include <list>

namespace MetalBone
{
    class MRect;
    class D2DBrushHandle;
    class MWidget;
    class MD2DPaintContextData
    {
        public:
            explicit MD2DPaintContextData(MWidget* w):widget(w),renderTarget(0){ createRenderTarget(); }
            explicit MD2DPaintContextData(ID2D1HwndRenderTarget* rt):widget(0),renderTarget(rt){}
            ~MD2DPaintContextData() { SafeRelease(renderTarget); }

            void beginDraw();
            bool endDraw();
            void discardResources(bool recreateRenderTarget);
            void resize(long w, long h)                     { renderTarget->Resize(D2D1::SizeU(w,h)); }
            void setRenderTarget(ID2D1HwndRenderTarget* rt) { renderTarget = rt; }
            ID2D1HwndRenderTarget* getRenderTarget()        { return renderTarget; }


        private:
            MWidget*               widget;
            ID2D1HwndRenderTarget* renderTarget;

            static std::list<ID2D1HwndRenderTarget*> rtStack;

            void createRenderTarget();
            friend class MD2DPaintContext;
    };
};