#include "MToolTip.h"
#include "MStyleSheet.h"
#include "MApplication.h"
namespace MetalBone
{
// 	bool MToolTip::bInited = false;
	void MToolTip::setText(const std::wstring& text)
	{
		if(tipString.empty())
		{
			style = mApp->getStyleSheet()->hasRenderRuleForClass((void*)this) ?
				CustomText : SystemText;
		}

		tipString = text; 
	}
}