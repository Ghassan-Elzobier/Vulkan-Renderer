#include "image/compare.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

ImageDiff compareImages(const Image& a, const Image& b) {
    if (a.width != b.width || a.height != b.height) {
        throw std::runtime_error("Sizes differ: '" + std::to_string(a.width) + "x" + std::to_string(a.height) +
                                 "' against '" + std::to_string(b.width) + "x" + std::to_string(b.height) + "'");
    }
    ImageDiff diff{};
    diff.pixels = uint64_t(a.width) * a.height;
    double sumSquares = 0.0;
    for (uint64_t p = 0; p < diff.pixels; p++) {
        bool differs = false;
        for (int c = 0; c < 3; c++) {
            const int d = std::abs(int(a.rgb[p * 3 + c]) - int(b.rgb[p * 3 + c]));
            sumSquares += double(d) * d;
            diff.maxDiff = std::max(diff.maxDiff, d);
            differs = differs || d != 0;
        }
        diff.pixelsDiffering += differs ? 1 : 0;
    }
    // Mean over every channel value, then the square root brings it back to 0-255 units.
    diff.rmse = std::sqrt(sumSquares / double(diff.pixels * 3));
    return diff;
}

Image diffImage(const Image& a, const Image& b, int gain) {
    compareImages(a, b); // checks the sizes
    Image out{a.width, a.height, std::vector<uint8_t>(a.rgb.size())};
    for (size_t p = 0; p < size_t(a.width) * a.height; p++) {
        int d = 0;
        for (int c = 0; c < 3; c++) {
            d = std::max(d, std::abs(int(a.rgb[p * 3 + c]) - int(b.rgb[p * 3 + c])));
        }
        const uint8_t v = uint8_t(std::min(255, d * gain));
        out.rgb[p * 3 + 0] = out.rgb[p * 3 + 1] = out.rgb[p * 3 + 2] = v;
    }
    return out;
}
