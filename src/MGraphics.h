#pragma once
#include "MBGlobal.h"
#include "MUtils.h"
#include "MApplication.h"

namespace MetalBone
{
    class MWidget;
    class MBrushHandle;
    class MImageHandle;
    class MGraphicsData;
    namespace CSS { class RenderRule; }
    class METALBONE_EXPORT MGraphics
    {
        public:
            explicit MGraphics(MWidget* w);
            explicit MGraphics(MGraphicsData* d):data(d){}

            // Clipping
            void pushClip (const MRect& clipRect);
            void popClip  ();

            void clear    ();
            void clear    (const MRect& clearRect);

            // We can always get the HDC of a window to draw on.
            // We should call releaseDC() after we finish using the DC.
            // The updatedRect may be ignored depending on what Graphics
            // Backend it is.
            HDC  getDC    ();
            void releaseDC(const MRect& updatedRect);

            // Common drawing functions.
            void drawImage      (MImageHandle& image, const MRect& drawRect);
            void fill9PatchRect (MBrushHandle& brush, const MRect& drawRect,
                bool scaleX, bool scaleY, const MRect* clipRect = 0);

        protected:
            MGraphicsData* data;
            friend class CSS::RenderRule;
    };
}

#ifdef MB_USE_D2D
struct ID2D1RenderTarget;
namespace MetalBone
{
    // ======================================================
    //  A convenient class for getting the ID2D1RenderTarget 
    //  of a window, to do custom drawing. It has no use if
    //  the Graphics Backend is not Direct2D
    // ======================================================

    class METALBONE_EXPORT MD2DGraphics : public MGraphics
    {
        public:
            explicit MD2DGraphics(MWidget* w):MGraphics(w)
                { M_ASSERT(mApp->getGraphicsBackend() == MApplication::Direct2D); }
            explicit MD2DGraphics(MGraphicsData* d):MGraphics(d)
                { M_ASSERT(mApp->getGraphicsBackend() == MApplication::Direct2D); }

            ID2D1RenderTarget* getRenderTarget();
            // Get the latest ID2D1RenderTarget which called BeginDraw()
            static ID2D1RenderTarget* getRecentRenderTarget();
    };
}
#endif
#ifdef MB_USE_SKIA
class SkCanvas;
namespace MetalBone
{
    // ==================================================
    //  A convenient class for getting the SkCanvas of a 
    //  window, to do custom drawing. It has no use if
    //  the Graphics Backend is not Skia 
    // ==================================================

    class METALBONE_EXPORT MSkiaGraphics : public MGraphics
    {
        public:
            explicit MSkiaGraphics(MWidget* w):MGraphics(w)
                { M_ASSERT(mApp->getGraphicsBackend() == MApplication::Skia); }
            explicit MSkiaGraphics(MGraphicsData* d):MGraphics(d)
                { M_ASSERT(mApp->getGraphicsBackend() == MApplication::Skia); }

            // Calling this function between MGraphicsData::beginDraw() and
            // MGraphicsData::endDraw() (i.e. when in MWidget::drawWindow())
            // will return the SkCanvas of the window. You can draw anything
            // on it.
            // In other time, this function returns 0.
            SkCanvas* getCanvas();
    };
}
#endif
