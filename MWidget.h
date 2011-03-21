#ifndef GUI_MWIDGET_H
#define GUI_MWIDGET_H
#include "MBGlobal.h"
#include <string>
namespace MetalBone
{
	enum WindowFlags
	{
		WF_Widget			= 0,
		WF_Window			= 0x1, // ����������󻯣���С�����رհ�ť
		WF_Dialog			= 0x2 | WF_Window, // ���������رհ�ť
		WF_Popup			= 0x4 | WF_Window, // û��ϵͳ�߿������WF_BORDERһ��ʹ�ã��򴴽�һ��ֻ�б߿�û�б������Ĵ���
		WF_Tool				= 0x8 | WF_Window,

		WF_MaximizeButton	= 0x100, // ��ͬʱ�����رհ�ť��
		WF_MinimizeButton	= 0x200, // ��ͬʱ�����رհ�ť��
		WF_HelpButton		= 0x400, // ��ͬʱ�����رհ�ť��
		WF_CloseButton	= 0x800,
		WF_TitleBar		= 0x1000, // ֻ����WF_CAPTION�ᴴ��һ��ֻ�б�������û�йرհ�ť�Ĵ���
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
			// ����ʱ��ɾ������children��
			virtual ~MWidget();

			// ��¼WindowFlags�����isWindow()�����棬��WindowFlagsӦ�õ����ڡ�
			void setWindowFlags(unsigned int);
			unsigned int windowFlags() const;

			// ���onΪtrue�����������attribute������������attribute��
			void setAttributes(WidgetAttributes,bool on = true);
			bool testAttributes(WidgetAttributes) const;
			unsigned int attributes() const;

			void setStyleSheet(const std::wstring&);
			void ensurePolished();

			void setObjectName(const std::wstring&);
			const std::wstring& objectName() const;

			// ���MWidget����ΪWF_Window��һ������true��
			// ���MWidget����ΪWF_Widget�������parent()���ز�Ϊ0����isWindow()һ������false;
			// ���parent()Ϊ0����windowHandle()������-1ʱ��isWindow()����true��
			bool	 isWindow() const;
			HWND	 windowHandle() const; // ���ذ������Widget�Ĵ��ڵ�HWND Handle �� NULL
			MWidget* windowWidget() const; // ���ذ������Widget��TopLevelWidget �� this

			inline void addChild(MWidget* c);
			inline void removeChild(MWidget* c);

			// �����WF_Window�������parent�Ĵ��ڣ��ᱻ���ó�������ڵ�Owner��
			// ��������ӵ�parent����ʾ�б����
			// setParent()��ͬʱ����hide()��������widget��
			void	 setParent(MWidget* parent);
			MWidget* parent() const; // �������Widget�ĸ�MWidget �� 0
			const std::list<MWidget*>&	children() const;

			void setWindowTitle(const std::wstring&);
			const std::wstring& windowTitle() const;

			void show();
			void hide();
			bool isHidden() const;

			// ���isWindow()����false����showMaximized(),showMinimized(),closeWindow()�޲�����
			void showMaximized();
			void showMinimized();
			void closeWindow(); // ����SendMessage()�򴰿ڷ���һ��WM_CLOSE�¼���

			// ֻ��Widget�Ǵ��ڵ�����²Ż���յ�����¼���
			// Ĭ������£�MEventΪaccepted����ʱ���Widget����WA_DeleteOnClose���ԣ�
			// ���ɾ�����Widget;�������ش��ڡ�
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
