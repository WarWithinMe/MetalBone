#include "CSSParser.h"
#include <string>
#include <sstream>

namespace MetalBone
{
	namespace CSS
	{
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
			{ L"single-loop",  Value_SingleLoop  },
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
	}
}

using namespace MetalBone::CSS;
const CSSValuePair* findCSSValue(const std::wstring& p, const CSSValuePair values[], int valueCount)
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
	for (size_t i = 0; i < basicSelectors.size(); ++i) {
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

StyleSheet::~StyleSheet()
{
	size_t length = styleRules.size();
	for(size_t i = 0; i < length; ++i)
		delete styleRules.at(i);
}

StyleRule::~StyleRule()
{
	size_t length = selectors.size();
	for(size_t i = 0; i < length; ++i)
		delete selectors.at(i);

	length = declarations.size();
	for(size_t i = 0; i < length; ++i)
		delete declarations.at(i);
}

Selector::~Selector()
{
	size_t length = basicSelectors.size();
	for(size_t i = 0;i < length; ++i)
		delete basicSelectors.at(i);
}

Declaration::~Declaration()
{
	size_t length = values.size();
	for(size_t i = 0; i< length; ++i) {
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
		else { break; }
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
	if(crItem) {
		pseudo |= crItem->value;
		++pseudoCount;
	}
	p.clear();
}

// Background:      { Brush/Image frame-count Repeat Clip Alignment pos-x pos-y width height }*;
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
		} else { ++bufferLength; }

		++index;
	}
}