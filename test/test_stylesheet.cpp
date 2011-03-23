#include "MStyleSheet.h"
#include "MWidget.h"
#include "MApplication.h"
#include "stdio.h"
class PushButton: public MWidget{};
class LineEidt: public MWidget{};
class TextBrowser: public MWidget{};

void testCSS()
{
	// Test Parsing
//	std::string css =
//			"ClassA :active:checked{}\n"
//			"ClassB #FFFF :checked:default{backg/*test comment*/round : url(:/MyWebsite.png)/*test comment*/ }\n"
//			"./*Comments*/ClassC ClassD/*Comments*/ :disabled:edit-focus{border :value1;}"
//			"*:first:enabled{text-align: /*test comment*/value2;}:focus:hover:vertical{color:value3; height:value3}\n"
//			"ClassE >Clas/*Comments*/sF{text-decoration:value4;}"
//			" ClassG> ClassF#MyObject/*Comments*/ ,ClassG> ClassD#MyObject2:pressed{font:value5;}"
//			"ClassX ClassY, ClassZ {width:value6;}";
	std::wstring css2 =
L"#DoumailWidget #mailTreeView:pressed    {"
L"   background:#3b4a47;                   "
L"   border-left:   1px solid #122420;     "
L"   border-right:  1px solid #122420;     "
L"   border-bottom: 1px solid #122420;     "
L"   height: 20;                          }"
L"#RecommendSubViewer > PushButton        {"
L"   background:#d1d1d1;                   "
L"   border: 1px solid #6e6e6e;            "
L"   border-radius: 8px;                  }"
L"#RecommendSubViewer #wishBtn:checked    {"
L"   color:#FFFFFF;                        "
L"   background-image: url(:/Wishc.png);  }"
L"#RecommendSubViewer #privateBtn         {"
L"   border:none;                          "
L"   border-radius:0;                      "
L"   background:transparent;               "
L"   background-image: url(:/Tick2.png);  }"
L"#RecommendSubViewer #privateBtn:checked {"
L"   background-image: url(:/Tick.png);   }"
L"#RecommendSubViewer LineEdit            {"
L"   background-color: #d9d9d9;            "
L"   border: 1px solid #525252;           }"
L"#RecommendSubViewer TextEdit            {"
L"   background-color: #d9d9d9;            "
L"   border: 1px solid #525252;           }"
L"#EventViewer                            {"
L"   background-image: url(:/EV_BG.jpg);  }"
L"#EventViewer #titleLabel                {"
L"   font:18pt;                            "
L"   color: white;                        }"
L"#EventViewer #iconLabel                 {"
L"   border-image: url(:/Rb.png) 4;        "
L"   border: 4;                           }"
L"#EventViewer TextBrowser                {"
L"   background:transparent;              }"
L"#EventViewer PushButton                 {" // 1
L"   background:#d1d1d1;                   "
L"   border: 1px solid #6e6e6e;            "
L"   border-radius: 8px;                  }"
L"#EventViewer PushButton:hover           {" // 2
L"   background:#FFFFFF;                  }"
L"#EventViewer PushButton:pressed         {" // 3
L"   color:#FFFFFF;                        "
L"   background:#121212;                  }"
L"TextBroswer PushButton:pressed          {" // 4
L"   color:#556677;                        "
L"   background:#112233;                  }";

	MetalBone::MStyleSheetStyle sss;
	sss.setAppSS(css2);

	MApplication app;
	MWidget widget;
	widget.setObjectName(L"RecommendSubViewer");
	PushButton btn;
	btn.setObjectName(L"wishBtn");
	btn.setParent(&widget);
	TextBrowser browser;
	browser.setObjectName(L"EventViewer");

	// TextBrowser#EventViewer PushButton
	// Match 1 2 3
	PushButton browser2;
	browser2.setParent(&browser);

	sss.getStyleObject(&browser2,0);
	sss.getStyleObject(&browser2,CSS::PC_Pressed);
	sss.getStyleObject(&browser2,CSS::PC_Default);
	sss.dumpStyleSheet();
}
