#pragma once

// Visualisation helpers (assignment bonus item 7a: step-by-step visualisation).

#include "seam/carver.hpp" // Orientation
#include "seam/image.hpp"
#include "seam/seam.hpp"

#include <cstdint>
#include <string>

namespace seam {

// An RGB colour used to draw seams.
struct Colour {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

const Colour kRed{255, 0, 0};  // vertical seams
const Colour kBlue{0, 0, 255}; // horizontal seams

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
    // Builds "<folder>/frame_0007_v.png" style paths (number padded to 4 digits).
    std::string framePath(int number, const std::string& suffix) const;

    std::string folder_;
    int every_;
    int seamsSeen_ = 0;
    int framesSaved_ = 0;
    int canvasWidth_ = 0;  // size of the first image seen = the original image size
    int canvasHeight_ = 0;
};

} // namespace seam
