#pragma once

#include "MBGlobal.h"
#include "MUtils.h"
#include "MWidget.h"
#include "private/MWidget_p.h"

#include <d2d1.h>
#include <string>
#include <unordered_map>

using namespace std;
using namespace std::tr1;

namespace MetalBone
{
    ID2D1Bitmap*      createD2DBitmap     (const wstring& imagePath, ID2D1RenderTarget* renderTarget, unsigned int* frameCount = 0);
    ID2D1BitmapBrush* createD2DBitmapBrush(const wstring& imagePath, ID2D1RenderTarget* renderTarget, unsigned int* frameCount = 0);

    class D2DBrushHandle
    {
        public:
            enum Type { 
                Unknown, Solid, Gradient, Bitmap, NinePatch
            };
            enum Portion {
                TopCenter = 0,
                CenterLeft   ,
                Center       ,
                CenterRight  ,
                BottomCenter ,
                Conner
            };

            inline D2DBrushHandle():etype(Unknown),pdata(0){}
            inline D2DBrushHandle(const D2DBrushHandle& rhs):etype(rhs.etype),pdata(rhs.pdata){}
            inline const D2DBrushHandle& operator=(const D2DBrushHandle& rhs) { etype = rhs.etype; pdata = rhs.pdata; return *this; }

            inline operator ID2D1Brush*() const;
            inline ID2D1Brush* getBrush() const;
            ID2D1BitmapBrush*  getPortion(Portion) const;

            unsigned int frameCount() const;

            inline Type type() const { return etype; }

        protected:
            inline D2DBrushHandle(Type t, ID2D1Brush** p):etype(t),pdata(p){}
            Type  etype;
            ID2D1Brush** pdata;

        friend class D2DResPool;
        friend class MD2DPaintContextData;
    };

    class D2DImageHandle
    {
        public:
            inline D2DImageHandle():pdata(0){}
            inline D2DImageHandle(const D2DImageHandle& rhs):pdata(rhs.pdata){}
            inline const D2DImageHandle& operator=(const D2DImageHandle& rhs){ pdata = rhs.pdata; }

            inline operator ID2D1Bitmap*() const;
            inline ID2D1Bitmap* getImage() const;

            unsigned int frameCount() const;

        protected:
            inline D2DImageHandle(ID2D1Bitmap** p):pdata(p){}
            ID2D1Bitmap** pdata;

        friend class D2DResPool;
    };

    struct RectU
    {
        unsigned int left;
        unsigned int top;
        unsigned int right;
        unsigned int bottom;

        RectU():left(0),top(0),right(0),bottom(0){}
        RectU(unsigned int left,  unsigned int top, 
            unsigned int right, unsigned int bottom) :
            left(left),top(top),right(right),bottom(bottom){}

        const RectU& operator=(const RectU& rhs)
        {
            left   = rhs.left;
            right  = rhs.right;
            top    = rhs.top;
            bottom = rhs.bottom;
            return *this;
        }
    };

    struct PortionBrushes
    {
        PortionBrushes () { memset(b,0,sizeof(ID2D1BitmapBrush*) * 5); }
        ~PortionBrushes() { releaseBrushes(); }
        inline void releaseBrushes()
        {
            for(int i = 0; i<5; ++i) SafeRelease(b[i]);
            memset(b,0,sizeof(ID2D1BitmapBrush*) * 5);
        }
        RectU borders;
        ID2D1BitmapBrush* b[5];
    };

    class D2DResPool 
    {
        public:
            ~D2DResPool();

            D2DBrushHandle getBrush(MColor);
            D2DBrushHandle getBrush(const wstring&);
            D2DBrushHandle getBrush(const wstring&, const RectU&);
            D2DImageHandle getImage(const wstring&);

            void createBrush(const D2DBrushHandle*);
            void createImage(const D2DImageHandle*);
            void clearResources();
            void removeResource(D2DBrushHandle&);
            void removeResource(D2DImageHandle&);

            inline void setWorkingRT(ID2D1RenderTarget* rt) { workingRT = rt; }

        private:
            typedef unordered_map<ID2D1SolidColorBrush**, MColor>     BrushColorMap;
            typedef unordered_map<void**, wstring>                    BrushBmpPathMap;
            typedef unordered_map<MColor, ID2D1SolidColorBrush*>      SolidBrushMap;
            typedef unordered_map<wstring, ID2D1BitmapBrush*>         BitmapBrushMap;
            typedef unordered_map<ID2D1BitmapBrush**, PortionBrushes> PortionBrushMap;
            typedef unordered_map<wstring, ID2D1Bitmap*>              BitmapMap;
            typedef unordered_map<void**, unsigned int>               FrameCountMap;

            BrushColorMap   colorMap;
            BrushBmpPathMap bitmapPathMap;
            SolidBrushMap   solidBrushCache;
            BitmapBrushMap  bitmapBrushCache;
            PortionBrushMap biPortionCache;
            BitmapMap       bitmapCache;
            FrameCountMap   frameCountMap;

            ID2D1RenderTarget* workingRT;

            void create9PatchBrush(const D2DBrushHandle*);

            friend class D2DBrushHandle;
            friend class D2DImageHandle;
            friend class MD2DPaintContextData;
    };

    class MWidget;
    class MD2DPaintContextData;
    class MD2DPaintContext
    {
        public:
            inline explicit MD2DPaintContext(MWidget* w);
            inline explicit MD2DPaintContext(MD2DPaintContextData* d);

            inline static D2DImageHandle createImage(const wstring& imagePath);
            inline static D2DBrushHandle createBrush(const wstring& imagePath);
            inline static D2DBrushHandle createBrush(MColor color);
            inline static D2DBrushHandle create9PatchBrush(const wstring& imagePath,const RectU& borderWidths);

            inline static void removeResource(D2DBrushHandle& b);
            inline static void removeResource(D2DImageHandle& i);

            ID2D1RenderTarget* getRenderTarget();

            void fill9PatchRect(const D2DBrushHandle& brush, const MRect& rect,
                const MRect& clipRect, bool scaleX, bool scaleY);

        private:
            static D2DResPool resPool;
            MD2DPaintContextData* data;

            friend class D2DBrushHandle;
            friend class D2DImageHandle;
            friend class MD2DPaintContextData;
    };

    inline D2DBrushHandle::operator ID2D1Brush*() const
    {
        M_ASSERT(etype != Unknown);
        if(pdata == 0) return 0;
        if(*(ID2D1Brush**)pdata == 0)
            MD2DPaintContext::resPool.createBrush(this);
        return *(ID2D1Brush**)pdata;
    }
    inline ID2D1Brush* D2DBrushHandle::getBrush() const
    {
        M_ASSERT(etype != Unknown);
        if(pdata == 0) return 0;
        if(*(ID2D1Brush**)pdata == 0)
            MD2DPaintContext::resPool.createBrush(this);
        return *(ID2D1Brush**)pdata;
    }
    inline D2DImageHandle::operator ID2D1Bitmap*() const
    {
        if(pdata == 0) return 0;
        if(*(ID2D1Bitmap**)pdata == 0)
            MD2DPaintContext::resPool.createImage(this);
        return *(ID2D1Bitmap**)pdata;
    }
    inline ID2D1Bitmap* D2DImageHandle::getImage() const
    {
        if(pdata == 0) return 0;
        if(*(ID2D1Bitmap**)pdata == 0)
            MD2DPaintContext::resPool.createImage(this);
        return *(ID2D1Bitmap**)pdata;
    }

    inline void MD2DPaintContext::removeResource(D2DBrushHandle& b)
        { resPool.removeResource(b); }
    inline void MD2DPaintContext::removeResource(D2DImageHandle& i)
        { resPool.removeResource(i); }
    inline D2DBrushHandle MD2DPaintContext::createBrush(MColor c)
        { return resPool.getBrush(c); }
    inline D2DBrushHandle MD2DPaintContext::createBrush(const wstring& p)
        { return resPool.getBrush(p); }
    inline D2DImageHandle MD2DPaintContext::createImage(const wstring& p)
        { return resPool.getImage(p); }
    inline D2DBrushHandle MD2DPaintContext::create9PatchBrush(const wstring& p, const RectU& r)
        { return resPool.getBrush(p, r); }
    inline MD2DPaintContext::MD2DPaintContext(MWidget* w):data(w->windowWidget()->m_windowExtras->m_pcData){}
    inline MD2DPaintContext::MD2DPaintContext(MD2DPaintContextData* d):data(d){}
}