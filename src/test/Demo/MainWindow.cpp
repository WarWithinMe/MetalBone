#include "MainWindow.h"
#include "MApplication.h"
#include "MStyleSheet.h"

namespace MetalBone {
namespace Demo {

	void SALabel::setText(const std::wstring& t)
	{
		if(text == t) return;

		text = t;
		repaint();
	}

	void SALabel::doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT)
		{ mApp->getStyleSheet()->draw(this,widgetRectInRT,clipRectInRT,text); }

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

		closeButton.clicked.Connect(this, &SAMainWindow::closeBtnClicked);
		redxButton.clicked.Connect(this, &SAMainWindow::closeBtnClicked);

		titleLabel.setText(L"MetalBone Demo");
		statusLabel.setText(L"ÕâÊÇMetalBone Demo.");

		setAttributes(WA_DeleteOnClose);
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

		// TODO: Resize content widget. ( take scroll bar into account )
	}
}
}