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

  Idea: every pixel is a node. A vertical seam is a cut that splits the
  image into a left part S (connected to the source) and a right part T
  (connected to the sink). The only finite edges point right, so the cut
  only pays for the pixels where S ends in each row. Those pixels are
  the seam, and the min cut is the cheapest seam.
**************************************************************************/
#include "seam/graphcut.hpp"
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace seam {

    /*!***********************************************************************
    \brief
        Builds the vertical seam graph from the energy map.
        For every pixel p = (row, col):
          - source -> p                INF   if p is in the first column
          - p -> sink                  INF   if p is in the last column
          - p -> (row, col+1)          e(row, col)   (the only finite edge)
          - (row, col+1) -> p          INF   (so each row is cut once)
          - p -> (row-1, col-1)        INF   (diagonals: keep the seam
          - p -> (row+1, col-1)        INF    connected between rows)
        V = rows*cols + 2 nodes, E = O(rows*cols) edges. Time O(rows*cols).
    \param energy
        Energy map of the image (needs at least 2 columns)
    \return
        The graph, ready for max-flow
    *************************************************************************/
    FlowGraph buildSeamGraph(const Matrix& energy) {
        if (energy.rows <= 0 || energy.cols < 2) {
            // With 1 column the source and sink would be joined by INF edges
            throw std::invalid_argument("buildSeamGraph: energy map needs at least 1 row and 2 columns");
        }

        const int rows = energy.rows;
        const int cols = energy.cols;
        FlowGraph g(rows, cols);

        for (int row = 0; row < rows; ++row) {
            for (int col = 0; col < cols; ++col) {
                const int p = g.node(row, col);

                if (col == 0) {
                    g.addEdge(g.source(), p, kInfiniteCapacity); // first column is always in S
                }
                if (col == cols - 1) {
                    g.addEdge(p, g.sink(), kInfiniteCapacity); // last column is always in T
                } else {
                    const int right = g.node(row, col + 1);
                    // Cutting this edge means "p is the seam pixel of this row", costing its energy
                    g.addEdge(p, right, energy(row, col));
                    // If right is in S, p must be in S too: S is a solid block from the left,
                    // so each row is cut exactly once
                    g.addEdge(right, p, kInfiniteCapacity);
                }

                if (col > 0) {
                    // If p is in S, the pixels up-left and down-left are in S too.
                    // So the seam column changes by at most 1 between rows.
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
        Runs max-flow with Edmonds-Karp: BFS finds the shortest path from
        source to sink using only edges with capacity left, the smallest
        capacity on it (the bottleneck) is pushed, and this repeats until
        no path is left. Capacities are updated in place, so afterwards
        g holds the residual graph.

        Edges are stored in pairs by addEdge (forward, then reverse), so
        the reverse of edge e is always e ^ 1 (0<->1, 2<->3, ...).

        Time O(V * E^2): at most O(V * E) augmenting paths, each found
        by an O(E) BFS. Space O(V) for the BFS arrays.
    \param g
        Graph to run on. Modified in place
    \return
        Total flow, which equals the min cut capacity (the seam cost)
    *************************************************************************/
    double maxFlow(FlowGraph& g) {
        const int n = g.nodeCount();
        double totalFlow = 0.0;

        while (true) {
            // ---- 1. BFS for the shortest augmenting path
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
                break; // no augmenting path left: the flow is maximum
            }

            // ---- 2. Bottleneck: smallest capacity along the path (walk back from the sink)
            // edges[e ^ 1].to is the node the edge e came from
            double push = kInfiniteCapacity;
            for (int v = g.sink(); v != g.source(); v = g.edges[parentEdge[v] ^ 1].to) {
                push = std::min(push, g.edges[parentEdge[v]].cap);
            }
            if (push == kInfiniteCapacity) {
                throw std::logic_error("maxFlow: source and sink are joined by infinite edges");
            }

            // ---- 3. Push the bottleneck: less room forward, more room to undo it backward
            for (int v = g.sink(); v != g.source(); v = g.edges[parentEdge[v] ^ 1].to) {
                g.edges[parentEdge[v]].cap -= push;
                g.edges[parentEdge[v] ^ 1].cap += push;
            }
            totalFlow += push;
        }
        return totalFlow;
    }

}
