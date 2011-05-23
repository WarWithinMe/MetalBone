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
	void MButton::doStyleSheetDraw(ID2D1RenderTarget* rt, const MRect& widgetRectInRT, const MRect& clipRectInRT)
		{ mApp->getStyleSheet()->draw(this,rt,widgetRectInRT,clipRectInRT,s_text); }
}