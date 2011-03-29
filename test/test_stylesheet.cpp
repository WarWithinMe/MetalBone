#include "MApplication.h"
#include "MStyleSheet.h"
#include "MResource.h"
#include "MWidget.h"
#include "stdio.h"
class PushButton: public MWidget{};
class LineEidt: public MWidget{};
class TextBrowser: public MWidget{};

void test_resource()
{
	MResource::addFileToCache(L"test.png");
	MResource::addFileToCache(L"test.txt");
	MResource::addZipToCache(L"test.zip");
	MResource::dumpCache();
	MResource::clearCache();
}

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
L"   background:#3b4a47 no-repeat repeat-x;"
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
L"   background:#112233 url(:/Wishc.png) no-repeat;}";
	MApplication app;
	mApp->setStyleSheet(css2);
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
	browser.show();
	mApp->getStyleSheet()->draw(&browser2,CSS::PC_Pressed);
	mApp->getStyleSheet()->dumpStyleSheet();
}

void testCSSBackgroundRenderObject()
{
	// Background: { Brush/Image Repeat Clip Alignment pos-x pos-y }*;
	std::wstring css =
L"#BackgroundWidget                                         {"
L"   background:#3b4a47 no-repeat border top center -5 -10 ;}"
L"#BackgroundWidget:pressed                                 {"
L"   background:url(testbg.png)                            ; "
L"   background:url(testbg2.png)                           ; "
L"   background-clip:border                                ; "
L"   background-alignment: top center                      ; "
L"   background-repeat: repeat-y                           ; "
L"   background-position: -5 18                            ;}"
L"#BackgroundWidget:hover                                   {"
L"   background:url(testbg.png) no-repeat border center -8 ;}"

L"#BackgroundInheritWidget                                   {"
L"   background:url(testbg.png) repeat-x                   ;}"
L"#BackgroundInheritChildWidget                              {"
L"   inherit-background:true                                ; "
L"   background:url(testbg2.png)                           ;}"

L"#BorderImageWidget                                        {"
L"   border-image:url(testbg.png) 10 20 stretch repeat     ;}"

L"#SimpleBorderWidget                                       {"
L"   border: 1px #567890 solid                             ;}"
L"#SimpleBorderWidget2                                      {"
L"   border: 1px                                           ; "
L"   border-width: 2px                                     ; "
L"   border-style: dashed                                  ; "
L"   border-color: #111111                                 ;}"

L"#RadiusBorderWidget                                       {"
L"   border: 1px                                           ; "
L"   border-width: 2px                                     ; "
L"   border-radius: 3px                                    ; "
L"   border-color: #111111                                 ;}"

L"#ComplexBorderWidget                                      {"
L"   border: 1px                                           ; "
L"   border-width: 2px  3px                                ; "
L"   border-radius: 3px                                    ; "
L"   border-color: #111111                                 ;}"
L"#ComplexBorderWidget2                                      {"
L"   border: 1px 5px                                       ; "
L"   border-color: #111111                                 ;}"
L"#ComplexBorderWidget3                                      {"
L"   border-color: #111111 #567890                         ;}"
L"#ComplexBorderWidget4                                      {"
L"   border-top-color: #111111                              ;}"
L"#ComplexBorderWidget5                                      {"
L"   border-radius: 2px 3px                                 ;}";

	MApplication app;
	mApp->setStyleSheet(css);
	MWidget bg;
	bg.setObjectName(L"BackgroundWidget");
	bg.show();
	mApp->getStyleSheet()->draw(&bg,CSS::PC_Default);
	mApp->getStyleSheet()->draw(&bg,CSS::PC_Pressed);
	mApp->getStyleSheet()->draw(&bg,CSS::PC_Hover);

	MWidget inheritBg;
	MWidget inheritBgChild;
	inheritBg.setObjectName(L"BackgroundInheritWidget");
	inheritBgChild.setObjectName(L"BackgroundInheritChildWidget");
	inheritBgChild.setParent(&inheritBg);
	bg.show();
	mApp->getStyleSheet()->draw(&inheritBgChild,CSS::PC_Default);

	MWidget borderImage;
	borderImage.setObjectName(L"BorderImageWidget");
	borderImage.show();
	mApp->getStyleSheet()->draw(&borderImage,CSS::PC_Default);

	MWidget simpleBorder;
	simpleBorder.setObjectName(L"SimpleBorderWidget");
	simpleBorder.show();
	mApp->getStyleSheet()->draw(&simpleBorder,CSS::PC_Default);
	MWidget simpleBorder2;
	simpleBorder2.setObjectName(L"SimpleBorderWidget2");
	simpleBorder2.show();
	mApp->getStyleSheet()->draw(&simpleBorder2,CSS::PC_Default);

	MWidget radiusBorder;
	radiusBorder.setObjectName(L"RadiusBorderWidget");
	radiusBorder.show();
	mApp->getStyleSheet()->draw(&radiusBorder,CSS::PC_Default);

	MWidget complexBorder;
	complexBorder.setObjectName(L"ComplexBorderWidget");
	complexBorder.show();
	mApp->getStyleSheet()->draw(&complexBorder,CSS::PC_Default);
	MWidget complexBorder2;
	complexBorder2.setObjectName(L"ComplexBorderWidget2");
	complexBorder2.show();
	mApp->getStyleSheet()->draw(&complexBorder2,CSS::PC_Default);
	MWidget complexBorder3;
	complexBorder3.setObjectName(L"ComplexBorderWidget3");
	complexBorder3.show();
	mApp->getStyleSheet()->draw(&complexBorder3,CSS::PC_Default);
	MWidget complexBorder4;
	complexBorder4.setObjectName(L"ComplexBorderWidget4");
	complexBorder4.show();
	mApp->getStyleSheet()->draw(&complexBorder4,CSS::PC_Default);
	MWidget complexBorder5;
	complexBorder5.setObjectName(L"ComplexBorderWidget5");
	complexBorder5.show();
	mApp->getStyleSheet()->draw(&complexBorder5,CSS::PC_Default);

//	mApp->getStyleSheet()->dumpStyleSheet();
}
