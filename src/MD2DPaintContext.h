#pragma once
#include "MBGlobal.h"
#include "MUtils.h"

#include <d2d1.h>
#include <list>

namespace MetalBone
{
    class MWidget;
    class MD2DPaintContextData;
    class MD2DBrushHandle;
    class METALBONE_EXPORT MD2DPaintContext
    {
        public:
            explicit MD2DPaintContext(MWidget* w);
            explicit MD2DPaintContext(MD2DPaintContextData* d):data(d) { }

            void fill9PatchRect(MD2DBrushHandle& b, bool scaleX, bool scaleY,
                const MRect& rect, const MRect* clipRect = 0);

            static void discardResources();
            // Whenever we do drawing in responds to WM_PAINT, getRenderTarget()
            // will return the ID2D1RenderTarget of the window which needs to be
            // redraw. In other time, it returns 0;
            static ID2D1RenderTarget* getRecentRenderTarget();
            ID2D1RenderTarget* getRenderTarget();

        private:
            MD2DPaintContextData* data;
    };
}