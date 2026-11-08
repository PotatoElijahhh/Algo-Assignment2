#pragma once

#include "seam/matrix.hpp"

#include <vector>

namespace seam {

// A vertical seam: seam[row] is the column of the pixel removed in that row.
// Consecutive entries differ by at most 1 (pixels touch vertically or diagonally).
using Seam = std::vector<int>;

// Seam-finding strategies compared in this project.
enum class Method {
    DynamicProgramming,        // original DP with the full cost table
    DynamicProgrammingCompact, // improvement: memory-light DP (same result)
    DynamicProgrammingParallel, // improvement: memory-light DP, rows filled by several threads
    Greedy,
    GraphCut,
};

// Runs the chosen strategy. Every strategy takes an energy map and returns a vertical seam.
// `threads` is only used by Method::DynamicProgrammingParallel.
Seam findVerticalSeam(const Matrix& energy, Method method, int threads = 1);

// Finds the minimum-energy vertical seam with dynamic programming.
//
//   M[0][j] = e[0][j]
//   M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])
//
// The seam ends at the smallest entry of the last row and is traced back
// upwards through the cheapest parent. Time O(rows * cols), space O(rows * cols).
Seam findVerticalSeamDP(const Matrix& energy);

// IMPROVEMENT (assignment item 5d, "do we need the entire DP table?"): memory-light DP.
// Same recurrence, same tie-breaking and exactly the same seam as findVerticalSeamDP, but:
//   - only two rows of costs are kept (the previous row and the current row), and
//   - a 1-byte "parent" table remembers which pixel above (-1, 0 or +1) each cell came from,
//     which is all the trace back needs.
// Time O(rows * cols). Memory: 2 * cols doubles + rows * cols bytes, instead of
// rows * cols doubles (8 bytes each), so roughly 8 times less.
Seam findVerticalSeamDPCompact(const Matrix& energy);

// IMPROVEMENT (assignment item 5d, "does multithreading help?"): multithreaded DP.
// Same as findVerticalSeamDPCompact (same seam), but the cells of each row are computed by
// `threads` threads at once. This works because a cell only depends on the row above it.
// Rows must still be processed in order, so threads start and stop once per row; for
// narrow images that overhead can cost more than it saves.
Seam findVerticalSeamDPParallel(const Matrix& energy, int threads);

// Greedy: starts at the cheapest pixel of the top row, then in each row below
// moves to the cheapest of the (up to) three touching pixels. Fast, but not
// guaranteed to find the minimum seam.
Seam findVerticalSeamGreedy(const Matrix& energy);

// Graph cut: builds a pixel graph and finds the seam as a minimum s-t cut
// using max-flow (Edmonds-Karp). Note: by construction the seam never uses the
// last column, which is tied to the sink.
Seam findVerticalSeamGraphCut(const Matrix& energy);

// Sum of the energies of the pixels on the seam.
double seamCost(const Matrix& energy, const Seam& seam);

// True if the seam has one in-range column per row and each step moves at most one column.
bool isValidSeam(const Seam& seam, int rows, int cols);

} // namespace seam
