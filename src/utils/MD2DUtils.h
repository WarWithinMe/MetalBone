#pragma once
#include "MBGlobal.h"

#ifdef MB_USE_D2D
#  include <d2d1.h>
#  include <string>

namespace MetalBone
{
    struct LinearGradientData;
    // When creating a image which has more than one frame.
    // Its frames will be layouted vertically. So the created
    // image's height is the original image's height x frameCount.
    METALBONE_EXPORT ID2D1Bitmap* createD2DBitmap(const std::wstring& imagePath,
        ID2D1RenderTarget*, unsigned int* frameCount = 0, bool forceOneFrame = false);
    METALBONE_EXPORT ID2D1BitmapBrush* createD2DBitmapBrush(const std::wstring& imagePath,
        ID2D1RenderTarget*, unsigned int* frameCount = 0, bool forceOneFrame = false);
    METALBONE_EXPORT ID2D1LinearGradientBrush* createD2DLinearGradientBrush(const LinearGradientData*,
        ID2D1RenderTarget*);
}

#endif
