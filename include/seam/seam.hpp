#pragma once

#include "seam/matrix.hpp"

#include <vector>

namespace seam {

// A vertical seam: seam[row] is the column of the pixel removed in that row.
// Consecutive entries differ by at most 1 (pixels touch vertically or diagonally).
using Seam = std::vector<int>;

// Seam-finding strategies compared in this project.
enum class Method {
    DynamicProgramming,
    Greedy,
    GraphCut,
};

// Runs the chosen strategy. Every strategy takes an energy map and returns a vertical seam.
Seam findVerticalSeam(const Matrix& energy, Method method);

// Finds the minimum-energy vertical seam with dynamic programming.
//
//   M[0][j] = e[0][j]
//   M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])
//
// The seam ends at the smallest entry of the last row and is traced back
// upwards through the cheapest parent. Time O(rows * cols), space O(rows * cols).
Seam findVerticalSeamDP(const Matrix& energy);

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
