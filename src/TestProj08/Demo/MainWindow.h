#pragma once

#include "MWidget.h"
#include "MCommonWidgets.h"

namespace MetalBone
{
    namespace Demo
    {
        class SAMainWindow : public MWidget
        {
            public:
                SAMainWindow();
            protected:
                virtual void resizeEvent(MResizeEvent*);

            private:
                MButton    redxButton;
                MButton    closeButton;
                MLabel     titleLabel;
                MLabel     statusLabel;
                MTabBar    tabBar;
                MWidget    contentWidget;
                MWidget    resizer;
                MCheckBox  checkbox;
                MScrollBar scrollBar;

                inline void closeBtnClicked() { closeWindow(); }
        };
    }
}