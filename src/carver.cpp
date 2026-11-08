#include "seam/carver.hpp"

#include "seam/energy.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace seam {

void removeVerticalSeam(Image& img, const Seam& seam) {
    if (img.width <= 1) {
        throw std::invalid_argument("removeVerticalSeam: image is only one pixel wide");
    }
    if (!isValidSeam(seam, img.height, img.width)) {
        throw std::invalid_argument("removeVerticalSeam: seam does not fit the image");
    }

    Image out(img.width - 1, img.height);
    for (int row = 0; row < img.height; ++row) {
        int outCol = 0;
        for (int col = 0; col < img.width; ++col) {
            if (col == seam[row]) {
                continue;
            }
            const std::uint8_t* src = img.pixel(row, col);
            std::uint8_t* dst = out.pixel(row, outCol);
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            ++outCol;
        }
    }
    img = std::move(out);
}

namespace {

// Removes vertical seams from img until it is targetWidth wide.
// When `transposed` is true, img is the transpose of the real image, so the
// callback gets the image transposed back and the seam labelled horizontal.
void removeSeamsUntil(Image& img, int targetWidth, Method method, const SeamCallback& onSeam,
                      bool transposed, int threads) {
    while (img.width > targetWidth) {
        const Matrix energy = computeEnergy(img, threads);
        const Seam seam = findVerticalSeam(energy, method);
        if (onSeam) {
            if (transposed) {
                onSeam(transpose(img), seam, Orientation::Horizontal);
            } else {
                onSeam(img, seam, Orientation::Vertical);
            }
        }
        removeVerticalSeam(img, seam);
    }
}

} // namespace

Image carve(const Image& input, int targetWidth, int targetHeight, Method method,
            const SeamCallback& onSeam, int threads) {
    if (targetWidth < 1 || targetWidth > input.width || targetHeight < 1 || targetHeight > input.height) {
        throw std::invalid_argument("carve: target size " + std::to_string(targetWidth) + "x" +
                                    std::to_string(targetHeight) + " must be between 1x1 and " +
                                    std::to_string(input.width) + "x" + std::to_string(input.height));
    }

    Image img = input;
    removeSeamsUntil(img, targetWidth, method, onSeam, false, threads);

    if (img.height > targetHeight) {
        img = transpose(img);
        removeSeamsUntil(img, targetHeight, method, onSeam, true, threads);
        img = transpose(img);
    }
    return img;
}

} // namespace seam
