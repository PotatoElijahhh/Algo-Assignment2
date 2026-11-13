// Unit tests for the seam carving core. Run with `ctest` or directly as seam_tests.
// Each test is a plain function; CHECK records a failure without stopping the test.

#include "seam/carver.hpp"
#include "seam/energy.hpp"
#include "seam/image.hpp"
#include "seam/interactive.hpp"
#include "seam/seam.hpp"
#include "seam/seam_order.hpp"
#include "seam/visualise.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
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

// ---------------------------------------------------------------- visualisation (bonus 7a)

// True if pixel (row, col) has exactly the colour c.
bool hasColour(const Image& img, int row, int col, seam::Colour c) {
    const std::uint8_t* p = img.pixel(row, col);
    return p[0] == c.r && p[1] == c.g && p[2] == c.b;
}

// Counts the pixels that differ between two images of the same size.
int countChangedPixels(const Image& a, const Image& b) {
    int changed = 0;
    for (int row = 0; row < a.height; ++row) {
        for (int col = 0; col < a.width; ++col) {
            const std::uint8_t* p = a.pixel(row, col);
            const std::uint8_t* q = b.pixel(row, col);
            if (p[0] != q[0] || p[1] != q[1] || p[2] != q[2]) {
                ++changed;
            }
        }
    }
    return changed;
}

void testPaintVerticalSeam() {
    Image img(4, 3); // 4 wide, 3 tall
    fillRect(img, 0, 4, 0, 3, 50);
    const Image before = img;
    const Seam s = {1, 2, 1}; // one column per row

    seam::paintSeam(img, s, seam::Orientation::Vertical, seam::kRed);

    CHECK(hasColour(img, 0, 1, seam::kRed));
    CHECK(hasColour(img, 1, 2, seam::kRed));
    CHECK(hasColour(img, 2, 1, seam::kRed));
    CHECK(countChangedPixels(before, img) == 3); // only the seam pixels changed
}

void testPaintHorizontalSeam() {
    Image img(4, 3); // 4 wide, 3 tall
    fillRect(img, 0, 4, 0, 3, 50);
    const Image before = img;
    const Seam s = {0, 1, 2, 2}; // one row per column

    seam::paintSeam(img, s, seam::Orientation::Horizontal, seam::kBlue);

    CHECK(hasColour(img, 0, 0, seam::kBlue));
    CHECK(hasColour(img, 1, 1, seam::kBlue));
    CHECK(hasColour(img, 2, 2, seam::kBlue));
    CHECK(hasColour(img, 2, 3, seam::kBlue));
    CHECK(countChangedPixels(before, img) == 4);
}

void testPaintSeamRejectsBadSeams() {
    Image img(4, 3);
    // Each entry is an invalid seam; painting any of them must throw.
    const std::vector<std::pair<Seam, seam::Orientation>> bad = {
        {{1, 2}, seam::Orientation::Vertical},          // too short (needs 3 rows)
        {{0, 2, 1}, seam::Orientation::Vertical},       // jumps two columns
        {{1, 4, 3}, seam::Orientation::Vertical},       // column 4 is out of range
        {{0, 1, 2}, seam::Orientation::Horizontal},     // too short (needs 4 columns)
        {{0, 1, 2, 3}, seam::Orientation::Horizontal},  // row 3 is out of range
    };
    for (const auto& entry : bad) {
        bool threw = false;
        try {
            seam::paintSeam(img, entry.first, entry.second, seam::kRed);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        CHECK(threw);
    }
}

// Every seam carve() reports can be painted, and exactly one pixel per row (vertical)
// or per column (horizontal) changes colour.
void testPaintEverySeamFromCarve() {
    std::mt19937 rng(8);
    const Image img = randomImage(rng, 20, 15);
    bool allOk = true;
    seam::carve(img, 14, 10, seam::Method::DynamicProgramming,
                [&allOk](const Image& current, const Seam& s, seam::Orientation orientation) {
                    // Paint on an all-black copy, so every seam pixel is guaranteed to change.
                    Image painted = current;
                    fillRect(painted, 0, painted.width, 0, painted.height, 0);
                    const Image black = painted;
                    seam::paintSeam(painted, s, orientation, seam::Colour{0, 255, 0});
                    const int expected =
                        orientation == seam::Orientation::Vertical ? current.height : current.width;
                    allOk = allOk && countChangedPixels(black, painted) == expected;
                });
    CHECK(allOk);
}

void testPadToSize() {
    Image img(2, 1);
    fillRect(img, 0, 2, 0, 1, 200);
    const seam::Colour grey{40, 40, 40};
    const Image padded = seam::padToSize(img, 4, 3, grey);
    CHECK(padded.width == 4 && padded.height == 3);
    CHECK(padded.pixel(0, 0)[0] == 200 && padded.pixel(0, 1)[0] == 200); // image in the top-left
    CHECK(hasColour(padded, 0, 2, grey));                                // right of the image
    CHECK(hasColour(padded, 2, 0, grey));                                // below the image

    bool threw = false;
    try {
        seam::padToSize(img, 1, 1, grey); // canvas smaller than the image
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

// Makes an empty temporary folder for a test and returns its path.
std::filesystem::path freshTempFolder(const std::string& name) {
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(folder);
    return folder;
}

void testFrameRecorderSavesEveryNthSeam() {
    std::mt19937 rng(21);
    const Image img = randomImage(rng, 12, 9);
    const std::filesystem::path folder = freshTempFolder("seam_frames_test_every");

    // 12x9 -> 9x7 removes 3 vertical then 2 horizontal seams (seams 1..5).
    // With every = 2, seams 1, 3 and 5 are saved, plus the final image.
    seam::FrameRecorder frames(folder.string(), 2);
    const Image out = seam::carve(img, 9, 7, seam::Method::DynamicProgramming,
                                  [&frames](const Image& current, const Seam& s, seam::Orientation o) {
                                      frames.record(current, s, o);
                                  });
    frames.saveFinal(out);

    CHECK(frames.seamsSeen() == 5);
    CHECK(frames.framesSaved() == 4);
    CHECK(std::filesystem::exists(folder / "frame_0001_v.png"));
    CHECK(!std::filesystem::exists(folder / "frame_0002_v.png")); // skipped
    CHECK(std::filesystem::exists(folder / "frame_0003_v.png"));
    CHECK(std::filesystem::exists(folder / "frame_0005_h.png")); // 5th seam is horizontal
    CHECK(std::filesystem::exists(folder / "frame_0006_final.png"));

    // Every frame is padded to the original 12x9.
    for (const auto& entry : std::filesystem::directory_iterator(folder)) {
        const Image frame = seam::loadImage(entry.path().string());
        CHECK(frame.width == 12 && frame.height == 9);
    }
    std::filesystem::remove_all(folder);
}

void testFirstFrameShowsTheFirstSeam() {
    std::mt19937 rng(22);
    Image img = randomImage(rng, 10, 6);
    // Avoid pure red pixels in the input, so the only red pixels in the frame are the seam.
    for (std::size_t i = 0; i < img.data.size(); i += 3) {
        img.data[i + 1] = std::max<std::uint8_t>(img.data[i + 1], 1);
    }
    const std::filesystem::path folder = freshTempFolder("seam_frames_test_first");

    seam::FrameRecorder frames(folder.string(), 1);
    seam::carve(img, 9, 6, seam::Method::DynamicProgramming,
                [&frames](const Image& current, const Seam& s, seam::Orientation o) {
                    frames.record(current, s, o);
                });

    // The first seam carve() removes is the DP seam of the original image's energy map.
    const Seam expected = seam::findVerticalSeamDP(seam::computeEnergy(img));
    const Image frame = seam::loadImage((folder / "frame_0001_v.png").string()); // PNG is lossless
    int redPixels = 0;
    for (int row = 0; row < frame.height; ++row) {
        CHECK(hasColour(frame, row, expected[row], seam::kRed));
        for (int col = 0; col < frame.width; ++col) {
            redPixels += hasColour(frame, row, col, seam::kRed) ? 1 : 0;
        }
    }
    CHECK(redPixels == frame.height); // exactly one red pixel per row: the seam
    std::filesystem::remove_all(folder);
}

void testFrameRecorderRejectsBadEvery() {
    bool threw = false;
    try {
        seam::FrameRecorder frames(freshTempFolder("seam_frames_test_bad").string(), 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

// Carves `img` to targetWidth x targetHeight while a SeamTracker records every seam.
// Returns the carved image; the tracker is filled in through the reference.
Image carveWithTracker(const Image& img, int targetWidth, int targetHeight, seam::SeamTracker& tracker) {
    return seam::carve(img, targetWidth, targetHeight, seam::Method::DynamicProgramming,
                       [&tracker](const Image& current, const Seam& s, seam::Orientation o) {
                           tracker.record(current, s, o);
                       });
}

void testTrackerFollowsEveryPixel() {
    // The key check: after carving, each pixel of the result must be exactly the original
    // pixel the tracker says it came from. Tried on several sizes, including width-only,
    // height-only and both.
    std::mt19937 rng(31);
    const int sizes[][4] = {{20, 15, 14, 15}, {20, 15, 20, 9}, {25, 18, 16, 11}, {8, 8, 1, 1}};
    for (const auto& size : sizes) {
        const Image img = randomImage(rng, size[0], size[1]);
        seam::SeamTracker tracker(img);
        const Image out = carveWithTracker(img, size[2], size[3], tracker);

        CHECK(tracker.currentWidth() == out.width && tracker.currentHeight() == out.height);
        bool allMatch = true;
        for (int row = 0; row < out.height; ++row) {
            for (int col = 0; col < out.width; ++col) {
                const int orig = tracker.originalIndex(row, col);
                const std::uint8_t* p = out.pixel(row, col);
                const std::uint8_t* q = img.pixel(orig / img.width, orig % img.width);
                allMatch = allMatch && p[0] == q[0] && p[1] == q[1] && p[2] == q[2];
            }
        }
        CHECK(allMatch);
    }
}

void testTrackerCountsRemovedPixels() {
    std::mt19937 rng(32);
    const Image img = randomImage(rng, 12, 9);
    seam::SeamTracker tracker(img);
    carveWithTracker(img, 9, 7, tracker);
    // 3 vertical seams on a 9-tall image (27 pixels) + 2 horizontal seams on a
    // 9-wide image (18 pixels) = 45 = 12*9 - 9*7. (markRemoved throws if any pixel
    // were removed twice, so this also proves every removed pixel is different.)
    CHECK(tracker.removedCount() == 3 * 9 + 2 * 9);
    CHECK(tracker.removedCount() == 12 * 9 - 9 * 7);
}

void testOverlayColoursRemovedPixels() {
    std::mt19937 rng(33);
    Image img = randomImage(rng, 16, 12);
    // Make sure no input pixel is pure red or pure blue, so only painted pixels match.
    for (std::size_t i = 0; i < img.data.size(); i += 3) {
        img.data[i + 1] = std::max<std::uint8_t>(img.data[i + 1], 1);
    }
    seam::SeamTracker tracker(img);
    carveWithTracker(img, 12, 10, tracker); // 4 vertical seams, then 2 horizontal seams
    const Image overlay = tracker.overlay();

    CHECK(overlay.width == 16 && overlay.height == 12); // drawn on the original
    int red = 0;
    int blue = 0;
    int unchanged = 0;
    for (int row = 0; row < overlay.height; ++row) {
        for (int col = 0; col < overlay.width; ++col) {
            if (hasColour(overlay, row, col, seam::kRed)) {
                ++red;
            } else if (hasColour(overlay, row, col, seam::kBlue)) {
                ++blue;
            } else {
                const std::uint8_t* p = overlay.pixel(row, col);
                const std::uint8_t* q = img.pixel(row, col);
                if (p[0] == q[0] && p[1] == q[1] && p[2] == q[2]) {
                    ++unchanged;
                }
            }
        }
    }
    CHECK(red == 4 * 12);           // 4 vertical seams, one pixel per row of the 12-tall image
    CHECK(blue == 2 * 12);          // 2 horizontal seams, one pixel per column of the 12-wide image
    CHECK(unchanged == 12 * 10);    // every kept pixel is untouched
}

void testTrackerRejectsWrongSize() {
    Image img(6, 4);
    seam::SeamTracker tracker(img);
    bool threw = false;
    try {
        tracker.record(Image(5, 4), Seam{0, 0, 0, 0}, seam::Orientation::Vertical); // not 6 wide
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

// ---------------------------------------------------------------- interactive mode (bonus 7b)

void testOutputNameForSize() {
    const std::filesystem::path expected = std::filesystem::path("results") / "balloon_300x200.png";
    CHECK(seam::outputNameForSize((std::filesystem::path("results") / "balloon.png").string(), 300, 200) ==
          expected.string());
    CHECK(seam::outputNameForSize("out.jpg", 5, 7) == "out_5x7.jpg");
}

void testInteractiveCarvesEachRequest() {
    std::mt19937 rng(41);
    const Image img = randomImage(rng, 16, 12);
    const std::filesystem::path folder = freshTempFolder("seam_interactive_test");
    std::filesystem::create_directories(folder);
    const std::string outputPath = (folder / "out.png").string();

    // What a user might type: two good sizes (the second BIGGER than the first, which works
    // because every request starts from the original), width only, then quit.
    std::istringstream typed("10 8\n14 11\n12\nq\n");
    std::ostringstream shown;
    const int saved = seam::runInteractive(typed, shown, img, outputPath, seam::Method::DynamicProgramming, 1);

    CHECK(saved == 3);
    const Image a = seam::loadImage((folder / "out_10x8.png").string());
    const Image b = seam::loadImage((folder / "out_14x11.png").string());
    const Image c = seam::loadImage((folder / "out_12x12.png").string()); // height kept at 12
    CHECK(a.width == 10 && a.height == 8);
    CHECK(b.width == 14 && b.height == 11);
    CHECK(c.width == 12 && c.height == 12);
    // Same pixels as calling carve() directly.
    CHECK(a.data == seam::carve(img, 10, 8, seam::Method::DynamicProgramming).data);
    std::filesystem::remove_all(folder);
}

void testInteractiveKeepsGoingAfterBadInput() {
    std::mt19937 rng(42);
    const Image img = randomImage(rng, 8, 6);
    const std::filesystem::path folder = freshTempFolder("seam_interactive_bad");
    std::filesystem::create_directories(folder);

    // Not numbers, extra words, too big, zero, a blank line, then one good size.
    // No "q": the end of the input must also stop the loop.
    std::istringstream typed("abc\n5 abc\n5 4 3\n20 4\n0 3\n\n5 4\n");
    std::ostringstream shown;
    const int saved = seam::runInteractive(typed, shown, img, (folder / "out.png").string(),
                                           seam::Method::DynamicProgramming, 1);

    CHECK(saved == 1); // only "5 4" worked
    CHECK(std::filesystem::exists(folder / "out_5x4.png"));
    const std::string text = shown.str();
    // 3 lines that are not sizes -> 3 hints; 2 sizes out of range -> 2 errors.
    int hints = 0;
    int errors = 0;
    for (std::size_t pos = 0; (pos = text.find("Please type", pos)) != std::string::npos; ++pos) {
        ++hints;
    }
    for (std::size_t pos = 0; (pos = text.find("Error:", pos)) != std::string::npos; ++pos) {
        ++errors;
    }
    CHECK(hints == 3);
    CHECK(errors == 2);
    std::filesystem::remove_all(folder);
}

// ---------------------------------------------------------------- seam order maps (GUI, bonus 7b)

void testVerticalOrderMatchesCarve() {
    // The key check: resizing with the precomputed order gives EXACTLY carve()'s pixels,
    // for every width the order covers.
    std::mt19937 rng(51);
    const Image img = randomImage(rng, 18, 11);
    const seam::SeamOrder order = seam::computeVerticalSeamOrder(img, 1, seam::Method::DynamicProgramming);
    CHECK(order.seamCount == 17);
    bool allMatch = true;
    for (int w = 1; w <= img.width; ++w) {
        const Image fast = seam::applyVerticalOrder(img, order, w);
        const Image slow = seam::carve(img, w, img.height, seam::Method::DynamicProgramming);
        allMatch = allMatch && fast.width == w && fast.data == slow.data;
    }
    CHECK(allMatch);
}

void testOrderHasOnePixelPerRowPerStep() {
    // Each vertical seam removes exactly one pixel per row, so in every row each step
    // 1..seamCount appears once, and the remaining (minWidth) pixels have step 0.
    std::mt19937 rng(52);
    const Image img = randomImage(rng, 14, 9);
    const seam::SeamOrder order = seam::computeVerticalSeamOrder(img, 4, seam::Method::DynamicProgramming);
    bool ok = true;
    for (int row = 0; row < img.height; ++row) {
        std::vector<int> seen(order.seamCount + 1, 0);
        for (int col = 0; col < img.width; ++col) {
            ++seen[static_cast<std::size_t>(order.at(row, col))];
        }
        ok = ok && seen[0] == 4; // 4 pixels never removed
        for (int k = 1; k <= order.seamCount; ++k) {
            ok = ok && seen[static_cast<std::size_t>(k)] == 1;
        }
    }
    CHECK(ok);
}

void testWidthThenHeightOrderMatchesCarve() {
    // What the GUI does: the vertical order gives the width; then a horizontal order computed
    // on that narrower image gives the height. Must equal carve(img, w, h).
    std::mt19937 rng(53);
    const Image img = randomImage(rng, 16, 13);
    const seam::SeamOrder vertical = seam::computeVerticalSeamOrder(img, 1, seam::Method::DynamicProgramming);
    bool allMatch = true;
    for (int w : {16, 12, 7}) {
        const Image narrow = seam::applyVerticalOrder(img, vertical, w);
        const seam::SeamOrder horizontal =
            seam::computeHorizontalSeamOrder(narrow, 1, seam::Method::DynamicProgramming);
        for (int h : {13, 10, 4, 1}) {
            const Image fast = seam::applyHorizontalOrder(narrow, horizontal, h);
            const Image slow = seam::carve(img, w, h, seam::Method::DynamicProgramming);
            allMatch = allMatch && fast.width == w && fast.height == h && fast.data == slow.data;
        }
    }
    CHECK(allMatch);
}

void testPaintRemovedPixelsMatchesTracker() {
    // The GUI's live "show seams" picture must be the same as the Step 3 overlay.
    std::mt19937 rng(54);
    const Image img = randomImage(rng, 15, 12);
    const int w = 10;
    const int h = 8;
    const seam::SeamOrder vertical = seam::computeVerticalSeamOrder(img, 1, seam::Method::DynamicProgramming);
    const Image narrow = seam::applyVerticalOrder(img, vertical, w);
    const seam::SeamOrder horizontal = seam::computeHorizontalSeamOrder(narrow, 1, seam::Method::DynamicProgramming);

    seam::SeamTracker tracker(img);
    carveWithTracker(img, w, h, tracker);
    CHECK(seam::paintRemovedPixels(img, vertical, w, horizontal, h).data == tracker.overlay().data);
}

void testSeamOrderProgressAndRange() {
    std::mt19937 rng(55);
    const Image img = randomImage(rng, 10, 6);
    int calls = 0;
    int lastDone = 0;
    const seam::SeamOrder order = seam::computeVerticalSeamOrder(
        img, 6, seam::Method::DynamicProgramming, 1, [&calls, &lastDone](int done, int total) {
            ++calls;
            lastDone = done;
            CHECK(total == 4);
        });
    CHECK(calls == 4 && lastDone == 4);

    // Widths below 6 were never computed, so they must be rejected.
    bool threw = false;
    try {
        seam::applyVerticalOrder(img, order, 5);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
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
        {"paint vertical seam", testPaintVerticalSeam},
        {"paint horizontal seam", testPaintHorizontalSeam},
        {"paint seam rejects bad seams", testPaintSeamRejectsBadSeams},
        {"paint every seam from carve", testPaintEverySeamFromCarve},
        {"pad to size", testPadToSize},
        {"frame recorder saves every N-th seam", testFrameRecorderSavesEveryNthSeam},
        {"first frame shows the first seam", testFirstFrameShowsTheFirstSeam},
        {"frame recorder rejects bad 'every'", testFrameRecorderRejectsBadEvery},
        {"tracker follows every pixel", testTrackerFollowsEveryPixel},
        {"tracker counts removed pixels", testTrackerCountsRemovedPixels},
        {"overlay colours removed pixels", testOverlayColoursRemovedPixels},
        {"tracker rejects wrong size", testTrackerRejectsWrongSize},
        {"output name for size", testOutputNameForSize},
        {"interactive carves each request", testInteractiveCarvesEachRequest},
        {"interactive keeps going after bad input", testInteractiveKeepsGoingAfterBadInput},
        {"vertical order matches carve", testVerticalOrderMatchesCarve},
        {"order has one pixel per row per step", testOrderHasOnePixelPerRowPerStep},
        {"width then height order matches carve", testWidthThenHeightOrderMatchesCarve},
        {"paint removed pixels matches tracker", testPaintRemovedPixelsMatchesTracker},
        {"seam order progress and range", testSeamOrderProgressAndRange},
    };

    for (const TestCase& test : tests) {
        const int failuresBefore = g_failures;
        test.run();
        std::cout << (g_failures == failuresBefore ? "[PASS] " : "[FAIL] ") << test.name << "\n";
    }
    std::cout << (g_failures == 0 ? "All tests passed\n" : "Some tests failed\n");
    return g_failures == 0 ? 0 : 1;
}
