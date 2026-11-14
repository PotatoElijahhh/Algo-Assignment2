/*!************************************************************************
  \file graphcut_flow.cpp
  \author Julius Cai
  \par SIT email: @sit.singaporetech.edu.sg
  \par Course: CSD3131
  \par Algorithm Assignment #2
  \date 13-11-2026
  \brief
  This source file implements the graph construction and max-flow
  parts of the graph cut seam finder.
**************************************************************************/
#include "seam/graphcut.hpp"
#include <stdexcept>

namespace seam {

    FlowGraph buildSeamGraph(const Matrix& /*energy*/) {
        // Placeholder until the graph construction is merged
        throw std::logic_error("buildSeamGraph is not implemented yet");
    }

    double maxFlow(FlowGraph& /*g*/) {
        // Placeholder until the max-flow implementation is merged
        throw std::logic_error("maxFlow is not implemented yet");
    }

}
