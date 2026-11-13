#include "seam/visualise.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace seam {

namespace {

// Sets one pixel to the given colour.
void setPixel(Image& img, int row, int col, Colour colour) {
    std::uint8_t* p = img.pixel(row, col);
    p[0] = colour.r;
    p[1] = colour.g;
    p[2] = colour.b;
}

} // namespace

void paintSeam(Image& img, const Seam& seam, Orientation orientation, Colour colour) {
    if (orientation == Orientation::Vertical) {
        // One entry per row, each a column in [0, width).
        if (!isValidSeam(seam, img.height, img.width)) {
            throw std::invalid_argument("paintSeam: vertical seam does not fit the image");
        }
        for (int row = 0; row < img.height; ++row) {
            setPixel(img, row, seam[row], colour);
        }
    } else {
        // A horizontal seam is a vertical seam of the transposed image, so the same
        // validity check works with rows and columns swapped: one entry per column,
        // each a row in [0, height).
        if (!isValidSeam(seam, img.width, img.height)) {
            throw std::invalid_argument("paintSeam: horizontal seam does not fit the image");
        }
        for (int col = 0; col < img.width; ++col) {
            setPixel(img, seam[col], col, colour);
        }
    }
}

Image padToSize(const Image& img, int width, int height, Colour background) {
    if (img.width > width || img.height > height) {
        throw std::invalid_argument("padToSize: image is bigger than the canvas");
    }
    Image canvas(width, height);
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            if (row < img.height && col < img.width) {
                // Inside the image: copy its pixel.
                const std::uint8_t* src = img.pixel(row, col);
                std::uint8_t* dst = canvas.pixel(row, col);
                dst[0] = src[0];
                dst[1] = src[1];
                dst[2] = src[2];
            } else {
                // Area the carving has already removed: fill with the background.
                setPixel(canvas, row, col, background);
            }
        }
    }
    return canvas;
}

// ---------------------------------------------------------------- FrameRecorder

namespace {

const Colour kFrameBackground{40, 40, 40}; // dark grey behind the shrinking image

} // namespace

FrameRecorder::FrameRecorder(const std::string& folder, int every) : folder_(folder), every_(every) {
    if (every < 1) {
        throw std::invalid_argument("FrameRecorder: 'every' must be at least 1");
    }
    std::filesystem::create_directories(folder_); // does nothing if it already exists
}

std::string FrameRecorder::framePath(int number, const std::string& suffix) const {
    // e.g. number 7, suffix "v"  ->  "<folder>/frame_0007_v.png"
    // Zero-padding makes the files sort in the right order in File Explorer.
    std::ostringstream name;
    name << "frame_" << std::setw(4) << std::setfill('0') << number << "_" << suffix << ".png";
    return (std::filesystem::path(folder_) / name.str()).string();
}

void FrameRecorder::record(const Image& current, const Seam& seam, Orientation orientation) {
    if (seamsSeen_ == 0) {
        // The first callback comes before anything is removed, so this is the original size.
        canvasWidth_ = current.width;
        canvasHeight_ = current.height;
    }
    ++seamsSeen_;

    // Save seams 1, 1 + every, 1 + 2*every, ... (always including the first one).
    if ((seamsSeen_ - 1) % every_ != 0) {
        return;
    }

    Image frame = current; // copy, so the image being carved is not changed
    const bool vertical = orientation == Orientation::Vertical;
    paintSeam(frame, seam, orientation, vertical ? kRed : kBlue);
    saveImage(padToSize(frame, canvasWidth_, canvasHeight_, kFrameBackground),
              framePath(seamsSeen_, vertical ? "v" : "h"));
    ++framesSaved_;
}

void FrameRecorder::saveFinal(const Image& result) {
    if (seamsSeen_ == 0) {
        // No seams were removed, so the result is the original size.
        canvasWidth_ = result.width;
        canvasHeight_ = result.height;
    }
    // Numbered one past the last seam, so it sorts after all the seam frames.
    saveImage(padToSize(result, canvasWidth_, canvasHeight_, kFrameBackground),
              framePath(seamsSeen_ + 1, "final"));
    ++framesSaved_;
}

// ---------------------------------------------------------------- SeamTracker

SeamTracker::SeamTracker(const Image& original)
    : original_(original),
      width_(original.width),
      height_(original.height),
      grid_(static_cast<std::size_t>(original.width) * original.height),
      status_(grid_.size(), Status::Kept) {
    // At the start nothing has moved: the pixel at position p is original pixel p.
    for (std::size_t p = 0; p < grid_.size(); ++p) {
        grid_[p] = static_cast<int>(p);
    }
}

int SeamTracker::originalIndex(int row, int col) const {
    return grid_[static_cast<std::size_t>(row) * width_ + col];
}

void SeamTracker::markRemoved(int originalPos, Status how) {
    const std::size_t p = static_cast<std::size_t>(originalPos);
    if (status_[p] != Status::Kept) {
        // Would mean the tracking is wrong: a pixel can only be removed once.
        throw std::logic_error("SeamTracker: pixel removed twice");
    }
    status_[p] = how;
    ++removedCount_;
}

void SeamTracker::record(const Image& current, const Seam& seam, Orientation orientation) {
    if (current.width != width_ || current.height != height_) {
        throw std::invalid_argument("SeamTracker: image size does not match the tracked size");
    }

    if (orientation == Orientation::Vertical) {
        if (!isValidSeam(seam, height_, width_)) {
            throw std::invalid_argument("SeamTracker: vertical seam does not fit the image");
        }
        // Same as removeVerticalSeam(), but on the grid of original positions:
        // in each row, drop the entry at column seam[row]; the rest shift left.
        std::vector<int> next(static_cast<std::size_t>(width_ - 1) * height_);
        for (int row = 0; row < height_; ++row) {
            int outCol = 0;
            for (int col = 0; col < width_; ++col) {
                if (col == seam[row]) {
                    markRemoved(originalIndex(row, col), Status::RemovedVertical);
                    continue;
                }
                next[static_cast<std::size_t>(row) * (width_ - 1) + outCol] = originalIndex(row, col);
                ++outCol;
            }
        }
        grid_ = std::move(next);
        --width_;
    } else {
        if (!isValidSeam(seam, width_, height_)) {
            throw std::invalid_argument("SeamTracker: horizontal seam does not fit the image");
        }
        // Horizontal seam: seam[col] = row removed in that column; the pixels below it
        // shift up by one. (carve() does this by transposing; here we do it directly.)
        std::vector<int> next(static_cast<std::size_t>(width_) * (height_ - 1));
        for (int col = 0; col < width_; ++col) {
            int outRow = 0;
            for (int row = 0; row < height_; ++row) {
                if (row == seam[col]) {
                    markRemoved(originalIndex(row, col), Status::RemovedHorizontal);
                    continue;
                }
                next[static_cast<std::size_t>(outRow) * width_ + col] = originalIndex(row, col);
                ++outRow;
            }
        }
        grid_ = std::move(next);
        --height_;
    }
}

Image SeamTracker::overlay() const {
    Image out = original_;
    for (int row = 0; row < out.height; ++row) {
        for (int col = 0; col < out.width; ++col) {
            const Status s = status_[static_cast<std::size_t>(row) * out.width + col];
            if (s == Status::RemovedVertical) {
                setPixel(out, row, col, kRed);
            } else if (s == Status::RemovedHorizontal) {
                setPixel(out, row, col, kBlue);
            }
        }
    }
    return out;
}

} // namespace seam
