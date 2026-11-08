// Unit tests for the seam carving core. Run with `ctest` or directly as seam_tests.
// Each test is a plain function; CHECK records a failure without stopping the test.

#include "seam/carver.hpp"
#include "seam/energy.hpp"
#include "seam/image.hpp"
#include "seam/seam.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

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
void bruteForceWalk(const Matrix& e, int row, int col, double sumSoFar, double& best) {
    const double sum = sumSoFar + e(row, col);
    if (row == e.rows - 1) {
        if (sum < best) {
            best = sum;
        }
        return;
    }
    for (int step = -1; step <= 1; ++step) { // down-left, straight down, down-right
        const int next = col + step;
        if (next >= 0 && next < e.cols) {
            bruteForceWalk(e, row + 1, next, sum, best);
        }
    }
}

// Tries every possible vertical seam (up to 3^rows of them) and returns the lowest cost.
// Exponential time, so only used on tiny grids to check the DP answer.
double bruteForceMinSeamCost(const Matrix& e) {
    double best = std::numeric_limits<double>::infinity();
    for (int col = 0; col < e.cols; ++col) {
        bruteForceWalk(e, 0, col, 0.0, best);
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
    };

    for (const TestCase& test : tests) {
        const int failuresBefore = g_failures;
        test.run();
        std::cout << (g_failures == failuresBefore ? "[PASS] " : "[FAIL] ") << test.name << "\n";
    }
    std::cout << (g_failures == 0 ? "All tests passed\n" : "Some tests failed\n");
    return g_failures == 0 ? 0 : 1;
}
