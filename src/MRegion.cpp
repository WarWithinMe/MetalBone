#include "MRegion.h"
namespace MetalBone
{
	bool MRegion::YNode::isEqualNodes(const YNode& node) const
	{
		if(!isEqualRanges(node))
			return false;

		return isIdentical(node);
	}

	bool MRegion::YNode::isIdentical(const YNode& node) const
	{
		const XNode* p1 = m_xCoorTree.FindMin();
		const XNode* p2 = node.m_xCoorTree.FindMin();
		while(true) {
			if(!p1)
				return !p2;
			if(!p2)
				return false;
			if(!p1->isEqualNodes(*p2))
				return false;
			p1 = m_xCoorTree.FindNext(*p1);
			p2 = node.m_xCoorTree.FindNext(*p2);
		}
		return true;
	}

	void MRegion::YNode::copy(const YNode& ynode)
	{
		M_ASSERT(isEmpty());
		for (const XNode* pxNode = ynode.m_xCoorTree.FindMin();
			pxNode != 0;
			pxNode = ynode.m_xCoorTree.FindNext(*pxNode))
		{
			XNode* pxDup = m_xCoorTree.Create(pxNode->m_Key);
			pxDup->m_End = pxNode->m_End;
		}
	}

	void MRegion::YNode::onNodeChanged(XNode* pNode)
	{
		M_ASSERT(pNode);

		while(true)
		{
			XNode* pVal = m_xCoorTree.FindPrev(*pNode);
			if (!pVal || (pVal->m_End != pNode->m_Key) )
				break;

			pVal->m_End = pNode->m_End;
			m_xCoorTree.Delete(pNode);
			pNode = pVal;
		}

		while (true)
		{
			XNode* pVal = m_xCoorTree.FindNext(*pNode);
			if (!pVal || (pNode->m_End != pVal->m_Key))
				break;

			pNode->m_End = pVal->m_End;
			m_xCoorTree.Delete(pVal);
		}
	}

	template<MRegion::OperationMode mode>
	inline void MRegion::YNode::perfOperation(const YNode& rgn, YNode* pIntersect)
	{
		assertValid();
		rgn.assertValid();

		// This function can still be optimized. When we are combining
		// two rect, we first check if they're not fully intersect.
		// If they are, we keep the intersect parts and add the parts
		// that they don't intersect to the tree. Then we search the 
		// tree to see if we can merge them. We could, however, simply
		// merge the two rects in the first place, so we don't have to
		// do all the "create/check/delete" stuff. 
		for (const XNode* pCurrent = rgn.m_xCoorTree.FindMin(); 
			pCurrent != 0;
			pCurrent = rgn.m_xCoorTree.FindNext(*pCurrent))
		{
			LONG key_left  = pCurrent->m_Key;
			LONG end_right = pCurrent->m_End;

			while(true)
			{
				// Find a node whose "left" is on the left side of pCurrent's "right"
				XNode* pVal = m_xCoorTree.FindSmaller(end_right);
				// If we found such a node and its "right" is on the right
				// side of pCurrent's "left"(i.e. they intersect)
				if(pVal && (pVal->m_End > key_left))
				{
					if(pVal->m_End > end_right)
					{
						addRange(end_right, pVal->m_End);
						pVal->m_End = end_right;
					} else if (pVal->m_End < end_right) {
						if(OMCombine == mode) //if(!code)
							onNodeChanged(addRange(pVal->m_End, end_right));

						end_right = pVal->m_End;

						if(OMCombine == mode) //if(!code)
							continue;
					}

					if(pVal->m_Key < key_left)
					{
						pVal->m_End = key_left;
						pVal = addRange(key_left, end_right);
					}

					M_ASSERT((pVal->m_Key >= key_left) && (pVal->m_End == end_right));

					if(OMIntersect == mode) // 2 == code)
					{
						XNode* pResNode = pIntersect->m_xCoorTree.Create(pVal->m_Key);
						pResNode->m_End = end_right;
						pIntersect->onNodeChanged(pResNode);
					}

				} else {
					if(OMCombine != mode) //if(code)
						break;
					pVal = addRange(key_left, end_right);
				}

				end_right = pVal->m_Key;

				if(OMCombine != mode) //if(code)
					m_xCoorTree.Delete(pVal);
				else
					onNodeChanged(pVal);

				if(key_left == end_right)
					break;
			}
		}

		assertValid();
		if(OMIntersect == mode) // 2 == code)
			pIntersect->assertValid();
	}

	void MRegion::YNode::assertValid() const 
	{
#ifdef MREGION_DEBUG
		const XNode* xPrev = NULL;
		for(const XNode* xNode = m_xCoorTree.FindMin();
			xNode != 0;
			xPrev = xNode, xNode = m_xCoorTree.FindNext(*xNode))
		{
			M_ASSERT(xNode->m_End > xNode->m_Key);
			if(xPrev) {
				M_ASSERT(xNode->m_Key >= xPrev->m_End);
				M_ASSERT((xNode->m_Key > xPrev->m_End) || !xNode->isEqualNodes(*xPrev));
			}
		}
#endif
	}





	void MRegion::addRect(const RECT& r) 
	{
		assertValid();
		if(isRectValidNonEmpty(r))
		{
			if(isEmpty()) {
				YNode* pyNode = m_yCoorTree.Create(r.top);
				pyNode->m_End = r.bottom;
				XNode* pxNode = pyNode->m_xCoorTree.Create(r.left);
				pxNode->m_End = r.right;
			} else if (!isRectFullyInside(r)) {
				MRegion newRgn;
				newRgn.addRect(r);
				combine(newRgn);
			}
		}
		assertValid();
	}

	void MRegion::combine(const MRegion& rgn)
		{ perfOperation<OMCombine>(rgn,0); }
	void MRegion::subtract(const MRegion& rgn)
		{ perfOperation<OMSubtract>(rgn, 0); }
	void MRegion::subtractEx(const MRegion& rgn, MRegion& rgnIntersect)
		{ perfOperation<OMIntersect>(rgn, &rgnIntersect); }
	void MRegion::intersect(const MRegion& rgn)
	{
		MRegion rgnDup;
		swap(rgnDup);
		rgnDup.perfOperation<OMIntersect>(rgn, this);
	}

	bool MRegion::isEqual(const MRegion& rgn) const
	{
		assertValid();
		rgn.assertValid();

		const YNode* p1 = m_yCoorTree.FindMin();
		const YNode* p2 = rgn.m_yCoorTree.FindMin();
		while(true)
		{
			if(p1 == 0)
				return p2 == 0;
			if(p2 == 0)
				return false;

			if(!p1->isEqualNodes(*p2))
				return false;
			p1 = m_yCoorTree.FindNext(*p1);
			p2 = rgn.m_yCoorTree.FindNext(*p2);
		}
		return true;
	}

	void MRegion::copyFrom(const MRegion& rgn)
	{
		if(&rgn == this)
			return;

		rgn.assertValid();
		clear();
		for (const YNode* pNode = rgn.m_yCoorTree.FindMin();
			pNode != 0;
			pNode = rgn.m_yCoorTree.FindNext(*pNode))
		{
			YNode* pDup = m_yCoorTree.Create(pNode->m_Key);
			pDup->m_End = pNode->m_End;
			pDup->copy(*pNode);
		}
		assertValid();
	}

	bool MRegion::isPointInside(const POINT& p) const
	{
		const YNode* yNode = m_yCoorTree.FindExactSmaller(p.y);
		if(yNode) {
			if(yNode->m_End > p.y) {
				const XNode* xNode = yNode->m_xCoorTree.FindExactSmaller(p.x);
				if(xNode && xNode->m_End > p.x)
					return true;
			}
		}
		return false;
	}

	bool MRegion::getBounds(RECT& r) const
	{
		assertValid();

		if(isEmpty()) {
			r.left = r.right = r.bottom = r.top = 0;
			return false;
		}

		const YNode* yNode = m_yCoorTree.FindMin();
		M_ASSERT(yNode);
		r.top = yNode->m_Key;
		for(bool first = true;;first = false)
		{
			const XNode* xNode = yNode->m_xCoorTree.FindMin();
			M_ASSERT(xNode);
			if(first || (r.left > xNode->m_Key))
				r.left = xNode->m_Key;

			xNode = yNode->m_xCoorTree.FindMax();
			if(first || (r.right < xNode->m_End))
				r.right = xNode->m_End;

			const YNode* yNext = m_yCoorTree.FindNext(*yNode);
			if(!yNext) break;
			yNode = yNext;
		}
		r.bottom = yNode->m_End;
		return true;
	}

	bool MRegion::isRectFullyInside(const RECT& r) const
	{
		if(!isRectValidNonEmpty(r))
			return false;

		const YNode* pyNode = m_yCoorTree.FindExactSmaller(r.top);
		if(!pyNode)
			return false;

		while(true) {
			const XNode* pxNode = pyNode->m_xCoorTree.FindExactSmaller(r.left);
			if(!pxNode || pxNode->m_End < r.right)
				return false;

			if(pyNode->m_End >= r.bottom)
				break;

			const YNode* pyNext = m_yCoorTree.FindNext(*pyNode);
			// If we cannot find another rect, or if there's space between
			// found Rect and the next found Rect(i.e. the bottom of found
			// Rect and the top of the next found Rect is not the same),
			// then the test Rect is not fully inside this region.
			if(!pyNext || (pyNext->m_Key != pyNode->m_End))
				return false;
			pyNode = pyNext;
		}
		return true;
	}

	bool MRegion::isRectPartlyInside(const RECT& r) const
	{
		for(const YNode* yNode = m_yCoorTree.FindSmaller(r.bottom);
			yNode != 0 && (yNode->m_End > r.top); 
			yNode = m_yCoorTree.FindPrev(*yNode))
		{
			const XNode* xNode = yNode->m_xCoorTree.FindSmaller(r.right);
			if(xNode != 0 && xNode->m_End > r.left)
				return true;
		}
		return false;
	}

	MRegion::YNode* MRegion::addRange(LONG start, LONG end, const YNode& y)
	{
		M_ASSERT(start < end);

		YNode* pyNode = m_yCoorTree.Create(start);
		pyNode->m_End = end;
		pyNode->copy(y);

		return pyNode;
	}

	void MRegion::onNodeChanged(YNode* pNode)
	{
		M_ASSERT(pNode);
		while (true)
		{
			YNode* pVal = m_yCoorTree.FindPrev(*pNode);
			if(!pVal ||
				(pVal->m_End != pNode->m_Key) ||
				!pVal->isIdentical(*pNode))
				break;

			pVal->m_End = pNode->m_End;
			m_yCoorTree.Delete(pNode);
			pNode = pVal; 
		}

		while (true)
		{
			YNode* pVal = m_yCoorTree.FindNext(*pNode);
			if(!pVal ||
				(pNode->m_End != pVal->m_Key) ||
				!pVal->isIdentical(*pNode))
				break;

			pNode->m_End = pVal->m_End;
			m_yCoorTree.Delete(pVal);
		}
	}

	template <MRegion::OperationMode mode>
	void MRegion::perfOperation(const MRegion& rgn, MRegion* pIntersect)
	{
		assertValid();
		rgn.assertValid();

		for(const YNode* pyCurrent = rgn.m_yCoorTree.FindMin();
			pyCurrent != 0;
			pyCurrent = rgn.m_yCoorTree.FindNext(*pyCurrent))
		{
			LONG key_top    = pyCurrent->m_Key;
			LONG end_bottom = pyCurrent->m_End;

			while(true)
			{
				// Find a node that its "top" is above the pyCurrent's "bottom"
				YNode* pVal = m_yCoorTree.FindSmaller(end_bottom);
				// If there's such a node, and the node's "bottom" is under
				// pyCurrent's "top", i.e. they intersect.
				if(pVal && (pVal->m_End > key_top))
				{
					// If this node's "bottom"(b1) is under pyCurrent's "bottom"(b2)
					// We take out the part between b2~b1 of this node, insert it
					// into the tree. And this node is truncated. 
					if(pVal->m_End > end_bottom)
					{
						addRange(end_bottom, pVal->m_End, *pVal);
						pVal->m_End = end_bottom;
					} else if(pVal->m_End < end_bottom) {

						// If this node's "bottom"(b1) is above pyCurrent's "bottom"(b2),
						// and if we are combining two region, we add the part between
						// b1~b2 of the pyCurrent into the tree.
						// pyCurrent is always trucated.

						if(OMCombine == mode) //if(code == 0 /*!code*/)
							onNodeChanged(addRange(pVal->m_End, end_bottom, *pyCurrent));

						end_bottom = pVal->m_End;
						if(OMCombine == mode) //if(code == 0 /*!code*/)
							continue;
					}

					// If this node's "top"(t1) is above pyCurrent's "top"(t2),
					// We take out the part between t2~t1 of this node, insert it
					// into the tree. And this node is truncated.
					if(pVal->m_Key < key_top)
					{
						pVal->m_End = key_top;
						pVal = addRange(key_top, end_bottom, *pVal);
					}

					M_ASSERT((pVal->m_Key >= key_top) && (pVal->m_End == end_bottom));

					if(OMIntersect == mode) // 2 == code)
					{
						YNode rgnRes;
						pVal->perfOperation<mode>(*pyCurrent, &rgnRes);

						if(!rgnRes.isEmpty())
						{
							YNode* pResNode = pIntersect->m_yCoorTree.Create(pVal->m_Key);
							pResNode->m_End = end_bottom;
							pResNode->swap(rgnRes);

							pIntersect->onNodeChanged(pResNode);
						}
					} else {
						pVal->perfOperation<mode>(*pyCurrent, 0);
					}

				} else {
					if(OMCombine != mode) // if(code != 0)
						break;
					// If we don't find Nodes that intersect with pyCurrent,
					// and we're combing two regions, we simply add the pyCurrent
					// into the tree.
					pVal = addRange(key_top, end_bottom, *pyCurrent);
				}

				end_bottom = pVal->m_Key;

				if(OMCombine != mode && pVal->isEmpty()) //if(code != 0 && pVal->isEmpty())
					m_yCoorTree.Delete(pVal);
				else
					onNodeChanged(pVal);

				if(key_top == end_bottom)
					break;
			}
		}

		assertValid();
		if(OMIntersect == mode) // if(2 == code)
			pIntersect->assertValid();
	}

	void MRegion::assertValid() const
	{
#ifdef MREGION_DEBUG
		const YNode* yPrev = NULL;
		for(const YNode* yNode = m_yCoorTree.FindMin();
			yNode != 0;
			yPrev = yNode, yNode = m_yCoorTree.FindNext(*yNode))
		{
			yNode->assertValid();

			M_ASSERT(yNode->m_End > yNode->m_Key);
			if(yPrev) {
				M_ASSERT(yNode->m_Key >= yPrev->m_End);
				M_ASSERT((yNode->m_Key > yPrev->m_End) || !yNode->isEqualNodes(*yPrev));
			}
		}
#endif
	}

	void MRegion::offset(LONG x, LONG y)
	{
		// In the original version of this region class,
		// offseting a region can cause overflow happens.
		// But I don't think this will happen in a normal GUI app.
		// So, simply ignore the overflow problem.
		for(YNode* yNode = m_yCoorTree.FindMin(); yNode; yNode = m_yCoorTree.FindNext(*yNode))
		{
			yNode->m_Key += y;
			yNode->m_End += y;
			for(XNode* xNode = yNode->m_xCoorTree.FindMin(); xNode; xNode = yNode->m_xCoorTree.FindNext(*xNode))
			{
				xNode->m_Key += x;
				xNode->m_End += x;
			}
		}
	}

	MRegion::Iterator MRegion::begin() const
	{
		Iterator it;
		it.ynode = m_yCoorTree.FindMin();
		if(it.ynode != 0) it.xnode = it.ynode->m_xCoorTree.FindMin();
		return it;
	}

	MRegion::Iterator& MRegion::Iterator::operator++()
	{
		xnode = ynode->m_xCoorTree.FindNext(*xnode);
		if(xnode != 0)
			return *this;

		ynode = ThirdParty::Container::TreeEx<MRegion::CoorTree,MRegion::YNode>::FindNext(*ynode);
		if(ynode != 0)
			xnode = ynode->m_xCoorTree.FindMin();

		return *this;
	}
}
