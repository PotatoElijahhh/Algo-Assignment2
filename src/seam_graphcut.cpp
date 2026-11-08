#include "seam/seam.hpp"

#include <stdexcept>

namespace seam {

Seam findVerticalSeamGraphCut(const Matrix& /*energy*/) {
    // Placeholder until the graph-cut (max-flow) implementation is merged.
    throw std::logic_error("Graph-cut seam finder is not implemented yet");
}

} // namespace seam
