#pragma once
#include "MBGlobal.h"

#ifdef MB_USE_SKIA
#include "MGraphicsData.h"
#include "MGraphicsResource.h"

#include "3rd/skia/core/SkCanvas.h"
#include "3rd/skia/core/SkBitmap.h"

#include <vector>

namespace MetalBone
{
    // TODO: Consider using Skia-OpenGL as a backend.
    class MSkiaGraphicsData : public MGraphicsData
    {
        public:
            inline MSkiaGraphicsData(HWND hwnd, long w, long h);
            ~MSkiaGraphicsData();

            virtual void beginDraw();
            virtual bool endDraw(const MRect& updatedRect);

            virtual void clear();
            virtual void clear(const MRect&);

            virtual void pushClip(const MRect&);
            virtual void popClip();

            virtual HDC  getDC();
            virtual void releaseDC(const MRect& updatedRect);

            virtual void drawRenderRule(CSS::RenderRuleData*,
                const MRect& widgetRect, const MRect& clipRect,
                const std::wstring& text, CSS::FixGDIAlpha fix,
                unsigned int frameIndex);

            virtual void drawImage     (MImageHandle& i, const MRect& drawRect);
            virtual void fill9PatchRect(MBrushHandle& b, const MRect& drawRect,
                bool scaleX, bool scaleY, const MRect* clipRect);

            virtual void resize(long w,long h);

            inline SkCanvas* getCanvas() { return canvas; }
        private:
            HWND hwnd;
            long width;
            long height;

            HDC     hdc;
            HBITMAP oldBMP;
            HBITMAP currentBitmap;

            SkCanvas* canvas;
            SkBitmap  bitmap; // We do the drawing in a bitmap.

            std::vector<MRect> clipRects;

            void drawSkiaText(CSS::RenderRuleData*, const SkRect& borderRect,
                const std::wstring& text);
            // This function doesn't clip.
            void drawBitmapRectEx(const SkBitmap&, const SkIRect* srcRect,
                const SkRect& dstRect, bool scaleX, bool scaleY);
    };

    inline MSkiaGraphicsData::MSkiaGraphicsData(HWND hwnd, long w, long h)
        : hwnd(hwnd), width(0), height(0),
          hdc(NULL), oldBMP(NULL), currentBitmap(NULL),
          canvas(0)
    { resize(w, h); }
}
#endif