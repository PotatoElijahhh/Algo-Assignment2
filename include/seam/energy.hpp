#pragma once

#include "seam/image.hpp"
#include "seam/matrix.hpp"

namespace seam {

// Energy map e[i][j]: how disruptive it would be to remove pixel (i, j).
// Computed as the Sobel gradient magnitude of the grayscale image, so edges and
// detailed regions get high energy and flat regions get low energy.
// Border pixels reuse their nearest neighbour (replicate padding).
Matrix computeEnergy(const Image& img);

} // namespace seam
