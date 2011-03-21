#ifndef GUI_MAPPLICATION_H
#define GUI_MAPPLICATION_H
#include "MBGlobal.h"
#include <string>
#include <windows.h>

#define mApp MetalBone::MApplication::instance()

namespace MetalBone
{
	class MWidget;
	struct MApplicationData;
	class MApplication
	{
		public:
			MApplication();
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
			// ��λֻ֧��px
			// ��֧����ɫ��
			void setStyleSheet(const std::wstring& css);
#ifdef METALBONE_USE_SIGSLOT
			// ��MApplication�˳�Message Loop��ʱ����aboutToQuit�ź�
			Signal0 aboutToQuit;
#else
		protected:
			virtual void aboutToQuit(){}
#endif

		protected:
			// ע�ᴰ����ǰ�������������ȡҪע��Ĵ�����
			virtual void setupRegisterClass(WNDCLASS&);


		private:
			static MApplication* s_instance;
			MApplicationData* mImpl;

			friend class MWidget;
	};

	inline MApplication* MApplication::instance() { return s_instance; }
	inline void MApplication::exit(int ret) { PostQuitMessage(ret); }
}

#endif // GUI_MAPPLICATION_H
