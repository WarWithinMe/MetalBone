#pragma once

#include <string>
namespace MetalBone
{
	class MToolTip
	{
		public:
			enum HidePolicy { WhenMoveOut, WhenMove };

			inline MToolTip(){}
			virtual ~MToolTip(){}

			inline void setHidePolicy(HidePolicy);
			inline HidePolicy hidePolicy() const;


			void setText(const std::wstring&){}
			explicit MToolTip(const std::wstring&) {}
			void show(int x, int y) {}
			void hide() {}
		protected:
			virtual void draw() {}
		private:
			enum TTStyle { Unknown, SystemText, CustomText, CustomWidget };

			TTStyle style;
			HidePolicy hp;
			std::wstring tipString;

// 			static bool bInited;
// 			static void init() {}
	};

// 	inline MToolTip::MToolTip():style(Unknown),hp(WhenMoveOut){ if(!bInited) init(); }
	inline void MToolTip::setHidePolicy(HidePolicy p) { hp = p; }
	inline MToolTip::HidePolicy MToolTip::hidePolicy() const { return hp; }
}