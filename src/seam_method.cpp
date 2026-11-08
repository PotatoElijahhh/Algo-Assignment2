#include "seam/seam.hpp"

#include <stdexcept>

namespace seam {

Seam findVerticalSeam(const Matrix& energy, Method method, int threads) {
    switch (method) {
    case Method::DynamicProgramming:
        return findVerticalSeamDP(energy);
    case Method::DynamicProgrammingCompact:
        return findVerticalSeamDPCompact(energy);
    case Method::DynamicProgrammingParallel:
        return findVerticalSeamDPParallel(energy, threads);
    case Method::Greedy:
        return findVerticalSeamGreedy(energy);
    case Method::GraphCut:
        return findVerticalSeamGraphCut(energy);
    }
    throw std::invalid_argument("findVerticalSeam: unknown method");
}

} // namespace seam
