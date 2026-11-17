// Greedy seam finder (assignment item 3).
//
// This follows the greedy idea from lecture L05: the algorithm works in PHASES, and in each
// phase it takes the best choice available RIGHT NOW, without thinking about what comes later
// and without ever going back to change an earlier choice (no backtracking).
//
//   Phase 0     : pick the cheapest pixel in the top row.
//   Phase i > 0 : from the pixel chosen in row i-1, move to the cheapest of the (up to) 3
//                 touching pixels in row i: col-1, col, col+1.
//
// Each choice is only a LOCAL optimum. Like the $1/$7/$10 coin example in L05, a cheap pixel
// now can lead into an expensive area later, so the seam is valid but not always the cheapest.
// Dynamic programming (seam_dp.cpp) looks at every path and is always optimal.
//
// Time:  Theta(cols) for the top row + Theta(rows) for the other rows (3 checks each)
//        = Theta(rows + cols). DP needs Theta(rows * cols).
// Space: Theta(rows) for the seam itself. No cost table is needed.

#include "seam/seam.hpp"

#include <stdexcept>

namespace seam {

namespace {

// Column (col-1, col or col+1) of the lowest-energy pixel in `row` that touches column `col`.
// Ties prefer straight down, then left, then right (the same rule as the DP), so the result
// is always the same for the same input.
int cheapestTouchingPixel(const Matrix& energy, int row, int col) {
    int best = col; // start with the pixel straight below
    if (col > 0 && energy(row, col - 1) < energy(row, best)) {
        best = col - 1; // down-left is cheaper (only if it is inside the image)
    }
    if (col + 1 < energy.cols && energy(row, col + 1) < energy(row, best)) {
        best = col + 1; // down-right is cheaper (only if it is inside the image)
    }
    return best;
}

} // namespace

Seam findVerticalSeamGreedy(const Matrix& energy) {
    const int rows = energy.rows;
    const int cols = energy.cols;
    if (rows <= 0 || cols <= 0) {
        throw std::invalid_argument("findVerticalSeamGreedy: energy map is empty");
    }

    Seam seam(rows); // seam[row] = column of the pixel removed in that row

    // Phase 0: the cheapest pixel of the top row (leftmost one if there is a tie).
    int col = 0;
    for (int j = 1; j < cols; ++j) {
        if (energy(0, j) < energy(0, col)) {
            col = j;
        }
    }
    seam[0] = col;

    // Phases 1 .. rows-1: one greedy choice per row, never revisited.
    for (int row = 1; row < rows; ++row) {
        col = cheapestTouchingPixel(energy, row, col);
        seam[row] = col;
    }
    return seam;
}

} // namespace seam
