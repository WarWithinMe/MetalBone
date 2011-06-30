#include "MSkiaGraphicsData.h"
#ifdef MB_USE_SKIA
#include <windows.h>
#include "RenderRuleData.h"
#include "MStyleSheet.h"
#include "3rd/skia/core/SkShader.h"

MSkiaGraphicsData::~MSkiaGraphicsData()
{
    if(hdc != NULL)
    {
        ::SelectObject(hdc, oldBMP);
        ::DeleteDC(hdc);
    }
    if(currentBitmap != NULL) ::DeleteObject(currentBitmap);
    delete canvas;
}

void MSkiaGraphicsData::resize(long w,long h)
{
    if(currentBitmap != NULL) ::DeleteObject(currentBitmap);

    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h; 
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = 0;

    void* pixel; 
    currentBitmap = ::CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pixel, NULL, 0);
    
    bitmap.setConfig(SkBitmap::kARGB_8888_Config,w,h);
    bitmap.setIsOpaque(false);
    bitmap.setPixels(pixel);

    width = w;
    height = h;
}

void MSkiaGraphicsData::clear()
{
    M_ASSERT(canvas != 0);
    canvas->clear(0);
}

void MSkiaGraphicsData::clear(const MRect& clearRect)
{
    M_ASSERT(canvas != 0);
    SkPaint paint;
    paint.setColor(0);
    paint.setXfermodeMode(SkXfermode::kSrc_Mode);
    pushClip(clearRect);
    canvas->drawPaint(paint);
    popClip();
}

HDC MSkiaGraphicsData::getDC() { return hdc; }
void MSkiaGraphicsData::releaseDC(const MRect&){}

void MSkiaGraphicsData::beginDraw()
{
    M_ASSERT(canvas == 0);
    M_ASSERT(hdc == NULL);

    canvas = new SkCanvas(bitmap);
    // We can't draw without clipping in skia.
    canvas->clipRect(SkRect::MakeWH((FLOAT)width, (FLOAT)height));

    hdc    = ::CreateCompatibleDC(NULL);
    oldBMP = (HBITMAP)::SelectObject(hdc, currentBitmap);
}

bool MSkiaGraphicsData::endDraw(const MRect& updatedRect)
{
    BITMAPINFO bmi;
    memset(&bmi, 0, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = bitmap.width();
    bmi.bmiHeader.biHeight      = -bitmap.height(); // top-down image 
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage   = 0;

    HDC wndDC = ::GetDC(hwnd);

    M_ASSERT(bitmap.width() * bitmap.bytesPerPixel() == bitmap.rowBytes());
    bitmap.lockPixels();
    BOOL res = ::SetDIBitsToDevice(wndDC, updatedRect.left, updatedRect.top, // Flip
        updatedRect.width(), updatedRect.height(),
        updatedRect.left, height - updatedRect.bottom, 0, bitmap.height(), bitmap.getPixels(), 
        &bmi, DIB_RGB_COLORS);
    bitmap.unlockPixels();

    if(isLayeredWindow())
    {
        BLENDFUNCTION blend = {AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        POINT sourcePos = {0, 0};
        SIZE windowSize = {width, height};

        UPDATELAYEREDWINDOWINFO info = {};
        info.cbSize   = sizeof(UPDATELAYEREDWINDOWINFO);
        info.dwFlags  = ULW_ALPHA;
        info.hdcSrc   = hdc;
        info.pblend   = &blend;
        info.psize    = &windowSize;
        info.pptSrc   = &sourcePos;
        info.prcDirty = &updatedRect;
        ::UpdateLayeredWindowIndirect(hwnd, &info);
    } 

    ::ReleaseDC(hwnd, wndDC);
    ::SelectObject(hdc, oldBMP);
    ::DeleteDC(hdc);
    oldBMP = NULL;
    hdc    = NULL;

    delete canvas;
    canvas = 0;
    return true;
}

void MSkiaGraphicsData::pushClip(const MRect& clipRect)
{
    M_ASSERT(canvas != 0);
    canvas->clipRect(clipRect, SkRegion::kReplace_Op);
    clipRects.push_back(clipRect);
}

void MSkiaGraphicsData::popClip()
{
    M_ASSERT(canvas != 0);
    clipRects.pop_back();

    if(clipRects.size() == 0)
        canvas->clipRect(SkRect::MakeWH((SkScalar)width, (SkScalar)height));
    else
        canvas->clipRect(clipRects.at(clipRects.size() - 1), SkRegion::kReplace_Op);
}

void MSkiaGraphicsData::drawImage(MImageHandle& i, const MRect& drawRect)
{
    M_ASSERT(canvas != 0);

    SkBitmap* image = (SkBitmap*)i.getImage();
    canvas->drawBitmapRect(*image, 0, drawRect);
}

static void drawBitmapRectEx(const SkBitmap&, const SkIRect& srcRect,
    const SkRect& dstRect, bool scaleX, bool scaleY);

void MSkiaGraphicsData::fill9PatchRect(MBrushHandle& b, const MRect& drawRect,
    bool scaleX, bool scaleY, const MRect* clipRect)
{
    M_ASSERT(b.type() == MBrushHandle::NinePatch);
    M_ASSERT(canvas != 0);

    SkBitmap* ninePatchImage = (SkBitmap*) b.getPortion(MBrushHandle::Conner);
    MRectU    borders   = b.ninePatchBorders();
    MRectU    imageRect = b.ninePatchImageRect();
    if(imageRect.right == 0 && imageRect.bottom == 0)
    {
        imageRect.right = ninePatchImage->width();
        imageRect.bottom = ninePatchImage->height();
    }

    // 0.TopLeft     1.TopCenter     2.TopRight
    // 3.CenterLeft  4.Center        5.CenterRight
    // 6.BottomLeft  7.BottomCenter  8.BottomRight
    bool drawParts[9] = {true,true,true,true,true,true,true,true,true};

    SkScalar dx1 = SkScalar(drawRect.left   + borders.left  );
    SkScalar dx2 = SkScalar(drawRect.right  - borders.right );
    SkScalar dy1 = SkScalar(drawRect.top    + borders.top   );
    SkScalar dy2 = SkScalar(drawRect.bottom - borders.bottom);

    int sx1 = imageRect.left  + borders.left  ;
    int sx2 = imageRect.right - borders.right ;
    int sy1 = imageRect.top   + borders.top   ;
    int sy2 = imageRect.bottom- borders.bottom;

    if(clipRect != 0)
    {
        pushClip(*clipRect);

        if(clipRect->left > dx1) {
            drawParts[0] = drawParts[3] = drawParts[6] = false;
            if(clipRect->left > dx2)    drawParts[1] = drawParts[4] = drawParts[7] = false;
        } else if(borders.left == 0) {
            drawParts[0] = drawParts[3] = drawParts[6] = false;
        }

        if(clipRect->top > dy1) {
            drawParts[0] = drawParts[1] = drawParts[2] = false;
            if(clipRect->top > dy2)     drawParts[3] = drawParts[4] = drawParts[5] = false;
        } else if(borders.top == 0) {
            drawParts[0] = drawParts[1] = drawParts[2] = false;
        }

        if(clipRect->right <= dx2) {
            drawParts[2] = drawParts[5] = drawParts[8] = false;
            if(clipRect->right <= dx1)  drawParts[1] = drawParts[4] = drawParts[7] = false;
        } else if(borders.right == 0) {
            drawParts[2] = drawParts[5] = drawParts[8] = false;
        }

        if(clipRect->bottom <= dy2) {
            drawParts[6] = drawParts[7] = drawParts[8] = false;
            if(clipRect->bottom <= dy1) drawParts[3] = drawParts[4] = drawParts[5] = false;
        } else if(borders.bottom == 0) {
            drawParts[6] = drawParts[7] = drawParts[8] = false;
        }
        
        if(sx1 == sx2) { drawParts[1] = drawParts[4] = drawParts[7] = false; }
        if(sy1 == sy2) { drawParts[3] = drawParts[4] = drawParts[5] = false; }
    }

    SkIRect srcRect;
    SkRect  dstRect;
    SkPaint paint;

    if(drawParts[0]) {
        srcRect.setLTRB(imageRect.left, imageRect.top, sx1, sy1);
        dstRect.setLTRB((SkScalar)drawRect.left,  (SkScalar)drawRect.top,  dx1, dy1);
        canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
    }
    if(drawParts[2]) {
        srcRect.setLTRB(sx2, imageRect.top, imageRect.right, sy1);
        dstRect.setLTRB(dx2, (SkScalar)drawRect.top, (SkScalar)drawRect.right,  dy1);
        canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
    }
    if(drawParts[6]) {
        srcRect.setLTRB(imageRect.left, sy2, sx1, imageRect.bottom);
        dstRect.setLTRB((SkScalar)drawRect.left,  dy2, dx1, (SkScalar)drawRect.bottom);
        canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
    }
    if(drawParts[8]) {
        srcRect.setLTRB(sx2, sy2, imageRect.right, imageRect.bottom);
        dstRect.setLTRB(dx2, dy2, (SkScalar)drawRect.right,  (SkScalar)drawRect.bottom);
        canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
    }

    if(scaleX) {
        if(drawParts[1]) {
            srcRect.setLTRB(sx1, imageRect.top, sx2, sy1);
            dstRect.setLTRB(dx1, (SkScalar)drawRect.top,  dx2, dy1);
            canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
        }
        if(drawParts[7]) {
            srcRect.setLTRB(sx1, sy2, sx2, imageRect.bottom);
            dstRect.setLTRB(dx1, dy2, dx2, (SkScalar)drawRect.bottom);
            canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
        }
    } else {
        if(drawParts[1]) {
            srcRect.setLTRB(sx1, imageRect.top, sx2, sy1);
            dstRect.setLTRB(dx1, (SkScalar)drawRect.top,  dx2, dy1);
            canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
            drawBitmapRectEx(*ninePatchImage, &srcRect, dstRect, false, false);
        }
        if(drawParts[7]) {
            srcRect.setLTRB(sx1, sy2, sx2, imageRect.bottom);
            dstRect.setLTRB(dx1, dy2, dx2, (SkScalar)drawRect.bottom);
            drawBitmapRectEx(*ninePatchImage, &srcRect, dstRect, false, false);
        }
    }

    if(scaleY) {
        if(drawParts[3]) {
            srcRect.setLTRB(imageRect.left, sy1, sx1, sy2);
            dstRect.setLTRB((SkScalar)drawRect.left,  dy1, dx1, dy2);
            canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
        }
        if(drawParts[5]) {
            srcRect.setLTRB(sx2, sy1, imageRect.right, sy2);
            dstRect.setLTRB(dx2, dy1, (SkScalar)drawRect.right,  dy2);
            canvas->drawBitmapRect(*ninePatchImage, &srcRect, dstRect);
        }

    } else {
        if(drawParts[3]) {
            srcRect.setLTRB(imageRect.left, sy1, sx1, sy2);
            dstRect.setLTRB((SkScalar)drawRect.left,  dy1, dx1, dy2);
            drawBitmapRectEx(*ninePatchImage, &srcRect, dstRect, false, false);
        }
        if(drawParts[5]) {
            srcRect.setLTRB(sx2, sy1, imageRect.right, sy2);
            dstRect.setLTRB(dx2, dy1, (SkScalar)drawRect.right,  dy2);
            drawBitmapRectEx(*ninePatchImage, &srcRect, dstRect, false, false);
        }
    }

    if(drawParts[4]) {
        srcRect.setLTRB(sx1, sy1, sx2, sy2);
        dstRect.setLTRB(dx1, dy1, dx2, dy2);
        drawBitmapRectEx(*ninePatchImage, &srcRect, dstRect, scaleX, scaleY);
    }

    popClip();
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

void MSkiaGraphicsData::drawRenderRule(RenderRuleData* rrdata,
    const MRect& widgetRect,  const MRect& clipRect,
    const std::wstring& text, FixGDIAlpha  fix,
    unsigned int frameIndex)
{
    pushClip(clipRect);

    MRect borderRect = widgetRect;
    if(rrdata->hasMargin()) {
        borderRect.left   += rrdata->margin->left;
        borderRect.right  -= rrdata->margin->right;
        borderRect.top    += rrdata->margin->top;
        borderRect.bottom -= rrdata->margin->bottom;
    }

    // We have to get the border geometry first if there's any.
    // Because drawing the background may depends on the border geometry.
    BorderRenderObject* borderRO = rrdata->borderRO;
    SkPath borderPath;
    if(borderRO != 0)
    {
        if(borderRO->type == BorderRenderObject::RadiusBorder)
        {
            SkScalar radius = SkScalar(reinterpret_cast<RadiusBorderRenderObject*>(borderRO)->radius);
            borderPath.addRoundRect(borderRect, radius, radius);
            canvas->clipPath(borderPath);
        } else if(borderRO->type == BorderRenderObject::ComplexBorder)
        {
            ComplexBorderRenderObject* cbro = reinterpret_cast<ComplexBorderRenderObject*>(borderRO);
            SkScalar radii[8];
            for(int i = 0; i < 8; i+=2)
                radii[i+1] = radii[i] = SkScalar(cbro->radiuses[i >> 1]);
            borderPath.addRoundRect(borderRect,radii);
            canvas->clipPath(borderPath);
        }
    }

    // === Backgrounds ===
    for(size_t i = 0; i < rrdata->backgroundROs.size(); ++i)
    {
        BackgroundRenderObject* bgro = rrdata->backgroundROs.at(i);
        MBrushHandle& bgBrush = bgro->brush;
        if(!bgro->brushChecked)
        {
            bgro->brushChecked = true;
            if(bgBrush.type() == MBrushHandle::Bitmap)
            {
                MSize size = bgBrush.bitmapSize();
                bgro->frameCount = bgBrush.frameCount();

                unsigned int& w = bgro->width;
                unsigned int& h = bgro->height;
                // Make sure the size is valid.
                if(w == 0 || w + bgro->x > (unsigned int)size.width())
                    w = size.width() - bgro->x;
                if(h == 0 || h + bgro->y > (unsigned int)size.height())
                    h = size.height() - bgro->y;
                h /= bgro->frameCount;

                if(bgro->frameCount != 1)
                {
                    if(rrdata->totalFrameCount % bgro->frameCount != 0)
                        rrdata->totalFrameCount *= bgro->frameCount;
                    bgro->values &= (~Value_Repeat);
                }
            } else {
                bgro->width  = 0;
                bgro->height = 0;
            }
        }

        MRect contentRect = widgetRect;
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

        bool repeatX = testValue(bgro, Value_RepeatX);
        bool repeatY = testValue(bgro, Value_RepeatY);
        if(bgro->width != 0) {
            if(testNoValue(bgro,Value_Left))
            {
                contentRect.left = testValue(bgro,Value_HCenter) ? 
                    (contentRect.left + contentRect.right - bgro->width) / 2 :
                    contentRect.right - bgro->width;
            }
            if(!repeatX) {
                int r = contentRect.left + bgro->width;
                if(contentRect.right > r) contentRect.right  = r;
            }
        }
        if(bgro->height != 0) {
            if(testNoValue(bgro, Value_Top))
            {
                contentRect.top = testValue(bgro, Value_VCenter) ?
                    (contentRect.top + contentRect.bottom - bgro->height) / 2 :
                    contentRect.bottom - bgro->height;
            }
            if(!repeatY) {
                int b = contentRect.top + bgro->height;
                if(contentRect.bottom > b) contentRect.bottom = b;
            }
        }

        if(frameIndex >= bgro->frameCount)
            frameIndex = frameIndex % bgro->frameCount;

        SkPaint paint;
        paint.setStyle(SkPaint::kFill_Style);
        paint.setAntiAlias(true);
        switch(bgBrush.type())
        {
            case MBrushHandle::Solid:
                paint.setColor(((MColor*)bgBrush.getBrush())->getARGB());
                break;
            case MBrushHandle::LinearGradient:
                bgBrush.updateLinearGraident(contentRect);
                paint.setShader((SkShader*)bgBrush.getBrush());
                break;
            case MBrushHandle::Bitmap:
                {
                    SkBitmap* image = (SkBitmap*)bgBrush.getBrush();
                    SkIRect src;
                    src.setXYWH(bgro->x, bgro->y, bgro->width, bgro->height);

                    if(contentRect.width() > src.width() ||
                       contentRect.height()> src.height())
                        drawBitmapRectEx(*image, &src, contentRect, !repeatX, !repeatY);
                    else
                        canvas->drawBitmapRect(*image, &src, contentRect);

                    continue; // Skip the following draw call.
                }
        }
        canvas->drawRect(contentRect, paint);
    }

    // Clear the clip path if there's any.
    canvas->clipRect(clipRect, SkRegion::kReplace_Op);

    // === BorderImage ===
    BorderImageRenderObject* borderImageRO = rrdata->borderImageRO;
    if(borderImageRO != 0)
    {
        fill9PatchRect(borderImageRO->brush,
            widgetRect,
            testNoValue(borderImageRO, Value_RepeatX),
            testNoValue(borderImageRO, Value_RepeatY),
            &clipRect);
    }

    // === Text ===
    if(!text.empty())
    {
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
                drawGdiText(rrdata, borderRect, clipRect, text, fix);
                break;
            case OtherSystem:
                drawSkiaText(rrdata, borderRect, text);
                break;
            case AutoDetermine:
                unsigned int fontPtSize = rrdata->textRO == 0 ?
                    12 : rrdata->textRO->font.pointSize();
                if(fontPtSize <= RenderRule::getMaxGdiFontPtSize())
                {
                    drawGdiText(rrdata, borderRect, clipRect, text, fix);
                } else {
                    drawSkiaText(rrdata, borderRect, text);
                }
                break;
        }
    }

    // === Border ===
    if(borderRO != 0)
    {
        SkPaint paint;
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setAntiAlias(true);
        MBrushHandle borderBrush;
        switch(borderRO->type)
        {
            case BorderRenderObject::ComplexBorder:
                {
                    ComplexBorderRenderObject* cbro = reinterpret_cast<ComplexBorderRenderObject*>(borderRO);
                    borderBrush = cbro->brushes[0];
                    paint.setStrokeWidth((SkScalar)(cbro->widths.left));
                    break;
                }
            default:
                {
                    SimpleBorderRenderObject* sbro = reinterpret_cast<SimpleBorderRenderObject*>(borderRO);
                    borderBrush = sbro->brush;
                    paint.setStrokeWidth((SkScalar)(sbro->width));
                }
        }

        switch(borderBrush.type())
        {
            case MBrushHandle::Solid:
                paint.setColor(((MColor*)borderBrush.getBrush())->getARGB());
                break;
            case MBrushHandle::LinearGradient:
                {
                    borderBrush.updateLinearGraident(borderRect);
                    paint.setShader((SkShader*)borderBrush.getBrush());
                }
                break;
        }

        if(borderRO->type == BorderRenderObject::SimpleBorder)
            canvas->drawRect(borderRect,paint);
        else
            canvas->drawPath(borderPath,paint);
    }
    popClip();
}

void MSkiaGraphicsData::drawBitmapRectEx(const SkBitmap& image, const SkIRect* srcRect,
    const SkRect& dstRect, bool scaleX, bool scaleY)
{
    if(scaleX && scaleY) { canvas->drawBitmapRect(image, srcRect, dstRect); return; }

    int tempWidth  = srcRect ? srcRect->width()  : image.width();
    int tempHeight = srcRect ? srcRect->height() : image.height();
    if(scaleX) { tempWidth  = (int)dstRect.width();  } else
    if(scaleY) { tempHeight = (int)dstRect.height(); }
    
    SkBitmap tempBMP;
    tempBMP.setConfig(SkBitmap::kARGB_8888_Config, tempWidth, tempHeight);
    tempBMP.setIsOpaque(false);
    tempBMP.allocPixels();

    // Draw to temp bitmap
    SkRect   dr = SkRect::MakeWH((SkScalar)tempWidth, (SkScalar)tempHeight);
    SkCanvas tempCanvas;
    tempCanvas.setBitmapDevice(tempBMP);
    tempCanvas.clear(0);
    tempCanvas.clipRect(dr, SkRegion::kReplace_Op);
    tempCanvas.drawBitmapRect(image, srcRect, dr);

    // Shader & Paint
    SkShader* bmpShader = SkShader::CreateBitmapShader(tempBMP,
        SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode);
    SkMatrix m;
    m.setTranslate(dstRect.left(), dstRect.top());
    bmpShader->setLocalMatrix(m);
    SkPaint paint;
    paint.setShader(bmpShader)->unref();

    // Draw
    canvas->drawRect(dstRect, paint);
}

void MSkiaGraphicsData::drawSkiaText(CSS::RenderRuleData* rrdata, const SkRect& borderRect,
    const std::wstring& text)
{
    SkPaint paint;
    paint.setAntiAlias(true);
    // TODO : It seems Skia doesn't work with wchar_t...
}

#endif