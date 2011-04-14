#include "MResource.h"
#include "MStyleSheet.h"
#include "MWidget.h"
#include "MApplication.h"
#include "MBGlobal.h"

#include "3rd/XUnzip.h"

#include <D2d1helper.h>
#include <algorithm>
#include <wincodec.h>
#include <stdlib.h>
#include <typeinfo>
#include <sstream>
#include <list>
#include <map>

namespace MetalBone
{
	// ========== MResource ==========
	MResource::ResourceCache MResource::cache;
	bool MResource::open(HINSTANCE hInstance, const wchar_t* name, const wchar_t* type)
	{
		HRSRC hrsrc = FindResourceW(hInstance,name,type);
		if(hrsrc) {
			HGLOBAL hglobal = LoadResource(hInstance,hrsrc);
			if(hglobal) {
				buffer = (const BYTE*)LockResource(hglobal);
				if(buffer) {
					size = SizeofResource(hInstance,hrsrc);
					return true;
				}
			}
		}
		return false;
	}

	bool MResource::open(const std::wstring& fileName)
	{
		ResourceCache::const_iterator iter;
		if(fileName.at(0) == L':' && fileName.at(1) == L'/')
		{
			std::wstring temp(fileName,2,fileName.length() - 2);
			iter = cache.find(temp);
		} else {
			iter = cache.find(fileName);
		}
		if(iter == cache.cend())
			return false;

		buffer = iter->second->buffer;
		size   = iter->second->length;
		return true;
	}

	void MResource::clearCache()
	{
		ResourceCache::const_iterator iter = cache.cbegin();
		ResourceCache::const_iterator iterEnd = cache.cend();
		while(iter != iterEnd) { delete iter->second; ++iter; }
		cache.clear();
	}

	bool MResource::addFileToCache(const std::wstring& filePath)
	{
		if(cache.find(filePath) != cache.cend())
			return false;

		HANDLE resFile = CreateFileW(filePath.c_str(),GENERIC_READ,
									 FILE_SHARE_READ,0,OPEN_EXISTING,
									 FILE_ATTRIBUTE_NORMAL,0);
		if(resFile == INVALID_HANDLE_VALUE)
			return false;

		ResourceEntry* newEntry = new ResourceEntry();

		DWORD size = GetFileSize(resFile,0);
		newEntry->length = size;
		newEntry->unity  = true;
		newEntry->buffer = new BYTE[size];

		DWORD readSize;
		if(ReadFile(resFile,newEntry->buffer,size,&readSize,0))
		{
			if(readSize != size) {
				delete newEntry;
			} else {
				cache.insert(ResourceCache::value_type(filePath, newEntry));
				return true;
			}
		} else {
			delete newEntry;
		}
		return false;
	}

	bool MResource::addZipToCache(const std::wstring& zipPath)
	{
		HZIP zip = OpenZip((void*)zipPath.c_str(), 0, ZIP_FILENAME);
		if(zip == 0)
			return false;

		bool success = true;
		ZIPENTRYW zipEntry;
		if(ZR_OK == GetZipItem(zip, -1, &zipEntry))
		{
			std::vector<std::pair<int,ResourceEntry*> > tempCache;

			int totalItemCount = zipEntry.index;
			int buffer = 0;
			// Get the size of every item.
			for(int i = 0; i < totalItemCount; ++i)
			{
				GetZipItem(zip,i,&zipEntry);
				if(!(zipEntry.attr & FILE_ATTRIBUTE_DIRECTORY)) // ignore every folder
				{
					ResourceEntry* entry = new ResourceEntry();
					entry->buffer += buffer;
					entry->length = zipEntry.unc_size;

					cache.insert(ResourceCache::value_type(zipEntry.name,entry));
					tempCache.push_back(std::make_pair(i,entry));

					buffer += zipEntry.unc_size;
				}
			}

			BYTE* memBlock = new BYTE[buffer]; // buffer is the size of the zip file.

			// Uncompress every item.
			totalItemCount = tempCache.size();
			for(int i = 0; i < totalItemCount; ++i)
			{
				ResourceEntry* re = tempCache.at(i).second;
				re->buffer = memBlock + (int)re->buffer;

				UnzipItem(zip, tempCache.at(i).first,
						  re->buffer, re->length, ZIP_MEMORY);
			}

			tempCache.at(0).second->unity = true;
		} else {
			success = false;
		}

		CloseZip(zip);
		return success;
	}





	// ========== StyleSheet ==========
	namespace CSS
	{
		struct CssValue
		{
			enum  Type    { Unknown, Number, Length, Identifier, Uri, Color };
			union Variant {
				int vint;
				unsigned int vuint;
				std::wstring* vstring;
			};

			Type    type;
			Variant data;

			inline CssValue():type(Unknown) { data.vint = 0; }
			inline CssValue(const CssValue& rhs):type(rhs.type),data(rhs.data){}

			inline D2D_COLOR_F getColor()
			{
				M_ASSERT(type == Color);
				return D2D1::ColorF(data.vint >> 8, float(data.vint & 0xFF) / 256);
			}

			inline CssValue& operator=(const CssValue& rhs)
			{
				type = rhs.type;
				data = rhs.data;
				return *this;
			}
		};

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
				pseudo(0),pseudoCount(0),
				relationToNext(MatchNextIfAncestor){}

			void addPseudoAndClearInput(std::wstring& p);

			std::wstring elementName; // ClassA .ClassA
			std::wstring id; // #Id
			unsigned int pseudo;
			unsigned int pseudoCount;
			Relation     relationToNext;
		};

		struct Selector
		{
			Selector(const std::wstring* css,int index = 0,int length = -1);
			~Selector();

			std::vector<BasicSelector*> basicSelectors;
			int  specificity() const;
			bool matchPseudo(unsigned int) const;
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
			int value;
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
		{ L"background",                 PT_Background },
		{ L"background-alignment",       PT_BackgroundAlignment },
		{ L"background-clip",            PT_BackgroundClip },
		{ L"background-position",        PT_BackgroundPosition },
		{ L"background-repeat",          PT_BackgroundRepeat },
		{ L"background-size",            PT_BackgroundSize },
		{ L"border",                     PT_Border },
		{ L"border-bottom",              PT_BorderBottom },
		{ L"border-bottom-color",        PT_BorderBottomColor },
		{ L"border-bottom-left-radius",  PT_BorderBottomLeftRadius },
		{ L"border-bottom-right-radius", PT_BorderBottomRightRadius },
		{ L"border-bottom-style",        PT_BorderBottomStyle },
		{ L"border-bottom-width",        PT_BorderBottomWidth },
		{ L"border-color",               PT_BorderColor },
		{ L"border-image",               PT_BorderImage },
		{ L"border-left",                PT_BorderLeft },
		{ L"border-left-color",          PT_BorderLeftColor },
		{ L"border-left-style",          PT_BorderLeftStyle },
		{ L"border-left-width",          PT_BorderLeftWidth },
		{ L"border-radius",              PT_BorderRadius },
		{ L"border-right",               PT_BorderRight },
		{ L"border-right-color",         PT_BorderRightColor },
		{ L"border-right-style",         PT_BorderRightStyle },
		{ L"border-right-width",         PT_BorderRightWidth },
		{ L"border-style",               PT_BorderStyles },
		{ L"border-top",                 PT_BorderTop },
		{ L"border-top-color",           PT_BorderTopColor },
		{ L"border-top-left-radius",     PT_BorderTopLeftRadius },
		{ L"border-top-right-radius",    PT_BorderTopRightRadius },
		{ L"border-top-style",           PT_BorderTopStyle },
		{ L"border-top-width",           PT_BorderTopWidth },
		{ L"border-width",               PT_BorderWidth },
		{ L"color",                      PT_Color },
		{ L"font",                       PT_Font },
		{ L"font-family",                PT_FontFamily },
		{ L"font-size",                  PT_FontSize },
		{ L"font-style",                 PT_FontStyle },
		{ L"font-weight",                PT_FontWeight },
		{ L"height",                     PT_Height },
		{ L"inherit-background",         PT_InheritBackground },
		{ L"margin",                     PT_Margin },
		{ L"margin-bottom",              PT_MarginBottom },
		{ L"margin-left",                PT_MarginLeft },
		{ L"margin-right",               PT_MarginRight },
		{ L"margin-top",                 PT_MarginTop },
		{ L"max-height",                 PT_MaximumHeight },
		{ L"max-width",                  PT_MaximumWidth },
		{ L"min-height",                 PT_MinimumHeight },
		{ L"min-width",                  PT_MinimumWidth },
		{ L"padding",                    PT_Padding },
		{ L"padding-bottom",             PT_PaddingBottom },
		{ L"padding-left",               PT_PaddingLeft },
		{ L"padding-right",              PT_PaddingRight },
		{ L"padding-top",                PT_PaddingTop },
		{ L"text-align",                 PT_TextAlignment },
		{ L"text-decoration",            PT_TextDecoration },
		{ L"text-indent",                PT_TextIndent },
		{ L"text-outline",               PT_TextOutline },
		{ L"text-overflow",              PT_TextOverflow },
		{ L"text-shadow",                PT_TextShadow },
		{ L"text-underline-style",       PT_TextUnderlineStyle },
		{ L"width",                      PT_Width }
	};

	static const CSSValuePair knownValues[KnownValueCount - 1] = {
		{ L"bold",         Value_Bold },
		{ L"border",       Value_Border },
		{ L"bottom",       Value_Bottom },
		{ L"center",       Value_Center },
		{ L"clip",         Value_Clip },
		{ L"content",      Value_Content },
		{ L"dashed",       Value_Dashed },
		{ L"dot-dash",     Value_DotDash },
		{ L"dot-dot-dash", Value_DotDotDash },
		{ L"dotted",       Value_Dotted },
		{ L"ellipsis",     Value_Ellipsis },
		{ L"italic",       Value_Italic },
		{ L"left",         Value_Left },
		{ L"line-through", Value_LineThrough },
		{ L"margin",       Value_Margin },
		{ L"no-repeat",    Value_NoRepeat },
		{ L"none",         Value_None },
		{ L"normal",       Value_Normal },
		{ L"oblique",      Value_Oblique },
		{ L"overline",     Value_Overline },
		{ L"padding",      Value_Padding },
		{ L"repeat",       Value_Repeat },
		{ L"repeat-x",     Value_RepeatX },
		{ L"repeat-y",     Value_RepeatY },
		{ L"right",        Value_Right },
		{ L"solid",        Value_Solid },
		{ L"stretch",      Value_Stretch },
		{ L"top",          Value_Top },
		{ L"transparent",  Value_Transparent },
		{ L"true",         Value_True },
		{ L"underline",    Value_Underline },
		{ L"wave",         Value_Wave },
		{ L"wrap",         Value_Wrap },
	};

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

	RenderRule& RenderRule::operator=(const RenderRule& other)
	{
		if(&other != this)
		{
			if(data) {
				--data->refCount;
				if(data->refCount == 0)
					delete data;
			}
			data = other.data;
			if(data)
				++data->refCount;
		}
		return *this;
	}

	bool RenderRuleData::setGeometry(MWidget* w)
	{
		if(geoData == 0)
			return false;

		D2D_SIZE_U size = w->size();
		D2D_SIZE_U size2 = size;
		if(geoData->width != -1 && geoData->width != size2.width)
			size2.width = geoData->width;
		if(geoData->height != -1 && geoData->height != size2.height)
			size2.height = geoData->height;

		if((unsigned int)geoData->minWidth > size2.width)
			size2.width = geoData->minWidth;
		else if ((unsigned int)geoData->maxWidth < size.width)
			size2.width = geoData->maxWidth;

		if((unsigned int)geoData->minHeight > size2.height)
			size2.height = geoData->minHeight;
		else if((unsigned int)geoData->maxHeight < size2.height)
			size2.height = geoData->maxHeight;

		D2D_SIZE_U range = w->minSize();
		if(geoData->minWidth != -1)
			range.width = geoData->minWidth;
		if(geoData->minHeight != -1)
			range.height = geoData->minHeight;
		w->setMinimumSize(range.width,range.height);

		range = w->maxSize();
		if(geoData->maxWidth != -1)
			range.width = geoData->maxWidth;
		if(geoData->maxHeight != -1)
			range.height = geoData->maxHeight;
		w->setMaximumSize(range.width,range.height);

		if(size2.width != size.width || size2.height != size.height)
		{
			w->resize(size2.width,size2.height);
			return true;
		}
		return false;
	}

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
		// Special treatment for Hover Pseudo.
		if(p == PC_Hover)
			return bs->pseudo == p;

		return bs->pseudo == 0 ? true :
				(p != (bs->pseudo & p)) ? false : (p != 0);
	}

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
			if(values.at(i).type == CssValue::Uri)
				delete values.at(i).data.vstring;
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
	// Margin:          Length{1,4}
	// Text-align:      Alignment;
	// Text-decoration: none | underline | overline | line-through
	// Text-overflow:   clip | ellipsis | wrap
	// Text-Shadow:     h-shadow v-shadow blur Color;
	// Text-underline-style: LineStyle
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
		while(index <= maxIndex)
		{
			wchar_t byte = css.at(index);
			if(iswspace(byte) || index == maxIndex)
			{
				if(index == maxIndex)
					buffer.append(1,byte);

				if(!buffer.empty())
				{
					if(buffer.at(buffer.size() - 1) == L'x' && buffer.at(buffer.size() - 2) == L'p') // Length
					{
						value = CssValue();
						value.type = CssValue::Length;
						buffer.erase(buffer.size() - 2, 2);
						value.data.vint = _wtoi(buffer.c_str());
						values.push_back(value);
						buffer.clear();
					} else if(iswdigit(buffer.at(0)) || buffer.at(0) == L'-') // Number
					{
						value = CssValue();
						value.type = CssValue::Number;
						value.data.vint = _wtoi(buffer.c_str());
						values.push_back(value);
						buffer.clear();
					} else // Identifier
					{
						const CSSValuePair* crItem = findCSSValue(buffer,knownValues,KnownValueCount - 1);
						if(crItem != 0)
						{
							value = CssValue();
							if(crItem->value == Value_Transparent)
							{
								value.type = CssValue::Color;
								value.data.vuint = 0;
							} else {
								value.type = CssValue::Identifier;
								value.data.vuint = crItem->value;
							}
							values.push_back(value);
							buffer.clear();
						}
					}
				}
			}else if(byte == L'#') // Hex Color
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
				value = CssValue();
				value.type = CssValue::Color;
				unsigned int hexCol;
				s >> std::hex >> hexCol;
				value.data.vuint = hexCol << 8 | 0xFF;

				values.push_back(value);

			}else if(byte == L'u' && css.at(index + 1) == L'r' &&
					 css.at(index + 2) == L'l' && css.at(index + 3) == L'(') // url()
			{
				index += 4;
				value = CssValue();
				value.type = CssValue::Uri;
				value.data.vstring = new std::wstring();
				while(true)
				{
					byte = css.at(index);
					if(byte == L')')
						break;

					value.data.vstring->append(1,byte);
					++index;
				}
				values.push_back(value);
			}else if(byte == L'r' && css.at(index + 1) == L'g' &&
					 css.at(index + 2) == L'b' &&
					 (css.at(index + 3) == L'(' ||
					  (css.at(index + 3) == L'a' && css.at(index + 4) == L'(')))
			{
				if(css.at(index + 3) == L'(')
					index += 4;
				else
					index += 5;

				value = CssValue();
				value.type = CssValue::Color;
				value.data.vuint = 0xFF;
				int shift = 24;
				while(true)
				{
					byte = css.at(index);
					if(byte == ',')
					{
						int c = _wtoi(buffer.c_str());
						value.data.vuint |= (c << shift);
						shift -= 8;
						buffer.clear();
					}else if(byte == ')')
						break;
					else if(!iswspace(byte))
						buffer.append(1,byte);
				}

				if(shift == 0) // alpha
				{
					float f;
					std::wstringstream stream;
					stream << buffer;
					stream >> f;
					int opacity = (int)(f * 255);
					value.data.vuint |= opacity;
				}else // blue
				{
					int b = _wtoi(buffer.c_str());
					value.data.vuint |= (b << 8);
				}

				values.push_back(value);
				buffer.clear();
			}else
				buffer.append(1,byte);

			++index;
		}
	}

	MStyleSheetStyle::~MStyleSheetStyle()
	{
		delete appStyleSheet;
		WidgetSSCache::iterator it = widgetSSCache.begin();
		WidgetSSCache::iterator itEnd = widgetSSCache.end();
		while(it != itEnd) {
			delete it->second;
			++it;
		}

		removeResources();
	}

	void MStyleSheetStyle::setAppSS(const std::wstring& css)
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

	void MStyleSheetStyle::setWidgetSS(MWidget* w, const std::wstring& css)
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
		clearRenderRuleCacheRecursively(w);
	}

	void MStyleSheetStyle::clearRenderRuleCacheRecursively(MWidget* w)
	{
		widgetStyleRuleCache.erase(w);
		widgetRenderRuleCache.erase(w);
		polish(w);

		const std::list<MWidget*>& children = w->children();
		std::list<MWidget*>::const_iterator it = children.begin();
		std::list<MWidget*>::const_iterator itEnd = children.end();
		while(it != itEnd)
		{
			clearRenderRuleCacheRecursively(*it);
			++it;
		}
	}

	void MStyleSheetStyle::removeResources()
	{
		D2D1SolidBrushMap::iterator sIter = solidBrushCache.begin();
		D2D1SolidBrushMap::iterator sIterEnd = solidBrushCache.end();
		while(sIter != sIterEnd)
		{
			SafeRelease(sIter->second);
			++sIter;
		}

		D2D1BitmapMap::iterator bIter = bitmapCache.begin();
		D2D1BitmapMap::iterator bIterEnd = bitmapCache.end();
		while(bIter != bIterEnd)
		{
			SafeRelease(bIter->second);
			++bIter;
		}
		solidBrushCache.clear();
		bitmapCache.clear();
	}

	void getWidgetClassName(const MWidget* w,std::wstring& name)
	{
		std::wstringstream s;
		s << typeid(*w).name();
		name = s.str();
#ifdef MSVC
		name.erase(0,6); // Eat the 'class ' generated by msvc.
#endif

		int pos = 0;
		while((pos = name.find(L"::",pos)) != std::wstring::npos)
		{
			name.replace(pos,2,L"--");
			pos += 2;
		}
	}

	bool basicSelectorMatches(const BasicSelector* bs, MWidget* w)
	{
		if(!bs->id.empty() && w->objectName() != bs->id)
			return false;

		if(!bs->elementName.empty())
		{
			std::wstring ss;
			getWidgetClassName(w,ss);
			if(bs->elementName != ss)
				return false;
		}
		return true;
	}

	bool selectorMatches(const Selector* sel, MWidget* w)
	{
		const std::vector<BasicSelector*>& basicSels = sel->basicSelectors;
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
			}else
				--i;

			if((w = w->parent()) == 0)
				return false;
		}

		return matched;
	}

	void matchRule(MWidget* w,const StyleRule* rule,int depth,
				   std::multimap<int, MatchedStyleRule>& weightedRules)
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
				weightedRules.insert(std::multimap<int, MatchedStyleRule>::value_type(weight,msr));
			}
		}
	}

	void MStyleSheetStyle::getMachedStyleRules(MWidget* widget, MatchedStyleRuleVector& srs)
	{
// 		if(widget->testAttributes(WA_NoStyleSheet))
// 			return;

		// Get all possible stylesheets
		std::vector<CSS::StyleSheet*> sheets;
		for(MWidget* p = widget; p != 0; p = p->parent())
		{
			WidgetSSCache::const_iterator wssIter = widgetSSCache.find(p);
			if(wssIter != widgetSSCache.end())
				sheets.push_back(wssIter->second);
		}
		sheets.push_back(appStyleSheet);

		std::multimap<int, MatchedStyleRule> weightedRules;
		int sheetCount = sheets.size();
		for(int sheetIndex = 0; sheetIndex < sheetCount; ++sheetIndex)
		{
			int depth = sheetCount - sheetIndex;
			const CSS::StyleSheet* sheet = sheets.at(sheetIndex);
			// search for universal rules.
			for(unsigned int i = 0;i< sheet->universal.size(); ++i) {
				matchRule(widget,sheet->universal.at(i),depth,weightedRules);
			}

			// check for Id
			if(!sheet->srIdMap.empty() && !widget->objectName().empty())
			{
				std::pair<StyleSheet::StyleRuleIdMap::const_iterator,
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
				std::wstring widgetClassName;
				getWidgetClassName(widget,widgetClassName);
				std::pair<StyleSheet::StyleRuleElementMap::const_iterator,
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
		std::multimap<int, MatchedStyleRule>::const_iterator wrIt = weightedRules.begin();
		std::multimap<int, MatchedStyleRule>::const_iterator wrItEnd = weightedRules.end();

		while(wrIt != wrItEnd)
		{
			srs.push_back(wrIt->second);
			++wrIt;
		}
	}

	namespace CSS
	{
		enum BrushType
		{
			SolidBrush,
			GradiantBrush,
			BitmapBrush
		};

		union D2D1BrushPointer
		{
			ID2D1SolidColorBrush** solidBrush;
			ID2D1Bitmap** bitmap;
		};

		// Remark: Replace repeatX and repeatY with D2D1 enums,
		// Remark: Add width and height for background.
		// so that we don't need to check them when we use them.
		struct BackgroundRenderObject
		{
			BackgroundRenderObject():bitmapBrush(0),
					x(0),y(0),width(0),height(0),
					clip(Value_Margin),alignmentX(Value_Left),
					alignmentY(Value_Top),repeatX(false),repeatY(false),
					lastUsedBitmap(0){}

			~BackgroundRenderObject() { SafeRelease(bitmapBrush); }

			// Return true if the brush should be deleted when drawing finshes.
			ID2D1BitmapBrush* getBitmapBrush(ID2D1RenderTarget*);

			D2D1BrushPointer brush;
			ID2D1BitmapBrush* bitmapBrush;
			BrushType brushType;
			int x;
			int y;
			unsigned int width;
			unsigned int height;
			unsigned int clip;
			unsigned int alignmentX;
			unsigned int alignmentY;
			bool repeatX;
			bool repeatY;
			private:
				ID2D1Bitmap* lastUsedBitmap;
		};

		struct BorderImageRenderObject
		{
			enum Portion
			{
				Conner = 0,
				TopCenter,
				CenterLeft,
				Center,
				CenterRight,
				BottomCenter
			};
			BorderImageRenderObject():topWidth(0),rightWidth(0),
						bottomWidth(0),leftWidth(0),
						repeatX(true),repeatY(true),lastUsedBitmap(0)
			{ ZeroMemory(portionBrushes,sizeof(ID2D1BitmapBrush*)*6); }
			~BorderImageRenderObject()
			{
				for(int i = 0; i < 6; ++i)
					SafeRelease(portionBrushes[i]);
			}
			ID2D1Bitmap**  borderImage;
			ID2D1BitmapBrush* portionBrushes[6];
			int  topWidth;
			int  rightWidth;
			int  bottomWidth;
			int  leftWidth;
			bool repeatX;
			bool repeatY;

			ID2D1BitmapBrush* getPortionBrush(ID2D1RenderTarget* renderTarget, Portion p);
			inline D2D1_SIZE_U imageSize() const { return (*borderImage)->GetPixelSize(); }

			private:
				ID2D1Bitmap* lastUsedBitmap;
		};

		struct SimpleBorderRenderObject : public BorderRenderObject
		{
			SimpleBorderRenderObject():width(0),style(Value_Solid),solidBrush(0),rectGeo(0)
			{ type = SimpleBorder; }
			int width;
			unsigned int style;
			ID2D1SolidColorBrush** solidBrush;
			ID2D1RectangleGeometry* rectGeo;

			void getBorderWidth(RECT& rect)
			{ rect.left = rect.right = rect.bottom = rect.top = width; }

			bool isVisible() { return *solidBrush != 0 && width != 0; }
			ID2D1Brush* getBrush() { return *solidBrush; }
			int getWidth() { return width; }
		};
		struct RadiusBorderRenderObject : public SimpleBorderRenderObject
		{
			RadiusBorderRenderObject():radius(0){ type = RadiusBorder; }
			int radius;
		};
		struct ComplexBorderRenderObject : public BorderRenderObject
		{
			D2D1_RECT_U styles;
			D2D1_RECT_U widths;
			int radiuses[4]; // TL, TR, BL, BR
			ID2D1SolidColorBrush** brushes[4]; // T, R, B, L
			ComplexBorderRenderObject() {
				type = ComplexBorder;
				styles.left = styles.top = styles.right = styles.bottom = Value_Solid;
				memset(&widths  , 0, sizeof(RECT));
				memset(&radiuses, 0, 4 * sizeof(int));
				memset(brushes  , 0, 4 * sizeof(ID2D1SolidColorBrush**));
			}

			void getBorderWidth(RECT& rect)
			{
				rect.left   = widths.left;
				rect.right  = widths.right;
				rect.bottom = widths.bottom;
				rect.top    = widths.top;
			}

			bool isVisible() {
				if(brushes[0] == 0 || *(brushes[0]) == 0)
					return false;
				if(widths.left == 0 && widths.right == 0 && widths.top == 0 && widths.bottom == 0)
					return false;
				return true;
			}

			ID2D1Geometry* createGeometry(const D2D1_RECT_F& rect)
			{
				ID2D1GeometrySink* sink;
				ID2D1PathGeometry* pathGeo;
				mApp->getD2D1Factory()->CreatePathGeometry(&pathGeo);
				pathGeo->Open(&sink);
				sink->BeginFigure(D2D1::Point2F(rect.left + radiuses[0],rect.top),D2D1_FIGURE_BEGIN_FILLED);
				sink->AddLine(D2D1::Point2F(rect.right - radiuses[1],rect.top));
				if(radiuses[1] != 0)
					sink->AddArc(D2D1::ArcSegment(
								D2D1::Point2F(rect.right,rect.top + radiuses[1]),
								D2D1::SizeF((FLOAT)radiuses[1],(FLOAT)radiuses[1]),
								0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
				sink->AddLine(D2D1::Point2F(rect.right,rect.bottom - radiuses[2]));
				if(radiuses[2] != 0)
					sink->AddArc(D2D1::ArcSegment(
								D2D1::Point2F(rect.right - radiuses[2],rect.bottom),
								D2D1::SizeF((FLOAT)radiuses[2],(FLOAT)radiuses[2]),
								0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
				sink->AddLine(D2D1::Point2F(rect.left + radiuses[3],rect.bottom));
				if(radiuses[3] != 0)
					sink->AddArc(D2D1::ArcSegment(
								D2D1::Point2F(rect.left,rect.bottom - radiuses[3]),
								D2D1::SizeF((FLOAT)radiuses[3],(FLOAT)radiuses[3]),
								0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
				sink->AddLine(D2D1::Point2F(rect.left, rect.top + radiuses[0]));
				if(radiuses[0] != 0)
					sink->AddArc(D2D1::ArcSegment(
								D2D1::Point2F(rect.left + radiuses[0],rect.top),
								D2D1::SizeF((FLOAT)radiuses[0],(FLOAT)radiuses[0]),
								0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
				sink->EndFigure(D2D1_FIGURE_END_CLOSED);
				sink->Close();
				SafeRelease(sink);
				return pathGeo; 
			}
			
			void draw(ID2D1RenderTarget* rt,ID2D1Geometry* geo,const D2D1_RECT_F& rect)
			{
				if(widths.left == widths.right && widths.right == widths.top && widths.top == widths.bottom &&
					brushes[0] == brushes[1] && brushes[1] == brushes[2] && brushes[2] == brushes[3])
				{
					rt->DrawGeometry(geo,*(brushes[0]),(FLOAT)widths.left);
				} else {
					if(widths.top > 0) {
						if(radiuses[0] != 0 || radiuses[1] != 0)
						{
							ID2D1PathGeometry* pathGeo;
							mApp->getD2D1Factory()->CreatePathGeometry(&pathGeo);
							ID2D1GeometrySink* sink;
							pathGeo->Open(&sink);
							if(radiuses[0] != 0) {
								sink->BeginFigure(D2D1::Point2F(rect.left, rect.top + radiuses[0]),D2D1_FIGURE_BEGIN_FILLED);
								sink->AddArc(D2D1::ArcSegment(
									D2D1::Point2F(rect.left + radiuses[0],rect.top),
									D2D1::SizeF((FLOAT)radiuses[0],(FLOAT)radiuses[0]),
									0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
							} else {
								sink->BeginFigure(D2D1::Point2F(rect.left,rect.top),D2D1_FIGURE_BEGIN_FILLED);
							}

							if(radiuses[1] != 0) {
								sink->AddLine(D2D1::Point2F(rect.right - radiuses[1],rect.top));
								sink->AddArc(D2D1::ArcSegment(
									D2D1::Point2F(rect.right,rect.top + radiuses[1]),
									D2D1::SizeF((FLOAT)radiuses[1],(FLOAT)radiuses[1]),
									0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
							} else {
								sink->AddLine(D2D1::Point2F(rect.right,rect.top));
							}
							sink->EndFigure(D2D1_FIGURE_END_OPEN);
							sink->Close();
							rt->DrawGeometry(pathGeo,*(brushes[0]),(FLOAT)widths.top);
							SafeRelease(sink);
							SafeRelease(pathGeo);
						} else {
							rt->DrawLine(D2D1::Point2F(rect.left,rect.top),
								D2D1::Point2F(rect.right,rect.top),*(brushes[0]),(FLOAT)widths.top);
						}
					}
					
					if(widths.right > 0)
						rt->DrawLine(D2D1::Point2F(rect.right,rect.top+radiuses[1]),
									D2D1::Point2F(rect.right,rect.bottom - radiuses[2]),
									*(brushes[1]),(FLOAT)widths.right);

					if(widths.bottom > 0) {
						if(radiuses[2] != 0 || radiuses[3] != 0)
						{
							ID2D1PathGeometry* pathGeo;
							mApp->getD2D1Factory()->CreatePathGeometry(&pathGeo);
							ID2D1GeometrySink* sink;
							pathGeo->Open(&sink);
							if(radiuses[2] != 0) {
								sink->BeginFigure(D2D1::Point2F(rect.right, rect.bottom - radiuses[2]),D2D1_FIGURE_BEGIN_FILLED);
								sink->AddArc(D2D1::ArcSegment(
									D2D1::Point2F(rect.right - radiuses[2],rect.bottom),
									D2D1::SizeF((FLOAT)radiuses[2],(FLOAT)radiuses[2]),
									0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
							} else {
								sink->BeginFigure(D2D1::Point2F(rect.right,rect.bottom),D2D1_FIGURE_BEGIN_FILLED);
							}

							if(radiuses[3] != 0) {
								sink->AddLine(D2D1::Point2F(rect.left + radiuses[3],rect.bottom));
								sink->AddArc(D2D1::ArcSegment(
									D2D1::Point2F(rect.left,rect.bottom - radiuses[3]),
									D2D1::SizeF((FLOAT)radiuses[3],(FLOAT)radiuses[3]),
									0.f,D2D1_SWEEP_DIRECTION_CLOCKWISE,D2D1_ARC_SIZE_SMALL));
							} else {
								sink->AddLine(D2D1::Point2F(rect.left,rect.bottom));
							}
							sink->EndFigure(D2D1_FIGURE_END_OPEN);
							sink->Close();
							rt->DrawGeometry(pathGeo,*(brushes[3]),(FLOAT)widths.bottom);
							SafeRelease(sink);
							SafeRelease(pathGeo);
						} else {
							rt->DrawLine(D2D1::Point2F(rect.right,rect.bottom),
								D2D1::Point2F(rect.left,rect.bottom),*(brushes[3]),(FLOAT)widths.bottom);
						}
					}

					if(widths.left > 0)
						rt->DrawLine(D2D1::Point2F(rect.left,rect.bottom-radiuses[3]),
									D2D1::Point2F(rect.left,rect.top-radiuses[0]),
									*(brushes[3]),(FLOAT)widths.left);
				}
			}
		};
	} // namespace CSS

	ID2D1SolidColorBrush** MStyleSheetStyle::createD2D1SolidBrush(CssValue& v)
	{
		// Cache the brush with color value.
		ID2D1SolidColorBrush*& brush = solidBrushCache[v.data.vuint];
		if(brush == 0)
			workingRenderTarget->CreateSolidColorBrush(v.getColor(),&brush);
		return &brush;
	}

	bool isImageOpaque(const std::wstring& uri)
	{
		std::wstring ex = uri.substr(uri.size() - 3);
		std::transform(ex.begin(),ex.end(),ex.begin(),::tolower);
		if(ex.find(L"png") == 0 || ex.find(L"gif") == 0)
			return false;
		return true;
	}

	ID2D1Bitmap** MStyleSheetStyle::createD2D1Bitmap(const std::wstring& uri, bool& isOpaque)
	{
		ID2D1Bitmap*& bitmap = bitmapCache[uri];
		if(bitmap != 0)
		{
			// Remark: This may causes error. If we cannot load a opaque image,
			// we will create a empty image instead, which is transparent.
			// But imageMustBeOpaque will return true.
			isOpaque = isImageOpaque(uri);
		} else {

			IWICImagingFactory*    wicFactory = mApp->getWICImagingFactory();
			IWICBitmapDecoder*     decoder	  = 0;
			IWICBitmapFrameDecode* frame	  = 0;
			IWICFormatConverter*   converter  = 0;
			IWICStream*            stream     = 0;

			bool hasError = false;
			HRESULT hr;
			if(uri.at(0) == L':') {
				// Lookup image file is inside MResources.
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
														   GENERIC_READ,
														   WICDecodeMetadataCacheOnDemand,&decoder);
				hasError = FAILED(hr);
			}

			if(hasError)
			{
				std::wstring error = L"[MStyleSheetStyle] Cannot open image file: ";
				error.append(uri);
				error.append(1,L'\n');
				mDebug(error.c_str());
				// create a empty bitmap because we can't find the image.
				hr = workingRenderTarget->CreateBitmap(
							D2D1::SizeU(),
							D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
																	 D2D1_ALPHA_MODE_PREMULTIPLIED)),
							&bitmap);

				isOpaque = false;
			} else {
				decoder->GetFrame(0,&frame);
				wicFactory->CreateFormatConverter(&converter);
				converter->Initialize(frame,
									  GUID_WICPixelFormat32bppPBGRA,
									  WICBitmapDitherTypeNone,NULL,
									  0.f,WICBitmapPaletteTypeMedianCut);
				workingRenderTarget->CreateBitmapFromWicBitmap(converter,
									D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, 
															D2D1_ALPHA_MODE_PREMULTIPLIED)),
									&bitmap);

				isOpaque = isImageOpaque(uri);
			}

			SafeRelease(decoder);
			SafeRelease(frame);
			SafeRelease(stream);
			SafeRelease(converter);
		}

		return &bitmap;
	}

	// Return true if the "prop" is AlignmentX value
	bool setBackgroundRenderObjectProperty(BackgroundRenderObject* object,
										unsigned int prop,bool alignmentY = false)
	{
		switch(prop) {
			case Value_NoRepeat:object->repeatX = object->repeatY =  false;       break;
			case Value_RepeatX: object->repeatX = true;  object->repeatY = false; break;
			case Value_RepeatY: object->repeatX = false; object->repeatY = true;  break;
			case Value_Repeat:  object->repeatX = object->repeatY = true;         break;

			case Value_Padding:
			case Value_Content:
			case Value_Border:
			case Value_Margin:  object->clip = prop; break;

			// We both set alignment X & Y, because one may only specific a alignment value.
			case Value_Left:
			case Value_Right:	 object->alignmentX = prop; return true;
			case Value_Top:
			case Value_Bottom: object->alignmentY = prop; return true;
			case Value_Center:
				object->alignmentY = prop;
				if(!alignmentY)
				{
					object->alignmentX = prop;
					return true;
				}else
					return false;
				break;
		}
		return false;
	}

	void setBackgroundRenderObjectsProperty(std::vector<BackgroundRenderObject*>& bgros,
			 unsigned int prop, bool alignmentY = false)
	{
		std::vector<BackgroundRenderObject*>::iterator iter = bgros.begin();
		std::vector<BackgroundRenderObject*>::iterator iterEnd = bgros.end();
		while(iter != iterEnd)
		{
			setBackgroundRenderObjectProperty(*iter,prop,alignmentY);
			++iter;
		}
	}

	namespace CSS {
		enum BackgroundOpaqueType {
			NonOpaque,
			OpaqueClipMargin,
			OpaqueClipBorder,
			OpaqueClipPadding
		};
	}

	// Background: { Brush/Image Repeat Clip Alignment pos-x pos-y }*;
	// Only one brush allowed in a declaration.
	BackgroundRenderObject* MStyleSheetStyle::createBackgroundRO(unsigned int& opaqueType)
	{
		BackgroundRenderObject* newObject = new BackgroundRenderObject();

		bool opaqueBrush = true;

		std::vector<CssValue>& values = workingDeclaration->values;
		CssValue& brushValue = values.at(0);
		if(brushValue.type == CssValue::Color)
		{
			newObject->brushType = SolidBrush;
			newObject->brush.solidBrush = createD2D1SolidBrush(brushValue);
		} else if(brushValue.type == CssValue::Uri) {
			newObject->brushType = BitmapBrush;
			newObject->brush.bitmap = createD2D1Bitmap(*(brushValue.data.vstring),opaqueBrush);
		}

		unsigned int index = 1;
		while(index < values.size())
		{
			if(values.at(index).type == CssValue::Identifier)
			{
				if(setBackgroundRenderObjectProperty(newObject,values.at(index).data.vuint))
				{
					++index;
					// Set AlignmentY
					if(index < values.size() && values.at(index).type == CssValue::Identifier)
						setBackgroundRenderObjectProperty(newObject,values.at(index).data.vuint,true);
				}
			}else
			{
				int prop = values.at(index).data.vint;
				newObject->x = prop;
				++index;
				if(index < values.size() && values.at(index).type == CssValue::Number)
					prop = values.at(index).data.vint;
				newObject->y = prop;
				++index;
				if(index < values.size() && values.at(index).type == CssValue::Number)
					newObject->width = values.at(index).data.vint;
				++index;
				if(index < values.size() && values.at(index).type == CssValue::Number)
					newObject->height = values.at(index).data.vint;
			}

			++index;
		}

		if(!opaqueBrush)
			opaqueType = NonOpaque;
		else
		{
			if(newObject->clip == Value_Margin)
				opaqueType = OpaqueClipMargin;
			else if(newObject->clip == Value_Border)
				opaqueType = OpaqueClipBorder;
			else
				opaqueType = OpaqueClipPadding;
		}

		if(values.at(0).type != CssValue::Color)
		{
			D2D1_SIZE_U size = (*newObject->brush.bitmap)->GetPixelSize();
			// If the user doesn't specify the width of the background,
			// we set it. Otherwise, check if the width is valid.
			if(newObject->width == 0 || newObject->width + newObject->x > size.width)
				newObject->width = size.width - newObject->x;

			if(newObject->height == 0 || newObject->height + newObject->y > size.height)
				newObject->height = size.height - newObject->y;
		}

		return newObject;
	}

	BorderImageRenderObject* MStyleSheetStyle::createBorderImageRO(bool& isOpaqueBG)
	{
		std::vector<CssValue>& values = workingDeclaration->values;

		BorderImageRenderObject* biro = new BorderImageRenderObject();
		// Cache.
		biro->borderImage = createD2D1Bitmap(*(values.at(0).data.vstring),isOpaqueBG);
		int endIndex = values.size() - 1;
		// Repeat or Stretch
		if(values.at(endIndex).type == CssValue::Identifier) {
			if(values.at(endIndex-1).type == CssValue::Identifier) {
				if(values.at(endIndex).data.vuint == Value_Stretch)
					biro->repeatY = false;
				if(values.at(endIndex - 1).data.vuint == Value_Stretch)
					biro->repeatX = false;
				endIndex -= 2;
			} else {
				if(values.at(endIndex).data.vuint == Value_Stretch) {
					biro->repeatY = false;
					biro->repeatX = false;
				}
				--endIndex;
			}
		}
		// Border
		if(endIndex == 4) {
			biro->topWidth    = values.at(1).data.vint;
			biro->rightWidth  = values.at(2).data.vint;
			biro->bottomWidth = values.at(3).data.vint;
			biro->leftWidth   = values.at(4).data.vint;
		} else if(endIndex == 2) {
			biro->bottomWidth = biro->topWidth   = values.at(1).data.vint;
			biro->leftWidth   = biro->rightWidth = values.at(2).data.vint;
		} else {
			biro->leftWidth   = biro->rightWidth =
			biro->bottomWidth = biro->topWidth   = values.at(1).data.vint;
		}

		return biro;
	}

	void MStyleSheetStyle::setSimpleBorederRO(SimpleBorderRenderObject* obj,
			DeclMap::iterator& iter, DeclMap::iterator iterEnd)
	{
		CssValue colorValue;
		colorValue.data.vuint = 0;
		colorValue.type = CssValue::Color;
		while(iter != iterEnd && iter->first <= PT_BorderStyles) {
			std::vector<CssValue>& values = iter->second->values;
			switch(iter->first) {
			case PT_Border: {
				for(unsigned int i = 0; i < values.size(); ++i) {
					CssValue& v = values.at(i);
					switch(v.type) {
						case CssValue::Identifier:	
							obj->style = v.data.vuint;
							break;
						case CssValue::Color:
							colorValue = v;
							break;
						default:
							obj->width = v.data.vint;
							break;
					}
				}
			}
				break;
			case PT_BorderWidth:	obj->width = values.at(0).data.vint; break;
			case PT_BorderColor:	colorValue = values.at(0); break;
			case PT_BorderStyles:	obj->style = values.at(0).data.vuint; break;
			}

			++iter;
		}
		obj->solidBrush = createD2D1SolidBrush(colorValue);
	}

	void setGroupIntValue(int (&intArray)[4], std::vector<CssValue>& values,
		int startValueIndex = 0, int endValueIndex = -1)
	{
		int size = (endValueIndex == -1 ? values.size() : endValueIndex + 1) - startValueIndex;
		if(size == 4) {
			intArray[0] = values.at(startValueIndex    ).data.vint;
			intArray[1] = values.at(startValueIndex + 1).data.vint;
			intArray[2] = values.at(startValueIndex + 2).data.vint;
			intArray[3] = values.at(startValueIndex + 3).data.vint;
		} else if(size == 2) {
			intArray[0] = intArray[2] = values.at(startValueIndex    ).data.vint;
			intArray[1] = intArray[3] = values.at(startValueIndex + 1).data.vint;
		}else
		{
			intArray[0] = intArray[1] = 
			intArray[2] = intArray[3] = values.at(startValueIndex).data.vint;
		}
	}

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
		}else
		{
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
		}else
		{
			rect.top    = rect.bottom =
			rect.right  = rect.left   = values.at(startValueIndex).data.vuint;
		}
	}

	void setD2DRectValue(D2D_RECT_U& rect, int border, unsigned int value)
	{
		if(border == 0)
			rect.top = value;
		else if(border == 1)
			rect.right = value;
		else if(border == 2)
			rect.bottom = value;
		else
			rect.left = value;
	}

	void MStyleSheetStyle::setComplexBorderRO(ComplexBorderRenderObject* obj,
			DeclMap::iterator& declIter, DeclMap::iterator declIterEnd)
	{
		while(declIter != declIterEnd && declIter->first <= PT_BorderRadius)
		{
			std::vector<CssValue>& values = declIter->second->values;
			switch(declIter->first)
			{
				case PT_Border: {
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
							case CssValue::Color: {
								int size = index - rangeStartIndex;
								if(size == 4) {
									obj->brushes[0] = createD2D1SolidBrush(values.at(rangeStartIndex  ));
									obj->brushes[1] = createD2D1SolidBrush(values.at(rangeStartIndex+1));
									obj->brushes[2] = createD2D1SolidBrush(values.at(rangeStartIndex+2));
									obj->brushes[3] = createD2D1SolidBrush(values.at(rangeStartIndex+3));
								} else if(size == 2) {
									obj->brushes[0] = 
									obj->brushes[2] = createD2D1SolidBrush(values.at(rangeStartIndex  ));
									obj->brushes[1] = 
									obj->brushes[3] = createD2D1SolidBrush(values.at(rangeStartIndex+1));
								} else {
									obj->brushes[0] = obj->brushes[1] = obj->brushes[2] =  
									obj->brushes[3] = createD2D1SolidBrush(values.at(rangeStartIndex));
								}
							}
								 break;
							default: setD2DRectValue(obj->widths,values,rangeStartIndex,index-1);
							}
							rangeStartIndex = index;
							if(index < values.size())
								valueType = values.at(index).type;
						}
						++index;
					}
				}
					break;
				case PT_BorderRadius:	setGroupIntValue(obj->radiuses,values); break;
				case PT_BorderWidth:	setD2DRectValue (obj->widths  ,values); break;
				case PT_BorderStyles:	setD2DRectValue (obj->styles  ,values); break;
				case PT_BorderColor:
					if(values.size() == 4)
					{
						obj->brushes[0] = createD2D1SolidBrush(values.at(0));
						obj->brushes[1] = createD2D1SolidBrush(values.at(1));
						obj->brushes[2] = createD2D1SolidBrush(values.at(2));
						obj->brushes[3] = createD2D1SolidBrush(values.at(3));
					} else if(values.size() == 2)
					{
						obj->brushes[0] = obj->brushes[2] = createD2D1SolidBrush(values.at(0));
						obj->brushes[1] = obj->brushes[3] = createD2D1SolidBrush(values.at(1));
					}else
					{
						obj->brushes[0] = obj->brushes[2] = 
						obj->brushes[1] = obj->brushes[3] = createD2D1SolidBrush(values.at(0));
					}
					break;

				case PT_BorderTop:
				case PT_BorderRight:
				case PT_BorderBottom:
				case PT_BorderLeft:
				{
					int index = declIter->first - PT_BorderTop;
					for(unsigned int i = 0; i < values.size(); ++i)
					{
						if(values.at(i).type == CssValue::Color)
							obj->brushes[index] = createD2D1SolidBrush(values.at(i));
						else if(values.at(i).type == CssValue::Identifier) {
							obj->brushes[index] = createD2D1SolidBrush(values.at(i));
						}else
							setD2DRectValue(obj->widths,index,values.at(i).data.vuint);
					}
				}
					break;

				case PT_BorderTopColor:
				case PT_BorderRightColor:
				case PT_BorderBottomColor:
				case PT_BorderLeftColor:
					obj->brushes[declIter->first - PT_BorderTopColor] = createD2D1SolidBrush(values.at(0));
					break;
				case PT_BorderTopWidth:
				case PT_BorderRightWidth:
				case PT_BorderBottomWidth:
				case PT_BorderLeftWidth:
					setD2DRectValue(obj->widths, declIter->first - PT_BorderTopWidth, values.at(0).data.vint);
					break;
				case PT_BorderTopStyle:
				case PT_BorderRightStyle:
				case PT_BorderBottomStyle:
				case PT_BorderLeftStyle:
					setD2DRectValue(obj->styles, declIter->first - PT_BorderTopStyle, values.at(0).data.vint);
					break;
				case PT_BorderTopLeftRadius:
				case PT_BorderTopRightRadius:
				case PT_BorderBottomLeftRadius:
				case PT_BorderBottomRightRadius:
					obj->radiuses[declIter->first - PT_BorderTopLeftRadius] = values.at(0).data.vint;
					break;
			}
			++declIter;
		}
	}

	enum BorderType { BT_Simple, BT_Radius, BT_Complex };

	BorderType testBorderObjectType(std::vector<CssValue>& values,
		const std::multimap<CSS::PropertyType,CSS::Declaration*>::iterator& declIter,
		const std::multimap<CSS::PropertyType,CSS::Declaration*>::iterator& declIterEnd)
	{
		std::multimap<PropertyType,Declaration*>::iterator seeker = declIter;
		while(seeker != declIterEnd && seeker->first <= PT_BorderRadius)
		{
			switch(seeker->first) {
				case PT_Border:
				{
					std::vector<CssValue>& values = seeker->second->values;
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
				default:	return BT_Complex;
			}
			++seeker;
		}
		return BT_Simple;
	}

	// Why we make such a big monster function...
	// Remark. We should make gif animation possible.
	RenderRule MStyleSheetStyle::getRenderRule(MWidget* w, unsigned int pseudo)
	{
		// == 1.Find RenderRule for MWidget w from Widget-RenderRule cache.
		WidgetRenderRuleMap::iterator widgetCacheIter = widgetRenderRuleCache.find(w);
		PseudoRenderRuleMap* prrMap = 0;
		if(widgetCacheIter != widgetRenderRuleCache.end())
		{
			prrMap = widgetCacheIter->second;
			PseudoRenderRuleMap::Element::iterator prrIter = prrMap->element.find(pseudo);
			if(prrIter != prrMap->element.end())
				return prrIter->second; // Found sth. we are insterested in.
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
		RenderRule& renderRule = prrMap->element[pseudo]; // Insert a RenderRule even if it's empty.
		// If there's no StyleRules for MWidget w, then we return a invalid RenderRule.
		if(matchedsrv->size() == 0)
			return renderRule;

		MatchedStyleRuleVector::const_iterator msrIter = matchedsrv->begin();
		MatchedStyleRuleVector::const_iterator msrIterEnd = matchedsrv->end();
		DeclMap declarations;
		while(msrIter != msrIterEnd)
		{
			const Selector* sel = msrIter->matchedSelector;
			if(sel->matchPseudo(pseudo))
			{
				// This rule is for the widget with pseudo.
				// Add declarations to map.
				const StyleRule* sr = msrIter->styleRule;
				std::vector<Declaration*>::const_iterator declIter = sr->declarations.begin();
				std::vector<Declaration*>::const_iterator declIterEnd = sr->declarations.end();
				std::set<PropertyType> tempProps;

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
				std::set<PropertyType>::iterator tempPropIter = tempProps.begin();
				std::set<PropertyType>::iterator tempPropIterEnd = tempProps.end();
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
		// Remark: If the declaration have values in wrong order, it might crash the program.
		// Maybe we should add some logic to avoid this flaw.
		renderRule.init();
		workingRenderTarget = w->getDCRenderTarget();
		DeclMap::iterator declIter = declarations.begin();
		DeclMap::iterator declIterEnd = declarations.end();

		std::vector<unsigned int> bgOpaqueTypes;// 0.NonOpaque,1.Opaque,2.OpauqeClipMargin,3.OpaqueClipBoarder,4.OpaqueClipPadding
		bool opaqueBorderImage = false;
		BorderType bt = BT_Simple;

		// --- Backgrounds ---
		while(declIter->first == PT_Background) {
			workingDeclaration = declIter->second;
			unsigned int bgOpaqueType = NonOpaque;
			renderRule->backgroundROs.push_back(createBackgroundRO(bgOpaqueType));
			bgOpaqueTypes.push_back(bgOpaqueType);
			if(++declIter == declIterEnd)
				goto END;
		}
		while(declIter->first < PT_BackgroundPosition) {
			setBackgroundRenderObjectsProperty(renderRule->backgroundROs,
							declIter->second->values.at(0).data.vuint);
			if(++declIter == declIterEnd)
				goto END;
		}
		if(declIter->first == PT_BackgroundPosition) {
			std::vector<CssValue>& values = declIter->second->values;
			int propx = values.at(0).data.vint;
			int propy = propx;
			if(values.size() == 2)
				propy = values.at(1).data.vint;

			std::vector<BackgroundRenderObject*>::iterator iter = renderRule->backgroundROs.begin();
			std::vector<BackgroundRenderObject*>::iterator iterEnd = renderRule->backgroundROs.end();
			while(iter != iterEnd) {
				(*iter)->x = propx;
				(*iter)->y = propy;
				++iter;
			}
			if(++declIter == declIterEnd)
				goto END;
		}
		if(declIter->first == PT_BackgroundSize) {
			std::vector<CssValue>& values = declIter->second->values;
			int propw = values.at(0).data.vint;
			int proph = propw;
			if(values.size() == 2)
				proph = values.at(1).data.vint;

			std::vector<BackgroundRenderObject*>::iterator iter = renderRule->backgroundROs.begin();
			std::vector<BackgroundRenderObject*>::iterator iterEnd = renderRule->backgroundROs.end();
			while(iter != iterEnd) {
				(*iter)->width  = propw;
				(*iter)->height = proph;
				++iter;
			}
			if(++declIter == declIterEnd)
				goto END;
		}
		if(declIter->first == PT_BackgroundAlignment) {
			std::vector<CssValue>& values = declIter->second->values;
			setBackgroundRenderObjectsProperty(renderRule->backgroundROs,values.at(0).data.vuint);
			if(values.size() == 2)
				setBackgroundRenderObjectsProperty(renderRule->backgroundROs,values.at(1).data.vuint,true);
			if(++declIter == declIterEnd)
				goto END;
		}
		
		// --- BorderImage --- 
		if(declIter->first == PT_BorderImage) {
			workingDeclaration = declIter->second;
			renderRule->borderImageRO = createBorderImageRO(opaqueBorderImage);
			if(++declIter == declIterEnd)
				goto END;
		}

		// --- Border ---
		if(declIter->first == PT_Border) {
			workingDeclaration = declIter->second;
			std::vector<CssValue>& values = workingDeclaration->values;

			if(values.at(0).type == CssValue::Identifier 
				&& values.at(0).data.vuint == Value_None) { // User specifies no border. Skip everything related to border.
				do {
					if(++declIter == declIterEnd)
						goto END;
				} while (declIter->first <= PT_BorderRadius);
			}
		}

		if(declIter->first <= PT_BorderRadius)
		{
			workingDeclaration = declIter->second;
			std::vector<CssValue>& values = workingDeclaration->values;
			bt = testBorderObjectType(values,declIter,declIterEnd);
			if(bt == BT_Simple)
			{
				SimpleBorderRenderObject* obj = new RadiusBorderRenderObject();
				renderRule->borderRO = obj;
				setSimpleBorederRO(obj,declIter,declIterEnd);
			}else if(bt == BT_Radius)
			{
				RadiusBorderRenderObject* obj = new RadiusBorderRenderObject();
				renderRule->borderRO = obj;
				setSimpleBorederRO(obj,declIter,declIterEnd);
				obj->radius = declIter->second->values.at(0).data.vint;
			}else {
				ComplexBorderRenderObject* obj = new ComplexBorderRenderObject();
				renderRule->borderRO = obj;
				setComplexBorderRO(obj, declIter, declIterEnd);
			}

			if(declIter == declIterEnd)
				goto END;
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
			if(++declIter == declIterEnd)
				goto END;
		}

		// --- Geometry ---
		if(declIter->first <= PT_MinimumHeight)
		{
			GeometryData* geoData = new GeometryData();
			renderRule->geoData = new GeometryData();
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
		}

		// TODO: Create TextRenderObject

		END:
		// Check if this RenderRule is opaque.
		bool opaqueBorder = (!renderRule->hasMargin && bt == BT_Simple);
		for(int i = bgOpaqueTypes.size() - 1; i >= 0; --i) {
			switch(bgOpaqueTypes.at(i)) {
			case OpaqueClipMargin:
				renderRule->opaqueBackground = true;
				i = -1;
				break;
			case OpaqueClipBorder:
				if(opaqueBorder) {
					renderRule->opaqueBackground = true;
					i = -1;
				}
				break;
			case OpaqueClipPadding:
				if(opaqueBorder && !renderRule->hasPadding) {
					renderRule->opaqueBackground = true;
					i = -1;
				}
				break;
			}
		}
		if(opaqueBorderImage)
			renderRule->opaqueBackground = true;

		workingRenderTarget = 0;
		workingDeclaration = 0;
		return renderRule;
	}

	void MStyleSheetStyle::polish(MWidget* w)
	{
		RenderRule rule = getRenderRule(w,PC_Default);
		if(rule.isValid())
			rule.setGeometry(w);

		if(getRenderRule(w, PC_Hover).isValid())
			w->setAttributes(WA_Hover);
	}

	RenderRuleData::~RenderRuleData()
	{
		for(int i = backgroundROs.size() -1; i >=0; --i)
			delete backgroundROs.at(i);
		delete borderImageRO;
		delete borderRO;
		delete geoData;
	}

	ID2D1BitmapBrush* BackgroundRenderObject::getBitmapBrush(ID2D1RenderTarget* renderTarget)
	{
		M_ASSERT_X(brushType == BitmapBrush, "This BackgroundRenderObject doesn't have a BitmapBrush.","BackgroundRenderObject::getBitmapBrush()");

		// The bitmapBrush is still valid.
		if(lastUsedBitmap == *(brush.bitmap))
			return bitmapBrush;

		// Create the brush.
		lastUsedBitmap = *(brush.bitmap);
		// For some reason, the bitmap have been recreated.
		// We need to recreated the bitmapBrush.
		SafeRelease(bitmapBrush);
		renderTarget->CreateBitmapBrush(lastUsedBitmap,
							D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP,D2D1_EXTEND_MODE_WRAP),
							D2D1::BrushProperties(), &bitmapBrush);

		return bitmapBrush;
	}

	void RenderRuleData::draw(ID2D1RenderTarget* rt, const RECT& widgetRectInRT, const RECT& clipRectInRT)
	{
		M_ASSERT(rt != 0);
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
		// === Draw Backgrounds ===
		if(backgroundROs.size() > 0)
			drawBackgrounds(rt, borderGeo, widgetRectInRT, clipRectInRT);

		// === Draw BorderImage ===
		if(borderImageRO != 0)
			drawBorderImage(rt, widgetRectInRT, clipRectInRT);

		// === Draw Border ===
		if(borderRO != 0 && borderRO->isVisible())
		{
			if(borderRO->type == BorderRenderObject::SimpleBorder) {
				SimpleBorderRenderObject* sbro = reinterpret_cast<SimpleBorderRenderObject*>(borderRO);
				rt->DrawRectangle(borderRect,sbro->getBrush(),(FLOAT)sbro->width);
			} else if(borderRO->type == BorderRenderObject::RadiusBorder) {
				RadiusBorderRenderObject* rbro = reinterpret_cast<RadiusBorderRenderObject*>(borderRO);
				D2D1_ROUNDED_RECT rrect;
				rrect.rect = borderRect;
				rrect.radiusX = rrect.radiusY = (FLOAT)rbro->radius;
				rt->DrawRoundedRectangle(rrect,rbro->getBrush(),(FLOAT)rbro->width);
			} else {
				reinterpret_cast<ComplexBorderRenderObject*>(borderRO)->draw(rt, borderGeo, borderRect);
			}
		}
		SafeRelease(borderGeo);
	}

	void RenderRuleData::drawBackgrounds(ID2D1RenderTarget* rt,ID2D1Geometry* borderGeo,
						const RECT& widgetRect, const RECT& clipRect)
	{
		for(unsigned int i = 0; i < backgroundROs.size(); ++i)
		{
			RECT contentRect = widgetRect;

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

			int dx = -bgro->x;
			int dy = -bgro->y;
			ID2D1Brush* brush;
			if(bgro->brushType == BitmapBrush)
				brush = bgro->getBitmapBrush(rt);
			else
				brush = *(bgro->brush.solidBrush);

			if(bgro->alignmentX != Value_Left) {
				if(bgro->alignmentX == Value_Center)
					contentRect.left = (contentRect.left + contentRect.right - bgro->width) / 2;
				else
					contentRect.left = contentRect.right - bgro->width;
			}
			if(bgro->alignmentY != Value_Top) {
				if(bgro->alignmentY == Value_Center)
					contentRect.top = (contentRect.top + contentRect.bottom - bgro->height) / 2;
				else
					contentRect.top = contentRect.bottom - bgro->height;
			}
			if(!bgro->repeatX) {
				int r = contentRect.left + bgro->width;
				if(contentRect.right > r)
					contentRect.right  = r;
			}
			if(!bgro->repeatY) {
				int b = contentRect.top + bgro->height;
				if(contentRect.bottom > b)
					contentRect.bottom = b;
			}

			dx += (int)contentRect.left;
			dy += (int)contentRect.top;
			if(dx != 0 || dy != 0)
				brush->SetTransform(D2D1::Matrix3x2F::Translation((FLOAT)dx,(FLOAT)dy));

			D2D1_RECT_F cr;
			cr.left   = (FLOAT)contentRect.left;
			cr.top    = (FLOAT)contentRect.top;
			cr.right  = (FLOAT)contentRect.right;
			cr.bottom = (FLOAT)contentRect.bottom;

			if(bgro->clip == Value_Border && borderRO != 0) {
				if(borderRO->type == BorderRenderObject::SimpleBorder) {
					rt->FillRectangle(cr,brush);
				} else if(borderRO->type == BorderRenderObject::RadiusBorder) {
					D2D1_ROUNDED_RECT rrect;
					rrect.rect = cr;
					rrect.radiusX = 
					rrect.radiusY = FLOAT(reinterpret_cast<RadiusBorderRenderObject*>(borderRO)->radius);
					rt->FillRoundedRectangle(rrect,brush);
				} else {
					rt->FillGeometry(borderGeo,brush);
				}
			} else 
				rt->FillRectangle(cr,brush);

			if(dx != 0 || dy != 0)
				brush->SetTransform(D2D1::Matrix3x2F::Identity());
		}
	}

	ID2D1BitmapBrush* BorderImageRenderObject::getPortionBrush(ID2D1RenderTarget* rt, Portion p)
	{
		if(lastUsedBitmap != *borderImage)
		{
			// If the borderImage has been recreated, we must recreate all
			// the portion brushes.
			for(int i = 0; i < 6; ++i)
				SafeRelease(portionBrushes[i]);

			ZeroMemory(portionBrushes, sizeof(ID2D1BitmapBrush*)*6);
			lastUsedBitmap = *borderImage;
		} else if(portionBrushes[p] != 0) {
			// The cache is still valid, so we just return the cahced brush.
			return portionBrushes[p];
		}

		ID2D1BitmapBrush* pb = 0;
		if(p == Conner)
		{
			rt->CreateBitmapBrush(lastUsedBitmap,
								  D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP,D2D1_EXTEND_MODE_WRAP),
								  D2D1::BrushProperties(), &pb);
			portionBrushes[Conner] = pb;
			return pb;
		}

		D2D1_RECT_F pbmpRect;
		D2D1_SIZE_U pbmpSize(imageSize());
		switch(p)
		{
			case TopCenter:
				pbmpRect.left   = (FLOAT)leftWidth;
				pbmpRect.right  = (FLOAT)pbmpSize.width - leftWidth;
				pbmpRect.top    = 0.f; 
				pbmpRect.bottom = (FLOAT)topWidth;
				pbmpSize.width  = int(pbmpRect.right - rightWidth);
				pbmpSize.height = topWidth;
				break;
			case CenterLeft:
				pbmpRect.left   = 0.f;
				pbmpRect.right  = (FLOAT)leftWidth;
				pbmpRect.top    = (FLOAT)topWidth;
				pbmpRect.bottom = (FLOAT)pbmpSize.height - bottomWidth;
				pbmpSize.width  = leftWidth;
				pbmpSize.height = int(pbmpRect.bottom - topWidth);
				break;
			case Center:
				pbmpRect.left   = (FLOAT)leftWidth;
				pbmpRect.right  = (FLOAT)pbmpSize.width  - rightWidth;
				pbmpRect.top    = (FLOAT)topWidth;
				pbmpRect.bottom = (FLOAT)pbmpSize.height - bottomWidth;
				pbmpSize.width  = int(pbmpRect.right  - leftWidth);
				pbmpSize.height = int(pbmpRect.bottom - topWidth);
				break;
			case CenterRight:
				pbmpRect.left   = (FLOAT)pbmpSize.width  - rightWidth;
				pbmpRect.right  = (FLOAT)pbmpSize.width;
				pbmpRect.top    = (FLOAT)topWidth;
				pbmpRect.bottom = (FLOAT)pbmpSize.height - bottomWidth;
				pbmpSize.width  = rightWidth;
				pbmpSize.height = int(pbmpRect.bottom - topWidth);
				break;
			case BottomCenter:
				pbmpRect.left   = (FLOAT)leftWidth;
				pbmpRect.right  = (FLOAT)pbmpSize.width  - rightWidth;
				pbmpRect.top    = (FLOAT)pbmpSize.height - bottomWidth;
				pbmpRect.bottom = (FLOAT)pbmpSize.height;
				pbmpSize.width  = int(pbmpRect.right  - leftWidth);
				pbmpSize.height = bottomWidth;
				break;
		}
		// There is a bug in ID2D1Bitmap::CopyFromBitmap(), we need to avoid this.
		ID2D1BitmapRenderTarget* intermediateRT;
		ID2D1Bitmap* portionBmp;
		D2D1_RECT_F intermediateRect = {pbmpRect.left,pbmpRect.top,pbmpRect.right,pbmpRect.bottom};
		HRESULT hr = rt->CreateCompatibleRenderTarget(D2D1::SizeF((FLOAT)pbmpSize.width,(FLOAT)pbmpSize.height),&intermediateRT);
		if(SUCCEEDED(hr))
		{
			intermediateRT->BeginDraw();
			intermediateRT->DrawBitmap(lastUsedBitmap,D2D1::RectF(0.f,0.f,(FLOAT)pbmpSize.width,(FLOAT)pbmpSize.height),
										1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
										pbmpRect);
			intermediateRT->EndDraw();
			intermediateRT->GetBitmap(&portionBmp);
			rt->CreateBitmapBrush(portionBmp,
								  D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP,D2D1_EXTEND_MODE_CLAMP),
								  D2D1::BrushProperties(), &pb);

			SafeRelease(portionBmp);
			SafeRelease(intermediateRT);
		}
		portionBrushes[p] = pb;
		return pb;
	}

	void RenderRuleData::drawBorderImage(ID2D1RenderTarget* rt,const RECT& widgetRect, const RECT& clipRect)
	{
		if(borderImageRO != 0)
		{
			// 0.TopLeft     1.TopCenter     2.TopRight
			// 3.CenterLeft  4.Center        5.CenterRight
			// 6.BottomLeft  7.BottomCenter  8.BottomRight
			bool drawParts[9] = {true,true,true,true,true,true,true,true,true};
			int x1 = widgetRect.left   + borderImageRO->leftWidth;
			int x2 = widgetRect.right  - borderImageRO->rightWidth;
			int y1 = widgetRect.top    + borderImageRO->topWidth;
			int y2 = widgetRect.bottom - borderImageRO->bottomWidth;
			D2D1_SIZE_U borderImageSize = borderImageRO->imageSize();
			FLOAT scaleX = FLOAT(x2 - x1) / (borderImageSize.width - borderImageRO->leftWidth - borderImageRO->rightWidth);
			FLOAT scaleY = FLOAT(y2 - y1) / (borderImageSize.height - borderImageRO->topWidth - borderImageRO->bottomWidth);
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
			D2D1_SIZE_U imageSize = borderImageRO->imageSize();
			D2D1_RECT_F drawRect;
			ID2D1BitmapBrush* brush = borderImageRO->getPortionBrush(rt,BorderImageRenderObject::Conner);
			// Assume we can always get a valid brush for Conner.
			if(drawParts[0]) {
				drawRect.left   = (FLOAT)widgetRect.left;
				drawRect.right  = (FLOAT)x1;
				drawRect.top    = (FLOAT)widgetRect.top;
				drawRect.bottom = (FLOAT)y1;
				brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
				rt->FillRectangle(drawRect,brush);
			}
			if(drawParts[2]) {
				drawRect.left   = (FLOAT)x2;
				drawRect.right  = (FLOAT)widgetRect.right;
				drawRect.top    = (FLOAT)widgetRect.top;
				drawRect.bottom = (FLOAT)y1;
				brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.right - imageSize.width, drawRect.top));
				rt->FillRectangle(drawRect,brush);
			}
			if(drawParts[6]) {
				drawRect.left   = (FLOAT)widgetRect.left;
				drawRect.right  = (FLOAT)x1;
				drawRect.top    = (FLOAT)y2;
				drawRect.bottom = (FLOAT)widgetRect.bottom;
				brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.bottom - imageSize.height));
				rt->FillRectangle(drawRect,brush);
			}
			if(drawParts[8]) {
				drawRect.left   = (FLOAT)x2;
				drawRect.right  = (FLOAT)widgetRect.right;
				drawRect.top    = (FLOAT)y2;
				drawRect.bottom = (FLOAT)widgetRect.bottom;
				brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.right - imageSize.width, drawRect.bottom - imageSize.height));
				rt->FillRectangle(drawRect,brush);
			}
			if(drawParts[1]) {
				drawRect.left   = (FLOAT)x1;
				drawRect.right  = (FLOAT)x2;
				drawRect.top    = (FLOAT)widgetRect.top;
				drawRect.bottom = (FLOAT)y1;
				brush = borderImageRO->getPortionBrush(rt,BorderImageRenderObject::TopCenter);
				if(brush) {
					D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
					if(!borderImageRO->repeatX)
						matrix = D2D1::Matrix3x2F::Scale(scaleX,1.0f) * matrix;
					brush->SetTransform(matrix);
					rt->FillRectangle(drawRect,brush);
				}
			}
			if(drawParts[3]) {
				drawRect.left   = (FLOAT)widgetRect.left;
				drawRect.right  = (FLOAT)x1;
				drawRect.top    = (FLOAT)y1;
				drawRect.bottom = (FLOAT)y2;
				brush = borderImageRO->getPortionBrush(rt,BorderImageRenderObject::CenterLeft);
				if(brush) {
					D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
					if(!borderImageRO->repeatY)
						matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleY) * matrix;
					brush->SetTransform(matrix);
					rt->FillRectangle(drawRect,brush);
				}
			}
			if(drawParts[4]) {
				drawRect.left   = (FLOAT)x1;
				drawRect.right  = (FLOAT)x2;
				drawRect.top    = (FLOAT)y1;
				drawRect.bottom = (FLOAT)y2;
				brush = borderImageRO->getPortionBrush(rt,BorderImageRenderObject::Center);
				if(brush) {
					D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
					if(!borderImageRO->repeatX)
						matrix = D2D1::Matrix3x2F::Scale(scaleX, borderImageRO->repeatY ? 1.0f : scaleY) * matrix; 
					else if(!borderImageRO->repeatY)
						matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleY) * matrix;
					brush->SetTransform(matrix);
					rt->FillRectangle(drawRect,brush);
				}
			}
			if(drawParts[5]) {
				drawRect.left   = (FLOAT)x2;
				drawRect.right  = (FLOAT)widgetRect.right;
				drawRect.top    = (FLOAT)y1;
				drawRect.bottom = (FLOAT)y2;
				brush = borderImageRO->getPortionBrush(rt,BorderImageRenderObject::CenterRight);
				if(brush) {
					D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
					if(!borderImageRO->repeatY)
						matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleY) * matrix;
					brush->SetTransform(matrix);
					rt->FillRectangle(drawRect,brush);
				}
			}
			if(drawParts[7]) {
				drawRect.left   = (FLOAT)x1;
				drawRect.right  = (FLOAT)x2;
				drawRect.top    = (FLOAT)y2;
				drawRect.bottom = (FLOAT)widgetRect.bottom;
				brush = borderImageRO->getPortionBrush(rt,BorderImageRenderObject::BottomCenter);
				if(brush) {
					D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
					if(!borderImageRO->repeatX)
						matrix = D2D1::Matrix3x2F::Scale(scaleX,1.0f) * matrix;
					brush->SetTransform(matrix);
					rt->FillRectangle(drawRect,brush);
				}
			}
		}
	}
} // namespace MetalBone







































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
