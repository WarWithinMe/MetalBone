#include "MStyleSheet.h"
#include "MResource.h"
#include "MWidget.h"
#include "MD2DPaintContext.h"
#include "private/CSSParser.h"
#include "private/RenderRuleData.h"

#include <sstream>
#include <map>

namespace MetalBone
{
    // ========== MSSSPrivate ==========
    using namespace CSS;
    class MSSSPrivate
    {
        public:
            typedef multimap<PropertyType, Declaration*> DeclMap;

            MSSSPrivate ();
            ~MSSSPrivate();

        private:
            void setAppSS(const wstring&);
            void setWidgetSS(MWidget*, const wstring&);
            void clearRRCacheRecursively(MWidget*);
            void draw(MWidget*, const MRect&, const MRect&, const wstring&, int);
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
            typedef map<RenderRuleCacheKey,           PseudoRenderRuleMap>     RenderRuleMap;
            typedef unordered_map<MWidget*,           MatchedStyleRuleVector>  WidgetStyleRuleMap;
            typedef unordered_map<MWidget*,           PseudoRenderRuleMap*>    WidgetRenderRuleMap;
            typedef unordered_map<RenderRuleQuerier*, MatchedStyleRuleVector>  QuerierStyleRuleMap;
            typedef unordered_map<RenderRuleQuerier*, PseudoRenderRuleMap*>    QuerierRenderRuleMap;
            typedef unordered_map<MWidget*,           unsigned int>            AniWidgetIndexMap;

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
            RenderRuleMap        renderRuleCollection;
            MTimer               aniBGTimer;
            AniWidgetIndexMap    widgetAniBGIndexMap;

            static MSSSPrivate* instance;

            friend class MStyleSheetStyle;
    };

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
	void MStyleSheetStyle::draw(MWidget* w, const MRect& wr,
        const MRect& cr, const std::wstring& t,int i)
		{ mImpl->draw(w,wr,cr,t,i); }
	void MStyleSheetStyle::removeCache(MWidget* w)
		{ mImpl->removeCache(w); }
	void MStyleSheetStyle::removeCache(RenderRuleQuerier* q)
		{ mImpl->removeCache(q); }
	RenderRule MStyleSheetStyle::getRenderRule(MWidget* w, unsigned int p)
		{return mImpl->getRenderRule(w,p); }
	RenderRule MStyleSheetStyle::getRenderRule(RenderRuleQuerier* q, unsigned int p)
		{return mImpl->getRenderRule(q,p); }
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
				// Mark if the widget has a animated background.
				if(currRule->getTotalFrameCount() > 1)
				{
					if(lastRule->getTotalFrameCount() == 1)
						mImpl->addAniWidget(w);
				} else if(lastRule->getTotalFrameCount() > 1)
					mImpl->removeAniWidget(w);

				// Remark: we can update the widget's property here.
			}
		}

		// We always update the cursor.
		MCursor* cursor = w->getCursor();
		if(cursor == 0 && currRule) cursor = currRule.getCursor();
		if(cursor == 0) cursor = &gArrowCursor;
		cursor->show();
	}




	// ********** MSSSPrivate Implementation
	MSSSPrivate* MSSSPrivate::instance = 0;
    void MSSSPrivate::polish(MWidget* w)
    {
        RenderRule rule = getRenderRule(w, w->getWidgetPseudo());
        if(rule) rule->setGeometry(w);

        RenderRule hoverRule = getRenderRule(w, w->getWidgetPseudo(false,PC_Hover));
        if(hoverRule && hoverRule != rule)
            w->setAttributes(WA_Hover);

        w->ssSetOpaque(rule.opaqueBackground());
    }

	void MSSSPrivate::draw(MWidget* w, const MRect& wr, 
        const MRect& cr, const wstring& t,int frameIndex)
	{
		RenderRule rule = getRenderRule(w,w->getWidgetPseudo(true));
        if(!rule) return;

		unsigned int ruleFrameCount = rule->getTotalFrameCount();
		unsigned int index = frameIndex >= 0 ? frameIndex : 0;
		if(ruleFrameCount > 1 && frameIndex == -1)
		{
			AniWidgetIndexMap::iterator it = widgetAniBGIndexMap.find(w);
			AniWidgetIndexMap::iterator itEnd = widgetAniBGIndexMap.end();

			if(it != itEnd)
			{
				index = it->second;
				if(index >= ruleFrameCount)
				{
					if(rule->isBGSingleLoop())
					{
						index = ruleFrameCount - 1;
						removeAniWidget(w);
					} else {
						index = 0;
						widgetAniBGIndexMap[w] = 1;
					}
				} else { widgetAniBGIndexMap[w] = index + 1; }
			} else {
				index = ruleFrameCount - 1;
			}
		}

        MD2DPaintContext context(w);
		rule->draw(context,wr,cr,t,index);
		if(ruleFrameCount != rule->getTotalFrameCount())
		{
			// The total frame count might change after calling draw();
			if(rule->getTotalFrameCount() > 1)
				addAniWidget(w);
			else
				removeAniWidget(w);
		}
	}
	void MSSSPrivate::updateAniWidgets()
	{
		AniWidgetIndexMap::iterator it    = widgetAniBGIndexMap.begin();
		AniWidgetIndexMap::iterator itEnd = widgetAniBGIndexMap.end();

		while(it != itEnd)
		{
			it->first->repaint();
			++it;
		}
	}
	void MSSSPrivate::addAniWidget(MWidget* w)
	{
		widgetAniBGIndexMap[w] = 0;
		if(!aniBGTimer.isActive())
			aniBGTimer.start(200);
	}
	void MSSSPrivate::removeAniWidget(MWidget* w)
	{
		widgetAniBGIndexMap.erase(w);
		if(widgetAniBGIndexMap.size() == 0)
			aniBGTimer.stop();
	}

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

	bool basicSelectorMatches(const BasicSelector* bs, RenderRuleQuerier* q)
	{
		if(!bs->id.empty() && q->objectName() != bs->id)
			return false;

		if(!bs->elementName.empty()) {
			if(bs->elementName != q->className())
				return false;
		}
		return true;
	}

	template<class Querier>
	bool selectorMatches(const Selector* sel, Querier* w)
	{
		const vector<BasicSelector*>& basicSels = sel->basicSelectors;
		if(basicSels.size() == 1)
			return basicSelectorMatches(basicSels.at(0),w);

		// first element must always match!
		if(!basicSelectorMatches(basicSels.at(basicSels.size() - 1), w))
			return false;

		w = w->parent();

		if(w == 0) return false;

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

	template<class Querier>
	void matchRule(Querier* w,const StyleRule* rule,int depth,
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
		vector<StyleSheet*> sheets;
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

	void MSSSPrivate::getMachedStyleRules(RenderRuleQuerier* querier, MatchedStyleRuleVector& srs)
	{
		// Get all possible stylesheets
		multimap<int, MatchedStyleRule> weightedRules;
		const StyleSheet* sheet = appStyleSheet;
		// search for universal rules.
		for(unsigned int i = 0;i< sheet->universal.size(); ++i) {
			matchRule(querier,sheet->universal.at(i),1,weightedRules);
		}

		// check for Id
		if(!sheet->srIdMap.empty() && !querier->objectName().empty())
		{
			pair<StyleSheet::StyleRuleIdMap::const_iterator,
				StyleSheet::StyleRuleIdMap::const_iterator> result =
				sheet->srIdMap.equal_range(querier->objectName());
			while(result.first != result.second)
			{
				matchRule(querier,result.first->second,1,weightedRules);
				++result.first;
			}
		}

		// check for class name
		if(!sheet->srElementMap.empty())
		{
			pair<StyleSheet::StyleRuleElementMap::const_iterator,
				StyleSheet::StyleRuleElementMap::const_iterator> result =
				sheet->srElementMap.equal_range(querier->className());
			while(result.first != result.second)
			{
				matchRule(querier,result.first->second,1,weightedRules);
				++result.first;
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

	template<class Querier, class QuerierRRMap, class QuerierSRMap>
	RenderRule MSSSPrivate::getRenderRule(Querier* q, unsigned int pseudo,
			QuerierRRMap& querierRRCache, QuerierSRMap& querierSRCache)
	{
		// == 1.Find RenderRule for MWidget w from Widget-RenderRule cache.
		QuerierRRMap::iterator querierCacheIter = querierRRCache.find(q);
		PseudoRenderRuleMap* prrMap = 0;
		if(querierCacheIter != querierRRCache.end())
		{
			prrMap = querierCacheIter->second;
			PseudoRenderRuleMap::Element::iterator prrIter = prrMap->element.find(pseudo);
			if(prrIter != prrMap->element.end())
				return prrIter->second;
		}

		// == 2.Find StyleRules for MWidget w from Widget-StyleRule cache.
		QuerierSRMap::iterator qsrIter = querierSRCache.find(q);
		MatchedStyleRuleVector* matchedsrv;
		if(qsrIter != querierSRCache.end()) {
			matchedsrv = &(qsrIter->second);
		} else {
			matchedsrv = &(querierSRCache.insert(
				QuerierSRMap::value_type(q, MatchedStyleRuleVector())).first->second);
			getMachedStyleRules(q,*matchedsrv);
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
			querierRRCache.insert(QuerierRRMap::value_type(q,prrMap));
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
        bool shareWidthRealPseudo = false;
		if(realPseudo != pseudo)
		{
			hasRRIter = prrMap->element.find(realPseudo);
			if(hasRRIter != prrMap->element.end())
			{
				renderRule = hasRRIter->second;
				return renderRule;
			} else
            {
                shareWidthRealPseudo = true;
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
		renderRule.init();
        renderRule->init(declarations);

        if(shareWidthRealPseudo) prrMap->element[realPseudo] = renderRule;

		return renderRule;
	}


	MSSSPrivate::MSSSPrivate():appStyleSheet(new StyleSheet())
	{ 
		instance = this;
		aniBGTimer.timeout.Connect(this,&MSSSPrivate::updateAniWidgets);
	}
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
	}

	void MSSSPrivate::setAppSS(const wstring& css)
	{
		delete appStyleSheet;
		renderRuleCollection.clear();
		widgetStyleRuleCache.clear();
		widgetRenderRuleCache.clear();
		querierStyleRuleCache.clear();
		querierRenderRuleCache.clear();

		MCSSParser parser;
		appStyleSheet = parser.parse(css);
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
			MCSSParser parser;
			widgetSSCache.insert(WidgetSSCache::value_type(w,parser.parse(css)));
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
} // namespace MetalBone
