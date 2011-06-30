#pragma once
#include "MBGlobal.h"
#include "MUtils.h"

#include <string>

namespace MetalBone
{
    struct BrushData;
    struct ImageData;
    struct LinearGradientData
    {
        enum PosEnum { StartX, StartY, EndX, EndY };

        inline LinearGradientData ();
        inline ~LinearGradientData();

        // Return true if the position's value is percentage.
        inline bool isPercentagePos(PosEnum e) const;
        inline int  getPosValue    (PosEnum e) const;
        inline void setPosType     (PosEnum e, bool isPercentage);

        struct GradientStop { int argb; float pos; };
        GradientStop* stops;
        int           stopCount;
        int           pos[4];
        char          posType;
    };

    class METALBONE_EXPORT MBrushHandle
    {
        public:
            enum Type {  Unknown, Solid, LinearGradient, Bitmap, NinePatch };

            // Notes for Direct2D, the 9-Patch Brush consist of four brushes.
            // The "Conner" contains the whole image. It's used to draw the 4 conners.
            // The "Center" is the center of the image. It will be
            // repeat/strech horizontally and vertically.
            // The "HorCenter" contains the center-left and center-right,
            // they're horizontally layout.
            // The "VerCenter" contains the top-center and bottom-center,
            // they're vertically layout.
            // 
            // The whole image:   The "Center":   The HorCenter:   The VerCenter:
            // ---------------     ---------        ---------        ---------
            // | 0 |  1  | 2 |     |       |        |   |   |        |   1   | 
            // |-------------|     |   4   |        | 3 | 5 |        ---------
            // |   |     |   |     ---------        ---------        |   7   |
            // | 3 |  4  | 5 |                                       ---------
            // |-------------|
            // | 6 |  7  | 8 |
            // ---------------
            // 
            // Notes for Skia, the 9-Patch Brush consist of only one brush (i.e. the
            // whole image). So calling getPortion will return the same value of
            // a brush, regardless of which portion it is.
            enum NinePatchPortion { VerCenter, HorCenter, Center, Conner };

            // The return value of getBrush() will always be 0, if the brush
            // is 9-patch (use getPortion() to get the 9-patch brush instead).
            // You should type-cast the return void* to something else.
            //
            // Notes for Direct2D, it is only safe to get the actual brush
            // during window redrawing. Because we need a ID2D1RenderTarget to
            // create the brush. Other Graphics Backend may not be restricted
            // at this point.
            // 
            // What getBrush() returns;
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-++-+-+-+-+-+-+     
            // |  BrushType  |         Solid        |     LinearGradient      |     Bitamp      | NinePatch |
            // +--------------------------------------------------------------------------------------------+
            // |  Direct2D   |ID2D1SolidColorBrush* |ID2D1LinearGradientBrush*|ID2D1BitmapBrush*|     0     |
            // +--------------------------------------------------------------------------------------------+
            // |    Skia     |        MColor*       |        SkShader*        |     SkBitmap*   |     0     |
            // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-
            void* getBrush();
            void* getPortion(NinePatchPortion);
            
            // Notes for Skia, we only create one SkShader for one MBrushHandle.
            // After calling updateLinearGradient(), the all we changed is the 
            // matrix applied to the SkShader.
            void updateLinearGraident(const MRect& drawingRect);

            Type type() const;

            // *Only valid for BitmapBrushes.
            // bitmapSize() returns the size of the bitmap.
            // frameCount() returns a non-zero value to indicate how much frames
            // stored inside this single brush. Frames are layouted vertically.
            // Getting the size of the bitmap may results in creating the brush.
            MSize        bitmapSize();
            unsigned int frameCount() const;

            // *Only valid for NinePatchBrushes.
            // ninePatchBorders() returns the border of the Nine-Patch brush.
            // ninePatchImageRect() returns the "imageRect" when creating the brush(see createNinePatch()), 
            // or empty rect(i.e.MRectU(0,0,0,0)).
            MRectU ninePatchBorders()   const;
            MRectU ninePatchImageRect() const;

            inline MBrushHandle();
            ~MBrushHandle();
            MBrushHandle(const MBrushHandle&);

            inline operator bool() const;

            const MBrushHandle& operator= (const MBrushHandle&);
            inline bool         operator==(const MBrushHandle&);

        private:
            inline MBrushHandle(BrushData*);
            BrushData* data;

            friend class MGraphicsResFactory;
    };

    class METALBONE_EXPORT MImageHandle
    {
        public:
            // Return a non-zero value to indicate how much frames stored inside this single image.
            // Frames are layouted vertically.
            unsigned int frameCount() const;

            // 1. When using Direct2D as Graphics Backend, getting the actual ID2D1Bitmap
            // relies on MD2DPaintContext::getRenderTarget(). That is, it is only safe
            // to get the actual image during window redrawing. 
            // 2. You should type-cast the return value to something else.
            // For example:
            // If you're using Direct2D as Grahphics Backend.
            // You can do this: (ID2D1Bitmap*) myBmp.getImage();
            // If you're using Skia.
            // You can do this: (SkBitmap*) myBmp.getImage();
            void* getImage();

            // If the image has not been created, calling this fuction will create the image.
            MSize getSize();

            inline MImageHandle();
            ~MImageHandle();
            MImageHandle (const MImageHandle&);

            inline operator bool() const;

            const MImageHandle& operator= (const MImageHandle&);
            inline bool         operator==(const MImageHandle&);

        private:
            inline MImageHandle(ImageData*);
            ImageData* data;

            friend class MGraphicsResFactory;
    };

    class METALBONE_EXPORT MGraphicsResFactory
    {
        public:
            // The newly created brush will be shared if they are identical.
            // For example, creating two MBrushHandle with the same color
            // results in only creating one ID2D1Brush or GDI brush.
            static MBrushHandle createBrush(MColor color);
            static MBrushHandle createBrush(const std::wstring& imagePath);
            // The LinearGradientData object must remain valid until 
            // all the copy of this MD2DBrushHandle is destory.
            // Because we will need the LinearGradientData to recreate the brush.
            // Passing the same LinearGradientData* will resulting creating shared brushes.
            static MBrushHandle createBrush(const LinearGradientData* linearGradientData);
            // Newly created NinePatchBrush can not be shared even if they're identical.
            // So prefer copying the MBrushHandle instead of calling createNinePatch().
            // If the image is an atlas, pass in imageRect to specify which portion of this atlas
            // will be used to create the ninePatch brush.
            static MBrushHandle createNinePatchBrush(const std::wstring& imagePath,
                const MRectU& borderWidths, const MRectU* imageRect = 0);

            // The image will be shared if they're identical.
            static MImageHandle createImage(const std::wstring& imagePath);

            // Calling discardResources() will clear up all brushes and images. All brushes will be automatically
            // recreated next time they're used.
            // It's useful in Direct2D, when the device losts, every resources have to be recreated.
            static void discardResources();
    };



    inline LinearGradientData::LinearGradientData():
        stops(0),stopCount(0),posType(0) { memset(pos,0,sizeof(int)*4); }
    inline LinearGradientData::~LinearGradientData() { delete[] stops; }

    inline bool LinearGradientData::isPercentagePos(PosEnum e) const { return (posType & (1 << e)) != 0; }
    inline int  LinearGradientData::getPosValue    (PosEnum e) const { return pos[e]; }
    inline void LinearGradientData::setPosType     (PosEnum e, bool isPercentage)
    {
        if(isPercentage) { posType |= (1 << e); }
        else { posType &= ~(1 << e); }
    }

    inline MBrushHandle::MBrushHandle():data(0){}
    inline MImageHandle::MImageHandle():data(0){}
    inline bool MBrushHandle::operator==(const MBrushHandle& rhs) { return rhs.data == data; }
    inline bool MImageHandle::operator==(const MImageHandle& rhs) { return rhs.data == data; }
    inline MBrushHandle::operator bool() const { return data !=0; }
    inline MImageHandle::operator bool() const { return data !=0; }
}