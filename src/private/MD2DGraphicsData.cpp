#include "MD2DGraphicsData.h"

#ifdef MB_USE_D2D
#include "MApplication.h"
#include "MGraphicsResource.h"
#include "RenderRuleData.h"
#include "MStyleSheet.h"
#include "MResource.h"

#include <dwrite.h>

namespace MetalBone
{
    std::list<ID2D1RenderTarget*> MD2DGraphicsData::rtStack;

    void MD2DGraphicsData::pushClip(const MRect& r)
    { renderTarget->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_ALIASED); }
    void MD2DGraphicsData::popClip() { renderTarget->PopLayer(); }
    void MD2DGraphicsData::clear() { renderTarget->Clear(); }
    void MD2DGraphicsData::clear(const MRect& r)
    {
        renderTarget->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_ALIASED);
        renderTarget->Clear();
        renderTarget->PopAxisAlignedClip();
    }

    void MD2DGraphicsData::beginDraw()
    {
        renderTarget->BeginDraw();
        rtStack.push_back(renderTarget);
    }

    bool MD2DGraphicsData::endDraw(const MRect& updatedRect)
    {
        if(isLayeredWindow())
        {
            ID2D1GdiInteropRenderTarget* gdiRT;
            HDC dc;
            renderTarget->QueryInterface(&gdiRT);
            gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,&dc);

            BLENDFUNCTION blend      = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
            POINT         sourcePos  = { 0, 0 };
            SIZE          windowSize = { width, height };

            UPDATELAYEREDWINDOWINFO info = {};
            info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
            info.dwFlags  = ULW_ALPHA;
            info.hdcSrc   = dc;
            info.pblend   = &blend;
            info.psize    = &windowSize;
            info.pptSrc   = &sourcePos;
            info.prcDirty = &updatedRect;
            ::UpdateLayeredWindowIndirect(wndHandle,&info);

            gdiRT->ReleaseDC(0);
            gdiRT->Release();
        }

        std::list<ID2D1RenderTarget*>::reverse_iterator rit = rtStack.rbegin();
        std::list<ID2D1RenderTarget*>::reverse_iterator ritEnd = rtStack.rend();
        while(rit != ritEnd)
        {
            if(*rit == renderTarget)
            {
                rtStack.erase(--rit.base());
                break;
            }
            ++rit;
        }

        HRESULT result = renderTarget->EndDraw();
        if(result == S_OK) return true;

        if(result == D2DERR_RECREATE_TARGET)
        {
            createRenderTarget();
            MGraphicsResFactory::discardResources();
        }
        return false;
    }

    void MD2DGraphicsData::createRenderTarget()
    {
        SafeRelease(renderTarget);

        D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
        p.usage  = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
        p.type   = mApp->isHardwareAccerated() ? 
            D2D1_RENDER_TARGET_TYPE_HARDWARE : D2D1_RENDER_TARGET_TYPE_SOFTWARE;
        p.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

        mApp->getD2D1Factory()->CreateHwndRenderTarget(p,
            D2D1::HwndRenderTargetProperties(wndHandle, D2D1::SizeU(width, height)),
            &renderTarget);
    }

    void MD2DGraphicsData::drawImage(MImageHandle& image, const MRect& drawRect)
        { renderTarget->DrawBitmap((ID2D1Bitmap*)image.getImage(),drawRect); }

    void MD2DGraphicsData::fill9PatchRect(MBrushHandle& b, const MRect& rect,
        bool scaleX, bool scaleY, const MRect* clipRect)
    {
        M_ASSERT(b.type() == MBrushHandle::NinePatch);

        ID2D1RenderTarget* rt        = getRenderTarget();
        ID2D1BitmapBrush*  brush     = (ID2D1BitmapBrush*)b.getPortion(MBrushHandle::Conner);
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

        brush = (ID2D1BitmapBrush*)b.getPortion(MBrushHandle::VerCenter);
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

        brush = (ID2D1BitmapBrush*)b.getPortion(MBrushHandle::HorCenter);
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
            brush = (ID2D1BitmapBrush*)b.getPortion(MBrushHandle::Center);
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

    HDC MD2DGraphicsData::getDC()
    {
        HRESULT result = renderTarget->Flush();
        if(result != S_OK) return NULL;

        HDC dc;
        renderTarget->QueryInterface(&gdiRT);
        result = gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &dc);

        if(result == D2DERR_RENDER_TARGET_HAS_LAYER_OR_CLIPRECT)
        {
            renderTarget->PopAxisAlignedClip();
            gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY, &dc);
        }

        return dc;
    }

    void MD2DGraphicsData::releaseDC(const MRect& updatedRect)
    {
        gdiRT->ReleaseDC(&updatedRect);
        gdiRT->Release();
        gdiRT = 0;
    }

    using namespace CSS;
    template<class T>
    inline bool testValue(const T* t, ValueType value) {
        M_ASSERT(value < Value_BitFlagsEnd);
        return (t->values & value) != 0;
    }
    template<class T>
    inline bool testNoValue(const T* t, ValueType value) {
        M_ASSERT(value < Value_BitFlagsEnd);
        return (t->values & value) == 0;
    }
    void MD2DGraphicsData::drawRenderRule(RenderRuleData* rrdata,
        const MRect& widgetRect, const MRect& cr,
        const std::wstring& text, FixGDIAlpha fix,
        unsigned int frameIndex)
    {
        renderTarget->PushAxisAlignedClip(cr, D2D1_ANTIALIAS_MODE_ALIASED);

        D2D1_RECT_F borderRect = widgetRect;
        if(rrdata->hasMargin()) {
            borderRect.left   += (FLOAT)rrdata->margin->left;
            borderRect.right  -= (FLOAT)rrdata->margin->right;
            borderRect.top    += (FLOAT)rrdata->margin->top;
            borderRect.bottom -= (FLOAT)rrdata->margin->bottom;
        }

        // We have to get the border geometry first if there's any.
        // Because drawing the background may depends on the border geometry.
        BorderRenderObject* borderRO = rrdata->borderRO;
        ID2D1Geometry* borderGeo = 0;
        if(borderRO != 0 && borderRO->type == BorderRenderObject::ComplexBorder)
        {
            borderGeo = reinterpret_cast<ComplexBorderRenderObject*>(borderRO)
                ->createGeometry(borderRect);
        }

        // === Backgrounds ===
        for(size_t i = 0; i < rrdata->backgroundROs.size(); ++i)
        {
            MRect contentRect = widgetRect;
            BackgroundRenderObject* bgro  = rrdata->backgroundROs.at(i);
            MBrushHandle& bgBrush = bgro->brush;

            if(frameIndex >= bgro->frameCount)
                frameIndex = frameIndex % bgro->frameCount;

            if(testNoValue(bgro,Value_Margin))
            {
                if(rrdata->hasMargin()) {
                    contentRect.left   += rrdata->margin->left;
                    contentRect.top    += rrdata->margin->top;
                    contentRect.right  -= rrdata->margin->right;
                    contentRect.bottom -= rrdata->margin->bottom;
                }

                if(testNoValue(bgro, Value_Border)) {
                    if(borderRO != 0) {
                        MRect borderWidth;
                        borderRO->getBorderWidth(borderWidth);
                        contentRect.left   += borderWidth.left;
                        contentRect.top    += borderWidth.top;
                        contentRect.right  -= borderWidth.right;
                        contentRect.bottom -= borderWidth.bottom;
                    }
                    if(rrdata->hasPadding()) {
                        contentRect.left   += rrdata->padding->left;
                        contentRect.top    += rrdata->padding->top;
                        contentRect.right  -= rrdata->padding->right;
                        contentRect.bottom -= rrdata->padding->bottom;
                    }
                }
            }
            
            if(!bgro->brushChecked)
            {
                bgro->brushChecked = true;
                if(bgBrush.type() == MBrushHandle::Bitmap)
                {
                    MSize         size = bgBrush.bitmapSize();
                    unsigned int& w    = bgro->width;
                    unsigned int& h    = bgro->height;

                    // Make sure the size is valid.
                    if(w == 0 || w + bgro->x > (unsigned int)size.width())
                        w = size.width() - bgro->x;

                    if(h == 0 || h + bgro->y > (unsigned int)size.height()) {
                        h = size.height() - bgro->y;
                        h /= bgro->frameCount;
                    }
                }
            }

            int dx = 0 - bgro->x;
            int dy = 0 - bgro->y;
            if(bgro->width != 0)
            {
                if(testNoValue(bgro,Value_Left)) {
                    if(testValue(bgro,Value_HCenter))
                        contentRect.left = (contentRect.left + contentRect.right - bgro->width) / 2;
                    else
                        contentRect.left = contentRect.right - bgro->width;
                }
                if(testNoValue(bgro,Value_RepeatX)) {
                    int r = contentRect.left + bgro->width;
                    if(contentRect.right > r)
                        contentRect.right  = r;
                }
            }
            if(bgro->height != 0)
            {
                if(testNoValue(bgro, Value_Top)) {
                    if(testValue(bgro, Value_VCenter))
                        contentRect.top = (contentRect.top + contentRect.bottom - bgro->height) / 2;
                    else
                        contentRect.top = contentRect.bottom - bgro->height;
                }
                if(testNoValue(bgro, Value_RepeatY)) {
                    int b = contentRect.top + bgro->height;
                    if(contentRect.bottom > b)
                        contentRect.bottom = b;
                }
            }

            dx += (int)contentRect.left;
            dy += (int)contentRect.top;
            dy -= frameIndex * bgro->height;

            ID2D1Brush* brush = (ID2D1Brush*)bgBrush.getBrush();

            if(bgBrush.type() == MBrushHandle::LinearGradient)
                bgBrush.updateLinearGraident(contentRect);

            if(dx != 0 || dy != 0)
                brush->SetTransform(D2D1::Matrix3x2F::Translation((FLOAT)dx,(FLOAT)dy));

            if(testNoValue(bgro,Value_Border) || borderRO == 0)
                renderTarget->FillRectangle(contentRect,brush);
            else
            {
                if(borderRO->type == BorderRenderObject::SimpleBorder) {
                    renderTarget->FillRectangle(contentRect,brush);
                } else if(borderRO->type == BorderRenderObject::RadiusBorder) {
                    D2D1_ROUNDED_RECT rrect;
                    rrect.rect    = contentRect;
                    rrect.radiusX = FLOAT(reinterpret_cast<RadiusBorderRenderObject*>(borderRO)->radius);
                    rrect.radiusY = rrect.radiusX;
                    renderTarget->FillRoundedRectangle(rrect,brush);
                } else {
                    renderTarget->FillGeometry(borderGeo,brush);
                }
            }
        }

        // === BorderImage ===
        BorderImageRenderObject* borderImageRO = rrdata->borderImageRO;
        if(borderImageRO != 0)
        {
            fill9PatchRect(borderImageRO->brush,
                widgetRect,
                testNoValue(borderImageRO, Value_RepeatX),
                testNoValue(borderImageRO, Value_RepeatY),
                &cr);
        }

        // === Border ===
        if(borderRO != 0)
        {
            if(borderRO->type == BorderRenderObject::ComplexBorder)
            {
                // Check the ComplexBorderRO's brushes here, so that it don't depend on
                // RenderRuleData.
                ComplexBorderRenderObject* cbro = reinterpret_cast<ComplexBorderRenderObject*>(borderRO);
                if(borderRO->isVisible())
                    cbro->draw(renderTarget, borderGeo, borderRect);

                borderRect.left  += cbro->widths.left;
                borderRect.top   += cbro->widths.top;
                borderRect.right -= cbro->widths.right;
                borderRect.bottom-= cbro->widths.bottom;
            } else {
                SimpleBorderRenderObject* sbro = reinterpret_cast<SimpleBorderRenderObject*>(borderRO);
                FLOAT w = (FLOAT)sbro->width;
                if(borderRO->isVisible())
                {
                    // MS said that the rectangle stroke is centered on the outline. So we
                    // need to shrink a half of the borderWidth.
                    FLOAT shrink = w / 2;
                    borderRect.left   += shrink;
                    borderRect.top    += shrink;
                    borderRect.right  -= shrink;
                    borderRect.bottom -= shrink;
                    if(borderRO->type == BorderRenderObject::SimpleBorder)
                        renderTarget->DrawRectangle(borderRect, (ID2D1Brush*)sbro->brush.getBrush(), w);
                    else {
                        RadiusBorderRenderObject* rbro = reinterpret_cast<RadiusBorderRenderObject*>(borderRO);
                        D2D1_ROUNDED_RECT rr;
                        rr.rect = borderRect;
                        rr.radiusX = rr.radiusY = (FLOAT)rbro->radius;
                        renderTarget->DrawRoundedRectangle(rr, (ID2D1Brush*)sbro->brush.getBrush(), w);
                    }
                    borderRect.left   += shrink;
                    borderRect.top    += shrink;
                    borderRect.right  -= shrink;
                    borderRect.bottom -= shrink;
                } else
                {
                    borderRect.left   += w;
                    borderRect.right  -= w;
                    borderRect.top    += w;
                    borderRect.bottom -= w;
                }
            }
        }

        // === Text ===
        if(!text.empty())
        {
            if(borderRO != 0)
            {
                MRect borderWidth;
                borderRO->getBorderWidth(borderWidth);
                borderRect.left   += borderWidth.left;
                borderRect.top    += borderWidth.top;
                borderRect.right  -= borderWidth.right;
                borderRect.bottom -= borderWidth.bottom;
            }
            if(rrdata->hasPadding())
            {
                borderRect.left   += rrdata->padding->left;
                borderRect.top    += rrdata->padding->top;
                borderRect.right  -= rrdata->padding->right;
                borderRect.bottom -= rrdata->padding->bottom;
            }

            switch(RenderRule::getTextRenderer())
            {
                case GdiText:
                    {
                        MRect br((LONG)borderRect.left,(LONG)borderRect.top,
                            (LONG)borderRect.right,(LONG)borderRect.bottom);
                        renderTarget->PopAxisAlignedClip();
                        drawGdiText(rrdata, br, cr, text, fix);
                    }
                    break;
                case OtherSystem:
                    drawD2DText(rrdata, borderRect, text);
                    renderTarget->PopAxisAlignedClip();
                    break;
                case AutoDetermine:
                    unsigned int fontPtSize = rrdata->textRO == 0 ?
                        12 : rrdata->textRO->font.pointSize();

                    if(fontPtSize <= RenderRule::getMaxGdiFontPtSize())
                    {
                        MRect br((LONG)borderRect.left,(LONG)borderRect.top,
                            (LONG)borderRect.right,(LONG)borderRect.bottom);
                        renderTarget->PopAxisAlignedClip();
                        drawGdiText(rrdata, br, cr, text, fix);
                    } else {
                        drawD2DText(rrdata, borderRect, text);
                        renderTarget->PopAxisAlignedClip();
                    }
                    break;
            }
        } else
            renderTarget->PopAxisAlignedClip();

        SafeRelease(borderGeo);
    }

    class D2DOutlineTextRenderer : public IDWriteTextRenderer
    {
        public:
            D2DOutlineTextRenderer(ID2D1HwndRenderTarget* r, TextRenderObject* tro, ID2D1Brush* outlineBrush,
                ID2D1Brush* textBrush, ID2D1Brush* shadowBrush = 0):
            rt(r),tRO(tro),obrush(outlineBrush),tbrush(textBrush),sbrush(shadowBrush){}

            HRESULT __stdcall DrawGlyphRun(void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
                DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const* glyphRun,
                DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
                IUnknown* clientDrawingEffect);

            HRESULT __stdcall IsPixelSnappingDisabled(void*, BOOL* isDiabled) { *isDiabled = FALSE; return S_OK; }
            HRESULT __stdcall GetCurrentTransform(void*, DWRITE_MATRIX* matrix) { rt->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(matrix)); return S_OK; }
            HRESULT __stdcall GetPixelsPerDip(void*, FLOAT* ppd) { *ppd = 1.f; return S_OK; }

            HRESULT __stdcall DrawUnderline(void*,FLOAT,FLOAT,DWRITE_UNDERLINE const*,IUnknown*) { return E_NOTIMPL; }
            HRESULT __stdcall DrawStrikethrough(void*,FLOAT,FLOAT,DWRITE_STRIKETHROUGH const*,IUnknown*) { return E_NOTIMPL; }
            HRESULT __stdcall DrawInlineObject(void*,FLOAT,FLOAT,IDWriteInlineObject*,BOOL,BOOL,IUnknown*) { return E_NOTIMPL; }
            unsigned long __stdcall AddRef() { return 0; }
            unsigned long __stdcall Release(){ return 0; }
            HRESULT __stdcall QueryInterface(IID const&, void**) { return E_NOTIMPL; }

        private:
            ID2D1HwndRenderTarget* rt;
            ID2D1Brush* obrush;
            ID2D1Brush* tbrush;
            ID2D1Brush* sbrush;
            TextRenderObject* tRO;
    };

    HRESULT D2DOutlineTextRenderer::DrawGlyphRun(void*, FLOAT baselineOriginX, FLOAT baselineOriginY,
        DWRITE_MEASURING_MODE, DWRITE_GLYPH_RUN const* glyphRun, DWRITE_GLYPH_RUN_DESCRIPTION const*, IUnknown*)
    {
        ID2D1PathGeometry* pPathGeometry = NULL;
        ID2D1GeometrySink* pSink = NULL;
        mApp->getD2D1Factory()->CreatePathGeometry(&pPathGeometry); 
        pPathGeometry->Open(&pSink);
        glyphRun->fontFace->GetGlyphRunOutline(
            glyphRun->fontEmSize,
            glyphRun->glyphIndices,
            glyphRun->glyphAdvances,
            glyphRun->glyphOffsets,
            glyphRun->glyphCount,
            glyphRun->isSideways,
            glyphRun->bidiLevel%2,
            pSink);

        pSink->Close();

        // Initialize a matrix to translate the origin of the glyph run.
        D2D1::Matrix3x2F const matrix = D2D1::Matrix3x2F(
            1.0f, 0.0f, 0.0f, 1.0f,
            baselineOriginX, baselineOriginY);

        ID2D1TransformedGeometry* pTransformedGeometry = NULL;
        mApp->getD2D1Factory()->CreateTransformedGeometry(pPathGeometry, &matrix, &pTransformedGeometry);
        ID2D1StrokeStyle* pStrokeStyle;
        D2D1_STROKE_STYLE_PROPERTIES strokeStyle = D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_FLAT,D2D1_CAP_STYLE_FLAT,D2D1_CAP_STYLE_FLAT,D2D1_LINE_JOIN_ROUND);
        mApp->getD2D1Factory()->CreateStrokeStyle(&strokeStyle,0,0,&pStrokeStyle);
        if(sbrush != 0)
        {
            // Draw shadow.
            // Remark: If the shadow color is not opaque, we will see the outline of the shadow.
            // We can however render the opaque shadow to a bitmap then render the bitmap to the
            // rendertarget with opacity applied.
            ID2D1TransformedGeometry* shadowGeo = NULL;
            D2D1::Matrix3x2F const shadowMatrix = D2D1::Matrix3x2F(
                1.0f, 0.0f, 0.0f, 1.0f,
                baselineOriginX + tRO->shadowOffsetX, baselineOriginY + tRO->shadowOffsetY);
            mApp->getD2D1Factory()->CreateTransformedGeometry(pPathGeometry, &shadowMatrix, &shadowGeo);

            rt->DrawGeometry(shadowGeo,sbrush,tRO->outlineWidth,pStrokeStyle);
            rt->FillGeometry(shadowGeo,sbrush);

            SafeRelease(shadowGeo);
        }

        rt->DrawGeometry(pTransformedGeometry,obrush,tRO->outlineWidth,pStrokeStyle);
        rt->FillGeometry(pTransformedGeometry,tbrush);

        SafeRelease(pStrokeStyle);
        SafeRelease(pPathGeometry);
        SafeRelease(pSink);
        SafeRelease(pTransformedGeometry);
        return S_OK;
    }

    void MD2DGraphicsData::drawD2DText(RenderRuleData* rrdata, const D2D1_RECT_F& drawRect, const wstring& text)
    {
        MFont defaultFont;
        TextRenderObject defaultRO(defaultFont);
        TextRenderObject* tro = rrdata->textRO == 0 ? &defaultRO : rrdata->textRO;

        bool hasOutline = tro->outlineWidth != 0 && !tro->outlineColor.isTransparent();
        bool hasShadow  = !tro->shadowColor.isTransparent() &&
            ( tro->shadowBlur != 0 || (tro->shadowOffsetX != 0 || tro->shadowOffsetY != 0) );

        IDWriteTextFormat* textFormat = tro->createDWTextFormat();
        IDWriteTextLayout* textLayout = 0;
        mApp->getDWriteFactory()->CreateTextLayout(text.c_str(), text.size(), textFormat,
            drawRect.right - drawRect.left, drawRect.bottom - drawRect.top, &textLayout);

        if(hasOutline)
        {
            ID2D1Brush* sbrush = hasShadow ? (ID2D1Brush*)tro->shadowBrush.getBrush() : 0;
            D2DOutlineTextRenderer renderer(renderTarget, tro,
                (ID2D1Brush*)tro->outlineBrush.getBrush(),
                (ID2D1Brush*)tro->textBrush.getBrush(), sbrush);
            textLayout->Draw(0,&renderer,drawRect.left,drawRect.top);
        } else
        {
            if(hasShadow)
            {
                renderTarget->DrawTextLayout(
                    D2D1::Point2F(drawRect.left + tro->shadowOffsetX, drawRect.top + tro->shadowOffsetY),
                    textLayout, (ID2D1Brush*)tro->shadowBrush.getBrush(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            }
            renderTarget->DrawTextLayout(D2D1::Point2F(drawRect.left, drawRect.top),
                textLayout, (ID2D1Brush*)tro->textBrush.getBrush(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }

        SafeRelease(textFormat);
        SafeRelease(textLayout);
    }
}
#endif
