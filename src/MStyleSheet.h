#pragma once

#include "MBGlobal.h"
#include "MUtils.h"
#include "MGraphics.h"
#include "private/MGraphicsData.h"

#include <windows.h>
#include <set>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <limits>

namespace MetalBone
{
    class MSSSPrivate;
    class MStyleSheetStyle;
    class MGraphics;
    class MWidget;
    class MCursor;
    namespace CSS
    {
        enum PseudoClassType {
            PC_Default    = 0,          PC_Enabled      = 0x2,
            PC_Disabled   = 0x4,        PC_Pressed      = 0x8,
            PC_Focus      = 0x10,       PC_Hover        = 0x20,
            PC_Checked    = 0x40,       PC_Unchecked    = 0x80,
            PC_Selected   = 0x100,      PC_Horizontal   = 0x200,
            PC_Vertical   = 0x400,      PC_Children     = 0x800,
            PC_ReadOnly   = 0x1000,     PC_Sibling      = 0x2000,
            PC_First      = 0x4000,     PC_Last         = 0x8000,
            PC_Active     = 0x10000,    PC_Editable     = 0x20000,
            PC_EditFocus  = 0x40000,    PC_Unknown      = 0x80000,
            knownPseudoCount = 20
        };

        enum TextRenderer
        {
            Gdi,
            Direct2D,
            AutoDetermine
        };

        // Specify how to fix the alpha after drawing GDI text.
        // If the window is not layered window, there's no fix
        // at all, no matter which enum value is chosen.
        enum FixGDIAlpha
        {
            NoFix,
            QuickOpaque, // All pixels which has 0 alpha will be of 255 alpha, if the background
                         // of the text is opaque, choose QuickOpaque is the best.
            SourceAlpha, // All pixels touched by GDI, their alpha will be the alpha before touched by GDI.
            GrayScale    // We draw white text on a black DC, then we can get the grayscale alpha.
                         // This may result in the text to be bolder/darker.
        };

        class RenderRuleData;
        class METALBONE_EXPORT RenderRule
        {
            // RenderRule is not thread-safe and it haven't to be.
            // Because all Styling/Drawing stuff should remain on the same thread(Main thread)
            public:
                inline RenderRule();
                RenderRule(RenderRuleData* d);
                RenderRule(const RenderRule& d);
                ~RenderRule();

                enum Geometry
                {
                    RRG_X,          RRG_Y,
                    RRG_Width,      RRG_Height,
                    RRG_MinWidth,   RRG_MinHeight,
                    RRG_MaxWidth,   RRG_MaxHeight
                };

                // If x or y not exist, return INT_MAX.
                // If others not exist, return -1.
                int      getGeometry(Geometry);
                MSize    getStringSize(const std::wstring&, int maxWidth = INT_MAX);
                MRect    getContentMargin();
                MCursor* getCursor();

                bool opaqueBackground() const;

                inline void draw(MGraphics&   graphcis,
                          const MRect&        drawRect,
                          const MRect&        clipRect,
                          const std::wstring& text = std::wstring(),
                          FixGDIAlpha         fixAlpha = NoFix,
                          unsigned int        frameIndex = 0);

                inline bool       isValid() const;
                inline bool       operator==(const RenderRule&) const;
                inline bool       operator!=(const RenderRule&) const;
                const RenderRule& operator=(const RenderRule& other);
                inline operator bool() const;

                // If TextRenderer is AutoDetermine, we use GDI to render text when the font size is
                // no bigger than maxGdiFontPtSize.
                static inline void setTextRenderer(TextRenderer, unsigned int maxGdiFontPtSize = 16);
                static inline TextRenderer getTextRenderer();
                static inline unsigned int getMaxGdiFontPtSize();

            private:
                void init();
                inline RenderRuleData* operator->();
                RenderRuleData* data;

                static unsigned int maxGdiFontPtSize;
                static TextRenderer textRenderer;

            friend class MSSSPrivate;
            friend class MStyleSheetStyle;
        };

        class METALBONE_EXPORT RenderRuleQuerier
        {
            public:
                // You should keep the RenderRuleQuerier instead of creating
                // it each time when you query a RenderRule.
                // Because MStyleSheetStyle internally cache the RenderRule
                // for every RenderRuleQuerier.

                // Construct a RenderRuleQuerier that has a className as its CLASS and objectName as its ID.
                inline RenderRuleQuerier(const std::wstring& className,
                    const std::wstring& objectName = std::wstring());
                ~RenderRuleQuerier();

                // Return a new RenderRuleQuerier that has a className as its CLASS and objectName as its ID. 
                // The new RenderRuleQuerier is a child of this RenderRuleQuerier.
                // When the parent is deleted, all its children will be deleted.
                inline RenderRuleQuerier* addChild(std::wstring& className,
                    std::wstring& objectName = std::wstring());

                inline RenderRuleQuerier* parent();
                inline const std::wstring& objectName() const;
                inline const std::wstring& className()  const;


            private:
                RenderRuleQuerier* mparent;
                std::wstring mclassName;
                std::wstring mobjectName;
                std::vector<RenderRuleQuerier*> children;

                RenderRuleQuerier(const RenderRuleQuerier&);
                const RenderRuleQuerier& operator=(const RenderRuleQuerier&);
        };
    } // namespace CSS

    class METALBONE_EXPORT MStyleSheetStyle
    {
        public:
            void setAppSS(const std::wstring& css);
            void setWidgetSS(MWidget* w, const std::wstring& css);
            void polish(MWidget* w);

            // FrameIndex == -1 means let MStyleSheetStyle to choose the frame.
            void draw(MWidget*            widget,
                      const MRect&        drawRect,
                      const MRect&        clipRect,
                      const std::wstring& text = std::wstring(),
                      CSS::FixGDIAlpha    fixAlpha = CSS::QuickOpaque,
                      int frameIndex = -1);

            void removeCache(MWidget* w);
            void removeCache(CSS::RenderRuleQuerier*);

            CSS::RenderRule getRenderRule(MWidget* w, unsigned int p = CSS::PC_Default);
            CSS::RenderRule getRenderRule(CSS::RenderRuleQuerier*, unsigned int p = CSS::PC_Default);

            // If the MWidget needs to update, this function calls MWidget::repaint();
            void updateWidgetAppearance(MWidget*);

            MStyleSheetStyle();
            ~MStyleSheetStyle();

        private:
            MSSSPrivate* mImpl;
    };





    inline CSS::RenderRule::RenderRule():data(0){}
    inline CSS::RenderRule::operator bool() const
        { return data != 0; }
    inline bool CSS::RenderRule::isValid() const
        { return data != 0; }
    inline bool CSS::RenderRule::operator==(const CSS::RenderRule& rhs) const
        { return data == rhs.data; }
    inline bool CSS::RenderRule::operator!=(const CSS::RenderRule& rhs) const
        { return data != rhs.data; }
    inline CSS::RenderRuleData* CSS::RenderRule::operator->()
        { return data; }
    inline CSS::RenderRuleQuerier::RenderRuleQuerier(const std::wstring& c, const std::wstring& o)
        : mclassName(c), mobjectName(o){}
    inline CSS::RenderRuleQuerier* CSS::RenderRuleQuerier::addChild(std::wstring& c, std::wstring& o)
    {
        RenderRuleQuerier* child = new RenderRuleQuerier(c,o);
        child->mparent = this;
        children.push_back(child);
        return child;
    }
    inline CSS::RenderRuleQuerier* CSS::RenderRuleQuerier::parent()
        { return mparent; }
    inline const std::wstring& CSS::RenderRuleQuerier::objectName() const
        { return mobjectName; }
    inline const std::wstring& CSS::RenderRuleQuerier::className() const
        { return mclassName; }
    CSS::TextRenderer CSS::RenderRule::getTextRenderer()
        { return textRenderer; }
    void CSS::RenderRule::setTextRenderer(TextRenderer tr, unsigned int maxSize)
    {
        textRenderer = tr;
        maxGdiFontPtSize = maxSize;
    }
    unsigned int CSS::RenderRule::getMaxGdiFontPtSize()
        { return maxGdiFontPtSize; }

    inline void CSS::RenderRule::draw(MGraphics& g,const MRect& wr, const MRect& cr,
        const std::wstring& t, CSS::FixGDIAlpha f, unsigned int i)
    { 
        if(!data) return;
        g.data->drawRenderRule(data,wr,cr,t,f,i);
    }
} // namespace MetalBone
