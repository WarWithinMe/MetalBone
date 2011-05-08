#pragma once
#include <string>
#include <Windows.h>
#include <CommCtrl.h>

namespace MetalBone
{
	class MWidget;
	class MToolTip
	{
		public:
			enum HidePolicy { WhenMoveOut, WhenMove };

			enum Icon
			{
				None      = TTI_NONE,
				Info      = TTI_INFO,
				Warning   = TTI_WARNING,
				Error     = TTI_ERROR,
				LargeInfo = TTI_INFO_LARGE,
				LargeWarning = TTI_WARNING_LARGE,
				LargeError = TTI_ERROR_LARGE
			};

			// MToolTip do not take control of the MWidget,
			// It's the responsibility of the user to delete the widget.
			explicit MToolTip(const std::wstring& tip = std::wstring(),
				MWidget* w = 0, HidePolicy hp = WhenMoveOut);
			~MToolTip();

			inline void setHidePolicy(HidePolicy);
			inline HidePolicy hidePolicy() const;
			
			// If the toolTip is shown when you are setting the data for it,
			// you must hide and show it again to take effect.
			inline void setText(const std::wstring&);
			// If there's any HICON/MWidget previous set, this function will return
			// that HICON/MWidget, you must free the returned HICON/MWidget yourself.
			// The HICON/MWidget parameter passed in this function is deleted when
			// this class is deleted.
			inline MWidget* setWidget(MWidget*);
			inline HICON setExtra(const std::wstring& title, HICON icon = NULL);
			inline HICON setExtra(const std::wstring& title, Icon);

			inline const std::wstring& getTitle()  const;
			inline const std::wstring& getText()   const;
			inline MWidget*            getWidget() const;

			void show(long x, long y);
			void hide();
			bool isShowing() const;

			static void hideAll();

		private:
			HidePolicy   hp;
			std::wstring tipString;
			std::wstring title;
			HICON        icon;
			Icon         iconEnum;
			MWidget*     customWidget;
	};

	inline void MToolTip::setHidePolicy(HidePolicy p)
		{ hp = p; }
	inline MToolTip::HidePolicy MToolTip::hidePolicy() const
		{ return hp; }
	inline const std::wstring& MToolTip::getText() const
		{ return tipString; }
	inline const std::wstring& MToolTip::getTitle() const
		{ return title; }
	inline MWidget* MToolTip::getWidget() const
		{ return customWidget; }
	inline void MToolTip::setText(const std::wstring& t)
		{ tipString = t; }
	inline HICON MToolTip::setExtra(const std::wstring& t, Icon i)
	{
		title = t;
		iconEnum = i;
		icon = NULL;
		return icon;
	}
	inline HICON MToolTip::setExtra(const std::wstring& t, HICON i)
	{
		HICON oldIcon = icon;
		title = t;
		icon  = i;
		return oldIcon;
	}
	inline MWidget* MToolTip::setWidget(MWidget* w)
	{ 
		hide();
		MWidget* oldW = customWidget;
		customWidget = w;
		return oldW;
	}
}