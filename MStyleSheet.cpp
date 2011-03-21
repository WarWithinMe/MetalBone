#include "MStyleSheet.h"
#include "MWidget.h"
#include "MResource.h"
#include "externutils/XUnzip.h"

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
				buffer = (const BYTE*) LockResource(hglobal);
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
		ResourceCache::const_iterator iter = cache.find(fileName);
		if(iter == cache.cend())
			return false;

		buffer = iter->second->buffer;
		size = iter->second->length;
		return true;
	}

	void MResource::clearCache()
	{
		ResourceCache::const_iterator iter = cache.cbegin();
		ResourceCache::const_iterator iterEnd = cache.cend();
		while(iter != iterEnd)
		{
			delete iter->second;
			++iter;
		}

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
		newEntry->unity = true;
		newEntry->buffer = new BYTE[size];

		DWORD readSize;
		if(ReadFile(resFile,newEntry->buffer,size,&readSize,0))
		{
			if(readSize != size)
				delete newEntry;
			else
			{
				cache.insert(ResourceCache::value_type(filePath, newEntry));
				return true;
			}
		}else
			delete newEntry;
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
		}else
			success = false;

		CloseZip(zip);
		return success;
	}





	// ========== StyleSheet ==========
	namespace CSS
	{
		struct CssValue {
			enum Type { Unknown, Number, Length, Identifier, Uri, Color };

			union Variant {
				int vint;
				unsigned int vuint;
				std::wstring* vstring;
			};

			CssValue():type(Unknown){}
			Type type;
			Variant data;
		};

		// 1. StyleRule		- x:hover , y:clicked > z:checked { prop1: value1; prop2: value2; }
		// 2. Selector		- x:hover | y:clicked > z:checked
		// 3. BasicSelector	- x:hover | y:clicked | z:checked
		// 4. Declaration	- prop1: value1; | prop2: value2;
		struct Declaration
		{
			~Declaration();
			void addValue(const std::wstring& css, int index, int length);

			PropertyType property;
			std::vector<CssValue> values;
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
			Relation relationToNext;
		};

		struct Selector
		{
			Selector(const std::wstring* css,int index = 0,int length = -1);
			~Selector();

			std::vector<BasicSelector*> basicSelectors;
			int specificity() const;

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
				int pos;
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

		bool RenderRuleCacheKey::operator<(const RenderRuleCacheKey& rhs) const
		{
			int size1 = styleRules.size();
			int size2 = rhs.styleRules.size();
			if(size1 > size2)
				return false;
			else if(size1 < size2)
				return true;
			else
			{
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
	}

	using namespace CSS;
	static const CSSValuePair pseudos[knownPseudoCount] = {
		{ L"active",		PC_Active },
		{ L"checked",	PC_Checked },
		{ L"default",	PC_Default },
		{ L"disabled",	PC_Disabled },
		{ L"edit-focus",	PC_EditFocus },
		{ L"editable",	PC_Editable },
		{ L"enabled",	PC_Enabled },
		{ L"first",		PC_First },
		{ L"focus",		PC_Focus },
		{ L"has-children",PC_Children },
		{ L"has-siblings", PC_Sibling },
		{ L"horizontal", PC_Horizontal },
		{ L"hover",		PC_Hover },
		{ L"last",		PC_Last },
		{ L"pressed",	PC_Pressed },
		{ L"read-only",	PC_ReadOnly },
		{ L"selected",	PC_Selected },
		{ L"unchecked",	PC_Unchecked },
		{ L"vertical",	PC_Vertical }
	};

	static const CSSValuePair properties[knownPropertyCount] = {
		{ L"background",			PT_Background },
		{ L"background-attachment",	PT_BackgroundAttachment },
		{ L"background-clip",		PT_BackgroundClip },
		{ L"background-color",		PT_BackgroundColor },
		{ L"background-image",		PT_BackgroundImage },
		{ L"background-origin",		PT_BackgroundOrigin },
		{ L"background-position",	PT_BackgroundPosition },
		{ L"background-repeat",		PT_BackgroundRepeat },
		{ L"border",				PT_Border },
		{ L"border-bottom",			PT_BorderBottom },
		{ L"border-bottom-color",	PT_BorderBottomColor },
		{ L"border-bottom-left-radius",	PT_BorderBottomLeftRadius },
		{ L"border-bottom-right-radius",	PT_BorderBottomRightRadius },
		{ L"border-bottom-style",	PT_BorderBottomStyle },
		{ L"border-bottom-width",	PT_BorderBottomWidth },
		{ L"border-color",			PT_BorderColor },
		{ L"border-image",			PT_BorderImage },
		{ L"border-left",			PT_BorderLeft },
		{ L"border-left-color",		PT_BorderLeftColor },
		{ L"border-left-style",		PT_BorderLeftStyle },
		{ L"border-left-width",		PT_BorderLeftWidth },
		{ L"border-radius",			PT_BorderRadius },
		{ L"border-right",			PT_BorderRight },
		{ L"border-right-color",		PT_BorderRightColor },
		{ L"border-right-style",		PT_BorderRightStyle },
		{ L"border-right-width",		PT_BorderRightWidth },
		{ L"border-style",			PT_BorderStyles },
		{ L"border-top",				PT_BorderTop },
		{ L"border-top-color",		PT_BorderTopColor },
		{ L"border-top-left-radius",	PT_BorderTopLeftRadius },
		{ L"border-top-right-radius",	PT_BorderTopRightRadius },
		{ L"border-top-style",		PT_BorderTopStyle },
		{ L"border-top-width",		PT_BorderTopWidth },
		{ L"border-width",			PT_BorderWidth },
		{ L"color",					PT_Color },
		{ L"font",					PT_Font },
		{ L"font-family",			PT_FontFamily },
		{ L"font-size",				PT_FontSize },
		{ L"font-style",				PT_FontStyle },
		{ L"font-weight",			PT_FontWeight },
		{ L"height",				PT_Height },
		{ L"margin",					PT_Margin },
		{ L"margin-bottom",			PT_MarginBottom },
		{ L"margin-left",			PT_MarginLeft },
		{ L"margin-right",			PT_MarginRight },
		{ L"margin-top",			PT_MarginTop },
		{ L"max-height",			PT_MaximumHeight },
		{ L"max-width",				PT_MaximumWidth },
		{ L"min-height",			PT_MinimumHeight },
		{ L"min-width",				PT_MinimumWidth },
		{ L"padding",				PT_Padding },
		{ L"padding-bottom",		PT_PaddingBottom },
		{ L"padding-left",			PT_PaddingLeft },
		{ L"padding-right",			PT_PaddingRight },
		{ L"padding-top",			PT_PaddingTop },
		{ L"text-align",				PT_TextAlignment },
		{ L"text-decoration",		PT_TextDecoration },
		{ L"text-indent",			PT_TextIndent },
		{ L"text-outline",			PT_TextOutline },
		{ L"text-overflow",			PT_TextOverflow },
		{ L"text-shadow",			PT_TextShadow },
		{ L"text-underline-style",	PT_TextUnderlineStyle },
		{ L"width",					PT_Width }
	};

	static const CSSValuePair knownValues[KnownValueCount - 1] = {
		{ L"bold",		Value_Bold },
		{ L"border",	Value_Border },
		{ L"bottom",	Value_Bottom },
		{ L"center",	Value_Center },
		{ L"clip",		Value_Clip },
		{ L"content",	Value_Content },
		{ L"dashed",	Value_Dashed },
		{ L"dot-dash",	Value_DotDash },
		{ L"dot-dot-dash",	Value_DotDotDash },
		{ L"dotted",	Value_Dotted },
		{ L"double",		Value_Double },
		{ L"ellipsis",	Value_Ellipsis },
		{ L"italic",	Value_Italic },
		{ L"left",		Value_Left },
		{ L"line-through",	Value_LineThrough },
		{ L"margin",		Value_Margin },
		{ L"none",		Value_None },
		{ L"normal",	Value_Normal },
		{ L"oblique",	Value_Oblique },
		{ L"overline",	Value_Overline },
		{ L"padding",	Value_Padding },
		{ L"repeat",	Value_Repeat },
		{ L"right",		Value_Right },
		{ L"solid",		Value_Solid },
		{ L"stretch",	Value_Stretch },
		{ L"top",		Value_Top },
		{ L"transparent",	Value_Transparent },
		{ L"underline",	Value_Underline },
		{ L"wave",		Value_Wave },
		{ L"wrap",		Value_Wrap },
	};

	const CSSValuePair* findCSSValue(const std::wstring& p, const CSSValuePair values[],int valueCount)
	{
		const CSSValuePair* crItem = 0;
		int left = 0;
		--valueCount; //int right = valueCount - 1;

		while(left <= valueCount /*right*/)
		{
			int middle = (left + valueCount /*right*/) >> 1;
			crItem = &(values[middle]);
			int result = wcscmp(crItem->name,p.c_str());
			if(result == 0)
				return crItem;
			else if(result == 1)
				valueCount /*right*/ = middle - 1;
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
			if(values.at(i).type == CssValue::Uri)
				delete values.at(i).data.vstring;
		}
	}

	MCSSParser::MCSSParser(const std::wstring& c):
		css(&c),cssLength(c.size()),
		pos(cssLength - 1),noSelector(false)
	{
		// 测试Declaration Block是否带有Selector
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
			// 如果selector分析成功，则分析declaration。
			// 如果declaration分析不成功，则保证declaration为空，
			// 从而删除创建了的StyleRule
			if(parseSelector(newStyleRule))
				parseDeclaration(newStyleRule);

			if(newStyleRule->declarations.empty())
				delete newStyleRule;
			else
			{
				const std::vector<Selector*>& sels = newStyleRule->selectors;
				for(unsigned int i = 0; i < sels.size(); ++i)
				{
					const Selector* sel = sels.at(i);
					const BasicSelector* bs = sel->basicSelectors.at(sel->basicSelectors.size() - 1);
					if(!bs->id.empty())
						ss->srIdMap.insert(StyleSheet::StyleRuleIdMap::value_type(bs->id,newStyleRule));
					else if(!bs->elementName.empty())
						ss->srElementMap.insert(StyleSheet::StyleRuleElementMap::value_type(bs->elementName,newStyleRule));
					else
						ss->universal.push_back(newStyleRule);
				}
			}

			ss->styleRules.push_back(newStyleRule);

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
			if(byte == L'{') // 遇到Declaration Block
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
					sr->declarations.push_back(d);

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
				}else
				{
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

			// 遇到:pressed{ 这种情况的话，退出while loop的时候，
			// 最后一个pseudo没有添加进去BasicSelector里面。
			if(!pseudo.empty())
				sel->addPseudoAndClearInput(pseudo);

			if(index == length)
				sel->relationToNext = BasicSelector::NoRelation;

			// 现在只能够识别类名，所以.ClassA和ClassA是一样的
			// TODO:添加类型识别
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

	// Background:		{ Brush Repeat Clip Image Origin Alignment }*;
	// Border:			none | (LineStyle Brush Lengths)
	// Border-radius:	Lengths
	// Border-image:		none | Url Number{4} (stretch | repeat){0,2} // should be used as background & don't have to specify border width
	// Color:			#rrggbb rgba(255,0,0,0) transparent
	// Font:			(FontStyle | FontWeight){0,2} Length String
	// FontStyle:		normal | italic | oblique
	// FontWeight:		normal | bold
	// Margin:			Length{1,4}
	// Text-align:		Alignment;
	// Text-decoration:	none | underline | overline | line-through
	// Text-overflow:	clip | ellipsis | wrap
	// Text-Shadow:		h-shadow v-shadow blur Color;
	// Text-underline-style: LineStyle
	// Brush:			Color | Gradient
	// Gradient:
	// Repeat:			repeat | repeat-x | repeat-y | no-repeat
	// Clip/Origin:		padding | border | content | margin
	// Image:			url(filename)
	// Alignment:		(left | top | right | bottom | center){0,2}
	// LineStyle:		dashed | dot-dash | dot-dot-dash | dotted | solid | wave
	// Length:			Number // do not support unit and the default unit is px
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
					}else if(iswdigit(buffer.at(0)) || buffer.at(0) == L'-') // Number
					{
						value = CssValue();
						value.type = CssValue::Number;
						value.data.vint = _wtoi(buffer.c_str());
						values.push_back(value);
						buffer.clear();
					}else // Identifier
					{
						const CSSValuePair* crItem = findCSSValue(buffer,knownValues,KnownValueCount - 1);
						if(crItem != 0)
						{
							value = CssValue();
							value.type = CssValue::Identifier;
							value.data.vuint = crItem->value;
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
	} // namespace CSS

	MStyleSheetStyle::~MStyleSheetStyle()
	{
		delete appStyleSheet;
		WidgetSSCache::iterator it = widgetSSCache.begin();
		WidgetSSCache::iterator itEnd = widgetSSCache.end();
		while(it != itEnd)
		{
			delete it->second;
			++it;
		}
	}

	void MStyleSheetStyle::setAppSS(const std::wstring& css)
	{
		delete appStyleSheet;
		appStyleSheet = new StyleSheet();
		MCSSParser parser(css);
		parser.parse(appStyleSheet);

		windowRenderRuleCache.clear();
		widgetStyleRuleCache.clear();
		widgetRenderRuleCache.clear();
	}

	void MStyleSheetStyle::clearRenderRuleCacheRecursively(MWidget* w)
	{
		widgetStyleRuleCache.erase(w);
		widgetRenderRuleCache.erase(w);
		const std::list<MWidget*>& children = w->children();
		std::list<MWidget*>::const_iterator it = children.begin();
		std::list<MWidget*>::const_iterator itEnd = children.end();
		while(it != itEnd)
		{
			clearRenderRuleCacheRecursively(*it);
			++it;
		}
	}

	void MStyleSheetStyle::setWidgetSS(MWidget* w, const std::wstring& css)
	{
		removeWidgetSS(w);

		if(!css.empty())
		{
			StyleSheet* ss = new StyleSheet();
			MCSSParser parser(css);
			parser.parse(ss);
			widgetSSCache.insert(WidgetSSCache::value_type(w,ss));
		}

		// 清空这个widget的所有子widget的缓存
		clearRenderRuleCacheRecursively(w);
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
		while( (pos = name.find(L"::",pos)) != std::wstring::npos)
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
		if(widget->testAttributes(WA_NoStyleSheet))
			return;

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
			if(!widget->objectName().empty())
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

	RenderRule MStyleSheetStyle::getStyleObject(MWidget* w, unsigned int pseudo)
	{
		// 0.Find RenderRule for MWidget w from widget renderrule cache.
		WidgetRenderRuleMap::iterator widgetCacheIter = widgetRenderRuleCache.find(w);
		PseudoRenderRuleMap* prrMap = 0;
		if(widgetCacheIter != widgetRenderRuleCache.end())
		{
			prrMap = widgetCacheIter->second;
			PseudoRenderRuleMap::Element::iterator prrIter = prrMap->element.find(pseudo);
			if(prrIter != prrMap->element.end())
				return prrIter->second; // Found sth. we are insterested in.
		}

		/* No RenderRule yet. Build it. */
		// 1.Find StyleRules for MWidget w from widget stylerule cache.
		WidgetStyleRuleMap::iterator wsrIter = widgetStyleRuleCache.find(w);
		MatchedStyleRuleVector* matchedsrv;
		if(wsrIter != widgetStyleRuleCache.end())
			matchedsrv = &(wsrIter->second);
		else
		{
			matchedsrv = &(widgetStyleRuleCache.insert(
							   WidgetStyleRuleMap::value_type(
								   w, MatchedStyleRuleVector())).first->second);
			getMachedStyleRules(w,*matchedsrv);
		}

		// 2.Ensure we have a cache for the window containing MWidget w.
		RenderRuleMap& rrMap = windowRenderRuleCache[w->windowHandle()];

		// 3.If we don't have a PseudoRenderRuleMap, build one.
		if(prrMap == 0)
		{
			RenderRuleCacheKey cacheKey;
			int srvSize = matchedsrv->size();
			cacheKey.styleRules.reserve(srvSize);
			for(int i = 0; i< srvSize; ++i)
				cacheKey.styleRules.push_back(const_cast<StyleRule*>(matchedsrv->at(i).styleRule));
			prrMap = &(rrMap.element[cacheKey]);
			widgetRenderRuleCache.insert(WidgetRenderRuleMap::value_type(w,prrMap));
		}

		// 4.Build the RenderRule
		RenderRule& renderRule = prrMap->element[pseudo];
		M_ASSERT(!renderRule.isValid()); // At this point, render rule must be invalid, or sth. went wrong.
		MatchedStyleRuleVector::const_reverse_iterator msrIter = matchedsrv->rbegin();
		MatchedStyleRuleVector::const_reverse_iterator msrIterEnd = matchedsrv->rend();
		std::tr1::unordered_multimap<PseudoClassType,Declaration*> declarations;
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

				while(declIter != declIterEnd)
				{
					// Merge declaration
					++declIter;
				}
			}
			++msrIter;
		}

		// TODO: create renderobject
		return renderRule;
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
