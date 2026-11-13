#pragma once

// Visualisation helpers (assignment bonus item 7a: step-by-step visualisation).

#include "seam/carver.hpp" // Orientation
#include "seam/image.hpp"
#include "seam/seam.hpp"

#include <cstdint>

namespace seam {

// An RGB colour used to draw seams.
struct Colour {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

const Colour kRed{255, 0, 0};  // vertical seams
const Colour kBlue{0, 0, 255}; // horizontal seams

// Paints every pixel of `seam` on `img` in `colour`.
//   Vertical seam:   one entry per row,    seam[row] = column of the pixel (seam.size() == img.height).
//   Horizontal seam: one entry per column, seam[col] = row of the pixel    (seam.size() == img.width).
// This matches what carve() passes to its SeamCallback.
// Throws std::invalid_argument if the seam does not fit the image. Time O(seam length).
void paintSeam(Image& img, const Seam& seam, Orientation orientation, Colour colour);

} // namespace seam
