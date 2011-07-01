#include "MD2DUtils.h"
#include "MApplication.h"
#include "MResource.h"

#ifdef MB_USE_D2D

#include "MGraphicsResource.h"
#include <d2d1.h>
#include <d2d1helper.h>
#include <wincodec.h>

using namespace std;

namespace MetalBone
{
    // If the image has many frames (e.g. GIF Format), we layout the frames
    // vertically in a larger image.
    ID2D1Bitmap* createD2DBitmap(const wstring& uri,
        ID2D1RenderTarget* workingRT, unsigned int* fc, bool forceOneFrame)
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

            if(!forceOneFrame)
            {
                decoder->GetFrameCount(&frameCount);
                decoder->GetFrame(0,&frame);
            }

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

    ID2D1BitmapBrush* createD2DBitmapBrush(const wstring& uri,
        ID2D1RenderTarget* workingRT, unsigned int* fc, bool forceOneFrame)
    {
        ID2D1Bitmap* bitmap = createD2DBitmap(uri, workingRT, fc, forceOneFrame);
        ID2D1BitmapBrush* brush;

        workingRT->CreateBitmapBrush(bitmap,
            D2D1::BitmapBrushProperties(D2D1_EXTEND_MODE_WRAP,D2D1_EXTEND_MODE_WRAP),
            D2D1::BrushProperties(), &brush);

        SafeRelease(bitmap);
        return brush;
    }

    ID2D1LinearGradientBrush* createD2DLinearGradientBrush(const LinearGradientData* data,
        ID2D1RenderTarget* rt)
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
#endif
