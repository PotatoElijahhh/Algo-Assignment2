/*!************************************************************************
  \file graphcut.hpp
  \author Go San Ni
  \par SIT email: 2403063@sit.singaporetech.edu.sg
  \par Course: CSD3131
  \par Algorithm Assignment #2
  \date 13-11-2026
  \brief
  This source file declares the shared graph type for the graph cut seam
  finder, plus the graph construction, max-flow and seam extraction
  functions.
**************************************************************************/
#pragma once

#include "seam/matrix.hpp"
#include "seam/seam.hpp"
#include <cstddef>
#include <limits>
#include <vector>

namespace seam {

    /*!***********************************************************************
    \brief
        Capacity for edges that must never be cut.
    *************************************************************************/
    constexpr double kInfiniteCapacity = std::numeric_limits<double>::infinity();

    /*!***********************************************************************
    \brief
        Residual capacity at or below this counts as saturated, to absorb
        floating-point rounding.
    *************************************************************************/
    constexpr double kFlowEpsilon = 1e-9;

    struct FlowEdge {
        int to = 0;
        double cap = 0.0;
    };

    struct FlowGraph {
        int rows = 0;
        int cols = 0;
        std::vector<FlowEdge> edges;
        std::vector<std::vector<int>> adj; // edge indices leaving each node

        FlowGraph() = default;
        FlowGraph(int r, int c) : rows(r), cols(c), adj(static_cast<std::size_t>(r) * c + 2) {}

        int node(int row, int col) const { return row * cols + col; }
        int source() const { return rows * cols; }
        int sink() const { return rows * cols + 1; }
        int nodeCount() const { return rows * cols + 2; }

        /*!***********************************************************************
        \brief
            Adds edge u -> v with capacity cap, and its reverse v -> u with
            capacity 0.
        \param u
            Start node
        \param v
            End node
        \param cap
            Capacity of the forward edge
        *************************************************************************/
        void addEdge(int u, int v, double cap) {
            adj[u].push_back(static_cast<int>(edges.size()));
            edges.push_back({v, cap});
            adj[v].push_back(static_cast<int>(edges.size()));
            edges.push_back({u, 0.0});
        }
    };

    // ---- graph construction + max-flow (src/graphcut_flow.cpp)

    /*!***********************************************************************
    \brief
        Builds the vertical seam graph from the energy map. Horizontal edges
        get the pixel energy; source, sink, backward and diagonal edges are
        infinite.
    \param energy
        Energy map of the image
    \return
        The graph, ready for max-flow
    *************************************************************************/
    FlowGraph buildSeamGraph(const Matrix& energy);

    /*!***********************************************************************
    \brief
        Runs max-flow (Edmonds-Karp) from source to sink and leaves the
        residual capacities in the graph.
    \param g
        Graph to run on. Modified in place
    \return
        Total flow, which equals the min cut capacity
    *************************************************************************/
    double maxFlow(FlowGraph& g);

    /*!***********************************************************************
    \brief
        BFS from the source over edges with residual capacity left. Finds
        the source side S of the min cut.
    \param g
        Graph after max-flow
    \return
        One flag per node, 1 if the node is in S
    *************************************************************************/
    std::vector<char> sourceSide(const FlowGraph& g);

    /*!***********************************************************************
    \brief
        Reads the seam from the min cut: in each row, the seam pixel is the
        rightmost pixel in S. Throws std::logic_error if the cut is not a
        valid seam.
    \param g
        Graph after max-flow
    \return
        Vertical seam, one column per row
    *************************************************************************/
    Seam extractSeam(const FlowGraph& g);

}
