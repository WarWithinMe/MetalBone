#include "MD2DPaintContext.h"
#include "MBGlobal.h"
#include "MResource.h"
#include "MApplication.h"
#include "MWidget.h"
#include "MD2DUtils.h"
#include "private/MWidget_p.h"
#include "private/MD2DPaintContextData.h"

namespace MetalBone
{
    std::list<ID2D1HwndRenderTarget*> MD2DPaintContextData::rtStack;

    MD2DPaintContext::MD2DPaintContext(MWidget* w):data(w->windowWidget()->m_windowExtras->m_pcData){}
    ID2D1RenderTarget* MD2DPaintContext::getRenderTarget() { return data->getRenderTarget(); }
    void MD2DPaintContext::discardResources()
    {
        MD2DBrushHandle::discardBrushes();
        MD2DImageHandle::discardImages();
    }

    ID2D1RenderTarget* MD2DPaintContext::getRecentRenderTarget()
    {
        M_ASSERT(MD2DPaintContextData::rtStack.size() != 0);
        return *MD2DPaintContextData::rtStack.rbegin();
    }

    void MD2DPaintContextData::beginDraw()
    {
        renderTarget->BeginDraw();
        rtStack.push_back(renderTarget);
    }

    bool MD2DPaintContextData::endDraw()
    {
        rtStack.pop_back();
        HRESULT result = renderTarget->EndDraw();
        if(result == S_OK)
            return true;

        if(result == D2DERR_RECREATE_TARGET)
        {
            if(widget) createRenderTarget();
            MD2DPaintContext::discardResources();
        }
        return false;
    }

    void MD2DPaintContextData::discardResources(bool recreateRenderTarget)
    { 
        if(recreateRenderTarget) createRenderTarget();
        MD2DPaintContext::discardResources();
    }

    void MD2DPaintContextData::createRenderTarget()
    {
        SafeRelease(renderTarget);

        D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
        p.usage  = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
        p.type   = mApp->isHardwareAccerated() ? 
            D2D1_RENDER_TARGET_TYPE_HARDWARE : D2D1_RENDER_TARGET_TYPE_SOFTWARE;
        p.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

        mApp->getD2D1Factory()->CreateHwndRenderTarget(p,
            D2D1::HwndRenderTargetProperties(widget->windowHandle(), widget->size()),
            &renderTarget);
    }

    void MD2DPaintContext::fill9PatchRect(MD2DBrushHandle& b, bool scaleX, bool scaleY,
        const MRect& rect, const MRect* clipRect)
    {
        if(b.type() != MD2DBrushHandle::NinePatch) return;

        ID2D1RenderTarget* rt        = getRenderTarget();
        ID2D1BitmapBrush*  brush     = b.getPortion(MD2DBrushHandle::Conner);
        MRectU             borders   = b.ninePatchBorders();
        MRectU             imageRect = b.ninePatchImageRect();
        D2D1_SIZE_U        imageSize = D2D1::SizeU(imageRect.width(), imageRect.height());
        D2D1_RECT_F        drawRect;

        if(imageSize.width == 0 && imageSize.height == 0)
        {
            ID2D1Bitmap* bitmap;
            brush->GetBitmap(&bitmap);
            imageSize = bitmap->GetPixelSize();
            bitmap->Release();
        }

        // 0.TopLeft     1.TopCenter     2.TopRight
        // 3.CenterLeft  4.Center        5.CenterRight
        // 6.BottomLeft  7.BottomCenter  8.BottomRight
        bool drawParts[9] = {true,true,true,true,true,true,true,true,true};
        int x1 = rect.left   + borders.left;
        int x2 = rect.right  - borders.right;
        int y1 = rect.top    + borders.top;
        int y2 = rect.bottom - borders.bottom;

        FLOAT scaleXFactor = FLOAT(x2 - x1) / (imageSize.width  - borders.left - borders.right);
        FLOAT scaleYFactor = FLOAT(y2 - y1) / (imageSize.height - borders.top  - borders.bottom);

        if(clipRect != 0)
        {
            rt->PushAxisAlignedClip(*clipRect, D2D1_ANTIALIAS_MODE_ALIASED);

            if(clipRect->left > x1) {
                drawParts[0] = drawParts[3] = drawParts[6] = false;
                if(clipRect->left > x2)    drawParts[1] = drawParts[4] = drawParts[7] = false;
            }
            if(clipRect->top > y1) {
                drawParts[0] = drawParts[1] = drawParts[2] = false;
                if(clipRect->top > y2)     drawParts[3] = drawParts[4] = drawParts[5] = false;
            }
            if(clipRect->right <= x2) {
                drawParts[2] = drawParts[5] = drawParts[8] = false;
                if(clipRect->right <= x1)  drawParts[1] = drawParts[4] = drawParts[7] = false;
            }
            if(clipRect->bottom <= y2) {
                drawParts[6] = drawParts[7] = drawParts[8] = false;
                if(clipRect->bottom <= y1) drawParts[3] = drawParts[4] = drawParts[5] = false;
            }
        }
        
        // Assume we can always get a valid brush for Conner.
        if(drawParts[0]) {
            drawRect.left   = (FLOAT)rect.left;
            drawRect.right  = (FLOAT)x1;
            drawRect.top    = (FLOAT)rect.top;
            drawRect.bottom = (FLOAT)y1;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(
                FLOAT(rect.left - (int)imageRect.left), FLOAT(rect.top - (int)imageRect.top)));
            rt->FillRectangle(drawRect,brush);
        }
        if(drawParts[2]) {
            drawRect.left   = (FLOAT)x2;
            drawRect.right  = (FLOAT)rect.right;
            drawRect.top    = (FLOAT)rect.top;
            drawRect.bottom = (FLOAT)y1;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(
                FLOAT(rect.right - (int)imageRect.right), FLOAT(rect.top - (int)imageRect.top)));
            rt->FillRectangle(drawRect,brush);
        }
        if(drawParts[6]) {
            drawRect.left   = (FLOAT)rect.left;
            drawRect.right  = (FLOAT)x1;
            drawRect.top    = (FLOAT)y2;
            drawRect.bottom = (FLOAT)rect.bottom;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(
                FLOAT(rect.left - (int)imageRect.left), FLOAT(rect.bottom - (int)imageRect.bottom)));
            rt->FillRectangle(drawRect,brush);
        }
        if(drawParts[8]) {
            drawRect.left   = (FLOAT)x2;
            drawRect.right  = (FLOAT)rect.right;
            drawRect.top    = (FLOAT)y2;
            drawRect.bottom = (FLOAT)rect.bottom;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(
                FLOAT(rect.right - (int)imageRect.right), FLOAT(rect.bottom - (int)imageRect.bottom)));
            rt->FillRectangle(drawRect,brush);
        }

        brush = b.getPortion(MD2DBrushHandle::VerCenter);
        if(brush)
        {
            drawRect.left   = (FLOAT)x1;
            drawRect.right  = (FLOAT)x2;

            D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, (FLOAT)rect.top));
            if(scaleX) matrix = D2D1::Matrix3x2F::Scale(scaleXFactor,1.0f) * matrix;

            if(drawParts[1]) {
                drawRect.top    = (FLOAT)rect.top;
                drawRect.bottom = (FLOAT)y1;

                brush->SetTransform(matrix);
                rt->FillRectangle(drawRect,brush);
            }

            if(drawParts[7]) {
                drawRect.top    = (FLOAT)y2;
                drawRect.bottom = (FLOAT)rect.bottom;

                matrix._32 = drawRect.top - (FLOAT)borders.top;
                brush->SetTransform(matrix);
                rt->FillRectangle(drawRect,brush);
            }
        }

        brush = b.getPortion(MD2DBrushHandle::HorCenter);
        if(brush)
        {
            drawRect.top    = (FLOAT)y1;
            drawRect.bottom = (FLOAT)y2;

            D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation((FLOAT)rect.left, drawRect.top));
            if(scaleY) matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleYFactor) * matrix;

            if(drawParts[3]) {
                drawRect.left   = (FLOAT)rect.left;
                drawRect.right  = (FLOAT)x1;
                brush->SetTransform(matrix);
                rt->FillRectangle(drawRect,brush);
            }

            if(drawParts[5]) {
                drawRect.left   = (FLOAT)x2;
                drawRect.right  = (FLOAT)rect.right;

                matrix._31 = drawRect.left - (FLOAT)borders.left;
                brush->SetTransform(matrix);
                rt->FillRectangle(drawRect,brush);
            }
        }

        if(drawParts[4])
        {
            brush = b.getPortion(MD2DBrushHandle::Center);
            if(brush)
            {
                drawRect.left   = (FLOAT)x1;
                drawRect.right  = (FLOAT)x2;
                drawRect.top    = (FLOAT)y1;
                drawRect.bottom = (FLOAT)y2;

                D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
                if(scaleX)
                    matrix = D2D1::Matrix3x2F::Scale(scaleXFactor, scaleY ? scaleYFactor : 1.0f) * matrix; 
                else if(scaleY)
                    matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleYFactor) * matrix;
                brush->SetTransform(matrix);
                rt->FillRectangle(drawRect,brush);
            }
        }

        if(clipRect != 0) rt->PopAxisAlignedClip();
    }
}