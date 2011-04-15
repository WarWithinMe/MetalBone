#ifndef GUI_MWIDGET_H
#define GUI_MWIDGET_H
#include "MBGlobal.h"

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

		WF_AllowTransparency= 0x10000,// This will create a window with style WS_EX_LAYERED.

		WF_AlwaysOnTop      = 0x20000,
		WF_AlwaysOnBottom   = 0x40000,

		WF_DontShowOnTaskbar= 0x80000
	};

	enum WidgetAttributes
	{
		WA_DeleteOnClose     = 0x1,
		WA_NoStyleSheet      = 0x2,
		WA_ConstStyleSheet   = 0x4,  // If set, we only polish widget once, so that changing
								     // parent won't recalc the stylesheet again.
		WA_AutoBG            = 0x8,  // Default. The widget's opacity is detemined by the CSS. When a 
									 // non-opaque widget redraws, it will cause widgets under it to redraw too.
		WA_OpaqueBG          = 0x10, // The widget is opaque. Clearing WA_AutoBG and WA_OpaqueBG makes it semi-transparent.
		WA_NonChildOverlap   = 0x20, // Default. The children of this widget are promised not overlapped
									 // each other. This will optimize a bit when we redraw, since we
									 // don't have to test if there's a widget overlaps the drawing rect.

		WA_Hover             = 0x40  // Change apperent when hover
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
	class MStyleSheetStyle;
	class MWidget;
	class MRegion;
	struct MApplicationData;
	struct WindowExtras;
	typedef std::list<MWidget*> MWidgetList; 
	class MWidget
	{
		public:
			MWidget(MWidget* parent = 0);
			virtual ~MWidget(); // We will delete very child of this widget when desctruction.

			inline void setObjectName (const std::wstring&);
			inline const std::wstring& objectName()  const;
			void setWindowTitle(const std::wstring&);
			const std::wstring& windowTitle() const;

			// Return the HWND handle contains this widget or NULL.
			HWND windowHandle() const; 
			// Return the top level widget contains this widget or this.
			inline MWidget* windowWidget();   
			// If WF_Window, return true.
			// If WF_Widget, and if parent() return non-zero, this return false
			// If parent() return zero, and windowHandle() return greater than -1, this return true.
			bool isWindow() const;
			bool hasWindow() const;
			// Set the window flags. The flag will take effort as soon as this widget
			// becomes a window (i.e. isWindow() return true).
			void setWindowFlags(unsigned int);
			inline unsigned int windowFlags() const;
			// If on is true, we set the attr, otherwise we clear it.
			inline void setAttributes(WidgetAttributes,bool on = true);
			inline bool testAttributes(WidgetAttributes) const;
			inline unsigned int attributes() const;

			void setStyleSheet(const std::wstring&);
			void ensurePolished(); // Call ensurePolished() to set Geometry for this.

			// Calling repaint() several times will result in a larger repaint
			// rect, which encloses every specific repaint rect. Such as, call for
			// rect(5,5,10,10) and rect(30,30,35,35) will results in a rect(5,5,35,35) 
			inline void repaint();
			void repaint(int x, int y, unsigned int width, unsigned int height);
			ID2D1RenderTarget* getRenderTarget();

			// Sets the owner of this Window to the Window contains p.
			// If this widget is not a window, nothing happens.
			void setWindowOwner(MWidget* p);
			// If parent is 0, the widget will be hidden.
			void setParent(MWidget* parent);
			// Return the parent widget of this or 0.
			inline MWidget* parent() const;
			inline const MWidgetList& children() const;

			// The widget can be invisible while it's not hidden.
			// The widget is not visible because its parent is not visible,
			// or if during last redraw, the widget doesn't redraw itself.
			bool isVisible() const;
			bool isHidden() const;

			// If isWindow() return false, nothing happen.
			void showMaximized();
			void showMinimized();
			// Use SendMessage() to send a WM_CLOSE msg to the window.
			// The window is then closed, and closeEvent() is called.
			void closeWindow();

			// After calling show() on a Window widget, the window is
			// ensured to be created.
			void show();
			void hide();
			inline D2D_SIZE_U size()     const;
			inline D2D_SIZE_U minSize()  const;
			inline D2D_SIZE_U maxSize()  const;
			inline POINT      pos()      const;
			inline RECT       geometry() const; 
			inline void move(int x, int y);
			inline void resize(unsigned int width, unsigned int height);
			void setGeometry(int x, int y, unsigned int width, unsigned int height);
			void setMinimumSize(unsigned int minWidth, unsigned int minHeight);
			void setMaximumSize(unsigned int maxWidth, unsigned int maxHeight);


			// Only received this event when MWidget is a window.
			// If the MEvent is not accepted, nothing happen.
			// By default the MEvent is accepted, and the window will
			// be destroyed which makes the widget hidden.
			// If the widget has the attribute WA_DeleteOnClose,
			// the widget is also be deleted.
			virtual void closeEvent(MEvent*     ) {}
			virtual void paintEvent(MPaintEvent*) {}









			// The StyleSheetStyle calls this to determine which RenderRule is needed.
			virtual unsigned int getCurrentWidgetPseudo() { return 0; }





		private:
			MWidget* m_parent;
			MWidget* m_topLevelParent;
			WindowExtras* m_windowExtras;

			// children[0] is the bottom-most child.
			MWidgetList m_children;

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

			std::wstring m_objectName;

			void setTopLevelParentRecursively(MWidget*);
			bool isOpaqueDrawing() const;
			inline void setWidgetState(unsigned int,bool);

			void createRenderTarget();
			void destroyWnd();
			void createWnd();

			void drawWindow();
			void draw(int xOffsetInWnd, int yOffsetInWnd, bool drawMySelf);
			void addToUpdateList(RECT& updateRect);

		friend struct MApplicationData;
	};





	inline const std::wstring& MWidget::objectName()   const          { return m_objectName; }
	inline D2D_SIZE_U   MWidget::size()                const          { return D2D1::SizeU(width,height); }
	inline D2D_SIZE_U   MWidget::minSize()             const          { return D2D1::SizeU(minWidth,minHeight); }
	inline D2D_SIZE_U   MWidget::maxSize()             const          { return D2D1::SizeU(maxWidth,maxHeight); }
	inline POINT        MWidget::pos()                 const          { POINT p = {x,y}; return p; }
	inline RECT         MWidget::geometry()            const          { RECT r = {x,y,x+width,y+height}; return r;}
	inline MWidget*     MWidget::parent()              const          { return m_parent; }
	inline unsigned int MWidget::windowFlags()         const          { return m_windowFlags; }
	inline unsigned int MWidget::attributes()          const          { return m_attributes; }
	inline const MWidgetList& MWidget::children()      const          { return m_children;}
	inline MWidget*     MWidget::windowWidget()                       { return m_topLevelParent; }
	inline void         MWidget::repaint()                            { repaint(0,0,width,height); }
	inline void         MWidget::setObjectName(const std::wstring& n) { m_objectName = n; }
	inline bool         MWidget::testAttributes(WidgetAttributes a) const { return (m_attributes & a) != 0; }
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

	inline void MWidget::setWidgetState(unsigned int s,bool on)
	{
		on ? (m_widgetState |= s) : (m_widgetState &= (~s));
	}
}
#endif // MWIDGET_H
