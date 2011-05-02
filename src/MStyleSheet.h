#ifndef GUI_MSTYLESHEET_H
#define GUI_MSTYLESHEET_H

#include "mb_switch.h"

#include <set>
#include <vector>
#include <string>
#include <Windows.h>
#include <d2d1.h>
#include <map>
#include <unordered_map>

namespace MetalBone
{
	class MSSSPrivate;
	class MStyleSheetStyle;
	class MWidget;
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

			PT_Cursor,

			PT_Font,                   PT_FontSize,                   PT_FontStyle,
			PT_FontWeight,             PT_Color,
			PT_TextAlignment,          PT_TextDecoration,             PT_TextOverflow,
			PT_TextUnderlineStyle,     PT_TextOutline,                PT_TextShadow,

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
			
			// The Cursor's order must be the same as MCursor::CursorType
			Value_Default,   Value_AppStarting, Value_Cross,   Value_Hand,    Value_Help,    Value_IBeam,
			Value_Wait,      Value_Forbidden,   Value_UpArrow, Value_SizeAll, Value_SizeVer, Value_SizeHor,
			Value_SizeBDiag, Value_SizeFDiag,   Value_Blank,

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

		struct RenderRuleCacheKey
		{
			std::vector<StyleRule*> styleRules;
			bool operator<(const RenderRuleCacheKey& rhs) const;
			RenderRuleCacheKey& operator=(RenderRuleCacheKey& rhs) { styleRules = rhs.styleRules; }
		};
		class RenderRuleData;
		class RenderRule
		{
			// RenderRule is not thread-safe and it haven't to be.
			// Because all Styling/Drawing stuff should remain on the same thread(Main thread)
			public:
				inline RenderRule();
				RenderRule(RenderRuleData* d);
				RenderRule(const RenderRule& d);
				~RenderRule();

				const RenderRule& operator=(const RenderRule& other);
				inline operator bool() const;
				inline bool isValid() const;
				bool opaqueBackground() const;
				void draw(ID2D1RenderTarget*,const RECT& widgetRectInRT, const RECT& clipRectInRT,
					 const std::wstring& text = std::wstring(),unsigned int frameIndex = 0);
				MCursor* getCursor();
				inline bool operator==(const RenderRule&) const;
				inline bool operator!=(const RenderRule&) const;
			private:
				void init();
				inline RenderRuleData* operator->();
				RenderRuleData* data;

			friend class MSSSPrivate;
			friend class MStyleSheetStyle;
		};
	} // namespace CSS

	class MStyleSheetStyle
	{
		public:
			enum TextRenderer
			{
				Gdi,
				Direct2D,
				AutoDetermine
			};
			MStyleSheetStyle();
			~MStyleSheetStyle();
			void setAppSS(const std::wstring& css);
			void setWidgetSS(MWidget* w, const std::wstring& css);
			void polish(MWidget* w);
			// clipRect should be equal to or inside the widgetRect.
			void draw(MWidget* w,ID2D1RenderTarget* rt, const RECT& widgetRectInRT, const RECT& clipRectInRT,
					const std::wstring& text = std::wstring(), int frameIndex = -1);
			void removeCache(MWidget* w);
			CSS::RenderRule getRenderRule(MWidget* w, unsigned int p = CSS::PC_Default);
			// Check if the widget needs to be unpdated.
			void updateWidgetAppearance(MWidget*);

			// If TextRenderer is AutoDetermine, we use GDI to render text when the font size is
			// no bigger than maxGdiFontPtSize.
			static void setTextRenderer(TextRenderer, unsigned int maxGdiFontPtSize = 16);
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
} // namespace MetalBone
#endif // MSTYLESHEET_H
