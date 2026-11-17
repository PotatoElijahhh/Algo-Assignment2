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
#include <algorithm>
#include <stdexcept>
#include <vector>

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

    /*!***********************************************************************
    \brief
        Edmonds-Karp: BFS for the shortest source-to-sink path with
        residual capacity, push its bottleneck, repeat until the sink is
        unreachable. Leaves the residual capacities in g. O(V * E^2).
    \param g
        Graph to run on. Modified in place
    \return
        Total flow, which equals the min cut capacity
    *************************************************************************/
    double maxFlow(FlowGraph& g) {
        const int n = g.nodeCount();
        double totalFlow = 0.0;

        while (true) {
            // BFS for the shortest augmenting path
            std::vector<int> parentEdge(n, -1); // edge used to reach each node
            std::vector<char> visited(n, 0);
            std::vector<int> queue;
            visited[g.source()] = 1;
            queue.push_back(g.source());

            for (std::size_t head = 0; head < queue.size() && !visited[g.sink()]; ++head) {
                const int u = queue[head];
                for (int e : g.adj[u]) {
                    const FlowEdge& edge = g.edges[e];
                    if (edge.cap > kFlowEpsilon && !visited[edge.to]) { // edge still has room
                        visited[edge.to] = 1;
                        parentEdge[edge.to] = e;
                        queue.push_back(edge.to);
                    }
                }
            }
            if (!visited[g.sink()]) {
                break; // no augmenting path left, flow is maximum
            }

            // bottleneck = smallest residual on the path; edges[e ^ 1].to steps back one node
            double push = kInfiniteCapacity;
            for (int v = g.sink(); v != g.source(); v = g.edges[parentEdge[v] ^ 1].to) {
                push = std::min(push, g.edges[parentEdge[v]].cap);
            }
            if (push == kInfiniteCapacity) {
                throw std::logic_error("maxFlow: source and sink are joined by infinite edges");
            }

            for (int v = g.sink(); v != g.source(); v = g.edges[parentEdge[v] ^ 1].to) {
                g.edges[parentEdge[v]].cap -= push;     // less room forward
                g.edges[parentEdge[v] ^ 1].cap += push; // reverse edge gets the flow back
            }
            totalFlow += push;
        }
        return totalFlow;
    }

}
