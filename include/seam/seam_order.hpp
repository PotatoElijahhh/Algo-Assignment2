#pragma once

// Seam ORDER maps, for instant resizing in the GUI (assignment bonus item 7b).
//
// Idea (Avidan & Shamir 2007, "multi-size images"): carving to width w removes the first
// W - w seams of the same sequence, whatever w is, because each step always removes the
// current cheapest seam. So we carve ONCE, all the way down, and remember for every pixel
// at which step it was removed. Then the image at any width w is simply "every pixel that
// was removed after step W - w, or never", which takes O(W * H) with no energy or DP work.

#include "seam/carver.hpp"
#include "seam/image.hpp"
#include "seam/seam.hpp"
#include "seam/visualise.hpp"

#include <functional>
#include <vector>

namespace seam {

// For each pixel of an image: the step (1, 2, 3, ...) of the vertical seam that removed it,
// or 0 if it was never removed. step[row * width + col].
struct SeamOrder {
    int width = 0;  // size of the image the order was computed for
    int height = 0;
    int seamCount = 0; // number of seams removed = width - (narrowest width)
    std::vector<int> step;

    int at(int row, int col) const { return step[static_cast<std::size_t>(row) * width + col]; }
};

// Called after each seam with (seams done, seams in total), e.g. for a progress bar.
using ProgressCallback = std::function<void(int done, int total)>;

// Carves `img` with vertical seams down to minWidth and records the order.
// Time O((W - minWidth) * W * H), the same as one carve() to that width.
SeamOrder computeVerticalSeamOrder(const Image& img, int minWidth, Method method, int threads = 1,
                                   const ProgressCallback& progress = {});

// The same for horizontal seams (down to minHeight), computed on the transposed image.
// The result is transposed back, so step[row * width + col] is in normal orientation.
SeamOrder computeHorizontalSeamOrder(const Image& img, int minHeight, Method method, int threads = 1,
                                     const ProgressCallback& progress = {});

// The image at targetWidth: in each row, keep the pixels with step 0 or step > W - targetWidth.
// Gives exactly carve(img, targetWidth, img.height, ...). Time O(W * H).
// Throws std::invalid_argument if targetWidth is outside [W - seamCount, W] or the sizes differ.
Image applyVerticalOrder(const Image& img, const SeamOrder& order, int targetWidth);

// The same for height, using an order from computeHorizontalSeamOrder().
Image applyHorizontalOrder(const Image& img, const SeamOrder& order, int targetHeight);

// The original image with the pixels removed on the way to targetWidth x targetHeight painted:
// red = vertical seams, blue = horizontal seams (same picture as SeamTracker::overlay()).
//   vertical:   order computed on `original`
//   horizontal: order computed on applyVerticalOrder(original, vertical, targetWidth)
Image paintRemovedPixels(const Image& original, const SeamOrder& vertical, int targetWidth,
                         const SeamOrder& horizontal, int targetHeight);

} // namespace seam
