#include "MSkiaUtils.h"
#ifdef MB_USE_SKIA

#include "3rd/skia/core/SkAutoKern.h"
#include "3rd/skia/core/SkGlyphCache.h"
#include "3rd/skia/core/SkUtils.h"
#include "3rd/skia/core/SkCanvas.h"

namespace MetalBone
{
    static size_t linebreak(const wchar_t* text, const wchar_t* stop,
        const SkPaint& paint, int& width)
    {
        SkAutoGlyphCache    ac(paint, NULL);
        SkAutoKern          autokern;
        SkGlyphCache*       cache      = ac.getCache();
        bool                prevWS     = true;
        const wchar_t*      start      = text;
        const wchar_t*      word_start = text;
        int                 w          = 0;

        while(text < stop)
        {
            const wchar_t* prevText = text;
            SkUnichar      uni      = SkUTF16_NextUnichar((const uint16_t**)&text);
            bool           currWS   = iswspace(*text) != 0;
            const SkGlyph& glyph    = cache->getUnicharAdvance(uni);

            if(!currWS && prevWS) { word_start = prevText; }
            prevWS = currWS;

            w += SkFixedRound(autokern.adjust(glyph) + glyph.fAdvanceX);
            if(w > width)
            {
                if(currWS) // Eat the rest of the whitespace
                {
                    while(text < stop && iswspace(*text) != 0)
                        ++text;
                } else // backup until a whitespace (or 1 char)
                {
                    if(word_start != start) { text = word_start; } else
                    if(prevText   >  start) { text = prevText;   }
                }

                break;
            }
        }

        // If we only have one line, we return the line width.
        if(w < width) { width = w; } 

        return text - start;
    }

    size_t SkTextBoxW::countLines(const std::wstring& t, const SkPaint& paint, int* w_out) const
    {
        const wchar_t* text  = t.c_str();
        const wchar_t* stop  = text + t.size();
        int            count = 0;
        int            width = box.width();

        if(mode == kOneLine_Mode) width = INT_MAX;

        if(width > 0)
        {
            do {
                ++count;
                text += linebreak(text, stop, paint, width);
            } while (text < stop);
        }

        if(w_out != 0) { *w_out = count == 1 ? width : box.width(); }

        return count;
    }

    void SkTextBoxW::drawText(SkCanvas* canvas, const std::wstring& t, const SkPaint& paint, SkIRect* bound)
    {
        int width = box.width();
        if(width <= 0 || t.size() == 0) return;

        SkPaint::FontMetrics   metrics;
        const wchar_t*         text          = t.c_str();
        const wchar_t*         stop          = text + t.size();
        int                    height        = box.height();
        SkScalar               fontHeight    = paint.getFontMetrics(&metrics);
        SkScalar               scaledSpacing = fontHeight * mul + add;

        int                    ascent        = SkScalarRound(metrics.fAscent);
        int                    descent       = SkScalarRound(metrics.fDescent);
        int                    leading       = SkScalarRound(metrics.fLeading);
        int                    textHeight    = descent - ascent;
        int                    x;
        int                    y             = box.fTop - ascent;

        // If bound is not null, we need to get the width.
        size_t                 lines         = 1;
        int                    textWidth     = box.width();
        if(bound != 0) { lines = countLines(t, paint, &textWidth); }

        switch(paint.getTextAlign()) {
            case SkPaint::kLeft_Align:   x = box.fLeft;                break;
            case SkPaint::kCenter_Align: x = (width >> 1) + box.fLeft; break;
            default:                     x = box.fRight;
        }

        // Compute Y-Pos for the first line
        if(mode == kLineBreak_Mode && alignment != kStart_SpacingAlign)
        {
            // If bound is not null, we have already count the lines.
            if(bound == 0) lines = countLines(t, paint);
            textHeight += unsigned int(scaledSpacing * (lines - 1));
        }
        switch(alignment) {
            case kCenter_SpacingAlign: y += ((height - textHeight) >> 1); break;
            case kEnd_SpacingAlign:    y =  box.fBottom - textHeight - ascent;
        }

        while(true)
        {
            size_t len = linebreak(text, stop, paint, width);
            if(y > 0)
                canvas->drawText(text, len * sizeof(wchar_t), (SkScalar)x, (SkScalar)y, paint);

            text += len;
            if(text >= stop) { break; }

            y += (int)scaledSpacing;
            if(y + ascent >= height) break;
        }

        if(!bound) return;

        switch(paint.getTextAlign()) {
            case SkPaint::kRight_Align:
                bound->fLeft  = bound->fRight - textWidth;
                bound->fRight = bound->fRight;
                break;
            case SkPaint::kCenter_Align:
                bound->fLeft  = ((box.width() - textWidth) >> 1) + box.fLeft;
                bound->fRight = bound->fLeft + textWidth;
                break;
            default:
                bound->fLeft  = box.fLeft;
                bound->fRight = box.fLeft + textWidth;
        }
        switch(alignment) {
            case kEnd_SpacingAlign:
                bound->fTop    = box.fBottom - textHeight;
                bound->fBottom = box.fBottom;
                break;
            case kCenter_SpacingAlign:
                bound->fTop    = ((box.height() - textHeight) >> 1) + box.fTop;
                bound->fBottom = bound->fTop + textHeight;
                break;
            default:
                bound->fTop    = box.fTop;
                bound->fBottom = box.fTop + textHeight;
        }
        bound->fTop    -= ascent;
        bound->fBottom -= ascent;
    }

    void SkTextBoxW::getBoundingRect(const std::wstring& text, const SkPaint& paint, SkIRect* rect) const
    {
        if(rect == 0) return;

        int width = box.width();
        if(width <= 0 || text.size() == 0)
        {
            memset(rect, 0, sizeof(SkIRect));
            return;
        }

        SkPaint::FontMetrics metrics; 
        SkScalar     fontHeight = paint.getFontMetrics(&metrics);
        int          textHeight = int(metrics.fDescent - metrics.fAscent);
        int          textWidth  = 0;
        size_t       lines      = countLines(text, paint, &textWidth);

        if(mode == kLineBreak_Mode)
        {
            SkScalar scaledSpacing = fontHeight * mul + add;
            textHeight += unsigned int( scaledSpacing * (lines - 1) );
        }

        switch(paint.getTextAlign()) {
            case SkPaint::kRight_Align:
                rect->fLeft  = rect->fRight - textWidth;
                rect->fRight = rect->fRight;
                break;
            case SkPaint::kCenter_Align:
                rect->fLeft  = ((box.width() - textWidth) >> 1) + box.fLeft;
                rect->fRight = rect->fLeft + textWidth;
                break;
            default:
                rect->fLeft  = box.fLeft;
                rect->fRight = box.fLeft + textWidth;
        }

        switch(alignment) {
            case kEnd_SpacingAlign:
                rect->fTop    = box.fBottom - textHeight;
                rect->fBottom = box.fBottom;
                break;
            case kCenter_SpacingAlign:
                rect->fTop    = ((box.height() - textHeight) >> 1) + box.fTop;
                rect->fBottom = rect->fTop + textHeight;
                break;
            default:
                rect->fTop    = box.fTop;
                rect->fBottom = box.fTop + textHeight;
        }

        int total = int(metrics.fDescent - metrics.fAscent);
        rect->fTop    -= total;
        rect->fBottom -= total;
    }
}
#endif