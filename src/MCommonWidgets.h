#pragma once

#include "MWidget.h"
#include "MD2DPaintContext.h"
#include "MResource.h"

// MButton
// MLabel
// MTabBar
// MScrollBar

namespace MetalBone
{
    class METALBONE_EXPORT MButton : public MWidget
    {
        public:
            inline MButton(MWidget* parent = 0);

            inline void setCheckable(bool checkable);
            inline bool isCheckbale() const;
            inline bool isChecked()   const;
            void        setChecked(bool checked);

            void setText(const std::wstring& text);
            inline const std::wstring& text() const;

            Signal0<> clicked;
            // toggled is emitted whenever the check state of
            // the button changes.
            Signal0<> toggled;

            virtual unsigned int getWidgetPseudo(bool markAsLast = false, unsigned int initP = 0);

        protected:
            virtual void mousePressEvent(MMouseEvent*);
            virtual void mouseReleaseEvent(MMouseEvent*);
            virtual void doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT);

        private:
            std::wstring s_text;
            bool         b_checkable;
            bool         b_isChecked;
    };

    class METALBONE_EXPORT MLabel : public MWidget
    {
        public:
            void setText(const std::wstring& t)
            {
                if(text == t) return;
                text = t;
                repaint();
            }
        protected:
            void doStyleSheetDraw(const MRect& wr, const MRect& cr)
                { mApp->getStyleSheet()->draw(this,wr,cr,text); }
        private:
            std::wstring text;
    };

    class METALBONE_EXPORT MTabBar : public MWidget
    {
        public:
            MTabBar();

            int  addTab        (const std::wstring& text, const std::wstring& imagePath = std::wstring());
            void removeTab     (int id);
            void selectTab     (int id);
            void updateTabText (int id, const std::wstring& text);
            void updateTabImage(int id, const std::wstring& imagePath);

            Signal1<int> tabSelected;

        protected:
            void resizeEvent(MResizeEvent*);
            void mousePressEvent(MMouseEvent*);
            void doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT);

        private:
            struct TabData
            {
                std::wstring   text;
                D2DImageHandle icon;
                int            id;
                int            width;
            };

            std::map<int, TabData> tabs;
            CSS::RenderRuleQuerier tabQuerier;

            MButton leftButton;
            MButton rightButton;

            MTimer slideTimer;

            int minTabWidth;
            int tabHeight;
            int topMargin;
            int leftMargin;
            int rightMargin;

            int totalWidth;
            int shownStartIndex;
            int shownEndIndex;
            int slideToOffset;
            int offset;
            int slideFactor;

            int checkedID;

            void updateLayout();
            void slideToIndex(int index);
            void onSlideTimer();
            void onLeftButton();
            void onRightButton();
    };

    class METALBONE_EXPORT MScrollBar : public MWidget
    {
        public:
            enum Orientation
            {
                Vertical,
                Horizontal
            };

            MScrollBar(Orientation or = Vertical);
            ~MScrollBar(){}

            inline bool hasTracking() const;
            inline int  maximum()     const;
            inline int  minimum()     const;
            inline int  pageStep()    const;
            inline int  singleStep()  const;
            inline int  value()       const;

            inline Orientation orientation() const;

            // When tracking is true, valueChanged will emit whenever
            // the user moves the mouse. Otherwise, valueChanged will emit
            // when the user release the mouse.
            inline void setTracking(bool);
            inline void setPageStep(int);

            void setRange(int min, int max);
            void showButtons(bool);
            void setSingleStep(int);
            void setValue(int value, bool emitSignal = true);

            Signal1<int> valueChanged;

            void ensurePolished();

        protected:
            void mousePressEvent(MMouseEvent*);
            void mouseMoveEvent(MMouseEvent*);
            void mouseReleaseEvent(MMouseEvent*);
            void resizeEvent(MResizeEvent*);
            void doStyleSheetDraw(const MRect&, const MRect&);
            void leaveEvent();
            unsigned int getWidgetPseudo(bool markAsLast = false, unsigned int initPseudo = 0);

        private:
            class SliderButton : public MWidget
            {
                public:
                    Signal0<> clicked;
                    virtual unsigned int getWidgetPseudo(bool markAsLast = false, unsigned int initP = 0)
                    {
                        unsigned int direction = ((MScrollBar*)parent())->orientation() == Vertical ? 
                            CSS::PC_Vertical : CSS::PC_Horizontal;
                        return MWidget::getWidgetPseudo(markAsLast, direction);
                    }
            };
            CSS::RenderRuleQuerier thumbQuerier;
            SliderButton addButton;
            SliderButton subButton;
            Orientation e_orientation;
            int  n_min;
            int  n_max;
            int  n_value;
            int  n_pageStep;
            int  n_singleStep;
            int  n_mousePos;

            int  n_trackStartPos;
            int  n_trackEndPos;
            int  n_thumbStartPos;
            int  n_thumbEndPos;
            MRect margin;

            bool b_tracking;
            bool b_thumbHover;

            void onAddButton();
            void onSubButton();
            int  calcThumbSize(int sliderSize);
    };

    inline bool MScrollBar::hasTracking() const { return b_tracking; }
    inline int  MScrollBar::maximum()     const { return n_max; }
    inline int  MScrollBar::minimum()     const { return n_min; }
    inline int  MScrollBar::pageStep()    const { return n_pageStep; }
    inline int  MScrollBar::singleStep()  const { return n_singleStep; }
    inline int  MScrollBar::value()       const { return n_value; }
    inline void MScrollBar::setTracking(bool t) { b_tracking = t; }
    inline void MScrollBar::setPageStep(int p)  { n_pageStep = p; }
    inline MScrollBar::Orientation MScrollBar::orientation() const
        { return e_orientation; }

    inline MButton::MButton(MWidget* parent):MWidget(parent),b_checkable(false),b_isChecked(false){}
    inline bool MButton::isCheckbale() const { return b_checkable; }
    inline bool MButton::isChecked()   const { return b_isChecked; }
    inline const std::wstring& MButton::text() const { return s_text; }
    inline void MButton::setCheckable(bool checkable) { b_checkable = checkable; }
}