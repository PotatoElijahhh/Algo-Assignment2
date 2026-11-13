#include "seam/visualise.hpp"

#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

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

} // namespace seam
