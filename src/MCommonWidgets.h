#pragma once

#include "MWidget.h"
#include "MD2DPaintContext.h"
#include "MResource.h"

// MButton
// MLabel
// MTabBar

namespace MetalBone
{
    class MButton : public MWidget
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
            virtual void mouseReleaseEvent(MMouseEvent*);
            virtual void doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT);

        private:
            std::wstring s_text;
            bool         b_checkable;
            bool         b_isChecked;
    };

    class MLabel : public MWidget
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

    class MTabBar : public MWidget
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


    inline MButton::MButton(MWidget* parent):MWidget(parent),b_checkable(false),b_isChecked(false){}
    inline bool MButton::isCheckbale() const { return b_checkable; }
    inline bool MButton::isChecked()   const { return b_isChecked; }
    inline const std::wstring& MButton::text() const { return s_text; }
    inline void MButton::setCheckable(bool checkable) { b_checkable = checkable; }
}