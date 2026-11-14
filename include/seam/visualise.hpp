#pragma once

// Visualisation helpers (assignment bonus item 7a: step-by-step visualisation).

#include "seam/carver.hpp" // Orientation
#include "seam/image.hpp"
#include "seam/seam.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace seam {

// An RGB colour used to draw seams.
struct Colour {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

const Colour kRed{255, 0, 0};  // vertical seams
const Colour kBlue{0, 0, 255}; // horizontal seams

// Sets pixel (row, col) of img to `colour`.
void paintPixel(Image& img, int row, int col, Colour colour);

// Copies pixel (fromRow, fromCol) of `from` to pixel (toRow, toCol) of `to`.
void copyPixel(const Image& from, int fromRow, int fromCol, Image& to, int toRow, int toCol);

// Like saveImage(), but first creates the folder the file goes in (and any missing parents),
// e.g. "results/" on a fresh clone, where it is git-ignored and does not exist yet.
void saveImageCreatingFolder(const Image& img, const std::string& path);

// Frame file name, zero-padded so that names sort in number order:
//   frameFileName(7, 1004, "v") == "frame_0007_v.png"
//   frameFileName(7, 12000, "v") == "frame_00007_v.png"
// `largestNumber` is the biggest number that can occur; at least 4 digits are always used.
std::string frameFileName(int number, int largestNumber, const std::string& suffix);

// Paints every pixel of `seam` on `img` in `colour`.
//   Vertical seam:   one entry per row,    seam[row] = column of the pixel (seam.size() == img.height).
//   Horizontal seam: one entry per column, seam[col] = row of the pixel    (seam.size() == img.width).
// This matches what carve() passes to its SeamCallback.
// Throws std::invalid_argument if the seam does not fit the image. Time O(seam length).
void paintSeam(Image& img, const Seam& seam, Orientation orientation, Colour colour);

// Returns `img` placed in the top-left corner of a width x height canvas filled with
// `background`. Used so every frame has the original size even though the carved image
// shrinks; that keeps the frames lined up when they are played as an animation.
// Throws std::invalid_argument if img is bigger than width x height.
Image padToSize(const Image& img, int width, int height, Colour background);

// Saves the step-by-step frames of a carve (assignment bonus 7a).
//
// Pass record() to carve() through its SeamCallback. Before each seam is removed,
// record() saves the current image with that seam painted (red = vertical,
// blue = horizontal), padded to the original size:
//     <folder>/frame_0001_v.png, frame_0002_v.png, ..., frame_0031_h.png, ...
// Only every `every`-th seam is saved (1 = all of them), counting from the first.
// After carving, call saveFinal() with the result to add it as the last frame.
class FrameRecorder {
public:
    // Creates `folder` (and any missing parent folders). Throws std::invalid_argument if every < 1.
    FrameRecorder(const std::string& folder, int every);

    // Called once per seam, with the arguments carve() gives its callback.
    void record(const Image& current, const Seam& seam, Orientation orientation);

    // Saves the finished image as the last frame (frame_NNNN_final.png).
    void saveFinal(const Image& result);

    int seamsSeen() const { return seamsSeen_; }   // how many seams record() was called for
    int framesSaved() const { return framesSaved_; } // how many PNG files were written

private:
    // Builds "<folder>/frame_0007_v.png" style paths (see frameFileName).
    std::string framePath(int number, const std::string& suffix) const;

    std::string folder_;
    int every_;
    int seamsSeen_ = 0;
    int framesSaved_ = 0;
    int canvasWidth_ = 0;  // size of the first image seen = the original image size
    int canvasHeight_ = 0;
};

// Shows all removed seams on the original image (assignment bonus 7a, "all the seams visualised").
//
// The problem: after a few seams are removed, pixel (row, col) of the current image is no
// longer pixel (row, col) of the original, because everything right of (or below) a removed
// pixel has shifted. So the tracker keeps a grid with the ORIGINAL position of every pixel
// that is still in the image. Each time a seam is removed from the image, the same entries
// are removed from the grid, and those original positions are marked as removed.
//
// Pass record() to carve() through its SeamCallback, then call overlay().
// Time O(W * H) per seam (same as removing the seam itself). Space O(W * H).
class SeamTracker {
public:
    explicit SeamTracker(const Image& original);

    // Called once per seam, with the arguments carve() gives its callback.
    // Throws std::invalid_argument if `current` is not the size the tracker expects
    // or the seam does not fit.
    void record(const Image& current, const Seam& seam, Orientation orientation);

    // The original image with every removed pixel painted:
    // red = removed by a vertical seam, blue = removed by a horizontal seam.
    Image overlay() const;

    // Original position (as row * originalWidth + col) of the pixel that is now at
    // (row, col) of the current image.
    int originalIndex(int row, int col) const;

    int currentWidth() const { return width_; }
    int currentHeight() const { return height_; }
    int removedCount() const { return removedCount_; } // pixels removed so far

private:
    // What happened to each pixel of the original image (1 byte each).
    enum class Status : std::uint8_t { Kept, RemovedVertical, RemovedHorizontal };

    void markRemoved(int originalPos, Status how);

    Image original_;
    int width_;  // current image size
    int height_;
    std::vector<int> grid_;      // width_ * height_ entries: original position of each current pixel
    std::vector<Status> status_; // one entry per pixel of the original image
    int removedCount_ = 0;
};

} // namespace seam
