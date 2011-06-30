#pragma once

#include "MBGlobal.h"
#include <string>
#include <windows.h>

#define mApp MetalBone::MApplication::instance()

#ifdef MB_USE_D2D
struct IDWriteFactory;
struct IWICImagingFactory;
struct ID2D1Factory;
#endif

namespace MetalBone
{
    class MWidget;
    class MStyleSheetStyle;
    struct MApplicationData;

    class METALBONE_EXPORT MApplication
    {
        public:
            enum GraphicsBackend
            {
                GDI,
                Direct2D,
                Skia
                // We could also add AGG, Cario...
            };

            // hardwareAccelerated is ignore if the backend is not Direct2D.
            MApplication(GraphicsBackend backend = Direct2D, bool hardwareAccelerated = true);
            virtual ~MApplication();

            inline static MApplication* instance();

            // Enter Main Event Loop
            int exec();
            // To exit the application.
            inline void exit(int returnCode);

            // If true (by default), when the last window is closed, it will call exit(0);
            void setQuitOnLastWindowClosed(bool);

            // Get the HINSTANCE of this app.
            HINSTANCE getAppHandle() const;

            // Set a custom Window Procedure. If the custom winproc doesn't
            // handle the message, it should return false.
            typedef bool (*WinProc)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
            void setCustomWindowProc(WinProc);

            const std::set<MWidget*>& topLevelWindows() const;

            inline bool isHardwareAccerated() const;
            inline GraphicsBackend getGraphicsBackend() const;

            // ===== Supports : 
            // .ClassA       // Matches widgets whose type is ClassA
            // ClassB       // The same as .ClassA
            // #MyClass       // Matches widgets whose objectName is MyClass
            // .ClassA .ClassB  // Mathces widgets whose tye is ClassB and one of its parent is ClassA widget.
            // .ClassA >.ClassB // Matches widgets whose type is ClassB 
                                // and has a ClassA widget as its parent.
            // ===== Do not support : 
            // .ClassA.ClassB  // Widget has only one type 
            // .ClassA#AAA#BBB // Widget has only one objectName(ID)

            // ===== Only support 'px' unit. If a unit is not 'px', it's considered to be 'pt'
            // ===== Do not support Color Name 
            void setStyleSheet(const std::wstring& css);
            // Load StyleSheet from a text file, if isAscii is true,
            // it will be parsed as Ascii Text File. Otherwise,
            // it will be parsed as UTF-8 Text File.
            void loadStyleSheetFromFile(const std::wstring& path, bool isAscii = true);
            MStyleSheetStyle* getStyleSheet();

#ifdef MB_USE_D2D
            ID2D1Factory*       getD2D1Factory();
            IDWriteFactory*     getDWriteFactory();
            IWICImagingFactory* getWICImagingFactory();
#endif

            inline unsigned int winDpi() const;

            // When MApplication quits the Message Loop. This signal is emitted.
            Signal0<> aboutToQuit;

        protected:
            // Override this function to register a custom Window Class
            virtual void setupRegisterClass(WNDCLASSW&);

        private:
            static MApplication* s_instance;
            MApplicationData*    mImpl;
            unsigned int         windowsDPI;
            GraphicsBackend      graphicsBackend;
            bool                 hardwareAccelerated;

            friend class MWidget;
    };





    inline MApplication* MApplication::instance()     { return s_instance; }
    inline void          MApplication::exit(int ret)  { ::PostQuitMessage(ret); }
    inline unsigned int  MApplication::winDpi() const { return windowsDPI; }
    inline bool          MApplication::isHardwareAccerated() const { return hardwareAccelerated; }
    inline MApplication::GraphicsBackend MApplication::getGraphicsBackend() const { return graphicsBackend; }
}

