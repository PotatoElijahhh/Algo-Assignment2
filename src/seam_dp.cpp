#include "seam/seam.hpp"

#include <cstdlib>
#include <stdexcept>

namespace seam {

namespace {

// Column (j-1, j or j+1) of the cheapest entry of `row` next to column j.
// Ties prefer straight up, then left, then right, so results are deterministic.
int cheapestNeighbour(const Matrix& m, int row, int j) {
    int best = j;
    if (j > 0 && m(row, j - 1) < m(row, best)) {
        best = j - 1;
    }
    if (j + 1 < m.cols && m(row, j + 1) < m(row, best)) {
        best = j + 1;
    }
    return best;
}

} // namespace

Seam findVerticalSeamDP(const Matrix& energy) {
    const int rows = energy.rows;
    const int cols = energy.cols;
    if (rows <= 0 || cols <= 0) {
        throw std::invalid_argument("findVerticalSeamDP: energy map is empty");
    }

    // cost(i, j) = minimum total energy of a seam from the top row down to pixel (i, j).
    Matrix cost(rows, cols);
    for (int j = 0; j < cols; ++j) {
        cost(0, j) = energy(0, j);
    }
    for (int i = 1; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            cost(i, j) = energy(i, j) + cost(i - 1, cheapestNeighbour(cost, i - 1, j));
        }
    }

    // The cheapest seam ends at the minimum of the last row.
    Seam seam(rows);
    int col = 0;
    for (int j = 1; j < cols; ++j) {
        if (cost(rows - 1, j) < cost(rows - 1, col)) {
            col = j;
        }
    }
    seam[rows - 1] = col;

    // Walk back up, each time moving to the parent that produced the minimum.
    for (int i = rows - 1; i > 0; --i) {
        col = cheapestNeighbour(cost, i - 1, col);
        seam[i - 1] = col;
    }
    return seam;
}

double seamCost(const Matrix& energy, const Seam& seam) {
    double total = 0.0;
    for (int row = 0; row < static_cast<int>(seam.size()); ++row) {
        total += energy(row, seam[row]);
    }
    return total;
}

bool isValidSeam(const Seam& seam, int rows, int cols) {
    if (static_cast<int>(seam.size()) != rows) {
        return false;
    }
    for (int row = 0; row < rows; ++row) {
        if (seam[row] < 0 || seam[row] >= cols) {
            return false;
        }
        if (row > 0 && std::abs(seam[row] - seam[row - 1]) > 1) {
            return false;
        }
    }
    return true;
}

} // namespace seam
