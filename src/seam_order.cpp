#include "seam/seam_order.hpp"

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
    SeamOrder order;
    order.width = img.width;
    order.height = img.height;
    order.seamCount = img.width - minWidth;
    order.step.assign(static_cast<std::size_t>(img.width) * img.height, 0); // 0 = never removed

    // SeamTracker (Step 3) knows the original position of every pixel of the shrinking image,
    // so each seam pixel can be written into the order map at its original position.
    SeamTracker tracker(img);
    int seamsDone = 0;
    carve(img, minWidth, img.height, method,
          [&](const Image& current, const Seam& s, Orientation o) {
              ++seamsDone;
              for (int row = 0; row < current.height; ++row) {
                  order.step[static_cast<std::size_t>(tracker.originalIndex(row, s[row]))] = seamsDone;
              }
              tracker.record(current, s, o);
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
            const std::uint8_t* src = img.pixel(row, col);
            std::uint8_t* dst = out.pixel(row, outCol);
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
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
            const bool removedByVertical = v != 0 && v <= removedCols;
            Colour paint{};
            bool painted = false;
            if (removedByVertical) {
                paint = kRed;
                painted = true;
            } else {
                // Kept by the vertical seams, so it is pixel (row, narrowCol) of the narrower
                // image; check whether a horizontal seam then removed it.
                const int h = horizontal.at(row, narrowCol);
                if (h != 0 && h <= removedRows) {
                    paint = kBlue;
                    painted = true;
                }
                ++narrowCol;
            }
            if (painted) {
                std::uint8_t* p = out.pixel(row, col);
                p[0] = paint.r;
                p[1] = paint.g;
                p[2] = paint.b;
            }
        }
    }
    return out;
}

} // namespace seam
