// Comparing two images of the same size, pixel by pixel.
#pragma once

#include "image/image_io.h"

#include <cstdint>

struct ImageDiff {
    double rmse = 0.0;        // root mean square error over every R, G and B value, in 0-255 units
    int maxDiff = 0;          // the largest difference of any one channel, 0-255
    uint64_t pixelsDiffering = 0; // pixels where any channel differs at all
    uint64_t pixels = 0;
};

// Throws if the sizes differ: images of different sizes have no pixel-by-pixel answer.
ImageDiff compareImages(const Image& a, const Image& b);

// A grey image where each pixel's brightness is that pixel's largest channel difference,
// scaled by `gain` so small differences are visible.
Image diffImage(const Image& a, const Image& b, int gain);
