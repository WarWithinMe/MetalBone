#include <windows.h>
#include "Assert.h"
#define RGN_INTERNAL_TEST
#include "Rgn.h"
#include "GuardObjs.h"
#include "MRegion.h"
using namespace GObj;
using namespace MetalBone;
class GdiRgn {

	HGdiObj_G m_hRgn;

	void TestRgnRes(int val)
	{
		switch (val)
		{
		case SIMPLEREGION:
		case COMPLEXREGION:
			break; // ok

		default: ASSERT(FALSE);

		case NULLREGION:
			m_hRgn.Destroy();
		}
	}

	void AddFirstRect(const RECT& rc)
	{
		ASSERT(!m_hRgn);
		VERIFY(m_hRgn = CreateRectRgnIndirect(&rc));
	}

	void CopyInternal(const GdiRgn& rgn)
	{
		ASSERT(_Empty && !rgn._Empty);

		RECT rcDummy = { 0, 0, 0, 0 };
		AddFirstRect(rcDummy);

		CombineInternal(rgn, NULL, RGN_COPY);
	}

	void CombineInternal(const GdiRgn& rgn1, HGDIOBJ hRgn2, int nCode)
	{
		ASSERT(m_hRgn);
		TestRgnRes(CombineRgn((HRGN) m_hRgn.GetValue(), (HRGN) rgn1.m_hRgn.GetValue(), (HRGN) hRgn2, nCode));
	}

public:

	bool IsEmpty() const { return !m_hRgn; }
	__declspec(property(get=IsEmpty)) bool _Empty;

	void Clear() { m_hRgn.Destroy(); }

	void AddRect(const RECT& rc)
	{
		if ((rc.right > rc.left) &&
			(rc.bottom > rc.top))
			if (m_hRgn)
			{
				GdiRgn rgn;
				rgn.AddFirstRect(rc);

				Combine(rgn);
				
			} else
				AddFirstRect(rc);
	}

	void Combine(const GdiRgn& rgn)
	{
		if (!rgn._Empty)
			if (_Empty)
				CopyInternal(rgn);
			else
				CombineInternal(rgn, m_hRgn, RGN_OR);
	}

	void Intersect(const GdiRgn& rgn)
	{
		if (!_Empty)
			if (rgn._Empty)
				Clear();
			else
				CombineInternal(rgn, m_hRgn, RGN_AND);
	}

	void Subtract(const GdiRgn& rgn)
	{
		if (!_Empty && !rgn._Empty)
			CombineInternal(*this, rgn.m_hRgn, RGN_DIFF);
	}

	size_t ToRects(GPtr_T<RECT>& pArr)
	{
		if (_Empty)
			return 0;

		DWORD dwSize = GetRegionData((HRGN) m_hRgn.GetValue(), 0, NULL);
		ASSERT(dwSize > sizeof(RGNDATAHEADER));

		GPtr_T<RGNDATAHEADER> pHdr = (RGNDATAHEADER*) new BYTE[dwSize];

		VERIFY(GetRegionData((HRGN) m_hRgn.GetValue(), dwSize, (RGNDATA*) pHdr.GetValue()));

		ASSERT(pHdr->nCount);
		pArr = new RECT[pHdr->nCount];
		CopyMemory(pArr, pHdr.GetValue() + 1, sizeof(RECT) * pHdr->nCount);

		return pHdr->nCount;
	}

	bool GetBox(RECT& rc)
	{
		if (!m_hRgn)
		{
			memset(&rc, 0, sizeof(rc));
			return false;
		}
		VERIFY(GetRgnBox((HRGN) m_hRgn.GetValue(), &rc));
		return true;
	}

	HRGN getValue()
	{
		return HRGN(m_hRgn.GetValue());
	}
};

const ULONG SIZE_X = 3000;
const ULONG SIZE_Y = 4500;

void RandomRect(RECT& rc)
{
	rc.left = rand() % SIZE_X;
	rc.top = rand() % SIZE_Y;

	rc.right = rc.left + 1 + rand() % SIZE_X;
	rc.bottom = rc.top + 1 + rand() % SIZE_Y;
}

// typedef Rgn_Dim_T<long, 2> MyRgn;
typedef Rgn_T<long, Rgn_T<long, RgnNull> > MyRgn;

void CvtRect(const RECT& rc, MyRgn::Cube& cc)
{
	cc.lo.get_Level<0>() = rc.top;
	cc.lo.get_Level<1>() = rc.left;

	cc.hi.get_Level<0>() = rc.bottom;
	cc.hi.get_Level<1>() = rc.right;

	ASSERT(cc.IsValidNonEmpty());
}

void AddRandomRects(GdiRgn& rgnGdi, MyRgn& rgnMy, MRegion& mRegion)
{
	for (int i = 0; i < 10; i++)
	{
		RECT rc;
		RandomRect(rc);
		rgnGdi.AddRect(rc);

		MyRgn::Cube cc;
		CvtRect(rc, cc);
		rgnMy.AddCube(cc);

		mRegion.addRect(rc);
	}
}

void TestEqual(GdiRgn& rgnGdi, MyRgn& rgnMy)
{
	// Test
	GPtr_T<RECT> pRects;
	size_t nRects = rgnGdi.ToRects(pRects);

	MyRgn rgnMyTst;
	for (size_t i = 0; i < nRects; i++)
	{
		MyRgn::Cube cc;
		CvtRect(pRects[i], cc);
		rgnMyTst.AddCube(cc);
	}

	ASSERT(rgnMy.IsEqual(rgnMyTst));

	MyRgn::Iterator it;
	it.Init(rgnMy);
	for (size_t i = 0; i < nRects; i++)
	{
		ASSERT(it);
		ASSERT(!memcmp(pRects.GetValue() + i, (MyRgn::Cube*) &it, sizeof(RECT)));
		it.MoveNext();
	}
	ASSERT(!it);

	MyRgn::Cube ccc;
	rgnMy.GetBounds(ccc);

	bool bb;
	bb = rgnMy.PtInside(ccc.lo);
	bb = rgnMy.PtInside(ccc.hi);
	bb = rgnMy.CubeInsidePart(ccc);
	bb = rgnMy.CubeInsideFull(ccc);

	RECT rc;
	rgnGdi.GetBox(rc);

	ASSERT(!memcmp(&rc, &ccc, sizeof(rc)));
}

void TestEqual(GdiRgn& rgnGdi, MRegion& mRgn)
{
	// Test
	GPtr_T<RECT> pRects;
	size_t nRects = rgnGdi.ToRects(pRects);

	MRegion rgnMyTst;
	for (size_t i = 0; i < nRects; i++)
		rgnMyTst.addRect(pRects[i]);

	ASSERT(rgnMyTst.isEqual(mRgn));

// 	MyRgn::Iterator it;
// 	it.Init(rgnMy);
// 	for (size_t i = 0; i < nRects; i++)
// 	{
// 		ASSERT(it);
// 		ASSERT(!memcmp(pRects.GetValue() + i, (MyRgn::Cube*) &it, sizeof(RECT)));
// 		it.MoveNext();
// 	}
// 	ASSERT(!it);

	RECT boundingRect;
	mRgn.getBounds(boundingRect);

	RECT rc;
	rgnGdi.GetBox(rc);

	ASSERT(!memcmp(&rc, &boundingRect, sizeof(rc)));

	BOOL bb;
	POINT p;
	p.x = boundingRect.left;
	p.y = boundingRect.top;
	bb = mRgn.isPointInside(p);
	ASSERT(bb == PtInRegion(rgnGdi.getValue(),boundingRect.left,boundingRect.top));
	p.x = boundingRect.right;
	p.y = boundingRect.bottom;
	bb = mRgn.isPointInside(p);
	ASSERT(bb == PtInRegion(rgnGdi.getValue(),boundingRect.right,boundingRect.bottom));
	bb = mRgn.isRectPartlyInside(boundingRect);
	ASSERT(bb == RectInRegion(rgnGdi.getValue(),&boundingRect));
	bb = mRgn.isRectFullyInside(boundingRect);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	srand(GetTickCount());


	GdiRgn rgnGdi;
	MyRgn rgnMy;
	MRegion mmmRegion;

	for (int i = 0; i < 100; i++)
	{
		GdiRgn rgnGdi2;
		MyRgn rgnMy2;
		MRegion mmmRegion2;

		AddRandomRects(rgnGdi2, rgnMy2,mmmRegion2);
		TestEqual(rgnGdi2, rgnMy2);
		TestEqual(rgnGdi2, mmmRegion2);

		MyRgn::Cube ccc;
		rgnMy2.GetBounds(ccc);

		switch (i & 7)
		{
			case 3:
				rgnGdi.Intersect(rgnGdi2);
				rgnMy.Intersect(rgnMy2);
				mmmRegion.intersect(mmmRegion2);
		// 		break;


			case 2:
				rgnGdi.Subtract(rgnGdi2);
				//rgnMy.Subtract(rgnMy2);
				{
					MyRgn rgnInt;
					rgnMy.SubtractEx(rgnMy2, rgnInt);
				}
				 mmmRegion.subtract(mmmRegion2);
// 				{
// 					MRegion mrgnInt;
// 					mmmRegion.subtractEx(mmmRegion2,mrgnInt);
// 				}

	// 			break;
			default:
				rgnGdi.Combine(rgnGdi2);
				rgnMy.Combine(rgnMy2);
				mmmRegion.combine(mmmRegion2);
		}
		TestEqual(rgnGdi, rgnMy);
		TestEqual(rgnGdi, mmmRegion);
	}

	bool bb = rgnMy._Empty;
	rgnMy.Clear();
	bb = rgnMy._Empty;
	
	return 0;
}
