#include "MD2DPaintContext.h"
#include "MD2DPaintContextData.h"
#include "MBGlobal.h"
#include "MResource.h"
#include "MApplication.h"
#include "MWidget.h"

#include <d2d1helper.h>
#include <wincodec.h>

namespace MetalBone
{
    D2DResPool MD2DPaintContext::resPool;
    ID2D1RenderTarget* MD2DPaintContext::getRenderTarget()
    {
        if(data) return data->renderTarget;
        return 0;
    }

    void MD2DPaintContext::fill9PatchRect(const D2DBrushHandle& brush, const MRect& rect,
        const MRect& clipRect, bool scaleX, bool scaleY)
    {
        if(data) data->fill9PatchRect(brush, rect, clipRect, scaleX, scaleY);
    }


    void MD2DPaintContextData::beginDraw()
    {
        MD2DPaintContext::resPool.setWorkingRT(renderTarget);
        renderTarget->BeginDraw();
    }

    bool MD2DPaintContextData::endDraw()
    {
        MD2DPaintContext::resPool.setWorkingRT(0);
        HRESULT result = renderTarget->EndDraw();
        if(result == S_OK)
            return true;

        if(result == D2DERR_RECREATE_TARGET)
            MD2DPaintContext::resPool.clearResources();
        return false;
    }

    void MD2DPaintContextData::discardResources()
    { 
        createRenderTarget(); 
        MD2DPaintContext::resPool.clearResources();
    }

    unsigned int D2DBrushHandle::frameCount() const
    {
        D2DResPool::FrameCountMap::iterator it =
            MD2DPaintContext::resPool.frameCountMap.find((void**)pdata);

        if(it == MD2DPaintContext::resPool.frameCountMap.end())
            return 1;
        return it->second;
    }

    unsigned int D2DImageHandle::frameCount() const
    {
        D2DResPool::FrameCountMap::iterator it =
            MD2DPaintContext::resPool.frameCountMap.find((void**)pdata);

        if(it == MD2DPaintContext::resPool.frameCountMap.end())
            return 1;
        return it->second;
    }

    void D2DResPool::removeResource(D2DBrushHandle& b)
    {
        if(b.type() == D2DBrushHandle::Solid)
        {
            ID2D1SolidColorBrush** bp = (ID2D1SolidColorBrush**)b.pdata;
            SafeRelease(*bp);
            *bp = 0;
        } else if(b.type() == D2DBrushHandle::Bitmap)
        {
            ID2D1BitmapBrush** bp = (ID2D1BitmapBrush**)b.pdata;
            frameCountMap.erase((void**)bp);
            SafeRelease(*bp);
            *bp = 0;
        } else if(b.type() == D2DBrushHandle::NinePatch)
        {
            ID2D1BitmapBrush** bp = (ID2D1BitmapBrush**)b.pdata;
            biPortionCache.erase(bp);
            SafeRelease(*bp);
            *bp = 0;
        }
    }

    void D2DResPool::removeResource(D2DImageHandle& i)
    {
        ID2D1Bitmap** bp = i.pdata;
        frameCountMap.erase((void**)bp);
        SafeRelease(*bp);
        *bp = 0;
    }

    D2DBrushHandle D2DResPool::getBrush(MColor color)
    {
        ID2D1SolidColorBrush*& brush = solidBrushCache[color];
        if(brush == 0) colorMap[&brush] = color;
        return D2DBrushHandle(D2DBrushHandle::Solid,(ID2D1Brush**)&brush);
    }

    D2DBrushHandle D2DResPool::getBrush(const wstring& path)
    {
        ID2D1BitmapBrush*& brush = bitmapBrushCache[path];
        if(brush == 0) bitmapPathMap[(void**)&brush] = path;
        return D2DBrushHandle(D2DBrushHandle::Bitmap,(ID2D1Brush**)&brush);
    }

    D2DBrushHandle D2DResPool::getBrush(const wstring& path, const RectU& rect)
    {
        ID2D1BitmapBrush*& brush = bitmapBrushCache[path];
        if(brush == 0)
        {
            bitmapPathMap[(void**)&brush] = path;
            biPortionCache[&brush].borders = rect;
        }
        return D2DBrushHandle(D2DBrushHandle::NinePatch,(ID2D1Brush**)&brush);
    }

    D2DImageHandle D2DResPool::getImage(const wstring& imagePath)
    {
        ID2D1Bitmap*& bp = bitmapCache[imagePath];
        if(bp == 0) bitmapPathMap[(void**)&bp] = imagePath;
        return D2DImageHandle(&bp);
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

    void D2DResPool::createBrush(const D2DBrushHandle* handle)
    {
        M_ASSERT(handle->type() != D2DBrushHandle::Unknown);
        if(handle->type() == D2DBrushHandle::Solid)
        {
            ID2D1SolidColorBrush** brush = (ID2D1SolidColorBrush**)handle->pdata;
            workingRT->CreateSolidColorBrush(colorMap[brush],brush);
        } else if(handle->type() == D2DBrushHandle::Bitmap)
        {
            unsigned int fc = 1;
            ID2D1BitmapBrush** brush = (ID2D1BitmapBrush**)handle->pdata;
            *brush = createD2DBitmapBrush(bitmapPathMap[(void**)brush],workingRT,&fc);

            if(fc > 1) frameCountMap[(void**)brush] = fc;

        } else if(handle->type() == D2DBrushHandle::NinePatch)
        {
            create9PatchBrush(handle);
        }
    }

    void D2DResPool::createImage(const D2DImageHandle* handle)
    {
        unsigned int fc = 1;
        ID2D1Bitmap** bp = (ID2D1Bitmap**)handle->pdata;
        *bp = createD2DBitmap(bitmapPathMap[(void**)bp],workingRT,&fc);

        if(fc > 1) frameCountMap[(void**)bp] = fc;
    }

    void D2DResPool::create9PatchBrush(const D2DBrushHandle* handle)
    {
        ID2D1BitmapBrush** bp    = (ID2D1BitmapBrush**)handle->pdata;
        ID2D1BitmapBrush*  brush = *bp = createD2DBitmapBrush(bitmapPathMap[(void**)bp],workingRT);
        ID2D1Bitmap*       bitmap;
        brush->GetBitmap(&bitmap);

        PortionBrushes& pbs        = biPortionCache[bp];
        RectU&          widths     = pbs.borders;
        D2D1_SIZE_U     imageSize  = bitmap->GetPixelSize();
        D2D1_SIZE_U     pbmpSize;
        D2D1_RECT_F     pbmpRect;
        for(int i = 0; i < 5; ++i)
        {
            switch(i)
            {
                case D2DBrushHandle::TopCenter:
                    pbmpRect.left   = (FLOAT)widths.left;
                    pbmpRect.right  = (FLOAT)imageSize.width - widths.right;
                    pbmpRect.top    = 0.f; 
                    pbmpRect.bottom = (FLOAT)widths.top;
                    pbmpSize.width  = int(pbmpRect.right - widths.left);
                    pbmpSize.height = widths.top;
                    break;
                case D2DBrushHandle::CenterLeft:
                    pbmpRect.left   = 0.f;
                    pbmpRect.right  = (FLOAT)widths.left;
                    pbmpRect.top    = (FLOAT)widths.top;
                    pbmpRect.bottom = (FLOAT)imageSize.height - widths.bottom;
                    pbmpSize.width  = widths.left;
                    pbmpSize.height = int(pbmpRect.bottom - widths.top);
                    break;
                case D2DBrushHandle::Center:
                    pbmpRect.left   = (FLOAT)widths.left;
                    pbmpRect.right  = (FLOAT)imageSize.width  - widths.right;
                    pbmpRect.top    = (FLOAT)widths.top;
                    pbmpRect.bottom = (FLOAT)imageSize.height - widths.bottom;
                    pbmpSize.width  = int(pbmpRect.right  - widths.left);
                    pbmpSize.height = int(pbmpRect.bottom - widths.top );
                    break;
                case D2DBrushHandle::CenterRight:
                    pbmpRect.left   = (FLOAT)imageSize.width  - widths.right;
                    pbmpRect.right  = (FLOAT)imageSize.width;
                    pbmpRect.top    = (FLOAT)widths.top;
                    pbmpRect.bottom = (FLOAT)imageSize.height - widths.bottom;
                    pbmpSize.width  = widths.right;
                    pbmpSize.height = int(pbmpRect.bottom - widths.top);
                    break;
                case D2DBrushHandle::BottomCenter:
                    pbmpRect.left   = (FLOAT)widths.left;
                    pbmpRect.right  = (FLOAT)imageSize.width  - widths.right;
                    pbmpRect.top    = (FLOAT)imageSize.height - widths.bottom;
                    pbmpRect.bottom = (FLOAT)imageSize.height;
                    pbmpSize.width  = int(pbmpRect.right  - widths.left);
                    pbmpSize.height = widths.bottom;
                    break;
            }

            // There is a bug in ID2D1Bitmap::CopyFromBitmap(), we need to avoid it.
            ID2D1BitmapBrush* pb;
            ID2D1Bitmap* portionBmp;
            ID2D1BitmapRenderTarget* intermediateRT;

            HRESULT hr = workingRT->CreateCompatibleRenderTarget(D2D1::SizeF((FLOAT)pbmpSize.width,
                (FLOAT)pbmpSize.height),&intermediateRT);

            if(FAILED(hr)) break;

            intermediateRT->BeginDraw();
            intermediateRT->DrawBitmap(bitmap,
                D2D1::RectF(0.f,0.f,(FLOAT)pbmpSize.width,(FLOAT)pbmpSize.height),
                1.0f,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, pbmpRect);
            intermediateRT->EndDraw();
            intermediateRT->GetBitmap(&portionBmp);
            workingRT->CreateBitmapBrush(portionBmp,
                D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_CLAMP,D2D1_EXTEND_MODE_CLAMP),
                D2D1::BrushProperties(), &pb);

            SafeRelease(portionBmp);
            SafeRelease(intermediateRT);
            pbs.b[i] = pb;
        }

        SafeRelease(bitmap);
    }

    ID2D1BitmapBrush* D2DBrushHandle::getPortion(Portion p) const
    {
        M_ASSERT(etype == D2DBrushHandle::NinePatch);
        if(*pdata == 0)
            MD2DPaintContext::resPool.createBrush(this);

        ID2D1BitmapBrush* b = *(ID2D1BitmapBrush**)pdata;
        if(Conner == p)
            return b;
        else {
            PortionBrushes& pbs = MD2DPaintContext::resPool.biPortionCache[(ID2D1BitmapBrush**)pdata];
            return pbs.b[p];
        }
    }

    D2DResPool::~D2DResPool()
    {
        SolidBrushMap::iterator sbIt    = solidBrushCache.begin();
        SolidBrushMap::iterator sbItEnd = solidBrushCache.end();
        while(sbIt != sbItEnd)
        {
            SafeRelease(sbIt->second);
            ++sbIt;
        }
        BitmapBrushMap::iterator bbIt    = bitmapBrushCache.begin();
        BitmapBrushMap::iterator bbItEnd = bitmapBrushCache.end();
        while(bbIt != bbItEnd)
        {
            SafeRelease(bbIt->second);
            ++bbIt;
        }
        BitmapMap::iterator bmpIt    = bitmapCache.begin();
        BitmapMap::iterator bmpItEnd = bitmapCache.end();
        while(bmpIt != bmpItEnd)
        {
            SafeRelease(bmpIt->second);
            ++bmpIt;
        }
    }

    void D2DResPool::clearResources()
    {
        SolidBrushMap::iterator sit    = solidBrushCache.begin();
        SolidBrushMap::iterator sitEnd = solidBrushCache.end();
        while(sit != sitEnd)
        {
            SafeRelease(sit->second);
            // Keep the brush position inside the map, because
            // the MBrush stores the BrushPosition.
            solidBrushCache[sit->first] = 0;
            ++sit;
        }

        BitmapBrushMap::iterator bit    = bitmapBrushCache.begin();
        BitmapBrushMap::iterator bitEnd = bitmapBrushCache.end();
        while(bit != bitEnd)
        {
            SafeRelease(bit->second);
            bitmapBrushCache[bit->first] = 0;
            ++bit;
        }

        PortionBrushMap::iterator pit    = biPortionCache.begin();
        PortionBrushMap::iterator pitEnd = biPortionCache.end();
        while(pit != pitEnd)
        {
            pit->second.releaseBrushes();
            ++pit;
        }

        BitmapMap::iterator bmpIt    = bitmapCache.begin();
        BitmapMap::iterator bmpItEnd = bitmapCache.end();
        while(bmpIt != bmpItEnd)
        {
            SafeRelease(bmpIt->second);
            bitmapCache[bmpIt->first] = 0;
            ++bmpIt;
        }
        frameCountMap.clear();
    }

    void MD2DPaintContextData::createRenderTarget()
    {
        SafeRelease(renderTarget);

        D2D1_RENDER_TARGET_PROPERTIES p = D2D1::RenderTargetProperties();
        p.usage  = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
        p.type   = mApp->isHardwareAccerated() ? 
                D2D1_RENDER_TARGET_TYPE_HARDWARE : D2D1_RENDER_TARGET_TYPE_SOFTWARE;
        p.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        p.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;

        mApp->getD2D1Factory()->CreateHwndRenderTarget(p,
                D2D1::HwndRenderTargetProperties(widget->windowHandle(), widget->size()),
                &renderTarget);
    }

    void MD2DPaintContextData::fill9PatchRect(const D2DBrushHandle& b, const MRect& rect,
        const MRect& clipRect, bool scaleX, bool scaleY)
    {
        if(b.type() != D2DBrushHandle::NinePatch) return;

        renderTarget->PushAxisAlignedClip(clipRect,D2D1_ANTIALIAS_MODE_ALIASED);

        ID2D1BitmapBrush* brush    = b.getPortion(D2DBrushHandle::Conner);
        PortionBrushes&   pbs      = MD2DPaintContext::resPool.biPortionCache[(ID2D1BitmapBrush**)b.pdata];

        // 0.TopLeft     1.TopCenter     2.TopRight
        // 3.CenterLeft  4.Center        5.CenterRight
        // 6.BottomLeft  7.BottomCenter  8.BottomRight
        bool drawParts[9] = {true,true,true,true,true,true,true,true,true};
        int x1 = rect.left   + pbs.borders.left;
        int x2 = rect.right  - pbs.borders.right;
        int y1 = rect.top    + pbs.borders.top;
        int y2 = rect.bottom - pbs.borders.bottom;

        ID2D1Bitmap* bitmap;
        brush->GetBitmap(&bitmap);
        D2D1_SIZE_U imageSize = bitmap->GetPixelSize();
        bitmap->Release();

        FLOAT scaleXFactor = FLOAT(x2 - x1) / (imageSize.width  - pbs.borders.left - pbs.borders.right);
        FLOAT scaleYFactor = FLOAT(y2 - y1) / (imageSize.height - pbs.borders.top  - pbs.borders.bottom);

        if(clipRect.left > x1) {
            drawParts[0] = drawParts[3] = drawParts[6] = false;
            if(clipRect.left > x2)
                drawParts[1] = drawParts[4] = drawParts[7] = false;
        }
        if(clipRect.top > y1) {
            drawParts[0] = drawParts[1] = drawParts[2] = false;
            if(clipRect.top > y2)
                drawParts[3] = drawParts[4] = drawParts[5] = false;
        }
        if(clipRect.right <= x2) {
            drawParts[2] = drawParts[5] = drawParts[8] = false;
            if(clipRect.right <= x1)
                drawParts[1] = drawParts[4] = drawParts[7] = false;
        }
        if(clipRect.bottom <= y2) {
            drawParts[6] = drawParts[7] = drawParts[8] = false;
            if(clipRect.bottom <= y1)
                drawParts[3] = drawParts[4] = drawParts[5] = false;
        }

        // Draw!
        D2D1_RECT_F drawRect;
        // Assume we can always get a valid brush for Conner.
        if(drawParts[0]) {
            drawRect.left   = (FLOAT)rect.left;
            drawRect.right  = (FLOAT)x1;
            drawRect.top    = (FLOAT)rect.top;
            drawRect.bottom = (FLOAT)y1;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
            renderTarget->FillRectangle(drawRect,brush);
        }
        if(drawParts[2]) {
            drawRect.left   = (FLOAT)x2;
            drawRect.right  = (FLOAT)rect.right;
            drawRect.top    = (FLOAT)rect.top;
            drawRect.bottom = (FLOAT)y1;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.right - imageSize.width, drawRect.top));
            renderTarget->FillRectangle(drawRect,brush);
        }
        if(drawParts[6]) {
            drawRect.left   = (FLOAT)rect.left;
            drawRect.right  = (FLOAT)x1;
            drawRect.top    = (FLOAT)y2;
            drawRect.bottom = (FLOAT)rect.bottom;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.bottom - imageSize.height));
            renderTarget->FillRectangle(drawRect,brush);
        }
        if(drawParts[8]) {
            drawRect.left   = (FLOAT)x2;
            drawRect.right  = (FLOAT)rect.right;
            drawRect.top    = (FLOAT)y2;
            drawRect.bottom = (FLOAT)rect.bottom;
            brush->SetTransform(D2D1::Matrix3x2F::Translation(drawRect.right - imageSize.width, drawRect.bottom - imageSize.height));
            renderTarget->FillRectangle(drawRect,brush);
        }
        if(drawParts[1]) {
            drawRect.left   = (FLOAT)x1;
            drawRect.right  = (FLOAT)x2;
            drawRect.top    = (FLOAT)rect.top;
            drawRect.bottom = (FLOAT)y1;
            brush = b.getPortion(D2DBrushHandle::TopCenter);
            if(brush)
            {
                D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
                if(scaleX) matrix = D2D1::Matrix3x2F::Scale(scaleXFactor,1.0f) * matrix;
                brush->SetTransform(matrix);
                renderTarget->FillRectangle(drawRect,brush);
            }
        }
        if(drawParts[3]) {
            drawRect.left   = (FLOAT)rect.left;
            drawRect.right  = (FLOAT)x1;
            drawRect.top    = (FLOAT)y1;
            drawRect.bottom = (FLOAT)y2;
            brush = b.getPortion(D2DBrushHandle::CenterLeft);
            if(brush)
            {
                D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
                if(scaleY) matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleYFactor) * matrix;
                brush->SetTransform(matrix);
                renderTarget->FillRectangle(drawRect,brush);
            }
        }
        if(drawParts[4]) {
            drawRect.left   = (FLOAT)x1;
            drawRect.right  = (FLOAT)x2;
            drawRect.top    = (FLOAT)y1;
            drawRect.bottom = (FLOAT)y2;
            brush = b.getPortion(D2DBrushHandle::Center);
            if(brush)
            {
                D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
                if(scaleX)
                    matrix = D2D1::Matrix3x2F::Scale(scaleXFactor, scaleY ? scaleYFactor : 1.0f) * matrix; 
                else if(scaleY)
                    matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleYFactor) * matrix;
                brush->SetTransform(matrix);
                renderTarget->FillRectangle(drawRect,brush);
            }
        }
        if(drawParts[5]) {
            drawRect.left   = (FLOAT)x2;
            drawRect.right  = (FLOAT)rect.right;
            drawRect.top    = (FLOAT)y1;
            drawRect.bottom = (FLOAT)y2;
            brush = b.getPortion(D2DBrushHandle::CenterRight);
            if(brush)
            {
                D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
                if(scaleY)
                    matrix = D2D1::Matrix3x2F::Scale(1.0f,scaleYFactor) * matrix;
                brush->SetTransform(matrix);
                renderTarget->FillRectangle(drawRect,brush);
            }
        }
        if(drawParts[7]) {
            drawRect.left   = (FLOAT)x1;
            drawRect.right  = (FLOAT)x2;
            drawRect.top    = (FLOAT)y2;
            drawRect.bottom = (FLOAT)rect.bottom;
            brush = b.getPortion(D2DBrushHandle::BottomCenter);
            if(brush)
            {
                D2D1::Matrix3x2F matrix(D2D1::Matrix3x2F::Translation(drawRect.left, drawRect.top));
                if(scaleX) matrix = D2D1::Matrix3x2F::Scale(scaleXFactor,1.0f) * matrix;
                brush->SetTransform(matrix);
                renderTarget->FillRectangle(drawRect,brush);
            }
        }

        renderTarget->PopAxisAlignedClip();
    }
}