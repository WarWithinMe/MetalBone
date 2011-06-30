#pragma once
#include "MBGlobal.h"
#include <string>
namespace MetalBone
{
    class MRect;
    class MImageHandle;
    class MBrushHandle;
    namespace CSS { class RenderRuleData; enum FixGDIAlpha; }

    class METALBONE_EXPORT MGraphicsData
    {
        public:
            static MGraphicsData* create(HWND, long width, long height);

            virtual ~MGraphicsData(){}

            virtual void beginDraw ()                         = 0;
            virtual bool endDraw   (const MRect& updatedRect) = 0;

            virtual void clear     ()                         = 0;
            virtual void clear     (const MRect& clearRect)   = 0;

            virtual void pushClip  (const MRect& clipRect)    = 0;
            virtual void popClip   ()                         = 0;

            virtual HDC  getDC     ()                         = 0;
            virtual void releaseDC (const MRect& updatedRect) = 0;

            virtual void resize    (long width, long height)  = 0;

            // For RenderRule
            virtual void drawRenderRule(
                CSS::RenderRuleData* data,
                const MRect&         widgetRect,
                const MRect&         clipRect,
                const std::wstring&  text,
                CSS::FixGDIAlpha     fix,
                unsigned int         frameIndex) = 0;

            // Common drawing functions.
            virtual void drawImage(
                MImageHandle& image,
                const MRect&  drawRect) = 0;
            virtual void fill9PatchRect(
                MBrushHandle& brush,
                const MRect&  drawRect,
                bool          scaleX, 
                bool          scaleY,
                const MRect*  clipRect = 0) = 0;

            // Call this method to tell MGraphicsData if the window is a layered
            // window. So that when calling endDraw(), it will do something different.
            inline void setLayeredWindow(bool l) { layeredWindow = l;    }
            inline bool isLayeredWindow() const  { return layeredWindow; }

        protected:
            MGraphicsData():layeredWindow(false){}
            bool layeredWindow;

            // This function use getDC() to get the HDC of the window,
            // then use GDI API to draw the text for the RenderRule.
            // The purpose of this function is to make it possible to be hooked
            // by GDI++. GDI++ can produce nice effect when drawing small text.
            void drawGdiText(CSS::RenderRuleData*, const MRect& drawingRectInDC,
                const MRect& clipRectInDC, const std::wstring& text, CSS::FixGDIAlpha);
    };
};