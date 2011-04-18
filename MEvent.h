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
	// If MPaintEvent is not accepted, its children won't be painted.
	class MPaintEvent : public MEvent
	{
		public:
			explicit MPaintEvent(const MRegion& r):region(r){}
			inline const MRegion& getUpdateRegion() const { return region; }
		private:
			const MRegion& region;
	};
}
#endif // MEVENT_H
