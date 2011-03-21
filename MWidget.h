#ifndef GUI_MWIDGET_H
#define GUI_MWIDGET_H
#include "MBGlobal.h"
#include <string>
namespace MetalBone
{
	enum WindowFlags
	{
		WF_Widget			= 0,
		WF_Window			= 0x1, // 标题栏，最大化，最小化，关闭按钮
		WF_Dialog			= 0x2 | WF_Window, // 标题栏，关闭按钮
		WF_Popup			= 0x4 | WF_Window, // 没有系统边框，如果和WF_BORDER一起使用，则创建一个只有边框，没有标题栏的窗体
		WF_Tool				= 0x8 | WF_Window,

		WF_MaximizeButton	= 0x100, // 会同时创建关闭按钮。
		WF_MinimizeButton	= 0x200, // 会同时创建关闭按钮。
		WF_HelpButton		= 0x400, // 会同时创建关闭按钮。
		WF_CloseButton	= 0x800,
		WF_TitleBar		= 0x1000, // 只包含WF_CAPTION会创建一个只有标题栏，没有关闭按钮的窗体
		WF_Border			= 0x2000,
		WF_ThinBorder		= 0x4000,

		WF_AlwaysOnTop		= 0x10000,
		WF_AlwaysOnBottom	= 0x20000,

		WF_DontShowOnTaskbar	= 0x40000
	};

	enum WidgetAttributes
	{
		WA_DeleteOnClose = 0x1,
		WA_NoStyleSheet = 0x2,
		WA_TranspantBackground = 0x4,
		WA_Hover = 0x8 // Change apperent when hover
	};

	enum WindowStates
	{
		WindowNoState = 0,
		WindowMinimized,
		WindowMaximized,
		WindowActive
	};

	class MEvent;
	class MWidget
	{
		public:
			MWidget(MWidget* parent = 0);
			// 析构时会删除所有children。
			virtual ~MWidget();

			// 记录WindowFlags。如果isWindow()返回真，则将WindowFlags应用到窗口。
			void setWindowFlags(unsigned int);
			unsigned int windowFlags() const;

			// 如果on为true，则设置这个attribute，否则清除这个attribute。
			void setAttributes(WidgetAttributes,bool on = true);
			bool testAttributes(WidgetAttributes) const;
			unsigned int attributes() const;

			void setStyleSheet(const std::wstring&);
			void ensurePolished();

			void setObjectName(const std::wstring&);
			const std::wstring& objectName() const;

			// 如果MWidget类型为WF_Window，一定返回true。
			// 如果MWidget类型为WF_Widget，如果有parent()返回不为0，则isWindow()一定返回false;
			// 如果parent()为0，则windowHandle()不等于-1时，isWindow()返回true。
			bool	 isWindow() const;
			HWND	 windowHandle() const; // 返回包含这个Widget的窗口的HWND Handle 或 NULL
			MWidget* windowWidget() const; // 返回包含这个Widget的TopLevelWidget 或 this

			inline void addChild(MWidget* c);
			inline void removeChild(MWidget* c);

			// 如果是WF_Window，则包含parent的窗口，会被设置成这个窗口的Owner。
			// 否则则添加到parent的显示列表那里。
			// setParent()会同时调用hide()，来隐藏widget。
			void	 setParent(MWidget* parent);
			MWidget* parent() const; // 返回这个Widget的父MWidget 或 0
			const std::list<MWidget*>&	children() const;

			void setWindowTitle(const std::wstring&);
			const std::wstring& windowTitle() const;

			void show();
			void hide();
			bool isHidden() const;

			// 如果isWindow()返回false，则showMaximized(),showMinimized(),closeWindow()无操作。
			void showMaximized();
			void showMinimized();
			void closeWindow(); // 利用SendMessage()向窗口发送一个WM_CLOSE事件。

			// 只当Widget是窗口的情况下才会接收到这个事件。
			// 默认情况下，MEvent为accepted。此时如果Widget带有WA_DeleteOnClose属性，
			// 则会删除这个Widget;否则隐藏窗口。
			virtual void closeEvent(MEvent*) {}

			inline void move(int x, int y);
			inline void resize(int width, int height);
			void setGeometry(int x, int y, int width, int height);
		private:
			int x;
			int y;
			int width;
			int height;

			struct WidgetExtraData* data;
			friend struct WidgetExtraData;
	};

	inline void MWidget::addChild(MWidget* c){ c->setParent(this); }
	inline void MWidget::move(int xpos, int ypos)
	{
		if(xpos != x && ypos != y)
			setGeometry(xpos,ypos,width,height);
	}
	inline void MWidget::resize(int w, int h)
	{
		if(w != width && h != height)
			setGeometry(x,y,w,h);
	}
	inline void MWidget::removeChild(MWidget* c)
	{
		M_ASSERT(c->parent() == this);
		c->setParent(0);
	}

}
#endif // MWIDGET_H
