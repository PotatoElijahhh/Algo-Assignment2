#include "seam/energy.hpp"

#include <algorithm>
#include <cmath>

namespace seam {

namespace {

// 3x3 Sobel kernels for the horizontal (gx) and vertical (gy) gradients.
const int kSobelX[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
const int kSobelY[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};

Matrix toGrayscale(const Image& img, int threads) {
    Matrix gray(img.height, img.width);

    // MULTITHREADING: each thread converts a different set of rows.
    // (The pragma is ignored when OpenMP is off, and `if` keeps one thread when threads == 1.)
#ifdef _OPENMP
#pragma omp parallel for num_threads(threads) if (threads > 1)
#endif
    for (int row = 0; row < img.height; ++row) {
        for (int col = 0; col < img.width; ++col) {
            const std::uint8_t* p = img.pixel(row, col);
            gray(row, col) = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
        }
    }
    return gray;
}

} // namespace

Matrix computeEnergy(const Image& img, int threads) {
    if (threads < 1) {
        threads = 1;
    }
    const Matrix gray = toGrayscale(img, threads);
    Matrix energy(img.height, img.width);

    // MULTITHREADING: each thread computes the energy of a different set of rows.
    // Safe because every thread only reads `gray` and writes its own rows of `energy`.
#ifdef _OPENMP
#pragma omp parallel for num_threads(threads) if (threads > 1)
#endif
    for (int row = 0; row < img.height; ++row) {
        for (int col = 0; col < img.width; ++col) {
            double gx = 0.0;
            double gy = 0.0;
            for (int dr = -1; dr <= 1; ++dr) {
                const int r = std::clamp(row + dr, 0, img.height - 1);
                for (int dc = -1; dc <= 1; ++dc) {
                    const int c = std::clamp(col + dc, 0, img.width - 1);
                    const double v = gray(r, c);
                    gx += kSobelX[dr + 1][dc + 1] * v;
                    gy += kSobelY[dr + 1][dc + 1] * v;
                }
            }
            energy(row, col) = std::sqrt(gx * gx + gy * gy);
        }
    }
    return energy;
}

} // namespace seam
