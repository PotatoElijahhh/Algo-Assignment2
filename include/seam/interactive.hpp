#pragma once

// Interactive resizing (assignment bonus item 7b: "enable user to interactively edit image sizes").

#include "seam/image.hpp"
#include "seam/seam.hpp"

#include <iosfwd>
#include <string>

namespace seam {

// Returns `outputPath` with the size added before the extension:
//   outputNameForSize("results/balloon.png", 300, 200) == "results/balloon_300x200.png"
// so each size the user tries is kept as a separate file.
std::string outputNameForSize(const std::string& outputPath, int width, int height);

// Asks for target sizes until the user quits. Each line read from `in` is one of:
//   W H   carve to W x H
//   W     carve to width W, keeping the original height
//   q     quit (also "quit", any upper/lower case; end of input also quits)
// Spaces around the input are ignored.
// Every request carves from the ORIGINAL image (not the previous result), so any size up to
// the original can be chosen in any order. Each result is saved with outputNameForSize(),
// creating the output folder if it does not exist.
// Bad input or an out-of-range size prints a message and asks again instead of stopping.
// Returns the number of images saved.
int runInteractive(std::istream& in, std::ostream& out, const Image& input, const std::string& outputPath,
                   Method method, int threads);

} // namespace seam
