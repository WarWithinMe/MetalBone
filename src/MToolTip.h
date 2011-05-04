#pragma once
#include <string>

namespace MetalBone
{
	class MWidget;
	class MToolTip
	{
		public:
			enum HidePolicy { WhenMoveOut, WhenMove };

			// MToolTip do not take control of the MWidget,
			// It's the responsibility of the user to delete the widget.
			explicit MToolTip(const std::wstring& tip = std::wstring(),
				MWidget* w = 0, HidePolicy hp = WhenMoveOut);
			inline ~MToolTip();

			inline void setHidePolicy(HidePolicy);
			inline HidePolicy hidePolicy() const;

			inline void setText(const std::wstring&);
			inline void setWidget(MWidget*);
			inline const std::wstring& getText() const;
			inline MWidget* getWidget() const;

			void show(long x, long y);
			void hide();

		private:
			HidePolicy   hp;
			std::wstring tipString;
			MWidget*     customWidget;

			static MWidget* toolTipWidget;
	};

	inline MToolTip::~MToolTip(){}
	inline void MToolTip::setHidePolicy(HidePolicy p)
		{ hp = p; }
	inline MToolTip::HidePolicy MToolTip::hidePolicy() const
		{ return hp; }
	inline void MToolTip::setText(const std::wstring& t)
		{ tipString = t; customWidget = 0; }
	inline const std::wstring& MToolTip::getText() const
		{ return tipString; }
	inline void MToolTip::setWidget(MWidget* w)
		{ customWidget = w; tipString.clear(); }
	inline MWidget* MToolTip::getWidget() const
		{ return customWidget; }
}