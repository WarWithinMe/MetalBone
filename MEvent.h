#ifndef GUI_MEVENT_H
#define GUI_MEVENT_H

namespace MetalBone
{
	class MRegion;
	class MEvent
	{
		public:
			inline MEvent(bool a = true);
			inline ~MEvent();
			inline void accept();
			inline void ignore();
			inline bool isAccepted() const;
		private:
			bool accepted;
			MEvent(const MEvent&);
			const MEvent& operator=(const MEvent&);
	};

	class MPaintEvent : public MEvent
	{
		public:
			inline explicit MPaintEvent(const MRegion&);
			inline const MRegion& getUpdateRegion() const;
		private:
			const MRegion& region;
	};


	enum MouseButton
	{
		NoButton     = 0,
		LeftButton   = 1,
		RightButton  = 2,
		MiddleButton = 4
	};
	class MMouseEvent : public MEvent
	{
		public:
			inline MMouseEvent(int, int, int, int, MouseButton);


			inline MouseButton button() const;
			inline int getX()       const;
			inline int getY()       const;
			inline int getGlobalX() const;
			inline int getGlobalY() const;
			inline void offsetPos(int,int);
		private:
			int ix,iy,igx,igy;
			MouseButton btn;
	};

	inline MEvent::MEvent(bool a):accepted(a){}
	inline MEvent::~MEvent(){}
	inline void MEvent::accept() { accepted = true; }
	inline void MEvent::ignore() { accepted = false;}
	inline bool MEvent::isAccepted() const { return accepted; }

	inline MPaintEvent::MPaintEvent(const MRegion& r):region(r){}
	inline const MRegion& MPaintEvent::getUpdateRegion() const
		{ return region; }

	inline MMouseEvent::MMouseEvent(int x, int y, int gx, int gy,MouseButton b):
		ix(x),iy(y),igx(gx),igy(gy),btn(b){}
	inline int MMouseEvent::getX()           const { return ix;  }
	inline int MMouseEvent::getY()           const { return iy;  }
	inline int MMouseEvent::getGlobalX()     const { return igx; }
	inline int MMouseEvent::getGlobalY()     const { return igy; }
	inline MouseButton MMouseEvent::button() const { return btn; }
	inline void MMouseEvent::offsetPos(int x,int y)
		{ ix += x; iy += y; }
}
#endif // MEVENT_H
