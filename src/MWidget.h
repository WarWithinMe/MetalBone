#pragma once

#include "MBGlobal.h"
#include "MEvent.h"
#include "MUtils.h"
#include "MApplication.h"
#include "MStyleSheet.h"

#include <string>
#include <list>
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
                                               // The window will behave like a normal window if you
                                               // specify the WF_Popup | WF_MinimizeButton
        WF_Tool             = 0x8 | WF_Window,

        WF_MaximizeButton   = 0x100, // Also create close button.
        WF_MinimizeButton   = 0x200, // Also create close button.
        WF_HelpButton       = 0x400, // Also create close button.
        WF_CloseButton      = 0x800,
        WF_TitleBar         = 0x1000,// Will create a window which has titlebar, but no buttons.
        WF_Border           = 0x2000,
        WF_ThinBorder       = 0x4000,

        WF_AllowTransparency= 0x10000,// You must set WF_AllowTransparency | WF_MinimizeButton to make the window
                                        // behave the same as the other window (i.e. can minimize when clicking
                                        // the taskbar icon)

        WF_AlwaysOnTop      = 0x20000,
        WF_AlwaysOnBottom   = 0x40000,

        WF_DontShowOnTaskbar= 0x80000,
        WF_NoActivate       = 0x100000
    };

    enum WidgetAttributes
    {
        WA_DeleteOnClose     = 0x1,
        WA_NoStyleSheet      = 0x2,
        WA_ConstStyleSheet   = 0x4,  // If set, we only polish widget once, so that changing
                                     // parent won't recalc the stylesheet again.
        WA_AutoBG            = 0x8,  // Default. The widget's opacity is detemined by the CSS. When a 
                                     // non-opaque widget redraws, it will cause widgets under it to redraw too.
                                     // If the background image is png of gif, or if it has a complex
                                     // clipping border. Or whatever so makes a widget that may not be a
                                     // opaque rectangle, it's considered non-opaque.
        WA_OpaqueBG          = 0x10, // The widget is opaque. Clearing WA_AutoBG and WA_OpaqueBG makes it semi-transparent.
        WA_NonChildOverlap   = 0x20, // Default. The children of this widget are promised not overlapped
                                     // each other. This will optimize a bit when we redraw, since we
                                     // don't have to test if there's a widget overlaps the drawing rect.

        WA_Hover              = 0x40, // Change appearence when hover
        WA_MouseThrough       = 0x80, // When set, the widget and its children won't receive any mouse
                                      // event, as if they do not exist.
        WA_NoMousePropagation = 0x100,// If a MouseEvent is not accpeted, and this attribute is not set,
                                      // it will by default propagete to its parent.
        WA_TrackMouseMove     = 0x200,// If set, the widget will receive MouseMoveEvent.
        WA_DontShowOnScreen   = 0x400 // If set, the widget won't do any painting. But it still receives mouse event.
    };

    // WidgetRole is used to make the widget to simulate the 
    // Non-client area of the window. For example, if a widget
    // is set to WR_Caption, when the mouse press on it, the 
    // user can drag the window.
    enum WidgetRole
    {
        WR_Normal      = 1,
        WR_Caption     = HTCAPTION     - 2,
        WR_Left        = HTLEFT        - 2,
        WR_Right       = HTRIGHT       - 2,
        WR_Top         = HTTOP         - 2,
        WR_TopLeft     = HTTOPLEFT     - 2,
        WR_TopRight    = HTTOPRIGHT    - 2,
        WR_Bottom      = HTBOTTOM      - 2,
        WR_BottomLeft  = HTBOTTOMLEFT  - 2,
        WR_BottomRight = HTBOTTOMRIGHT - 2
    };

    enum WindowStates
    {
        WindowNoState = 0,
        WindowMinimized,
        WindowMaximized
    };

    enum FocusPolicy
    {
        NoFocus = 0,
        TabFocus,
        ClickFocus,
        MoveOverFocus, // If set, the widget will automatically grab focus when move over.
    };

    class MWidget;
    class MRegion;
    class MToolTip;
    class MCursor;
    class MStyleSheetStyle;
    struct WindowExtras;
    struct MApplicationData;
    typedef std::list<MWidget*> MWidgetList;
    class METALBONE_EXPORT MWidget
    {
        public:
            MWidget(MWidget* parent = 0);
            // We will delete very child of this widget when desctruction.
            virtual ~MWidget(); 

            inline void setObjectName(const std::wstring&);
            inline const std::wstring& objectName()  const;

            // One can only set the window title after the window
            // is created. The window is created when the first
            // time it shows.
            void setWindowTitle(const std::wstring&);
            std::wstring windowTitle() const;

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

            inline void setWidgetRole(WidgetRole);
            inline WidgetRole widgetRole() const;

            void setStyleSheet(const std::wstring&);
            void ensurePolished(); // Call ensurePolished() to set Geometry for this.

            // Calling repaint() several times will result in a larger repaint
            // rect, which encloses every specific repaint rect. Such as, call for
            // rect(5,5,10,10) and rect(30,30,35,35) will results in a rect(5,5,35,35) 
            inline void repaint();
            void repaint(int x, int y, unsigned int width, unsigned int height);

            // Sets the owner of this Window to the Window contains p.
            // If this widget is not a window, nothing happens.
            void setWindowOwner(MWidget* p);
            // If parent is 0, the widget will be hidden.
            void setParent(MWidget* parent);
            // Return the parent widget of this or 0.
            inline MWidget* parent() const;
            inline const MWidgetList& children() const;

            // Finds a visible widget a point{x,y}. After calling this function,
            // x and y will be in the found widget's coordinate.
            // If ignoreMouseThroughWidget is true, we only check those
            // widgets who receive mouseEvent.
            MWidget* findWidget(int& x, int& y, bool ignoreMouseThroughWidget = true);

            void setFocus();
            inline FocusPolicy focusPolicy() const;
            inline void setFocusPolicy(FocusPolicy);

            inline MToolTip* getToolTip();
            // MWidget will take control of the MToolTip
            // And setting a new tooltip will delete the old one.
            void setToolTip(MToolTip*);
            void setToolTip(const std::wstring&);

            // StyleSheet only changes a widget's cursor if it doesn't have one.
            // This function take control of the MCursor. The caller don't have to 
            // delete the cursor.
            inline void setCursor(MCursor*);
            MCursor* getCursor();

            // The widget can be invisible while it's not hidden.
            // The widget is not visible because its parent is not visible,
            // or if during last redraw, the widget doesn't redraw itself.
            bool isVisible()  const;
            bool isHidden()   const;
            bool isDisabled() const;

            // After calling show() on a Window widget, the window is
            // ensured to be created.
            void show();
            void hide();
            void setEnabled(bool enabled);

            // If isWindow() return false, nothing happen.
            void showMaximized();
            void showMinimized();
            // Use PossMessage() to send a WM_CLOSE msg to the window.
            // The window is then closed, and closeEvent() is called.
            void closeWindow();



            // Raise the widget to the top or lower to the bottom of its parent.
            void raise();
            void lower();

            inline MPoint pos()      const;
            inline MSize  size()     const;
            inline MSize  minSize()  const;
            inline MSize  maxSize()  const;
            inline MRect  geometry() const; 
            inline void move   (long x, long y);
            inline void move   (MPoint pos);
            inline void resize (long width, long height);
            inline void resize (MSize size);
            void setGeometry   (long x, long y, long width, long height);
            void setMinimumSize(long minWidth, long minHeight);
            void setMaximumSize(long maxWidth, long maxHeight);


            // These function SHOULD only be used by StyleSheetStyle.
            // The StyleSheetStyle calls this to determine which RenderRule is needed.
            virtual unsigned int getLastWidgetPseudo() const;
            virtual unsigned int getWidgetPseudo(bool markAsLast = false, unsigned int initPseudo = 0);
            void ssSetOpaque(bool opaque);


        protected:
            // MWidget only receive closeEvent when it is a window.
            // If the MEvent is not accepted, nothing happen.
            // By default the MEvent is accepted, and the window will
            // be destroyed which makes the widget hidden.
            // If the widget has the attribute WA_DeleteOnClose,
            // the widget is also be deleted.
            virtual void closeEvent(MEvent*)            {}
            // If MPaintEvent is not accepted, its children won't be painted.
            virtual void focusEvent()                   {}
            virtual void paintEvent(MPaintEvent*)       {}
            virtual void leaveEvent()                   {}
            // If enterEvent is not accepted, it will generate a mouseMoveEvent
            // The event is not accepted by default.
            virtual void enterEvent(MEvent*)            {}
            // mouseMoveEvent is accepted by default.
            virtual void mouseMoveEvent(MMouseEvent*)   {}
            // mousePressEvent / mouseReleaseEvent / mouseDClickEvent is ignored by default.
            virtual void mousePressEvent(MMouseEvent*)  {}
            virtual void mouseReleaseEvent(MMouseEvent*){}
            virtual void mouseDClickEvent(MMouseEvent*) {}
            virtual void keyPressEvent(MKeyEvent*)      {}
            virtual void keyUpEvent(MKeyEvent*)         {}
            virtual void charEvent(MCharEvent*)         {}
            
            virtual void resizeEvent(MResizeEvent*)     {}

            virtual void recreateResource()             {}

            virtual void doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT);

            // When your widget's pseudo state changed, call this function
            // to request a update for your widget (if necessary, i.e. the
            // new pseudo results in a new RenderRule different from the 
            // current one).
            // One can also call repaint() directly, instead of this function
            // to update your widget.
            inline void updateSSAppearance();


        private:
            MWidget* m_parent;
            MWidget* m_topLevelParent;
            WindowExtras* m_windowExtras;

            // children[0] is the bottom-most child.
            MWidgetList m_children;

            long x;
            long y;
            long width;
            long height;
            long minWidth;
            long minHeight;
            long maxWidth;
            long maxHeight;

            unsigned int m_attributes;
            unsigned int m_windowFlags;
            unsigned int m_windowState;
            unsigned int m_widgetState;

            unsigned int lastPseudo;
            unsigned int mainPseudo;

            char e_widgetRole;

            FocusPolicy  fp;
            MToolTip*    m_toolTip;
            MCursor*     m_cursor;
            std::wstring m_objectName;

            void setTopLevelParentRecursively(MWidget*);
            bool isOpaqueDrawing() const;
            inline void setWidgetState(unsigned int, bool on);
            inline bool testWidgetState(unsigned int) const;

            void destroyWnd();
            void createWnd();

            void drawWindow();
            void draw(int xOffsetInWnd, int yOffsetInWnd, bool drawMySelf);

        friend struct MApplicationData;
        friend class  MD2DPaintContext;
    };





    inline const std::wstring& MWidget::objectName()   const          { return m_objectName; }
    inline MSize        MWidget::size()                const          { return MSize(width,height); }
    inline MSize        MWidget::minSize()             const          { return MSize(minWidth,minHeight); }
    inline MSize        MWidget::maxSize()             const          { return MSize(maxWidth,maxHeight); }
    inline MPoint       MWidget::pos()                 const          { return MPoint(x,y); }
    inline MRect        MWidget::geometry()            const          { return MRect(x,y,x+width,y+height); }
    inline MWidget*     MWidget::parent()              const          { return m_parent;      }
    inline unsigned int MWidget::windowFlags()         const          { return m_windowFlags; }
    inline unsigned int MWidget::attributes()          const          { return m_attributes;  }
    inline const MWidgetList& MWidget::children()      const          { return m_children;    }
    inline FocusPolicy  MWidget::focusPolicy()         const          { return fp;            }
    inline void         MWidget::setFocusPolicy(FocusPolicy p)        { fp = p;               }
    inline MToolTip*    MWidget::getToolTip()                         { return m_toolTip;     }
    inline MCursor*     MWidget::getCursor()                          { return m_cursor;      }
    inline MWidget*     MWidget::windowWidget()                       { return m_topLevelParent;   }
    inline void         MWidget::repaint()                            { repaint(0,0,width,height); }
    inline void         MWidget::setWidgetRole(WidgetRole wr)         { e_widgetRole = wr; }
    inline WidgetRole   MWidget::widgetRole() const                   { return (WidgetRole)e_widgetRole; }
    inline unsigned int MWidget::getLastWidgetPseudo() const          { return lastPseudo;         }
    inline void         MWidget::updateSSAppearance()                 { mApp->getStyleSheet()->updateWidgetAppearance(this); }
    inline void         MWidget::setObjectName(const std::wstring& n) { m_objectName = n;          }
    inline bool         MWidget::testAttributes(WidgetAttributes a) const { return (m_attributes & a) != 0; }
    inline bool         MWidget::testWidgetState(unsigned int s)    const { return (m_widgetState & s) != 0; }
    inline void MWidget::move(long xpos, long ypos)
        { if(xpos != x || ypos != y) setGeometry(xpos,ypos,width,height); }
    inline void MWidget::move(MPoint pos)
        { if(pos.x != x || pos.y != y) setGeometry(pos.x,pos.y,width,height); }
    inline void MWidget::resize(long w, long h)
        { if(w != width || h != height) setGeometry(x,y,w,h); }
    inline void MWidget::resize(MSize size)
        { if(size.width() != width || size.height() != height) setGeometry(x,y,size.width(),size.height()); }
    inline void MWidget::setAttributes(WidgetAttributes attr, bool on)
        { on ? (m_attributes |= attr) : (m_attributes &= (~attr)); }
    inline void MWidget::setWidgetState(unsigned int s, bool on)
        { on ? (m_widgetState |= s) : (m_widgetState &= (~s)); }
}
