#pragma once

#include "MBGlobal.h"
#include <d2d1.h>
using namespace std;

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
            void discardResources();

            inline void resize(long w, long h) { renderTarget->Resize(D2D1::SizeU(w,h)); }

            void fill9PatchRect(const D2DBrushHandle& brush, const MRect& rect,
                const MRect& clipRect, bool scaleX, bool scaleY);

            void setRenderTarget(ID2D1HwndRenderTarget* rt) { renderTarget = rt; }
            ID2D1HwndRenderTarget* getRenderTarget() { return renderTarget; }

        private:
            MWidget* widget;
            ID2D1HwndRenderTarget* renderTarget;

            void createRenderTarget();

            friend class MD2DPaintContext;
    };
};