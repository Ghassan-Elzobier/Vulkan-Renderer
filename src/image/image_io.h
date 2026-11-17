// Saving and loading 8-bit RGB images, with no Vulkan in sight, so the harness and the
// compare tool share one copy. PNG goes through stb_image_write; loading reads PNG or PPM.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Image {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgb; // 3 bytes per pixel, top row first
};

// Drops the alpha byte of each pixel: the GPU's output image is RGBA8.
Image fromRgba(const uint8_t* rgba, uint32_t width, uint32_t height);

// Writes a PNG or a binary PPM, chosen by the file's extension (.png or .ppm).
void saveImage(const std::string& path, const Image& image);

// Reads a PNG or PPM into RGB, whatever channels the file has.
Image loadImage(const std::string& path);
