#include "MResource.h"
#include "MStyleSheet.h"
#include "MWidget.h"
#include "MApplication.h"
#include "MBGlobal.h"

#include <D2d1helper.h>
#include <algorithm>
#include <wincodec.h>
#include <stdlib.h>
#include <typeinfo>
#include <sstream>
#include <list>
#include <map>
#include <GdiPlus.h>

namespace MetalBone
{
	// ========== StyleSheet ==========
	namespace CSS
	{
		struct CssValue
		{
			// Color store inside CssValue is ARGB format. (The same as MColor)
			enum  Type    { Unknown, Number, Length, Identifier, Uri, Color, String};
			union Variant
			{
				int           vint;
				unsigned int  vuint;
				std::wstring* vstring;
				ValueType     videntifier;
			};

			Type    type;
			Variant data;

			inline CssValue(const CssValue&);
			explicit inline CssValue(Type t = Unknown);

			inline MColor getColor() const;
 
			inline const CssValue& operator=(const CssValue&);
		};

		inline CssValue::CssValue(const CssValue& rhs):type(rhs.type),data(rhs.data){}
		inline CssValue::CssValue(Type t):type(t)
			{ data.vint = 0; }
		inline const CssValue& CssValue::operator=(const CssValue& rhs)
			{ type = rhs.type; data = rhs.data; return *this; }
		inline MColor CssValue::getColor() const
			{ M_ASSERT(type == Color); return MColor(data.vuint); }

		typedef std::vector<CssValue> CssValueArray;

		// 1. StyleRule     - x:hover , y:clicked > z:checked { prop1: value1; prop2: value2; }
		// 2. Selector      - x:hover | y:clicked > z:checked
		// 3. BasicSelector - x:hover | y:clicked | z:checked
		// 4. Declaration   - prop1: value1; | prop2: value2;
		struct Declaration
		{
			~Declaration();
			void addValue(const std::wstring& css, int index, int length);
			PropertyType  property;
			CssValueArray values;
		};

		struct BasicSelector
		{
			enum Relation { NoRelation, MatchNextIfAncestor, MatchNextIfParent };

			BasicSelector():
				pseudo(PC_Default),pseudoCount(0),
				relationToNext(MatchNextIfAncestor){}

			void addPseudoAndClearInput(std::wstring& p);

			std::wstring elementName; // ClassA .ClassA
			std::wstring id; // #Id
			unsigned int pseudo; // pseudo may contain a pseudo-element and many pseudo-classes
			unsigned int pseudoCount;
			Relation     relationToNext;
		};

		struct Selector
		{
			Selector(const std::wstring* css, int index = 0,int length = -1);
			~Selector();

			std::vector<BasicSelector*> basicSelectors;
			int  specificity() const;
			bool matchPseudo(unsigned int) const;
			inline unsigned int pseudo() const;
		};

		struct StyleRule
		{
			~StyleRule();
			std::vector<Selector*> selectors;
			std::vector<Declaration*> declarations;
			unsigned int order;
		};

		class MCSSParser
		{
			public:
				MCSSParser(const std::wstring& c);
				void parse(StyleSheet*);

			private:
				const std::wstring* css;
				const int cssLength;
				int  pos;
				bool noSelector;

				void skipWhiteSpace();
				void skipComment();
				bool parseSelector(StyleRule*);
				void parseDeclaration(StyleRule*);
		};

		struct CSSValuePair {
			const wchar_t* name;
			unsigned int   value;
		};
	} // namespace CSS

	using namespace CSS;
	static const CSSValuePair pseudos[knownPseudoCount] = {
		{ L"active",      PC_Active     },
		{ L"checked",     PC_Checked    },
		{ L"default",     PC_Default    },
		{ L"disabled",    PC_Disabled   },
		{ L"edit-focus",  PC_EditFocus  },
		{ L"editable",    PC_Editable   },
		{ L"enabled",     PC_Enabled    },
		{ L"first",       PC_First      },
		{ L"focus",       PC_Focus      },
		{ L"has-children",PC_Children   },
		{ L"has-siblings",PC_Sibling    },
		{ L"horizontal",  PC_Horizontal },
		{ L"hover",       PC_Hover      },
		{ L"last",        PC_Last       },
		{ L"pressed",     PC_Pressed    },
		{ L"read-only",   PC_ReadOnly   },
		{ L"selected",    PC_Selected   },
		{ L"unchecked",   PC_Unchecked  },
		{ L"vertical",    PC_Vertical   }
	};

	static const CSSValuePair properties[knownPropertyCount] = {
		{ L"background",                 PT_Background              },
		{ L"background-alignment",       PT_BackgroundAlignment     },
		{ L"background-clip",            PT_BackgroundClip          },
		{ L"background-position",        PT_BackgroundPosition      },
		{ L"background-repeat",          PT_BackgroundRepeat        },
		{ L"background-size",            PT_BackgroundSize          },
		{ L"border",                     PT_Border                  },
		{ L"border-bottom",              PT_BorderBottom            },
		{ L"border-bottom-color",        PT_BorderBottomColor       },
		{ L"border-bottom-left-radius",  PT_BorderBottomLeftRadius  },
		{ L"border-bottom-right-radius", PT_BorderBottomRightRadius },
		{ L"border-bottom-style",        PT_BorderBottomStyle       },
		{ L"border-bottom-width",        PT_BorderBottomWidth       },
		{ L"border-color",               PT_BorderColor             },
		{ L"border-image",               PT_BorderImage             },
		{ L"border-left",                PT_BorderLeft              },
		{ L"border-left-color",          PT_BorderLeftColor         },
		{ L"border-left-style",          PT_BorderLeftStyle         },
		{ L"border-left-width",          PT_BorderLeftWidth         },
		{ L"border-radius",              PT_BorderRadius            },
		{ L"border-right",               PT_BorderRight             },
		{ L"border-right-color",         PT_BorderRightColor        },
		{ L"border-right-style",         PT_BorderRightStyle        },
		{ L"border-right-width",         PT_BorderRightWidth        },
		{ L"border-style",               PT_BorderStyles            },
		{ L"border-top",                 PT_BorderTop               },
		{ L"border-top-color",           PT_BorderTopColor          },
		{ L"border-top-left-radius",     PT_BorderTopLeftRadius     },
		{ L"border-top-right-radius",    PT_BorderTopRightRadius    },
		{ L"border-top-style",           PT_BorderTopStyle          },
		{ L"border-top-width",           PT_BorderTopWidth          },
		{ L"border-width",               PT_BorderWidth             },
		{ L"color",                      PT_Color                   },
		{ L"cursor",                     PT_Cursor                  },
		{ L"font",                       PT_Font                    },
		{ L"font-size",                  PT_FontSize                },
		{ L"font-style",                 PT_FontStyle               },
		{ L"font-weight",                PT_FontWeight              },
		{ L"height",                     PT_Height                  },
		{ L"inherit-background",         PT_InheritBackground       },
		{ L"margin",                     PT_Margin                  },
		{ L"margin-bottom",              PT_MarginBottom            },
		{ L"margin-left",                PT_MarginLeft              },
		{ L"margin-right",               PT_MarginRight             },
		{ L"margin-top",                 PT_MarginTop               },
		{ L"max-height",                 PT_MaximumHeight           },
		{ L"max-width",                  PT_MaximumWidth            },
		{ L"min-height",                 PT_MinimumHeight           },
		{ L"min-width",                  PT_MinimumWidth            },
		{ L"padding",                    PT_Padding                 },
		{ L"padding-bottom",             PT_PaddingBottom           },
		{ L"padding-left",               PT_PaddingLeft             },
		{ L"padding-right",              PT_PaddingRight            },
		{ L"padding-top",                PT_PaddingTop              },
		{ L"text-align",                 PT_TextAlignment           },
		{ L"text-decoration",            PT_TextDecoration          },
		{ L"text-outline",               PT_TextOutline             },
		{ L"text-overflow",              PT_TextOverflow            },
		{ L"text-shadow",                PT_TextShadow              },
		{ L"text-underline-style",       PT_TextUnderlineStyle      },
		{ L"width",                      PT_Width                   }
	};
	static const CSSValuePair knownValues[KnownValueCount - 1] = {
		{ L"bold",         Value_Bold        },
		{ L"border",       Value_Border      },
		{ L"bottom",       Value_Bottom      },
		{ L"center",       Value_Center      },
		{ L"clip",         Value_Clip        },
		{ L"content",      Value_Content     },
		{ L"crosshair",    Value_Cross       },
		{ L"dashed",       Value_Dashed      },
		{ L"default",      Value_Default     },
		{ L"dot-dash",     Value_DotDash     },
		{ L"dot-dot-dash", Value_DotDotDash  },
		{ L"dotted",       Value_Dotted      },
		{ L"ellipsis",     Value_Ellipsis    },
		{ L"ew-resize",    Value_SizeHor     },
		{ L"help",         Value_Help        },
		{ L"italic",       Value_Italic      },
		{ L"left",         Value_Left        },
		{ L"line-through", Value_LineThrough },
		{ L"margin",       Value_Margin      },
		{ L"move",         Value_SizeAll     },
		{ L"nesw-resize",  Value_SizeBDiag   },
		{ L"no-cursor",    Value_Blank       },
		{ L"no-repeat",    Value_NoRepeat    },
		{ L"none",         Value_None        },
		{ L"normal",       Value_Normal      },
		{ L"not-allowed",  Value_Forbidden   },
		{ L"ns-resize",    Value_SizeVer     },
		{ L"nwse-resize",  Value_SizeFDiag   },
		{ L"oblique",      Value_Oblique     },
		{ L"overline",     Value_Overline    },
		{ L"padding",      Value_Padding     },
		{ L"pointer",      Value_Hand        },
		{ L"progress",     Value_AppStarting },
		{ L"repeat",       Value_Repeat      },
		{ L"repeat-x",     Value_RepeatX     },
		{ L"repeat-y",     Value_RepeatY     },
		{ L"right",        Value_Right       },
		{ L"solid",        Value_Solid       },
		{ L"stretch",      Value_Stretch     },
		{ L"text",         Value_IBeam       },
		{ L"top",          Value_Top         },
		{ L"transparent",  Value_Transparent },
		{ L"true",         Value_True        },
		{ L"underline",    Value_Underline   },
		{ L"up-arrow",     Value_UpArrow     },
		{ L"wait",         Value_Wait        },
		{ L"wave",         Value_Wave        },
		{ L"wrap",         Value_Wrap        },
	};

	const CSSValuePair* findCSSValue(const std::wstring& p, const CSSValuePair values[],int valueCount)
	{
		const CSSValuePair* crItem = 0;
		int left = 0;
		--valueCount;
		while(left <= valueCount)
		{
			int middle = (left + valueCount) >> 1;
			crItem = &(values[middle]);
			int result = wcscmp(crItem->name,p.c_str());
			if(result == 0)
				return crItem;
			else if(result == 1)
				valueCount = middle - 1;
			else
				left = middle + 1;
		}

		return 0;
	}

	int Selector::specificity() const
	{
		int val = 0;
		for (unsigned int i = 0; i < basicSelectors.size(); ++i) {
			const BasicSelector* sel = basicSelectors.at(i);
			if (!sel->elementName.empty())
				val += 1;

			val += sel->pseudoCount * 0x10;
			if(!sel->id.empty())
				val += 0x100;
		}
		return val;
	}

	bool Selector::matchPseudo(unsigned int p) const
	{
		BasicSelector* bs = basicSelectors.at(basicSelectors.size() - 1);
		// Always match the default pseudo
		if(PC_Default == bs->pseudo)
			return true;
		// If bs->pseudo is a subset of p, it matches.
		return (bs->pseudo & p) != bs->pseudo ? false : (PC_Default != p);
	}

	inline unsigned int Selector::pseudo() const
		{ return basicSelectors.at(basicSelectors.size() - 1)->pseudo; }

	StyleSheet::~StyleSheet()
	{
		int length = styleRules.size();
		for(int i = 0; i < length; ++i)
			delete styleRules.at(i);
	}

	StyleRule::~StyleRule()
	{
		int length = selectors.size();
		for(int i = 0; i < length; ++i)
			delete selectors.at(i);

		length = declarations.size();
		for(int i = 0; i < length; ++i)
			delete declarations.at(i);
	}

	Selector::~Selector()
	{
		int length = basicSelectors.size();
		for(int i = 0;i < length; ++i)
			delete basicSelectors.at(i);
	}

	Declaration::~Declaration()
	{
		int length = values.size();
		for(int i = 0; i< length; ++i) {
			// We cannot delete the String in the ~CssValue();
			// Because the String are intended to share among CssValues.
			switch(values.at(i).type)
			{
				case CssValue::Uri:
				case CssValue::String:
					delete values.at(i).data.vstring;
				default: break;
			}
		}
	}

	MCSSParser::MCSSParser(const std::wstring& c):
		css(&c),cssLength(c.size()),
		pos(cssLength - 1),noSelector(false)
	{
		// Test if Declaration block contains Selector
		while(pos > 0) {
			if(iswspace(c.at(pos)))
				--pos;
			else if(c.at(pos) == L'/' && c.at(pos - 1) == L'*') {
				pos -= 2;
				while(pos > 0) {
					if(c.at(pos) == L'*' && c.at(pos-1) == L'/') {
						pos -= 2;
						break;
					}
					--pos;
				}
			}else {
				noSelector = (c.at(pos) != L'}');
				break;
			}
		}
	}

	void MCSSParser::skipWhiteSpace()
	{
		while(pos < cssLength) {
			if(iswspace(css->at(pos)))
				++pos;
			else
				break;
		}
	}

	// if meet "/*", call this function to make pos behind "*/"
	void MCSSParser::skipComment()
	{
		pos += 2;
		int cssLengthMinus = cssLength - 1;
		while(pos < cssLengthMinus) {
			if(css->at(pos) == L'*' && css->at(pos + 1) == L'/') {
				pos += 2;
				break;
			}
			++pos;
		}
	}

	void MCSSParser::parse(StyleSheet* ss)
	{
		pos = 0;
		std::wstring css_copy;
		if(noSelector)
		{
			css_copy.append(L"*{");
			css_copy.append(*css);
			css_copy.append(1,L'}');
			css = &css_copy;
		}

		int order = 0;
		while(pos < cssLength)
		{
			StyleRule* newStyleRule = new StyleRule();
			newStyleRule->order = order;
			// If we successfully parsed Selector, we then parse Declaration.
			// If we failed to parse Declaration, the Declaration must be empty,
			// so that we can delete the created StyleRule.
			if(parseSelector(newStyleRule))
				parseDeclaration(newStyleRule);

			if(newStyleRule->declarations.empty()) {
				delete newStyleRule;
			} else {
				const std::vector<Selector*>& sels = newStyleRule->selectors;
				for(unsigned int i = 0; i < sels.size(); ++i)
				{
					const Selector* sel = sels.at(i);
					const BasicSelector* bs = sel->basicSelectors.at(sel->basicSelectors.size() - 1);
					if(!bs->id.empty()) {
						ss->srIdMap.insert(StyleSheet::StyleRuleIdMap::value_type(bs->id,newStyleRule));
					} else if(!bs->elementName.empty())
						ss->srElementMap.insert(StyleSheet::StyleRuleElementMap::value_type(bs->elementName,newStyleRule));
					else
						ss->universal.push_back(newStyleRule);
				}

				ss->styleRules.push_back(newStyleRule);
			}

			++order;
		}
	}

	bool MCSSParser::parseSelector(StyleRule* sr)
	{
		skipWhiteSpace();

		int crSelectorStartIndex = pos;
		std::wstring commentBuffer;
		int cssLengthMinus = cssLength - 1;
		wchar_t byte;
		while(pos < cssLengthMinus)
		{
			byte = css->at(pos);
			if(byte == L'{') // Declaration Block
			{
				int posBeforeEndingSpace = pos;
				while(iswspace(css->at(posBeforeEndingSpace - 1)))
					--posBeforeEndingSpace;
				Selector* sel;
				if(commentBuffer.size() > 0)
				{
					commentBuffer.append(*css,crSelectorStartIndex,posBeforeEndingSpace - crSelectorStartIndex);
					sel = new Selector(&commentBuffer);
				}else
					sel = new Selector(css,crSelectorStartIndex,posBeforeEndingSpace - crSelectorStartIndex);
				sr->selectors.push_back(sel);
				break;

			}else if(byte == L',')
			{
				int posBeforeEndingSpace = pos;
				while(iswspace(css->at(posBeforeEndingSpace - 1)))
					--posBeforeEndingSpace;
				Selector* sel;
				if(commentBuffer.size() > 0)
				{
					commentBuffer.append(*css,crSelectorStartIndex,posBeforeEndingSpace - crSelectorStartIndex);
					sel = new Selector(&commentBuffer);
					commentBuffer.clear();
				}else
					sel = new Selector(css,crSelectorStartIndex,posBeforeEndingSpace - crSelectorStartIndex);
				sr->selectors.push_back(sel);

				++pos;
				skipWhiteSpace();
				crSelectorStartIndex = pos;
			}else if(byte == L'/' && css->at(pos + 1) == L'*')
			{
				commentBuffer.append(*css,crSelectorStartIndex,pos-crSelectorStartIndex);
				skipComment();
				if(commentBuffer.size() == 0)
					skipWhiteSpace();
				crSelectorStartIndex = pos;
				continue;
			}
			++pos;
		}

		return (pos < cssLength);
	}

	void MCSSParser::parseDeclaration(StyleRule* sr)
	{
		++pos; // skip '{'
		skipWhiteSpace();
		int index = pos;
		std::wstring commentBuffer;
		while(pos < cssLength)
		{
			// property
			wchar_t byte = css->at(pos);
			if(byte == L'}')
				break;
			else if(byte == L';')
			{
				++pos;
				index = pos;
			}else if(byte == L'/' && css->at(pos + 1) == L'*') // Skip Comment
			{
				commentBuffer.append(*css,index,pos - index);
				skipComment();
				index = pos;
			}else if(byte != L':')
				++pos;
			else
			{
				int posBeforeEndingSpace = pos;
				while(iswspace(css->at(posBeforeEndingSpace - 1)))
					--posBeforeEndingSpace;

				const CSSValuePair* crItem = 0;
				if(commentBuffer.size() != 0)
				{
					commentBuffer.append(*css,index, posBeforeEndingSpace - index);
					crItem = findCSSValue(commentBuffer,properties,knownPropertyCount);
					commentBuffer.clear();
				}else
				{
					std::wstring propName(*css,index,posBeforeEndingSpace - index);
					crItem = findCSSValue(propName,properties,knownPropertyCount);
				}

				if(crItem)
				{
					Declaration* d = new Declaration();
					d->property = static_cast<PropertyType>(crItem->value);

					++pos;
					skipWhiteSpace();

					// value
					int valueStartIndex = pos;
					while(pos < cssLength)
					{
						byte = css->at(pos);
						if(byte == L';' || byte == L'}')
						{
							posBeforeEndingSpace = pos;
							while(iswspace(css->at(posBeforeEndingSpace - 1)))
								--posBeforeEndingSpace;

							if(commentBuffer.size() != 0)
							{
								commentBuffer.append(*css,valueStartIndex,posBeforeEndingSpace - valueStartIndex);
								d->addValue(commentBuffer,0,commentBuffer.length());
								commentBuffer.clear();
							}else
								d->addValue(*css,valueStartIndex,posBeforeEndingSpace - valueStartIndex);

							break;
						}else if(byte == L'/' && css->at(pos + 1) == L'*')
						{
							commentBuffer.append(*css,valueStartIndex,pos - valueStartIndex);
							skipComment();
							valueStartIndex = pos;
						}else
							++pos;
					}
					// If we don't have valid values for the declaration.
					// We delete the declaration.
					if(d->values.empty())
						delete d;
					else
						sr->declarations.push_back(d);
						
				} else {
					while(pos < cssLength) // Unknown Property, Skip to Next.
					{
						++pos;
						byte = css->at(pos);
						if(byte == L'}' || byte == L';')
							break;
					}
				}

				if(byte == L'}')
					break;
				else if(byte == L';')
				{
					++pos;
					skipWhiteSpace();
					index = pos;
				}
			}
		}
		++pos; // skip '}'
	}

	Selector::Selector(const std::wstring* css, int index, int length)
	{
		if(length == -1)
			length = css->size();

		length += index;

		if(index == length)
		{
			BasicSelector* sel = new BasicSelector();
			sel->relationToNext = BasicSelector::NoRelation;
			basicSelectors.push_back(sel);
		}

		while(index < length)
		{
			BasicSelector* sel = new BasicSelector();
			basicSelectors.push_back(sel);
			bool newBS = false;
			std::wstring pseudo;
			std::wstring* buffer = &(sel->elementName);
			while(index < length)
			{
				wchar_t byte = css->at(index);
				if(iswspace(byte))
					newBS = true;
				else if(byte == L'>')
				{
					newBS = true;
					sel->relationToNext = BasicSelector::MatchNextIfParent;
				}else
				{
					if(newBS)
					{
						if(!pseudo.empty())
							sel->addPseudoAndClearInput(pseudo);
						break;
					}

					if(byte == L'#')
					   buffer = &(sel->id);// And skip '#';
					else if(byte == L':')
					{
					   if(!pseudo.empty())
						   sel->addPseudoAndClearInput(pseudo);

					   buffer = &pseudo; // And Skip ':';
					}else if(byte != L'*') // Don't add (*)
						buffer->append(1,byte);
				}
				++index;
			}

			// When we meet ":pressed{" and it ends the loop,
			// we should add the last pseudo into the BasicSelector
			if(!pseudo.empty())
				sel->addPseudoAndClearInput(pseudo);

			if(index == length)
				sel->relationToNext = BasicSelector::NoRelation;

			// TODO: Add more RTTI stuff. Now we can only identify the Class name,
			// not its inherited Class name. So ".ClassA" is the same as "ClassA" .
			if(!sel->elementName.empty() && sel->elementName.at(0) == L'.')
				sel->elementName.erase(0,1);
		}
	}

	void BasicSelector::addPseudoAndClearInput(std::wstring& p)
	{
		const CSSValuePair* crItem = findCSSValue(p,pseudos,knownPseudoCount);
		if(crItem)
		{
			pseudo |= crItem->value;
			++pseudoCount;
		}
		p.clear();
	}

	// Background:      { Brush Image Repeat Clip Alignment pos-x pos-y }*;
	// Border:          none | (Brush LineStyle Lengths)
	// Border-radius:   Lengths
	// Border-image:    none | Url Number{4} (stretch | repeat){0,2}
						// should be used as background & don't have to specify border width
	// Color:           #rrggbb rgba(255,0,0,0) transparent

	// Font:            (FontStyle | FontWeight){0,2} Length String
	// FontStyle:       normal | italic | oblique
	// FontWeight:      normal | bold
	// Text-align:      Alignment;
	// Text-decoration: none | underline | overline | line-through
	// Text-overflow:   clip | ellipsis | wrap
	// Text-Shadow:     h-shadow v-shadow blur Color;
	// Text-underline-style: LineStyle

	// Margin:          Length{1,4}
	// Brush:           Color | Gradient
	// Gradient:
	// Repeat:          repeat | repeat-x | repeat-y | no-repeat
	// Clip/Origin:     padding | border | content | margin
	// Image:           url(filename)
	// Alignment:       (left | top | right | bottom | center){0,2}
	// LineStyle:       dashed | dot-dash | dot-dot-dash | dotted | solid | wave
	// Length:          Number // do not support unit and the default unit is px
	void Declaration::addValue(const std::wstring& css, int index, int length)
	{
		std::wstring buffer;
		CssValue value;
		int maxIndex = index + length - 1;
		int bufferLength = 0;
		while(index <= maxIndex)
		{
			wchar_t byte = css.at(index);
			if(iswspace(byte) || index == maxIndex)
			{
				if(index == maxIndex) {
					++index;
					++bufferLength;
				}

				buffer.assign(css,index - bufferLength,bufferLength);

				if(!buffer.empty())
				{
					if(buffer.at(buffer.size() - 1) == L'x' && buffer.at(buffer.size() - 2) == L'p') // Length
					{
						value.type = CssValue::Length;
						buffer.erase(buffer.size() - 2, 2);
						value.data.vint = _wtoi(buffer.c_str());
						values.push_back(value);
					} else if(iswdigit(buffer.at(0)) || buffer.at(0) == L'-') // Number
					{
						value.type = CssValue::Number;
						value.data.vint = _wtoi(buffer.c_str());
						values.push_back(value);
					} else // Identifier or String
					{
						const CSSValuePair* crItem = findCSSValue(buffer,knownValues,KnownValueCount - 1);
						if(crItem != 0)
						{
							if(crItem->value == Value_Transparent)
							{
								value.type = CssValue::Color;
								value.data.vuint = 0;
							} else {
								value.type = CssValue::Identifier;
								value.data.videntifier = static_cast<ValueType>(crItem->value);
							}
						} else // Treat it as string
						{
							value.type = CssValue::String;
							value.data.vstring = new std::wstring(buffer);
						}
						values.push_back(value);
					}

					buffer.clear();
					bufferLength = 0;
				}
			} else if(byte == L'#') // Hex Color
			{
				++index;
				byte = css.at(index);
				std::wstringstream s;
				s << L"0x";
				while(index <= maxIndex)
				{
					if(iswalpha(byte) || iswdigit(byte))
					{
						s << byte;
						++index;
						byte = css.at(index);
					}else
						break;
				}
				unsigned int hexCol;
				s >> std::hex >> hexCol;
				value.type = CssValue::Color;
				value.data.vuint = hexCol > 0xFFFFFF ? hexCol : (hexCol | 0xFF000000);
				values.push_back(value);

			} else if(byte == L'u' && css.at(index + 1) == L'r' &&
					 css.at(index + 2) == L'l' && css.at(index + 3) == L'(') // url()
			{
				index += 4;
				value.type = CssValue::Uri;
				int startIndex = index;
				while(true)
				{
					if(css.at(index) == L')')
						break;
					++index;
				}
				value.data.vstring = new std::wstring(css,startIndex,index - startIndex);
				values.push_back(value);
				++index;
			} else if(byte == L'r' && css.at(index + 1) == L'g' &&
					 css.at(index + 2) == L'b' &&
					 (css.at(index + 3) == L'(' ||
					  (css.at(index + 3) == L'a' && css.at(index + 4) == L'(')))
			{
				index += (css.at(index + 3) == L'(') ? 4 : 5;

				value.type = CssValue::Color;
				value.data.vuint = 0xFF000000;
				int shift = 16;
				while(true)
				{
					byte = css.at(index);
					if(byte == ',') {
						unsigned int c = _wtoi(buffer.c_str());
						value.data.vuint |= (c << shift);
						shift -= 8;
						buffer.clear();
					} else if(byte == ')')
						break;
					else if(!iswspace(byte))
						buffer.append(1,byte);
				}

				if(shift == -8) {// alpha
					float f;
					std::wstringstream stream;
					stream << buffer;
					stream >> f;
					unsigned int opacity = (int)(f * 255) << 24;
					value.data.vuint |= opacity;
				} else {// blue
					value.data.vuint |= _wtoi(buffer.c_str());
				}

				values.push_back(value);
				buffer.clear();
			} else if(byte == '"')
			{
				++index;
				value.type = CssValue::String;
				int startIndex = index;
				while(true)
				{
					if(css.at(index) == L'"')
						break;
					++index;
				}
				value.data.vstring = new std::wstring(css,startIndex,index);
				values.push_back(value);
				++index;
			} else
			{
				++bufferLength;
// 				buffer.append(1,byte);
			}

			++index;
		}
	}



	struct PortionBrushes
	{
		PortionBrushes() { memset(b,0,sizeof(ID2D1BitmapBrush*) * 5); }
		~PortionBrushes() { for(int i =0; i<5; ++i) SafeRelease(b[i]); }
		ID2D1BitmapBrush* b[5];
	};
	// ********** MStyleSheetStylePrivate
	namespace CSS {
		struct BackgroundRenderObject;
		struct BorderImageRenderObject;
		struct SimpleBorderRenderObject;
		struct ComplexBorderRenderObject;
	}
	using namespace std;
	using namespace std::tr1;
	class MSSSPrivate
	{
		public:
			inline MSSSPrivate();
			~MSSSPrivate();

			typedef multimap<PropertyType,Declaration*> DeclMap;

		private:
			void setAppSS    (const wstring&);
			void setWidgetSS (MWidget*, const wstring&);
			void clearRRCacheRecursively(MWidget*);

			void removeResources();

			inline void polish(MWidget*);
			inline void draw(MWidget*,ID2D1RenderTarget*,const RECT&,const RECT&, const wstring&);
			inline void removeCache(MWidget*);

			typedef unordered_map<MWidget*, StyleSheet*> WidgetSSCache;
			WidgetSSCache widgetSSCache; // Widget specific StyleSheets
			StyleSheet*   appStyleSheet; // Application specific StyleSheets

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
			typedef map<RenderRuleCacheKey, PseudoRenderRuleMap>      RenderRuleMap;
			typedef unordered_map<MWidget*, MatchedStyleRuleVector>   WidgetStyleRuleMap;
			typedef unordered_map<MWidget*, PseudoRenderRuleMap*>     WidgetRenderRuleMap;
			RenderRuleMap       renderRuleCollection;
			WidgetStyleRuleMap  widgetStyleRuleCache;
			WidgetRenderRuleMap widgetRenderRuleCache;

			void getMachedStyleRules(MWidget*, MatchedStyleRuleVector&);
			RenderRule getRenderRule(MWidget*, unsigned int);
			void initRenderRule(RenderRule&, DeclMap&);

			BackgroundRenderObject*  createBackgroundRO (CssValueArray&);
			BorderImageRenderObject* createBorderImageRO(CssValueArray&);
			void setSimpleBorderRO (SimpleBorderRenderObject*,  DeclMap::iterator&, DeclMap::iterator);
			void setComplexBorderRO(ComplexBorderRenderObject*, DeclMap::iterator&, DeclMap::iterator);


			typedef unordered_map<unsigned int, ID2D1SolidColorBrush*> D2D1SolidBrushMap;
			typedef unordered_map<wstring, ID2D1BitmapBrush*>          D2D1BitmapBrushMap;
			typedef unordered_map<ID2D1BitmapBrush*, PortionBrushes>   BorderImagerPortionMap;
			D2D1SolidBrushMap      solidBrushCache;
			D2D1BitmapBrushMap     bitmapBrushCache;
			BorderImagerPortionMap biPortionCache;


			static MSSSPrivate* instance;
			static MStyleSheetStyle::TextRenderer textRenderer;

		friend class MStyleSheetStyle;
		friend struct RenderRuleData;
		friend struct ComplexBorderRenderObject;
	};


	// ========== XXRenderObject ==========
	namespace CSS
	{
		enum ImageRepeat {
			NoRepeat = 0,
			RepeatX  = 1,
			RepeatY  = 2,
			RepeatXY = RepeatX | RepeatY
		};

		template<class T>
		inline bool isRepeatX(const T* t) { return (t->repeat & RepeatX) != 0; }
		template<class T>
		inline bool isRepeatY(const T* t) { return (t->repeat & RepeatY) != 0; }

		// The RenderObjects are created when the RenderRule is acquired,
		// but sometimes, the RenderRule is only used to detemine the properties
		// of a widget, not to draw. The D2D1 resources are only needed when the
		// draw action performs.
		// So, every kind of RenderObject must follow the same constraint.
		// That is, if they use D2D1 resources, they must use the RIIA idiom.
		// The resources are created when they are first needed.
		// Its their responsibility to free the resoures when necessary.

		struct GeometryRenderObject
		{
			int width   , height;
			int minWidth, minHeight;
			int maxWidth, maxHeight;

			inline GeometryRenderObject() : width(-1), height(-1), minWidth(-1),
				minHeight(-1), maxWidth(-1), maxHeight(-1) {}
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

			MColor color;
			ValueType alignX;
			ValueType alignY;
			ValueType decoration; // none | underline | overline | line-through
			ValueType lineStyle;
			ValueType overflow; // clip | ellipsis | wrap

			char outlineWidth;
			char outlineBlur;
			MColor outlineColor;

			char shadowOffsetX;
			char shadowOffsetY;
			char shadowBlur;
			MColor shadowColor;

			MFont font;
		};
		inline TextRenderObject::TextRenderObject(const MFont& f):
			alignX(Value_Left), alignY(Value_Center), decoration(Value_None),
			lineStyle(Value_Solid),outlineWidth(0),outlineBlur(0),
			shadowOffsetX(0),shadowOffsetY(0),shadowBlur(0),font(f){}



		// Created by MSSSPrivate::createBackgroundRO()
		struct BackgroundRenderObject
		{
			inline BackgroundRenderObject(const CssValue&);
			~BackgroundRenderObject(){}

			unsigned int x, y, width, height;
			ValueType    clip;
			ValueType    alignX;
			ValueType    alignY;
			ImageRepeat  repeat;

			bool checkedSize;

			CssValue     brushCSSValue;
			ID2D1Brush** brushPosition;
		};
		inline BackgroundRenderObject::BackgroundRenderObject(const CssValue& v):
				x(0),y(0),width(0),height(0),clip(Value_Margin),
				alignX(Value_Left),alignY(Value_Top),
				repeat(NoRepeat),checkedSize(false),
				brushCSSValue(v),brushPosition(0){}



		// I have removed some interfaces of BorderRenderObject,
		// because some only works for Simple~
		// some only works for Complex~
		// Maybe the BorderRenderObject is a bad design.
		// But for now, I don't have another idea.
		struct BorderRenderObject
		{
			enum BROType { SimpleBorder, RadiusBorder, ComplexBorder };

			BROType type;
			virtual ~BorderRenderObject() {}
			virtual void getBorderWidth(RECT&) const = 0;
			virtual bool isVisible() const = 0;
		};
		// Set by MSSSPrivate::setSimpleBorderRO()
		struct SimpleBorderRenderObject : public BorderRenderObject
		{
			inline SimpleBorderRenderObject();

			unsigned int width;
			unsigned int color;
			ValueType    style;
			ID2D1SolidColorBrush**  brushPosition;

			void getBorderWidth(RECT&) const;
			bool isVisible() const;
			int  getWidth()  const;
		};
		inline SimpleBorderRenderObject::SimpleBorderRenderObject():
			width(0),style(Value_Solid),brushPosition(0) { type = SimpleBorder; }
		void   SimpleBorderRenderObject::getBorderWidth(RECT& rect) const
			{ rect.left = rect.right = rect.bottom = rect.top = width; }
		int    SimpleBorderRenderObject::getWidth() const
			{ return width; }
		bool   SimpleBorderRenderObject::isVisible() const
			{ return width != 0 && color != 0; }
		// Set by MSSSPrivate::setSimpleBorderRO()
		struct RadiusBorderRenderObject : public SimpleBorderRenderObject
		{
			inline RadiusBorderRenderObject();
			int radius;
		};
		inline RadiusBorderRenderObject::RadiusBorderRenderObject():radius(0){ type = RadiusBorder; }
		// Set by MSSSPrivate::setComplexBorderRO()
		struct ComplexBorderRenderObject : public BorderRenderObject
		{
			ComplexBorderRenderObject();

			bool uniform;
			D2D1_RECT_U styles;
			D2D1_RECT_U widths;
			unsigned int radiuses[4]; // TL, TR, BL, BR
			unsigned int colors[4]; // T, R, B, L
			ID2D1SolidColorBrush** brushPositions[4]; // T, R, B, L

			void getBorderWidth(RECT&) const;
			bool isVisible() const;

			void checkUniform();

			ID2D1Geometry* createGeometry(const D2D1_RECT_F&);
			void draw(ID2D1RenderTarget*,ID2D1Geometry*,const D2D1_RECT_F&);

			void setColors(MSSSPrivate*,const CssValueArray&,size_t startIndex,size_t endIndexPlus);
		};
		ComplexBorderRenderObject::ComplexBorderRenderObject():uniform(false)
		{
			type = ComplexBorder;
			styles.left = styles.top = styles.right = styles.bottom = Value_Solid;
			memset(&widths  , 0, sizeof(RECT));
			memset(&radiuses, 0, 4 * sizeof(int));
			memset(brushPositions, 0, 4 * sizeof(ID2D1SolidColorBrush**));
			memset(colors, 0, 4 * sizeof(unsigned int));
		}
		void ComplexBorderRenderObject::setColors(MSSSPrivate* sss, const CssValueArray& values,size_t start,size_t end)
		{
			int size = end - start;
			if(size  == 4) {
				colors[0] = values.at(start  ).data.vuint;
				colors[1] = values.at(start+1).data.vuint;
				colors[2] = values.at(start+2).data.vuint;
				colors[3] = values.at(start+3).data.vuint;
				ID2D1SolidColorBrush*& brush0 = MSSSPrivate::instance->solidBrushCache[colors[0]];
				ID2D1SolidColorBrush*& brush1 = MSSSPrivate::instance->solidBrushCache[colors[1]];
				ID2D1SolidColorBrush*& brush2 = MSSSPrivate::instance->solidBrushCache[colors[2]];
				ID2D1SolidColorBrush*& brush3 = MSSSPrivate::instance->solidBrushCache[colors[3]];
				brushPositions[0] = &brush0;
				brushPositions[1] = &brush1;
				brushPositions[2] = &brush2;
				brushPositions[3] = &brush3;
			} else if(values.size() == 2) {
				colors[0] = colors[2] = values.at(start  ).data.vuint;
				colors[1] = colors[3] = values.at(start+1).data.vuint;
				ID2D1SolidColorBrush*& brush = MSSSPrivate::instance->solidBrushCache[colors[0]];
				brushPositions[0] = brushPositions[2] = &brush;
				ID2D1SolidColorBrush*& brush2 = MSSSPrivate::instance->solidBrushCache[colors[1]];
				brushPositions[1] = brushPositions[3] = &brush2;
			} else {
				colors[0] = colors[1] =
				colors[2] = colors[3] = values.at(start).data.vuint; 
				ID2D1SolidColorBrush*& brush = MSSSPrivate::instance->solidBrushCache[colors[0]];
				brushPositions[0] = brushPositions[1] = 
				brushPositions[2] = brushPositions[3] = &brush;
			}
		}
		void ComplexBorderRenderObject::checkUniform()
		{
			if(widths.left == widths.right && widths.right == widths.top && widths.top == widths.bottom &&
				colors[0] == colors[1] &&  colors[1] == colors[2] &&  colors[2] == colors[3])
				uniform = true;
		}
		void ComplexBorderRenderObject::getBorderWidth(RECT& rect) const
		{
			rect.left   = widths.left;
			rect.right  = widths.right;
			rect.bottom = widths.bottom;
			rect.top    = widths.top;
		}
		bool ComplexBorderRenderObject::isVisible() const
		{
			if(colors[0] == 0 && colors[1] == 0 && colors[2] == 0 && colors[3] == 0)
				return false;
			if(widths.left == 0 && widths.right == 0 && widths.top == 0 && widths.bottom == 0)
				return false;
			return true;
		}
		ID2D1Geometry* ComplexBorderRenderObject::createGeometry(const D2D1_RECT_F& rect)
		{
			ID2D1GeometrySink* sink;
			ID2D1PathGeometry* pathGeo;

			FLOAT ft = rect.top    + widths.top    / 2;
			FLOAT fr = rect.right  + widths.right  / 2;
			FLOAT fl = rect.left   + widths.left   / 2;
			FLOAT fb = rect.bottom + widths.bottom / 2;

			mApp->getD2D1Factory()->CreatePathGeometry(&pathGeo);
			pathGeo->Open(&sink);
			sink->BeginFigure(D2D1::Point2F(rect.left + radiuses[0],ft),D2D1_FIGURE_BEGIN_FILLED);

			sink->AddLine(D2D1::Point2F(rect.right - radiuses[1], ft));
			if(radiuses[1] != 0)
				sink->AddArc(D2D1::ArcSegment(
					D2D1::Point2F(fr, ft + radiuses[1]),
					D2D1::SizeF((FLOAT)radiuses[1],(FLOAT)radiuses[1]),
					0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
			sink->AddLine(D2D1::Point2F(fr, fb - radiuses[2]));
			if(radiuses[2] != 0)
				sink->AddArc(D2D1::ArcSegment(
					D2D1::Point2F(fr - radiuses[2], fb),
					D2D1::SizeF((FLOAT)radiuses[2],(FLOAT)radiuses[2]),
					0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
			sink->AddLine(D2D1::Point2F(fl + radiuses[3], fb));
			if(radiuses[3] != 0)
				sink->AddArc(D2D1::ArcSegment(
					D2D1::Point2F(fl, fb - radiuses[3]),
					D2D1::SizeF((FLOAT)radiuses[3],(FLOAT)radiuses[3]),
					0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
			sink->AddLine(D2D1::Point2F(fl, ft + radiuses[0]));
			if(radiuses[0] != 0)
				sink->AddArc(D2D1::ArcSegment(
					D2D1::Point2F(fl + radiuses[0], ft),
					D2D1::SizeF((FLOAT)radiuses[0],(FLOAT)radiuses[0]),
					0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));

			sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			sink->Close();
			SafeRelease(sink);
			return pathGeo; 
		}
		void ComplexBorderRenderObject::draw(ID2D1RenderTarget* rt,ID2D1Geometry* geo,const D2D1_RECT_F& rect)
		{
			if(uniform) {
				rt->DrawGeometry(geo,*(brushPositions[0]),(FLOAT)widths.left);
				return;
			}

			FLOAT fl = rect.left   + widths.left   / 2;
			FLOAT ft = rect.top    + widths.top    / 2;
			FLOAT fr = rect.right  + widths.right  / 2;
			FLOAT fb = rect.bottom + widths.bottom / 2;

			if(widths.top > 0)
			{
				FLOAT offset = (FLOAT)widths.top / 2;
				if(radiuses[0] != 0 || radiuses[1] != 0)
				{
					ID2D1PathGeometry* pathGeo;
					ID2D1GeometrySink* sink;
					mApp->getD2D1Factory()->CreatePathGeometry(&pathGeo);
					pathGeo->Open(&sink);

					if(radiuses[0] != 0) {
						sink->BeginFigure(D2D1::Point2F(fl, ft + radiuses[0]),D2D1_FIGURE_BEGIN_FILLED);
						sink->AddArc(D2D1::ArcSegment(
							D2D1::Point2F(rect.left + radiuses[0] + offset, ft),
							D2D1::SizeF((FLOAT)radiuses[0],(FLOAT)radiuses[0]),
							0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
					} else {
						sink->BeginFigure(D2D1::Point2F(rect.left,ft),D2D1_FIGURE_BEGIN_FILLED);
					}

					if(radiuses[1] != 0) {
						sink->AddLine(D2D1::Point2F(fr - radiuses[1], ft));
						sink->AddArc(D2D1::ArcSegment(
							D2D1::Point2F(rect.right - offset, ft + radiuses[1]),
							D2D1::SizeF((FLOAT)radiuses[1],(FLOAT)radiuses[1]),
							0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
					} else {
						sink->AddLine(D2D1::Point2F(rect.right,ft));
					}
					sink->EndFigure(D2D1_FIGURE_END_OPEN);
					sink->Close();
					rt->DrawGeometry(pathGeo,*(brushPositions[0]),(FLOAT)widths.top);
					SafeRelease(sink);
					SafeRelease(pathGeo);
				} else {
					rt->DrawLine(D2D1::Point2F(rect.left,ft), D2D1::Point2F(rect.right,ft),
							*(brushPositions[0]),(FLOAT)widths.top);
				}
			}

			if(widths.right > 0) {
				rt->DrawLine(D2D1::Point2F(fr, ft + radiuses[1]), D2D1::Point2F(fr,fb - radiuses[2]),
					*(brushPositions[1]),(FLOAT)widths.right);
			}

			if(widths.bottom > 0)
			{
				FLOAT offset = (FLOAT)widths.bottom / 2;
				if(radiuses[2] != 0 || radiuses[3] != 0)
				{
					ID2D1PathGeometry* pathGeo;
					ID2D1GeometrySink* sink;
					mApp->getD2D1Factory()->CreatePathGeometry(&pathGeo);
					pathGeo->Open(&sink);
					if(radiuses[2] != 0) {
						sink->BeginFigure(D2D1::Point2F(fr, fb - radiuses[2]),D2D1_FIGURE_BEGIN_FILLED);
						sink->AddArc(D2D1::ArcSegment(
							D2D1::Point2F(rect.right - radiuses[2] - offset,fb),
							D2D1::SizeF((FLOAT)radiuses[2],(FLOAT)radiuses[2]),
							0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
					} else {
						sink->BeginFigure(D2D1::Point2F(rect.right,fb),D2D1_FIGURE_BEGIN_FILLED);
					}

					if(radiuses[3] != 0) {
						sink->AddLine(D2D1::Point2F(fl + radiuses[3], fb));
						sink->AddArc(D2D1::ArcSegment(
							D2D1::Point2F(rect.left + offset, fb - radiuses[3]),
							D2D1::SizeF((FLOAT)radiuses[3],(FLOAT)radiuses[3]),
							0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
					} else {
						sink->AddLine(D2D1::Point2F(rect.left, fb));
					}
					sink->EndFigure(D2D1_FIGURE_END_OPEN);
					sink->Close();
					rt->DrawGeometry(pathGeo,*(brushPositions[3]),(FLOAT)widths.bottom);
					SafeRelease(sink);
					SafeRelease(pathGeo);
				} else {
					rt->DrawLine(D2D1::Point2F(rect.right,fb), D2D1::Point2F(rect.left,fb),
						*(brushPositions[3]),(FLOAT)widths.bottom);
				}
			}

			if(widths.left > 0) {
				rt->DrawLine(D2D1::Point2F(fl, fb-radiuses[3]), D2D1::Point2F(fl,ft + radiuses[0]),
					*(brushPositions[3]),(FLOAT)widths.left);
			}
		}


		enum Portion
		{
			TopCenter = 0,
			CenterLeft,
			Center,
			CenterRight,
			BottomCenter,
			Conner
		};
		struct BorderImageRenderObject
		{
			BorderImageRenderObject();
			unsigned int topW, rightW, bottomW, leftW;
			ImageRepeat  repeat;
			wstring*     imageUri;
			ID2D1BitmapBrush** brushPosition;
		};
		BorderImageRenderObject::BorderImageRenderObject():topW(0),rightW(0),
			bottomW(0),leftW(0), repeat(NoRepeat),imageUri(0),brushPosition(0){}






		// ========== RenderRuleData ==========
		struct RenderRuleData
		{
			inline RenderRuleData();
			~RenderRuleData();

			int  refCount;

			bool opaqueBackground;
			bool animated; // Remark: We need to know whether the background is GIF animated image.
			bool hasMargin;
			bool hasPadding;

			vector<BackgroundRenderObject*> backgroundROs;
			BorderImageRenderObject*        borderImageRO;
			BorderRenderObject*             borderRO;
			GeometryRenderObject*           geoRO;
			TextRenderObject*               textRO;
			MCursor*                        cursor;

			D2D_RECT_U margin;
			D2D_RECT_U padding;

			void createBrush(const CssValue&);
			void createBrush( unsigned int);
			ID2D1BitmapBrush* createBrush(const wstring&);
			void createBorderImagerBrush();

			inline ID2D1SolidColorBrush* getBrush(SimpleBorderRenderObject*);
			inline ID2D1Brush* getBrush(BackgroundRenderObject*);
			ID2D1BitmapBrush* getBorderImageBrush(Portion);

			// Return true if we changed the widget's size
			bool setGeometry    (MWidget*);
			void draw           (ID2D1RenderTarget*,const RECT& widgetRectInRT, const RECT& clipRectInRT, const wstring& text);
			void drawBackgrounds(const RECT& widgetRectInRT, const RECT& clipRectInRT, ID2D1Geometry*);
			void drawBorderImage(const RECT& widgetRectInRT, const RECT& clipRectInRT);

			// The rect don't have to const, because we always draw text in the last place.
			void drawGdiText    (const RECT& widgetRectInRT, const D2D1_RECT_F& borderRectInRT,
							const RECT& clipRectInRT, const wstring& text);

			// Since we only deal with GUI in the main thread,
			// we can safely store the renderTarget in a static member for later use.
			static ID2D1RenderTarget* workingRT;
		};
	} // namespace CSS



	// ********** RenderRuleData Impl
	ID2D1RenderTarget* RenderRuleData::workingRT = 0;
	inline RenderRuleData::RenderRuleData():
		refCount(1), opaqueBackground(false), animated(false),
		hasMargin(false), hasPadding(false), borderImageRO(0),
		borderRO(0), geoRO(0), textRO(0), cursor(0)
	{
		memset(&margin, 0,sizeof(D2D_RECT_U));
		memset(&padding,0,sizeof(D2D_RECT_U));
	}
	RenderRuleData::~RenderRuleData()
	{
		for(int i = backgroundROs.size() -1; i >=0; --i)
			delete backgroundROs.at(i);
		delete borderImageRO;
		delete borderRO;
		delete geoRO;
		delete cursor;
		delete textRO;
	}
	bool RenderRuleData::setGeometry(MWidget* w)
	{
		if(geoRO == 0)
			return false;

		D2D_SIZE_U size  = w->size();
		D2D_SIZE_U size2 = size;
		if(geoRO->width  != -1 && geoRO->width  != size2.width)  size2.width   = geoRO->width;
		if(geoRO->height != -1 && geoRO->height != size2.height) size2.height  = geoRO->height;

		D2D_SIZE_U range = w->minSize();
		bool changed     = false;
		if(geoRO->minWidth  != -1) { range.width  = geoRO->minWidth;  changed = true; }
		if(geoRO->minHeight != -1) { range.height = geoRO->minHeight; changed = true; }
		if(changed) w->setMinimumSize(range.width,range.height);

		if(size2.width  < range.width)  size2.width  = range.width;
		if(size2.height < range.height) size2.height = range.height;

		range   = w->maxSize();
		changed = false;
		if(geoRO->maxWidth  != -1) { range.width  = geoRO->maxWidth;  changed = true; }
		if(geoRO->maxHeight != -1) { range.height = geoRO->maxHeight; changed = true; }
		if(changed) w->setMaximumSize(range.width,range.height);

		if(size2.width  > range.width)  size2.width  = range.width;
		if(size2.height > range.height) size2.height = range.height;

		if(size2.width == size.width && size2.height == size.height)
			return false;

		w->resize(size2.width,size2.height);
		return true;
	}

	void RenderRuleData::createBrush(const CssValue& v)
	{
		if(v.type == CssValue::Uri)
			createBrush(*v.data.vstring);
		else
			createBrush(v.data.vuint);
	}
	void RenderRuleData::createBrush(unsigned int color)
	{
		ID2D1SolidColorBrush* brush = MSSSPrivate::instance->solidBrushCache[color];
		if(brush == 0) {
			workingRT->CreateSolidColorBrush(MColor(color),&brush);
			MSSSPrivate::instance->solidBrushCache[color] = brush;
		}
	}
	ID2D1BitmapBrush* RenderRuleData::createBrush(const wstring& uri)
	{
		ID2D1BitmapBrush* brush = MSSSPrivate::instance->bitmapBrushCache[uri];
		M_ASSERT(brush == 0);

		IWICImagingFactory*    wicFactory = mApp->getWICImagingFactory();
		IWICBitmapDecoder*     decoder	  = 0;
		IWICBitmapFrameDecode* frame	  = 0;
		IWICFormatConverter*   converter  = 0;
		IWICStream*            stream     = 0;
		ID2D1Bitmap*           bitmap     = 0;

		bool hasError = false;
		HRESULT hr;
		if(uri.at(0) == L':') {
			// Image file is inside MResources.
			MResource res;
			if(res.open(uri))
			{
				wicFactory->CreateStream(&stream);
				stream->InitializeFromMemory((WICInProcPointer)res.byteBuffer(),res.length());
				wicFactory->CreateDecoderFromStream(stream,NULL,WICDecodeMetadataCacheOnDemand,&decoder);
			}else
				hasError = true;
		} else {
			hr = wicFactory->CreateDecoderFromFilename(uri.c_str(),NULL,
				GENERIC_READ, WICDecodeMetadataCacheOnDemand,&decoder);
			hasError = FAILED(hr);
		}

		if(hasError)
		{
			std::wstring error = L"[MStyleSheetStyle] Cannot open image file: ";
			error.append(uri);
			error.append(1,L'\n');
			mWarning(true,error.c_str());
			// create a empty bitmap because we can't find the image.
			hr = workingRT->CreateBitmap( D2D1::SizeU(),
				D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
				D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
		} else {
			decoder->GetFrame(0,&frame);
			wicFactory->CreateFormatConverter(&converter);
			converter->Initialize(frame,
				GUID_WICPixelFormat32bppPBGRA,
				WICBitmapDitherTypeNone,NULL,
				0.f,WICBitmapPaletteTypeMedianCut);
			workingRT->CreateBitmapFromWicBitmap(converter,
				D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, 
				D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
		}

		workingRT->CreateBitmapBrush(bitmap,
			D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP,D2D1_EXTEND_MODE_WRAP),
			D2D1::BrushProperties(), &brush);

		MSSSPrivate::instance->bitmapBrushCache[uri] = brush;

		SafeRelease(decoder);
		SafeRelease(frame);
		SafeRelease(stream);
		SafeRelease(converter);
		SafeRelease(bitmap);

		return brush;
	}
	void RenderRuleData::createBorderImagerBrush()
	{
		wstring& uri = *borderImageRO->imageUri;
		ID2D1BitmapBrush* brush = brush = createBrush(uri);
		// Create Portion.
		PortionBrushes& pbs = MSSSPrivate::instance->biPortionCache[brush];
		ID2D1Bitmap* bitmap;
		brush->GetBitmap(&bitmap);

		D2D1_SIZE_U imageSize = bitmap->GetPixelSize();

		const unsigned int& leftWidth   = borderImageRO->leftW;
		const unsigned int& rightWidth  = borderImageRO->rightW;
		const unsigned int& topWidth    = borderImageRO->topW;
		const unsigned int& bottomWidth = borderImageRO->bottomW;

		D2D1_RECT_F pbmpRect;
		for(int i = 0; i < 5; ++i)
		{
			D2D1_SIZE_U pbmpSize;
			switch(i)
			{
				case TopCenter:
					pbmpRect.left   = (FLOAT)leftWidth;
					pbmpRect.right  = (FLOAT)imageSize.width - rightWidth;
					pbmpRect.top    = 0.f; 
					pbmpRect.bottom = (FLOAT)topWidth;
					pbmpSize.width  = int(pbmpRect.right - leftWidth);
					pbmpSize.height = topWidth;
					break;
				case CenterLeft:
					pbmpRect.left   = 0.f;
					pbmpRect.right  = (FLOAT)leftWidth;
					pbmpRect.top    = (FLOAT)topWidth;
					pbmpRect.bottom = (FLOAT)imageSize.height - bottomWidth;
					pbmpSize.width  = leftWidth;
					pbmpSize.height = int(pbmpRect.bottom - topWidth);
					break;
				case Center:
					pbmpRect.left   = (FLOAT)leftWidth;
					pbmpRect.right  = (FLOAT)imageSize.width  - rightWidth;
					pbmpRect.top    = (FLOAT)topWidth;
					pbmpRect.bottom = (FLOAT)imageSize.height - bottomWidth;
					pbmpSize.width  = int(pbmpRect.right  - leftWidth);
					pbmpSize.height = int(pbmpRect.bottom - topWidth);
					break;
				case CenterRight:
					pbmpRect.left   = (FLOAT)imageSize.width  - rightWidth;
					pbmpRect.right  = (FLOAT)imageSize.width;
					pbmpRect.top    = (FLOAT)topWidth;
					pbmpRect.bottom = (FLOAT)imageSize.height - bottomWidth;
					pbmpSize.width  = rightWidth;
					pbmpSize.height = int(pbmpRect.bottom - topWidth);
					break;
				case BottomCenter:
					pbmpRect.left   = (FLOAT)leftWidth;
					pbmpRect.right  = (FLOAT)imageSize.width  - rightWidth;
					pbmpRect.top    = (FLOAT)imageSize.height - bottomWidth;
					pbmpRect.bottom = (FLOAT)imageSize.height;
					pbmpSize.width  = int(pbmpRect.right  - leftWidth);
					pbmpSize.height = bottomWidth;
					break;
			}

			// There is a bug in ID2D1Bitmap::CopyFromBitmap(), we need to avoid this.
			ID2D1Bitmap* portionBmp;
			ID2D1BitmapBrush* pb;

			ID2D1BitmapRenderTarget* intermediateRT;
			HRESULT hr = workingRT->CreateCompatibleRenderTarget(D2D1::SizeF((FLOAT)pbmpSize.width,
				(FLOAT)pbmpSize.height),&intermediateRT);

			if(FAILED(hr)) break;

			intermediateRT->BeginDraw();
			intermediateRT->DrawBitmap(bitmap,D2D1::RectF(0.f,0.f,(FLOAT)pbmpSize.width,(FLOAT)pbmpSize.height),
				1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, pbmpRect);
			intermediateRT->EndDraw();
			intermediateRT->GetBitmap(&portionBmp);
			workingRT->CreateBitmapBrush(portionBmp,
				D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP,D2D1_EXTEND_MODE_CLAMP),
				D2D1::BrushProperties(), &pb);

			SafeRelease(portionBmp);
			SafeRelease(intermediateRT);
			pbs.b[i] = pb;
		}

		SafeRelease(bitmap);
	}
	inline ID2D1SolidColorBrush* RenderRuleData::getBrush(SimpleBorderRenderObject* sbro)
	{
		if(*(sbro->brushPosition) == 0)
			createBrush(sbro->color);
		return *(sbro->brushPosition);
	}
	inline ID2D1Brush* RenderRuleData::getBrush(BackgroundRenderObject* bgro)
	{
		ID2D1Brush** bp = bgro->brushPosition;
		if(*bp == 0)
			createBrush(bgro->brushCSSValue);

		if(!bgro->checkedSize)
		{
			bgro->checkedSize = true;
			if(bgro->brushCSSValue.type == CssValue::Uri) {
				// A bitmap brush
				ID2D1BitmapBrush* bb = (ID2D1BitmapBrush*) *bp;
				ID2D1Bitmap* bmp;
				bb->GetBitmap(&bmp);
				D2D1_SIZE_F size = bmp->GetSize();
				unsigned int& w = bgro->width;
				unsigned int& h = bgro->height;
				// If the user doesn't specify the size of the background,
				// we need to make sure the size is valid.
				if(w == 0 || w + bgro->x > (unsigned int)size.width )
					w = (unsigned int)size.width  - bgro->x;
				if(h == 0 || h + bgro->y > (unsigned int)size.height)
					h = (unsigned int)size.height - bgro->y;
			}
		}
		return *bp;
	}
	ID2D1BitmapBrush* RenderRuleData::getBorderImageBrush(Portion p)
	{
		M_ASSERT(borderImageRO);
		if(*(borderImageRO->brushPosition) == 0)
			createBorderImagerBrush();

		ID2D1BitmapBrush* b = *(borderImageRO->brushPosition);
		if(Conner == p)
			return b;
		else {
			PortionBrushes& pbs = MSSSPrivate::instance->biPortionCache[b];
			return pbs.b[p];
		}
	} 
	void RenderRuleData::draw(ID2D1RenderTarget* rt, const RECT& widgetRectInRT, const RECT& clipRectInRT, const wstring& text)
	{
		M_ASSERT(rt != 0);
		workingRT = rt;
		// We have to get the border geometry first if there's any.
		// Because drawing the background may depends on the border geometry.
		ID2D1Geometry* borderGeo = 0;
		D2D1_RECT_F borderRect = {(FLOAT)widgetRectInRT.left, (FLOAT)widgetRectInRT.top,
								(FLOAT)widgetRectInRT.right, (FLOAT)widgetRectInRT.bottom};
		if(borderRO != 0) {
			if(hasMargin) {
				borderRect.left   += (FLOAT)margin.left;
				borderRect.right  -= (FLOAT)margin.right;
				borderRect.top    += (FLOAT)margin.top;
				borderRect.bottom -= (FLOAT)margin.bottom;
			}

			if(borderRO->type == BorderRenderObject::ComplexBorder)
				borderGeo = reinterpret_cast<ComplexBorderRenderObject*>(borderRO)->createGeometry(borderRect);
		}
		// === Backgrounds ===
		if(backgroundROs.size() > 0)
			drawBackgrounds(widgetRectInRT, clipRectInRT,borderGeo);

		// === BorderImage ===
		if(borderImageRO != 0)
			drawBorderImage(widgetRectInRT, clipRectInRT);

		// === Border ===
		if(borderRO != 0 && borderRO->isVisible())
		{
			if(borderRO->type == BorderRenderObject::ComplexBorder)
			{
				// Check the ComplexBorderRO's brushes here, so that it don't depend on
				// RenderRuleData.
				ComplexBorderRenderObject* cbro = reinterpret_cast<ComplexBorderRenderObject*>(borderRO);
				if(*(cbro->brushPositions[0]) == 0) createBrush(cbro->colors[0]);
				if(*(cbro->brushPositions[1]) == 0) createBrush(cbro->colors[1]);
				if(*(cbro->brushPositions[2]) == 0) createBrush(cbro->colors[2]);
				if(*(cbro->brushPositions[3]) == 0) createBrush(cbro->colors[3]);
				cbro->draw(rt, borderGeo, borderRect);
			} else {
				SimpleBorderRenderObject* sbro = reinterpret_cast<SimpleBorderRenderObject*>(borderRO);
				ID2D1SolidColorBrush* b = getBrush(sbro);
				FLOAT w = (FLOAT)sbro->width;
				// MS said that the rectangle stroke is centered on the outline. So we
				// need to shrink a half of the borderWidth.
				FLOAT shrink = w / 2;
				borderRect.left   += shrink;
				borderRect.top    += shrink;
				borderRect.right  -= shrink;
				borderRect.bottom -= shrink;
				if(borderRO->type == BorderRenderObject::SimpleBorder)
					rt->DrawRectangle(borderRect,b,w);
				else {
					RadiusBorderRenderObject* rbro = reinterpret_cast<RadiusBorderRenderObject*>(borderRO);
					D2D1_ROUNDED_RECT rr;
					rr.rect = borderRect;
					rr.radiusX = rr.radiusY = (FLOAT)rbro->radius;
					rt->DrawRoundedRectangle(rr,b,w);
				}
			}
		}

		// === Text ===
		if(!text.empty())
			drawGdiText(widgetRectInRT,borderRect,clipRectInRT,text);
		
		SafeRelease(borderGeo);
		workingRT = 0;
	}

	void RenderRuleData::drawBackgrounds(const RECT& wr, const RECT& cr,ID2D1Geometry* borderGeo)
	{
		for(unsigned int i = 0; i < backgroundROs.size(); ++i)
		{
			RECT contentRect = wr;

			ID2D1Geometry*          bgGeo = 0;
			BackgroundRenderObject* bgro  = backgroundROs.at(i);

			if(bgro->clip != Value_Margin)
			{
				if(hasMargin) {
					contentRect.left   += margin.left;
					contentRect.top    += margin.top;
					contentRect.right  -= margin.right;
					contentRect.bottom -= margin.bottom;
				}

				if(bgro->clip != Value_Border) {
					if(borderRO != 0) {
						RECT borderWidth;
						borderRO->getBorderWidth(borderWidth);
						contentRect.left   += borderWidth.left;
						contentRect.top    += borderWidth.top;
						contentRect.right  -= borderWidth.right;
						contentRect.bottom -= borderWidth.bottom;
					}
					if(hasPadding) {
						contentRect.left   += padding.left;
						contentRect.top    += padding.top;
						contentRect.right  -= padding.right;
						contentRect.bottom -= padding.bottom;
					}
				}
			}

			int dx = 0 - bgro->x;
			int dy = 0 - bgro->y;
			ID2D1Brush* brush = getBrush(bgro);

			if(bgro->width != 0)
			{
				if(bgro->alignX != Value_Left) {
					if(bgro->alignX == Value_Center)
						contentRect.left = (contentRect.left + contentRect.right - bgro->width) / 2;
					else
						contentRect.left = contentRect.right - bgro->width;
				}
				if(!isRepeatX(bgro)) {
					int r = contentRect.left + bgro->width;
					if(contentRect.right > r)
						contentRect.right  = r;
				}
			}
			if(bgro->height != 0)
			{
				if(bgro->alignY != Value_Top) {
					if(bgro->alignY == Value_Center)
						contentRect.top = (contentRect.top + contentRect.bottom - bgro->height) / 2;
					else
						contentRect.top = contentRect.bottom - bgro->height;
				}
				if(!isRepeatY(bgro)) {
					int b = contentRect.top + bgro->height;
					if(contentRect.bottom > b)
						contentRect.bottom = b;
				}
			}

			dx += (int)contentRect.left;
			dy += (int)contentRect.top;
			if(dx != 0 || dy != 0)
				brush->SetTransform(D2D1::Matrix3x2F::Translation((FLOAT)dx,(FLOAT)dy));

			D2D1_RECT_F clip;
			clip.left   = (FLOAT) max(contentRect.left  ,cr.left);
			clip.top    = (FLOAT) max(contentRect.top   ,cr.top);
			clip.right  = (FLOAT) min(contentRect.right ,cr.right);
			clip.bottom = (FLOAT) min(contentRect.bottom,cr.bottom);

			if(bgro->clip != Value_Border || borderRO == 0)
				workingRT->FillRectangle(clip,brush);
			else
			{
				if(borderRO->type == BorderRenderObject::SimpleBorder) {
					workingRT->FillRectangle(clip,brush);
				} else if(borderRO->type == BorderRenderObject::RadiusBorder) {
					D2D1_ROUNDED_RECT rrect;
					rrect.rect = clip;
					rrect.radiusX = rrect.radiusY = FLOAT(reinterpret_cast<RadiusBorderRenderObject*>(borderRO)->radius);
					workingRT->FillRoundedRectangle(rrect,brush);
				} else {
					workingRT->FillGeometry(borderGeo,brush);
				}
			}
		}
	}

	void RenderRuleData::drawBorderImage(const RECT& widgetRect, const RECT& clipRect)
	{
		// 0.TopLeft     1.TopCenter     2.TopRight
		// 3.CenterLeft  4.Center        5.CenterRight
		// 6.BottomLeft  7.BottomCenter  8.BottomRight
		bool drawParts[9] = {true,true,true,true,true,true,true,true,true};
		int x1 = widgetRect.left   + borderImageRO->leftW;
		int x2 = widgetRect.right  - borderImageRO->rightW;
		int y1 = widgetRect.top    + borderImageRO->topW;
		int y2 = widgetRect.bottom - borderImageRO->bottomW;

		ID2D1BitmapBrush* brush = getBorderImageBrush(Conner);
		ID2D1Bitmap* bitmap;
		brush->GetBitmap(&bitmap);
		D2D1_SIZE_U imageSize = bitmap->GetPixelSize();
		bitmap->Release();

		FLOAT scaleX = FLOAT(x2 - x1) / (imageSize.width - borderImageRO->leftW- borderImageRO->rightW);
		FLOAT scaleY = FLOAT(y2 - y1) / (imageSize.height - borderImageRO->topW- borderImageRO->bottomW);
		if(clipRect.left > x1) {
			drawParts[0] = drawParts[3] = drawParts[6] = false;
			if(clipRect.left > x2)
				drawParts[1] = drawParts[4] = drawParts[7] = false;
		}
		if(clipRect.top > y1) {
			drawParts[0] = drawParts[1] = drawParts[2] = false;
			if(clipRect.top > y2)
				drawParts[3] = drawParts[4] = drawParts[5] = false;
		}
		if(clipRect.right <= x2) {
			drawParts[2] = drawParts[5] = drawParts[8] = false;
			if(clipRect.right <= x1)
				drawParts[1] = drawParts[4] = drawParts[7] = false;
		}
		if(clipRect.bottom <= y2) {
			drawParts[6] = drawParts[7] = drawParts[8] = false;
			if(clipRect.bottom <= y1)
				drawParts[3] = drawParts[4] = drawParts[5] = false;
		}

		// Draw!
		D2D1_RECT_F drawRect;
		// Assume we can always get a valid brush for Conner.
		if(drawParts[0]) {
			drawRect.left   = (FLOAT)widgetRect.left;
			drawRect.right  = (FLOAT)x1;
			drawRect.top    = (FLOAT)widgetRect.top;
			drawRect.bottom = (FLOAT)y1;
			brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
			workingRT->FillRectangle(drawRect,brush);
		}
		if(drawParts[2]) {
			drawRect.left   = (FLOAT)x2;
			drawRect.right  = (FLOAT)widgetRect.right;
			drawRect.top    = (FLOAT)widgetRect.top;
			drawRect.bottom = (FLOAT)y1;
			brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.right - imageSize.width, drawRect.top));
			workingRT->FillRectangle(drawRect,brush);
		}
		if(drawParts[6]) {
			drawRect.left   = (FLOAT)widgetRect.left;
			drawRect.right  = (FLOAT)x1;
			drawRect.top    = (FLOAT)y2;
			drawRect.bottom = (FLOAT)widgetRect.bottom;
			brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.bottom - imageSize.height));
			workingRT->FillRectangle(drawRect,brush);
		}
		if(drawParts[8]) {
			drawRect.left   = (FLOAT)x2;
			drawRect.right  = (FLOAT)widgetRect.right;
			drawRect.top    = (FLOAT)y2;
			drawRect.bottom = (FLOAT)widgetRect.bottom;
			brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.right - imageSize.width, drawRect.bottom - imageSize.height));
			workingRT->FillRectangle(drawRect,brush);
		}
		if(drawParts[1]) {
			drawRect.left   = (FLOAT)x1;
			drawRect.right  = (FLOAT)x2;
			drawRect.top    = (FLOAT)widgetRect.top;
			drawRect.bottom = (FLOAT)y1;
			brush = getBorderImageBrush(TopCenter);
			if(brush) {
				D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
				if(!isRepeatX(borderImageRO))
					matrix = D2D1::Matrix3x2F::Scale(scaleX,1.0f) * matrix;
				brush->SetTransform(matrix);
				workingRT->FillRectangle(drawRect,brush);
			}
		}
		if(drawParts[3]) {
			drawRect.left   = (FLOAT)widgetRect.left;
			drawRect.right  = (FLOAT)x1;
			drawRect.top    = (FLOAT)y1;
			drawRect.bottom = (FLOAT)y2;
			brush = getBorderImageBrush(CenterLeft);
			if(brush) {
				D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
				if(!isRepeatY(borderImageRO))
					matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleY) * matrix;
				brush->SetTransform(matrix);
				workingRT->FillRectangle(drawRect,brush);
			}
		}
		if(drawParts[4]) {
			drawRect.left   = (FLOAT)x1;
			drawRect.right  = (FLOAT)x2;
			drawRect.top    = (FLOAT)y1;
			drawRect.bottom = (FLOAT)y2;
			brush = getBorderImageBrush(Center);
			if(brush) {
				D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
				if(!isRepeatX(borderImageRO))
					matrix = D2D1::Matrix3x2F::Scale(scaleX, isRepeatY(borderImageRO) ? 1.0f : scaleY) * matrix; 
				else if(!isRepeatY(borderImageRO))
					matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleY) * matrix;
				brush->SetTransform(matrix);
				workingRT->FillRectangle(drawRect,brush);
			}
		}
		if(drawParts[5]) {
			drawRect.left   = (FLOAT)x2;
			drawRect.right  = (FLOAT)widgetRect.right;
			drawRect.top    = (FLOAT)y1;
			drawRect.bottom = (FLOAT)y2;
			brush = getBorderImageBrush(CenterRight);
			if(brush) {
				D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
				if(!isRepeatY(borderImageRO))
					matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleY) * matrix;
				brush->SetTransform(matrix);
				workingRT->FillRectangle(drawRect,brush);
			}
		}
		if(drawParts[7]) {
			drawRect.left   = (FLOAT)x1;
			drawRect.right  = (FLOAT)x2;
			drawRect.top    = (FLOAT)y2;
			drawRect.bottom = (FLOAT)widgetRect.bottom;
			brush = getBorderImageBrush(BottomCenter);
			if(brush) {
				D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
				if(!isRepeatX(borderImageRO))
					matrix = D2D1::Matrix3x2F::Scale(scaleX,1.0f) * matrix;
				brush->SetTransform(matrix);
				workingRT->FillRectangle(drawRect,brush);
			}
		}
	}




	double get_time()
	{
		LARGE_INTEGER t, f;
		QueryPerformanceCounter(&t);
		QueryPerformanceFrequency(&f);
		return double(t.QuadPart)/double(f.QuadPart);
	}

	using namespace Gdiplus;
	void RenderRuleData::drawGdiText(const RECT& widgetRect, const D2D1_RECT_F& borderRect, const RECT& clipRectInRT, const wstring& text)
	{
		// TODO: Add support for underline and its line style.
		MFont defaultFont;
		TextRenderObject defaultRO(defaultFont);
		TextRenderObject* tro = textRO == 0 ? &defaultRO : textRO;
		bool hasOutline = tro->outlineWidth != 0 && !tro->outlineColor.isTransparent();
		bool hasShadow  = !tro->shadowColor.isTransparent() &&
			( tro->shadowBlur != 0 || (tro->shadowOffsetX != 0 || tro->shadowOffsetY != 0) );

		// drawText uses GDI to render the Text, because D2D1 have poor performance when rendering text.
		// Also, GDI(with GDI++ hook) makes text looks really good.

		RECT drawRect = {(LONG)borderRect.left, (LONG)borderRect.top, (LONG)borderRect.right, (LONG)borderRect.bottom };
		if(hasPadding) {
			drawRect.left   += padding.left;
			drawRect.right  -= padding.right;
			drawRect.top    += padding.top;
			drawRect.bottom -= padding.bottom;
		}

		// We need to get the background to which the anti-alias text to be rendered on.
		workingRT->PopAxisAlignedClip();
		ID2D1GdiInteropRenderTarget* gdiTarget;
		HDC textDC;
		workingRT->QueryInterface(&gdiTarget);
		gdiTarget->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,&textDC);

		HFONT oldFont = (HFONT)::SelectObject(textDC,tro->font.getHandle());
		HRGN  clipRGN = ::CreateRectRgn(clipRectInRT.left,clipRectInRT.top,clipRectInRT.right,clipRectInRT.bottom);
		HRGN  oldRgn  = (HRGN)::SelectObject(textDC,clipRGN);
		::SetBkMode(textDC,TRANSPARENT);

		// Draw the text
		if(hasOutline) // If we need to draw the outline, we have to use GDI+.
		{
			StringFormat sf;
			if(tro->overflow == Value_Ellipsis) sf.SetTrimming(StringTrimmingEllipsisWord);
			else sf.SetTrimming(StringTrimmingNone);
			if(tro->overflow != Value_Wrap) sf.SetFormatFlags(StringFormatFlagsNoWrap);
			switch(tro->alignX) {
				case Value_Center: sf.SetAlignment(StringAlignmentCenter); break;
				case Value_Right:  sf.SetAlignment(StringAlignmentFar);    break;
				default:           sf.SetAlignment(StringAlignmentNear);   break;
			}
			switch(tro->alignY) {
				case Value_Top:    sf.SetLineAlignment(StringAlignmentNear);   break;
				case Value_Bottom: sf.SetLineAlignment(StringAlignmentFar);    break;
				default:           sf.SetLineAlignment(StringAlignmentCenter); break;
			}

			GraphicsPath textPath;
			Font f(textDC);
			FontFamily fm;
			f.GetFamily(&fm);
			unsigned int style = FontStyleRegular;
			if(tro->font.isBold())
				style = tro->font.isItalic() ? FontStyleBoldItalic : FontStyleBold;
			else if(tro->font.isItalic())
				style = FontStyleItalic;
			int w = drawRect.right  - drawRect.left + tro->shadowBlur * 2;
			int h = drawRect.bottom - drawRect.top  + tro->shadowBlur * 2;
			Rect layoutRect(drawRect.left + tro->shadowOffsetX, drawRect.top + tro->shadowOffsetY, w, h);
			textPath.AddString(text.c_str(), -1, &fm, style, f.GetSize(), layoutRect, &sf);

			Graphics graphics(textDC);
			if(hasShadow)
			{
				Pen shadowPen(tro->shadowColor.getARGB());
				shadowPen.SetLineJoin(LineJoinRound);
				shadowPen.SetWidth(tro->outlineWidth);
				graphics.DrawPath(&shadowPen,&textPath);
				SolidBrush shadowBrush(tro->shadowColor.getARGB());
				graphics.FillPath(&shadowBrush,&textPath);
			}
			if(tro->shadowOffsetX != 0 || tro->shadowOffsetY != 0)
			{
				Matrix matrix;
				matrix.Translate(-tro->shadowOffsetX,-tro->shadowOffsetY);
				textPath.Transform(&matrix);
			}
			Pen outlinePen(tro->outlineColor.getARGB());
			outlinePen.SetLineJoin(LineJoinRound);
			outlinePen.SetWidth(tro->outlineWidth);
			graphics.DrawPath(&outlinePen,&textPath);
			SolidBrush textBrush(tro->color.getARGB());
			graphics.FillPath(&textBrush,&textPath);
		} else
		{
			unsigned int formatParam = tro->overflow == Value_Wrap ? 0 :
				tro->overflow == Value_Ellipsis ? DT_END_ELLIPSIS : DT_SINGLELINE;
			switch(tro->alignX) {
				case Value_Center: formatParam |= DT_CENTER; break;
				case Value_Right:  formatParam |= DT_RIGHT;  break;
				default:           formatParam |= DT_LEFT;   break;
			}
			switch(tro->alignY) {
				case Value_Top:    formatParam |= DT_TOP;     break;
				case Value_Bottom: formatParam |= DT_BOTTOM;  break;
				default:           formatParam |= DT_VCENTER; break;
			}

			if(hasShadow)
			{
				if(tro->shadowBlur != 0 || tro->shadowColor.getAlpha() != 0xFF)
				{
					HDC tempDC = ::CreateCompatibleDC(0);
					void* pvBits;
					BITMAPINFO bmi;
					ZeroMemory(&bmi,sizeof(BITMAPINFO));
					int width  = drawRect.right  - drawRect.left + tro->shadowBlur * 2;
					int height = drawRect.bottom - drawRect.top  + tro->shadowBlur * 2;
					bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
					bmi.bmiHeader.biWidth       = width;
					bmi.bmiHeader.biHeight      = height;
					bmi.bmiHeader.biCompression = BI_RGB;
					bmi.bmiHeader.biPlanes      = 1;
					bmi.bmiHeader.biBitCount    = 32;
					HBITMAP tempBMP = ::CreateDIBSection(tempDC, &bmi, DIB_RGB_COLORS, &pvBits, 0, 0);
					HBITMAP oldBMP  = (HBITMAP)::SelectObject(tempDC, tempBMP);
					HFONT   oldFont = (HFONT)  ::SelectObject(tempDC, tro->font);
					::SetTextColor(tempDC, RGB(255,255,255));
					::SetBkColor  (tempDC, RGB(0,0,0));
					::SetBkMode   (tempDC, OPAQUE);

					RECT shadowRect = {tro->shadowBlur, tro->shadowBlur,
							drawRect.right  - drawRect.left, drawRect.bottom - drawRect.top};
					::DrawTextW(tempDC, (LPWSTR)text.c_str(), -1, &shadowRect, formatParam);

					unsigned int* pixel = (unsigned int*)pvBits;
					for(int i = width * height - 1; i >= 0; --i)
					{
						if(pixel[i] != 0) {
							if(pixel[i] == 0xFFFFFF)
								pixel[i] = 0xFF000000 | tro->shadowColor.getRGB();
							else {
								BYTE alpha = (pixel[i] >> 24) & 0xFF;
								BYTE* component = (BYTE*)(pixel+i);
								unsigned int color = tro->shadowColor.getARGB();
								component[0] = ( color      & 0xFF) * alpha >> 8; 
								component[1] = ((color >> 8)& 0xFF) * alpha >> 8;
								component[2] = ((color >>16)& 0xFF) * alpha >> 8;
								component[3] = alpha;
							}
						}
					}

					if(tro->shadowBlur == 0)
					{
						BLENDFUNCTION bf = { AC_SRC_OVER, 0, tro->shadowColor.getAlpha(), AC_SRC_ALPHA };
						int xCor = drawRect.left - tro->shadowBlur;
						int yCor = drawRect.top  - tro->shadowBlur;
						int tempXCor = xCor; int tempYCor = yCor;
						if(xCor < clipRectInRT.left) xCor = clipRectInRT.left;
						if(yCor < clipRectInRT.top ) yCor = clipRectInRT.top;
						int w = 0 - xCor + (clipRectInRT.right < drawRect.right ? clipRectInRT.right : drawRect.right);
						int h = 0 - yCor + (clipRectInRT.bottom < drawRect.bottom ? clipRectInRT.bottom : drawRect.bottom);
						int r = ::GdiAlphaBlend(textDC, xCor + tro->shadowOffsetX, yCor + tro->shadowOffsetY,
									w, h, tempDC, xCor - tempXCor, yCor - tempYCor, w, h, bf);
						r = r;
					} else {
						// TODO: Implement blur. Don't know why the GDI+ version is still 1.0 on Win7 SDK,
						// So, we cannot use the gdiplus's blur effect.

// 						Bitmap gdiBMP(width,height,4 * width,PixelFormat32bppPARGB,(BYTE*)pvBits);
// 						Graphics graphics(textDC);
// 						RectF srcRect(0.f,0.f,(REAL)width,(REAL)height);
// 						Matrix matrix;
// 						Blur blurEffect;
// 						BlurParams bp;
// 						bp.expandEdge = true;
// 						bp.radius = tro->shadowBlur;
// 						blurEffect.SetParameters(&bp);
// 						graphics.DrawImage(&gdiBMP,&srcRect,&matrix,&blurEffect,0,UnitPixel);
					}

					::SelectObject(tempDC,oldBMP);
					::SelectObject(tempDC,oldFont);
					::DeleteObject(tempBMP);
					::DeleteDC(tempDC);

				} else {
					RECT shadowRect = {drawRect.left + tro->shadowOffsetX, drawRect.top + tro->shadowOffsetY,
						drawRect.right + tro->shadowOffsetX, drawRect.bottom + tro->shadowOffsetY};
					::SetTextColor(textDC, tro->shadowColor.getCOLORREF());
					::DrawTextW(textDC,(LPWSTR)text.c_str(),-1,&shadowRect,formatParam);
				}
			}
			::SetTextColor(textDC,tro->color.getCOLORREF());
			::DrawTextW(textDC,(LPWSTR)text.c_str(),-1,&drawRect,formatParam);
		}

		::SelectObject(textDC,oldFont);
		::SelectObject(textDC,oldRgn);
		::DeleteObject(clipRGN);

		gdiTarget->ReleaseDC(0);
		gdiTarget->Release();

		D2D1_RECT_F destRect;
		destRect.left   = (FLOAT)clipRectInRT.left;
		destRect.right  = (FLOAT)clipRectInRT.right;
		destRect.top    = (FLOAT)clipRectInRT.top;
		destRect.bottom = (FLOAT)clipRectInRT.bottom;
		workingRT->PushAxisAlignedClip(destRect, D2D1_ANTIALIAS_MODE_ALIASED);


		/*
		 * Second approach, it seems that this approach is even slower than the first.
		 * Besides, GDI++ doesn't work with these lines of code.
		 */
// 		{   
// 			unsigned int width   = drawRect.right  - drawRect.left;
// 			unsigned int height  = drawRect.bottom - drawRect.top;
// 			D2D1_RECT_U copyRect = {drawRect.left,drawRect.top,drawRect.right,drawRect.bottom};
// 
// 			ID2D1BitmapRenderTarget*     compatibleRT;
// 			ID2D1Bitmap*                 compatibleBMP;
// 			ID2D1GdiInteropRenderTarget* gdiRT;
// 			HDC textDC;
// 			D2D1_PIXEL_FORMAT pf   = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED);
// 			D2D1_SIZE_U       size = D2D1::SizeU(width,height);
// 
// 			double time = get_time();
// 			workingRT->PopAxisAlignedClip();
// 			workingRT->CreateCompatibleRenderTarget(0,&size,&pf,D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_GDI_COMPATIBLE,&compatibleRT);
// 			compatibleRT->BeginDraw();
// 			compatibleRT->GetBitmap(&compatibleBMP);
// 			compatibleBMP->CopyFromRenderTarget(0,workingRT,&copyRect);
// 			compatibleRT->QueryInterface(&gdiRT);
// 
// 			gdiRT->GetDC(D2D1_DC_INITIALIZE_MODE_COPY,&textDC);
// 
// 			RECT clipRect = {0,0,width,height};
// 			if(clipRectInRT.left   > drawRect.left  ) clipRect.left    = clipRectInRT.left - drawRect.left;
// 			if(clipRectInRT.top    > drawRect.top   ) clipRect.top     = clipRectInRT.top  - drawRect.top;
// 			if(clipRectInRT.right  < drawRect.right ) clipRect.right  -= (drawRect.right   - clipRectInRT.right );
// 			if(clipRectInRT.bottom < drawRect.bottom) clipRect.bottom -= (drawRect.bottom  - clipRectInRT.bottom);
// 			HRGN  clipRGN  = ::CreateRectRgn(clipRect.left,clipRect.top,clipRect.right,clipRect.bottom);
// 			HRGN  oldRgn  = (HRGN)::SelectObject(textDC,clipRGN);
// 			HFONT oldFont = (HFONT)::SelectObject(textDC,tro->font.getHandle());
// 			::SetBkMode(textDC,TRANSPARENT);
// 			::SetTextColor(textDC,tro->color);
// 			RECT dr = {0,0,width,height};
// 			::SelectObject(textDC,::GetStockObject(WHITE_BRUSH));
// 			BITMAP bmp;
// 			::GetObjectW(textDC,sizeof(BITMAP),&bmp);
// 			::DrawTextExW(textDC,(LPWSTR)text.c_str(),-1,&dr,formatParam,0);
// 			::SelectObject(textDC,oldFont);
// 			::SelectObject(textDC,oldRgn);
// 			::DeleteObject(clipRGN);
// 
// 			D2D1_RECT_F destRectF = { drawRect.left + clipRect.left,drawRect.top + clipRect.top, 0,0 };
// 			destRectF.right  = destRectF.left + clipRect.right  - clipRect.left;
// 			destRectF.bottom = destRectF.top  + clipRect.bottom - clipRect.top;
// 			workingRT->DrawBitmap(compatibleBMP,destRectF,1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
// 				D2D1::RectF(clipRect.left,clipRect.top,clipRect.right,clipRect.bottom));
// 			workingRT->PushAxisAlignedClip(D2D1::RectF((FLOAT)clipRectInRT.left, (FLOAT)clipRectInRT.top,
// 				(FLOAT)clipRectInRT.right, (FLOAT)clipRectInRT.bottom), D2D1_ANTIALIAS_MODE_ALIASED);
// 
// 			gdiRT->ReleaseDC(0);
// 			gdiRT->Release();
// 			compatibleRT->EndDraw();
// 			compatibleBMP->Release();
// 			compatibleRT->Release();
// 
// 			time = get_time() - time;
// 			std::stringstream s;
// 			s << "B:" << time << "\n";
// 			OutputDebugStringA(s.str().c_str());
// 		}
	}

	// ********** RenderRule Impl
	RenderRule::RenderRule(RenderRuleData* d):data(d)
		{ if(data) ++data->refCount; }
	RenderRule::RenderRule(const RenderRule& d):data(d.data)
		{ if(data) ++data->refCount; }
	bool RenderRule::opaqueBackground() const
		{ return data == 0 ? false : data->opaqueBackground; }
	void RenderRule::draw(ID2D1RenderTarget* rt,const RECT& wr, const RECT& cr,const wstring& t)
		{ if(data) data->draw(rt,wr,cr,t); }
	void RenderRule::init()
		{ M_ASSERT(data==0); data = new RenderRuleData(); }
	RenderRule::~RenderRule()
	{
		if(data) {
			if(--data->refCount == 0)
				delete data;
		}
	}
	const RenderRule& RenderRule::operator=(const RenderRule& rhs)
	{
		if(&rhs != this)
		{
			if(data) {
				if(--data->refCount == 0)
					delete data;
			}
			if((data = rhs.data) != 0)
				++data->refCount;
		}
		return *this;
	}

	// ********** RenderRuleCacheKey Impl 
	bool RenderRuleCacheKey::operator<(const RenderRuleCacheKey& rhs) const
	{
		if(this == &rhs)
			return false;

		int size1 = styleRules.size();
		int size2 = rhs.styleRules.size();
		if(size1 > size2) {
			return false;
		} else if(size1 < size2) {
			return true;
		} else {
			for(int i = 0; i < size1; ++i)
			{
				StyleRule* sr1 = styleRules.at(i);
				StyleRule* sr2 = rhs.styleRules.at(i);
				if(sr1 > sr2)
					return false;
				else if(sr1 < sr2)
					return true;
			}
		}
		return false;
	}


	extern MCursor gArrowCursor;
	// ********** MStyleSheetStyle Impl
	MStyleSheetStyle::MStyleSheetStyle():mImpl(new MSSSPrivate()) {}
	MStyleSheetStyle::~MStyleSheetStyle()
		{ delete mImpl; }
	void MStyleSheetStyle::setAppSS(const wstring& css)
		{ mImpl->setAppSS(css); }
	void MStyleSheetStyle::setWidgetSS(MWidget* w, const wstring& css)
		{ mImpl->setWidgetSS(w,css); }
	void MStyleSheetStyle::polish(MWidget* w)
		{ mImpl->polish(w); }
	void MStyleSheetStyle::draw(MWidget* w,ID2D1RenderTarget* rt, const RECT& wr, const RECT& cr, const std::wstring& t)
		{ mImpl->draw(w,rt,wr,cr,t); } 
	void MStyleSheetStyle::removeCache(MWidget* w)
		{ mImpl->removeCache(w); }
	RenderRule MStyleSheetStyle::getRenderRule(MWidget* w, unsigned int p)
		{return mImpl->getRenderRule(w,p); }
	void MStyleSheetStyle::updateWidgetAppearance(MWidget* w)
	{
		unsigned int lastP = w->getLastWidgetPseudo();
		unsigned int currP = w->getWidgetPseudo();
		RenderRule currRule = getRenderRule(w,currP);
		if(lastP != currP)
		{
			RenderRule lastRule = getRenderRule(w,w->getLastWidgetPseudo());
			if(lastRule != currRule)
			{
				w->repaint();
				w->ssSetOpaque(currRule.opaqueBackground());
				// Remark: we can update the widget's property here.
			}
		}

		// We always update the cursor.
		MCursor* cursor = w->getCursor();
		if(cursor == 0 && currRule) cursor = currRule->cursor;
		if(cursor == 0) cursor = &gArrowCursor;
		cursor->show();
	}
	void MStyleSheetStyle::setTextRenderer(TextRenderer t)
		{ MSSSPrivate::textRenderer = t; }
	MStyleSheetStyle::TextRenderer MStyleSheetStyle::getTextRenderer()
		{ return MSSSPrivate::textRenderer; }
	

	// ********** MSSSPrivate Implementation
	MSSSPrivate* MSSSPrivate::instance = 0;
	MStyleSheetStyle::TextRenderer MSSSPrivate::textRenderer = MStyleSheetStyle::Gdi;
	void getWidgetClassName(const MWidget* w,wstring& name)
	{
		wstringstream s;
		s << typeid(*w).name();
		name = s.str();
		name.erase(0,6); // Eat the 'class ' generated by msvc.

		int pos = 0;
		while((pos = name.find(L"::",pos)) != wstring::npos)
		{
			name.replace(pos,2,L"--");
			pos += 2;
		}
	}

	bool basicSelectorMatches(const BasicSelector* bs, MWidget* w)
	{
		if(!bs->id.empty() && w->objectName() != bs->id)
			return false;

		if(!bs->elementName.empty()) {
			wstring ss;
			getWidgetClassName(w,ss);
			if(bs->elementName != ss)
				return false;
		}
		return true;
	}

	bool selectorMatches(const Selector* sel, MWidget* w)
	{
		const vector<BasicSelector*>& basicSels = sel->basicSelectors;
		if(basicSels.size() == 1)
			return basicSelectorMatches(basicSels.at(0),w);

		// first element must always match!
		if(!basicSelectorMatches(basicSels.at(basicSels.size() - 1), w))
			return false;

		w = w->parent();

		if(w == 0)
			return false;

		int i = basicSels.size() - 2;
		bool matched = true;
		BasicSelector* bs = 0;/*basicSels.at(i);*/
		while(i > 0 && (matched || bs->relationToNext == BasicSelector::MatchNextIfAncestor))
		{
			bs = basicSels.at(i);
			matched = basicSelectorMatches(bs,w);

			if(!matched)
			{
				if(bs->relationToNext == BasicSelector::MatchNextIfParent)
					break;
			} else
				--i;

			if((w = w->parent()) == 0)
				return false;
		}

		return matched;
	}

	void matchRule(MWidget* w,const StyleRule* rule,int depth,
		multimap<int, MatchedStyleRule>& weightedRules)
	{
		for(unsigned int i = 0; i < rule->selectors.size(); ++i)
		{
			const Selector* sel = rule->selectors.at(i);
			if(selectorMatches(sel, w))
			{
				int weight = rule->order + sel->specificity() * 0x100 + depth* 0x100000;
				MatchedStyleRule msr;
				msr.matchedSelector = sel;
				msr.styleRule = rule;
				weightedRules.insert(multimap<int, MatchedStyleRule>::value_type(weight,msr));
			}
		}
	}

	void MSSSPrivate::getMachedStyleRules(MWidget* widget, MatchedStyleRuleVector& srs)
	{
		// Get all possible stylesheets
		vector<CSS::StyleSheet*> sheets;
		for(MWidget* p = widget; p != 0; p = p->parent())
		{
			WidgetSSCache::const_iterator wssIter = widgetSSCache.find(p);
			if(wssIter != widgetSSCache.end())
				sheets.push_back(wssIter->second);
		}
		sheets.push_back(appStyleSheet);

		multimap<int, MatchedStyleRule> weightedRules;
		int sheetCount = sheets.size();
		for(int sheetIndex = 0; sheetIndex < sheetCount; ++sheetIndex)
		{
			int depth = sheetCount - sheetIndex;
			const StyleSheet* sheet = sheets.at(sheetIndex);
			// search for universal rules.
			for(unsigned int i = 0;i< sheet->universal.size(); ++i) {
				matchRule(widget,sheet->universal.at(i),depth,weightedRules);
			}

			// check for Id
			if(!sheet->srIdMap.empty() && !widget->objectName().empty())
			{
				pair<StyleSheet::StyleRuleIdMap::const_iterator,
					StyleSheet::StyleRuleIdMap::const_iterator> result =
					sheet->srIdMap.equal_range(widget->objectName());
				while(result.first != result.second)
				{
					matchRule(widget,result.first->second,depth,weightedRules);
					++result.first;
				}
			}

			// check for class name
			if(!sheet->srElementMap.empty())
			{
				wstring widgetClassName;
				getWidgetClassName(widget,widgetClassName);
				pair<StyleSheet::StyleRuleElementMap::const_iterator,
					StyleSheet::StyleRuleElementMap::const_iterator> result =
					sheet->srElementMap.equal_range(widgetClassName);
				while(result.first != result.second)
				{
					matchRule(widget,result.first->second,depth,weightedRules);
					++result.first;
				}
			}
		}

		srs.clear();
		srs.reserve(weightedRules.size());
		multimap<int, MatchedStyleRule>::const_iterator wrIt    = weightedRules.begin();
		multimap<int, MatchedStyleRule>::const_iterator wrItEnd = weightedRules.end();

		while(wrIt != wrItEnd) {
			srs.push_back(wrIt->second);
			++wrIt;
		}
	}

	// Remark. We should make gif animation possible.
	RenderRule MSSSPrivate::getRenderRule(MWidget* w, unsigned int pseudo)
	{
		// == 1.Find RenderRule for MWidget w from Widget-RenderRule cache.
		WidgetRenderRuleMap::iterator widgetCacheIter = widgetRenderRuleCache.find(w);
		PseudoRenderRuleMap* prrMap = 0;
		if(widgetCacheIter != widgetRenderRuleCache.end())
		{
			prrMap = widgetCacheIter->second;
			PseudoRenderRuleMap::Element::iterator prrIter = prrMap->element.find(pseudo);
			if(prrIter != prrMap->element.end())
				return prrIter->second;
		}

		// == 2.Find StyleRules for MWidget w from Widget-StyleRule cache.
		WidgetStyleRuleMap::iterator wsrIter = widgetStyleRuleCache.find(w);
		MatchedStyleRuleVector* matchedsrv;
		if(wsrIter != widgetStyleRuleCache.end()) {
			matchedsrv = &(wsrIter->second);
		} else {
			matchedsrv = &(widgetStyleRuleCache.insert(
				WidgetStyleRuleMap::value_type(w, MatchedStyleRuleVector())).first->second);
			getMachedStyleRules(w,*matchedsrv);
		}

		// == 3.If we don't have a PseudoRenderRuleMap, build one.
		if(prrMap == 0)
		{
			RenderRuleCacheKey cacheKey;
			int srvSize = matchedsrv->size();
			cacheKey.styleRules.reserve(srvSize);
			for(int i = 0; i< srvSize; ++i)
				cacheKey.styleRules.push_back(const_cast<StyleRule*>(matchedsrv->at(i).styleRule));
			prrMap = &(renderRuleCollection[cacheKey]);
			widgetRenderRuleCache.insert(WidgetRenderRuleMap::value_type(w,prrMap));
		}

		// == 4. Merge the declarations.
		PseudoRenderRuleMap::Element::iterator hasRRIter = prrMap->element.find(pseudo);
		if(hasRRIter != prrMap->element.end())
		{
			// There's a RenderRule can be used for this widget. Although this
			// renderRule may still be invalid.
			return hasRRIter->second;
		}
		RenderRule& renderRule = prrMap->element[pseudo]; // Insert a RenderRule even if it's empty.
		// If there's no StyleRules for MWidget w, then we return a invalid RenderRule.
		if(matchedsrv->size() == 0)
			return renderRule;

		MatchedStyleRuleVector::const_iterator msrIter = matchedsrv->begin();
		MatchedStyleRuleVector::const_iterator msrIterEnd = matchedsrv->end();
		DeclMap declarations;
		unsigned int realPseudo = 0;
		while(msrIter != msrIterEnd)
		{
			const Selector* sel = msrIter->matchedSelector;
			if(sel->matchPseudo(pseudo))
			{
				realPseudo |= sel->pseudo();
				// This rule is for the widget with pseudo.
				// Add declarations to map.
				const StyleRule* sr = msrIter->styleRule;
				vector<Declaration*>::const_iterator declIter = sr->declarations.begin();
				vector<Declaration*>::const_iterator declIterEnd = sr->declarations.end();
				set<PropertyType> tempProps;

				bool inheritBG = false;
				// Remove duplicate properties
				while(declIter != declIterEnd)
				{
					// Check if user defined "PT_InheritBackground",
					// If true, we keep any background specified in other declarations.
					// otherwise, keep every backgrounds
					PropertyType prop = (*declIter)->property;
					if(prop == PT_InheritBackground)
						inheritBG = true;
					else
						tempProps.insert(prop);
					++declIter;
				}
				set<PropertyType>::iterator tempPropIter = tempProps.begin();
				set<PropertyType>::iterator tempPropIterEnd = tempProps.end();
				while(tempPropIterEnd != tempPropIter) {
					if(!(inheritBG && *tempPropIter == PT_Background))
						declarations.erase(*tempPropIter);
					++tempPropIter;
				}

				// Merge declaration
				declIter = sr->declarations.begin();
				while(declIter != declIterEnd) {
					Declaration* d = *declIter;
					if(d->property != PT_InheritBackground)
						declarations.insert(DeclMap::value_type(d->property,d));
					++declIter;
				}
			}
			++msrIter;
		}

		// Sometimes, if our code wants a RenderRule for pseudo Hover|Checked|FirstChild|...,
		// but the user only defines pseudo "Checked" for the widget, so we end up with a
		// RenderRule that is the same as the RenderRule for "Checked", in this case, we can
		// share them.
		if(realPseudo != pseudo)
		{
			hasRRIter = prrMap->element.find(realPseudo);
			if(hasRRIter != prrMap->element.end())
			{
				renderRule = hasRRIter->second;
				return renderRule;
			}
		}
		
		// Remove duplicate declarations (except background, because we support multi backgrounds)
		PropertyType lastType = knownPropertyCount;
		DeclMap::iterator declRIter = declarations.begin();
		DeclMap::iterator declRIterEnd = declarations.end();
		while(declRIter != declRIterEnd) {
			PropertyType type = declRIter->second->property;
			if(type != PT_Background) {
				if(lastType == type) {
					declRIter = declarations.erase(--declRIter);
				} else {
					lastType = type;
				}
			}
			++declRIter;
		}

		if(declarations.size() == 0)
			return renderRule;

		// == 5.Create RenderRule
		initRenderRule(renderRule,declarations);
		return renderRule;
	}

	inline void MSSSPrivate::polish(MWidget* w)
	{
		RenderRule rule = getRenderRule(w, PC_Default);
		if(rule) rule->setGeometry(w);

		RenderRule hoverRule = getRenderRule(w, PC_Hover);
		if(hoverRule && hoverRule != rule)
			w->setAttributes(WA_Hover);

		w->ssSetOpaque(rule.opaqueBackground());
	}

	inline void MSSSPrivate::draw(MWidget* w, ID2D1RenderTarget* rt,
		const RECT& wr, const RECT& cr, const wstring& t)
	{
		CSS::RenderRule rule = getRenderRule(w,w->getWidgetPseudo(true));
		rule.draw(rt,wr,cr,t);
	}

	inline void MSSSPrivate::removeCache(MWidget* w)
	{
		// We just have to remove from these two cache, without touching
		// renderRuleCollection, because some of them might be using by others.
		widgetStyleRuleCache.erase(w);
		widgetRenderRuleCache.erase(w);
	}

	inline MSSSPrivate::MSSSPrivate():appStyleSheet(new StyleSheet())
		{ instance = this; }
	MSSSPrivate::~MSSSPrivate()
	{
		instance = 0;
		delete appStyleSheet;
		WidgetSSCache::iterator it    = widgetSSCache.begin();
		WidgetSSCache::iterator itEnd = widgetSSCache.end();
		while(it != itEnd) {
			delete it->second;
			++it;
		}

		removeResources();
	}

	void MSSSPrivate::setAppSS(const wstring& css)
	{
		delete appStyleSheet;
		renderRuleCollection.clear();
		widgetStyleRuleCache.clear();
		widgetRenderRuleCache.clear();

		removeResources();

		appStyleSheet = new StyleSheet();
		MCSSParser parser(css);
		parser.parse(appStyleSheet);
	}

	void MSSSPrivate::setWidgetSS(MWidget* w, const wstring& css)
	{
		// Remove StyleSheet Cache for MWidget w.
		WidgetSSCache::const_iterator iter = widgetSSCache.find(w);
		if(iter != widgetSSCache.end())
		{
			delete iter->second;
			widgetSSCache.erase(iter);
		}

		if(!css.empty())
		{
			StyleSheet* ss = new StyleSheet();
			MCSSParser parser(css);
			parser.parse(ss);
			widgetSSCache.insert(WidgetSSCache::value_type(w,ss));
		}

		// Remove every child widgets' cache, so that they can recalc the StyleRules
		// next time the StyleRules are needed.
		clearRRCacheRecursively(w);
	}

	void MSSSPrivate::clearRRCacheRecursively(MWidget* w)
	{
		widgetStyleRuleCache.erase(w);
		widgetRenderRuleCache.erase(w);

		const MWidgetList& children = w->children();
		MWidgetList::const_iterator it = children.begin();
		MWidgetList::const_iterator itEnd = children.end();
		while(it != itEnd)
		{
			clearRRCacheRecursively(*it);
			++it;
		}
	}

	void MSSSPrivate::removeResources()
	{
		D2D1SolidBrushMap::iterator sit = solidBrushCache.begin();
		D2D1SolidBrushMap::iterator sitEnd = solidBrushCache.end();
		while(sit != sitEnd)
		{
			SafeRelease(sit->second);
			// Keep the brush position inside the map, because
			// the RenderRule use the BrushPosition to find the
			// resources.
			solidBrushCache[sit->first] = 0;
			++sit;
		}

		D2D1BitmapBrushMap::iterator bit = bitmapBrushCache.begin();
		D2D1BitmapBrushMap::iterator bitEnd = bitmapBrushCache.end();
		while(bit != bitEnd)
		{
			SafeRelease(bit->second);
			bitmapBrushCache[bit->first] = 0;
			++bit;
		}

		biPortionCache.clear();
	}

	enum BorderType { BT_Simple, BT_Radius, BT_Complex };
	enum BackgroundOpaqueType {
		NonOpaque,
		OpaqueClipMargin,
		OpaqueClipBorder,
		OpaqueClipPadding
	};
	BorderType testBorderObjectType(CssValueArray& values,
		const MSSSPrivate::DeclMap::iterator& declIter,
		const MSSSPrivate::DeclMap::iterator& declIterEnd)
	{
		MSSSPrivate::DeclMap::iterator seeker = declIter;
		while(seeker != declIterEnd && seeker->first <= PT_BorderRadius)
		{
			switch(seeker->first) {
			case PT_Border:
				{
					CssValueArray& values = seeker->second->values;
					// If there're more than 3 values in PT_Border, it must be complex.
					if(values.size() > 3)
						return BT_Complex;
					for(unsigned int i = 1; i < values.size() ; ++i)
					{ // If there are the same type values, it's complex.
						if(values.at(i - 1).type == values.at(i).type)
							return BT_Complex;
					}
				}
				break;
			case PT_BorderWidth:
			case PT_BorderColor:
			case PT_BorderStyles:
				if(seeker->second->values.size() > 1)
					return BT_Complex;
				break;
			case PT_BorderRadius:	return seeker->second->values.size() > 1 ? BT_Complex : BT_Radius;
			default: return BT_Complex;
			}
			++seeker;
		}
		return BT_Simple;
	}

	// ********** Create The BackgroundRO
	bool isBrushValueOpaque(const CssValue& value)
	{
		if(value.type == CssValue::Color)
			return true;

		M_ASSERT_X(value.type == CssValue::Uri,"The brush value must be neither Color or Uri(for now)","isBrushValueOpaque()");
		const std::wstring* uri = value.data.vstring;
		std::wstring ex = uri->substr(uri->size() - 3);
		std::transform(ex.begin(),ex.end(),ex.begin(),::tolower);
		if(ex.find(L"png") == 0 || ex.find(L"gif") == 0)
			return false;
		return true;
	}
	// Return true if the "prop" affects alignX
	bool setBGROProperty(BackgroundRenderObject* object, ValueType prop, bool isPrevPropAlignX)
	{
		switch(prop) {

		case Value_NoRepeat: object->repeat = CSS::NoRepeat; break;
		case Value_RepeatX:  object->repeat = CSS::RepeatX;  break;
		case Value_RepeatY:  object->repeat = CSS::RepeatY;  break;
		case Value_Repeat:   object->repeat = CSS::RepeatXY; break;

		case Value_Padding:
		case Value_Content:
		case Value_Border:
		case Value_Margin:   object->clip = prop; break;

		case Value_Left:
		case Value_Right:	 object->alignX = prop; return true;
		case Value_Top:
		case Value_Bottom:   object->alignY = prop; break;
		// We both set alignment X & Y, because one may only specific a alignment value.
		case Value_Center:
			object->alignY = prop;
			if(!isPrevPropAlignX) { object->alignX= prop; return true; }
		}
		return false;
	}
	void batchSetBGROProps(std::vector<BackgroundRenderObject*>& bgros, ValueType prop, bool isPrevPropAlignX = false)
	{
		std::vector<BackgroundRenderObject*>::iterator iter = bgros.begin();
		std::vector<BackgroundRenderObject*>::iterator iterEnd = bgros.end();
		while(iter != iterEnd) { setBGROProperty(*iter,prop,isPrevPropAlignX); ++iter; }
	}
	// Background: { Brush/Image Repeat Clip Alignment pos-x pos-y }*;
	// If we want to make it more like CSS3 syntax,
	// we can parse a CSS3 syntax background into multiple Background Declarations.
	BackgroundRenderObject* MSSSPrivate::createBackgroundRO(CssValueArray& values)
	{
		const CssValue& brushValue = values.at(0);
		BackgroundRenderObject* newObject = new BackgroundRenderObject(brushValue);
		// We tell the BGRO that the position of the brush. We don't care if the brush
		// is created right now.
		if(brushValue.type == CssValue::Uri)
		{
			ID2D1BitmapBrush*& brush = bitmapBrushCache[*brushValue.data.vstring];
			newObject->brushPosition = (ID2D1Brush**) &brush; 
		} else {
			ID2D1SolidColorBrush*& brush = solidBrushCache[brushValue.data.vuint];
			newObject->brushPosition = (ID2D1Brush**) &brush;
		}

		size_t index = 1;
		size_t valueCount = values.size();
		bool isPropAlignX = false;
		while(index < valueCount)
		{
			const CssValue& v = values.at(index);
			if(v.type == CssValue::Identifier) {
				isPropAlignX = setBGROProperty(newObject, static_cast<ValueType>(v.data.vuint), isPropAlignX);
			} else {
				mWarning(v.type == CssValue::Length || v.type == CssValue::Number, L"The rest of background css value should be of type 'length' or 'number'");

				newObject->x = v.data.vuint;

				if(++index >= values.size()) { break; }
				if(values.at(index).type == CssValue::Identifier) { continue; }
				newObject->y = v.data.vuint;

				if(++index >= values.size()) { break; }
				if(values.at(index).type == CssValue::Identifier) { continue; }
				newObject->width = v.data.vuint;

				if(++index >= values.size()) { break; }
				if(values.at(index).type == CssValue::Identifier) { continue; }
				newObject->height = v.data.vuint;
			}
			++index;
		}

		return newObject;
	}

	// ********** Set the SimpleBorderRO
	void MSSSPrivate::setSimpleBorderRO(SimpleBorderRenderObject* obj,
		DeclMap::iterator& iter, DeclMap::iterator iterEnd)
	{
		CssValue colorValue(CssValue::Color);
		while(iter != iterEnd)
		{
			vector<CssValue>& values = iter->second->values;
			switch(iter->first) {
				case PT_Border:
					for(unsigned int i = 0; i < values.size(); ++i) {
						CssValue& v = values.at(i);
						switch(v.type) {
							case CssValue::Identifier: obj->style = v.data.videntifier; break;
							case CssValue::Color:      colorValue = v;                  break;
							default:                   obj->width = v.data.vuint;       break;
						}
					}
					break;
				case PT_BorderWidth:  obj->width = values.at(0).data.vuint;       break;
				case PT_BorderStyles: obj->style = values.at(0).data.videntifier; break;
				case PT_BorderColor:  colorValue = values.at(0);                  break;
			}

			++iter;
		}
		obj->color = colorValue.data.vuint;
		ID2D1SolidColorBrush*& brush = solidBrushCache[obj->color];
		obj->brushPosition = &brush;
	}

	// ********** Create the ComplexBorderRO
	void setGroupUintValue(unsigned int (&intArray)[4], std::vector<CssValue>& values,
		int startValueIndex = 0, int endValueIndex = -1)
	{
		int size = (endValueIndex == -1 ? values.size() : endValueIndex + 1) - startValueIndex;
		if(size == 4) {
			intArray[0] = values.at(startValueIndex    ).data.vuint;
			intArray[1] = values.at(startValueIndex + 1).data.vuint;
			intArray[2] = values.at(startValueIndex + 2).data.vuint;
			intArray[3] = values.at(startValueIndex + 3).data.vuint;
		} else if(size == 2) {
			intArray[0] = intArray[2] = values.at(startValueIndex    ).data.vuint;
			intArray[1] = intArray[3] = values.at(startValueIndex + 1).data.vuint;
		} else {
			intArray[0] = intArray[1] =
			intArray[2] = intArray[3] = values.at(startValueIndex).data.vuint;
		}
	}
	void setD2DRectValue(D2D_RECT_U& rect, std::vector<CssValue>& values,
		int startValueIndex = 0, int endValueIndex = -1)
	{
		int size = (endValueIndex == -1 ? values.size() : endValueIndex + 1) - startValueIndex;
		if(size == 4) {
			rect.top    = values.at(startValueIndex    ).data.vuint;
			rect.right  = values.at(startValueIndex + 1).data.vuint;
			rect.bottom = values.at(startValueIndex + 2).data.vuint;
			rect.left   = values.at(startValueIndex + 3).data.vuint;
		} else if(size == 2) {
			rect.top    = rect.bottom = values.at(startValueIndex    ).data.vuint;
			rect.right  = rect.left   = values.at(startValueIndex + 1).data.vuint;
		} else {
			rect.top    = rect.bottom =
			rect.right  = rect.left   = values.at(startValueIndex).data.vuint;
		}
	}
	void setD2DRectValue(D2D_RECT_U& rect, int border, unsigned int value)
	{
		if(border == 0) { rect.top    = value; return; }
		if(border == 1) { rect.right  = value; return; }
		if(border == 2) { rect.bottom = value; return; }
		rect.left = value;
	}
	void MSSSPrivate::setComplexBorderRO(ComplexBorderRenderObject* obj,
		DeclMap::iterator& declIter, DeclMap::iterator declIterEnd)
	{
		while(declIter != declIterEnd)
		{
			vector<CssValue>& values = declIter->second->values;
			switch(declIter->first)
			{
			case PT_Border:
				{
					int rangeStartIndex = 0;
					CssValue::Type valueType = values.at(0).type;
					unsigned int index = 1;
					while(index <= values.size())
					{
						if(index == values.size() || values.at(index).type != valueType)
						{
							switch(valueType) {
								case CssValue::Identifier:
									setD2DRectValue(obj->styles,values,rangeStartIndex,index - 1);
									break;
								case CssValue::Color:
									obj->setColors(this,values,rangeStartIndex,index);
									break;
								default:
									setD2DRectValue(obj->widths,values,rangeStartIndex,index-1);
							}
							rangeStartIndex = index;
							if(index < values.size())
								valueType = values.at(index).type;
						}
						++index;
					}
				}
				break;

			case PT_BorderRadius:  setGroupUintValue(obj->radiuses, values);    break;
			case PT_BorderWidth:   setD2DRectValue(obj->widths, values);        break;
			case PT_BorderStyles:  setD2DRectValue(obj->styles, values);        break;
			case PT_BorderColor:   obj->setColors(this,values,0,values.size()); break;

			case PT_BorderTop:
			case PT_BorderRight:
			case PT_BorderBottom:
			case PT_BorderLeft:
				{
					int index = declIter->first - PT_BorderTop;
					for(unsigned int i = 0; i < values.size(); ++i)
					{
						if(values.at(i).type == CssValue::Color) {
							obj->colors[index] = values.at(0).data.vuint;
							ID2D1SolidColorBrush*& brush = solidBrushCache[obj->colors[index]];
							obj->brushPositions[index] = &brush;
						} else if(values.at(i).type == CssValue::Identifier) {
							ValueType vi = values.at(0).data.videntifier;
							if(index == 0)      obj->styles.top = vi;
							else if(index == 1) obj->styles.right = vi;
							else if(index == 2) obj->styles.bottom = vi;
							else obj->styles.left = vi;
						} else
							setD2DRectValue(obj->widths,index,values.at(i).data.vuint);
					}
				}
				break;
			case PT_BorderTopColor:
			case PT_BorderRightColor:
			case PT_BorderBottomColor:
			case PT_BorderLeftColor: 
				{
					int index = declIter->first - PT_BorderTopColor;
					obj->colors[index] = values.at(0).data.vuint;
					ID2D1SolidColorBrush*& brush = solidBrushCache[obj->colors[index]];
					obj->brushPositions[index] = &brush;
				}
				break;
			case PT_BorderTopWidth:
			case PT_BorderRightWidth:
			case PT_BorderBottomWidth:
			case PT_BorderLeftWidth:
				setD2DRectValue(obj->widths, declIter->first - PT_BorderTopWidth, values.at(0).data.vuint);
				break;
			case PT_BorderTopStyle:
			case PT_BorderRightStyle:
			case PT_BorderBottomStyle:
			case PT_BorderLeftStyle:
				setD2DRectValue(obj->styles, declIter->first - PT_BorderTopStyle, values.at(0).data.videntifier);
				break;
			case PT_BorderTopLeftRadius:
			case PT_BorderTopRightRadius:
			case PT_BorderBottomLeftRadius:
			case PT_BorderBottomRightRadius:
				obj->radiuses[declIter->first - PT_BorderTopLeftRadius] = values.at(0).data.vuint;
				break;
			}
			++declIter;
		}
		obj->checkUniform();
	}


	// ********** Create the BorderImageRO
	BorderImageRenderObject* MSSSPrivate::createBorderImageRO(CssValueArray& values)
	{
		BorderImageRenderObject* biro = new BorderImageRenderObject();

		biro->imageUri = values.at(0).data.vstring;
		ID2D1BitmapBrush*& brush = bitmapBrushCache[*biro->imageUri];
		biro->brushPosition = &brush;

		int endIndex = values.size() - 1;
		// Repeat or Stretch
		if(values.at(endIndex).type == CssValue::Identifier)
		{
			if(values.at(endIndex-1).type == CssValue::Identifier) {
				if(values.at(endIndex).data.vuint != Value_Stretch)
					biro->repeat = RepeatY;
				if(values.at(endIndex-1).data.vuint != Value_Stretch)
					biro->repeat = (biro->repeat == RepeatY) ? RepeatXY : RepeatY;
				endIndex -= 2;
			} else {
				if(values.at(endIndex).data.vuint != Value_Stretch)
					biro->repeat = RepeatXY;
				--endIndex;
			}
		}
		// Border
		if(endIndex == 4) {
			biro->topW = values.at(1).data.vuint;
			biro->rightW = values.at(2).data.vuint;
			biro->bottomW = values.at(3).data.vuint;
			biro->leftW = values.at(4).data.vuint;
		} else if(endIndex == 2) {
			biro->bottomW = biro->topW   = values.at(1).data.vuint;
			biro->leftW   = biro->rightW = values.at(2).data.vuint;
		} else {
			biro->leftW   = biro->rightW =
			biro->bottomW = biro->topW   = values.at(1).data.vuint;
		}

		return biro;
	}

	void MSSSPrivate::initRenderRule(RenderRule& renderRule, DeclMap& declarations)
	{
		// Remark: If the declaration have values in wrong order, it might crash the program.
		// Maybe we should add some logic to avoid this flaw.
		renderRule.init();
		DeclMap::iterator declIter    = declarations.begin();
		DeclMap::iterator declIterEnd = declarations.end();

		vector<BackgroundOpaqueType> bgOpaqueTypes;
		bool opaqueBorderImage = false;
		BorderType bt = BT_Simple;

		// --- Backgrounds ---
		while(declIter->first == PT_Background)
		{
			BackgroundOpaqueType bgOpaqueType = NonOpaque;
			CssValueArray& values = declIter->second->values;
			BackgroundRenderObject* o = createBackgroundRO(values);
			if(isBrushValueOpaque(values.at(0)))
			{
				if(o->clip == Value_Margin)
					bgOpaqueType = OpaqueClipMargin;
				else if(o->clip == Value_Border)
					bgOpaqueType = OpaqueClipBorder;
				else
					bgOpaqueType = OpaqueClipPadding;
			}

			renderRule->backgroundROs.push_back(o);
			bgOpaqueTypes.push_back(bgOpaqueType);

			if(++declIter == declIterEnd) goto END;
		}
		while(declIter->first < PT_BackgroundPosition)
		{
			batchSetBGROProps(renderRule->backgroundROs, declIter->second->values.at(0).data.videntifier);

			if(++declIter == declIterEnd) goto END;
		}
		if(declIter->first == PT_BackgroundPosition)
		{
			vector<CssValue>& values = declIter->second->values;
			int propx = values.at(0).data.vint;
			int propy = values.size() == 1 ? propx : values.at(1).data.vint;

			vector<BackgroundRenderObject*>::iterator it    = renderRule->backgroundROs.begin();
			vector<BackgroundRenderObject*>::iterator itEnd = renderRule->backgroundROs.end();
			while(it != itEnd) {
				(*it)->x = propx;
				(*it)->y = propy;
				++it;
			}
			if(++declIter == declIterEnd) goto END;
		}
		if(declIter->first == PT_BackgroundSize)
		{
			vector<CssValue>& values = declIter->second->values;
			int propw = values.at(0).data.vuint;
			int proph = values.size() == 1 ? propw : values.at(1).data.vuint;

			vector<BackgroundRenderObject*>::iterator it    = renderRule->backgroundROs.begin();
			vector<BackgroundRenderObject*>::iterator itEnd = renderRule->backgroundROs.end();
			while(it != itEnd) {
				(*it)->width  = propw;
				(*it)->height = proph;
				++it;
			}
			if(++declIter == declIterEnd) goto END;
		}
		if(declIter->first == PT_BackgroundAlignment)
		{
			vector<CssValue>& values = declIter->second->values;
			batchSetBGROProps(renderRule->backgroundROs,values.at(0).data.videntifier);
			if(values.size() == 2)
				batchSetBGROProps(renderRule->backgroundROs,values.at(1).data.videntifier,true);

			if(++declIter == declIterEnd) goto END;
		}

		// --- BorderImage --- 
		if(declIter->first == PT_BorderImage)
		{
			CssValueArray& values = declIter->second->values;
			opaqueBorderImage = isBrushValueOpaque(values.at(0));
			renderRule->borderImageRO = createBorderImageRO(declIter->second->values);
			if(++declIter == declIterEnd) goto END;
		}

		// --- Border ---
		if(declIter->first == PT_Border)
		{
			CssValue& v = declIter->second->values.at(0);
			if(v.type == CssValue::Identifier  && v.data.vuint == Value_None)
			{ // User specifies no border. Skip everything related to border.
				do {
					if(++declIter == declIterEnd) goto END;
				} while (declIter->first <= PT_BorderRadius);
			}
		}

		if(declIter->first <= PT_BorderRadius)
		{
			bt = testBorderObjectType(declIter->second->values,declIter,declIterEnd);

			DeclMap::iterator declIter2 = declIter;
			if(bt == BT_Simple)
			{
				while(declIter2 != declIterEnd && declIter2->first <= PT_BorderStyles)
					++declIter2;
				SimpleBorderRenderObject* obj = new SimpleBorderRenderObject();
				renderRule->borderRO = obj;
				setSimpleBorderRO(obj,declIter,declIter2);
			} else if(bt == BT_Radius)
			{
				while(declIter2 != declIterEnd && declIter2->first <= PT_BorderStyles)
					++declIter2;
				RadiusBorderRenderObject* obj = new RadiusBorderRenderObject();
				renderRule->borderRO = obj;
				setSimpleBorderRO(obj,declIter,declIter2);
				obj->radius = declIter->second->values.at(0).data.vuint;
			} else {
				while(declIter2 != declIterEnd && declIter2->first <= PT_BorderRadius)
					++declIter2;
				ComplexBorderRenderObject* obj = new ComplexBorderRenderObject();
				renderRule->borderRO = obj;
				setComplexBorderRO(obj, declIter, declIterEnd);
			}

			if(declIter == declIterEnd) goto END;
		}

		// --- Margin & Padding --- 
		while(declIter->first <= PT_PaddingLeft)
		{
			std::vector<CssValue>& values = declIter->second->values;
			switch(declIter->first)
			{
			case PT_Margin:
				setD2DRectValue(renderRule->margin,values);
				renderRule->hasMargin = true;
				break;
			case PT_MarginTop:
			case PT_MarginRight:
			case PT_MarginBottom:
			case PT_MarginLeft:
				setD2DRectValue(renderRule->margin,declIter->first - PT_MarginTop, values.at(0).data.vuint);
				renderRule->hasMargin = true;
				break;
			case PT_Padding:
				setD2DRectValue(renderRule->padding,values);
				renderRule->hasPadding = true;
				break;
			case PT_PaddingTop:
			case PT_PaddingRight:
			case PT_PaddingBottom:
			case PT_PaddingLeft:
				setD2DRectValue(renderRule->padding,declIter->first - PT_PaddingTop, values.at(0).data.vuint);
				renderRule->hasPadding = true;
				break;
			}

			if(++declIter == declIterEnd) goto END;
		}

		// --- Geometry ---
		if(declIter->first <= PT_MaximumHeight)
		{
			GeometryRenderObject* geoData = new GeometryRenderObject();
			renderRule->geoRO = geoData;
			do {
				int data = declIter->second->values.at(0).data.vint;
				switch(declIter->first)
				{
				case PT_Width:        geoData->width     = data; break;
				case PT_Height:       geoData->height    = data; break;
				case PT_MinimumWidth: geoData->minWidth  = data; break;
				case PT_MinimumHeight:geoData->minHeight = data; break;
				case PT_MaximumWidth: geoData->maxWidth  = data; break;
				case PT_MaximumHeight:geoData->maxHeight = data; break;
				}
			}while(++declIter != declIterEnd);
			if(++declIter == declIterEnd) goto END;
		}

		// --- Cursor ---
		if(declIter->first == PT_Cursor)
		{
			MCursor* cursor = new MCursor();
			const CssValue& value = declIter->second->values.at(0);
			switch(value.type)
			{
				case CssValue::Identifier:
					if(value.data.videntifier < Value_Default || value.data.videntifier > Value_Blank)
					{
						delete cursor;
						cursor = 0;
					} else {
						cursor->setType(static_cast<MCursor::CursorType>
							(value.data.videntifier - Value_Default));
					}
					break;
				case CssValue::Uri:
					cursor->loadCursorFromFile(*value.data.vstring);
					break;
				case CssValue::Number:
					cursor->loadCursorFromRes(MAKEINTRESOURCEW(value.data.vint));
					break;
				default:
					delete cursor;
					cursor = 0;
			}
			renderRule->cursor = cursor;
			if(++declIter == declIterEnd) goto END;
		}

		// --- Text ---
		if(declIter->first <= PT_TextShadow)
		{
			std::wstring fontFace = L"Arial";
			bool bold = false;
			bool italic = false;
			bool pixelSize = false;
			unsigned int size = 12;
			while(declIter != declIterEnd && declIter->first <= PT_FontWeight)
			{
				const CssValue& value = declIter->second->values.at(0);
				switch(declIter->first)
				{
					case PT_Font:
						{
							const CssValueArray& values = declIter->second->values;
							for(int i = values.size() - 1; i >= 0; --i)
							{
								const CssValue& v = values.at(i);
								if(v.type == CssValue::String)
									fontFace = *v.data.vstring;
								else if(v.type == CssValue::Identifier) {
									if(v.data.videntifier == Value_Bold)
										bold = true;
									else if(v.data.videntifier == Value_Italic || 
										v.data.videntifier == Value_Oblique)
										italic = true;
								} else {
									size = v.data.vuint;
									pixelSize = v.type == CssValue::Length;
								}
							}

						}
						break;
					case PT_FontSize:
						size = value.data.vuint;
						// If the value is 12px, the CssValue is Length.
						// If the value is 16, the CssValue is Number.
						// We don't support unit 'pt'.
						pixelSize = (value.type == CssValue::Length);
						break;
					case PT_FontStyle: italic = (value.data.videntifier != Value_Normal); break;
					case PT_FontWeight:  bold = (value.data.videntifier == Value_Bold);   break;
				}
				++declIter;
			}

			MFont font(size,fontFace, bold, italic, pixelSize);
			TextRenderObject* textRO = new TextRenderObject(font);
			renderRule->textRO = textRO;
			if(declIter == declIterEnd) goto END;

			while(declIter != declIterEnd /*&& declIter->first <= PT_TextShadow*/)
			{
				const CssValueArray& values = declIter->second->values;
				switch(declIter->first)
				{
					case PT_Color:
						textRO->color = values.at(0).getColor();
						break;
					case PT_TextShadow:
						if(values.size() < 2)
							break;
						textRO->shadowOffsetX = (char)values.at(0).data.vint;
						textRO->shadowOffsetY = (char)values.at(1).data.vint;
						for(size_t i = 2; i < values.size(); ++i)
						{
							if(values.at(i).type == CssValue::Color)
								textRO->shadowColor = values.at(i).getColor();
							else
								textRO->shadowBlur = values.at(i).data.vuint;
						}
						break;
					case PT_TextOutline:
						textRO->outlineWidth = (char)values.at(0).data.vuint;
						textRO->outlineColor = values.at(values.size() - 1).getColor();
						if(values.size() >= 3)
							textRO->outlineBlur = (char)values.at(1).data.vuint;
						break;
					case PT_TextAlignment:
						if(values.size()>1)
						{
							textRO->alignY = values.at(1).data.videntifier;
							textRO->alignX = values.at(0).data.videntifier;
						} else {
							ValueType vt = values.at(0).data.videntifier;
							if(vt == Value_Center)
								textRO->alignX = textRO->alignY = vt;
							else if(vt == Value_Top || vt == Value_Bottom)
								textRO->alignY = vt;
							else
								textRO->alignX = vt;
						}
						break;
					case PT_TextDecoration:
						textRO->decoration = values.at(0).data.videntifier; break;
					case PT_TextUnderlineStyle:
						textRO->lineStyle  = values.at(0).data.videntifier; break;
					case PT_TextOverflow:
						textRO->overflow   = values.at(0).data.videntifier; break;
				}

				++declIter;
			}
		}

		END:// Check if this RenderRule is opaque.
		int opaqueCount = 0;
		bool opaqueBorder = (!renderRule->hasMargin && bt == BT_Simple);
		for(int i = bgOpaqueTypes.size() - 1; i >= 0 && opaqueCount == 0; --i)
		{
			switch(bgOpaqueTypes.at(i)) {
				case OpaqueClipMargin:
					++opaqueCount;
					break;
				case OpaqueClipBorder:
					if(opaqueBorder)
						++opaqueCount;
					break;
				case OpaqueClipPadding:
					if(opaqueBorder && !renderRule->hasPadding)
						++opaqueCount;
					break;
			}
		}
		if(opaqueBorderImage) ++opaqueCount;
		renderRule->opaqueBackground = opaqueCount > 0;
	}
	
} // namespace MetalBone







































/*
#ifdef MB_DEBUG
#include <string>
#include <sstream>
using namespace MetalBone;
using namespace MetalBone::CSS;

void MResource::dumpCache() {
	ResourceCache::iterator iter = cache.begin();
	ResourceCache::iterator iterEnd = cache.end();
	while(iter != iterEnd)
	{
		std::wstringstream sss;
		sss << L"=== " << iter->first << L"\n  Size: " << iter->second->length << L" bytes\n";
		OutputDebugStringW(sss.str().c_str());
		dumpMemory(iter->second->buffer,iter->second->length);
		++iter;
	}
}

void printPseudo(const BasicSelector* bs) {
	for(unsigned int i = 0; i < bs->pseudoCount; ++i) {
		int j = 0;
		for(; j < knownPseudoCount; ++j) {
			if(pseudos[j].value & bs->pseudo) {
				std::wstringstream s;
				s << "||        - Pseudo:           " << pseudos[j].name << '\n';
				OutputDebugStringW(s.str().c_str());
				break;
			}
		}
		if(j == knownPseudoCount)
			OutputDebugStringA("||        - Pseudo:           Error Parsing\n");
	}
}

void printPseudoCollection(unsigned int pcollection) {
	if(pcollection == 0)
	{
		OutputDebugStringA("||  - Pseudo:\tNone\n");
		return;
	}
	for(int j = 0; j < knownPseudoCount; ++j) {
		if(pseudos[j].value & pcollection) {
			std::wstringstream s;
			s << "||  - Pseudo:\t" << pseudos[j].name << '\n';
			OutputDebugStringW(s.str().c_str());
		}
	}
}

void printDeclaration(const Declaration* del) {
	for(int i = 0; i < knownPropertyCount; ++i) {
		if(properties[i].value == del->property) {
			std::wstringstream s;
			s << "||      + Property:         " << properties[i].name << '\n';
			OutputDebugStringW(s.str().c_str());
			break;
		}
	}
	for(unsigned int k = 0; k < del->values.size(); ++k) {
		CssValue val = del->values.at(k);
		if(val.type == CssValue::Color) {
			unsigned int color = val.data.vuint;
			std::stringstream s;
			s << std::hex << (color >> 8);

			std::stringstream ss;
			ss << "||        - Value(Color):     #" << s.str().c_str()
			   << "\trgba(" << (color >> 24) << ','
			   << ((color >> 16) & 0xFF) << ','
			   << ((color >> 8) & 0xFF) << ','
			   << (color & 0xFF) << ")\n";
			OutputDebugStringA(ss.str().c_str());

		}else if(val.type == CssValue::Uri)
		{
			std::wstringstream s;
			s << L"||        - Value(Uri):       " << val.data.vstring->c_str() << L'\n';
			OutputDebugStringW(s.str().c_str());
		}else if(val.type == CssValue::Length)
		{
			std::stringstream s;
			s << "||        - Value(Length):    " << val.data.vint << "px\n";
			OutputDebugStringA(s.str().c_str());
		}else if(val.type == CssValue::Number)
		{
			std::stringstream s;
			s << "||        - Value(Number):    " << val.data.vint << "px\n";
			OutputDebugStringA(s.str().c_str());
		}else if(val.type == CssValue::Identifier) {
			for(int i = 0;i<KnownValueCount - 1;++i) {
				if(knownValues[i].value == (int)val.data.vuint) {
					std::wstringstream s;
					s << "||        - Value(Ident):     " << knownValues[i].name << '\n';
					OutputDebugStringW(s.str().c_str());
					break;
				}
			}
		}
	}
}
void outputStyleRule(const StyleRule* rule)
{
	std::vector<Selector*>::const_iterator selIter = rule->selectors.begin();
	std::vector<Selector*>::const_iterator selIterEnd = rule->selectors.end();
	int index2 = 0;
	while(selIter != selIterEnd)
	{
		std::stringstream s;
		s << "||    + Selector            #" << index2 << " \n";
		OutputDebugStringA(s.str().c_str());

		std::vector<BasicSelector*>::const_iterator bsIter = (*selIter)->basicSelectors.begin();
		std::vector<BasicSelector*>::const_iterator bsIterEnd = (*selIter)->basicSelectors.end();
		int index3 = 0;
		while(bsIter != bsIterEnd)
		{
			std::wstringstream s;
			s << "||      + BasicSel          #" << index3 << '\n';
			s << "||        - Element:        " << (*bsIter)->elementName.c_str() << '\n';
			OutputDebugStringW(s.str().c_str());

			if(!(*bsIter)->id.empty())
			{
				std::wstringstream s;
				s << "||        - ID:             " << (*bsIter)->id.c_str() << '\n';
				OutputDebugStringW(s.str().c_str());
			}
			printPseudo(*bsIter);

			++index3;
			++bsIter;
		}

		++index2;
		++selIter;
	}

	OutputDebugStringA("||    + Declarations:\n");
	std::vector<Declaration*>::const_iterator deIter = rule->declarations.begin();
	std::vector<Declaration*>::const_iterator deIterEnd = rule->declarations.end();
	while(deIter != deIterEnd)
	{
		printDeclaration(*deIter);
		++deIter;
	}
}

void outputStyleRules(std::vector<StyleRule*>& srs)
{
	std::vector<StyleRule*>::iterator iter = srs.begin();
	std::vector<StyleRule*>::iterator iterEnd = srs.end();
	int index = 0;
	while(iter != iterEnd)
	{
		std::stringstream s;
		s << "==================\n+++ StyleRule #" << index << '\n';
		OutputDebugStringA(s.str().c_str());

		outputStyleRule(*iter);
		++iter;
		++index;
	}
}

void printStyleSheet(StyleSheet* sheet)
{
	OutputDebugStringA("|| ^^^^^^^^^^^^^^^^^ StyleRules Universal ^^^^^^^^^^^^^^^^^ ||\n");
	outputStyleRules(sheet->universal);
	OutputDebugStringA("|| ################# StyleRules Universal ################# ||\n|\n");

	OutputDebugStringA("|| ^^^^^^^^^^^^^^^^ StyleRules ClassId Map ^^^^^^^^^^^^^^^^ ||\n");
	StyleSheet::StyleRuleIdMap::const_iterator iter = sheet->srIdMap.begin();
	StyleSheet::StyleRuleIdMap::const_iterator iterEnd = sheet->srIdMap.end();
	while(iter != iterEnd)
	{
		std::wstringstream s;
		s << L"||  + Id: " << iter->first.c_str() << '\n';
		OutputDebugStringW(s.str().c_str());
		outputStyleRule(iter->second);
		++iter;
	}
	OutputDebugStringA("|| ################ StyleRules ClassId Map ################ ||\n|\n");

	OutputDebugStringA("|| ^^^^^^^^^^^^^^^^ StyleRules Element Map ^^^^^^^^^^^^^^^^ ");
	StyleSheet::StyleRuleElementMap::const_iterator iter2 = sheet->srElementMap.begin();
	StyleSheet::StyleRuleElementMap::const_iterator iterEnd2 = sheet->srElementMap.end();
	while(iter2 != iterEnd2)
	{
		std::wstringstream s;
		s << L"||\n||  + Element( Class ):     " << iter2->first.c_str() << '\n';
		OutputDebugStringW(s.str().c_str());

		outputStyleRule(iter2->second);
		++iter2;
	}
	OutputDebugStringA("|| ################ StyleRules Element Map ################ ||\n");
}

void MStyleSheetStyle::dumpStyleSheet()
{
	OutputDebugStringA("|****************** Application StyleSheet ******************|\n|\n");
	printStyleSheet(appStyleSheet);
	OutputDebugStringA("|****************** Application StyleSheet ******************|\n\n\n");

	WidgetSSCache::const_iterator wssIt = widgetSSCache.begin();
	WidgetSSCache::const_iterator wssItEnd = widgetSSCache.end();
	OutputDebugStringA("\n|******************** Widgets StyleSheet ********************|\n");
	while(wssIt != wssItEnd)
	{
		std::stringstream s;
		s << "+++++ Widget: " << wssIt->first->objectName().c_str() << '\n';
		OutputDebugStringA(s.str().c_str());
		printStyleSheet(wssIt->second);
		++wssIt;
	}
	OutputDebugStringA("|******************** Widgets StyleSheet ********************|\n\n\n");

	OutputDebugStringA("\n|****************** Widget StyleRule Cache ******************|\n");
	WidgetStyleRuleMap::const_iterator srIt = widgetStyleRuleCache.begin();
	WidgetStyleRuleMap::const_iterator srItEnd = widgetStyleRuleCache.end();
	while(srIt != srItEnd)
	{
		std::wstringstream s;
		std::wstring name;
		getWidgetClassName(srIt->first,name);
		s << "|| < " << name;
		if(!(srIt->first->objectName().empty()))
			s << " , " << srIt->first->objectName().c_str();
		s <<" >\n";
		OutputDebugStringW(s.str().c_str());

		MatchedStyleRuleVector::const_iterator it = srIt->second.begin();
		MatchedStyleRuleVector::const_iterator itEnd = srIt->second.end();
		while(it != itEnd)
		{
			outputStyleRule(it->styleRule);
			++it;
		}
		++srIt;
	}
	OutputDebugStringA("\n|****************** Widget StyleRule Cache ******************|\n\n\n");

	OutputDebugStringA("\n|****************** Widget RenderRule Cache *****************|\n");
	WidgetRenderRuleMap::iterator  soIter = widgetRenderRuleCache.begin();
	WidgetRenderRuleMap::iterator  soIterEnd = widgetRenderRuleCache.end();
	while(soIter != soIterEnd)
	{
		std::wstringstream s;
		std::wstring className;
		getWidgetClassName(soIter->first,className);
		s << L"|| < " << className << L':' << soIter->first->objectName().c_str() << L" >\n";
		OutputDebugStringW(s.str().c_str());

		PseudoRenderRuleMap* prrmap = soIter->second;
		PseudoRenderRuleMap::Element::const_iterator soMapIter = prrmap->element.begin();
		PseudoRenderRuleMap::Element::const_iterator soMapIterEnd = prrmap->element.end();
		while(soMapIter != soMapIterEnd)
		{
//			printPseudoCollection(soMapIter->first);
//			StyleObject::DeclarationMap::const_iterator soDeclIter = soMapIter->second.declarations.begin();
//			StyleObject::DeclarationMap::const_iterator soDeclIterEnd =
//					soMapIter->second.declarations.end();
//			while(soDeclIter != soDeclIterEnd)
//			{
//				printDeclaration(soDeclIter->second);
//				++soDeclIter;
//			}
			++soMapIter;
		}
		++soIter;
	}
	OutputDebugStringA("\n|***************** Widget StyleObject Cache *****************|\n\n\n");
}
#endif
*/
