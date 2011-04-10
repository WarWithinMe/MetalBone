#ifndef GUI_MSTYLESHEET_H
#define GUI_MSTYLESHEET_H

#include "mb_switch.h"

#include <set>
#include <vector>
#include <string>
#include <Windows.h>
#include <d2d1.h>
#include <map>
#ifdef MSVC
#  include <unordered_map>
#else
#  include <tr1/unordered_map>
#endif

namespace MetalBone
{
	class MWidget;
	class MStyleSheetStyle;
	namespace CSS
	{
		enum PseudoClassType {
			PC_Default    = 0x1,        PC_Enabled      = 0x2,
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

		// 只包含了样式相关的属性，删除了位置相关的属性，
		// 而outline和box-shadow等效果应该使用MWidgetFilter来实现
		// Do not change the enum value's order. Because the algorithm rely on the order.
		enum PropertyType {
			PT_InheritBackground,
			PT_Background,             PT_BackgroundClip,             PT_BackgroundRepeat,
			PT_BackgroundPosition,     PT_BackgroundSize,             PT_BackgroundAlignment,

			PT_BorderImage,

			PT_Border,                 PT_BorderWidth,                PT_BorderColor,
			PT_BorderStyles,
			PT_BorderTop,              PT_BorderRight,                PT_BorderBottom,
			PT_BorderLeft,
			PT_BorderTopWidth,         PT_BorderRightWidth,           PT_BorderBottomWidth,
			PT_BorderLeftWidth,
			PT_BorderTopColor,         PT_BorderRightColor,           PT_BorderBottomColor,
			PT_BorderLeftColor,
			PT_BorderTopStyle,         PT_BorderRightStyle,           PT_BorderBottomStyle,
			PT_BorderLeftStyle,
			PT_BorderTopLeftRadius,    PT_BorderTopRightRadius,       PT_BorderBottomLeftRadius,
			PT_BorderBottomRightRadius,
			PT_BorderRadius,

			PT_Margin,                 PT_MarginTop,                  PT_MarginRight,
			PT_MarginBottom,           PT_MarginLeft,
			PT_Padding,                PT_PaddingTop,                 PT_PaddingRight,
			PT_PaddingBottom,          PT_PaddingLeft,

			PT_Width,                  PT_Height,
			PT_MinimumWidth,           PT_MinimumHeight,
			PT_MaximumWidth,           PT_MaximumHeight,

			PT_Color,                  PT_Font,                       PT_FontFamily,
			PT_FontSize,               PT_FontStyle,                  PT_FontWeight,
			PT_TextAlignment,          PT_TextDecoration,             PT_TextIndent,
			PT_TextOverflow,           PT_TextUnderlineStyle,         PT_TextOutline,
			PT_TextShadow,

			knownPropertyCount
		};

		enum ValueType {
			Value_Unknown,    Value_None,        Value_Transparent,

			Value_Bold,
			Value_Normal,     Value_Italic,      Value_Oblique,
			Value_Clip,       Value_Ellipsis,    Value_Wrap,
			Value_Underline,  Value_Overline,    Value_LineThrough,

			Value_Dashed,     Value_DotDash,     Value_DotDotDash, Value_Dotted,    Value_Solid,   Value_Wave,

			Value_Padding,    Value_Border,      Value_Content,    Value_Margin,
			Value_Top,        Value_Right,       Value_Left,       Value_Bottom,    Value_Center,
			Value_NoRepeat,   Value_RepeatX,     Value_RepeatY,    Value_Repeat,    Value_Stretch,
			Value_True,
			KnownValueCount
		};

		struct Selector;
		struct Declaration;
		struct StyleRule;
		struct CssValue;
		struct MatchedStyleRule
		{
			const Selector*  matchedSelector;
			const StyleRule* styleRule;
		};
		struct StyleSheet
		{
			typedef std::tr1::unordered_multimap<std::wstring, StyleRule*> StyleRuleElementMap;
			typedef std::tr1::unordered_multimap<std::wstring, StyleRule*> StyleRuleIdMap;
			~StyleSheet();
			std::vector<StyleRule*>	styleRules; // Keeps track of every StyleRule
			std::vector<StyleRule*>	universal;  // Universal
			StyleRuleElementMap		srElementMap;
			StyleRuleIdMap			srIdMap;
		};

		struct BackgroundRenderObject;
		struct BorderImageRenderObject;
		// I have removed some interfaces of BorderRenderObject,
		// because some only works for Simple~
		// some only works for Complex~
		// Maybe the BorderRenderObject is a bad design.
		// But for now, I don't have another idea.
		struct BorderRenderObject
		{
			enum ROType {
				SimpleBorder,
				RadiusBorder,
				ComplexBorder
			};

			ROType type;
			virtual ~BorderRenderObject() {}
			virtual void getBorderWidth(RECT&) = 0;
			virtual bool isVisible() = 0;
		};
		struct SimpleBorderRenderObject;
		struct ComplexBorderRenderObject;

		struct GeometryData
		{
			GeometryData():width(-1),height(-1),
				minWidth(-1),minHeight(-1),
				maxWidth(-1),maxHeight(-1){}
			int width;
			int height;
			int minWidth;
			int minHeight;
			int maxWidth;
			int maxHeight;
		};

		struct RenderRuleData
		{
			RenderRuleData():refCount(1),opaqueBackground(false),
					animated(false),hasMargin(false),
					hasPadding(false),geoData(0),
					borderImageRO(0),borderRO(0)
			{
				memset(&margin, 0,sizeof(D2D_RECT_U));
				memset(&padding,0,sizeof(D2D_RECT_U));
			}
			~RenderRuleData();
			int  refCount;
			bool opaqueBackground;
			bool animated; // Remark: We need to know whether the background is GIF animated image.
			bool hasMargin;
			bool hasPadding;

			std::vector<BackgroundRenderObject*> backgroundROs;
			BorderImageRenderObject* borderImageRO;
			BorderRenderObject* borderRO;
			GeometryData* geoData;

			D2D_RECT_U margin;
			D2D_RECT_U padding;

			// Return true if we changed the widget's size
			bool setGeometry(MWidget*);
			void draw(MWidget* w, int x, int y, const RECT& rect);
			void drawBackgrounds(ID2D1RenderTarget*,ID2D1Geometry*,const RECT& widgetRectInWnd, const RECT& clipRectInWnd);
			void drawBorderImage(ID2D1RenderTarget*,const RECT& widgetRectInWnd, const RECT& clipRectInWnd);
			void drawBorder(ID2D1RenderTarget*,const RECT& widgetRectInWnd, const RECT& clipRectInWnd);
		};
		struct RenderRuleCacheKey
		{
			std::vector<StyleRule*> styleRules;
			bool operator<(const RenderRuleCacheKey& rhs) const;
			RenderRuleCacheKey& operator=(RenderRuleCacheKey& rhs) { styleRules = rhs.styleRules; }
		};
		class RenderRule
		{
			// RenderRule is not thread-safe and it haven't to be.
			// Because all Styling/Drawing stuff should remain on the same thread(Main thread)
			public:
				RenderRule():data(0){}
				RenderRule::RenderRule(RenderRuleData* d):data(d) { if(data) ++data->refCount; }
				RenderRule::RenderRule(const RenderRule& d):data(d.data) { if(data) ++data->refCount; }
				~RenderRule() {
					if(data) {
						--data->refCount;
						if(data->refCount == 0)
							delete data;
					}
				}

				RenderRule& operator=(const RenderRule& other);
				inline bool isValid() const { return data != 0; }
				inline bool opaqueBackground() const { return data->opaqueBackground; }
				inline bool setGeometry(MWidget* w) { return data->setGeometry(w); }
			private:
				void init() { data = new RenderRuleData(); }
				RenderRuleData* operator->() { return data; }
				RenderRuleData* data;
			friend class MStyleSheetStyle;
		};
	} // namespace CSS

	class MStyleSheetStyle
	{
		public:
			MStyleSheetStyle():appStyleSheet(new CSS::StyleSheet()){}
			~MStyleSheetStyle();

			// Setting an empty css string will remove the existed StyleSheet
			void setAppSS				(const std::wstring&);
			void setWidgetSS			(MWidget*, const std::wstring&);

			// polish() calls getRenderRule() to generate RenderRule(default state)
			// for the widget. Then we set Widget Properties (width, height,
			// minWidth, hover, etc.) according to the RenderRule.
			// One should call polish first if there's no StyleRules(cached)
			// for a widget.
			void polish(MWidget* w);
			inline void draw(MWidget* w, int xPosInWnd, int yPosInWnd, 
						    const RECT& clipRectInWnd, unsigned int p = CSS::PC_Default);

			// Remove every stylesheet resource cached for MWidget w;
			inline void removeCache(MWidget*);

// TODO: Implement recreateResource
void recreateResources(MWidget*) {}


		private:
			typedef std::tr1::unordered_map<MWidget*, CSS::StyleSheet*> WidgetSSCache;
			WidgetSSCache    widgetSSCache; // Widget specific StyleSheets
			CSS::StyleSheet* appStyleSheet; // Application specific StyleSheets

			// Cache for RenderRules. Each RenderRule consists of several D2D resources.
			// Assume these resources always are sharable (i.e. either created
			// by hardware RenderTarget or software RenderTarget).
			// Remark:
			// **We also keep track of which render target has created the resources,
			// **so we can deal with D2DERR_RECREATE_TARGET error.
			// **(Not now, because I don't know if it's necessary to recreate sharable resources)
			struct PseudoRenderRuleMap {
				typedef std::tr1::unordered_map<unsigned int, MetalBone::CSS::RenderRule> Element;
				Element element;
			};
			typedef std::map<CSS::RenderRuleCacheKey, PseudoRenderRuleMap> RenderRuleMap;
			RenderRuleMap renderRuleCollection;

			// Cache for widgets. Provide fast look up for their associated RenderRules.
			// MatchedStyleRuleVector is ordered by specifity from low to high
			typedef std::vector<CSS::MatchedStyleRule> MatchedStyleRuleVector;
			typedef std::tr1::unordered_map<MWidget*, MatchedStyleRuleVector>	WidgetStyleRuleMap;
			typedef std::tr1::unordered_map<MWidget*, PseudoRenderRuleMap*>		WidgetRenderRuleMap;
			WidgetStyleRuleMap  widgetStyleRuleCache;
			WidgetRenderRuleMap widgetRenderRuleCache;

			// Each declaration can only declare one brush.
			typedef std::tr1::unordered_map<unsigned int, ID2D1SolidColorBrush*> D2D1SolidBrushMap;
			typedef std::tr1::unordered_map<std::wstring, ID2D1Bitmap*> D2D1BitmapMap;
			D2D1SolidBrushMap solidBrushCache;
			D2D1BitmapMap     bitmapCache;

			CSS::RenderRule getRenderRule(MWidget*, unsigned int pseudo);
			void getMachedStyleRules(MWidget*, MatchedStyleRuleVector&);
			void clearRenderRuleCacheRecursively(MWidget*);

			ID2D1Bitmap**                 createD2D1Bitmap     (const std::wstring&, bool& isOpaque);
			ID2D1SolidColorBrush**        createD2D1SolidBrush (CSS::CssValue&);
			CSS::BackgroundRenderObject*  createBackgroundRO   (unsigned int& opaqueType);
			CSS::BorderImageRenderObject* createBorderImageRO  (bool& isOpaqueBG);

			typedef std::multimap<CSS::PropertyType,CSS::Declaration*> DeclMap;
			void setSimpleBorederRO(CSS::SimpleBorderRenderObject*,  DeclMap::iterator&, DeclMap::iterator);
			void setComplexBorderRO(CSS::ComplexBorderRenderObject*, DeclMap::iterator&, DeclMap::iterator);

			// We save the working renderTarget and declaration here,
			// so we don't have to pass it around functions.
			ID2D1DCRenderTarget* workingRenderTarget;
			CSS::Declaration*    workingDeclaration;

			void removeResources();
#ifdef MB_DEBUG
		public: void dumpStyleSheet();
#endif
	};




	inline void MStyleSheetStyle::draw(MWidget* w, int x, int y, const RECT& r, unsigned int p)
	{
		CSS::RenderRule rule = getRenderRule(w,p);
		if(rule.isValid())
			rule->draw(w,x,y,r); 
	}
	inline void MStyleSheetStyle::removeCache(MWidget* w)
	{
		// We just have to remove from these two cache, without touching
		// renderRuleCollection, because some of them might be using by others.
		widgetStyleRuleCache.erase(w);
		widgetRenderRuleCache.erase(w);
	}
} // namespace MetalBone
#endif // MSTYLESHEET_H
