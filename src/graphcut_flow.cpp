/*!************************************************************************
  \file graphcut_flow.cpp
  \author Julius Cai
  \par SIT email: 2401998@sit.singaporetech.edu.sg
  \par Course: CSD3131
  \par Algorithm Assignment #2
  \date 16-11-2026
  \brief
  This source file implements the graph construction and max-flow
  (Edmonds-Karp) parts of the graph cut seam finder.
**************************************************************************/
#include "seam/graphcut.hpp"
#include <stdexcept>

namespace seam {

    /*!***********************************************************************
    \brief
        Builds the vertical seam graph from the energy map. The only finite
        edges are (i, j) -> (i, j+1) with capacity e(i, j); source, sink,
        backward and diagonal edges are infinite. O(rows * cols).
    \param energy
        Energy map of the image, at least 2 columns
    \return
        The graph, ready for max-flow
    *************************************************************************/
    FlowGraph buildSeamGraph(const Matrix& energy) {
        if (energy.rows <= 0 || energy.cols < 2) {
            throw std::invalid_argument("buildSeamGraph: energy map needs at least 1 row and 2 columns");
        }

        const int rows = energy.rows;
        const int cols = energy.cols;
        FlowGraph g(rows, cols);

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const int p = g.node(row, col);

                if (col == 0) {
                    g.addEdge(g.source(), p, kInfiniteCapacity); // first column always in S
                }
                if (col == cols - 1) {
                    g.addEdge(p, g.sink(), kInfiniteCapacity); // last column always in T
                } else {
                    const int right = g.node(row, col + 1);
                    g.addEdge(p, right, energy(row, col));      // cutting it removes pixel p
                    g.addEdge(right, p, kInfiniteCapacity);     // each row is cut exactly once
                }

                if (col > 0) { // neighbouring rows differ by at most 1 column
                    if (row > 0) {
                        g.addEdge(p, g.node(row - 1, col - 1), kInfiniteCapacity);
                    }
                    if (row + 1 < rows) {
                        g.addEdge(p, g.node(row + 1, col - 1), kInfiniteCapacity);
                    }
                }
            }
        }
        return g;
    }

    double maxFlow(FlowGraph& /*g*/) {
        // Placeholder until the max-flow implementation is merged
        throw std::logic_error("maxFlow is not implemented yet");
    }

}
