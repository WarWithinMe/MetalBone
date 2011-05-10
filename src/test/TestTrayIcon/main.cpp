#include "MWidget.h"
#include "MApplication.h"
#include "MResource.h"
#include "Resource.h"

class TestSysTrayIcon : public MSysTrayIcon
{
	protected:
		void event(unsigned int msg) { mApp->exit(0); }
};

int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR,int)
{
	MApplication app;
	TestSysTrayIcon tray;
	
	HICON icon = (HICON)::LoadImageW(mApp->getAppHandle(),
		MAKEINTRESOURCEW(IDI_TESTTRAYICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);

	MIcon micon(icon,true);
	tray.setIcon(micon);
	tray.setName(L"TestTrayIconApp");
	tray.show();

	app.exec();
	return 0;
}