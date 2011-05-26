#include <windows.h>
#include "MApplication.h"
#include "MainWindow.h"

using namespace MetalBone;
using namespace MetalBone::Demo;

int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int)
{
	MApplication app;
	app.loadStyleSheetFromFile(L"theme\\theme.css");

	SAMainWindow* mainWindow = new SAMainWindow();
	mainWindow->show();

	// The title can only be set after the window is shown.
	mainWindow->setWindowTitle(L"MetalBone Demo");
	return app.exec();
}