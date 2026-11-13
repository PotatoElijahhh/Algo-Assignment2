#include "seam/visualise.hpp"

#include <stdexcept>

namespace seam {

namespace {

// Sets one pixel to the given colour.
void setPixel(Image& img, int row, int col, Colour colour) {
    std::uint8_t* p = img.pixel(row, col);
    p[0] = colour.r;
    p[1] = colour.g;
    p[2] = colour.b;
}

} // namespace

void paintSeam(Image& img, const Seam& seam, Orientation orientation, Colour colour) {
    if (orientation == Orientation::Vertical) {
        // One entry per row, each a column in [0, width).
        if (!isValidSeam(seam, img.height, img.width)) {
            throw std::invalid_argument("paintSeam: vertical seam does not fit the image");
        }
        for (int row = 0; row < img.height; ++row) {
            setPixel(img, row, seam[row], colour);
        }
    } else {
        // A horizontal seam is a vertical seam of the transposed image, so the same
        // validity check works with rows and columns swapped: one entry per column,
        // each a row in [0, height).
        if (!isValidSeam(seam, img.width, img.height)) {
            throw std::invalid_argument("paintSeam: horizontal seam does not fit the image");
        }
        for (int col = 0; col < img.width; ++col) {
            setPixel(img, seam[col], col, colour);
        }
    }
}

} // namespace seam
