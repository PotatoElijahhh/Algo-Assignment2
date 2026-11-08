// Command-line front end:
//   seamcarve <input> <output> [--width W] [--height H] [--method dp|dp-compact|greedy|graphcut]

#include "seam/carver.hpp"
#include "seam/image.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

namespace {

void printUsage() {
    std::cerr << "Usage: seamcarve <input> <output> [--width W] [--height H] [--method M]\n"
                 "  --width W    target width in pixels (default: keep current width)\n"
                 "  --height H   target height in pixels (default: keep current height)\n"
                 "  --method M   seam-finding method (default: dp):\n"
                 "                 dp          dynamic programming, full cost table\n"
                 "                 dp-compact  dynamic programming, memory-light (same result)\n"
                 "                 greedy      greedy row-by-row choice\n"
                 "                 graphcut    minimum cut / max-flow\n";
}

seam::Method parseMethod(const std::string& name) {
    if (name == "dp") {
        return seam::Method::DynamicProgramming;
    }
    if (name == "dp-compact") {
        return seam::Method::DynamicProgrammingCompact;
    }
    if (name == "greedy") {
        return seam::Method::Greedy;
    }
    if (name == "graphcut") {
        return seam::Method::GraphCut;
    }
    throw std::invalid_argument("unknown method '" + name + "' (use dp, dp-compact, greedy or graphcut)");
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

        for (int i = 3; i < argc; ++i) {
            const std::string arg = argv[i];
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
            } else {
                throw std::invalid_argument("unknown option " + arg);
            }
        }

        const seam::Method method = parseMethod(methodName);
        const seam::Image input = seam::loadImage(inputPath);
        if (targetWidth < 0) {
            targetWidth = input.width;
        }
        if (targetHeight < 0) {
            targetHeight = input.height;
        }

        const auto start = std::chrono::steady_clock::now();
        const seam::Image output = seam::carve(input, targetWidth, targetHeight, method);
        const auto end = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        seam::saveImage(output, outputPath);
        std::cout << "Carved " << input.width << "x" << input.height << " -> " << output.width << "x"
                  << output.height << " using " << methodName << " in " << ms << " ms\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        printUsage();
        return 1;
    }
}
