#pragma once
/*
 * This class is originally developed by Vladislav Gelfer. The original source code
 * can be obtain from the article - Advanced multi-dimensional region template class
 * on CodeProject.com (http://www.codeproject.com/KB/graphics/rgnex.aspx). MRegion is
 * just a specialized version of the original implementation. So all regards should be
 * to Vladislav Gelfer. 
 */
#include <windows.h>
#include "MBGlobal.h"
#include "3rd/ContainersInl.h"
namespace MetalBone
{
	class MRegion
	{
		public:
			class Iterator;
			MRegion (){}
			~MRegion(){}
			explicit MRegion(const RECT& r) { addRect(r); }
			MRegion(const MRegion& r) { copyFrom(r); }

			void addRect   (const RECT&);
			void combine   (const MRegion&); // RGN_OR
			void intersect (const MRegion&); // RGN_AND
			void subtract  (const MRegion&); // RGN_DIFF
			void subtractEx(const MRegion&, MRegion& intersectOut);
			void offset    (LONG x, LONG y);


			Iterator begin() const;

			inline void clear();
			inline void swap(MRegion&);
			inline bool isEmpty() const;

			inline static bool isRectValidNonEmpty(const RECT&);

			void copyFrom          (const MRegion&);
			bool isEqual           (const MRegion&) const;
			bool getBounds         (RECT&)          const;
			bool isPointInside     (const POINT&)   const;
			bool isRectFullyInside (const RECT&)    const;
			bool isRectPartlyInside(const RECT&)    const;




		private:
			enum OperationMode {
				OMCombine   = 0,
				OMSubtract  = 1,
				OMIntersect = 2
			};
			// We have one CoorTree (CoordianteTree) in each MRegion;
			// This CoorTree is used to sort Y-axis. Every MRegion::YNode
			// contains another CoorTree which is used to sort X-axis.
			typedef ThirdParty::Container::TreeEng<LONG, LONG, void> CoorTree;
			struct Range : public CoorTree::Node {
				LONG m_End;
				inline bool isEqualRanges(const Range& range) const;
			};
			struct XNode : public Range { 
				inline bool isEqualNodes(const XNode& node) const;
			};
			struct YNode : public Range {
				ThirdParty::Container::TreeDyn<CoorTree, XNode> m_xCoorTree;

				inline bool isEmpty() const;
				inline void swap(YNode& y);
				inline XNode* addRange(LONG start, LONG end);
				void copy(const YNode& ynode);
				void assertValid() const;
				void onNodeChanged(XNode* pNode);
				bool isEqualNodes(const YNode& node) const;
				// If the m_xCoorTree is the same, we consider
				// this two YNode is identical, that is they may
				// merge together if there's no gap between them
				bool isIdentical(const YNode& node) const;

				template<OperationMode mode>
				void perfOperation(const YNode& rgn, YNode* pIntersect);
			};
			ThirdParty::Container::TreeDyn<CoorTree, YNode> m_yCoorTree;

			YNode* addRange(LONG start, LONG end, const YNode&);
			void onNodeChanged(YNode*);
			void assertValid() const;

			template <OperationMode mode>
			void perfOperation(const MRegion&, MRegion* pIntersect);

			const MRegion& operator=(const MRegion&);


		public:
			class Iterator {
					const YNode* ynode;
					const XNode* xnode;
				public:
					Iterator():ynode(0),xnode(0){}
					inline void getRect(RECT& r) const;
					inline operator bool() const;
					Iterator& operator++();
				friend class MRegion;
			};
	};




	inline bool MRegion::isRectValidNonEmpty(const RECT& r)
		{ return r.left < r.right && r.top < r.bottom; }
	inline void MRegion::swap(MRegion& rgn)
		{ m_yCoorTree.Swap(rgn.m_yCoorTree); }
	inline void MRegion::clear()
		{ m_yCoorTree.Clear(); }
	inline bool MRegion::isEmpty() const
		{ return m_yCoorTree.IsEmpty(); }

	inline bool MRegion::Range::isEqualRanges(const Range& range) const
		{ return  (m_Key == range.m_Key) && (m_End == range.m_End); }
	inline bool MRegion::XNode::isEqualNodes(const XNode& node) const
		{ return isEqualRanges(node); }

	inline bool MRegion::YNode::isEmpty() const
		{ return m_xCoorTree.IsEmpty(); }
	inline void MRegion::YNode::swap(YNode& y)
		{ m_xCoorTree.Swap(y.m_xCoorTree); }
	inline MRegion::XNode* MRegion::YNode::addRange(LONG start, LONG end) {
		M_ASSERT(start < end);
		XNode* pxNode = m_xCoorTree.Create(start);
		pxNode->m_End = end;
		return pxNode;
	}

	inline MRegion::Iterator::operator bool() const
	 { return ynode && xnode; }
	inline void MRegion::Iterator::getRect(RECT& r) const {
		r.left   = xnode->m_Key;
		r.right  = xnode->m_End;
		r.top    = ynode->m_Key;
		r.bottom = ynode->m_End;
	}
}
