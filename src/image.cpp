// Must come before any standard header so MSVC accepts stb's use of sprintf/fopen.
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "seam/image.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace seam {

namespace {

std::string lowerExtension(const std::string& path) {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }
    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

} // namespace

Image loadImage(const std::string& path) {
    int w = 0;
    int h = 0;
    int channelsInFile = 0;
    unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channelsInFile, 3);
    if (pixels == nullptr) {
        throw std::runtime_error("Could not load image '" + path + "': " + stbi_failure_reason());
    }

    Image img(w, h);
    std::memcpy(img.data.data(), pixels, img.data.size());
    stbi_image_free(pixels);
    return img;
}

void saveImage(const Image& img, const std::string& path) {
    const std::string ext = lowerExtension(path);
    const int stride = img.width * 3;
    int ok = 0;

    if (ext == "png") {
        ok = stbi_write_png(path.c_str(), img.width, img.height, 3, img.data.data(), stride);
    } else if (ext == "jpg" || ext == "jpeg") {
        ok = stbi_write_jpg(path.c_str(), img.width, img.height, 3, img.data.data(), 95);
    } else if (ext == "bmp") {
        ok = stbi_write_bmp(path.c_str(), img.width, img.height, 3, img.data.data());
    } else {
        throw std::runtime_error("Unsupported output format '" + ext + "' (use png, jpg or bmp)");
    }

    if (ok == 0) {
        throw std::runtime_error("Could not write image '" + path + "'");
    }
}

Image transpose(const Image& img) {
    Image out(img.height, img.width);
    for (int row = 0; row < img.height; ++row) {
        for (int col = 0; col < img.width; ++col) {
            const std::uint8_t* src = img.pixel(row, col);
            std::uint8_t* dst = out.pixel(col, row);
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        }
    }
    return out;
}

} // namespace seam
