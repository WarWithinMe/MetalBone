#pragma once
#include "MBGlobal.h"
#include <windef.h>

#include <d2d1helper.h>

//MSize
//MPoint
//MRect
//MColor
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

			inline operator D2D1_SIZE_U() const;
			inline operator D2D1_SIZE_F() const;
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

			inline operator D2D1_POINT_2U() const;
			inline operator D2D1_POINT_2F() const;
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

			inline operator D2D1_RECT_U() const;
			inline operator D2D1_RECT_F() const;
	};

	class METALBONE_EXPORT MColor
	{
		// A class for storing ARGB(0xAARRGGBB) color value.
		public:
			inline MColor();
			inline explicit MColor(unsigned int argb);
			inline MColor(const MColor&);

			inline void setRed  (BYTE red);
			inline void setGreen(BYTE green);
			inline void setBlue (BYTE blue);
			inline void setAlpha(BYTE alpha);

			inline BYTE getRed()   const;
			inline BYTE getGreen() const;
			inline BYTE getBlue()  const;
			inline BYTE getAlpha() const;
			inline bool isTransparent() const;

			inline unsigned int getARGB() const;
			inline unsigned int getRGB()  const;
			inline COLORREF getCOLORREF() const;

			inline static MColor fromCOLORREF(COLORREF color, BYTE alpha);

			inline const MColor& operator=(const MColor&);
			inline bool operator==(const MColor&) const;

			inline operator D2D1_COLOR_F() const;

		private:
			unsigned int argb;
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
		{ return memcmp(this,&rc,sizeof(RECT)) == 0; }
	inline bool MRect::operator!=(const RECT& rc)
		{ return memcmp(this,&rc,sizeof(RECT)) != 0; }
	inline void MRect::operator= (const RECT& rc)
		{ left = rc.left; right = rc.right; top = rc.top; bottom = rc.bottom; }
	inline bool MRect::operator==(const MRect& rc)
		{ return memcmp(this,&rc,sizeof(MRect)) == 0; }
	inline bool MRect::operator!=(const MRect& rc)
		{ return memcmp(this,&rc,sizeof(MRect)) != 0; }
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


	inline MColor MColor::fromCOLORREF(COLORREF color, BYTE alpha)
	{
		unsigned int argb = alpha;
		argb = argb << 24 | ((color & 0xFF) << 16) | (color & 0xFF00) | (color >> 16) & 0xFF;
		return MColor(argb);
	}
	inline COLORREF MColor::getCOLORREF() const // ColorRef is bgr format.
		{ return ((argb & 0xFF) << 16) | (argb & 0xFF00) | (argb >> 16 & 0xFF); }
	inline MColor::MColor():argb(0xFF000000){}
	inline MColor::MColor(unsigned int c):argb(c){}
	inline MColor::MColor(const MColor& rhs):argb(rhs.argb){}
	inline void MColor::setRed(BYTE red)
		{ argb = argb & 0xFF00FFFF | (red << 16); }
	inline void MColor::setGreen(BYTE green)
		{ argb = argb & 0xFFFF00FF | (green << 8); }
	inline void MColor::setBlue(BYTE blue)
		{ argb = argb & 0xFFFFFF00 | blue; }
	inline void MColor::setAlpha(BYTE alpha)
		{ argb = argb & 0xFFFFFF | (alpha << 24); }
	inline BYTE MColor::getRed() const
		{ return BYTE(argb >> 16); }
	inline BYTE MColor::getGreen() const
		{ return BYTE(argb >> 8); }
	inline BYTE MColor::getBlue() const
		{ return (BYTE)argb; }
	inline BYTE MColor::getAlpha() const
		{ return BYTE(argb >> 24); }
	inline bool MColor::isTransparent() const
		{ return argb <= 0x00FFFFFF; }
	inline const MColor& MColor::operator=(const MColor& rhs)
		{ argb = rhs.argb; return *this; }
	inline bool MColor::operator==(const MColor& rhs) const
		{ return argb == rhs.argb; }
	inline unsigned int MColor::getARGB() const
		{ return argb; }
	inline unsigned int MColor::getRGB() const
		{ return argb & 0xFFFFFF; }

	inline MSize:: operator D2D1_SIZE_U  () const { return D2D1::SizeU(cx,cy); }
	inline MSize:: operator D2D1_SIZE_F  () const { return D2D1::SizeF((FLOAT)cx, (FLOAT)cy); }
	inline MPoint::operator D2D1_POINT_2U() const { return D2D1::Point2U(x,y); }
	inline MPoint::operator D2D1_POINT_2F() const { return D2D1::Point2F((FLOAT)x,(FLOAT)y); }
	inline MRect:: operator D2D1_RECT_U  () const { return D2D1::RectU(left,top,right,bottom); }
	inline MRect:: operator D2D1_RECT_F  () const { return D2D1::RectF((FLOAT)left,(FLOAT)top,(FLOAT)right,(FLOAT)bottom); }
	inline MColor::operator D2D1_COLOR_F () const { return D2D1::ColorF(argb & 0xFFFFFF, float(argb >> 24 & 0xFF) / 255); }
}

template<>
class std::tr1::hash<MColor>
{
	public:
		size_t operator()(const MColor& color) const
			{ return std::tr1::hash<unsigned int>().operator()(color.getARGB()); }
};