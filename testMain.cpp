#include "MBGlobal.h"
#include "MApplication.h"
#include "MWidget.h"
#include "vld.h"

#include <iostream>
#include <windows.h>
#include <tchar.h>

// #include "test/test_slot.cpp"
// #include "test/test_widget.cpp"
// #include "test/test_stylesheet.cpp"

#ifdef MSVC
int WINAPI _tWinMain(HINSTANCE,HINSTANCE,PTSTR,int)
#else
int main(int argc,char** argv)
#endif
{
//	testDisplay();
//	testWidgetclose();
//	test_resource();
//	testCSSBackgroundRenderObject();
//	

	// std::wstring css = L"#MainWindow{background:url(test.jpg) 30 20 20 30 no-repeat right center padding;padding:150;}";
	std::wstring css = L"#MainWindow{background:url(testBg.jpg) border;margin:10px;border: 4px solid #000000;border-radius:5 10 0 15;}";
	MApplication app;
	app.setStyleSheet(css);
	MWidget* mainWindow = new MWidget();
	mainWindow->setObjectName(L"MainWindow");
	mainWindow->resize(400,380);
	mainWindow->show();
	mainWindow->setAttributes(WA_DeleteOnClose);
	app.exec();
	return 0;
}
