#include "MCommonWidgets.h"
#include "MStyleSheet.h"
#include "MGraphics.h"
#include "MResource.h"
#include "private/MApp_p.h"

namespace MetalBone
{
    using namespace std;
    void MCheckBox::setText(const std::wstring& text)
    {
        if(text == s_text) return;

        s_text = text;
        repaint();
    }

    unsigned int MCheckBox::getLastWidgetPseudo()
    {
        // If MStyleSheetStyle draws our checkbox the first time.
        // We return a value the same as getWidgetPseudo() to make
        // it not animate our widget.
        unsigned int last = MWidget::getLastWidgetPseudo();
        if(last == CSS::PC_Default) return getWidgetPseudo(true);
        last &= (~CSS::PC_Hover);
        last &= (~CSS::PC_Focus);
        return last;
    }

    unsigned int MCheckBox::getWidgetPseudo(bool markAsLast, unsigned int initP)
    { 
        initP |= b_isChecked ? CSS::PC_Checked : CSS::PC_Unchecked;
        return MWidget::getWidgetPseudo(markAsLast, initP);
    }
    void MCheckBox::mousePressEvent(MMouseEvent* e) { e->accept(); }
    void MCheckBox::mouseReleaseEvent(MMouseEvent* e)
    {
        if(e->getX() >= 0  && e->getX() <= width() &&
            e->getY() >= 0 && e->getY() <= height())
        {
            e->accept();
            setChecked(!b_isChecked);
        }
    }

    void MCheckBox::setChecked(bool checked)
    {
        if(b_isChecked == checked) return;

        b_isChecked = checked;
        toggled.Emit();

        updateSSAppearance();
    }

    void MCheckBox::doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT)
        { mApp->getStyleSheet()->draw(this, widgetRectInRT, clipRectInRT, s_text); }

    void MButton::setChecked(bool checked)
    {
        if(!b_checkable || checked == b_isChecked) return;

        b_isChecked = checked;
        toggled.Emit();
        updateSSAppearance();
    }

    void MButton::setText(const std::wstring& text)
    {
        if(text == s_text) return;

        s_text = text;
        repaint();
    }

    void MButton::mousePressEvent(MMouseEvent* e)
    {
        e->accept();
    }

    void MButton::mouseReleaseEvent(MMouseEvent* e)
    {
        if(e->getX() >= 0 && e->getX() <= width() &&
           e->getY() >= 0 && e->getY() <= height())
        {
            e->accept();
            clicked.Emit();
            setChecked(!b_isChecked);
        }
    }

    unsigned int MButton::getWidgetPseudo(bool markAsLast, unsigned int initP)
    { 
        initP |= b_isChecked ? CSS::PC_Checked : CSS::PC_Unchecked;
        return MWidget::getWidgetPseudo(markAsLast, initP);
    }
    void MButton::doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT)
        { mApp->getStyleSheet()->draw(this,widgetRectInRT,clipRectInRT,s_text); }


    MTabBar::MTabBar():tabQuerier(L"TabBar",L"Tab"),
        shownStartIndex(0),shownEndIndex(0),offset(0),totalWidth(0),checkedID(0)
    {
        setObjectName(L"TabBar");

        CSS::RenderRule rule = mApp->getStyleSheet()->getRenderRule(&tabQuerier);
        minTabWidth = rule.getGeometry(CSS::RenderRule::RRG_MinWidth);
        tabHeight   = rule.getGeometry(CSS::RenderRule::RRG_Height);
        if(minTabWidth == -1) minTabWidth = 60;
        if(tabHeight   == -1) tabHeight   = 57;
        setMinimumSize(0,        tabHeight);
        setMaximumSize(LONG_MAX, tabHeight);

        MRect r = rule.getContentMargin();
        leftMargin  = r.left;
        rightMargin = r.right;
        topMargin   = r.top;

        rightButton.setParent(this);
        rightButton.setObjectName(L"RightButton");
        rule = mApp->getStyleSheet()->getRenderRule(&rightButton);
        int rw = rule.getGeometry(CSS::RenderRule::RRG_Width);
        int rh = rule.getGeometry(CSS::RenderRule::RRG_Height);
        if(rw == -1) rw = 16;
        if(rh == -1) rh = 16;
        int rx = width() - rw;
        int ry = tabHeight - rh;

        leftButton.setParent(this);
        leftButton.setObjectName(L"LeftButton");
        rule = mApp->getStyleSheet()->getRenderRule(&leftButton);
        int lw = rule.getGeometry(CSS::RenderRule::RRG_Width);
        int lh = rule.getGeometry(CSS::RenderRule::RRG_Height);
        if(lw == -1) lw = 16;
        if(lh == -1) lh = 16;
        int ly = 0;
        int lx = 0;
        if(ry < lh)
        {
            ly = ry;
            lx = rx - lw;
        } else
        {
            lx = size().width() - lw;
            if(lx < rx) rx = lx;
            else        lx = rx;
        }

        rightButton.setGeometry(rx,ry,rw,rh);
        leftButton.setGeometry(lx,ly,lw,lh);
        leftButton.clicked.Connect(this,  &MTabBar::onLeftButton);
        rightButton.clicked.Connect(this, &MTabBar::onRightButton);

        slideTimer.setInterval(33);
        slideTimer.timeout.Connect(this, &MTabBar::onSlideTimer);
    }

    int MTabBar::addTab(const std::wstring& tabText, const std::wstring& imagePath)
    {
        static int sagID = 0;

        int id = sagID++;
        TabData& tabData = tabs[id];

        tabData.id   = id;
        tabData.text = tabText;
        if(!imagePath.empty())
            tabData.icon = MGraphicsResFactory::createImage(imagePath);

        CSS::RenderRule rule = mApp->getStyleSheet()->getRenderRule(&tabQuerier);
        MSize size = rule.getStringSize(tabText);
        int width  = minTabWidth - leftMargin - rightMargin;
        width = size.width() - width;
        width = width < 0 ? minTabWidth : width + minTabWidth;
        tabData.width = width;

        totalWidth += width;
        updateLayout();

        return id;
    }

    void MTabBar::removeTab(int id)
    {
        if(tabs.find(id) != tabs.end())
        {
            TabData& data = tabs[id];
            totalWidth -= data.width;
            tabs.erase(id);
            updateLayout();
        }
    }

    void MTabBar::resizeEvent(MResizeEvent* e)
    {
        int xDelta = e->getNewSize().width() - e->getOldSize().width();
        leftButton.move(leftButton.pos().offset(xDelta,0));
        rightButton.move(rightButton.pos().offset(xDelta,0));
        updateLayout();
    }

    void MTabBar::updateTabText(int id, const std::wstring& tabText)
    {
        if(tabs.find(id) != tabs.end())
        {
            TabData& tabData = tabs[id];

            CSS::RenderRule rule = mApp->getStyleSheet()->getRenderRule(&tabQuerier);
            MSize size = rule.getStringSize(tabText);
            int width = minTabWidth - leftMargin - rightMargin;
            width = size.width() - width;
            width = width < 0 ? minTabWidth : width + minTabWidth;
            totalWidth    += width;
            tabData.width =  width;
            tabData.text  =  tabText;

            updateLayout();
        }
    }

    void MTabBar::updateTabImage(int id, const std::wstring& imagePath)
    {
        if(tabs.find(id) != tabs.end())
        {
            TabData& tabData = tabs[id];
            if(!imagePath.empty())
                tabData.icon = MGraphicsResFactory::createImage(imagePath);
            else
                tabData.icon = MImageHandle();
            repaint();
        }
    }

    void MTabBar::onRightButton()
    {
        slideToIndex(shownEndIndex + 1);
    }

    void MTabBar::onLeftButton()
    {
        std::map<int, TabData>::reverse_iterator tit    = tabs.rbegin();
        std::map<int, TabData>::reverse_iterator titEnd = tabs.rend();

        int currentIndex = tabs.size() - 1;
        int width = 0;
        while(tit != titEnd)
        {
            if(currentIndex < shownStartIndex)
            {
                width += tit->second.width;
                if(width > leftButton.x())
                {
                    break;
                }
            }

            --currentIndex;
            if(currentIndex == 0)
                break;
            ++tit;
            continue;
        }

        slideToIndex(currentIndex);
    }

    void MTabBar::onSlideTimer()
    {
        setEnabled(false);
        slideFactor += slideToOffset > offset ? 5 : -5;
        if(abs(slideToOffset - offset) <= abs(slideFactor))
        {
            offset = slideToOffset;
            slideTimer.stop();
            setEnabled(true);
        } else
            offset += slideFactor;

        updateLayout();
    }

    void MTabBar::slideToIndex(int index)
    {
        if(shownStartIndex == index)          return;
        if(index >= (int)tabs.size() || index < 0) return;

        std::map<int, TabData>::iterator tit    = tabs.begin();
        std::map<int, TabData>::iterator titEnd = tabs.end();
        int currentIndex = 0;
        int left = 0;
        while(tit != titEnd && currentIndex < index)
        {
            left += tit->second.width;
            ++currentIndex;
            ++tit;
        }

        slideToOffset = -left;
        slideFactor = 0;
        slideTimer.start();
    }

    void MTabBar::updateLayout()
    {
        std::map<int, TabData>::iterator tit    = tabs.begin();
        std::map<int, TabData>::iterator titEnd = tabs.end();
        if(tit == titEnd)
        {
            leftButton.hide();
            rightButton.hide();
            repaint();
            return;
        }

        int tabLeft      = offset;
        int tabRight     = offset;
        int currentIndex = 0;

        while(tit != titEnd)
        {
            tabRight += tit->second.width;

            if(tabLeft <= 0)
            {
                if(tabRight > 0)
                    shownStartIndex = currentIndex;
            } else
            {
                if(tabRight < leftButton.x())
                    shownEndIndex = currentIndex;
                else
                    break;
            }

            tabLeft = tabRight;
            ++currentIndex;
            ++tit;
        }

        if(shownStartIndex > 0)
            leftButton.show();
        else
            leftButton.hide();
        if(shownEndIndex < (int)tabs.size() - 1)
            rightButton.show();
        else
            rightButton.hide();
        repaint();
    }

    void MTabBar::mousePressEvent(MMouseEvent* e)
    {
        e->accept();
        std::map<int, TabData>::iterator tit    = tabs.begin();
        std::map<int, TabData>::iterator titEnd = tabs.end();

        int updatedItem = 0;
        int tabLeft     = 0;
        int tabRight    = 0;
        int prevSelID   = checkedID;
        int index       = 0;

        while(tit != titEnd && updatedItem < 2 && index <= shownEndIndex)
        {
            if(index >= shownStartIndex)
            {
                tabRight += tit->second.width;
                if(tabLeft <= e->getX() && tabRight >= e->getX())
                {
                    // The item has been selected, we just have to return.
                    if(checkedID == tit->first)
                        return;

                    checkedID = tit->second.id;
                    tabSelected(checkedID);
                    repaint(tabLeft, 0, tabRight - tabLeft, tabHeight);

                    ++updatedItem;
                } else if(tit->first == prevSelID)
                {
                    // update the previous checked one.
                    repaint(tabLeft, 0, tabRight - tabLeft, tabHeight);
                    ++updatedItem;
                }
                tabLeft = tabRight;
            }

            ++tit;
            ++index;
        }
    }

    void MTabBar::selectTab(int id)
    {
        if(checkedID == id) return;

        checkedID = id;
        tabSelected(id);

        std::map<int, TabData>::iterator tit    = tabs.begin();
        std::map<int, TabData>::iterator titEnd = tabs.end();

        int index = 0;
        int groupIndex = 0;
        int right = 0;
        while(tit != titEnd)
        {
            right += tit->second.width;
            if(right > leftButton.x())
            {
                groupIndex = index;
                right = 0;
            }

            if(tit->second.id == id)
                break;

            ++index;
            ++tit;
        }
        slideToIndex(groupIndex);
    }

    void MTabBar::doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT)
    {
        MGraphics graphics(this);
        MRect tabRect;
        tabRect.top    = widgetRectInRT.top;
        tabRect.bottom = widgetRectInRT.top  + tabHeight;
        tabRect.left   = widgetRectInRT.left + offset;
        int index = 0;

        bool prevDrawn = false;

        mApp->getStyleSheet()->draw(this, widgetRectInRT, clipRectInRT);

        std::map<int, TabData>::iterator tit    = tabs.begin();
        std::map<int, TabData>::iterator titEnd = tabs.end();
        while(tit != titEnd && index <= shownEndIndex)
        {
            tabRect.right = tabRect.left + tit->second.width;
            if(index >= shownStartIndex)
            {
                if(tabRect.intersectsRect(clipRectInRT))
                {
                    CSS::RenderRule rule = 
                        mApp->getStyleSheet()->getRenderRule(&tabQuerier,
                        tit->first == checkedID ? CSS::PC_Checked : CSS::PC_Default);
                    rule.draw(graphics,tabRect,clipRectInRT,tit->second.text,CSS::QuickOpaque);

                    MSize iconSize = tit->second.icon.getSize();
                    int iconX = tabRect.left + (tit->second.width - iconSize.width()) / 2;

                    graphics.drawImage(tit->second.icon, MRect(iconX,
                        tabRect.top + topMargin,
                        iconX + iconSize.width(), 
                        tabRect.top + topMargin + iconSize.height()));

                    prevDrawn = true;
                } else if(prevDrawn) {
                    // If the previous item is drawn, then we can assume the rest 
                    // won't be drawn.
                    return;
                }
            }

            tabRect.left = tabRect.right;

            ++tit;
            ++index;
        }
    }

    MScrollBar::MScrollBar(Orientation or):
    thumbQuerier(L"MScrollBar",L"Thumb"),
        e_orientation(or),
        n_min(0), n_max(0), n_value(0),
        n_pageStep(10), n_singleStep(1),
        n_trackStartPos(0), n_trackEndPos(0),
        n_thumbStartPos(0), n_thumbEndPos(0),
        n_mousePos(-1),
        b_tracking(true), b_thumbHover(false)
    {
        setObjectName(L"MScrollBar");
        setAttributes(WA_TrackMouseMove);

        subButton.setObjectName(L"SubButton");
        addButton.setObjectName(L"AddButton");

        subButton.setParent(this);
        addButton.setParent(this);

        subButton.clicked.Connect(this, &MScrollBar::onSubButton);
        addButton.clicked.Connect(this, &MScrollBar::onAddButton);

        showButtons(true);
    }

    void MScrollBar::ensurePolished()
    {
        MWidget::ensurePolished();

        subButton.ensurePolished();
        addButton.ensurePolished();

        if(e_orientation == Vertical)
        {
            CSS::RenderRule rule = mApp->getStyleSheet()->getRenderRule(this, CSS::PC_Vertical);
            int marginTop = margin.top;
            int marginBottom = margin.bottom;
            margin = rule.getContentMargin();

            n_trackStartPos += margin.top - marginTop;
            n_trackEndPos   -= margin.bottom - marginBottom;
        } else
        {
            CSS::RenderRule rule = mApp->getStyleSheet()->getRenderRule(this, CSS::PC_Horizontal);
            int marginLeft = margin.left;
            int marginRight = margin.right;
            margin = rule.getContentMargin();

            n_trackStartPos += margin.left - marginLeft;
            n_trackEndPos   -= margin.right - marginRight;
        }
        
    };

    void MScrollBar::showButtons(bool s)
    { 
        if(addButton.isHidden() != s) return;
        if(s)
        {
            addButton.show();
            subButton.show();

            if(e_orientation == Vertical)
            {
                if(subButton.y() == 0) {
                    n_trackStartPos = subButton.height() + margin.top;
                    n_trackEndPos   = addButton.y() - margin.bottom;
                } else {
                    n_trackStartPos = margin.top;
                    n_trackEndPos   = subButton.y() - margin.bottom;
                }

            } else {
                if(subButton.x() == 0)
                {
                    n_trackStartPos = subButton.width() + margin.left;
                    n_trackEndPos   = addButton.x() - margin.right;
                } else
                {
                    n_trackStartPos = margin.left;
                    n_trackEndPos   = subButton.x() - margin.right;
                }
            }
        } else
        {
            addButton.hide();
            subButton.hide();

            if(e_orientation == Vertical)
            {
                n_trackStartPos = margin.top;
                n_trackEndPos   = height() - margin.bottom;
            } else
            {
                n_trackStartPos = margin.left;
                n_trackEndPos   = width() - margin.right;
            }
        }

        n_thumbEndPos = n_thumbStartPos + calcThumbSize(n_trackStartPos - n_trackEndPos);
    }

    void MScrollBar::resizeEvent(MResizeEvent* e)
    {
        MSize newSize = e->getNewSize();
        MSize oldSize = e->getOldSize();
        int   xDelta  = newSize.getWidth() - oldSize.getWidth();
        int   yDelta  = newSize.getHeight() - oldSize.getHeight();

        if(e_orientation == Vertical)
        {
            int contentY = subButton.y();
            // Mac-style SubButton
            if(subButton.y() != 0) contentY += yDelta;
            subButton.setGeometry(0, contentY, newSize.getWidth(), subButton.height());

            contentY = addButton.y() + yDelta;
            addButton.setGeometry(0, contentY, newSize.getWidth(), addButton.height());

            n_trackEndPos += yDelta;
        } else
        {
            int contentX      = subButton.x();

            if(subButton.x() != 0) contentX += xDelta;
            subButton.setGeometry(contentX, 0, subButton.width(), newSize.height());

            contentX = addButton.x() + xDelta;
            addButton.setGeometry(contentX, 0, addButton.width(), newSize.height());

            n_trackEndPos += xDelta;
        }

        n_thumbEndPos = n_thumbStartPos + calcThumbSize(n_trackEndPos - n_trackStartPos);
    }

    void MScrollBar::setSingleStep(int s)
    {
        if(n_singleStep == s) return;

        n_singleStep = s;
        n_thumbEndPos = n_thumbStartPos + calcThumbSize(n_trackEndPos - n_trackStartPos);

        if(e_orientation == Vertical)
            repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
        else
            repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
    }

    int MScrollBar::calcThumbSize(int sliderSize)
    {
        int range = maximum() - minimum();
        int neededSize = range / singleStep();
        return sliderSize - neededSize;
    }

    void MScrollBar::onSubButton() { setValue(n_value - n_singleStep); }
    void MScrollBar::onAddButton() { setValue(n_value + n_singleStep); }

    void MScrollBar::setRange(int min, int max)
    {
        if(n_min != min || n_max != max)
        {
            n_min = min;
            n_max = max;
            if(n_value < min)
            {
                n_value = min;
                valueChanged(min);
            }else if(n_value > max)
            {
                n_value = max;
                valueChanged(max);
            }

            n_thumbStartPos = n_value / n_singleStep;
            n_thumbEndPos   = n_thumbStartPos + calcThumbSize(n_trackEndPos - n_trackStartPos);

            if(e_orientation == Vertical)
                repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
            else
                repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
        }
    }

    void MScrollBar::setValue(int value, bool emitSignal)
    {
        if(n_value == value) return;

        if(value < n_min) value = n_min;
        else if(value > n_max) value = n_max;

        if(n_value == value) return;
        n_value = value;

        int oldThumbStart = n_thumbStartPos;
        n_thumbStartPos = n_value / n_singleStep;
        n_thumbEndPos   += (n_thumbStartPos - oldThumbStart);

        if(emitSignal) valueChanged(n_value);

        if(e_orientation == Vertical)
            repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
        else
            repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
    }

    void MScrollBar::leaveEvent()
    {
        if(n_mousePos == -1 && b_thumbHover == true)
        {
            b_thumbHover = false;
            if(e_orientation == Vertical)
                repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
            else
                repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
        }
    }

    void MScrollBar::mousePressEvent(MMouseEvent* e)
    {
        e->accept();
        int pos = e_orientation == Vertical ? e->getY() : e->getX();

        if(pos >= n_thumbStartPos && pos <= n_thumbEndPos)
        {
            n_mousePos = pos;

            if(e_orientation == Vertical)
                repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
            else
                repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
        } else if(pos < n_thumbStartPos)
        {
            setValue(n_value - n_pageStep);
            n_mousePos = -1;
        } else
        {
            setValue(n_value + n_pageStep);
            n_mousePos = -1;
        }
    }

    void MScrollBar::mouseMoveEvent(MMouseEvent* e)
    {
        e->accept();

        if(n_mousePos == -1)
        {
            if(e_orientation == Vertical)
            {
                bool oldHover = b_thumbHover;

                if(e->getX() >= margin.left &&
                    e->getX() <= width() - margin.right &&
                    e->getY() >= n_thumbStartPos &&
                    e->getY() <= n_thumbEndPos)
                { b_thumbHover = true; }
                else { b_thumbHover = false; }

                if(oldHover != b_thumbHover)
                    repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
            } else
            {
                bool oldHover = b_thumbHover;

                if(e->getY() >= margin.top &&
                    e->getY() <= height() - margin.bottom &&
                    e->getX() >= n_thumbStartPos &&
                    e->getX() <= n_thumbEndPos)
                { b_thumbHover = true; }
                else { b_thumbHover = false; }

                if(oldHover != b_thumbHover)
                    repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
            }
        } else
        {
            int newPos = (e_orientation == Vertical) ? e->getY() : e->getX();
            int delta  = newPos - n_mousePos;

            if(delta == 0) return;
            n_mousePos = newPos;

            if(n_thumbStartPos + delta < n_trackStartPos || n_thumbEndPos + delta > n_trackEndPos)
                return;

            n_thumbStartPos += delta;
            n_thumbEndPos   += delta;

            int oldValue = n_value;
            n_value = n_thumbStartPos / n_singleStep;
            if(n_value != oldValue && b_tracking)
                valueChanged(n_value);

            if(e_orientation == Vertical)
                repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
            else
                repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
        }
    }

    void MScrollBar::mouseReleaseEvent(MMouseEvent* e)
    {
        if(n_mousePos != -1)
        {
            n_mousePos = -1;
            e->accept();
            if(!b_tracking) valueChanged(n_value);

            if(e_orientation == Vertical)
                repaint(0,n_trackStartPos,width(),n_trackEndPos - n_trackStartPos);
            else
                repaint(n_trackStartPos,0,n_trackEndPos - n_trackStartPos,height());
        }
    }

    unsigned int MScrollBar::getWidgetPseudo(bool markAsLast, unsigned int initPseudo)
    {
        unsigned int defaults = MWidget::getWidgetPseudo(markAsLast,initPseudo);
        defaults |= (e_orientation == Vertical ? CSS::PC_Vertical : CSS::PC_Horizontal);
        return defaults;
    }

    void MScrollBar::doStyleSheetDraw(const MRect& widgetRectInRT, const MRect& clipRectInRT)
    {
        mApp->getStyleSheet()->draw(this,widgetRectInRT,clipRectInRT);

        unsigned int pesudo = n_mousePos == -1 ? 0 : CSS::PC_Pressed;
        if(b_thumbHover) pesudo |= CSS::PC_Hover;
        pesudo |= e_orientation == Vertical ? CSS::PC_Vertical : CSS::PC_Horizontal;

        CSS::RenderRule rule = mApp->getStyleSheet()->getRenderRule(&thumbQuerier, pesudo);
        MRect thumbRectInRT = widgetRectInRT;
        if(e_orientation == Vertical)
        {
            thumbRectInRT.left   += margin.left;
            thumbRectInRT.right  -= margin.right;
            thumbRectInRT.bottom = thumbRectInRT.top + n_thumbEndPos;
            thumbRectInRT.top    += n_thumbStartPos;
        } else
        {
            thumbRectInRT.top    += margin.top;
            thumbRectInRT.bottom -= margin.right;
            thumbRectInRT.right  =  thumbRectInRT.left + n_thumbEndPos;
            thumbRectInRT.left   += n_thumbStartPos;
        }

        MGraphics graphics(this);
        rule.draw(graphics, thumbRectInRT, clipRectInRT);
    }
}