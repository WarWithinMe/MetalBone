#pragma once

#include "MWidget.h"
#include "MCommonWidgets.h"

namespace MetalBone
{
	namespace Demo
	{
		class SAMainWindow : public MetalBone::MWidget
		{
			public:
				SAMainWindow();
			protected:
				virtual void resizeEvent(MResizeEvent*);

			private:
				MetalBone::MButton redxButton;
				MetalBone::MButton closeButton;

				MetalBone::MLabel titleLabel;
				MetalBone::MLabel statusLabel;

				MetalBone::MTabBar tabBar;
				MetalBone::MWidget contentWidget;
				MetalBone::MWidget resizer;

				inline void closeBtnClicked() { closeWindow(); }
		};
	}
}