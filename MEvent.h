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

	class MRegion;
	class MWidget;
	class MPaintEvent : public MEvent
	{
		public:
			inline const MRegion& getUpdateRegion() const  { return *region; }
		private:
			MRegion* region;
			inline void           setUpdateRegion(MRegion* r) { region = r; }
			friend class MWidget;
	};
}
#endif // MEVENT_H
