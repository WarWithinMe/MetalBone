#pragma once

#include "MWidget.h"
#include "MCommonWidgets.h"

namespace MetalBone
{
	namespace Demo
	{
		class SALabel : public MetalBone::MWidget
		{
			public:
				void setText(const std::wstring&);
			protected:
				void doStyleSheetDraw(const MRect&,const MRect&);
			private:
				std::wstring text;
		};

		class SATabBar : public MetalBone::MWidget
		{

		};

		class SAMainWindow : public MetalBone::MWidget
		{
			public:
				SAMainWindow();
			protected:
				virtual void resizeEvent(MResizeEvent*);

			private:
				MetalBone::MButton redxButton;
				MetalBone::MButton closeButton;

				SALabel titleLabel;
				SALabel statusLabel;

				SATabBar tabBar;
				MetalBone::MWidget contentWidget;
				MetalBone::MWidget resizer;

				inline void closeBtnClicked() { closeWindow(); }
		};
	}
}