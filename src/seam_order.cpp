#include "seam/seam_order.hpp"

#include <algorithm>
#include <stdexcept>

namespace seam {

namespace {

// Swaps rows and columns of an order map (like transpose() does for images).
SeamOrder transposeOrder(const SeamOrder& in) {
    SeamOrder out;
    out.width = in.height;
    out.height = in.width;
    out.seamCount = in.seamCount;
    out.step.resize(in.step.size());
    for (int row = 0; row < in.height; ++row) {
        for (int col = 0; col < in.width; ++col) {
            out.step[static_cast<std::size_t>(col) * out.width + row] = in.at(row, col);
        }
    }
    return out;
}

void checkTarget(const SeamOrder& order, int imgWidth, int imgHeight, int target, int full) {
    if (order.width != imgWidth || order.height != imgHeight) {
        throw std::invalid_argument("seam order was computed for a different image size");
    }
    if (target > full || target < full - order.seamCount) {
        throw std::invalid_argument("target size is outside the range the seam order covers");
    }
}

} // namespace

SeamOrder computeVerticalSeamOrder(const Image& img, int minWidth, Method method, int threads,
                                   const ProgressCallback& progress) {
    if (minWidth < 1 || minWidth > img.width) {
        throw std::invalid_argument("computeVerticalSeamOrder: minWidth must be between 1 and the image width");
    }
    const int width = img.width; // original width = row stride of `column` below
    SeamOrder order;
    order.width = width;
    order.height = img.height;
    order.seamCount = width - minWidth;
    order.step.assign(static_cast<std::size_t>(width) * img.height, 0); // 0 = never removed

    // column[row * width + c] = original column of the pixel now at column c of that row.
    // Only the first currentWidth entries of each row are in use. Removing a seam pixel shifts
    // the rest of that row left by one, in place (same idea as SeamTracker, but without a copy
    // of the image or a new grid per seam: rows never change for vertical seams).
    std::vector<int> column(order.step.size());
    for (int row = 0; row < img.height; ++row) {
        for (int c = 0; c < width; ++c) {
            column[static_cast<std::size_t>(row) * width + c] = c;
        }
    }

    int seamsDone = 0;
    int currentWidth = width;
    carve(img, minWidth, img.height, method,
          [&](const Image& current, const Seam& s, Orientation) {
              ++seamsDone;
              for (int row = 0; row < current.height; ++row) {
                  int* rowStart = &column[static_cast<std::size_t>(row) * width];
                  const int originalCol = rowStart[s[row]];
                  order.step[static_cast<std::size_t>(row) * width + originalCol] = seamsDone;
                  // Shift the entries right of the seam one place left.
                  std::copy(rowStart + s[row] + 1, rowStart + currentWidth, rowStart + s[row]);
              }
              --currentWidth;
              if (progress) {
                  progress(seamsDone, order.seamCount);
              }
          },
          threads);
    return order;
}

SeamOrder computeHorizontalSeamOrder(const Image& img, int minHeight, Method method, int threads,
                                     const ProgressCallback& progress) {
    // A horizontal seam is a vertical seam of the transposed image (as in carve()).
    return transposeOrder(computeVerticalSeamOrder(transpose(img), minHeight, method, threads, progress));
}

Image applyVerticalOrder(const Image& img, const SeamOrder& order, int targetWidth) {
    checkTarget(order, img.width, img.height, targetWidth, img.width);
    const int removed = img.width - targetWidth; // seams 1..removed are gone
    Image out(targetWidth, img.height);
    for (int row = 0; row < img.height; ++row) {
        int outCol = 0;
        for (int col = 0; col < img.width; ++col) {
            const int s = order.at(row, col);
            if (s != 0 && s <= removed) {
                continue; // removed by one of the first `removed` seams
            }
            copyPixel(img, row, col, out, row, outCol);
            ++outCol;
        }
    }
    return out;
}

Image applyHorizontalOrder(const Image& img, const SeamOrder& order, int targetHeight) {
    checkTarget(order, img.width, img.height, targetHeight, img.height);
    return transpose(applyVerticalOrder(transpose(img), transposeOrder(order), targetHeight));
}

Image paintRemovedPixels(const Image& original, const SeamOrder& vertical, int targetWidth,
                         const SeamOrder& horizontal, int targetHeight) {
    checkTarget(vertical, original.width, original.height, targetWidth, original.width);
    checkTarget(horizontal, targetWidth, original.height, targetHeight, original.height);
    const int removedCols = original.width - targetWidth;
    const int removedRows = original.height - targetHeight;

    Image out = original;
    for (int row = 0; row < original.height; ++row) {
        int narrowCol = 0; // column of this pixel in the image after the vertical seams
        for (int col = 0; col < original.width; ++col) {
            const int v = vertical.at(row, col);
            if (v != 0 && v <= removedCols) {
                paintPixel(out, row, col, kRed); // removed by a vertical seam
                continue;
            }
            // Kept by the vertical seams, so it is pixel (row, narrowCol) of the narrower
            // image; check whether a horizontal seam then removed it.
            const int h = horizontal.at(row, narrowCol);
            if (h != 0 && h <= removedRows) {
                paintPixel(out, row, col, kBlue);
            }
            ++narrowCol;
        }
    }
    return out;
}

} // namespace seam
