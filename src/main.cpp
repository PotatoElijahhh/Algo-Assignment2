// Command-line front end:
//   seamcarve <input> <output> [--width W] [--height H] [--method M] [--threads N]
//             [--frames DIR] [--frame-every N] [--overlay FILE] [--interactive]

#include "seam/carver.hpp"
#include "seam/image.hpp"
#include "seam/interactive.hpp"
#include "seam/parallel.hpp"
#include "seam/visualise.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

namespace {

void printUsage() {
    std::cerr << "Usage: seamcarve <input> <output> [--width W] [--height H] [--method M]\n"
                 "  --width W    target width in pixels (default: keep current width)\n"
                 "  --height H   target height in pixels (default: keep current height)\n"
                 "  --method M   seam-finding method (default: dp):\n"
                 "                 dp          dynamic programming, full cost table\n"
                 "                 dp-compact  dynamic programming, memory-light (same result)\n"
                 "                 dp-parallel memory-light DP with each row split across threads\n"
                 "                 greedy      greedy row-by-row choice\n"
                 "                 graphcut    minimum cut / max-flow\n"
                 "  --threads N  number of threads for the energy map, and for the DP with\n"
                 "               dp-parallel (default: 1)\n"
                 "  --frames DIR  save a PNG of every step into folder DIR, with the seam about\n"
                 "               to be removed in red (vertical) or blue (horizontal)\n"
                 "  --frame-every N  with --frames, only save every N-th seam (default: 1)\n"
                 "  --overlay FILE  save the original image with every removed seam painted\n"
                 "               (red = vertical, blue = horizontal)\n"
                 "  --interactive  keep asking for new sizes; each result is saved as\n"
                 "               <output>_WxH (e.g. out_300x200.png). --width/--height are ignored\n";
}

seam::Method parseMethod(const std::string& name) {
    if (name == "dp") {
        return seam::Method::DynamicProgramming;
    }
    if (name == "dp-compact") {
        return seam::Method::DynamicProgrammingCompact;
    }
    if (name == "dp-parallel") {
        return seam::Method::DynamicProgrammingParallel;
    }
    if (name == "greedy") {
        return seam::Method::Greedy;
    }
    if (name == "graphcut") {
        return seam::Method::GraphCut;
    }
    throw std::invalid_argument("unknown method '" + name + "' (use dp, dp-compact, dp-parallel, greedy or graphcut)");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    try {
        const std::string inputPath = argv[1];
        const std::string outputPath = argv[2];
        int targetWidth = -1;
        int targetHeight = -1;
        std::string methodName = "dp";
        int threads = 1;
        std::string framesFolder; // empty = don't save frames
        int frameEvery = 1;
        std::string overlayPath; // empty = don't save the overlay
        bool interactive = false;

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
            // Flags without a value come first; every other option is followed by a value.
            if (arg == "--interactive") {
                interactive = true;
                continue;
            }
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value after " + arg);
            }
            const std::string value = argv[++i];
            if (arg == "--width") {
                targetWidth = std::stoi(value);
            } else if (arg == "--height") {
                targetHeight = std::stoi(value);
            } else if (arg == "--method") {
                methodName = value;
            } else if (arg == "--threads") {
                threads = std::stoi(value);
            } else if (arg == "--frames") {
                framesFolder = value;
            } else if (arg == "--frame-every") {
                frameEvery = std::stoi(value);
            } else if (arg == "--overlay") {
                overlayPath = value;
            } else {
                throw std::invalid_argument("unknown option " + arg);
            }
        }

        const seam::Method method = parseMethod(methodName);
        if (threads < 1) {
            throw std::invalid_argument("--threads must be at least 1");
        }
        if (threads > 1 && !seam::hasOpenMP()) {
            std::cout << "Note: built without OpenMP, so running on 1 thread\n";
            threads = 1;
        }
        const seam::Image input = seam::loadImage(inputPath);

        if (interactive) {
            // Tell the user about options that interactive mode does not use, instead of
            // silently ignoring them.
            if (targetWidth >= 0 || targetHeight >= 0) {
                std::cout << "Note: --width and --height are not used in interactive mode; type the sizes below\n";
            }
            if (!framesFolder.empty() || !overlayPath.empty()) {
                std::cout << "Note: --frames and --overlay are not used in interactive mode\n";
            }
            seam::runInteractive(std::cin, std::cout, input, outputPath, method, threads);
            return 0;
        }
        if (targetWidth < 0) {
            targetWidth = input.width;
        }
        if (targetHeight < 0) {
            targetHeight = input.height;
        }

        if (framesFolder.empty() && frameEvery != 1) {
            std::cout << "Note: --frame-every only has an effect together with --frames\n";
        }

        // Visualisation: --frames and --overlay each need to see every seam before it is
        // removed. Only the ones asked for are created (nullptr = off).
        std::unique_ptr<seam::FrameRecorder> frames;
        std::unique_ptr<seam::SeamTracker> tracker;
        if (!framesFolder.empty()) {
            frames = std::make_unique<seam::FrameRecorder>(framesFolder, frameEvery);
        }
        if (!overlayPath.empty()) {
            tracker = std::make_unique<seam::SeamTracker>(input);
        }

        // carve() calls this before removing each seam. If neither option is on it stays
        // empty, and carve() skips it.
        seam::SeamCallback onSeam;
        if (frames || tracker) {
            onSeam = [&frames, &tracker](const seam::Image& current, const seam::Seam& s,
                                         seam::Orientation o) {
                if (frames) {
                    frames->record(current, s, o);
                }
                if (tracker) {
                    tracker->record(current, s, o);
                }
            };
        }

        const auto start = std::chrono::steady_clock::now();
        const seam::Image output = seam::carve(input, targetWidth, targetHeight, method, onSeam, threads);
        const auto end = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        seam::saveImageCreatingFolder(output, outputPath); // creates e.g. results/ if needed
        std::cout << "Carved " << input.width << "x" << input.height << " -> " << output.width << "x"
                  << output.height << " using " << methodName << " (" << threads << " thread"
                  << (threads == 1 ? "" : "s") << ") in " << ms << " ms";
        if (frames) {
            // Writing PNGs is slow, so the time above includes it when frames are on.
            std::cout << " (including saving frames)";
        }
        std::cout << "\n";

        if (frames) {
            frames->saveFinal(output);
            std::cout << "Saved " << frames->framesSaved() << " frames for " << frames->seamsSeen()
                      << " seams to " << framesFolder << "\n";
        }
        if (tracker) {
            seam::saveImageCreatingFolder(tracker->overlay(), overlayPath); // folder may not exist yet
            std::cout << "Saved overlay of " << tracker->removedCount() << " removed pixels to "
                      << overlayPath << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        printUsage();
        return 1;
    }
}
