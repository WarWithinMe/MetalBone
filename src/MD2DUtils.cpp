#include "MD2DUtils.h"
#include "MApplication.h"
#include "MResource.h"
#include "MD2DPaintContext.h"

#include <unordered_map>
#include <d2d1.h>
#include <d2d1helper.h>
#include <wincodec.h>

using namespace std;
using namespace std::tr1;

namespace MetalBone
{
    struct NinePatchData;
    struct BitmapData;
    struct D2DBrushData
    {
        D2DBrushData():type(MD2DBrushHandle::Unknown),v(),brush(0),refCount(0){}
        ~D2DBrushData();

        inline void attach();
        inline void detach();

        MD2DBrushHandle::Type type;
        union
        {
            unsigned int              color;
            const LinearGradientData* linearData;
            BitmapData*               imageData;
            NinePatchData*            ninePatchData;
        } v;

        ID2D1Brush*  brush;
        unsigned int refCount;
    };

    struct NinePatchData
    {
        NinePatchData ():mainBmpBrush(0),imageRect(0),portionCreated(false) { memset(b,0,sizeof(ID2D1BitmapBrush*) * 3); }
        ~NinePatchData() { releaseBrushes(); delete imageRect; mainBmpBrush->detach(); }
        inline void releaseBrushes()
        {
            for(int i = 0; i<3; ++i) SafeRelease(b[i]);
            memset(b,0,sizeof(ID2D1BitmapBrush*) * 3);
        }

        D2DBrushData*     mainBmpBrush;
        ID2D1BitmapBrush* b[3];
        MRectU            borders;
        MRectU*           imageRect;
        bool              portionCreated;
    };

    struct BitmapData
    {
        BitmapData():frameCount(0){}
        wstring filePath;
        unsigned int frameCount;
    };

    struct D2DImageData
    {
        D2DImageData():bitmap(0),frameCount(0),refCount(0){}
        ~D2DImageData();

        inline void attach();
        inline void detach();

        wstring      filePath;
        ID2D1Bitmap* bitmap;
        unsigned int frameCount;
        unsigned int refCount;
    };

    struct D2DResManager
    {
        ~D2DResManager();

        typedef unordered_map<MColor             ,       D2DBrushData*>  SolidBrushCache;
        typedef unordered_map<wstring            ,       D2DBrushData*>  BitmapBrushCache;
        typedef unordered_map<const LinearGradientData*, D2DBrushData*>  LinearBrushCache;
        typedef unordered_multimap<wstring       ,       D2DBrushData*>  NinePatchBrushCache;
        typedef unordered_map<wstring            ,       D2DImageData*>  BitmapCache;

        SolidBrushCache     solidBrushCache;
        BitmapBrushCache    bitmapBrushCache;
        BitmapCache         bitmapCache;
        LinearBrushCache    linearBrushCache;
        NinePatchBrushCache ninePatchCache;

        D2DBrushData* getBrush(MColor);
        D2DBrushData* getBrush(const std::wstring&);
        D2DBrushData* getBrush(const LinearGradientData*);
        D2DBrushData* getBrush(const std::wstring&, const MRectU&, const MRectU*);

        D2DImageData* getImage(const std::wstring&);

        void discardBrushes();
        void discardImages();
    };

    static D2DResManager d2dResManager;

    D2DImageData* D2DResManager::getImage(const std::wstring& imagePath)
    {
        D2DImageData*& image = bitmapCache[imagePath];
        if(image == 0)
        {
            image = new D2DImageData();
            image->filePath = imagePath;
        }
        return image;
    }

    D2DBrushData* D2DResManager::getBrush(MColor color)
    {
        D2DBrushData*& brush = solidBrushCache[color];
        if(brush == 0)
        {
            brush          = new D2DBrushData();
            brush->type    = MD2DBrushHandle::Solid;
            brush->v.color = color.getARGB(); 
        }
        return brush;
    }

    D2DBrushData* D2DResManager::getBrush(const std::wstring& imagePath)
    {
        D2DBrushData*& brush = bitmapBrushCache[imagePath];
        if(brush == 0)
        {
            brush              = new D2DBrushData();
            brush->type        = MD2DBrushHandle::Bitmap;
            BitmapData* data   = new BitmapData();
            brush->v.imageData = data;
            data->filePath     = imagePath;
        }
        return brush;
    }

    D2DBrushData* D2DResManager::getBrush(const LinearGradientData* linear)
    {
        D2DBrushData*& brush = linearBrushCache[linear];
        if(brush == 0)
        {
            brush               = new D2DBrushData();
            brush->type         = MD2DBrushHandle::LinearGradient;
            brush->v.linearData = linear;
        }
        return brush;
    }

    D2DBrushData* D2DResManager::getBrush(const std::wstring& imagePath,
        const MRectU& border, const MRectU* imageRect)
    {
        D2DBrushData* newBrush    = new D2DBrushData();
        newBrush->type            = MD2DBrushHandle::NinePatch;

        NinePatchData* ninePatch  = new NinePatchData();
        ninePatch->mainBmpBrush   = getBrush(imagePath);
        ninePatch->borders        = border;

        if(imageRect) ninePatch->imageRect = new MRectU(*imageRect);
        
        newBrush->v.ninePatchData = ninePatch;

        ninePatch->mainBmpBrush->attach();

        // Caching is only for discarding.
        ninePatchCache.insert(NinePatchBrushCache::value_type(imagePath,newBrush));
        return newBrush;
    }

    D2DResManager::~D2DResManager()
    {
        discardImages();
        discardBrushes();
    }

    void D2DResManager::discardImages()
    {
        BitmapCache::iterator it    = bitmapCache.begin();
        BitmapCache::iterator itEnd = bitmapCache.end();
        while(it != itEnd)
        {
            D2DImageData* data = it->second;
            SafeRelease(data->bitmap);
            ++it;
        }
    }

    void D2DResManager::discardBrushes()
    {
        SolidBrushCache::iterator sit    = solidBrushCache.begin();
        SolidBrushCache::iterator sitEnd = solidBrushCache.end();
        while(sit != sitEnd)
        {
            D2DBrushData* data = sit->second;
            SafeRelease(data->brush);
            ++sit;
        }

        BitmapBrushCache::iterator bit    = bitmapBrushCache.begin();
        BitmapBrushCache::iterator bitEnd = bitmapBrushCache.end();
        while(bit != bitEnd)
        {
            D2DBrushData* data = bit->second;
            SafeRelease(data->brush);
            ++bit;
        }

        LinearBrushCache::iterator lit    = linearBrushCache.begin();
        LinearBrushCache::iterator litEnd = linearBrushCache.end();
        while(lit != litEnd)
        {
            D2DBrushData* data = lit->second;
            SafeRelease(data->brush);
            ++lit;
        }

        NinePatchBrushCache::iterator nit    = ninePatchCache.begin();
        NinePatchBrushCache::iterator nitEnd = ninePatchCache.end();
        while(nit != nitEnd)
        {
            D2DBrushData* data = nit->second;
            data->v.ninePatchData->releaseBrushes();
            ++nit;
        }
    }


    inline void D2DBrushData::attach() { ++refCount; }
    inline void D2DImageData::attach() { ++refCount; }
    inline void D2DBrushData::detach()
    { 
        if(--refCount > 0) return;
        switch(type)
        {
            case MD2DBrushHandle::Solid:
                d2dResManager.solidBrushCache.erase(MColor(v.color)); break;
            case MD2DBrushHandle::Bitmap:
                d2dResManager.bitmapBrushCache.erase(v.imageData->filePath);
                delete v.imageData;
                break;
            case MD2DBrushHandle::NinePatch:
                {
                    typedef D2DResManager::NinePatchBrushCache::iterator It;
                    pair<It, It> result = d2dResManager.ninePatchCache.equal_range(
                        v.ninePatchData->mainBmpBrush->v.imageData->filePath);

                    while(result.first != result.second)
                    {
                        if(result.first->second == this)
                        {
                            d2dResManager.ninePatchCache.erase(result.first);
                            break;
                        }
                        ++result.first;
                    }
                }
                delete v.ninePatchData;
                break;
            case MD2DBrushHandle::LinearGradient:
                d2dResManager.linearBrushCache.erase(v.linearData);
                break;
        }
        delete this;
    }
    inline void D2DImageData::detach()
    {
        if(--refCount > 0) return;
        d2dResManager.bitmapCache.erase(filePath);
        delete this;
    }
    D2DBrushData::~D2DBrushData()
    {
        M_ASSERT(refCount == 0);
        if(brush != 0) brush->Release();
    }
    D2DImageData::~D2DImageData()
    {
        M_ASSERT(refCount == 0);
        if(bitmap != 0) bitmap->Release();
    }

    inline MD2DBrushHandle::MD2DBrushHandle(D2DBrushData* d):data(d){ d->attach(); }
    inline MD2DImageHandle::MD2DImageHandle(D2DImageData* d):data(d){ d->attach(); }

    MD2DBrushHandle::Type MD2DBrushHandle::type() const
    {
        if(data) return data->type;
        return Unknown;
    }

    MRectU MD2DBrushHandle::ninePatchBorders() const
    {
        if(data && data->type == NinePatch)
            return data->v.ninePatchData->borders;

        return MRectU();
    }

    MRectU MD2DBrushHandle::ninePatchImageRect() const
    {
        if(data && data->type == NinePatch)
        {
            MRectU* rect = data->v.ninePatchData->imageRect;
            if(rect) return *rect;
        }
        return MRectU();
    }

    unsigned int MD2DBrushHandle::frameCount() const
    {
        if(data == 0 || data->type != Bitmap) return 0;
        return data->v.imageData->frameCount;
    }
    unsigned int MD2DImageHandle::frameCount() const
    {
        if(data) return data->frameCount;
        return 0;
    }

    void MD2DBrushHandle::discardBrushes()
        { d2dResManager.discardBrushes(); }
    void MD2DImageHandle::discardImages()
        { d2dResManager.discardImages(); }

    MD2DBrushHandle::~MD2DBrushHandle() { if(data) data->detach(); }
    MD2DImageHandle::~MD2DImageHandle() { if(data) data->detach(); }
    MD2DImageHandle::MD2DImageHandle(const MD2DImageHandle& rhs)
    {
        data = rhs.data;
        if(data != 0) data->attach();
    }
    MD2DBrushHandle::MD2DBrushHandle(const MD2DBrushHandle& rhs)
    {
        data = rhs.data;
        if(data != 0) data->attach();
    }
    const MD2DImageHandle& MD2DImageHandle::operator=(const MD2DImageHandle& rhs)
    {
        if(data) data->detach();
        if((data = rhs.data) != 0) data->attach();
        return *this;
    }
    const MD2DBrushHandle& MD2DBrushHandle::operator=(const MD2DBrushHandle& rhs)
    {
        if(data) data->detach();
        if((data = rhs.data) != 0) data->attach();
        return *this;
    }

    MD2DBrushHandle MD2DBrushHandle::create(MColor color)
        { return MD2DBrushHandle(d2dResManager.getBrush(color)); }
    MD2DBrushHandle MD2DBrushHandle::create(const std::wstring& imagePath)
        { return MD2DBrushHandle(d2dResManager.getBrush(imagePath)); }
    MD2DBrushHandle MD2DBrushHandle::create(const LinearGradientData* linearGradientData)
        { return MD2DBrushHandle(d2dResManager.getBrush(linearGradientData)); }
    MD2DBrushHandle MD2DBrushHandle::createNinePatch(const std::wstring& imagePath,
        const MRectU& borderWidths, const MRectU* imageRect)
        { return MD2DBrushHandle(d2dResManager.getBrush(imagePath,borderWidths,imageRect)); }
    MD2DImageHandle MD2DImageHandle::create(const wstring& imagePath)
        { return MD2DImageHandle(d2dResManager.getImage(imagePath)); }
    

    ID2D1LinearGradientBrush* MD2DBrushHandle::getLinearGraidentBrush(const D2D1_RECT_F* drawingRect)
    {
        M_ASSERT(data != 0);
        M_ASSERT(data->type == LinearGradient);

        ID2D1LinearGradientBrush* brush = (ID2D1LinearGradientBrush*)getBrush();
        if(drawingRect == 0) return brush;

        const LinearGradientData* linear = data->v.linearData;

        FLOAT width  = drawingRect->right  - drawingRect->left;
        FLOAT height = drawingRect->bottom - drawingRect->top;

        D2D1_POINT_2F startPoint;
        D2D1_POINT_2F endPoint;

        if(linear->isPercentagePos(LinearGradientData::StartX))
            startPoint.x = width * linear->getPosValue(LinearGradientData::StartX) / 100;
        else
            startPoint.x = (FLOAT)linear->getPosValue(LinearGradientData::StartX);

        if(linear->isPercentagePos(LinearGradientData::StartY))
            startPoint.y = height * linear->getPosValue(LinearGradientData::StartY) / 100;
        else
            startPoint.y = (FLOAT)linear->getPosValue(LinearGradientData::StartY);

        if(linear->isPercentagePos(LinearGradientData::EndX))
            endPoint.x   = width * linear->getPosValue(LinearGradientData::EndX) / 100;
        else
            endPoint.x   = (FLOAT)linear->getPosValue(LinearGradientData::EndX);

        if(linear->isPercentagePos(LinearGradientData::EndY))
            endPoint.y   = height * linear->getPosValue(LinearGradientData::EndY) / 100;
        else
            endPoint.y   = (FLOAT)linear->getPosValue(LinearGradientData::EndY);

        brush->SetStartPoint(startPoint);
        brush->SetEndPoint(endPoint);
        return brush;
    }

    ID2D1BitmapBrush* MD2DBrushHandle::getPortion(NinePatchPortion p)
    {
        M_ASSERT(data != 0);
        M_ASSERT(data->type == NinePatch);
        NinePatchData* ninePatch = data->v.ninePatchData;
        MD2DBrushHandle mainBrush(ninePatch->mainBmpBrush);
        ID2D1BitmapBrush* conner = (ID2D1BitmapBrush*)mainBrush.getBrush();
        if(p == Conner) return conner;
        if(ninePatch->portionCreated) return ninePatch->b[p];

        ninePatch->portionCreated = true;

        // Create the portions.
        ID2D1Bitmap* bitmap;
        conner->GetBitmap(&bitmap);

        MRectU&      borders    = ninePatch->borders;
        D2D1_SIZE_U  imageSize;
        unsigned int portionWidth;
        unsigned int portionHeight;
        D2D1_RECT_F  portionRect;
        D2D1_RECT_F  imageRect;

        if(ninePatch->imageRect == 0)
        {
            imageSize = bitmap->GetPixelSize();
            imageRect = D2D1::RectF(0.f,0.f,(FLOAT)imageSize.width,(FLOAT)imageSize.height);
        } else
        {
            imageRect = *ninePatch->imageRect;
            imageSize.width  = ninePatch->imageRect->width();
            imageSize.height = ninePatch->imageRect->height(); 
        }

        ID2D1Bitmap*             portionBitmap;
        ID2D1BitmapBrush*        portionBrush;
        ID2D1BitmapRenderTarget* intermediateRT;
        ID2D1RenderTarget*       workingRT = MD2DPaintContext::getRecentRenderTarget();

        // *********** VerCenter
        portionWidth  = imageSize.width - borders.left - borders.right;
        if(portionWidth > 0)
        {
            portionHeight = borders.top + borders.bottom;

            portionRect.left   = imageRect.left  + borders.left;
            portionRect.right  = imageRect.right - borders.right;
            portionRect.top    = imageRect.top;
            portionRect.bottom = imageRect.top   + borders.top;

            HRESULT hr = workingRT->CreateCompatibleRenderTarget(
                D2D1::SizeF((FLOAT)portionWidth,(FLOAT)portionHeight), &intermediateRT);

            if(FAILED(hr)) return 0;

            intermediateRT->BeginDraw();
            intermediateRT->DrawBitmap(bitmap,
                D2D1::RectF(0.f,0.f,(FLOAT)portionWidth,(FLOAT)borders.top),
                1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, portionRect);

            portionRect.bottom = imageRect.bottom;
            portionRect.top    = portionRect.bottom - borders.bottom; 

            intermediateRT->DrawBitmap(bitmap,
                D2D1::RectF(0.f,(FLOAT)borders.top,
                (FLOAT)portionWidth,(FLOAT)borders.top + (FLOAT)borders.bottom),
                1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, portionRect);
            intermediateRT->EndDraw();
            intermediateRT->GetBitmap(&portionBitmap);
            workingRT->CreateBitmapBrush(portionBitmap,
                D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP,D2D1_EXTEND_MODE_CLAMP),
                D2D1::BrushProperties(), &portionBrush);

            ninePatch->b[VerCenter] = portionBrush;
            intermediateRT->Release();
            portionBitmap->Release();
        }

        // *********** HorCenter
        portionHeight = imageSize.height - borders.top - borders.bottom;
        if(portionHeight > 0)
        {
            portionWidth  = borders.left + borders.right;

            portionRect.left   = imageRect.left;
            portionRect.right  = imageRect.left + borders.left;
            portionRect.top    = imageRect.top  + borders.top;
            portionRect.bottom = imageRect.bottom - borders.bottom;

            HRESULT hr = workingRT->CreateCompatibleRenderTarget(
                D2D1::SizeF((FLOAT)portionWidth,(FLOAT)portionHeight), &intermediateRT);

            if(FAILED(hr)) return 0;

            intermediateRT->BeginDraw();
            intermediateRT->DrawBitmap(bitmap,
                D2D1::RectF(0.f,0.f,(FLOAT)borders.left,(FLOAT)portionHeight),
                1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, portionRect);

            portionRect.left  = imageRect.right - borders.right;
            portionRect.right = imageRect.right; 

            intermediateRT->DrawBitmap(bitmap,
                D2D1::RectF((FLOAT)borders.left,0.f,
                (FLOAT)borders.left + (FLOAT)borders.right,(FLOAT)portionHeight),
                1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, portionRect);
            intermediateRT->EndDraw();
            intermediateRT->GetBitmap(&portionBitmap);
            workingRT->CreateBitmapBrush(portionBitmap,
                D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP,D2D1_EXTEND_MODE_CLAMP),
                D2D1::BrushProperties(), &portionBrush);

            ninePatch->b[HorCenter] = portionBrush;
            intermediateRT->Release();
            portionBitmap->Release();
        }

        // *********** Center
        portionWidth  = imageSize.width  - borders.left - borders.right;
        portionHeight = imageSize.height - borders.top  - borders.bottom;

        if(portionWidth > 0 && portionHeight > 0)
        {
            portionRect.left   = imageRect.left   + borders.left;
            portionRect.right  = imageRect.right  - borders.right;
            portionRect.top    = imageRect.top    + borders.top;
            portionRect.bottom = imageRect.bottom - borders.bottom;

            HRESULT hr = workingRT->CreateCompatibleRenderTarget(
                D2D1::SizeF((FLOAT)portionWidth,(FLOAT)portionHeight), &intermediateRT);

            if(FAILED(hr)) return 0;

            intermediateRT->BeginDraw();
            intermediateRT->DrawBitmap(bitmap,
                D2D1::RectF(0.f,0.f,(FLOAT)portionWidth,(FLOAT)portionHeight),
                1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, portionRect);
            intermediateRT->EndDraw();
            intermediateRT->GetBitmap(&portionBitmap);
            workingRT->CreateBitmapBrush(portionBitmap,
                D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP,D2D1_EXTEND_MODE_CLAMP),
                D2D1::BrushProperties(), &portionBrush);

            ninePatch->b[Center] = portionBrush;
            intermediateRT->Release();
            portionBitmap->Release();
        }

        SafeRelease(bitmap);

        return ninePatch->b[p];
    }

    MD2DImageHandle::operator ID2D1Bitmap*() { return getImage(); }
    MD2DBrushHandle::operator ID2D1Brush*()  { return getBrush(); }
    ID2D1Bitmap* MD2DImageHandle::getImage()
    {
        M_ASSERT(data != 0);
        if(data->bitmap == 0)
        {
            data->bitmap = createD2DBitmap(data->filePath,
                MD2DPaintContext::getRecentRenderTarget(), &data->frameCount);
        }
        
        return data->bitmap;
    }

    ID2D1Brush* MD2DBrushHandle::getBrush()
    {
        ID2D1RenderTarget* rt = MD2DPaintContext::getRecentRenderTarget();

        M_ASSERT(data != 0);
        if(data->brush == 0)
        {
            if(data->type == MD2DBrushHandle::Solid)
            {
                ID2D1SolidColorBrush** brush = (ID2D1SolidColorBrush**) &data->brush;
                rt->CreateSolidColorBrush(MColor(data->v.color),brush);
            } else if(data->type == MD2DBrushHandle::Bitmap)
            {
                data->brush = createD2DBitmapBrush(data->v.imageData->filePath,
                    rt, &(data->v.imageData->frameCount));
            } else if(data->type == LinearGradient)
            {
                data->brush = createD2DLinearGradientBrush(data->v.linearData, rt);
            } else if(data->type == NinePatch)
            {
                return 0;
            }
        }
        return data->brush;
    }


    // If the image has many frames (e.g. GIF Format), we layout the frames
    // vertically in a larger image.
    ID2D1Bitmap* createD2DBitmap(const wstring& uri, ID2D1RenderTarget* workingRT, unsigned int* fc)
    {
        IWICImagingFactory*    wicFactory = mApp->getWICImagingFactory();
        IWICBitmapDecoder*     decoder	  = 0;
        IWICBitmapFrameDecode* frame	  = 0;
        IWICFormatConverter*   converter  = 0;
        ID2D1Bitmap*           bitmap     = 0;

        bool hasError = false;
        HRESULT hr;
        if(uri.at(0) == L':')  // Image file is inside MResources.
        {
            MResource res;
            if(res.open(uri))
            {
                IWICStream* stream = 0;
                wicFactory->CreateStream(&stream);
                stream->InitializeFromMemory((WICInProcPointer)res.byteBuffer(),res.length());
                wicFactory->CreateDecoderFromStream(stream,NULL,WICDecodeMetadataCacheOnDemand,&decoder);
                SafeRelease(stream);
            } else { hasError = true; }
        } else {
            hr = wicFactory->CreateDecoderFromFilename(uri.c_str(),NULL,
                GENERIC_READ, WICDecodeMetadataCacheOnDemand,&decoder);
            hasError = FAILED(hr);
        }

        unsigned int frameCount = 1;
        if(hasError)
        {
            wstring error = L"[D2D1BrushPool] Cannot open image file: ";
            error.append(uri);
            error.append(1,L'\n');
            mWarning(true,error.c_str());
            // create a empty bitmap because we can't find the image.
            hr = workingRT->CreateBitmap(D2D1::SizeU(),
                D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);

        } else {
            decoder->GetFrameCount(&frameCount);
            decoder->GetFrame(0,&frame);
            if(frameCount == 1)
            {
                wicFactory->CreateFormatConverter(&converter);
                converter->Initialize(frame,
                    GUID_WICPixelFormat32bppPBGRA,
                    WICBitmapDitherTypeNone,NULL,
                    0.f,WICBitmapPaletteTypeMedianCut);
                workingRT->CreateBitmapFromWicBitmap(converter,
                    D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, 
                    D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
            } else
            {
                D2D1_SIZE_U rtSize;
                frame->GetSize(&rtSize.width, &rtSize.height);
                unsigned int frameHeight = rtSize.height;
                rtSize.height *= frameCount;

                ID2D1BitmapRenderTarget* bitmapRT = 0;
                D2D1_PIXEL_FORMAT pf = D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, 
                    D2D1_ALPHA_MODE_PREMULTIPLIED);
                workingRT->CreateCompatibleRenderTarget(0, &rtSize, &pf, 
                    D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE,&bitmapRT);
                bitmapRT->BeginDraw();
                for(unsigned int i = 0; i < frameCount; ++i)
                {
                    ID2D1Bitmap* frameBitmap = 0;

                    if(i != 0) decoder->GetFrame(i, &frame);

                    wicFactory->CreateFormatConverter(&converter);
                    converter->Initialize(frame,
                        GUID_WICPixelFormat32bppPBGRA,
                        WICBitmapDitherTypeNone,NULL,
                        0.f,WICBitmapPaletteTypeMedianCut);
                    workingRT->CreateBitmapFromWicBitmap(converter,
                        D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, 
                        D2D1_ALPHA_MODE_PREMULTIPLIED)), &frameBitmap);

                    FLOAT y = FLOAT(frameHeight * i);
                    bitmapRT->DrawBitmap(frameBitmap,
                        D2D1::RectF(0.f, y, (FLOAT)rtSize.width, y + frameHeight));

                    SafeRelease(frameBitmap);
                    SafeRelease(frame);
                    SafeRelease(converter);
                }
                bitmapRT->EndDraw();
                bitmapRT->GetBitmap(&bitmap);
                bitmapRT->Release();
            }
        }

        if(fc) *fc = frameCount;

        SafeRelease(decoder);
        SafeRelease(frame);
        SafeRelease(converter);

        return bitmap;
    }

    ID2D1BitmapBrush* createD2DBitmapBrush(const wstring& uri, ID2D1RenderTarget* workingRT, unsigned int* fc)
    {
        ID2D1Bitmap* bitmap = createD2DBitmap(uri, workingRT, fc);
        ID2D1BitmapBrush* brush;

        workingRT->CreateBitmapBrush(bitmap,
            D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP,D2D1_EXTEND_MODE_WRAP),
            D2D1::BrushProperties(), &brush);

        SafeRelease(bitmap);
        return brush;
    }

    ID2D1LinearGradientBrush* createD2DLinearGradientBrush(const LinearGradientData* data, ID2D1RenderTarget* rt)
    {
        D2D1_GRADIENT_STOP* d2dStops = new D2D1_GRADIENT_STOP[data->stopCount];
        for(int i = 0; i < data->stopCount; ++i)
        {
            d2dStops[i].position = data->stops[i].pos;
            d2dStops[i].color    = D2D1::ColorF(data->stops[i].argb & 0xFFFFFF,
                FLOAT(data->stops[i].argb >> 24 & 0xFF) / 255);
        }

        ID2D1LinearGradientBrush* brush;
        ID2D1GradientStopCollection* collection;
        rt->CreateGradientStopCollection(d2dStops, data->stopCount, &collection);
        rt->CreateLinearGradientBrush(D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES(),
            D2D1::BrushProperties(),collection,&brush);

        delete[] d2dStops;
        SafeRelease(collection);
        return brush;
    }
}
