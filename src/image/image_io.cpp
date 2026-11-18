#include "image/image_io.h"

#include <cstring>
#include <fstream>
#include <stdexcept>

// stb is header-only: exactly one .cpp file defines these to compile the implementation.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_PNM
#include "stb_image.h"

namespace {

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Binary PPM (P6): a short text header, then 3 bytes per pixel.
void writePpm(const std::string& path, const Image& image) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open '" + path + "' for writing");
    }
    file << "P6\n" << image.width << " " << image.height << "\n255\n";
    file.write(reinterpret_cast<const char*>(image.rgb.data()), std::streamsize(image.rgb.size()));
}

} // namespace

Image fromRgba(const uint8_t* rgba, uint32_t width, uint32_t height) {
    Image image{width, height, std::vector<uint8_t>(size_t(width) * height * 3)};
    for (size_t i = 0; i < size_t(width) * height; i++) {
        std::memcpy(&image.rgb[i * 3], &rgba[i * 4], 3);
    }
    return image;
}

void saveImage(const std::string& path, const Image& image) {
    if (endsWith(path, ".ppm")) {
        writePpm(path, image);
    } else if (endsWith(path, ".png")) {
        const int stride = int(image.width) * 3;
        if (!stbi_write_png(path.c_str(), int(image.width), int(image.height), 3, image.rgb.data(), stride)) {
            throw std::runtime_error("Failed to write '" + path + "'");
        }
    } else {
        throw std::runtime_error("Output '" + path + "' must end in .png or .ppm");
    }
}

Image loadImage(const std::string& path) {
    int w = 0, h = 0, channelsInFile = 0;
    // Asking for 3 channels makes stb convert grey or RGBA files to RGB for us.
    uint8_t* pixels = stbi_load(path.c_str(), &w, &h, &channelsInFile, 3);
    if (!pixels) {
        throw std::runtime_error("Failed to read '" + path + "': " + stbi_failure_reason());
    }
    Image image{uint32_t(w), uint32_t(h), std::vector<uint8_t>(pixels, pixels + size_t(w) * h * 3)};
    stbi_image_free(pixels);
    return image;
}
