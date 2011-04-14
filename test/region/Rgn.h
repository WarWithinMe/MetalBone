#pragma once
#include "ContainersInl.h"

template <typename T, class RgnInternal>
class Rgn_T
{
	typedef Container::TreeEng<T, T, void> TreeT;

	struct Range :public TreeT::Node {
		T m_End;
		bool IsEqualRanges(const Range& range) const
		{
			return 
				(m_Key == range.m_Key) &&
				(m_End == range.m_End);
		}
	};

	struct Node
		:public Range
		,public RgnInternal
	{
		bool IsEqualNodes(const Node& node) const
		{
			return
				IsEqualRanges(node) &&
				RgnInternal::IsEqual(node);
		}
	};

	Container::TreeDyn<TreeT, Node> m_treNodes;

	template <int Depth>
	struct TypeHelper :public RgnInternal::TypeHelper<Depth-1> {};

	template <>
	struct TypeHelper<0> {
		typedef T Type;
	};

public:
	Rgn_T() {}
	~Rgn_T() {}

	struct Point
		:public RgnInternal::Point
	{
		template <int Depth>
		typename TypeHelper<Depth>::Type& get_Level() { return RgnInternal::Point::get_Level<Depth-1>(); }

		template <>
		T& get_Level<0>() { return m_Value; }

		T m_Value;
		operator T () const { return m_Value; }
		Point& operator = (T newVal) { m_Value = newVal; return *this; }

		void Zero()
		{
			m_Value = 0;
			RgnInternal::Point::Zero();
		}
	};

	struct Cube {
		Point lo, hi; // low-hi
		bool IsValidNonEmpty() const { return IsValidPointsOrder(lo, hi); }
	};

	class IteratorHelper
		:public RgnInternal::IteratorHelper
	{
	protected:
		IteratorHelper() {}

		const Node* m_pPos;
		bool MoveNextHelper(Point& lo, Point& hi)
		{
			ASSERT(IsValid());

			if (!__super::MoveNextHelper(lo, hi))
			{
				m_pPos = Container::TreeEx<TreeT, Node>::FindNext(*m_pPos);
				if (!m_pPos)
					return false;

				InitStrict(lo, hi);
			}
			return true;
		}

		void InitStrict(Point& lo, Point& hi)
		{
			ASSERT(m_pPos);
			lo = m_pPos->m_Key;
			hi = m_pPos->m_End;
			__super::InitStrict(lo, hi, *m_pPos);
		}

		void InitStrict(Point& lo, Point& hi, const Rgn_T& rgn)
		{
			m_pPos = rgn.m_treNodes.FindMin();
			InitStrict(lo, hi);
		}

	public:
		bool IsValid() const { return NULL != m_pPos; }
		operator bool () const { return IsValid(); }
	};

	class Iterator
		:public IteratorHelper
		,public Cube
	{
	public:
		Iterator() { m_pPos = NULL; }
		bool Init(const Rgn_T& rgn)
		{
			m_pPos = rgn.m_treNodes.FindMin();
			if (!m_pPos)
				return false;
			InitStrict(lo, hi);
			return true;
		}
		bool MoveNext() { return MoveNextHelper(lo, hi); }
	};

	void Clear() { m_treNodes.Clear(); }

	bool IsEmpty() const { return m_treNodes._Empty; }
	__declspec(property(get=IsEmpty)) bool _Empty;

	void AddCube(const Cube&);

	bool GetBounds(Cube& c) const;
	void Offset(const Point&);

	bool IsEqual(const Rgn_T&) const;

	bool CubeInsidePart(const Point& lo, const Point& hi) const;
	bool CubeInsidePart(const Cube& c) const { return CubeInsidePart(c.lo, c.hi); }

	bool CubeInsideFull(const Cube&c) const { return CubeInsideFull(c.lo, c.hi); }
	bool CubeInsideFull(const Point& lo, const Point& hi) const;

	bool PtInside(const Point&) const;

	void Swap(Rgn_T& rgn)
	{
		m_treNodes.Swap(rgn.m_treNodes);
	}

	// RGN_DIFF
	void Subtract(const Rgn_T& rgn)
	{
		PerfOperation<1>(rgn, NULL);
	}
	void SubtractEx(const Rgn_T& rgn, Rgn_T& rgnIntersect)
	{
		PerfOperation<2>(rgn, &rgnIntersect);
	}

	// RGN_COPY
	void Copy(const Rgn_T&);

	// RGN_AND
	void Intersect(const Rgn_T&);

	// RGN_OR
	void Combine(const Rgn_T& rgn)
	{
		PerfOperation<0>(rgn, NULL);
	}


	void AssertValid() const
#ifdef _DEBUG
		;
#else // _DEBUG
	{}
#endif // _DEBUG

	void AssertValidInternal() const
	{
#ifdef RGN_INTERNAL_TEST
		AssertValid();
#endif // RGN_INTERNAL_TEST
	}

private:
	bool CubeInsideFullInternal(const Point& lo, const Point& hi) const;
	bool CubeInsidePartInternal(const Point& lo, const Point& hi) const;
	void AddFirstCube(const Point& lo, const Point& hi);
	void GetBoundsInternal(Point& lo, Point& hi, bool bFirst) const;
	void CopyInternal(const Rgn_T&);
	void OnNodeChanged(Node*);
	static bool IsNullClass() { return false; }

	struct SingleCubeRgnHelper
		:public RgnInternal::SingleCubeRgnHelper
	{
		Node m_Node;
		void Init(const Point& lo, const Point& hi, Rgn_T& rgn)
		{
			m_Node.m_Key = lo;
			m_Node.m_End = hi;
			rgn.m_treNodes.Insert(m_Node);

			RgnInternal::SingleCubeRgnHelper::Init(lo, hi, m_Node);
		}
		void Uninit(Rgn_T& rgn)
		{
			rgn.m_treNodes.Remove(m_Node);
			RgnInternal::SingleCubeRgnHelper::Uninit(m_Node);
		}
	};

	struct SingleCubeRgn
	{
		Rgn_T m_Rgn;
		SingleCubeRgnHelper m_Helper;

		SingleCubeRgn(const Point& lo, const Point& hi)
		{
			m_Helper.Init(lo, hi, m_Rgn);
		}
		~SingleCubeRgn()
		{
			m_Helper.Uninit(m_Rgn);
		}
	};

	void ZeroBounds(Cube& c) const
	{
		c.lo = 0;
		c.hi = 0;
		RgnInternal::ZeroBounds(c);
	}

	Node* AddRangeInternal(T start, T end, const RgnInternal&);

	template <int code>
	void PerfOperation(const Rgn_T& rgn, Rgn_T* pIntersect);

	static bool IsValidPointsOrder(const Point& lo, const Point& hi)
	{
		return
			(lo.m_Value < hi.m_Value) &&
			RgnInternal::IsValidPointsOrder(lo, hi);

	}

	friend class Rgn_T;
};

class RgnNull
{
	template <typename T, class RgnInternal>
	friend class Rgn_T;

	template <typename T, int Dim>
	friend class Rgn_Dim_T;

	RgnNull() {}

	struct Point {
		void Zero() {}
	};

	class IteratorHelper
	{
	public:
		bool MoveNextHelper(Point&, Point&) { return false; }
		void InitStrict(Point&, Point&, const RgnNull&) {}
	};

	struct SingleCubeRgnHelper
	{
		void Init(const Point&, const Point&, RgnNull&) {}
		void Uninit(RgnNull&) {}
	};

	void AddFirstCube(const Point&, const Point&) {}
	void GetBoundsInternal(Point& lo, Point& hi, bool bFirst) const {}
	void Offset(const Point&) {}
	bool PtInside(const Point&) const { return true; }
	bool CubeInsideFullInternal(const Point&, const Point&) const { return true; }
	bool CubeInsidePartInternal(const Point&, const Point&) const { return true; }
	void CopyInternal(const RgnNull&) {}
	bool IsEqual(const RgnNull&) const { return true; }
	void AssertValid() const {}
	bool IsEmpty() const { return false; }
	void Swap(RgnNull&) {}
	static bool IsNullClass() { return true; }
	static bool IsValidPointsOrder(const Point&, const Point&) { return true; }

	template <int code>
	void PerfOperation(const RgnNull& rgn, RgnNull* pIntersect) {}
};

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::AddCube(const Cube& c)
{
	AssertValidInternal();

	if (c.IsValidNonEmpty())
		if (IsEmpty())
			AddFirstCube(c.lo, c.hi);
		else
			if (!CubeInsideFullInternal(c.lo, c.hi))
			{
				// a hack for speedup
				SingleCubeRgn rgnPrep(c.lo, c.hi);

				Combine(rgnPrep.m_Rgn);
			}
			AssertValidInternal();
}

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::AddFirstCube(const Point& lo, const Point& hi)
{
	ASSERT(_Empty && IsValidPointsOrder(lo, hi));

	Node* pNode = m_treNodes.Create(lo);
	pNode->m_End = hi;

	pNode->AddFirstCube(lo, hi);
}

template <typename T, class RgnInternal>
inline bool Rgn_T<T, RgnInternal>::GetBounds(Cube& c) const
{
	AssertValidInternal();

	if (_Empty)
	{
		c.lo.Zero();
		c.hi.Zero();
		return false;
	}
	GetBoundsInternal(c.lo, c.hi, true);
	return true;
}

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::GetBoundsInternal(Point& lo, Point& hi, bool bFirst) const
{
	const Node* pNode = m_treNodes.FindMin();
	ASSERT(pNode);
	if (bFirst || (lo > pNode->m_Key))
		lo = pNode->m_Key;

	if (RgnInternal::IsNullClass())
		pNode = m_treNodes.FindMax();
	else
		for (bool bInnerFirst = true; ; bInnerFirst = false)
		{
			pNode->GetBoundsInternal(lo, hi, bInnerFirst);

			const Node* pNext = m_treNodes.FindNext(*pNode);
			if (!pNext)
				break;
			pNode = pNext;
		}

		if (bFirst || (hi < pNode->m_End))
			hi = pNode->m_End;
}

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::Offset(const Point& p)
{
	AssertValidInternal();

	bool bOverflow = false, bFirst = true;
	T keyPrev;

	for (Node* pNode = m_treNodes.FindMin(); pNode; pNode = m_treNodes.FindNext(*pNode))
	{
		pNode->Offset(p);

		pNode->m_Key += p;
		pNode->m_End += p;

		// test for overflow
		if (bFirst)
			bFirst = false;
		else
			if (keyPrev > pNode->m_Key)
				bOverflow = true;

		if (pNode->m_Key > pNode->m_End)
			bOverflow = true;

		keyPrev = pNode->m_Key;
	}

	if (bOverflow)
	{
		Container::TreeEx<TreeT, Node> treNodes;
		treNodes.Swap(m_treNodes);

		for (Node* pNode; pNode = treNodes.FindMin(); )
		{
			treNodes.Remove(*pNode);

			if (pNode->m_Key < pNode->m_End)
				m_treNodes.Insert(*pNode);
			else
				delete pNode;
		}

	}

	AssertValidInternal();
}

template <typename T, class RgnInternal>
inline bool Rgn_T<T, RgnInternal>::IsEqual(const Rgn_T& rgn) const
{
	AssertValidInternal();
	rgn.AssertValidInternal();

	const Node* p1 = m_treNodes.FindMin();
	const Node* p2 = rgn.m_treNodes.FindMin();
	while (true)
	{
		if (!p1)
			return !p2;
		if (!p2)
			return false;

		if (!p1->IsEqualNodes(*p2))
			return false;

		p1 = m_treNodes.FindNext(*p1);
		p2 = rgn.m_treNodes.FindNext(*p2);
	}
	return true;
}

template <typename T, class RgnInternal>
inline bool Rgn_T<T, RgnInternal>::PtInside(const Point& p) const
{
	const Node* pNode = m_treNodes.FindExactSmaller(p);
	return
		pNode &&
		(pNode->m_End > p) &&
		pNode->PtInside(p);
}

template <typename T, class RgnInternal>
inline bool Rgn_T<T, RgnInternal>::CubeInsideFull(const Point& lo, const Point& hi) const
{
	return
		IsValidPointsOrder(lo, hi) &&
		CubeInsideFullInternal(lo, hi);
}

template <typename T, class RgnInternal>
inline bool Rgn_T<T, RgnInternal>::CubeInsideFullInternal(const Point& lo, const Point& hi) const
{
	const Node* pNode = m_treNodes.FindExactSmaller(lo);
	if (!pNode)
		return false;

	if (RgnInternal::IsNullClass())
		return (pNode->m_End >= hi.m_Value);

	while (true)
	{
		if (!pNode->CubeInsideFullInternal(lo, hi))
			return false;

		if (pNode->m_End >= hi.m_Value)
			break;

		const Node* pNext = m_treNodes.FindNext(*pNode);
		if (!pNext || (pNext->m_Key != pNode->m_End))
			return false;

		pNode = pNext;
	}

	return true;
}

template <typename T, class RgnInternal>
inline bool Rgn_T<T, RgnInternal>::CubeInsidePart(const Point& lo, const Point& hi) const
{
	return
		IsValidPointsOrder(lo, hi) &&
		CubeInsidePartInternal(lo, hi);
}

template <typename T, class RgnInternal>
inline bool Rgn_T<T, RgnInternal>::CubeInsidePartInternal(const Point& lo, const Point& hi) const
{
	for (const Node* pNode = m_treNodes.FindSmaller(hi); pNode && (pNode->m_End > lo); pNode = m_treNodes.FindPrev(*pNode))
		if (pNode->CubeInsidePartInternal(lo, hi))
			return true;

	return false;
}

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::Copy(const Rgn_T& rgn)
{
	AssertValidInternal();
	rgn.AssertValidInternal();

	ASSERT(&rgn != this);
	Clear();
	CopyInternal(rgn);

	AssertValidInternal();
}

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::CopyInternal(const Rgn_T& rgn)
{
	ASSERT(_Empty);

	for (const Node* pNode = rgn.m_treNodes.FindMin(); pNode; pNode = rgn.m_treNodes.FindNext(*pNode))
	{
		Node* pDup = m_treNodes.Create(pNode->m_Key);
		pDup->m_End = pNode->m_End;
		pDup->CopyInternal(*pNode);
	}
}

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::OnNodeChanged(Node* pNode)
{
	ASSERT(pNode);

	while (true)
	{
		Node* pVal = m_treNodes.FindPrev(*pNode);
		if (!pVal ||
			(pVal->m_End != pNode->m_Key) ||
			!pVal->IsEqual(*pNode))
			break;

		pVal->m_End = pNode->m_End;
		m_treNodes.Delete(pNode);
		pNode = pVal;
	}

	while (true)
	{
		Node* pVal = m_treNodes.FindNext(*pNode);
		if (!pVal ||
			(pNode->m_End != pVal->m_Key) ||
			!pVal->IsEqual(*pNode))
			break;

		pNode->m_End = pVal->m_End;
		m_treNodes.Delete(pVal);
	}
}

template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::Intersect(const Rgn_T& rgn)
{
	Rgn_T rgnDup;
	Swap(rgnDup);

	rgnDup.PerfOperation<2>(rgn, this);
}

template <typename T, class RgnInternal>
inline typename Rgn_T<T, RgnInternal>::Node* Rgn_T<T, RgnInternal>::AddRangeInternal(T start, T end, const RgnInternal& rgn)
{
	ASSERT(start < end);

	Node* pNode = m_treNodes.Create(start);
	pNode->m_End = end;
	pNode->CopyInternal(rgn);

	return pNode;
}

template <typename T, class RgnInternal>
template <int code>
inline void Rgn_T<T, RgnInternal>::PerfOperation(const Rgn_T& rgn, Rgn_T* pIntersect)
{
	AssertValidInternal();
	rgn.AssertValidInternal();

	for (const Node* pCurrent = rgn.m_treNodes.FindMin(); pCurrent; pCurrent = rgn.m_treNodes.FindNext(*pCurrent))
	{
		T key = pCurrent->m_Key;
		T end = pCurrent->m_End;

		while (true)
		{
			Node* pVal = m_treNodes.FindSmaller(end);
			if (pVal && (pVal->m_End > key))
			{
				if (pVal->m_End > end)
				{
					AddRangeInternal(end, pVal->m_End, *pVal);
					pVal->m_End = end;
				} else
					if (pVal->m_End < end)
					{
						if (!code)
							OnNodeChanged(AddRangeInternal(pVal->m_End, end, *pCurrent));

						end = pVal->m_End;

						if (!code)
							continue;
					}

					if (pVal->m_Key < key)
					{
						pVal->m_End = key;
						pVal = AddRangeInternal(key, end, *pVal);
					}

					ASSERT((pVal->m_Key >= key) && (pVal->m_End == end));

					if (2 == code)
					{
						RgnInternal rgnRes;
						pVal->PerfOperation<code>(*pCurrent, &rgnRes);

						if (RgnInternal::IsNullClass() || !rgnRes.IsEmpty())
						{
							Node* pResNode = pIntersect->m_treNodes.Create(pVal->m_Key);
							pResNode->m_End = end;
							pResNode->Swap(rgnRes);

							pIntersect->OnNodeChanged(pResNode);
						}

					} else
						pVal->PerfOperation<code>(*pCurrent, NULL);

			} else
			{
				if (code)
					break;
				pVal = AddRangeInternal(key, end, *pCurrent);
			}

			end = pVal->m_Key;

			if (code && (RgnInternal::IsNullClass() || pVal->IsEmpty()))
				m_treNodes.Delete(pVal);
			else
				OnNodeChanged(pVal);

			if (key == end)
				break;
		}
	}

	AssertValidInternal();
	if (2 == code)
		pIntersect->AssertValidInternal();
}


#ifdef _DEBUG
template <typename T, class RgnInternal>
inline void Rgn_T<T, RgnInternal>::AssertValid() const
{
	const Node* pPrev = NULL;
	for (const Node* pNode = m_treNodes.FindMin(); pNode; pPrev = pNode, pNode = m_treNodes.FindNext(*pNode))
	{
		pNode->AssertValid();

		ASSERT(pNode->m_End > pNode->m_Key);
		if (pPrev)
		{
			ASSERT(pNode->m_Key >= pPrev->m_End);
			ASSERT((pNode->m_Key > pPrev->m_End) || !pNode->IsEqual(*pPrev));
		}
	}

}

#endif // _DEBUG


template <typename T, int Dim>
class Rgn_Dim_T
	:public Rgn_T<T, Rgn_Dim_T<T, Dim-1> >
{
};

template <typename T>
class Rgn_Dim_T<T, 0>
	:public RgnNull
{
};
