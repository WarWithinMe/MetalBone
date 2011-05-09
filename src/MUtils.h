#pragma once
#include "MBGlobal.h"
#include <windef.h>

#include <d2d1helper.h>

namespace MetalBone
{

	class METALBONE_EXPORT MSize : public SIZE
	{
		public:
			inline MSize();
			inline MSize(long width, long height);
			inline explicit MSize(SIZE size);
			inline MSize(const MSize& size);

			inline void setWidth (long width);
			inline void setHeight(long height);
			inline void resize   (long width, long height);

			inline long getWidth () const;
			inline long getHeight() const;

			inline long& width ();
			inline long& height();

			inline bool operator==(const MSize&) const;
			inline bool operator!=(const MSize&) const;
			inline bool operator==(const SIZE&)  const;
			inline bool operator!=(const SIZE&)  const;

			inline const MSize& operator=(const MSize&);
			inline const MSize& operator=(const SIZE&);

			inline operator D2D1_SIZE_U();
			inline operator D2D1_SIZE_F();
	};

	class METALBONE_EXPORT MPoint : public POINT
	{
		public:
			inline MPoint();
			inline MPoint(long x, long y);
			inline explicit MPoint(POINT);
			inline MPoint(const MPoint& pt);

			inline void move(long x, long y);

			inline bool operator==(const MPoint&) const;
			inline bool operator!=(const MPoint&) const;
			inline bool operator==(const POINT&)  const;
			inline bool operator!=(const POINT&)  const;

			inline const MPoint& operator=(const MPoint&);
			inline const MPoint& operator=(const POINT&);

			inline operator D2D1_POINT_2U();
			inline operator D2D1_POINT_2F();
	};

	class METALBONE_EXPORT MRect : public RECT
	{
		public:
			inline MRect();
			inline MRect(long left, long top, long right, long bottom);
			inline MRect(POINT pt, SIZE sz);
			inline MRect(POINT topleft, POINT bottomRight);
			inline MRect(const MRect&);
			inline explicit MRect(const RECT&);

			inline bool operator==(const RECT& rc);
			inline bool operator!=(const RECT& rc);
			inline void operator= (const RECT& rc);
			inline bool operator==(const MRect& rc);
			inline bool operator!=(const MRect& rc);
			inline void operator= (const MRect& rc);

			inline long height() const;
			inline long width()  const;
			inline bool isPointInside(POINT pt) const;
			inline bool isEmpty() const;

			inline void offset (int dx, int dy);
			inline void inflate(int dx, int dy);
			inline BOOL subtract (const RECT& rc1, const RECT& rc2);
			inline BOOL intersect(const RECT& rc1, const RECT& rc2);
			inline BOOL unionRect(const RECT& rc1, const RECT& rc2);

			inline operator D2D1_RECT_U();
			inline operator D2D1_RECT_F();
	};

	inline       MSize::MSize    ()                { cx = 0; cy = 0; }
	inline       MSize::MSize    (long w, long h)  { cx = w; cy = h; }
	inline       MSize::MSize    (SIZE sz)         { cx = sz.cx; cy = sz.cy; }
	inline       MSize::MSize    (const MSize& sz) { cx = sz.cx; cy = sz.cy; }
	inline void  MSize::setWidth (long w)          { cx = w; }
	inline void  MSize::setHeight(long h)          { cy = h; }
	inline void  MSize::resize   (long w, long h)  { cx = w; cy = h; }
	inline long  MSize::getWidth () const          { return cx; }
	inline long  MSize::getHeight() const          { return cy; }
	inline long& MSize::width    ()                { return cx; }
	inline long& MSize::height   ()                { return cy; }

	inline bool  MSize::operator==(const MSize& sz) const { return (cx == sz.cx && cy == sz.cy); }
	inline bool  MSize::operator!=(const MSize& sz) const { return (cx != sz.cx || cy != sz.cy); }
	inline bool  MSize::operator==(const SIZE&  sz) const { return (cx == sz.cx && cy == sz.cy); }
	inline bool  MSize::operator!=(const SIZE&  sz) const { return (cx != sz.cx || cy != sz.cy); }
	inline const MSize& MSize::operator=(const MSize& sz) { cx = sz.cx; cy = sz.cy; return *this; }
	inline const MSize& MSize::operator=(const SIZE&  sz) { cx = sz.cx; cy = sz.cy; return *this; }

	inline       MPoint::MPoint    ()                       { x = 0 ; y = 0; }
	inline       MPoint::MPoint    (long cx, long cy)       { x = cx; y = cy; }
	inline       MPoint::MPoint    (POINT pt)               { x = pt.x; y = pt.y; }
	inline       MPoint::MPoint    (const MPoint& pt)       { x = pt.x; y = pt.y; }
	inline void  MPoint::move      (long cx, long cy)       { x = cx; y = cy; }
	inline bool  MPoint::operator==(const MPoint& pt) const { return (x == pt.x && y == pt.y); }
	inline bool  MPoint::operator!=(const MPoint& pt) const { return (x != pt.x || y != pt.y); }
	inline bool  MPoint::operator==(const POINT& pt)  const { return (x == pt.x && y == pt.y); }
	inline bool  MPoint::operator!=(const POINT& pt)  const { return (x != pt.x || y != pt.y); }
	inline const MPoint& MPoint::operator=(const MPoint& pt){ x = pt.x; y = pt.y; return *this; }
	inline const MPoint& MPoint::operator=(const POINT& pt) { x = pt.x; y = pt.y; return *this; }

	inline MRect::MRect() { left = top = right = bottom = 0; }
	inline MRect::MRect(long l, long t, long r, long b)
		{ left = l; top = t; right = r; bottom = b; }
	inline MRect::MRect(POINT TL, POINT BR)
		{ left = TL.x; top = TL.y; right = BR.x; bottom = BR.y; }
	inline MRect::MRect(POINT pt, SIZE sz)
	{
		left   = pt.x;
		top    = pt.y;
		right  = left + sz.cx;
		bottom = top  + sz.cy;
	}
	inline MRect::MRect(const MRect& r)
		{ left = r.left; right = r.right; top = r.top; bottom = r.bottom; }
	inline MRect::MRect(const RECT& r)
		{ left = r.left; right = r.right; top = r.top; bottom = r.bottom; }
	inline bool MRect::operator==(const RECT& rc)
		{ return memcpy(this,&rc,sizeof(RECT)) == 0; }
	inline bool MRect::operator!=(const RECT& rc)
		{ return memcpy(this,&rc,sizeof(RECT)) != 0; }
	inline void MRect::operator= (const RECT& rc)
		{ left = rc.left; right = rc.right; top = rc.top; bottom = rc.bottom; }
	inline bool MRect::operator==(const MRect& rc)
		{ return memcpy(this,&rc,sizeof(MRect)) == 0; }
	inline bool MRect::operator!=(const MRect& rc)
		{ return memcpy(this,&rc,sizeof(MRect)) != 0; }
	inline void MRect::operator= (const MRect& rc)
		{ left = rc.left; right = rc.right; top = rc.top; bottom = rc.bottom; }
	inline long MRect::height() const { return bottom - top;  }
	inline long MRect::width()  const { return right  - left; }
	inline bool MRect::isPointInside(POINT pt) const
	{
		return pt.x >= left && pt.x <= right &&
			pt.y >= top && pt.y <= bottom;
	}
	inline bool MRect::isEmpty() const
		{ return (right == left && bottom == top); }
	inline void MRect::offset(int dx, int dy)
		{ left += dx; right  += dx; top  += dy; bottom += dy; }
	inline void MRect::inflate(int dx, int dy)
	{
		if((right += dx) < left) right = left;
		if((bottom+= dy) < top ) bottom= top;
	}
	inline BOOL MRect::subtract(const RECT& rc1, const RECT& rc2)
		{ return ::SubtractRect(this,&rc1,&rc2); }
	inline BOOL MRect::intersect(const RECT& rc1, const RECT& rc2)
		{ return ::IntersectRect(this,&rc1,&rc2); }
	inline BOOL MRect::unionRect(const RECT& rc1, const RECT& rc2)
		{ return ::UnionRect(this,&rc1,&rc2); }

	inline MSize:: operator D2D1_SIZE_U()   { return D2D1::SizeU(cx,cy); }
	inline MSize:: operator D2D1_SIZE_F()   { return D2D1::SizeF((FLOAT)cx, (FLOAT)cy); }
	inline MPoint::operator D2D1_POINT_2U() { return D2D1::Point2U(x,y); }
	inline MPoint::operator D2D1_POINT_2F() { return D2D1::Point2F((FLOAT)x,(FLOAT)y); }
	inline MRect::operator D2D1_RECT_U() { return D2D1::RectU(left,top,right,bottom); }
	inline MRect::operator D2D1_RECT_F() { return D2D1::RectF((FLOAT)left,(FLOAT)top,(FLOAT)right,(FLOAT)bottom); }
}