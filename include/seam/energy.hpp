#pragma once

#include "seam/image.hpp"
#include "seam/matrix.hpp"

namespace seam {

// Energy map e[i][j]: how disruptive it would be to remove pixel (i, j).
// Computed as the Sobel gradient magnitude of the grayscale image, so edges and
// detailed regions get high energy and flat regions get low energy.
// Border pixels reuse their nearest neighbour (replicate padding).
//
// IMPROVEMENT (assignment item 5d, multithreading): every pixel's energy only reads the
// image, never another pixel's energy, so the rows can be split across `threads` threads.
// threads = 1 (the default) runs the original single-threaded loop. The result is
// exactly the same for any number of threads.
Matrix computeEnergy(const Image& img, int threads = 1);

} // namespace seam
