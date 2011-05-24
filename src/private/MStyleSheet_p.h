#pragma once
#include "CSSParser.h"

#include <map>
#include <d2d1.h>

using namespace std;
using namespace std::tr1;

namespace MetalBone
{
    enum BorderImagePortion
    {
        TopCenter = 0 ,
        CenterLeft    ,
        Center        ,
        CenterRight   ,
        BottomCenter  ,
        Conner
    };

    struct NewBrushInfo
    {
        unsigned int frameCount;
    };

    struct BrushHandle
    {
        enum  Type { Unknown, Solid, Gradient, Bitmap };

        void** brushPosition;
        Type   type;

        inline BrushHandle();
        inline BrushHandle(Type,void**);
        inline BrushHandle(const BrushHandle&);

        inline const BrushHandle& operator=(const BrushHandle&);

        // Get the recently created brush info.
        inline static const NewBrushInfo& getNewBrushInfo();

        // Never call Release() on the returned ID2D1Brush,
        // Because it will free the brush. 
        inline operator ID2D1Brush*() const;
        inline ID2D1Brush* getBrush() const;

        ID2D1BitmapBrush* getBorderPotion(BorderImagePortion, const MRect& widths);
    };

    class D2D1BrushPool
    {
        public:
            D2D1BrushPool ();
            ~D2D1BrushPool();

            BrushHandle getSolidBrush (MColor);
            BrushHandle getBitmapBrush(const wstring&);

            void removeCache();

            // Sets the renderTarget, so that we can use it to 
            // create brushes.
            inline void static setWorkingRT(ID2D1RenderTarget*);

        private:
            struct PortionBrushes
            {
                PortionBrushes () { memset(b,0,sizeof(ID2D1BitmapBrush*) * 5); }
                ~PortionBrushes() { for(int i =0; i<5; ++i) SafeRelease(b[i]); }
                ID2D1BitmapBrush* b[5];
            };

            typedef unordered_map<void**,  MColor>                     SolidColorMap;
            typedef unordered_map<void**,  wstring>                    BitmapPathMap;
            typedef unordered_map<MColor,  ID2D1SolidColorBrush*>      SolidBrushMap;
            typedef unordered_map<wstring, ID2D1BitmapBrush*>          BitmapBrushMap;
            typedef unordered_map<ID2D1BitmapBrush*, PortionBrushes>   PortionBrushMap;

            SolidBrushMap   solidBrushCache;
            SolidColorMap   solidColorMap;
            BitmapPathMap   bitmapPathMap;
            BitmapBrushMap  bitmapBrushCache;
            PortionBrushMap biPortionCache;

            NewBrushInfo    newBrushInfo; // Store the recently created brush infomation.

            void createBrush(const BrushHandle*);
            void createBorderImageBrush(const BrushHandle*, const MRect&);

            static ID2D1RenderTarget* workingRT;
            static D2D1BrushPool*     instance;

            friend struct BrushHandle;
    };

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

            inline static unsigned int   gdiRenderMaxFontSize();
            inline static D2D1BrushPool& getBrushPool();

        private:
            void setAppSS(const wstring&);
            void setWidgetSS(MWidget*, const wstring&);
            void clearRRCacheRecursively(MWidget*);
            void discardResource(ID2D1RenderTarget* = 0);
            void draw(MWidget*, ID2D1RenderTarget*, const MRect&, const MRect&, const wstring&, int);
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
            D2D1BrushPool        brushPool;
            RenderRuleMap        renderRuleCollection;
            MTimer               aniBGTimer;
            AniWidgetIndexMap    widgetAniBGIndexMap;

            static MSSSPrivate* instance;
            static unsigned int maxGdiFontPtSize;

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
            BrushHandle    brush;
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
            unsigned int values;
            BrushHandle  brush;
            MRect        widths;
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

            unsigned int width;
            ValueType    style;
            BrushHandle  brush;
            MColor       color;

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

            D2D1_RECT_U  styles;
            D2D1_RECT_U  widths;
            unsigned int radiuses[4]; // TL, TR, BL, BR
            MColor       colors[4];   // T, R, B, L
            BrushHandle  brushes[4];  // T, R, B, L
            bool         uniform;

            void getBorderWidth(MRect&) const;
            bool isVisible()            const;
            void checkUniform();

            ID2D1Geometry* createGeometry(const D2D1_RECT_F&);
            void draw(ID2D1RenderTarget*,ID2D1Geometry*,const D2D1_RECT_F&);

            void setColors(const CssValueArray&,size_t startIndex,size_t endIndexPlus);
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

            void init(MSSSPrivate::DeclMap&);
            // Return true if we changed the widget's size
            bool setGeometry(MWidget*);
            void draw(ID2D1RenderTarget*,const MRect&,const MRect&,const wstring&,unsigned int);

            MSize getStringSize(const std::wstring&, int maxWidth);
            void  getContentMargin(MRect&);

            bool  isBGSingleLoop();
            inline unsigned int getTotalFrameCount();
            inline bool         hasMargin()  const;
            inline bool         hasPadding() const;

            void drawBackgrounds(const MRect& widgetRect, const MRect& clipRect, ID2D1Geometry*, unsigned int frameIndex);
            void drawBorderImage(const MRect& widgetRect, const MRect& clipRect);
            void drawGdiText    (const D2D1_RECT_F& borderRect, const MRect& clipRect, const wstring& text);
            void drawD2DText    (const D2D1_RECT_F& borderRect, const wstring& text);

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

            D2D1_RECT_U* margin;
            D2D1_RECT_U* padding;
            int    refCount;
            int    totalFrameCount;
            bool   opaqueBackground;

            // Since we only deal with GUI in the main thread,
            // we can safely store the renderTarget in a static member for later use.
            static ID2D1RenderTarget* workingRT;
        };
    }

    inline RenderRuleData::RenderRuleData():
        borderImageRO(0), borderRO(0), geoRO(0),
        textRO(0), cursor(0), margin(0), padding(0),
        refCount(1), totalFrameCount(1), opaqueBackground(false){}
    inline unsigned int RenderRuleData::getTotalFrameCount()
        { return totalFrameCount; }
    inline bool RenderRuleData::hasMargin() const
        { return margin != 0; }
    inline bool RenderRuleData::hasPadding() const
        { return padding != 0; }

    inline BrushHandle::BrushHandle():type(Unknown),brushPosition(0){}
    inline BrushHandle::BrushHandle(Type t, void** p):
        type(t),brushPosition(p){}
    inline BrushHandle::BrushHandle(const BrushHandle& rhs):
        type(rhs.type),brushPosition(rhs.brushPosition){}
    inline const BrushHandle& BrushHandle::operator=(const BrushHandle& rhs) 
        { type = rhs.type; brushPosition = rhs.brushPosition; return *this; }
    inline const NewBrushInfo& BrushHandle::getNewBrushInfo()
        { return D2D1BrushPool::instance->newBrushInfo; }
    inline BrushHandle::operator ID2D1Brush*() const
    {
        if(*brushPosition == 0) D2D1BrushPool::instance->createBrush(this);
        return reinterpret_cast<ID2D1Brush*>(*brushPosition);
    }
    inline ID2D1Brush* BrushHandle::getBrush() const
    {
        if(*brushPosition == 0) D2D1BrushPool::instance->createBrush(this);
        return reinterpret_cast<ID2D1Brush*>(*brushPosition);
    }
    inline void D2D1BrushPool::setWorkingRT(ID2D1RenderTarget* rt)
        { workingRT = rt; }

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
    inline unsigned int MSSSPrivate::gdiRenderMaxFontSize()
        { return maxGdiFontPtSize; }
    inline D2D1BrushPool& MSSSPrivate::getBrushPool()
        { return instance->brushPool; }
    inline BorderImageRenderObject::BorderImageRenderObject():
        values(Value_Unknown),widths(){}
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
        { return !(width == 0 || color.isTransparent()); }
}