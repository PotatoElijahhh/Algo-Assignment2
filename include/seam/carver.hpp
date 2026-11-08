#pragma once

#include "seam/image.hpp"
#include "seam/seam.hpp"

#include <functional>

namespace seam {

enum class Orientation {
    Vertical,   // seam[row] = column removed in that row (image gets narrower)
    Horizontal, // seam[col] = row removed in that column (image gets shorter)
};

// Called just before each seam is removed, with the image as it currently looks
// (in normal orientation) and the seam about to be removed. Used for step-by-step
// visualisation; leave empty when not needed.
using SeamCallback = std::function<void(const Image& current, const Seam& seam, Orientation orientation)>;

// Removes one pixel per row at the columns given by a vertical seam.
void removeVerticalSeam(Image& img, const Seam& seam);

// Shrinks the image to targetWidth x targetHeight by repeatedly:
//   1. computing the energy map,
//   2. finding the cheapest seam with the chosen method,
//   3. removing it.
// Vertical seams are removed first. Horizontal seams are removed as vertical
// seams of the transposed image. Targets must be between 1 and the current size.
// `threads` (default 1) is the number of threads used to compute the energy map.
// The output does not depend on it.
Image carve(const Image& input, int targetWidth, int targetHeight, Method method,
            const SeamCallback& onSeam = {}, int threads = 1);

} // namespace seam
