#include "CSSParser.h"
#include "MStyleSheet.h"
#include "MD2DPaintContext.h"
#include <string>
#include <sstream>

namespace MetalBone
{
	namespace CSS
	{
        struct CSSValuePair { const wchar_t* name; unsigned int value; };
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
			{ L"width",                      PT_Width                   },
			{ L"x",                          PT_PosX                    },
			{ L"y",                          PT_PosY                    }
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
using namespace std;
const CSSValuePair* findCSSValue(const wstring& p, const CSSValuePair values[], int valueCount)
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
                break;
            case CssValue::LiearGradient:
                delete values.at(i).data.vlinearGradient;
                break;
		    default: break;
		}
	}
}

inline void MCSSParser::skipWhiteSpace()
{
    while(pos < cssLength && iswspace(css->at(pos)))
        ++pos;
}

// if meet "/*", call this function to make pos behind "*/"
void MCSSParser::skipComment()
{
    int lengthMinus = cssLength - 1;
    while(pos < lengthMinus)
    {
        if(css->at(pos) == L'*' && css->at(pos + 1) == L'/')
        {
            pos += 2;
            break;
        }
        ++pos;
    }
    skipWhiteSpace();
}

void MCSSParser::skipComment2()
{
    int lengthMinus = cssLength - 1;
    while(pos < lengthMinus && css->at(pos) == L'/')
    {
        ++pos;
        if(css->at(pos) != L'*') continue;

        ++pos;
        while(pos < lengthMinus)
        {
            if(css->at(pos) == L'*' &&
                css->at(pos + 1) == L'/')
            { pos+=2; break; }
            ++pos;
        }
    }
    skipWhiteSpace();
}

StyleSheet* MCSSParser::parse(const wstring& sourceCSS)
{
    wstring css_copy;
    pos = sourceCSS.size() - 1;
    css = &sourceCSS;
    // Test if Declaration block contains Selector
    while(pos > 0) {
        if(iswspace(sourceCSS.at(pos))) { --pos; continue;}

        if(sourceCSS.at(pos) == L'/' && sourceCSS.at(pos - 1) == L'*')
        {
            pos -= 2;
            while(pos > 0)
            {
                if(sourceCSS.at(pos) == L'*' && sourceCSS.at(pos-1) == L'/')
                { pos -= 2; break; }
                --pos;
            }
            continue;
        }

        if(sourceCSS.at(pos) != L'}')
        {
            css_copy.append(L"*{");
            css_copy.append(sourceCSS);
            css_copy.append(1, L'}');
            css = &css_copy;
        }
        break;
    }

    StyleSheet* ss = new StyleSheet();
	int order      = pos = 0;
    cssLength      = css->size();

	while(pos < cssLength)
	{
		StyleRule* newStyleRule = new StyleRule(order);

		// If we successfully parsed Selector, we then parse Declaration.
		// If we failed to parse Declaration, the Declaration must be empty,
		// so that we can delete the created StyleRule.
		if(parseSelector(newStyleRule))
			parseDeclaration(newStyleRule);

		if(newStyleRule->declarations.empty()) 
        {
			delete newStyleRule;
            continue;
		}

		const vector<Selector*>& sels = newStyleRule->selectors;
		for(size_t i = 0; i < sels.size(); ++i)
		{
			const Selector* sel     = sels.at(i);
			const BasicSelector* bs = sel->basicSelectors.at(sel->basicSelectors.size() - 1);

			if(!bs->id.empty()) {
				ss->srIdMap.insert(StyleSheet::StyleRuleIdMap::value_type(bs->id, newStyleRule));
                continue;
			}

            if(bs->elementName.empty()) {
				ss->universal.push_back(newStyleRule);
                continue;
            }

			ss->srElementMap.insert(StyleSheet::
                StyleRuleElementMap::value_type(bs->elementName,newStyleRule));
		}

		ss->styleRules.push_back(newStyleRule);
		++order;
	}

    return ss;
}

bool MCSSParser::parseSelector(StyleRule* sr)
{
	skipWhiteSpace();

	int     crSelectorStartIndex = pos;
	int     cssLengthMinus       = cssLength - 1;
	wstring commentBuffer;
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
			} else
				sel = new Selector(css,crSelectorStartIndex,posBeforeEndingSpace - crSelectorStartIndex);
			sr->selectors.push_back(sel);
			break;

		}

        if(byte == L'/' && css->at(pos + 1) == L'*')
        {
            commentBuffer.append(*css, crSelectorStartIndex, pos-crSelectorStartIndex);
            skipComment();
            if(commentBuffer.size() == 0)
                skipWhiteSpace();
            crSelectorStartIndex = pos;
            continue;
        }

        if(byte == L',')
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
			} else
				sel = new Selector(css,crSelectorStartIndex,posBeforeEndingSpace - crSelectorStartIndex);
			sr->selectors.push_back(sel);

			++pos;
			skipWhiteSpace();
			crSelectorStartIndex = pos;
		}
		++pos;
	}

	return (pos < cssLength);
}

Selector::Selector(const wstring* css, int index, int length)
{
    if(length == -1) { length = css->size(); }
	if(length ==  0)
	{
		BasicSelector* sel = new BasicSelector();
		sel->relationToNext = BasicSelector::NoRelation;
		basicSelectors.push_back(sel);
	}

    length += index;

	while(index < length)
	{
		BasicSelector* sel = new BasicSelector();
		basicSelectors.push_back(sel);

		wstring  pseudo;
		wstring* buffer = &(sel->elementName);
		bool     newBS  = false;

		while(index < length)
		{
			wchar_t byte = css->at(index);
			if(iswspace(byte))
			{
				while((++index) < length)
                {
					if(css->at(index) == L':')    { --index; break; }
					if(!iswspace(css->at(index))) { newBS = true; --index; break; }
				}
			} else if(byte == L'>')
			{
				newBS = true;
				sel->relationToNext = BasicSelector::MatchNextIfParent;
			} else
			{
				if(newBS)
				{
					if(!pseudo.empty())
						sel->addPseudoAndClearInput(pseudo);
					break;
				}

				if(byte == L'#') {
					buffer = &(sel->id);// And skip '#';
                } else if(byte == L':')
				{
					if(!pseudo.empty())
						sel->addPseudoAndClearInput(pseudo);

					buffer = &pseudo; // And Skip ':';
					while((++index) < length)
                    {
						if(!iswspace(css->at(index)))
                            { --index; break; }
					}
				} else if(byte != L'*') // Don't add (*)
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
	const CSSValuePair* crItem = findCSSValue(p, pseudos, knownPseudoCount);
    p.clear();

	if(crItem) {
		pseudo |= crItem->value;
		++pseudoCount;
	}
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
// Gradient:        linear-gradient(left|right top|bottom, color position,...)
// Repeat:          repeat | repeat-x | repeat-y | no-repeat
// Clip/Origin:     padding | border | content | margin
// Image:           url(filename)
// Alignment:       (left | top | right | bottom | center){0,2}
// LineStyle:       dashed | dot-dash | dot-dot-dash | dotted | solid | wave
// Length:          Number // do not support unit and the default unit is px

void MCSSParser::parseDeclaration(StyleRule* sr)
{
    ++pos; // skip '{'
    skipWhiteSpace();
    skipComment2();

    int selectorStartIndex = pos;
    int selectorEndIndex   = 0;

    while(pos < cssLength)
    {
        // property
        wchar_t byte = css->at(pos);
        if(byte == L'/' || iswspace(byte))
        {
            selectorEndIndex = pos - 1;
            while(pos < cssLength)
            {
                byte = css->at(pos);
                if(byte == L':') break;
                ++pos;
            }
        }

        if(byte == L':')
        {
            const CSSValuePair* crItem = 0;
            size_t end = selectorEndIndex == 0 ? pos : selectorEndIndex + 1;
            wstring propName(*css, selectorStartIndex, end - selectorStartIndex);
            selectorEndIndex = 0;
            crItem = findCSSValue(propName, properties, knownPropertyCount);

            ++pos;

            if(crItem == 0)
            {
                while(pos < cssLength) // Unknown Property, Skip to Next.
                {
                    byte = css->at(pos);
                    if(byte == L';')
                    {
                        ++pos;
                        skipWhiteSpace();
                        skipComment2();
                        selectorStartIndex = pos;
                    } else if(byte == L'}')
                        break;
                    ++pos;
                }
            } else
            {
                skipWhiteSpace();
                skipComment2();
                Declaration* d = new Declaration((PropertyType)crItem->value);
                pos = d->addValue(*css, pos, cssLength);

                // If we don't have valid values for the declaration.
                // We delete the declaration.
                if(d->values.empty()) { delete d; }
                else { sr->declarations.push_back(d); }

                if(pos < cssLength && css->at(pos) == L';')
                {
                    ++pos;
                    skipWhiteSpace();
                    skipComment2();
                    selectorStartIndex = pos;
                }
                if(pos < cssLength && css->at(pos) == L'}')
                    break;
            }
        } else if(byte == L'}')
            break;

        ++pos;
    }

    ++pos; // skip '}'
}

static int simplySplitAndIngoreWS(const wstring& text, int index,
    int length, wchar_t delemiter, wchar_t endByte, vector<wstring>& output)
{
    int tokenStartIndex = index;
    int tokenEndIndex   = 0;

    while(index < length)
    {
        wchar_t byte = text.at(index);
        if(byte == delemiter || byte == endByte)
        {
            tokenEndIndex = index - 1;
            while(iswspace(text.at(tokenEndIndex)) &&
                tokenEndIndex >= tokenStartIndex)
            { --tokenEndIndex; }
            output.push_back(wstring(text,tokenStartIndex,tokenEndIndex - tokenStartIndex + 1));

            if(byte == endByte) return index;

            ++index;
            while(iswspace(text.at(index)))
                ++index;
            tokenStartIndex = index;
            tokenEndIndex   = 0;
        }
        ++index;
    }

    if(tokenStartIndex < length)
        output.push_back(wstring(text,tokenStartIndex,index - tokenStartIndex + 1));

    return index;
}

LinearGradientData* parseLinearGradient(const wstring& css, int& index, int length)
{
    while(iswspace(css.at(index)))
        ++index;

    int tokenStartIndex = index;
    int tokenEndIndex   = 0;
    vector<wstring> tokens;
    index = simplySplitAndIngoreWS(css,index,length,L',',L')',tokens);

    int checkingToken = 0;

    if(*tokens.at(checkingToken).c_str() == L'#')
        return 0; // No point defined.
    if(*tokens.at(tokens.size()-1).c_str() != L'#')
        return 0; // No color defined.
    if(tokens.size() < 2)
        return 0;

    LinearGradientData* data = new LinearGradientData();

    vector<wstring> coor;
    const wstring& point = tokens.at(checkingToken);
    simplySplitAndIngoreWS(point,0,point.size(),L' ',L'\0',coor);
    if(coor.size() == 1) coor.push_back(wstring(L"0"));

    checkingToken = 1;
    if(*tokens.at(checkingToken).c_str() != L'#')
    {
        const wstring& endPoint = tokens.at(checkingToken);
        simplySplitAndIngoreWS(endPoint,0,endPoint.size(),L' ',L'\0',coor);
        checkingToken = 2;
    }

    for(int i = coor.size() - 1; i >=0; --i)
    {
        wstring& pos = coor.at(i);

        if(iswdigit(pos.at(0)))
        {
            if(pos.at(pos.size()-1) == L'%')
                data->setPosType((LinearGradientData::PosEnum)i, true);
            data->pos[i] = _wtoi(pos.c_str());
        } else if(pos == L"right" || pos == L"bottom")
        {
            data->pos[i] = 100;
            data->setPosType((LinearGradientData::PosEnum)i, true);
        }
    }

    vector<wstring> colors;
    for(int i = checkingToken; i < tokens.size(); ++i)
    {
        vector<wstring> temp;
        simplySplitAndIngoreWS(tokens.at(i),0,tokens.at(i).size(),L' ',L'\0',temp);
        colors.push_back(temp.at(0));
        if(temp.size() < 2)
            colors.push_back(wstring(L"0%"));
        else
            colors.push_back(temp.at(1));
    }

    data->stopCount = colors.size() / 2;
    data->stops = new LinearGradientData::GradientStop[data->stopCount];

    for(int i = 0; i < data->stopCount; ++i)
    {
        wstringstream hexStream;
        wstring color(colors.at(i*2),1,colors.at(i*2).size() - 1);
        hexStream << L"0x" << color;
        unsigned int h;
        hexStream >> std::hex >> h;
        data->stops[i].argb = (h > 0xFFFFFF ? h : (h | 0xFF000000));
        data->stops[i].pos  = float(_wtoi(colors.at(i*2+1).c_str())) / 100;
    }

    return data;
}

// Comment cannot be in the middle of a expression list.
// e.g. *{ aaa: /*comments*/ aaa bbb ccc; } is legal
// but  *{ aaa: bbb /*comments*/ ccc ddd; } is not legal
int Declaration::addValue(const wstring& css, int index, int length)
{
    int  valueStartIndex  = index;
    int  valueEndIndex    = 0;
    bool isString         = false;
    bool atEnd            = false;
    while(index < length)
    {
        wchar_t byte = css.at(index);
        if(byte == L';' || byte == L'}')
        {
            valueEndIndex = index - 1;
            atEnd = true;
        } else if(byte == L'"')
        {
            if(isString) {
                valueEndIndex = index - 1;
                ++index;
                while(index < length && iswspace(css.at(index)))
                    ++index;
            }
            isString = !isString;
        } else if(!isString && iswspace(byte))
        {
            valueEndIndex = index - 1;
            ++index;
            while(index < length && iswspace(css.at(index)))
                ++index;
        } else if(byte == L'(') // Functions
        { 
            wstring funcName(css, valueStartIndex, index - valueStartIndex);
            if(funcName == L"url")
            {
                ++index;
                int startIndex = index;
                while(index < length && css.at(index) != L')')
                    ++index;

                CssValue value;
                value.setString(new wstring(css, startIndex, index - startIndex));
                value.setType(CssValue::Uri);
                values.push_back(value);
            } else if(funcName == L"rgba" || funcName == L"rgb")
            {
                wstring buffer;
                unsigned int color = 0xFF000000;
                int shift = 16;
                ++index;
                while(index < length)
                {
                    byte = css.at(index);
                    if(byte == L',')
                    {
                        unsigned int c = _wtoi(buffer.c_str());
                        color |= c << shift;
                        shift -= 8;
                        buffer.clear();
                    } else if(byte == L')') {
                        break;
                    } else if(!iswspace(byte)) {
                        buffer.append(1, byte);
                    }
                    ++index;
                }

                if(shift == -8) // alpha
                {
                    float f;
                    wstringstream stream;
                    stream << buffer;
                    stream >> f;
                    unsigned int opacity = (int)(f * 255) << 24;
                    color |= opacity;
                } else { // blue
                    color |= _wtoi(buffer.c_str());
                }

                CssValue value;
                value.setUInt(color);
                value.setType(CssValue::Color);
                values.push_back(value);
            } else if(funcName == L"linear-gradient")
            {
                ++index;
                LinearGradientData* lgd = parseLinearGradient(css, index, length);
                if(lgd != 0)
                {
                    CssValue value;
                    value.setLinearGradient(lgd);
                    value.setType(CssValue::LiearGradient);
                    values.push_back(value);
                }
                
            } else
            {
                // Unknow function
                while(index < length && css.at(index) != L')')
                    ++index;
            }

            ++index;
            while(index < length && iswspace(css.at(index)))
                ++index;
            valueEndIndex   = 0;
            valueStartIndex = index;
            continue;
        }

        if(valueEndIndex != 0 && valueStartIndex <= valueEndIndex)
        {
            wchar_t begin = css.at(valueStartIndex);
            CssValue value;
            // Find a single value.
            if(begin == L'#')
            {
                // Hex Color
                ++valueStartIndex;
                wstring hexColor(css, valueStartIndex, valueEndIndex - valueStartIndex + 1);
                wstringstream hexStream;
                hexStream << L"0x" << hexColor;
                unsigned int h;
                hexStream >> std::hex >> h;

                value.setType(CssValue::Color);
                value.setUInt((h > 0xFFFFFF ? h : (h | 0xFF000000)));
            } else if(iswdigit(begin) || begin == L'-')
            {
                // Number, Length
                if(valueEndIndex - valueStartIndex > 1 &&
                   css.at(valueEndIndex) == L'x' &&
                   css.at(valueEndIndex - 1) == L'p')
                {
                    wstring length(css, valueStartIndex, valueEndIndex - 1 - valueStartIndex);
                    value.setType(CssValue::Length);
                    value.setInt(_wtoi(length.c_str()));
                } else
                {
                    wstring length(css, valueStartIndex, valueEndIndex - valueStartIndex + 1);
                    value.setType(CssValue::Number);
                    value.setInt(_wtoi(length.c_str()));
                }
            } else if(begin == L'"')
            {
                // String
                value.setType(CssValue::String);
                value.setString(new wstring(css, valueStartIndex + 1, valueEndIndex - valueStartIndex));
            } else
            {
                // Indentifier, Color, String
                wstring buffer(css, valueStartIndex, valueEndIndex - valueStartIndex + 1);
                const CSSValuePair* crItem = findCSSValue(buffer, knownValues, KnownValueCount - 1);
                if(crItem != 0)
                {
                    if(crItem->value == Value_Transparent)
                    {
                        value.setType(CssValue::Color);
                        value.setColor(0);
                    } else {
                        value.setType(CssValue::Identifier);
                        value.setIdentifier(static_cast<ValueType>(crItem->value));
                    }
                } else
                {
                    value.setType(CssValue::String);
                    value.setString(new wstring(buffer));
                }
            }

            values.push_back(value);

            if(atEnd) break;

            valueEndIndex   = 0;
            valueStartIndex = index;
            continue;
        }

        if(atEnd) break;
        ++index;
    }
    return index;
}