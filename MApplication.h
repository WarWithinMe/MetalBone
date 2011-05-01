#ifndef GUI_MAPPLICATION_H
#define GUI_MAPPLICATION_H
#include "MBGlobal.h"
#include <string>
#include <windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <dwrite.h>

#define mApp MetalBone::MApplication::instance()

namespace MetalBone
{
	class MWidget;
	class MStyleSheetStyle;
	struct MApplicationData;

	class MApplication
	{
		public:
			MApplication(bool hardwareAccelerated = true);
			virtual ~MApplication();
			inline static MApplication* instance();

			// ������Event Loop
			int exec();

			// �����˳�Message Loop�����򼴽�������
			inline void exit(int returnCode);
			// �����Ƿ������һ�����ڱ��رգ��������أ��Ǳ�ɾ����ʱ������exit(0);
			// Ĭ��Ϊtrue��
			void setQuitOnLastWindowClosed(bool);

			// ����ָ�򱾳����HINSTANCE
			HINSTANCE getAppHandle() const;

			// �����Զ����Window Procedure�����յ�������Ϣʱ��
			// ���ȵ���Custom Window Procedure�����������true��
			// �������Ϣ�Ѿ��������������Ĭ�ϵĴ���ʽ��
			typedef bool (*WinProc)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
			void setCustomWindowProc(WinProc);

			const std::set<MWidget*>& topLevelWindows() const;

			bool isHardwareAccerated() const;

			// ����Ӧ���������������ʽ��
			// ѡ����
			// .ClassA	ƥ������ΪClassA��widget
			// ClassB	ƥ��ClassB���ͼ̳���ClassB��widget
			// #MyClass	ƥ��idΪMyClass��widget
			// ���ѡ����
			// .ClassA .ClassB ƥ��ClassA���������ΪClassB��widget
			// .ClassA >.ClassB ƥ�丸��ΪClassA������ΪClassB��widget
			// ��֧��
			// .ClassA.ClassB // Widget����û���������
			// .ClassA#AAA#BBB // Widgetֻ��һ��Id
			// ��ֵû�е�λ����λĬ����px��
			// ��֧����ɫ��
			void setStyleSheet(const std::wstring& css);
			MStyleSheetStyle* getStyleSheet();

			ID2D1Factory*       getD2D1Factory();
			IDWriteFactory*     getDWriteFactory();
			IWICImagingFactory* getWICImagingFactory();

			inline unsigned int winDpi() const;

#ifdef METALBONE_USE_SIGSLOT
			Signal0 aboutToQuit;		// ��MApplication�˳�Message Loop��ʱ����aboutToQuit�ź�
#else
		protected: virtual void aboutToQuit(){}
#endif

		protected:
			// MApplicationע�ᴰ����ǰ�������������ȡҪע��Ĵ�����
			virtual void setupRegisterClass(WNDCLASSW&);


		private:
			static MApplication* s_instance;
			MApplicationData* mImpl;
			unsigned int windowsDPI;
			friend class MWidget;
	};






	inline MApplication* MApplication::instance()	{ return s_instance; }
	inline void MApplication::exit(int ret)			{ PostQuitMessage(ret); }
	inline unsigned int MApplication::winDpi() const { return windowsDPI; }
}

#endif // GUI_MAPPLICATION_H
