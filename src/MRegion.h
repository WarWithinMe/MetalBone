/*
 * This class is originally developed by Vladislav Gelfer. The original source code
 * can be obtain from the article - Advanced multi-dimensional region template class
 * on CodeProject.com (http://www.codeproject.com/KB/graphics/rgnex.aspx). MRegion is
 * just a specialized version of the original implementation. So all regards should be
 * to Vladislav Gelfer. 
 */

#pragma once

#include "MBGlobal.h"
#include "MUtils.h"
#include "3rd/ContainersInl.h"

#include <windows.h>
namespace MetalBone
{
	class METALBONE_EXPORT MRegion
	{
		public:
			inline MRegion ();
			inline ~MRegion();
			inline explicit MRegion(const MRect& r);
			inline MRegion(const MRegion& r);

			void addRect   (const MRect&);
			void combine   (const MRegion&); // RGN_OR
			void intersect (const MRegion&); // RGN_AND
			void subtract  (const MRegion&); // RGN_DIFF
			void subtractEx(const MRegion&, MRegion& intersectOut);
			void offset    (long x, long y);

			inline void clear();
			inline void swap(MRegion&);
			inline bool isEmpty() const;

			inline static bool isRectValidNonEmpty(const MRect&);

			void copyFrom          (const MRegion&);
			bool isEqual           (const MRegion&) const;
			bool getBounds         (MRect&)         const;
			bool isPointInside     (const MPoint&)  const;
			bool isRectFullyInside (const MRect&)   const;
			bool isRectPartlyInside(const MRect&)   const;

			class Iterator;
			Iterator begin() const;


		private:
			enum OperationMode {
				OMCombine   = 0,
				OMSubtract  = 1,
				OMIntersect = 2
			};
			// We have one CoorTree (CoordianteTree) in each MRegion;
			// This CoorTree is used to sort Y-axis. Every MRegion::YNode
			// contains another CoorTree which is used to sort X-axis.
			typedef ThirdParty::Container::TreeEng<long, long, void> CoorTree;
			struct Range : public CoorTree::Node {
				long m_End;
				inline bool isEqualRanges(const Range& range) const;
			};
			struct XNode : public Range { 
				inline bool isEqualNodes(const XNode& node) const;
			};
			struct YNode : public Range {
				ThirdParty::Container::TreeDyn<CoorTree, XNode> m_xCoorTree;

				inline bool isEmpty() const;
				inline void swap(YNode& y);
				inline XNode* addRange(long start, long end);
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

			YNode* addRange(long start, long end, const YNode&);
			void onNodeChanged(YNode*);
			void assertValid() const;

			template <OperationMode mode>
			void perfOperation(const MRegion&, MRegion* pIntersect);

			const MRegion& operator=(const MRegion&);
		public:
			class METALBONE_EXPORT Iterator {
					const YNode* ynode;
					const XNode* xnode;
				public:
					Iterator():ynode(0),xnode(0){}
					inline void getRect(MRect& r) const;
					inline operator bool() const;
					Iterator& operator++();
				friend class MRegion;
			};
	};





	inline MRegion::MRegion() {}
	inline MRegion::~MRegion(){}
	inline MRegion::MRegion(const MRect& r)   { addRect(r);  }
	inline MRegion::MRegion(const MRegion& r) { copyFrom(r); }
	inline bool MRegion::isRectValidNonEmpty(const MRect& r)
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
	inline MRegion::XNode* MRegion::YNode::addRange(long start, long end) {
		M_ASSERT(start < end);
		XNode* pxNode = m_xCoorTree.Create(start);
		pxNode->m_End = end;
		return pxNode;
	}

	inline MRegion::Iterator::operator bool() const
	 { return ynode && xnode; }
	inline void MRegion::Iterator::getRect(MRect& r) const {
		r.left   = xnode->m_Key;
		r.right  = xnode->m_End;
		r.top    = ynode->m_Key;
		r.bottom = ynode->m_End;
	}
}
