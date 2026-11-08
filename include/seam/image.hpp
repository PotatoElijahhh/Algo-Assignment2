#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace seam {

// 8-bit RGB image stored row-major.
// Pixel (row, col) occupies data[3 * (row * width + col)] .. data[3 * (row * width + col) + 2].
struct Image {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> data;

    Image() = default;
    Image(int w, int h) : width(w), height(h), data(static_cast<std::size_t>(w) * h * 3, 0) {}

    std::uint8_t* pixel(int row, int col) {
        return &data[3 * (static_cast<std::size_t>(row) * width + col)];
    }
    const std::uint8_t* pixel(int row, int col) const {
        return &data[3 * (static_cast<std::size_t>(row) * width + col)];
    }
};

// Loads PNG, JPG, BMP, etc. as 3-channel RGB. Throws std::runtime_error on failure.
Image loadImage(const std::string& path);

// Saves as PNG, JPG or BMP depending on the file extension. Throws std::runtime_error on failure.
void saveImage(const Image& img, const std::string& path);

// Swaps rows and columns. Horizontal seams are found as vertical seams of the transposed image.
Image transpose(const Image& img);

} // namespace seam
