#ifndef GUI_MEVENT_H
#define GUI_MEVENT_H

namespace MetalBone
{
	class MEvent
	{
		public:
			MEvent():accepted(true)        {}
			virtual ~MEvent()              {}
			inline void accept()           { accepted = true; }
			inline void ignore()           { accepted = false;}
			inline bool isAccepted() const { return accepted; }
		private:
			bool accepted;
	};

	class MPaintEvent : public MEvent
	{
		public:
			inline const RECT& getUpdateRect() const  { return *rect; }
			inline void        setUpdateRect(RECT* r) { rect = r; }
		private:
			RECT* rect;
	};
}
#endif // MEVENT_H
