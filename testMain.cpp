#include "MBGlobal.h"
#include "MApplication.h"
#include "MWidget.h"
#include "vld.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <windows.h>
#include <tchar.h>

#ifdef MSVC
int WINAPI _tWinMain(HINSTANCE,HINSTANCE,PTSTR,int)
#else
int main(int argc,char** argv)
#endif
{
	std::wifstream cssReader;
	cssReader.open("theme.css",std::ios_base::in);
	std::wstring wss((std::istreambuf_iterator<wchar_t>(cssReader)),std::istreambuf_iterator<wchar_t>());
	MApplication app;
	app.setStyleSheet(wss);
	MWidget* mainWindow = new MWidget();
	MWidget* checkBox = new MWidget();
	mainWindow->setObjectName(L"MainWindow");
	mainWindow->resize(400,380);
	mainWindow->setAttributes(WA_DeleteOnClose);
	checkBox->setObjectName(L"CheckBox");
	checkBox->setGeometry(30,30,50,30);
	checkBox->setParent(mainWindow);
	mainWindow->show();
	app.exec();
	return 0;
}
