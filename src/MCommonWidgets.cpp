#include "MCommonWidgets.h"
#include "MStyleSheet.h"

namespace MetalBone
{
	using namespace std;
	void MButton::setChecked(bool checked)
	{
		if(!b_checkable) return;

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

	void MButton::mouseReleaseEvent(MMouseEvent* e)
	{
		e->accept();
		clicked.Emit();
		setChecked(!b_isChecked);
	}

	unsigned int MButton::getWidgetPseudo(bool markAsLast, unsigned int initP)
		{ return MWidget::getWidgetPseudo(markAsLast, b_isChecked ? CSS::PC_Checked : 0); }
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
        int rx = size().width() - rw;
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
            tabData.icon = MD2DPaintContext::createImage(imagePath);

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
            MD2DPaintContext::removeResource(tabData.icon);
            if(!imagePath.empty())
                tabData.icon = MD2DPaintContext::createImage(imagePath);
            else
                tabData.icon = D2DImageHandle();
            repaint();
        }
    }

    void MTabBar::onLeftButton()
    {
        slideToIndex(shownEndIndex + 1);
    }

    void MTabBar::onRightButton()
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
                if(width > leftButton.pos().x)
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
        if(index >= tabs.size() || index < 0) return;

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
                if(tabRight < leftButton.pos().x)
                    shownEndIndex = currentIndex;
                else
                    break;
            }

            tabLeft = tabRight;
            ++currentIndex;
            ++tit;
        }

        if(shownStartIndex > 0)
            rightButton.show();
        else
            rightButton.hide();
        if(shownEndIndex < tabs.size() - 1)
            leftButton.show();
        else
            leftButton.hide();
        repaint();
    }

    void MTabBar::mousePressEvent(MMouseEvent* e)
    {
        int clickX = e->getX();

        std::map<int, TabData>::iterator tit    = tabs.begin();
        std::map<int, TabData>::iterator titEnd = tabs.end();

        int currentIndex = 0;
        int tabLeft  = 0;
        int tabRight = 0;
        while(tit != titEnd)
        {
            if(currentIndex >= shownStartIndex)
            {
                tabRight += tit->second.width;
                if(tabLeft <= clickX && tabRight >= clickX)
                {
                    checkedID = tit->second.id;
                    tabSelected(checkedID);
                    repaint();
                    return;
                }
                tabLeft = tabRight;
            }

            ++currentIndex;
            ++tit;
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
            if(right > leftButton.pos().x)
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
        MD2DPaintContext context(this);
        MRect tabRect;
        tabRect.top    = widgetRectInRT.top;
        tabRect.bottom = widgetRectInRT.top  + tabHeight;
        tabRect.left   = widgetRectInRT.left + offset;
        int index = 0;

        mApp->getStyleSheet()->draw(this,widgetRectInRT,clipRectInRT);

        std::map<int, TabData>::iterator tit    = tabs.begin();
        std::map<int, TabData>::iterator titEnd = tabs.end();
        while(tit != titEnd && index <= shownEndIndex)
        {
            tabRect.right = tabRect.left + tit->second.width;
            if(index >= shownStartIndex)
            {
                CSS::RenderRule rule = 
                    mApp->getStyleSheet()->getRenderRule(&tabQuerier,
                    tit->first == checkedID ? CSS::PC_Checked : CSS::PC_Default);
                rule.draw(context,tabRect,clipRectInRT,tit->second.text);

                ID2D1Bitmap* iconBmp = tit->second.icon;
                D2D1_SIZE_U iconSize = iconBmp->GetPixelSize();

                int iconX = tabRect.left + (tit->second.width - iconSize.width) / 2;
                context.getRenderTarget()->DrawBitmap(iconBmp,
                    D2D1::RectF((FLOAT)iconX,(FLOAT)tabRect.top + topMargin,
                    (FLOAT)iconX + iconSize.width,
                    (FLOAT)tabRect.top + topMargin + iconSize.height));
            }
            tabRect.left = tabRect.right;

            ++tit;
            ++index;
        }
    }
}