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
	namespace CSS {
		/*
		enum Alignment { AlignLeft = 0x1, AlignRight = 0x2, AlignHCenter = 0x4, AlignTop = 0x10, AlignBottom = 0x20,
			AlignVCenter = 0x40, AlignCenter = AlignVCenter | AlignHCenter };
		enum BorderStyle { BorderStyle_None, BorderStyle_Dotted, BorderStyle_Dashed, BorderStyle_Solid,
			BorderStyle_Double, BorderStyle_DotDash, BorderStyle_DotDotDash };
		enum Corner { TopLeftCorner, TopRightCorner, BottomLeftCorner, BottomRightCorner};
		enum TileMode { TileMode_Repeat, TileMode_Stretch };
		enum Repeat { Repeat_None, Repeat_X, Repeat_Y, Repeat_XY };
		enum Origin { Origin_Content, Origin_Padding, Origin_Border,Origin_Margin };
		enum TextOverflow { Overflow_Clip, Overflow_ellipsis, Overflow_WrapClip, Overflow_WrapEllipsis };
		enum TextDecoration { Decoration_None, Decoration_Underline, Decoration_Overline, Decoration_LineThrough };
		*/
		enum PseudoClassType {
			PC_Default		= 0x1,		PC_Enabled		= 0x2,
			PC_Disabled		= 0x4,		PC_Pressed		= 0x8,
			PC_Focus		= 0x10,		PC_Hover		= 0x20,
			PC_Checked		= 0x40,		PC_Unchecked	= 0x80,
			PC_Selected		= 0x100,	PC_Horizontal	= 0x200,
			PC_Vertical		= 0x400,	PC_Children		= 0x800,
			PC_ReadOnly		= 0x1000,	PC_Sibling		= 0x2000,
			PC_First		= 0x4000,	PC_Last			= 0x8000,
			PC_Active		= 0x10000,	PC_Editable		= 0x20000,
			PC_EditFocus	= 0x40000,	PC_Unknown		= 0x80000,
			knownPseudoCount = 20
		};

		// 只包含了样式相关的属性，删除了位置相关的属性，
		// 而outline和box-shadow等效果应该使用MWidgetFilter来实现
		enum PropertyType {
			PT_Background,				PT_BackgroundAttachment,		PT_BackgroundClip,
			PT_BackgroundColor,			PT_BackgroundImage,				PT_BackgroundPosition,
			PT_BackgroundRepeat,

			PT_BorderImage,
			PT_Border,					PT_BorderTop,					PT_BorderRight,
			PT_BorderBottom,			PT_BorderLeft,
			PT_BorderRadius,			PT_BorderTopLeftRadius,			PT_BorderTopRightRadius,
			PT_BorderBottomLeftRadius,	PT_BorderBottomRightRadius,
			PT_BorderWidth,				PT_BorderTopWidth,				PT_BorderRightWidth,
			PT_BorderBottomWidth,		PT_BorderLeftWidth,
			PT_BorderColor,				PT_BorderTopColor,				PT_BorderRightColor,
			PT_BorderBottomColor,		PT_BorderLeftColor,
			PT_BorderStyles,				PT_BorderTopStyle,				PT_BorderRightStyle,
			PT_BorderBottomStyle,		PT_BorderLeftStyle,

			PT_Color,					PT_Font,						PT_FontFamily,
			PT_FontSize,				PT_FontStyle,					PT_FontWeight,
			PT_TextAlignment,			PT_TextDecoration,				PT_TextIndent,
			PT_TextOverflow,				PT_TextUnderlineStyle,			PT_TextOutline,
			PT_TextShadow,

			PT_Height,					PT_Width,						PT_MaximumHeight,
			PT_MaximumWidth,			PT_MinimumHeight,				PT_MinimumWidth,

			PT_Margin,					PT_MarginBottom,				PT_MarginLeft,
			PT_MarginRight,				PT_MarginTop,
			PT_Padding,					PT_PaddingBottom,				PT_PaddingLeft,
			PT_PaddingRight,				PT_PaddingTop,

			knownPropertyCount
		};

		enum ValueType {
			Value_Unknown,		Value_None,		Value_Transparent,

			Value_Bold,
			Value_Normal,		Value_Italic,		Value_Oblique,
			Value_Clip,			Value_Ellipsis,		Value_Wrap,
			Value_Underline,		Value_Overline,		Value_LineThrough,
			Value_Dashed, Value_DotDash, Value_DotDotDash, Value_Dotted, Value_Solid, Value_Wave,

			Value_Padding,		Value_Border,		Value_Content,		Value_Margin,
			Value_Top,			Value_Right,			Value_Left,			Value_Bottom,		Value_Center,
			Value_NoRepeat,		Value_RepeatX,		Value_RepeatY,		Value_Repeat,		Value_Stretch,

			KnownValueCount
		};

		struct Selector;
		struct Declaration;
		struct StyleRule;
		struct MatchedStyleRule
		{
			const Selector*	 matchedSelector;
			const StyleRule*	 styleRule;
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

		struct RenderRuleData
		{
			RenderRuleData():refCount(1){}
			int refCount;
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
				RenderRule(RenderRuleData* d):data(d)	{ ++data->refCount; }
				RenderRule(RenderRule* d):data(d->data)	{ ++data->refCount; }
				~RenderRule() {
					if(data) {
						--data->refCount;
						if(data->refCount == 0)
							delete data;
					}
				}

				bool isValid() const { return data != 0; }
				void init() { data = new RenderRuleData(); }
				RenderRule& operator=(RenderRule& other) {
					if(&other == this)
						return other;
					if(data) {
						--data->refCount;
						if(data->refCount == 0)
							delete data;
					}

					data = other.data;
					++data->refCount;
					return *this;
				}

				// TODO: Implement draw();
				void draw(MWidget* w);
			private:
				RenderRuleData* data;
		};
	} // namespace CSS

	class MWidget;
	class MStyleSheetStyle
	{
		public:
			MStyleSheetStyle():appStyleSheet(new CSS::StyleSheet()){}
			~MStyleSheetStyle();

			// Setting an empty css string will remove the existed StyleSheet
			void setAppSS				(const std::wstring&);
			void setWidgetSS			(MWidget*, const std::wstring&);

			// Remark:
			// polish() basically calls getMachedStyleRules() to generate StyleRules
			// for the widget. And within getMachedStyleRules(), we set Widget Properties
			// (width, height, minWidth, hover, etc.) according to the StyleRules.
			// Every time a StyleSheet is set or removed, we can automatically
			// reset these properties within getMachedStyleRules().
			void polish(MWidget* w){} // TODO: implement polish()
			inline void draw(MWidget* w, unsigned int pseudo);

			// Remove every stylesheet resource cached for MWidget w;
			inline void removeCache		(MWidget*);


// TODO: void recreateResources();




		private:
			typedef std::tr1::unordered_map<MWidget*, CSS::StyleSheet*> WidgetSSCache;
			WidgetSSCache		widgetSSCache; // Widget specific StyleSheets
			CSS::StyleSheet*		appStyleSheet; // Application specific StyleSheets

			// Cache for RenderRules. Each RenderRule consists of several D2D resources.
			// Assume these resources always are sharable (i.e. either created
			// by hardware RenderTarget or software RenderTarget).
			//
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
			// MatchedStyleRuleVector is ordered by specifity from high to low
			typedef std::vector<CSS::MatchedStyleRule> MatchedStyleRuleVector;
			typedef std::tr1::unordered_map<MWidget*, MatchedStyleRuleVector>	WidgetStyleRuleMap;
			typedef std::tr1::unordered_map<MWidget*, PseudoRenderRuleMap*>		WidgetRenderRuleMap;
			WidgetStyleRuleMap widgetStyleRuleCache;
			WidgetRenderRuleMap widgetRenderRuleCache;

			CSS::RenderRule getRenderRule(MWidget*, unsigned int pseudo);
			void getMachedStyleRules(MWidget*, MatchedStyleRuleVector&);
			void clearRenderRuleCacheRecursively(MWidget*);

#ifdef MB_DEBUG
		public: void dumpStyleSheet();
#endif
	};




	inline void MStyleSheetStyle::draw(MWidget* w,unsigned int p) { getRenderRule(w,p).draw(w); }
	inline void MStyleSheetStyle::removeCache(MWidget* w)
	{
		// We just have to remove from these two cache, without touching renderRuleCollection
		widgetStyleRuleCache.erase(w);
		widgetRenderRuleCache.erase(w);
	}
} // namespace MetalBone
#endif // MSTYLESHEET_H
