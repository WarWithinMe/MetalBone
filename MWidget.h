#ifndef GUI_MWIDGET_H
#define GUI_MWIDGET_H
#include "MBGlobal.h"
#include "MStyleSheet.h"

#include <string>
#include <d2d1.h>

namespace MetalBone
{
	enum WindowFlags
	{
		WF_Widget           = 0,
		WF_Window           = 0x1, // TitleBar, Maximize, Minimize, Close Button
		WF_Dialog           = 0x2 | WF_Window, // TitleBar, Close Button
		WF_Popup            = 0x4 | WF_Window, // No system border. Use it with WF_Border, you can
											   // create a window which has border but no titlebar.
		WF_Tool             = 0x8 | WF_Window,

		WF_MaximizeButton   = 0x100, // Also create close button.
		WF_MinimizeButton   = 0x200, // Also create close button.
		WF_HelpButton       = 0x400, // Also create close button.
		WF_CloseButton      = 0x800,
		WF_TitleBar         = 0x1000,// Will create a window which has titlebar, but no buttons.
		WF_Border           = 0x2000,
		WF_ThinBorder       = 0x4000,

		WF_AlwaysOnTop      = 0x10000,
		WF_AlwaysOnBottom   = 0x20000,

		WF_DontShowOnTaskbar= 0x40000
	};

	enum WidgetAttributes
	{
		WA_DeleteOnClose     = 0x1,
		WA_NoStyleSheet      = 0x2,
		WA_ConstStyleSheet   = 0x4,  // If set, we only polish widget once, so that changing
								 // parent won't recalc the stylesheet again.
		WA_OpaqueBackground  = 0x8,
		WA_Hover             = 0x10  // Change apperent when hover
	};

	enum WindowStates
	{
		WindowNoState = 0,
		WindowMinimized,
		WindowMaximized,
		WindowActive
	};

	class MEvent;
	class MPaintEvent;
	class MWidget
	{
		public:
			MWidget(MWidget* parent = 0);
			virtual ~MWidget(); // We will delete very child of this widget when desctruction.

			inline void setObjectName (const std::wstring&);
			inline void setWindowTitle(const std::wstring&);
			inline const std::wstring& objectName()  const;
			inline const std::wstring& windowTitle() const;

			// Return the HWND handle contains this widget or NULL.
			inline HWND windowHandle() const; 
			// Return the top level widget contains this widget or this.
			inline MWidget* windowWidget();   
			// If WF_Window, return true.
			// If WF_Widget, and if parent() return non-zero, this return false
			// If parent() return zero, and windowHandle() return greater than -1, this return true.
			bool	 isWindow() const;
			// Set the window flags. The flag will take effort as soon as this widget
			// becomes a window (i.e. isWindow() return true).
			void setWindowFlags(unsigned int);
			inline unsigned int windowFlags() const;
			// If on is true, we set the attr, otherwise we clear it.
			inline void setAttributes(WidgetAttributes,bool on = true);
			inline bool testAttributes(WidgetAttributes) const;
			inline unsigned int attributes() const;

			void setStyleSheet(const std::wstring&);
			void ensurePolished(); // Call ensurePolished() to set Geometry of this.

			inline ID2D1DCRenderTarget* getDCRenderTarget();
			inline void repaint();
			void repaint(int x, int y, unsigned int width, unsigned int height);

			// Return the parent widget of this or 0.
			inline MWidget* parent() const;
			// If parent is 0, the widget will be hidden.
			void setParent(MWidget* parent);
			// Sets the owner of this Window to the Window contains p.
			void setWindowOwner(MWidget* p);
			inline const std::list<MWidget*>& children() const;

			bool isHidden() const;

			// If isWindow() return false. No-op.
			void showMaximized();
			void showMinimized();
			// Use SendMessage() to send a WM_CLOSE msg to the window.
			// The window is then closed, and closeEvent() is called.
			void closeWindow();

			// After calling show() on a Window widget, the window is
			// ensured to be created.
			void show();
			void hide();
			inline D2D_SIZE_U size() const;
			inline D2D_SIZE_U minSize() const;
			inline D2D_SIZE_U maxSize() const;
			inline void move(int x, int y);
			inline void resize(unsigned int width, unsigned int height);
			void setGeometry(int x, int y, unsigned int width, unsigned int height);
			void setMinimumSize(unsigned int minWidth, unsigned int minHeight);
			void setMaximumSize(unsigned int maxWidth, unsigned int maxHeight);


			// Only received this event when MWidget is a window.
			// The MEvent is accepted by default, and if the MWidget has WA_DeleteOnClose,
			// it will be deleted. Otherwise, the window is hidden.
			virtual void closeEvent(MEvent*) {}
			virtual void paintEvent(MPaintEvent*) {}










			// The rect has been mapped to the topLevel parent.
			virtual void draw(RECT& rect);
			// MApplcation call this overloaded method when it receives WM_PAINT event.
			void drawWindow(PAINTSTRUCT& ps);







		private:
			MWidget* m_parent;
			MWidget* m_topLevelParent;
			HWND m_winHandle; // Only valid when widget is a window.
			ID2D1DCRenderTarget* m_renderTarget;

			std::wstring m_windowTitle;
			std::wstring m_objectName;

			int x;
			int y;
			unsigned int width;
			unsigned int height;
			unsigned int minWidth;
			unsigned int minHeight;
			unsigned int maxWidth;
			unsigned int maxHeight;

			unsigned int m_attributes;
			unsigned int m_windowFlags;
			unsigned int m_windowState;
			unsigned int m_widgetState;

			// children[0] is the bottom-most child.
			std::list<MWidget*> m_children;

			void setTopLevelParentRecursively(MWidget*);
			void setOpaqueBackground(bool);
			bool isOpaqueDrawing() const;

			void createRenderTarget();

		friend class MStyleSheetStyle;
	};





	inline const std::wstring& MWidget::objectName()   const          { return m_objectName; }
	inline const std::wstring& MWidget::windowTitle()  const          { return m_windowTitle; }
	inline D2D_SIZE_U   MWidget::size()                const          { return D2D1::SizeU(width,height); }
	inline D2D_SIZE_U   MWidget::minSize()             const          { return D2D1::SizeU(minWidth,minHeight); }
	inline D2D_SIZE_U   MWidget::maxSize()             const          { return D2D1::SizeU(maxWidth,maxHeight); }
	inline MWidget*     MWidget::parent()              const          { return m_parent; }
	inline HWND         MWidget::windowHandle()        const          { return m_topLevelParent->m_winHandle; }
	inline unsigned int MWidget::windowFlags()         const          { return m_windowFlags; }
	inline unsigned int MWidget::attributes()          const          { return m_attributes; }
	inline MWidget*     MWidget::windowWidget()                       { return m_topLevelParent; }
	inline void         MWidget::repaint()                            { repaint(x,y,width,height); }
	inline void         MWidget::setObjectName(const std::wstring& n) { m_objectName = n; }
	inline bool         MWidget::testAttributes(WidgetAttributes a) const { return (m_attributes & a) != 0; }
	inline const std::list<MWidget*>& MWidget::children() const       { return m_children;}
	inline ID2D1DCRenderTarget* MWidget::getDCRenderTarget()            { return windowWidget()->m_renderTarget; }
	void                MWidget::setWindowTitle(const std::wstring& t)
	{
		m_windowTitle = t;
		if(isWindow())
			SetWindowText(windowHandle(),t.c_str());
	}
	inline void MWidget::move(int xpos, int ypos)
	{
		if(xpos != x && ypos != y)
			setGeometry(xpos,ypos,width,height);
	}
	inline void MWidget::resize(unsigned int w, unsigned int h)
	{
		if(w != width && h != height)
			setGeometry(x,y,w,h);
	}
	inline void MWidget::setAttributes(WidgetAttributes attr, bool on)
	{
		on ? (m_attributes |= attr) : (m_attributes &= (~attr));
	}
}
#endif // MWIDGET_H
