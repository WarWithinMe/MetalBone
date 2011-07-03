#include "MGraphicsResource.h"
#include "MGraphics.h"

#include <unordered_map>

#ifdef MB_USE_D2D
#  include "utils/MD2DUtils.h"
#  include <d2d1.h>
#  include <d2d1helper.h>
#endif

#ifdef MB_USE_SKIA
#  include "MResource.h"
#  include "3rd/skia/core/SkBitmap.h"
#  include "3rd/skia/core/SkShader.h"
#  include "3rd/skia/effects/SkGradientShader.h"
#  include <wincodec.h>
#endif

using namespace std;
using namespace std::tr1;

namespace MetalBone
{
    struct NinePatchData;
    struct BitmapData
    {
        BitmapData():frameCount(0){}

        wstring      filePath;
        unsigned int frameCount;
    };
    struct BrushData
    {
        BrushData ():type(MBrushHandle::Unknown),v(),brush(0),refCount(0){}
        ~BrushData();

        inline void attach();
        inline void detach();

        MBrushHandle::Type type;
        union
        {
            unsigned int              color;
            const LinearGradientData* linearData;
            BitmapData*               imageData;
            NinePatchData*            ninePatchData;
        } v;

        unsigned int refCount;
        void*        brush;
    };
    struct NinePatchData
    {
        NinePatchData ():mainBmpBrush(0),imageRect(0),portionCreated(false) { memset(b,0,sizeof(void*)*3); }
        ~NinePatchData() { releaseBrushes(); delete imageRect; mainBmpBrush->detach(); }

        void releaseBrushes();

        void*      b[3];
        BrushData* mainBmpBrush;
        MRectU     borders;
        MRectU*    imageRect;
        bool       portionCreated;
    };

    struct ImageData
    {
        ImageData():bitmap(0),frameCount(0),refCount(0){}
        ~ImageData();

        inline void attach();
        inline void detach();

        wstring      filePath;
        unsigned int frameCount;
        unsigned int refCount;
        void*        bitmap;
    };

    struct ResourceManager
    {
        typedef unordered_map<MColor                   , BrushData*>  SolidBrushCache;
        typedef unordered_map<wstring                  , BrushData*>  BitmapBrushCache;
        typedef unordered_map<const LinearGradientData*, BrushData*>  LinearBrushCache;
        typedef unordered_multimap<wstring             , BrushData*>  NinePatchBrushCache;
        typedef unordered_map<wstring                  , ImageData*>  BitmapCache;

        SolidBrushCache     solidBrushCache;
        BitmapBrushCache    bitmapBrushCache;
        BitmapCache         bitmapCache;
        LinearBrushCache    linearBrushCache;
        NinePatchBrushCache ninePatchCache;

        BrushData* getBrush(MColor);
        BrushData* getBrush(const std::wstring&);
        BrushData* getBrush(const LinearGradientData*);
        BrushData* getBrush(const std::wstring&, const MRectU&, const MRectU*);
        ImageData* getImage(const std::wstring&);

        void discardBrushes();
        void discardImages();
    };

    static ResourceManager resManager;

    inline void BrushData::attach() { ++refCount; }
    inline void ImageData::attach() { ++refCount; }
    inline void BrushData::detach()
    { 
        if(--refCount > 0) return;
        switch(type)
        {
            case MBrushHandle::Solid:
                resManager.solidBrushCache.erase(MColor(v.color)); break;
            case MBrushHandle::Bitmap:
                resManager.bitmapBrushCache.erase(v.imageData->filePath);
                delete v.imageData;
                break;
            case MBrushHandle::NinePatch:
                {
                    typedef ResourceManager::NinePatchBrushCache::iterator It;
                    pair<It, It> result = resManager.ninePatchCache.equal_range(
                        v.ninePatchData->mainBmpBrush->v.imageData->filePath);

                    while(result.first != result.second)
                    {
                        if(result.first->second == this) {
                            resManager.ninePatchCache.erase(result.first);
                            break;
                        }
                        ++result.first;
                    }
                }
                delete v.ninePatchData;
                break;
            case MBrushHandle::LinearGradient:
                resManager.linearBrushCache.erase(v.linearData);
                break;
        }
        delete this;
    }
    inline void ImageData::detach()
    {
        if(--refCount > 0) return;
        resManager.bitmapCache.erase(filePath);
        delete this;
    }

    inline MBrushHandle::MBrushHandle(BrushData* d):data(d){ d->attach(); }
    inline MImageHandle::MImageHandle(ImageData* d):data(d){ d->attach(); }
    MBrushHandle::~MBrushHandle() { if(data) data->detach(); }
    MImageHandle::~MImageHandle() { if(data) data->detach(); }

    MBrushHandle MGraphicsResFactory::createBrush(MColor color)
    {
        BrushData*& brush = resManager.solidBrushCache[color];
        if(brush == 0)
        {
            brush          = new BrushData();
            brush->type    = MBrushHandle::Solid;
            brush->v.color = color.getARGB();
        }

        return MBrushHandle(brush);
    }

    MBrushHandle MGraphicsResFactory::createBrush(const std::wstring& imagePath)
    {
        BrushData*& brush = resManager.bitmapBrushCache[imagePath];
        if(brush == 0)
        {
            brush              = new BrushData();
            brush->type        = MBrushHandle::Bitmap;
            BitmapData* data   = new BitmapData();
            brush->v.imageData = data;
            data->filePath     = imagePath;
        }

        return MBrushHandle(brush);
    }

    MBrushHandle MGraphicsResFactory::createBrush(const LinearGradientData* linear)
    {
        if(linear->stopCount < 2)
        {
            // We cannot initialize such kind of linear gradient brush.
            // So we change the brush to solid transparent.
            return createBrush(MColor());
        }

        BrushData*& brush = resManager.linearBrushCache[linear];
        if(brush == 0)
        {
            brush               = new BrushData();
            brush->type         = MBrushHandle::LinearGradient;
            brush->v.linearData = linear;
        }

        return MBrushHandle(brush);
    }

    MBrushHandle MGraphicsResFactory::createNinePatchBrush(const std::wstring& imagePath,
        const MRectU& border, const MRectU* imageRect)
    {
        BrushData* newBrush       = new BrushData();
        NinePatchData* ninePatch  = new NinePatchData();
        MBrushHandle mainBmpBrush = createBrush(imagePath);
        mainBmpBrush.data->attach();

        ninePatch->borders        = border;
        ninePatch->mainBmpBrush   = mainBmpBrush.data;

        if(imageRect)
            ninePatch->imageRect = new MRectU(*imageRect);

        newBrush->type            = MBrushHandle::NinePatch;
        newBrush->v.ninePatchData = ninePatch;

        // Caching is only for discarding.
        resManager.ninePatchCache.insert(
            ResourceManager::NinePatchBrushCache::value_type(imagePath,newBrush));
        return newBrush;
    }

    MImageHandle MGraphicsResFactory::createImage(const std::wstring& imagePath)
    {
        ImageData*& image = resManager.bitmapCache[imagePath];
        if(image == 0)
        {
            image = new ImageData();
            image->filePath = imagePath;
        }
        return MImageHandle(image);
    }

    MBrushHandle::Type MBrushHandle::type() const
    {
        if(data) return data->type;
        return Unknown;
    }
    MRectU MBrushHandle::ninePatchBorders() const
    {
        if(data && data->type == NinePatch)
            return data->v.ninePatchData->borders;

        return MRectU();
    }
    MRectU MBrushHandle::ninePatchImageRect() const
    {
        if(data && data->type == NinePatch)
        {
            MRectU* rect = data->v.ninePatchData->imageRect;
            if(rect) return *rect;
        }
        return MRectU();
    }
    unsigned int MBrushHandle::frameCount() const
    {
        if(data == 0 || data->type != Bitmap) return 0;
        return data->v.imageData->frameCount;
    }
    unsigned int MImageHandle::frameCount() const
    {
        if(data) return data->frameCount;
        return 0;
    }

    MImageHandle::MImageHandle(const MImageHandle& rhs)
    {
        data = rhs.data;
        if(data != 0) data->attach();
    }
    MBrushHandle::MBrushHandle(const MBrushHandle& rhs)
    {
        data = rhs.data;
        if(data != 0) data->attach();
    }
    const MImageHandle& MImageHandle::operator=(const MImageHandle& rhs)
    {
        if(data) data->detach();
        if((data = rhs.data) != 0) data->attach();
        return *this;
    }
    const MBrushHandle& MBrushHandle::operator=(const MBrushHandle& rhs)
    {
        if(data) data->detach();
        if((data = rhs.data) != 0) data->attach();
        return *this;
    }





    // ====================================================
    // === Implementations for various Graphics Backend ===
    // ====================================================

    void MGraphicsResFactory::discardResources()
    {
        ResourceManager::SolidBrushCache::iterator sit    = resManager.solidBrushCache.begin();
        ResourceManager::SolidBrushCache::iterator sitEnd = resManager.solidBrushCache.end();
        ResourceManager::BitmapBrushCache::iterator bit    = resManager.bitmapBrushCache.begin();
        ResourceManager::BitmapBrushCache::iterator bitEnd = resManager.bitmapBrushCache.end();
        ResourceManager::LinearBrushCache::iterator lit    = resManager.linearBrushCache.begin();
        ResourceManager::LinearBrushCache::iterator litEnd = resManager.linearBrushCache.end();
        ResourceManager::BitmapCache::iterator it    = resManager.bitmapCache.begin();
        ResourceManager::BitmapCache::iterator itEnd = resManager.bitmapCache.end();

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                while(sit != sitEnd) {
                    BrushData* data = sit->second;
                    if(data->brush) {
                        ((ID2D1SolidColorBrush*)data->brush)->Release();
                        data->brush = 0;
                    }
                    ++sit;
                }
                while(bit != bitEnd) {
                    BrushData* data = bit->second;
                    if(data->brush) {
                        ((ID2D1BitmapBrush*)data->brush)->Release();
                        data->brush = 0;
                    }
                    ++bit;
                }
                while(lit != litEnd) {
                    BrushData* data = lit->second;
                    if(data->brush) {
                        ((ID2D1LinearGradientBrush*)data->brush)->Release();
                        data->brush = 0;
                    }
                    ++lit;
                }
                while(it != itEnd) {
                    ImageData* data = it->second;
                    if(data->bitmap) {
                        ((ID2D1Bitmap*)data->bitmap)->Release();
                        data->bitmap = 0;
                    }
                    ++it;
                }
                break;
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                while(sit != sitEnd) {
                    BrushData* data = sit->second;
                    if(data->brush) {
                        delete (MColor*)data->brush;
                        data->brush = 0;
                    }
                    ++sit;
                }
                while(bit != bitEnd) {
                    BrushData* data = bit->second;
                    if(data->brush) {
                        delete (SkBitmap*)data->brush;
                        data->brush = 0;
                    }
                    ++bit;
                }
                while(lit != litEnd) {
                    BrushData* data = lit->second;
                    if(data->brush) {
                        ((SkShader*)data->brush)->unref();
                        data->brush = 0;
                    }
                    ++lit;
                }
                while(it != itEnd) {
                    ImageData* data = it->second;
                    if(data->bitmap) {
                        delete (SkBitmap*)data->bitmap;
                        data->bitmap = 0;
                    }
                    ++it;
                }
                break;
#endif
        }

        ResourceManager::NinePatchBrushCache::iterator nit    = resManager.ninePatchCache.begin();
        ResourceManager::NinePatchBrushCache::iterator nitEnd = resManager.ninePatchCache.end();
        while(nit != nitEnd)
        {
            BrushData* data = nit->second;
            data->v.ninePatchData->releaseBrushes();
            ++nit;
        }
    }
    
    void NinePatchData::releaseBrushes()
    {
        for(int i = 0; i<3; ++i)
        {
            switch(mApp->getGraphicsBackend())
            {
#ifdef MB_USE_D2D
                case MApplication::Direct2D:
                    {
                        ID2D1BitmapBrush* brush = (ID2D1BitmapBrush*)b[i];
                        if(brush) brush->Release();
                    }
                    break;
#endif
#ifdef MB_USE_SKIA
                case MApplication::Skia:
                    // Nothing to do.
                    return;
#endif
                case MApplication::GDI:
                    break;
            }
        }
        memset(b,0,sizeof(void*)*3);
    }

    BrushData::~BrushData()
    {
        M_ASSERT(refCount == 0);
        if(brush == 0) return;

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                ((ID2D1Brush*)brush)->Release();
                break;
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                if(type == MBrushHandle::Bitmap)
                    delete (SkBitmap*)brush;
                else if(type == MBrushHandle::Solid)
                    delete (MColor*)brush;
                else if(type == MBrushHandle::LinearGradient)
                    ((SkShader*)brush)->unref();
                break;
#endif
            case MApplication::GDI:
                break;
        }
    }

    ImageData::~ImageData()
    {
        M_ASSERT(refCount == 0);
        if(bitmap == 0) return;

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                ((ID2D1Bitmap*)bitmap)->Release();
                break;
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                delete (SkBitmap*)bitmap;
                break;
#endif
            case MApplication::GDI:
                break;
        }
    }

    static void* createPortionD2D(NinePatchData* data, 
        MBrushHandle& connerBrush, MBrushHandle::NinePatchPortion p);
    static void  updateLinearGraidentD2D(void* linearBrush,
        const LinearGradientData* linearData, const MRect& drawingRect);
    static void* updateLinearGraidentSkia(void* linearBrush,
        const LinearGradientData* linear, const MRect& drawingRect);

    void MBrushHandle::updateLinearGraident(const MRect& drawingRect)
    {
        M_ASSERT(data != 0);
        M_ASSERT(data->type == LinearGradient);

        switch(mApp->getGraphicsBackend())
        {
            case MApplication::Direct2D:
                updateLinearGraidentD2D(getBrush(), data->v.linearData, drawingRect);
                break;
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                data->brush = updateLinearGraidentSkia(getBrush(),data->v.linearData,drawingRect);
                break;
#endif
        }
    }

    void* MBrushHandle::getPortion(NinePatchPortion p)
    {
        M_ASSERT(data != 0);
        M_ASSERT(data->type == NinePatch);

        NinePatchData* ninePatch = data->v.ninePatchData;
        MBrushHandle mainBrush(ninePatch->mainBmpBrush);

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                {
                    ID2D1BitmapBrush* conner = (ID2D1BitmapBrush*)mainBrush.getBrush();
                    if(p == Conner) return conner;
                    if(ninePatch->portionCreated)
                        return ninePatch->b[p];
                    else
                        return createPortionD2D(ninePatch, mainBrush, p);
                }
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                return mainBrush.getBrush();
#endif
            break;
        }

        return 0;
    }

    MSize MImageHandle::getSize()
    {
        if(data == 0) return MSize();

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                {
                    ID2D1Bitmap* bmp = (ID2D1Bitmap*)getImage();
                    D2D1_SIZE_U size = bmp->GetPixelSize();
                    return MSize(size.width,size.height);
                }
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                {
                    SkBitmap* bmp = (SkBitmap*)getImage();
                    return MSize(bmp->width(), bmp->height());
                }
#endif
        }

        return MSize();
    }

    MSize MBrushHandle::bitmapSize()
    {
        mWarning(data->type != MBrushHandle::Bitmap,
            L"[Warning]Getting bitmap size of a brush, which is not a bitmap brush.");

        if(data->type != MBrushHandle::Bitmap) return MSize();

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                {
                    ID2D1BitmapBrush* bmpBrush = (ID2D1BitmapBrush*) getBrush();
                    ID2D1Bitmap* bmp;
                    bmpBrush->GetBitmap(&bmp);

                    D2D1_SIZE_U size = bmp->GetPixelSize();
                    if(bmp) bmp->Release();

                    return MSize(size.width, size.height);
                }
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                {
                    SkBitmap* bmp = (SkBitmap*)getBrush();
                    return MSize(bmp->width(), bmp->height());
                }
#endif
        }

        return MSize();
    }

    void* createSkiaBitmap(const std::wstring& filePath, unsigned int* frameCount);

    void* MBrushHandle::getBrush()
    {
        M_ASSERT(data != 0);
        if(data->brush != 0) return data->brush;

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                {
                    ID2D1RenderTarget* rt = MD2DGraphics::getRecentRenderTarget();
                    if(type() == Solid) {
                        ID2D1SolidColorBrush** brush = (ID2D1SolidColorBrush**) &data->brush;
                        rt->CreateSolidColorBrush(MColor(data->v.color),brush);
                    } else if(type() == Bitmap) {
                        data->brush = createD2DBitmapBrush(data->v.imageData->filePath,
                            rt, &(data->v.imageData->frameCount));
                    } else if(type() == LinearGradient) {
                        data->brush = createD2DLinearGradientBrush(data->v.linearData, rt);
                    } else if(type() == NinePatch) {
                        return 0;
                    }
                }
                break;
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                if(type() == Solid)
                    data->brush = new MColor(data->v.color);
                else if(type() == Bitmap)
                    data->brush = createSkiaBitmap(data->v.imageData->filePath,&(data->v.imageData->frameCount));
                else if(type() == LinearGradient)
                {
                    const LinearGradientData* linear = data->v.linearData;
                    SkPoint   pts[2]; pts[0].set(0.f, 0.f); pts[1].set(1.f, 0.f);
                    SkColor*  colors = new SkColor[linear->stopCount];
                    SkScalar* pos    = new SkScalar[linear->stopCount];

                    for(int i = 0; i < linear->stopCount; ++i) {
                        colors[i] = linear->stops[i].argb;
                        pos[i]    = linear->stops[i].pos;
                    }

                    data->brush = SkGradientShader::CreateLinear(pts, colors, pos,
                        linear->stopCount, SkShader::kClamp_TileMode);
                    delete[] colors;
                    delete[] pos;
                }
                else if(type() == NinePatch)
                    return 0;
                break;
#endif
        }

        if(data->brush == 0 && type() != Solid)
        {
            // If we still have a null brush here, we change it to solid brush
            // So that the user can still draw.
            data->type = Solid;
            return getBrush();
        }

        return data->brush;
    }

    void* MImageHandle::getImage()
    {
        M_ASSERT(data != 0);
        if(data->bitmap != 0) return data->bitmap;

        switch(mApp->getGraphicsBackend())
        {
#ifdef MB_USE_D2D
            case MApplication::Direct2D:
                data->bitmap = createD2DBitmap(data->filePath,
                    MD2DGraphics::getRecentRenderTarget(), &data->frameCount);
                break;
#endif
#ifdef MB_USE_SKIA
            case MApplication::Skia:
                data->bitmap = createSkiaBitmap(data->filePath, &data->frameCount);
                break;
#endif
        }
        return data->bitmap;
    }

    // ==============================
    static void updateLinearGraidentD2D(void* linearBrush,
        const LinearGradientData* linear, const MRect& drawingRect)
    {
#ifdef MB_USE_D2D
        ID2D1LinearGradientBrush* brush  = (ID2D1LinearGradientBrush*)linearBrush;

        FLOAT width  = FLOAT(drawingRect.right  - drawingRect.left);
        FLOAT height = FLOAT(drawingRect.bottom - drawingRect.top);

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
#endif
    }

    static void* updateLinearGraidentSkia(void* linearBrush,
        const LinearGradientData* linear, const MRect& drawingRect)
    {
#ifdef MB_USE_SKIA
        SkShader* brush = (SkShader*)linearBrush;

        // Transform
        // Calc the positions.
        int startX = linear->isPercentagePos(LinearGradientData::StartX) ?
                     drawingRect.width() * linear->getPosValue(LinearGradientData::StartX) / 100 :
                     linear->getPosValue(LinearGradientData::StartX);
        int startY = linear->isPercentagePos(LinearGradientData::StartY) ?
                     drawingRect.width() * linear->getPosValue(LinearGradientData::StartY) / 100 :
                     linear->getPosValue(LinearGradientData::StartY);
        startX += drawingRect.left;
        startY += drawingRect.top;

        int endX = linear->isPercentagePos(LinearGradientData::EndX) ?
                   drawingRect.width() * linear->getPosValue(LinearGradientData::EndX) / 100 :
                   linear->getPosValue(LinearGradientData::EndX);
        int endY = linear->isPercentagePos(LinearGradientData::EndY) ?
                   drawingRect.width() * linear->getPosValue(LinearGradientData::EndY) / 100 :
                   linear->getPosValue(LinearGradientData::EndY);
        endX += drawingRect.left;
        endY += drawingRect.top;

        // Calc the matrix
        SkMatrix matrix; matrix.setTranslate((SkScalar)startX, (SkScalar)startY);
        matrix.preScale((SkScalar)drawingRect.width(), (SkScalar)drawingRect.height());

        int dx = endX - startX;
        int dy = endY - startY;
        int length;
        if(dx == 0) length = abs(endY - startY); else
        if(dy == 0) length = abs(endX - startX); else
        { length = (int)sqrt(float(dx*dx + dy*dy)); }
        if(length == 0) length = 1;

        SkMatrix rotate; rotate.setSinCos(((SkScalar)dy) / length, ((SkScalar)dx) / length);
        matrix.preConcat(rotate);
        brush->setLocalMatrix(matrix);
        return brush;
#else
        return 0;
#endif
    }

    static void* createPortionD2D(NinePatchData* ninePatch, 
        MBrushHandle& connerBrush, MBrushHandle::NinePatchPortion p)
    {
#ifdef MB_USE_D2D
        ID2D1BitmapBrush* conner = (ID2D1BitmapBrush*)connerBrush.getBrush();
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
        ID2D1RenderTarget*       workingRT = MD2DGraphics::getRecentRenderTarget();

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

            ninePatch->b[MBrushHandle::VerCenter] = portionBrush;
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

            ninePatch->b[MBrushHandle::HorCenter] = portionBrush;
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

            ninePatch->b[MBrushHandle::Center] = portionBrush;
            intermediateRT->Release();
            portionBitmap->Release();
        }

        SafeRelease(bitmap);
        return ninePatch->b[p];
#else
        return 0;
#endif
    }

    void* createSkiaBitmap(const std::wstring& filePath, unsigned int* frameCount)
    {
#ifdef MB_USE_SKIA
        SkBitmap* bmp = new SkBitmap();
        IWICImagingFactory*    factory    = mApp->getWICImagingFactory();
        IWICBitmapDecoder*     decoder    = 0;
        IWICBitmapFrameDecode* frame	  = 0;

        // Create Decoder.
        if(filePath.at(0) == L':')  // Image file is inside MResources.
        {
            MResource res;
            if(res.open(filePath))
            {
                IWICStream* stream = 0;
                if(FAILED(factory->CreateStream(&stream))) goto END;
                if(FAILED(stream->InitializeFromMemory((BYTE*)res.byteBuffer(),res.length())))
                {
                    SafeRelease(stream);
                    goto END;
                }
                HRESULT hr = factory->CreateDecoderFromStream(stream,
                    NULL,WICDecodeMetadataCacheOnDemand,&decoder);
                SafeRelease(stream);
                if(FAILED(hr)) goto END;
            } else { goto END; }
        } else {
            if(FAILED( factory->CreateDecoderFromFilename(filePath.c_str(),NULL,
                GENERIC_READ, WICDecodeMetadataCacheOnDemand,&decoder) ))
                goto END;
        }

        if(FAILED(decoder->GetFrameCount(frameCount))) { goto END; }
        if(FAILED(decoder->GetFrame(0, &frame))) { goto END; }

        UINT width, height;
        if(FAILED(frame->GetSize(&width, &height))) { goto END; }

        bmp->setConfig(SkBitmap::kARGB_8888_Config, width, height * (*frameCount));
        bmp->allocPixels();

        for(size_t i = 0; i < *frameCount; ++i)
        {
            if(i != 0) decoder->GetFrame(i, &frame);
            IWICFormatConverter* converter  = 0;
            factory->CreateFormatConverter(&converter);
            converter->Initialize(frame,
                GUID_WICPixelFormat32bppPBGRA,
                WICBitmapDitherTypeNone,NULL,
                0.f,WICBitmapPaletteTypeCustom);

            IWICBitmapSource* convertedBMP;
            converter->QueryInterface(IID_PPV_ARGS(&convertedBMP));

            bmp->lockPixels();
            bmp->eraseColor(0);
            const int stride = bmp->rowBytes();
            convertedBMP->CopyPixels(NULL, stride, stride * height,
                reinterpret_cast<BYTE*>(bmp->getPixels()) + bmp->rowBytes() * height * i );
            bmp->unlockPixels();

            SafeRelease(frame);
            SafeRelease(convertedBMP);
            SafeRelease(converter);
        }

        END:
            SafeRelease(decoder);

        return bmp;
#else
        return 0;
#endif
    }
}
