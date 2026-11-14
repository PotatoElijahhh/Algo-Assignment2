// Unit tests for the seam carving core. Run with `ctest` or directly as seam_tests.
// Each test is a plain function; CHECK records a failure without stopping the test.

#include "seam/carver.hpp"
#include "seam/energy.hpp"
#include "seam/graphcut.hpp"
#include "seam/image.hpp"
#include "seam/seam.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
bool g_skipped = false; // set by a test that cannot run yet

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::cerr << "  " << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond "\n"; \
            ++g_failures;                                                                 \
        }                                                                                 \
    } while (0)

using seam::Image;
using seam::Matrix;
using seam::Seam;

Matrix makeMatrix(int rows, int cols, const std::vector<double>& values) {
    Matrix m(rows, cols);
    m.values = values;
    return m;
}

Matrix randomMatrix(std::mt19937& rng, int rows, int cols) {
    std::uniform_int_distribution<int> dist(0, 9);
    Matrix m(rows, cols);
    for (double& v : m.values) {
        v = dist(rng); // small integers keep sums exact, so costs can be compared with ==
    }
    return m;
}

Image randomImage(std::mt19937& rng, int width, int height) {
    std::uniform_int_distribution<int> dist(0, 255);
    Image img(width, height);
    for (auto& v : img.data) {
        v = static_cast<std::uint8_t>(dist(rng));
    }
    return img;
}

void fillRect(Image& img, int col0, int col1, int row0, int row1, std::uint8_t value) {
    for (int row = row0; row < row1; ++row) {
        for (int col = col0; col < col1; ++col) {
            std::uint8_t* p = img.pixel(row, col);
            p[0] = p[1] = p[2] = value;
        }
    }
}

// Recursively walks every seam that passes through (row, col), adding up the energy so far.
// When it reaches the bottom row it keeps the smallest total in `best`.
// Only columns below `cols` are used.
void bruteForceWalk(const Matrix& e, int row, int col, int cols, double sumSoFar, double& best) {
    const double sum = sumSoFar + e(row, col);
    if (row == e.rows - 1) {
        if (sum < best) {
            best = sum;
        }
        return;
    }
    for (int step = -1; step <= 1; ++step) { // down-left, straight down, down-right
        const int next = col + step;
        if (next >= 0 && next < cols) {
            bruteForceWalk(e, row + 1, next, cols, sum, best);
        }
    }
}

// Tries every possible vertical seam (up to 3^rows of them) and returns the lowest cost.
// Exponential time, so only used on tiny grids to check the DP answer.
// `cols` limits the seam to the first `cols` columns (default: all).
double bruteForceMinSeamCost(const Matrix& e, int cols = -1) {
    if (cols < 0) {
        cols = e.cols;
    }
    double best = std::numeric_limits<double>::infinity();
    for (int col = 0; col < cols; ++col) {
        bruteForceWalk(e, 0, col, cols, 0.0, best);
    }
    return best;
}

// ---------------------------------------------------------------- DP seam

void testDpFindsKnownSeam() {
    // The cheapest seam follows the 1s: columns 1, 2, 1, 0 with total cost 4.
    const Matrix e = makeMatrix(4, 4, {
        5, 1, 9, 9,
        9, 8, 1, 9,
        9, 1, 9, 9,
        1, 9, 9, 9,
    });
    const Seam s = seam::findVerticalSeamDP(e);
    CHECK((s == Seam{1, 2, 1, 0}));
    CHECK(seam::seamCost(e, s) == 4.0);
}

void testDpMatchesBruteForce() {
    std::mt19937 rng(12345);
    for (int trial = 0; trial < 500; ++trial) {
        const int rows = 1 + static_cast<int>(rng() % 7);
        const int cols = 1 + static_cast<int>(rng() % 6);
        const Matrix e = randomMatrix(rng, rows, cols);
        const Seam s = seam::findVerticalSeamDP(e);
        CHECK(seam::isValidSeam(s, rows, cols));
        CHECK(seam::seamCost(e, s) == bruteForceMinSeamCost(e));
    }
}

void testDpSingleColumnAndSingleRow() {
    const Matrix column = makeMatrix(3, 1, {4, 2, 7});
    CHECK((seam::findVerticalSeamDP(column) == Seam{0, 0, 0}));

    const Matrix row = makeMatrix(1, 4, {3, 1, 2, 5});
    CHECK((seam::findVerticalSeamDP(row) == Seam{1}));
}

void testIsValidSeam() {
    CHECK(seam::isValidSeam({0, 1, 2, 1}, 4, 3));
    CHECK(!seam::isValidSeam({0, 2}, 2, 3));    // jumps two columns
    CHECK(!seam::isValidSeam({0, 3}, 2, 3));    // out of range
    CHECK(!seam::isValidSeam({0, 1}, 3, 3));    // wrong length
}

// ---------------------------------------------------------------- energy

void testEnergyIsZeroForFlatImage() {
    Image img(8, 6);
    fillRect(img, 0, 8, 0, 6, 120);
    const Matrix e = seam::computeEnergy(img);
    CHECK(std::all_of(e.values.begin(), e.values.end(), [](double v) { return v == 0.0; }));
}

void testEnergyIsHighOnEdges() {
    // Left half black, right half white: energy only next to the boundary (columns 3 and 4).
    Image img(8, 6);
    fillRect(img, 4, 8, 0, 6, 255);
    const Matrix e = seam::computeEnergy(img);
    for (int row = 0; row < 6; ++row) {
        CHECK(e(row, 0) == 0.0);
        CHECK(e(row, 3) > 0.0);
        CHECK(e(row, 4) > 0.0);
        CHECK(e(row, 7) == 0.0);
    }
}

// ---------------------------------------------------------------- image and carving

void testTransposeRoundTrip() {
    std::mt19937 rng(7);
    const Image img = randomImage(rng, 5, 3);
    const Image t = seam::transpose(img);
    CHECK(t.width == 3 && t.height == 5);
    CHECK(t.pixel(4, 2)[0] == img.pixel(2, 4)[0]);
    CHECK(seam::transpose(t).data == img.data);
}

void testRemoveVerticalSeam() {
    // 3x3 image where each pixel's value is its index 0..8.
    Image img(3, 3);
    for (int i = 0; i < 9; ++i) {
        img.data[3 * i] = img.data[3 * i + 1] = img.data[3 * i + 2] = static_cast<std::uint8_t>(i);
    }
    seam::removeVerticalSeam(img, {0, 1, 2});
    CHECK(img.width == 2 && img.height == 3);
    const std::vector<int> expected = {1, 2, 3, 5, 6, 7};
    for (int i = 0; i < 6; ++i) {
        CHECK(img.data[3 * i] == expected[i]);
    }
}

void testCarveReachesTargetSize() {
    std::mt19937 rng(99);
    const Image img = randomImage(rng, 20, 15);
    const Image out = seam::carve(img, 14, 10, seam::Method::DynamicProgramming);
    CHECK(out.width == 14 && out.height == 10);
}

void testCarveKeepsHighEnergyObject() {
    // Flat grey image with a black/white checkered block in the middle. The block has
    // high energy, so all removed seams should go around it and every block pixel survives.
    Image img(30, 12);
    fillRect(img, 0, 30, 0, 12, 128);
    for (int row = 2; row < 10; ++row) {
        for (int col = 13; col < 17; ++col) {
            fillRect(img, col, col + 1, row, row + 1, ((row + col) % 2) ? 255 : 0);
        }
    }
    const Image out = seam::carve(img, 20, 12, seam::Method::DynamicProgramming);

    int blockPixels = 0;
    for (std::size_t i = 0; i < out.data.size(); i += 3) {
        if (out.data[i] == 0 || out.data[i] == 255) {
            ++blockPixels;
        }
    }
    CHECK(blockPixels == 4 * 8);
}

void testCarveRejectsInvalidTargets() {
    Image img(10, 10);
    bool threwTooBig = false;
    bool threwZero = false;
    try {
        seam::carve(img, 11, 10, seam::Method::DynamicProgramming);
    } catch (const std::invalid_argument&) {
        threwTooBig = true;
    }
    try {
        seam::carve(img, 0, 10, seam::Method::DynamicProgramming);
    } catch (const std::invalid_argument&) {
        threwZero = true;
    }
    CHECK(threwTooBig);
    CHECK(threwZero);
}

// Counters filled in by countSeams() during testCallbackSeesEverySeam.
int g_verticalSeams = 0;
int g_horizontalSeams = 0;
bool g_seamsValid = true;

void countSeams(const Image& current, const Seam& s, seam::Orientation orientation) {
    if (orientation == seam::Orientation::Vertical) {
        ++g_verticalSeams;
        g_seamsValid = g_seamsValid && seam::isValidSeam(s, current.height, current.width);
    } else {
        // A horizontal seam has one entry per column, giving the row removed there.
        ++g_horizontalSeams;
        g_seamsValid = g_seamsValid && seam::isValidSeam(s, current.width, current.height);
    }
}

void testCallbackSeesEverySeam() {
    std::mt19937 rng(3);
    const Image img = randomImage(rng, 12, 9);
    seam::carve(img, 9, 7, seam::Method::DynamicProgramming, countSeams);
    CHECK(g_verticalSeams == 3);   // 12 -> 9 columns
    CHECK(g_horizontalSeams == 2); // 9 -> 7 rows
    CHECK(g_seamsValid);
}

// ---------------------------------------------------------------- improvements (item 5d)
// The improved versions must give exactly the same answers as the originals.

void testCompactDpMatchesOriginalDp() {
    // Integer energies 0-9 create many ties, which also checks the tie-breaking is the same.
    std::mt19937 rng(2024);
    for (int trial = 0; trial < 300; ++trial) {
        const int rows = 1 + static_cast<int>(rng() % 40);
        const int cols = 1 + static_cast<int>(rng() % 40);
        const Matrix e = randomMatrix(rng, rows, cols);
        CHECK(seam::findVerticalSeamDPCompact(e) == seam::findVerticalSeamDP(e));
    }
}

void testParallelDpMatchesOriginalDp() {
    std::mt19937 rng(77);
    for (int trial = 0; trial < 100; ++trial) {
        const int rows = 1 + static_cast<int>(rng() % 60);
        const int cols = 1 + static_cast<int>(rng() % 200);
        const Matrix e = randomMatrix(rng, rows, cols);
        CHECK(seam::findVerticalSeamDPParallel(e, 4) == seam::findVerticalSeamDP(e));
    }
}

void testParallelEnergyMatchesSerial() {
    std::mt19937 rng(5);
    const Image img = randomImage(rng, 64, 48);
    const Matrix serial = seam::computeEnergy(img, 1);
    const Matrix parallel = seam::computeEnergy(img, 4);
    CHECK(parallel.values == serial.values); // exactly equal, not just close
}

void testAllDpVersionsCarveTheSameImage() {
    std::mt19937 rng(11);
    const Image img = randomImage(rng, 40, 30);
    const Image original = seam::carve(img, 30, 24, seam::Method::DynamicProgramming);
    const Image compact = seam::carve(img, 30, 24, seam::Method::DynamicProgrammingCompact);
    const Image parallel = seam::carve(img, 30, 24, seam::Method::DynamicProgrammingParallel, {}, 4);
    CHECK(compact.data == original.data);
    CHECK(parallel.data == original.data);
}

// -------------------------------- graph cut: seam extraction --------------------------------
using seam::FlowGraph;

// Residual graph as max-flow would leave it, keeping only the horizontal chains
// source -> (i, 0) -> ... -> (i, n-1) -> sink. In row i the edge leaving column
// cut[i] is saturated (residual 0). cut[i] = -1 leaves the row uncut.
FlowGraph chainResidual(int cols, const std::vector<int>& cut, double saturated = 0.0) {
    const int rows = static_cast<int>(cut.size());
    FlowGraph g(rows, cols);
    for (int row = 0; row < rows; ++row) {
        g.addEdge(g.source(), g.node(row, 0), seam::kInfiniteCapacity);
        for (int col = 0; col + 1 < cols; ++col) {
            g.addEdge(g.node(row, col), g.node(row, col + 1), col == cut[row] ? saturated : 1.0);
        }
        g.addEdge(g.node(row, cols - 1), g.sink(), seam::kInfiniteCapacity);
    }
    return g;
}

// True if extractSeam throws std::logic_error on this graph.
bool extractRejects(const FlowGraph& g) {
    try {
        seam::extractSeam(g);
    } catch (const std::logic_error&) {
        return true;
    }
    return false;
}

void testSourceSideFollowsResidualEdges() {
    FlowGraph g(1, 3);
    g.addEdge(g.source(), g.node(0, 0), seam::kInfiniteCapacity);
    g.addEdge(g.node(0, 0), g.node(0, 1), 0.0); // saturated, not followed
    g.addEdge(g.node(0, 2), g.node(0, 0), 5.0);
    g.edges[5].cap = 2.0; // reverse of (0,2) -> (0,0) after 2 units of flow
    const std::vector<char> inS = seam::sourceSide(g);
    CHECK(inS[g.source()]);
    CHECK(inS[g.node(0, 0)]);
    CHECK(!inS[g.node(0, 1)]);
    CHECK(inS[g.node(0, 2)]); // reached through the reverse edge
    CHECK(!inS[g.sink()]);
}

void testExtractReadsRightmostSourcePixel() {
    const FlowGraph g = chainResidual(4, {1, 2, 1, 0});
    CHECK((seam::extractSeam(g) == Seam{1, 2, 1, 0}));
}

void testExtractEdgeColumns() {
    // Seam on the first column, and on the last column allowed (cols - 2)
    CHECK((seam::extractSeam(chainResidual(3, {0, 0, 0})) == Seam{0, 0, 0}));
    CHECK((seam::extractSeam(chainResidual(3, {1, 1, 0})) == Seam{1, 1, 0}));
    CHECK((seam::extractSeam(chainResidual(2, {0})) == Seam{0}));
}

void testExtractTreatsTinyResidualAsSaturated() {
    // Rounding can leave 1e-12 on a saturated edge
    const FlowGraph g = chainResidual(5, {3, 2, 2}, 1e-12);
    CHECK((seam::extractSeam(g) == Seam{3, 2, 2}));
}

void testExtractRejectsReachableSink() {
    // Row 1 has no saturated edge, so the flow is not maximum
    CHECK(extractRejects(chainResidual(4, {1, -1, 1})));
}

void testExtractRejectsRowCutTwice() {
    // Source also reaches (0, 2) directly, so S in row 0 is {0, 2}
    FlowGraph g(1, 4);
    g.addEdge(g.source(), g.node(0, 0), seam::kInfiniteCapacity);
    g.addEdge(g.source(), g.node(0, 2), seam::kInfiniteCapacity);
    g.addEdge(g.node(0, 0), g.node(0, 1), 0.0);
    g.addEdge(g.node(0, 1), g.node(0, 2), 1.0);
    g.addEdge(g.node(0, 2), g.node(0, 3), 0.0);
    g.addEdge(g.node(0, 3), g.sink(), seam::kInfiniteCapacity);
    CHECK(extractRejects(g));
}

void testExtractRejectsRowWithoutSourcePixel() {
    FlowGraph g = chainResidual(3, {0, 0});
    g.edges[g.adj[g.source()][1]].cap = 0.0; // cut source -> (1, 0)
    CHECK(extractRejects(g));
}

void testExtractRejectsDisconnectedSeam() {
    // Columns 0 and 2 in neighbouring rows: the diagonal edges were missing
    CHECK(extractRejects(chainResidual(4, {0, 2})));
}

void testExtractRejectsBadGraph() {
    bool threw = false;
    try {
        seam::extractSeam(FlowGraph(3, 1));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

void testGraphCutSingleColumn() {
    // Only one column: no cut needed, max-flow is not called
    const Matrix e = makeMatrix(3, 1, {4, 2, 7});
    CHECK((seam::findVerticalSeamGraphCut(e) == Seam{0, 0, 0}));
}

// -------------------------------- graph cut: end to end --------------------------------
// These need buildSeamGraph and maxFlow. Until they are merged they are skipped.

// True once graph construction and max-flow are available.
bool graphCutReady() {
    try {
        seam::findVerticalSeamGraphCut(makeMatrix(2, 2, {1, 1, 1, 1}));
    } catch (const std::logic_error& e) {
        return std::string(e.what()).find("not implemented") == std::string::npos;
    }
    return true;
}

// Marks the current test as skipped if the graph cut cannot run yet.
bool requireGraphCut() {
    static const bool ready = graphCutReady();
    if (!ready) {
        g_skipped = true;
    }
    return ready;
}

void testGraphCutMatchesBruteForce() {
    if (!requireGraphCut()) {
        return;
    }
    // The last column is tied to the sink, so the seam uses columns 0 .. cols-2
    std::mt19937 rng(4242);
    for (int trial = 0; trial < 500; ++trial) {
        const int rows = 1 + static_cast<int>(rng() % 7);
        const int cols = 2 + static_cast<int>(rng() % 5);
        const Matrix e = randomMatrix(rng, rows, cols);
        const Seam s = seam::findVerticalSeamGraphCut(e);
        CHECK(seam::isValidSeam(s, rows, cols - 1));
        CHECK(seam::seamCost(e, s) == bruteForceMinSeamCost(e, cols - 1));
    }
}

void testMaxFlowEqualsSeamCost() {
    if (!requireGraphCut()) {
        return;
    }
    // Max-flow min-cut theorem: flow value = cut capacity = seam cost
    std::mt19937 rng(31);
    for (int trial = 0; trial < 200; ++trial) {
        const int rows = 1 + static_cast<int>(rng() % 12);
        const int cols = 2 + static_cast<int>(rng() % 12);
        const Matrix e = randomMatrix(rng, rows, cols);
        FlowGraph g = seam::buildSeamGraph(e);
        const double flow = seam::maxFlow(g);
        const Seam s = seam::extractSeam(g);
        CHECK(flow == seam::seamCost(e, s));
    }
}

void testGraphCutFindsKnownSeam() {
    if (!requireGraphCut()) {
        return;
    }
    const Matrix e = makeMatrix(4, 4, {
        5, 1, 9, 9,
        9, 8, 1, 9,
        9, 1, 9, 9,
        1, 9, 9, 9,
    });
    CHECK((seam::findVerticalSeamGraphCut(e) == Seam{1, 2, 1, 0}));
}

void testGraphCutMatchesDpWhenLastColumnBlocked() {
    if (!requireGraphCut()) {
        return;
    }
    // With a very expensive last column, DP avoids it so both costs must agree
    std::mt19937 rng(8);
    for (int trial = 0; trial < 40; ++trial) {
        const int rows = 1 + static_cast<int>(rng() % 25);
        const int cols = 2 + static_cast<int>(rng() % 25);
        Matrix e = randomMatrix(rng, rows, cols);
        for (int row = 0; row < rows; ++row) {
            e(row, cols - 1) = 1e6;
        }
        const Seam gc = seam::findVerticalSeamGraphCut(e);
        const Seam dp = seam::findVerticalSeamDP(e);
        CHECK(seam::isValidSeam(gc, rows, cols));
        CHECK(seam::seamCost(e, gc) == seam::seamCost(e, dp));
    }
}

void testGraphCutOnSobelEnergy() {
    if (!requireGraphCut()) {
        return;
    }
    // Real (non-integer) energies: compare costs with a small tolerance
    std::mt19937 rng(19);
    Image img = randomImage(rng, 24, 16);
    Matrix e = seam::computeEnergy(img);
    for (int row = 0; row < e.rows; ++row) {
        e(row, e.cols - 1) = 1e9;
    }
    const double gc = seam::seamCost(e, seam::findVerticalSeamGraphCut(e));
    const double dp = seam::seamCost(e, seam::findVerticalSeamDP(e));
    CHECK(std::abs(gc - dp) < 1e-6);
}

void testGraphCutSpecialShapes() {
    if (!requireGraphCut()) {
        return;
    }
    // Two columns: only column 0 can be cut
    CHECK((seam::findVerticalSeamGraphCut(makeMatrix(3, 2, {9, 0, 9, 0, 9, 0})) == Seam{0, 0, 0}));
    // One row: cheapest of columns 0 .. cols-2
    CHECK((seam::findVerticalSeamGraphCut(makeMatrix(1, 4, {3, 1, 2, 0})) == Seam{1}));
    // Zero energy everywhere: flow is 0 and S is the first column
    CHECK((seam::findVerticalSeamGraphCut(Matrix(4, 5, 0.0)) == Seam{0, 0, 0, 0}));
    // Equal energy everywhere: any straight seam is optimal
    const Matrix flat(5, 6, 3.0);
    const Seam s = seam::findVerticalSeamGraphCut(flat);
    CHECK(seam::isValidSeam(s, 5, 5));
    CHECK(seam::seamCost(flat, s) == 15.0);
}

void testCarveWithGraphCut() {
    if (!requireGraphCut()) {
        return;
    }
    std::mt19937 rng(23);
    const Image img = randomImage(rng, 16, 12);
    const Image out = seam::carve(img, 12, 9, seam::Method::GraphCut);
    CHECK(out.width == 12 && out.height == 9);
}

void testGraphCutKeepsHighEnergyObject() {
    if (!requireGraphCut()) {
        return;
    }
    // seams must go around the checkered block
    Image img(30, 12);
    fillRect(img, 0, 30, 0, 12, 128);
    for (int row = 2; row < 10; ++row) {
        for (int col = 13; col < 17; ++col) {
            fillRect(img, col, col + 1, row, row + 1, ((row + col) % 2) ? 255 : 0);
        }
    }
    const Image out = seam::carve(img, 20, 12, seam::Method::GraphCut);

    int blockPixels = 0;
    for (std::size_t i = 0; i < out.data.size(); i += 3) {
        if (out.data[i] == 0 || out.data[i] == 255) {
            ++blockPixels;
        }
    }
    CHECK(blockPixels == 4 * 8);
}

} // namespace

struct TestCase {
    std::string name;
    void (*run)(); // pointer to the test function
};

int main() {
    const std::vector<TestCase> tests = {
        {"DP finds known seam", testDpFindsKnownSeam},
        {"DP matches brute force", testDpMatchesBruteForce},
        {"DP single column and single row", testDpSingleColumnAndSingleRow},
        {"isValidSeam", testIsValidSeam},
        {"energy is zero for flat image", testEnergyIsZeroForFlatImage},
        {"energy is high on edges", testEnergyIsHighOnEdges},
        {"transpose round trip", testTransposeRoundTrip},
        {"remove vertical seam", testRemoveVerticalSeam},
        {"carve reaches target size", testCarveReachesTargetSize},
        {"carve keeps high-energy object", testCarveKeepsHighEnergyObject},
        {"carve rejects invalid targets", testCarveRejectsInvalidTargets},
        {"callback sees every seam", testCallbackSeesEverySeam},
        {"compact DP matches original DP", testCompactDpMatchesOriginalDp},
        {"parallel DP matches original DP", testParallelDpMatchesOriginalDp},
        {"parallel energy matches serial", testParallelEnergyMatchesSerial},
        {"all DP versions carve the same image", testAllDpVersionsCarveTheSameImage},
        {"graph cut: source side follows residual edges", testSourceSideFollowsResidualEdges},
        {"graph cut: extract reads rightmost source pixel", testExtractReadsRightmostSourcePixel},
        {"graph cut: extract edge columns", testExtractEdgeColumns},
        {"graph cut: tiny residual counts as saturated", testExtractTreatsTinyResidualAsSaturated},
        {"graph cut: rejects reachable sink", testExtractRejectsReachableSink},
        {"graph cut: rejects row cut twice", testExtractRejectsRowCutTwice},
        {"graph cut: rejects row without source pixel", testExtractRejectsRowWithoutSourcePixel},
        {"graph cut: rejects disconnected seam", testExtractRejectsDisconnectedSeam},
        {"graph cut: rejects bad graph", testExtractRejectsBadGraph},
        {"graph cut: single column", testGraphCutSingleColumn},
        {"graph cut: matches brute force", testGraphCutMatchesBruteForce},
        {"graph cut: max flow equals seam cost", testMaxFlowEqualsSeamCost},
        {"graph cut: finds known seam", testGraphCutFindsKnownSeam},
        {"graph cut: matches DP when last column blocked", testGraphCutMatchesDpWhenLastColumnBlocked},
        {"graph cut: Sobel energy", testGraphCutOnSobelEnergy},
        {"graph cut: special shapes", testGraphCutSpecialShapes},
        {"graph cut: carve reaches target size", testCarveWithGraphCut},
        {"graph cut: keeps high-energy object", testGraphCutKeepsHighEnergyObject},
    };

    int skipped = 0;
    for (const TestCase& test : tests) {
        const int failuresBefore = g_failures;
        g_skipped = false;
        test.run();
        const char* result = g_failures != failuresBefore ? "[FAIL] " : g_skipped ? "[SKIP] " : "[PASS] ";
        skipped += g_skipped ? 1 : 0;
        std::cout << result << test.name << "\n";
    }
    if (skipped > 0) {
        std::cout << skipped << " test(s) skipped: graph construction / max-flow not merged yet\n";
    }
    std::cout << (g_failures == 0 ? "All tests passed\n" : "Some tests failed\n");
    return g_failures == 0 ? 0 : 1;
}
