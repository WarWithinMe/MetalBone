QT -= core gui

TARGET = MetalBone
TEMPLATE = app
DEFINES -= QT_LARGEFILE_SUPPORT
DEFINES += _UNICODE UNICODE
win32-msvc*: DEFINES += MSVC
LIBS += -lGdi32 -luser32 -lvld -ld2d1 -lOle32 -L"D:/Program Files/Visual Leak Detector/lib/Win32"
INCLUDEPATH += "D:/Program Files/Visual Leak Detector/include"
QMAKE_INCDIR_QT =
QMAKE_LIBDIR_QT =
QMAKE_LIBS_QT_ENTRY -= -lqtmain
#QMAKE_LFLAGS_WINDOWS = -Wl,-subsystem,console
#QMAKE_CXXFLAGS += -finput-charset=GBK
#QMAKE_CXXFLAGS += -std=c++0x

HEADERS += mb_switch.h
HEADERS += MBGlobal.h
HEADERS += MResource.h
HEADERS += MStyleSheet.h
HEADERS += MEvent.h
HEADERS += MWidget.h
HEADERS += MApplication.h
HEADERS += externutils/XUnzip.h

HEADERS += test/test_widget.cpp
HEADERS += test/test_stylesheet.cpp
HEADERS += test/test_slot.cpp

SOURCES += testMain.cpp
SOURCES += MWidget.cpp
SOURCES += MBGlobal.cpp
SOURCES += MStyleSheet.cpp
SOURCES += externutils/XUnzip.cpp
