#include "MainWindow.h"
#include "MApplication.h"
#include "MStyleSheet.h"

namespace MetalBone {
namespace Demo {

    SAMainWindow::SAMainWindow()
    {
        setWindowFlags(WF_AllowTransparency | WF_MinimizeButton);
        setObjectName(L"SettingWindow");
        setMinimumSize(400,400);
        ensurePolished();

        titleLabel.setParent(this);
        titleLabel.setObjectName(L"TitleLabel");
        titleLabel.ensurePolished();
        titleLabel.setWidgetRole(WR_Caption);
        titleLabel.show();

        redxButton.setParent(this);
        redxButton.setObjectName(L"RedTrafficLight");
        redxButton.ensurePolished();
        redxButton.show();

        tabBar.setParent(this);
        tabBar.setObjectName(L"TabBar");
        tabBar.ensurePolished();
        tabBar.show();

        contentWidget.setParent(this);
        contentWidget.setObjectName(L"ContentArea");
        contentWidget.ensurePolished();
        contentWidget.show();

        closeButton.setParent(this);
        closeButton.setObjectName(L"CloseButton");
        closeButton.ensurePolished();
        closeButton.show();

        statusLabel.setParent(this);
        statusLabel.setObjectName(L"StatusLabel");
        statusLabel.ensurePolished();
        statusLabel.show();

        resizer.setParent(this);
        resizer.setObjectName(L"Resizer");
        resizer.setWidgetRole(WR_BottomRight);
        resizer.setAttributes(WA_DontShowOnScreen);
        resizer.ensurePolished();
        resizer.show();

        statusLabel.setToolTip(L"这是MetalBone Demo(๑￫ܫ￩)");

        scrollBar.setParent(this);
        scrollBar.showButtons(false);
        scrollBar.ensurePolished();
        scrollBar.setRange(0,100);
        scrollBar.show();

        closeButton.clicked.Connect(this, &SAMainWindow::closeBtnClicked);
        redxButton.clicked.Connect(this, &SAMainWindow::closeBtnClicked);

        titleLabel.setText(L"MetalBone Demo");
        statusLabel.setText(L"这是MetalBone Demo.");

        setAttributes(WA_DeleteOnClose);

        tabBar.addTab(L"常规设置",L"theme\\icon1.png");
        tabBar.addTab(L"设置2",L"theme\\icon2.png");
        tabBar.addTab(L"用户",L"theme\\icon3.png");
        tabBar.addTab(L"常规设置2",L"theme\\icon4.png");
        tabBar.addTab(L"常规设置3",L"theme\\icon5.png");
        tabBar.addTab(L"zh",L"theme\\icon6.png");
        tabBar.addTab(L"很长很长的设置名字",L"theme\\icon7.png");
        tabBar.addTab(L"关于插件",L"theme\\icon8.png");
    }

    void SAMainWindow::resizeEvent(MResizeEvent* ev)
    {
        MSize newSize = ev->getNewSize();
        MSize oldSize = ev->getOldSize();
        long xDelta = newSize.width() - oldSize.width(); 
        long yDelta = newSize.height() - oldSize.height();

        // Offset right-align widgets. 
        redxButton.move(redxButton.pos().offset(xDelta,0));
        closeButton.move(closeButton.pos().offset(xDelta,yDelta));
        resizer.move(resizer.pos().offset(xDelta,yDelta));

        // Resize left-align widgets.
        MPoint slPos = statusLabel.pos();
        MSize  slSize = statusLabel.size();
        statusLabel.setGeometry(slPos.x, slPos.y + yDelta, slSize.width() + xDelta, slSize.height());
        tabBar.resize(tabBar.size().enlarge(xDelta,0));
        titleLabel.resize(titleLabel.size().enlarge(xDelta,0));

        scrollBar.setGeometry(scrollBar.x() + xDelta, scrollBar.y(),
            scrollBar.width(), int(0.618 * contentWidget.height()));

        // TODO: Resize content widget. ( take scroll bar into account )
    }
}
}