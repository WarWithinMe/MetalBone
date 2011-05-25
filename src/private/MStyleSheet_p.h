#pragma once
#include "CSSParser.h"
#include "MD2DPaintContext.h"

#include <map>
#include <d2d1.h>

using namespace std;
using namespace std::tr1;

namespace MetalBone
{
    namespace CSS
    { 
        enum   PropertyType;
        struct Declaration;
        class  RenderRuleQuerier;
    }

    using namespace CSS;
    class MSSSPrivate
    {
        public:
            typedef multimap<PropertyType, Declaration*> DeclMap;

            MSSSPrivate ();
            ~MSSSPrivate();

        private:
            void setAppSS(const wstring&);
            void setWidgetSS(MWidget*, const wstring&);
            void clearRRCacheRecursively(MWidget*);
            void draw(MWidget*, const MRect&, const MRect&, const wstring&, int);
            void polish(MWidget*);

            inline void removeCache(MWidget*);
            inline void removeCache(RenderRuleQuerier*);

            typedef unordered_map<MWidget*, StyleSheet*> WidgetSSCache;

            // A RenderRule is widget-specific and pseudo-specific.
            // How to build a RenderRule for MWidget "w" with Pseudo "p"?
            //
            // 1. We first find out every StyleRule that affects "w".
            // 2. We put these StyleRules in a MatchedStyleRuleVector "msrv".
            // 3. Put this "msrv" into a WidgetStyleRuleMap.
            //    (After step 3, we don't have to re-search for the StyleRules
            //     again, if we need to generate another RenderRule for "w")
            // 4. Now we have the "msrv", we find out those match "p",
            //    then we make a RenderRule "rr" based on the result.
            // 5. We put the "rr" in a PseudoRenderRuleMap "prrm" with "p"
            //    as its key, and put "prrm" into RenderRuleMap with the "msrv"
            //    as its key.
            //    (After step 5, we don't need to build a new RenderRule if we
            //     have the same MatchedStyleRuleVector and Pseudo) 
            // 6. We also put this prrm into a WidgetRenderRuleMap, so that with
            //    the same MWidget "w" and Pseudo "p", we could re-use the RenderRule "rr" 

            // MatchedStyleRuleVector is ordered by specifity from low to high
            typedef vector<MatchedStyleRule> MatchedStyleRuleVector;
            struct PseudoRenderRuleMap {
                typedef unordered_map<unsigned int, RenderRule> Element;
                Element element;
            };
            typedef map<RenderRuleCacheKey, PseudoRenderRuleMap>              RenderRuleMap;
            typedef unordered_map<MWidget*,           MatchedStyleRuleVector> WidgetStyleRuleMap;
            typedef unordered_map<MWidget*,           PseudoRenderRuleMap*>   WidgetRenderRuleMap;
            typedef unordered_map<RenderRuleQuerier*, MatchedStyleRuleVector> QuerierStyleRuleMap;
            typedef unordered_map<RenderRuleQuerier*, PseudoRenderRuleMap*>   QuerierRenderRuleMap;
            typedef unordered_map<MWidget*,           unsigned int>           AniWidgetIndexMap;

            void getMachedStyleRules(MWidget*,           MatchedStyleRuleVector&);
            void getMachedStyleRules(RenderRuleQuerier*, MatchedStyleRuleVector&);

            inline RenderRule getRenderRule(MWidget*,           unsigned int);
            inline RenderRule getRenderRule(RenderRuleQuerier*, unsigned int);

            template<class Querier, class QuerierRRMap, class QuerierSRMap>
            RenderRule getRenderRule(Querier*, unsigned int, QuerierRRMap&, QuerierSRMap&);

            void updateAniWidgets();
            void addAniWidget(MWidget*);
            void removeAniWidget(MWidget*);

            WidgetSSCache        widgetSSCache; // Widget specific StyleSheets
            WidgetStyleRuleMap   widgetStyleRuleCache;
            WidgetRenderRuleMap  widgetRenderRuleCache;
            QuerierStyleRuleMap  querierStyleRuleCache;
            QuerierRenderRuleMap querierRenderRuleCache;
            StyleSheet*          appStyleSheet; // Application specific StyleSheets
            RenderRuleMap        renderRuleCollection;
            MTimer               aniBGTimer;
            AniWidgetIndexMap    widgetAniBGIndexMap;

            static MSSSPrivate* instance;

            friend class MStyleSheetStyle;
    };

    namespace CSS
    {
        struct GeometryRenderObject
        {
            inline GeometryRenderObject();

            int x       , y;
            int width   , height;
            int minWidth, minHeight;
            int maxWidth, maxHeight;
        };

        struct BackgroundRenderObject
        {
            inline BackgroundRenderObject();

            // We remember the brush's pointer, so that if the brush is recreated,
            // we can recheck the background property again. But if a brush is recreated
            // at the same position, we won't know.
            D2DBrushHandle brush;
            ID2D1Brush*    checkedBrush;
            unsigned int   x, y, width, height;
            unsigned int   userWidth, userHeight;
            unsigned int   values;
            unsigned short frameCount; // Do we need more frames?
            bool           infiniteLoop;
        };

        struct BorderImageRenderObject
        {
            inline BorderImageRenderObject();
            D2DBrushHandle brush;
            unsigned int   values;
        };

        struct BorderRenderObject
        {
            enum BROType { SimpleBorder, RadiusBorder, ComplexBorder };
            BROType type;
            virtual ~BorderRenderObject() {}
            virtual void getBorderWidth(MRect&) const = 0;
            virtual bool isVisible()            const = 0;
        };

        struct SimpleBorderRenderObject : public BorderRenderObject
        {
            inline SimpleBorderRenderObject();

            D2DBrushHandle brush;
            unsigned int   width;
            ValueType      style;
            bool           isColorTransparent;

            void getBorderWidth(MRect&) const;
            bool isVisible()            const;
            int  getWidth()             const;
        };
        struct RadiusBorderRenderObject : public SimpleBorderRenderObject
        {
            inline RadiusBorderRenderObject();
            int radius;
        };
        struct ComplexBorderRenderObject : public BorderRenderObject
        {
            ComplexBorderRenderObject();

            D2DBrushHandle brushes[4];  // T, R, B, L
            RectU          styles;
            RectU          widths;
            unsigned int   radiuses[4]; // TL, TR, BL, BR
            bool           uniform;
            bool           isTransparent;

            void getBorderWidth(MRect&) const;
            bool isVisible()            const;

            ID2D1Geometry* createGeometry(const D2D1_RECT_F&);
            void draw(ID2D1RenderTarget*,ID2D1Geometry*,const D2D1_RECT_F&);
        };

        // TextRenderObject is intended to use GDI to render text,
        // so that we can take the benefit of GDI++ (not GDI+) to enhance
        // the text output quality.
        // Note: we can however draw the text with D2D, the text output looks
        // ok in my laptop, but ugly on my old 17" LCD screen. Anyway, I don't
        // know to to achieve blur effect with D2D yet.
        struct TextRenderObject
        {
            inline TextRenderObject(const MFont& f);

            MFont  font;
            MColor color;
            MColor outlineColor;
            MColor shadowColor;
            
            unsigned int values;  // AlignmentXY, Decoration, OverFlow
            ValueType    lineStyle;

            short shadowOffsetX;
            short shadowOffsetY;
            char  shadowBlur;
            char  outlineWidth;
            char  outlineBlur;

            unsigned int       getGDITextFormat();
            IDWriteTextFormat* createDWTextFormat();
        };

        struct RenderRuleData
        {
            inline RenderRuleData();
            ~RenderRuleData();

            void draw(MD2DPaintContext&,const MRect&,const MRect&,const wstring&,unsigned int);

            void drawBackgrounds(const MRect& widgetRect, const MRect& clipRect, ID2D1Geometry*, unsigned int frameIndex);
            void drawGdiText    (const D2D1_RECT_F& borderRect, const MRect& clipRect, const wstring& text);
            void drawD2DText    (const D2D1_RECT_F& borderRect, const wstring& text);

            // Return true if we changed the widget's size
            bool  setGeometry(MWidget*);
            MSize getStringSize(const std::wstring&, int maxWidth);
            MRect getContentMargin();

            inline bool         hasMargin()          const;
            inline bool         hasPadding()         const;
            inline bool         isBGSingleLoop()     const;
            inline unsigned int getTotalFrameCount() const;

            // Initialization
            void init(MSSSPrivate::DeclMap&);
            BackgroundRenderObject* createBackgroundRO(CssValueArray&);
            void createBorderImageRO(CssValueArray&);
            void setSimpleBorderRO  (MSSSPrivate::DeclMap::iterator&, MSSSPrivate::DeclMap::iterator);
            void setComplexBorderRO (MSSSPrivate::DeclMap::iterator&, MSSSPrivate::DeclMap::iterator);

            vector<BackgroundRenderObject*> backgroundROs;
            BorderImageRenderObject*        borderImageRO;
            BorderRenderObject*             borderRO;
            GeometryRenderObject*           geoRO;
            TextRenderObject*               textRO;
            MCursor*                        cursor;
            RectU*                          margin;
            RectU*                          padding;

            int    refCount;
            int    totalFrameCount;
            bool   opaqueBackground;

            static ID2D1RenderTarget* workingRT;
        };
    }




    inline RenderRuleData::RenderRuleData():
        borderImageRO(0), borderRO(0), geoRO(0),
        textRO(0), cursor(0), margin(0), padding(0),
        refCount(1), totalFrameCount(1), opaqueBackground(false){}
    inline unsigned int RenderRuleData::getTotalFrameCount() const
        { return totalFrameCount; }
    inline bool RenderRuleData::hasMargin() const
        { return margin != 0; }
    inline bool RenderRuleData::hasPadding() const
        { return padding != 0; }
    inline bool RenderRuleData::isBGSingleLoop() const
    {
        for(unsigned int i = 0; i < backgroundROs.size(); ++i)
        {
            if(backgroundROs.at(i)->infiniteLoop)
                return false;
        }
        return true;
    }

    inline RenderRule MSSSPrivate::getRenderRule(MWidget* w, unsigned int p)
        { return getRenderRule(w,p,widgetRenderRuleCache,widgetStyleRuleCache); }
    inline RenderRule MSSSPrivate::getRenderRule(RenderRuleQuerier* q, unsigned int p)
        { return getRenderRule(q,p,querierRenderRuleCache,querierStyleRuleCache); }

    inline void MSSSPrivate::removeCache(MWidget* w)
    {
        // We just have to remove from these two cache, without touching
        // renderRuleCollection, because some of them might be using by others.
        widgetStyleRuleCache.erase(w);
        widgetRenderRuleCache.erase(w);
        widgetAniBGIndexMap.erase(w);
    }
    inline void MSSSPrivate::removeCache(RenderRuleQuerier* q)
    { 
        querierStyleRuleCache.erase(q);
        querierRenderRuleCache.erase(q);
    }
    inline BorderImageRenderObject::BorderImageRenderObject():
        values(Value_Unknown){}
    inline GeometryRenderObject::GeometryRenderObject():
        x(INT_MAX),y(INT_MAX), 
        width(-1), height(-1), minWidth(-1),
        minHeight(-1), maxWidth(-1), maxHeight(-1){}
    inline TextRenderObject::TextRenderObject(const MFont& f):
        font(f), outlineColor(0), shadowColor(0),
        values(Value_Unknown), lineStyle(Value_Solid), 
        shadowOffsetX(0), shadowOffsetY(0), shadowBlur(0),
        outlineWidth(0), outlineBlur(0){}
    inline BackgroundRenderObject::BackgroundRenderObject():
        checkedBrush(0), x(0), y(0), width(0), height(0), userWidth(0),
        userHeight(0), values(Value_Top | Value_Left | Value_Margin),
        frameCount(1), infiniteLoop(true){}
    inline SimpleBorderRenderObject::SimpleBorderRenderObject():
        width(0),style(Value_Solid) { type = SimpleBorder; }
    inline RadiusBorderRenderObject::RadiusBorderRenderObject():
        radius(0){ type = RadiusBorder; }
    void SimpleBorderRenderObject::getBorderWidth(MRect& rect) const
        { rect.left = rect.right = rect.bottom = rect.top = width; }
    int  SimpleBorderRenderObject::getWidth() const
        { return width; }
    bool SimpleBorderRenderObject::isVisible() const
        { return !(width == 0 || isColorTransparent); }
}