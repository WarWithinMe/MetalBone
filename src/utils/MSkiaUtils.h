#pragma once

#include "MBGlobal.h"
#ifdef MB_USE_SKIA
#include "3rd/skia/core/SkRect.h"
#include <string>

class SkCanvas;
class SkPaint;
namespace MetalBone
{
    // This class clones a subset of SkTextBox. So that we can deal with UNICODE.
    class SkTextBoxW
    {
        public:
            SkTextBoxW() : mode((char)kOneLine_Mode), alignment((char)kStart_SpacingAlign),
                mul(1), add(0) { box.setEmpty(); }

            enum Mode { kOneLine_Mode, kLineBreak_Mode };
            enum SpacingAlign {
                kStart_SpacingAlign,
                kCenter_SpacingAlign,
                kEnd_SpacingAlign
            };

            inline Mode         getMode()          const { return (Mode)mode; }
            inline void         setMode(Mode m)          { mode = m; }
            inline SpacingAlign getSpacingAlign()  const { return (SpacingAlign)alignment; }
            inline void         setSpacingAlign(SpacingAlign s) { alignment = s; }

            inline SkIRect      getBox()           const { return box; }
            inline void         setBox(const SkIRect& b) { box = b; }

            inline void         setSpacing(char m, char a) { mul = m; add = a; }

            // If boundingRect is not null, it will return the bounding rectangle
            // of the drawn text.
            void drawText(SkCanvas*, const std::wstring&, const SkPaint&, SkIRect* boundingRect = 0);

            size_t countLines(const std::wstring&, const SkPaint&, int* width = 0) const;
            // One should call setBox() to set the drawing rectangle of the text first.
            // Then call getBoundingRect() to get the bounding rectangle of the text.
            void getBoundingRect(const std::wstring&, const SkPaint&, SkIRect*) const;


        private:
            SkIRect box;
            char mode, alignment;
            char mul, add;
    };
}

#endif