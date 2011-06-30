#include "MGraphics.h"
#include "private/MGraphicsData.h"
#include "MWidget.h"
#include "private/MWidget_p.h"
#include "private/RenderRuleData.h"
#include "MResource.h"

#include "private/MD2DGraphicsData.h"
#include "private/MGDIGraphicsData.h"
#include "private/MSkiaGraphicsData.h"

namespace MetalBone
{
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

    using namespace CSS;
    void MGraphicsData::drawGdiText(RenderRuleData* rrdata, const MRect& drawRect,
        const MRect& clipRectInRT, const std::wstring& text, FixGDIAlpha fixAlpha)
    {
        MFont defaultFont;
        TextRenderObject defaultRO(defaultFont);
        TextRenderObject* tro = rrdata->textRO == 0 ? &defaultRO : rrdata->textRO;

        bool hasOutline = tro->outlineWidth != 0 && !tro->outlineColor.isTransparent();
        bool hasShadow  = !tro->shadowColor.isTransparent() &&
            ( tro->shadowBlur != 0 || (tro->shadowOffsetX != 0 || tro->shadowOffsetY != 0) );

        // TODO: Add support for underline and its line style.
        HDC   textDC  = getDC();
        HRGN  clipRGN = ::CreateRectRgn(drawRect.left, drawRect.top, drawRect.right, drawRect.bottom);
        HRGN  oldRgn  = (HRGN) ::SelectObject(textDC, clipRGN);
        HFONT oldFont = (HFONT)::SelectObject(textDC, tro->font.getHandle());
        ::SetBkMode(textDC, TRANSPARENT);

        unsigned int formatParam = tro->getGDITextFormat();
        MRect textBoundingRect(0,0,drawRect.width(),drawRect.height());

        // We have to calc the bounding rect of the text, so that
        // we can do the fixing.
        if(fixAlpha != NoFix)
        {
            formatParam |= DT_CALCRECT;

            bool alignBottom = (formatParam & DT_BOTTOM) != 0;
            formatParam &= (~DT_BOTTOM);

            unsigned int drawRectW = textBoundingRect.right;
            unsigned int drawRectH = textBoundingRect.bottom;
            unsigned int boundingH = ::DrawTextW(textDC, text.c_str(), text.size(),
                &textBoundingRect, formatParam);

            if(textBoundingRect.right <= drawRectW) {
                if(formatParam & DT_CENTER) {
                    textBoundingRect.left  = (drawRectW - textBoundingRect.right) / 2 + drawRect.left;
                    textBoundingRect.right += textBoundingRect.left; 
                } else if(formatParam & DT_RIGHT) {
                    textBoundingRect.left  = drawRect.right - textBoundingRect.right;
                    textBoundingRect.right = drawRect.right;
                } else {
                    textBoundingRect.left  = drawRect.left;
                    textBoundingRect.right += textBoundingRect.left;
                }
            } else {
                textBoundingRect.left  = drawRect.left;
                textBoundingRect.right = drawRect.right;
            }
            if(boundingH <= drawRectH) {
                if(alignBottom) {
                    textBoundingRect.top    = drawRect.bottom - textBoundingRect.bottom;
                    textBoundingRect.bottom += textBoundingRect.top;
                    formatParam |= DT_BOTTOM;
                } else if(formatParam & DT_VCENTER) {
                    textBoundingRect.top    = drawRectH - boundingH + drawRect.top;
                    textBoundingRect.bottom += textBoundingRect.top;
                } else {
                    textBoundingRect.top    = drawRect.top;
                    textBoundingRect.bottom = drawRect.top + boundingH;
                }
            } else {
                textBoundingRect.top = drawRect.top;
                textBoundingRect.bottom = drawRect.bottom;
            }

            formatParam &= ~(DT_CALCRECT);
        }

        // Copy the source bits if we have to do SourceAlpha fix.
        HDC     originalAlphaDC   = NULL;
        void*   originalAlphaBits = 0;
        HBITMAP originalBMP       = NULL;
        HBITMAP originalOldBMP    = NULL;
        if(fixAlpha == SourceAlpha)
        {
            originalAlphaDC = ::CreateCompatibleDC(0);
            BITMAPINFO bmi = {};
            int width  = textBoundingRect.width();
            int height = textBoundingRect.height();
            bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth       = width;
            bmi.bmiHeader.biHeight      = height;
            bmi.bmiHeader.biCompression = BI_RGB;
            bmi.bmiHeader.biPlanes      = 1;
            bmi.bmiHeader.biBitCount    = 32;
            originalBMP = ::CreateDIBSection(originalAlphaDC, &bmi,
                DIB_RGB_COLORS, &originalAlphaBits, 0, 0);
            originalOldBMP = (HBITMAP)::SelectObject(originalAlphaDC, originalBMP);
            ::BitBlt(originalAlphaDC, 0, 0, width, height,
                textDC, textBoundingRect.left, textBoundingRect.top, SRCCOPY);
        }

        if(hasShadow)
        {
            if(tro->shadowBlur != 0 || tro->shadowColor.getAlpha() != 0xFF)
            {
                HDC tempDC = ::CreateCompatibleDC(0);
                void* pvBits;

                int width  = drawRect.width() + tro->shadowBlur * 2;
                int height = drawRect.height()+ tro->shadowBlur * 2;

                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth       = width;
                bmi.bmiHeader.biHeight      = height;
                bmi.bmiHeader.biCompression = BI_RGB;
                bmi.bmiHeader.biPlanes      = 1;
                bmi.bmiHeader.biBitCount    = 32;

                HBITMAP tempBMP = ::CreateDIBSection(tempDC, &bmi, DIB_RGB_COLORS, &pvBits, 0, 0);
                HBITMAP oldBMP  = (HBITMAP)::SelectObject(tempDC, tempBMP);
                HFONT   oldFont = (HFONT)  ::SelectObject(tempDC, tro->font);

                ::SetTextColor(tempDC, RGB(255,255,255));
                ::SetBkColor  (tempDC, RGB(0,0,0));
                ::SetBkMode   (tempDC, OPAQUE);

                MRect shadowRect(tro->shadowBlur, tro->shadowBlur, drawRect.width(), drawRect.height());
                ::DrawTextW(tempDC, (LPWSTR)text.c_str(), -1, &shadowRect, formatParam);

                unsigned int* pixel = (unsigned int*)pvBits;
                for(int i = width * height - 1; i >= 0; --i)
                {
                    if(pixel[i] != 0) {
                        if(pixel[i] == 0xFFFFFF)
                            pixel[i] = 0xFF000000 | tro->shadowColor.getRGB();
                        else
                        {
                            BYTE alpha = (pixel[i] >> 24) & 0xFF;
                            BYTE* component = (BYTE*)(pixel+i);
                            unsigned int color = tro->shadowColor.getARGB();
                            component[0] = ( color      & 0xFF) * alpha >> 8; 
                            component[1] = ((color >> 8)& 0xFF) * alpha >> 8;
                            component[2] = ((color >>16)& 0xFF) * alpha >> 8;
                            component[3] = alpha;
                        }
                    }
                }

                if(tro->shadowBlur == 0)
                {
                    BLENDFUNCTION bf = { AC_SRC_OVER, 0, tro->shadowColor.getAlpha(), AC_SRC_ALPHA };
                    int xCor = drawRect.left - tro->shadowBlur;
                    int yCor = drawRect.top  - tro->shadowBlur;
                    int tempXCor = xCor;
                    int tempYCor = yCor;
                    if(xCor < clipRectInRT.left) xCor = clipRectInRT.left;
                    if(yCor < clipRectInRT.top ) yCor = clipRectInRT.top;
                    int w = 0 - xCor + (clipRectInRT.right  < drawRect.right  ? clipRectInRT.right  : drawRect.right);
                    int h = 0 - yCor + (clipRectInRT.bottom < drawRect.bottom ? clipRectInRT.bottom : drawRect.bottom);
                    ::GdiAlphaBlend(textDC, xCor + tro->shadowOffsetX, yCor + tro->shadowOffsetY,
                        w, h, tempDC, xCor - tempXCor, yCor - tempYCor, w, h, bf);
                } else {
                    // TODO: Implement blur.
                }

                ::SelectObject(tempDC,oldBMP);
                ::SelectObject(tempDC,oldFont);
                ::DeleteObject(tempBMP);
                ::DeleteDC(tempDC);

            } else
            {
                MRect shadowRect = drawRect;
                shadowRect.offset(tro->shadowOffsetX,tro->shadowOffsetY);
                ::SetTextColor(textDC, tro->shadowColor.getCOLORREF());
                ::DrawTextW(textDC, (LPWSTR)text.c_str(), -1, &shadowRect, formatParam);
            }
        }

        ::SetTextColor(textDC, tro->color.getCOLORREF());
        ::DrawTextW(textDC, (LPWSTR)text.c_str(), -1, (MRect*)&drawRect, formatParam);

        if(fixAlpha != NoFix)
        {
            HDC tempDC = ::CreateCompatibleDC(0);
            void* pvBits;
            BITMAPINFO bmi = {};
            int width  = textBoundingRect.width();
            int height = textBoundingRect.height();
            bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth       = width;
            bmi.bmiHeader.biHeight      = height;
            bmi.bmiHeader.biCompression = BI_RGB;
            bmi.bmiHeader.biPlanes      = 1;
            bmi.bmiHeader.biBitCount    = 32;
            HBITMAP tempBMP = ::CreateDIBSection(tempDC, &bmi, DIB_RGB_COLORS, &pvBits, 0, 0);
            HBITMAP oldBMP  = (HBITMAP)::SelectObject(tempDC, tempBMP);
            ::BitBlt(tempDC, 0, 0, width, height, textDC,
                textBoundingRect.left, textBoundingRect.top, SRCCOPY);

            unsigned int* pixel = (unsigned int*)pvBits;

            if(fixAlpha == QuickOpaque)
            {
                for(int i = width * height - 1; i >= 0; --i)
                {
                    BYTE* component = (BYTE*)(pixel + i);
                    if(component[3] == 0) component[3] = 0xFF;
                }
            } else if(fixAlpha == SourceAlpha)
            {
                unsigned int* originalPixel = (unsigned int*)originalAlphaBits;
                for(int i = width * height - 1; i >=  0; --i)
                {
                    BYTE* component = (BYTE*)(pixel + i);
                    BYTE* orComponent = (BYTE*)(originalPixel + i);
                    component[3] = orComponent[3];
                }
                ::SelectObject(originalAlphaDC, originalOldBMP);
                ::DeleteObject(originalBMP);
                ::DeleteDC(originalAlphaDC);

            }

            ::SetDIBitsToDevice(textDC, textBoundingRect.left, textBoundingRect.top,
                width, height, 0, 0, 0, height, pvBits, &bmi, DIB_RGB_COLORS);
            ::SelectObject(tempDC, oldBMP);
            ::DeleteObject(tempBMP);
            ::DeleteDC(tempDC);
        }

        ::SelectObject(textDC,oldFont);
        ::SelectObject(textDC,oldRgn);
        ::DeleteObject(clipRGN);

        releaseDC(drawRect);
    }

#ifdef MB_USE_D2D
    ID2D1RenderTarget* MD2DGraphics::getRecentRenderTarget() { return MD2DGraphicsData::getRecentRenderTarget(); }
    ID2D1RenderTarget* MD2DGraphics::getRenderTarget()       { return ((MD2DGraphicsData*)data)->getRenderTarget(); }
#endif
#ifdef MB_USE_SKIA
    SkCanvas* MSkiaGraphics::getCanvas() { return ((MSkiaGraphicsData*)data)->getCanvas(); }
#endif

    MGraphicsData* MGraphicsData::create(HWND hwnd, long width, long height)
    {
        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                return new MD2DGraphicsData(hwnd,width,height);
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                return new MSkiaGraphicsData(hwnd,width,height);
#endif
        }
        return 0;
    }
}