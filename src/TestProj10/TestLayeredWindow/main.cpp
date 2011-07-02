#include "MBGlobal.h"
#include "MApplication.h"
#include "MWidget.h"
#include "MResource.h"
#include "MStyleSheet.h"
#include "MEvent.h"

#ifdef _DEBUG
#include "vld.h"
#endif

#include <iostream>
#include <sstream>
#include <fstream>
#include <windows.h>
#include <vector>

class NewWidget : public MWidget
{

	virtual void mousePressEvent(MMouseEvent* e)
	{
		std::wstringstream ss;
		ss << "[On Mouse Press] X:" << e->getX();
		ss << ", Y:" << e->getY();
		ss << "   Widget: " << objectName() << "\n";

		mDebug(ss.str().c_str());
	}
};
class ChildWidget : public NewWidget
{
	virtual void doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT)
	{
		std::wstring text = L"Î¢ÈíÑÅºÚ";
		mApp->getStyleSheet()->draw(this,widgetRectInRT,clipRectInRT,text);
	}
};
class CheckBox    : public NewWidget {};
class Button      : public NewWidget
{
	virtual void doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT)
	{
		std::wstring text = L"Î¢ÈíÑÅºÚ";
		mApp->getStyleSheet()->draw(this,widgetRectInRT,clipRectInRT,text);
	}
};

struct TestWidgetController {
	std::vector<MWidget*> allWidgets;

	TestWidgetController() {}
	~TestWidgetController(){delete sc;delete sc2;delete sc3;}
	void createWidgets();

	void updateC10();
	void globalShortCut() { OutputDebugStringW(L"Global ShortCut."); allWidgets[0]->show(); }
	void localShortCut() { OutputDebugStringW(L"Local ShortCut."); allWidgets[0]->hide(); }

	MShortCut* sc;
	MShortCut* sc2;
	MShortCut* sc3;

	enum WidgetIndex
	{
		MainWindow = 0,
		C0,                     C1,             C2,          C3,         C4,
		              C10,      C11,C12,C13,C14,     C30,C31,C32,C33,C34,
			C100,C101,C102,C103,C104,

	};
};

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PTSTR,int)
{
	std::wifstream cssReader;
	cssReader.imbue(std::locale(".936"));
	cssReader.open("theme.css",std::ios_base::in);
	std::wstring wss((std::istreambuf_iterator<wchar_t>(cssReader)),std::istreambuf_iterator<wchar_t>());
	MApplication app;
	app.setStyleSheet(wss);

	TestWidgetController controller;
	controller.createWidgets();

	MTimer timer;
	timer.timeout.Connect(&controller,&TestWidgetController::updateC10);
	timer.start(5000);

	app.exec();
	return 0;
}

void TestWidgetController::updateC10()
{
// 	allWidgets.at(C10)->repaint();
}

void TestWidgetController::createWidgets()
{
	MWidget* mainWindow = new MWidget();
	mainWindow->setObjectName(L"mainWindow");
	mainWindow->setGeometry(100,100,500,500);
	mainWindow->setWindowFlags(WF_AllowTransparency | WF_MinimizeButton);
	mainWindow->setAttributes(WA_DeleteOnClose);
	mainWindow->setAttributes(WA_NonChildOverlap,false);
	allWidgets.push_back(mainWindow);

	MWidget* c0 = new ChildWidget; 
	c0->setObjectName(L"child0");
	c0->setGeometry(50,0,100,200);
	c0->setParent(mainWindow);
	allWidgets.push_back(c0);

	MWidget* c1 = new ChildWidget; 
	c1->setObjectName(L"child1");
	c1->setGeometry(50,100,300,300);
	c1->setParent(mainWindow);
	allWidgets.push_back(c1);

	MWidget* c2 = new ChildWidget; 
	c2->setObjectName(L"child2");
	c2->setGeometry(50,0,100,200);
	c2->setParent(mainWindow);
	allWidgets.push_back(c2);

	MWidget* c3 = new ChildWidget; 
	c3->setObjectName(L"child3");
	c3->setGeometry(100,0,50,50);
	c3->setParent(mainWindow);
	allWidgets.push_back(c3);

	MWidget* c4 = new ChildWidget;
	c4->setObjectName(L"child4");
	c4->setGeometry(355,150,200,200);
	c4->setParent(mainWindow);
	allWidgets.push_back(c4);

	c0->show();
	c1->show();
	c2->show();
	c3->show();
	c4->show();

	Button* c10 = new Button;
	c10->setObjectName(L"button10");
	c10->setGeometry(50,50,80,30);
	c10->setParent(c1);
	c10->show();
	c10->setFocusPolicy(ClickFocus);
	allWidgets.push_back(c10);
	c10->setToolTip(L"This is Button10. Test very long tooltip. Test very long tooltip. Test very long tooltip. Test very long tooltip.");
	sc = new MShortCut(WinModifier | CtrlModifier , 0x53, 0, true);
	sc2= new MShortCut(CtrlModifier | NoModifier, 0x53, c10, false);
	sc3= new MShortCut(CtrlModifier | NoModifier, 0x44, 0, false);
	sc->invoked.Connect(this,&TestWidgetController::globalShortCut);
	sc2->invoked.Connect(this,&TestWidgetController::localShortCut);
	sc3->invoked.Connect(this,&TestWidgetController::localShortCut);

// 	mainWindow->setMinimumSize(500,500);
// 	mainWindow->setMaximumSize(500,500);

	mainWindow->show();

	MWidget* mainWindow2 = new MWidget();
	mainWindow2->setObjectName(L"mainWindow");
	mainWindow2->setGeometry(200,200,500,500);
	mainWindow2->setAttributes(WA_DeleteOnClose);
	mainWindow2->setWindowFlags(WF_AllowTransparency | WF_MinimizeButton);
	mainWindow2->show();

	MWidget* tRoleWidget = new MWidget();
	tRoleWidget->setParent(mainWindow);
	tRoleWidget->setGeometry(0,0,500,30);
	tRoleWidget->setWidgetRole(WR_Caption);
	tRoleWidget->show();
}