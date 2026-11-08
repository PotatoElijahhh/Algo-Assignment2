// IMPROVEMENTS to the dynamic-programming seam finder (assignment item 5d).
// The original version is in seam_dp.cpp; the versions here must always return the same seam.

#include "seam/seam.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace seam {

Seam findVerticalSeamDPCompact(const Matrix& energy) {
    const int rows = energy.rows;
    const int cols = energy.cols;
    if (rows <= 0 || cols <= 0) {
        throw std::invalid_argument("findVerticalSeamDPCompact: energy map is empty");
    }

    // The recurrence only ever looks one row up, so two rows of costs are enough:
    // prev = costs of row i-1, curr = costs of row i being filled in.
    std::vector<double> prev(cols);
    std::vector<double> curr(cols);

    // parent[i * cols + j] is -1, 0 or +1: whether the cheapest seam reaching (i, j) came
    // from the pixel above-left, directly above or above-right. 1 byte per pixel.
    std::vector<std::int8_t> parent(static_cast<std::size_t>(rows) * cols, 0);

    for (int j = 0; j < cols; ++j) {
        prev[j] = energy(0, j);
    }

    for (int i = 1; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            // Same tie-breaking as findVerticalSeamDP: straight up, then left, then right.
            int best = j;
            if (j > 0 && prev[j - 1] < prev[best]) {
                best = j - 1;
            }
            if (j + 1 < cols && prev[j + 1] < prev[best]) {
                best = j + 1;
            }
            curr[j] = energy(i, j) + prev[best];
            parent[static_cast<std::size_t>(i) * cols + j] = static_cast<std::int8_t>(best - j);
        }
        std::swap(prev, curr); // the row just filled becomes "previous" for the next row
    }

    // After the loop, prev holds the costs of the last row. The seam ends at its minimum.
    int col = 0;
    for (int j = 1; j < cols; ++j) {
        if (prev[j] < prev[col]) {
            col = j;
        }
    }

    // Trace back using the parent table instead of the (no longer stored) cost table.
    Seam seam(rows);
    seam[rows - 1] = col;
    for (int i = rows - 1; i > 0; --i) {
        col += parent[static_cast<std::size_t>(i) * cols + col];
        seam[i - 1] = col;
    }
    return seam;
}

} // namespace seam
