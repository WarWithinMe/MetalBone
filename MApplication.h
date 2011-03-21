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

			// 进入主Event Loop
			int exec();

			// 导致退出Message Loop。程序即将结束。
			inline void exit(int returnCode);
			// 设置是否在最后一个窗口被关闭（不是隐藏，是被删除）时，调用exit(0);
			// 默认为true。
			void setQuitOnLastWindowClosed(bool);

			// 返回指向本程序的HINSTANCE
			HINSTANCE getAppHandle() const;

			// 设置自定义的Window Procedure，当收到窗口消息时，
			// 首先调用Custom Window Procedure，如果它返回true，
			// 则表明消息已经处理。否则，则采用默认的处理方式。
			typedef bool (*WinProc)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
			void setCustomWindowProc(WinProc);

			const std::set<MWidget*>& topLevelWindows() const;

			// 设置应用于整个程序的样式表。
			// 选择器
			// .ClassA	匹配类型为ClassA的widget
			// ClassB	匹配ClassB，和继承自ClassB的widget
			// #MyClass	匹配id为MyClass的widget
			// 组合选择器
			// .ClassA .ClassB 匹配ClassA的子孙，类型为ClassB的widget
			// .ClassA >.ClassB 匹配父亲为ClassA，类型为ClassB的widget
			// 不支持
			// .ClassA.ClassB // Widget里面没有这个概念
			// .ClassA#AAA#BBB // Widget只有一个Id
			// 单位只支持px
			// 不支持颜色名
			void setStyleSheet(const std::wstring& css);
#ifdef METALBONE_USE_SIGSLOT
			// 当MApplication退出Message Loop的时候发送aboutToQuit信号
			Signal0 aboutToQuit;
#else
		protected:
			virtual void aboutToQuit(){}
#endif

		protected:
			// 注册窗口类前调用这个函数获取要注册的窗口类
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
