#include "../MWidget.h"
#include "../MApplication.h"
#include <list>
#include <set>
#include <stdio.h>

template<class T>
void outputChildren(const T& container,MWidget* parent)
{
	if(parent)
	{
		fwprintf(stderr,L"\n============================\nChildren of \"%ls\" [%s]. Handle: %d",
				 parent->windowTitle().c_str(),
				 (parent->isWindow() ? L"Window" : L"Widget"), parent->windowHandle());
	}else
		fwprintf(stderr,L"\n============================\nTop Level Windows");
	typename T::const_iterator iter = container.begin();
	typename T::const_iterator iterEnd = container.end();
	typedef typename T::value_type Value;
	int index = 0;
	while(iter != iterEnd)
	{
		Value v = *iter;
		fwprintf(stderr,L"\n%d\t\"%ls\"\t\tHandle:%d",index,v->windowTitle().c_str(),v->windowHandle());
		++index;
		++iter;
	}
	fflush(stderr);
}

void testDisplay()
{
	MApplication app;
	setlocale(LC_ALL,".936");
	//	============================
	//	Children of "第一个窗口" [Widget]. Handle: 0
	//	0	"Widget1"		Handle:0
	//	1	"Widget2"		Handle:0
	//	2	"Widget3"		Handle:0
	//	3	"Widget4"		Handle:0
	MWidget window;
	window.setWindowTitle(L"第一个窗口");
	MWidget window2;
	window2.setWindowTitle(L"第二个窗口");
	MWidget widget1;
	widget1.setParent(&window);
	widget1.setWindowTitle(L"Widget1");
	MWidget widget2;
	widget2.setParent(&window);
	widget2.setWindowTitle(L"Widget2");
	MWidget widget3;
	widget3.setParent(&window);
	widget3.setWindowTitle(L"Widget3");
	MWidget widget4;
	widget4.setParent(&window);
	widget4.setWindowTitle(L"Widget4");
	outputChildren(window.children(),&window);

	//	============================
	//	Children of "第一个窗口" [Widget]. Handle: 0
	//	0	"Widget1"		Handle:0
	//	1	"Widget3"		Handle:0
	//	2	"Widget4"		Handle:0
	widget2.setParent(0);
	outputChildren(window.children(),&window);

	//	============================
	//	Children of "第一个窗口" [Widget]. Handle: 0
	//	0	"Widget1"		Handle:0
	//	1	"Widget4"		Handle:0
	//	============================
	//	Children of "第二个窗口" [Widget]. Handle: 0
	//	0	"Widget3"		Handle:0
	//	============================
	//	Top Level Windows
	window2.addChild(&widget3);
	outputChildren(window.children(),&window);
	outputChildren(window2.children(),&window2);
	outputChildren(app.topLevelWindows(),0);

	//	============================
	//	Top Level Windows
	//	0	"第二个窗口"		Handle:983748
	//	1	"第一个窗口"		Handle:459468
	window.show();
	window2.show();
	outputChildren(app.topLevelWindows(),0);

	//	============================
	//	Children of "第一个窗口" [Window]. Handle: 459468
	//	0	"Widget1"		Handle:459468
	//	1	"Widget4"		Handle:459468
	//	2	"第二个窗口"	Handle:459468
	//	============================
	//	Children of "第二个窗口" [Widget]. Handle: 459468
	//	0	"Widget3"		Handle:459468
	//	============================
	//	Top Level Windows
	//	0	"第一个窗口"		Handle:459468
	window2.setParent(&window);
	outputChildren(window.children(),&window);
	outputChildren(window2.children(),&window2);
	outputChildren(app.topLevelWindows(),0);

	//	============================
	//	Children of "第一个窗口" [Window]. Handle: 459468
	//	0	"Widget1"		Handle:459468
	//	1	"Widget4"		Handle:459468
	//	============================
	//	Children of "第二个窗口" [Window]. Handle: 1049284
	//	0	"Widget3"		Handle:1049284
	//	============================
	//	Top Level Windows
	//	0	"第二个窗口"		Handle:1049284
	//	1	"第一个窗口"		Handle:459468
	window2.setParent(0);
	window2.setWindowFlags(WF_Window);
	window2.setParent(&window);
	window2.show();
	outputChildren(window.children(),&window);
	outputChildren(window2.children(),&window2);
	outputChildren(app.topLevelWindows(),0);
}

void testWidgetclose()
{
	MApplication app;
	MWidget* widget = new MWidget();
	widget->show();
	widget->setWindowTitle(L"Widget1");
	widget->setAttributes(WA_DeleteOnClose);
	MWidget* widget2 = new MWidget();
	widget2->show();
	widget2->setWindowTitle(L"Widget2");
	widget2->setAttributes(WA_DeleteOnClose);
	widget2->showMaximized();
	app.exec();
}
