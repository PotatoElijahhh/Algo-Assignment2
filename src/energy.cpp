#include "seam/energy.hpp"

#include <algorithm>
#include <cmath>

namespace seam {

namespace {

Matrix toGrayscale(const Image& img) {
    Matrix gray(img.height, img.width);
    for (int row = 0; row < img.height; ++row) {
        for (int col = 0; col < img.width; ++col) {
            const std::uint8_t* p = img.pixel(row, col);
            gray(row, col) = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
        }
    }
    return gray;
}

} // namespace

Matrix computeEnergy(const Image& img) {
    const Matrix gray = toGrayscale(img);
    Matrix energy(img.height, img.width);

    // 3x3 Sobel kernels for the horizontal (gx) and vertical (gy) gradients.
    static const int kx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
    static const int ky[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

    for (int row = 0; row < img.height; ++row) {
        for (int col = 0; col < img.width; ++col) {
            double gx = 0.0;
            double gy = 0.0;
            for (int dr = -1; dr <= 1; ++dr) {
                const int r = std::clamp(row + dr, 0, img.height - 1);
                for (int dc = -1; dc <= 1; ++dc) {
                    const int c = std::clamp(col + dc, 0, img.width - 1);
                    const double v = gray(r, c);
                    gx += kx[dr + 1][dc + 1] * v;
                    gy += ky[dr + 1][dc + 1] * v;
                }
            }
            energy(row, col) = std::sqrt(gx * gx + gy * gy);
        }
    }
    return energy;
}

} // namespace seam
