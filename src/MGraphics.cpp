#include "MGraphics.h"
#include "private/MGraphicsData.h"
#include "MWidget.h"
#include "private/MWidget_p.h"

#include "private/MD2DGraphicsData.h"
#include "private/MGDIGraphicsData.h"

namespace MetalBone
{
#ifdef MB_USE_D2D
    ID2D1RenderTarget* MD2DGraphics::getRecentRenderTarget() { return MD2DGraphicsData::getRecentRenderTarget(); }
    ID2D1RenderTarget* MD2DGraphics::getRenderTarget()       { return ((MD2DGraphicsData*)data)->getRenderTarget(); }
#endif

    MGraphics::MGraphics(MWidget* w):
        data(w->windowWidget()->m_windowExtras->m_graphicsData){}

    void MGraphics::clear    ()               { data->clear();        }
    void MGraphics::clear    (const MRect& r) { data->clear(r);       }
    void MGraphics::pushClip (const MRect& r) { data->pushClip(r);    }
    void MGraphics::popClip  ()               { data->popClip();      }
    HDC  MGraphics::getDC    ()               { return data->getDC(); }
    void MGraphics::releaseDC(const MRect& u) { data->releaseDC(u);   }

    void MGraphics::drawImage(MImageHandle& i, const MRect& drawRect)
        { data->drawImage(i, drawRect); }

    void MGraphics::fill9PatchRect(MBrushHandle& b, const MRect& drawRect,
        bool scaleX, bool scaleY, const MRect* clipRect)
        { data->fill9PatchRect(b,drawRect,scaleX,scaleY,clipRect); }

    MGraphicsData* MGraphicsData::create(HWND hwnd, long width, long height)
    {
        // TODO: Even if we compile with MB_USE_D2D defined.
        // We can choose other drawing systems.
#ifdef MB_USE_D2D
        return new MD2DGraphicsData(hwnd,width,height);
#endif
        return 0;
    }
}