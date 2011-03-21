#ifndef TEST_WIDGETCLOSE_H
#define TEST_WIDGETCLOSE_H
#include "../MApplication.h"
#include "../MWidget.h"

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

#endif // TEST_WIDGETCLOSE_H
