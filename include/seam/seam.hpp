#pragma once

#include "seam/matrix.hpp"

#include <vector>

namespace seam {

// A vertical seam: seam[row] is the column of the pixel removed in that row.
// Consecutive entries differ by at most 1 (pixels touch vertically or diagonally).
using Seam = std::vector<int>;

// Finds the minimum-energy vertical seam with dynamic programming.
//
//   M[0][j] = e[0][j]
//   M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])
//
// The seam ends at the smallest entry of the last row and is traced back
// upwards through the cheapest parent. Time O(rows * cols), space O(rows * cols).
Seam findVerticalSeamDP(const Matrix& energy);

// Sum of the energies of the pixels on the seam.
double seamCost(const Matrix& energy, const Seam& seam);

// True if the seam has one in-range column per row and each step moves at most one column.
bool isValidSeam(const Seam& seam, int rows, int cols);

} // namespace seam
