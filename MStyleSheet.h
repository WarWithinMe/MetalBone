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
			PT_BackgroundColor,			PT_BackgroundImage,				PT_BackgroundOrigin,
			PT_BackgroundPosition,		PT_BackgroundRepeat,

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
			PT_MarginRight,				PT_MarginTop,					PT_Padding,
			PT_PaddingBottom,			PT_PaddingLeft,					PT_PaddingRight,
			PT_PaddingTop,

			knownPropertyCount
		};

		enum ValueType {
			Value_Unknown,		Value_Bold,			Value_Border,		Value_Bottom,
			Value_Center,		Value_Clip,			Value_Content,		Value_Dashed,
			Value_DotDash,		Value_DotDotDash,	Value_Dotted,		Value_Double,
			Value_Ellipsis,		Value_Italic,		Value_Left,			Value_LineThrough,
			Value_Margin,		Value_None,			Value_Normal,		Value_Oblique,
			Value_Overline,		Value_Padding,		Value_Repeat,		Value_Right,
			Value_Stretch,		Value_Solid,			Value_Transparent,	Value_Top,
			Value_Underline,		Value_Wave,			Value_Wrap,
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
			RenderRuleData():refCount(0){}
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
			public:
				RenderRule():data(0){}
				RenderRule(RenderRuleData* d):data(d) { ++data->refCount; }
				RenderRule(RenderRule* d):data(d->data) { ++data->refCount; }
				~RenderRule() {
					if(data) {
						--data->refCount;
						if(data->refCount == 0)
							delete data;
					}
				}

				bool isValid() const { return data != 0; }
				RenderRule& operator=(RenderRule& other)
				{
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

			private:
				RenderRuleData* data;
		};
	} // namespace CSS

	class MWidget;

	// Keep track of every individual StyleSheet ( parsed from individual css string )
	// Provide lookup of RenderRules for MWidget.
	class MStyleSheetStyle
	{
		public:
			MStyleSheetStyle():appStyleSheet(new CSS::StyleSheet()){}
			~MStyleSheetStyle();

			void setAppSS		(const std::wstring&);
			void setWidgetSS	(MWidget*, const std::wstring&);

			// Get a RenderRule for MWidget with pseudo.
			// Then use it to draw.
			CSS::RenderRule getStyleObject(MWidget*, unsigned int pseudo);

			// Rediscover any possible StyleRules for MWidget
//			void polish			(MWidget* w);

			// Remove every stylesheet resource cached for MWidget w;
			inline void removeCache		(MWidget* w);
			inline void removeWidgetSS	(MWidget* w);

		private:
			typedef std::tr1::unordered_map<MWidget*, CSS::StyleSheet* > WidgetSSCache;
			WidgetSSCache		widgetSSCache; // Widget specific StyleSheets
			CSS::StyleSheet*		appStyleSheet; // Application specific StyleSheets

			// Pesudo - RenderRule Map
			// StyleRules - RenderRules Map
			// HWND - StyleRules Map
			struct PseudoRenderRuleMap {
				typedef std::tr1::unordered_map<unsigned int, MetalBone::CSS::RenderRule> Element;
				Element element;
			};
			struct RenderRuleMap {
				typedef std::map<MetalBone::CSS::RenderRuleCacheKey, PseudoRenderRuleMap> Element;
				Element element;
			};
			typedef std::tr1::unordered_map<HWND, RenderRuleMap>				WindowRenderRuleMap;
			WindowRenderRuleMap windowRenderRuleCache;

			// Cache for D2D Resource. If the resource is sharable, we don't have to recreate them.
			// They fall into two category:
			// 1.Belong to hardware render target.
			// 2.Belong to software render target.
			//
			// We also keep track of which render target has created the resources,
			// so we can deal with D2DERR_RECREATE_TARGET error.
			// **(Not now, because I don't know if it's necessary to recreate the resources)
			//typedef std::tr1::unordered_map<ID2D1RenderTarget*, ID2D1Resource*> D2D1ResourceMap;



			// Cache for widgets. Provide fast look up for their associated RenderRules.
			typedef std::vector<CSS::MatchedStyleRule> MatchedStyleRuleVector; // Ordered by specifity from high to low
			typedef std::tr1::unordered_map<MWidget*, MatchedStyleRuleVector> WidgetStyleRuleMap;
			typedef std::tr1::unordered_map<MWidget*, PseudoRenderRuleMap*> WidgetRenderRuleMap;
			WidgetStyleRuleMap widgetStyleRuleCache;
			WidgetRenderRuleMap widgetRenderRuleCache;

			// Find every StyleRules for MWidget
			void getMachedStyleRules(MWidget*, MatchedStyleRuleVector&);
			void clearRenderRuleCacheRecursively(MWidget*);

#ifdef MB_DEBUG
		public:
			void dumpStyleSheet();
#endif
	};





	inline void MStyleSheetStyle::removeCache(MWidget* w)
	{
		widgetStyleRuleCache.erase(w);
		widgetRenderRuleCache.erase(w);
	}

	inline void MStyleSheetStyle::removeWidgetSS(MWidget* w)
	{
		WidgetSSCache::const_iterator iter = widgetSSCache.find(w);
		if(iter != widgetSSCache.end())
		{
			delete iter->second;
			widgetSSCache.erase(iter);
		}
	}

} // namespace MetalBone
#endif // MSTYLESHEET_H
