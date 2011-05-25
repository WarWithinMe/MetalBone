#pragma once

#include "MWidget.h"

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

    inline MButton::MButton(MWidget* parent):MWidget(parent),b_checkable(false),b_isChecked(false){}
    inline bool MButton::isCheckbale() const { return b_checkable; }
    inline bool MButton::isChecked()   const { return b_isChecked; }
    inline const std::wstring& MButton::text() const { return s_text; }
    inline void MButton::setCheckable(bool checkable) { b_checkable = checkable; }
}