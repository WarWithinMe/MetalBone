#pragma once
#include "MBGlobal.h"
#include "MUtils.h"

#include "d2d1.h"
#include <string>

namespace MetalBone
{
    struct D2DBrushData;
    struct D2DImageData;
    struct LinearGradientData
    {
        enum PosEnum { StartX, StartY, EndX, EndY };
        inline LinearGradientData();

        inline ~LinearGradientData();
        inline bool isPercentagePos(PosEnum e) const;
        inline int  getPosValue(PosEnum e) const;
        inline void setPosType(PosEnum e, bool isPercentage);

        struct GradientStop { int argb; float pos; };
        GradientStop* stops;
        int           stopCount;
        int           pos[4];
        char          posType;
    };

    // When creating a image which has more than one frame.
    // Its frames will be layouted vertically. So the created
    // image's height is the original image's height x frameCount.
    METALBONE_EXPORT ID2D1Bitmap* createD2DBitmap(const std::wstring& imagePath,
        ID2D1RenderTarget*, unsigned int* frameCount = 0);
    METALBONE_EXPORT ID2D1BitmapBrush* createD2DBitmapBrush(const std::wstring& imagePath,
        ID2D1RenderTarget*, unsigned int* frameCount = 0);
    METALBONE_EXPORT ID2D1LinearGradientBrush* createD2DLinearGradientBrush(const LinearGradientData*,
        ID2D1RenderTarget*);

    class METALBONE_EXPORT MD2DBrushHandle
    {
        public:
            enum Type { 
                Unknown, Solid, LinearGradient, Bitmap, NinePatch
            };

            // The 9-Patch Brush consist of four D2D brushes.
            // The "Conner" is the one containing the whole image,
            // which is to be drawn the conner.
            // The "Center" is the center of the image. It will be
            // repeat/strech horizontally and vertically.
            // The "HorCenter" contains the center-left and center-right,
            // they're horizontally layout.
            // The "VerCenter" contains the top-center and bottom-center,
            // they're vertically layout.
            enum NinePatchPortion {
                VerCenter = 0,
                HorCenter,
                Center,
                Conner
            };

            // The newly created brush will be shared if they are identical.
            // For example, creating two MD2DBrushHandle with the same color
            // results in only creating one ID2D1Brush.
            static MD2DBrushHandle create(MColor color);
            static MD2DBrushHandle create(const std::wstring& imagePath);
            // The LinearGradientData object must remain valid until 
            // all the copy of this MD2DBrushHandle is destory.
            // Because we will need the LinearGradientData to recreate the brush.
            static MD2DBrushHandle create(const LinearGradientData* linearGradientData);
            // Newly created NinePatchBrush can not be shared even if they're identical.
            // So prefer copying the MD2DBrushHandle instead of calling createNinePatch() 
            static MD2DBrushHandle createNinePatch(const std::wstring& imagePath, const MRectU& borderWidths,
                const MRectU* imageRect);

            // If the device losts, call this function to discard all MD2DBrushHandle,
            // so that they can be recreated during their next use.
            static void discardBrushes();

            // Getting the actual ID2D1Brush relies on MD2DPaintContext::getRenderTarget()
            // That is, it is only safe to get the actual brush during window redrawing. 
            // The return value will always be 0, if the brush is 9-patch
            // (use getPortion() to get the 9-patch brush instead).
            operator ID2D1Brush*();
            ID2D1Brush*               getBrush();

            ID2D1BitmapBrush*         getPortion(NinePatchPortion);
            ID2D1LinearGradientBrush* getLinearGraidentBrush(const D2D1_RECT_F* drawingRect);

            Type         type()               const;
            unsigned int frameCount()         const;

            // *Only valid for NinePatchBrushes.
            // ninePatchBorders() returns the border of the Nine-Patch brush.
            // ninePatchImageRect() returns the rect of the image or empty rect.
            // When creating a NinePatch Brush, it's possible to use part of a 
            // big image (by specifying the rect of the image).
            MRectU       ninePatchBorders()   const;
            MRectU       ninePatchImageRect() const;

            inline MD2DBrushHandle();
            ~MD2DBrushHandle();
            MD2DBrushHandle(const MD2DBrushHandle&);
            const MD2DBrushHandle& operator=(const MD2DBrushHandle&);

        private:
            inline MD2DBrushHandle(D2DBrushData*);
            D2DBrushData* data;
    };

    class METALBONE_EXPORT MD2DImageHandle
    {
        public:
            // The image will be shared if they're identical.
            static MD2DImageHandle create(const std::wstring& imagePath);
            // If the device losts, call this function to discard all MD2DImageHandle,
            // so that they can be recreated during their next use.
            static void discardImages();

            unsigned int frameCount() const;

            // Getting the actual ID2D1Bitmap relies on MD2DPaintContext::getRenderTarget()
            // That is, it is only safe to get the actual image during window redrawing. 
            ID2D1Bitmap* getImage();
            operator ID2D1Bitmap*();

            inline MD2DImageHandle();
            ~MD2DImageHandle();
            MD2DImageHandle(const MD2DImageHandle&);
            const MD2DImageHandle& operator=(const MD2DImageHandle&);

        private:
            inline MD2DImageHandle(D2DImageData*);
            D2DImageData* data;
    };



    inline LinearGradientData::LinearGradientData():stops(0),stopCount(0),posType(0)
        { memset(pos,0,sizeof(int)*4); }
    inline LinearGradientData::~LinearGradientData() { delete[] stops; }
    inline bool LinearGradientData::isPercentagePos(PosEnum e) const { return (posType & (1 << e)) != 0; }
    inline int  LinearGradientData::getPosValue(PosEnum e)     const { return pos[e]; }
    inline void LinearGradientData::setPosType(PosEnum e, bool isPercentage)
    {
        if(isPercentage) { posType |= (1 << e); }
        else { posType &= ~(1 << e); }
    }
    inline MD2DBrushHandle::MD2DBrushHandle():data(0){}
    inline MD2DImageHandle::MD2DImageHandle():data(0){}
}