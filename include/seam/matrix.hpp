#pragma once

#include <cstddef>
#include <vector>

namespace seam {

// Dense row-major grid of doubles, used for energy maps and DP cost tables.
struct Matrix {
    int rows = 0;
    int cols = 0;
    std::vector<double> values;

    Matrix() = default;
    Matrix(int r, int c, double init = 0.0)
        : rows(r), cols(c), values(static_cast<std::size_t>(r) * c, init) {}

    double& operator()(int row, int col) {
        return values[static_cast<std::size_t>(row) * cols + col];
    }
    double operator()(int row, int col) const {
        return values[static_cast<std::size_t>(row) * cols + col];
    }
};

} // namespace seam
