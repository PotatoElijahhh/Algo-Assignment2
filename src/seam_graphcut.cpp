/*!************************************************************************
  \file seam_graphcut.cpp
  \author Go San Ni
  \par SIT email: 2403063@sit.singaporetech.edu.sg
  \par Course: CSD3131
  \par Algorithm Assignment #2
  \date 13-11-2026
  \brief
  This source file implements seam extraction for the graph cut seam
  finder. It reads the seam from the min cut after max-flow.
**************************************************************************/
#include "seam/graphcut.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace seam {

    /*!***********************************************************************
    \brief
        BFS from the source over edges with residual capacity above
        kFlowEpsilon. The nodes reached form the source side S of the
        min cut. O(V + E).
    \param g
        Graph after max-flow
    \return
        One flag per node, 1 if the node is in S
    *************************************************************************/
    std::vector<char> sourceSide(const FlowGraph& g) {
        std::vector<char> inS(static_cast<std::size_t>(g.nodeCount()), 0);
        std::vector<int> queue;
        queue.reserve(static_cast<std::size_t>(g.nodeCount()));

        inS[g.source()] = 1;
        queue.push_back(g.source());
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const int u = queue[head];
            for (int e : g.adj[u]) {
                const FlowEdge& edge = g.edges[e];
                if (edge.cap > kFlowEpsilon && !inS[edge.to]) { // edge still has room
                    inS[edge.to] = 1;
                    queue.push_back(edge.to);
                }
            }
        }
        return inS;
    }

    /*!***********************************************************************
    \brief
        Takes the rightmost pixel in S in each row as the seam pixel.
        Throws std::logic_error if the sink is still reachable, a row is
        not cut exactly once, or the seam is not connected.
    \param g
        Graph after max-flow
    \return
        Vertical seam, one column per row
    *************************************************************************/
    Seam extractSeam(const FlowGraph& g) {
        if (g.rows <= 0 || g.cols < 2) {
            throw std::invalid_argument("extractSeam: graph needs at least 1 row and 2 columns");
        }
        if (static_cast<int>(g.adj.size()) != g.nodeCount()) {
            throw std::invalid_argument("extractSeam: adjacency list size does not match the graph");
        }

        const std::vector<char> inS = sourceSide(g);
        if (inS[g.sink()]) {
            throw std::logic_error("extractSeam: sink is still reachable, flow is not maximum");
        }

        Seam seam(g.rows);
        for (int row = 0; row < g.rows; ++row) {
            // rightmost pixel of this row on the source side
            int col = g.cols - 1;
            while (col >= 0 && !inS[g.node(row, col)]) {
                --col;
            }
            if (col < 0 || col == g.cols - 1) {
                throw std::logic_error("extractSeam: row " + std::to_string(row) + " is not cut");
            }
            // everything left of it must be in S so the row is cut exactly once
            for (int j = 0; j < col; ++j) {
                if (!inS[g.node(row, j)]) {
                    throw std::logic_error("extractSeam: row " + std::to_string(row) + " is cut more than once");
                }
            }
            seam[row] = col;
        }

        if (!isValidSeam(seam, g.rows, g.cols)) {
            throw std::logic_error("extractSeam: seam is not connected");
        }
        return seam;
    }

    /*!***********************************************************************
    \brief
        Finds the minimum energy vertical seam with graph cut: builds the
        graph, runs max-flow and extracts the seam. A one-column image
        returns column 0 without building a graph.
    \param energy
        Energy map of the image
    \return
        Vertical seam, one column per row.
    *************************************************************************/
    Seam findVerticalSeamGraphCut(const Matrix& energy) {
        if (energy.rows <= 0 || energy.cols <= 0) {
            throw std::invalid_argument("findVerticalSeamGraphCut: energy map is empty");
        }
        if (energy.cols == 1) {
            return Seam(energy.rows, 0); // only one column, no cut needed
        }

        FlowGraph g = buildSeamGraph(energy);
        maxFlow(g);
        return extractSeam(g);
    }

}
