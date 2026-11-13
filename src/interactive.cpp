#include "seam/interactive.hpp"

#include "seam/carver.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <istream>
#include <ostream>
#include <sstream>

namespace seam {

std::string outputNameForSize(const std::string& outputPath, int width, int height) {
    const std::filesystem::path path(outputPath);
    // stem = name without folder or extension ("balloon"), extension = ".png"
    const std::string name =
        path.stem().string() + "_" + std::to_string(width) + "x" + std::to_string(height) + path.extension().string();
    return (path.parent_path() / name).string();
}

namespace {

// Reads "W H" or "W" from a line. Returns false if the line is not one or two whole numbers.
bool parseSize(const std::string& line, int defaultHeight, int& width, int& height) {
    std::istringstream words(line);
    if (!(words >> width)) {
        return false; // no number at the start
    }
    if (!(words >> height)) {
        if (!words.eof()) {
            return false; // something after the width that is not a number, e.g. "300 abc"
        }
        height = defaultHeight; // only a width was given
        return true;
    }
    std::string extra;
    return !(words >> extra); // reject anything after the two numbers, e.g. "300 200 7"
}

} // namespace

int runInteractive(std::istream& in, std::ostream& out, const Image& input, const std::string& outputPath,
                   Method method, int threads) {
    out << "Interactive mode. Original size: " << input.width << "x" << input.height << "\n"
        << "Enter a new size as 'W H' (or just 'W' to keep the height), or 'q' to quit.\n"
        << "Sizes can go from 1x1 up to the original; each one is carved from the original.\n";

    int saved = 0;
    std::string line;
    while (true) {
        out << "> " << std::flush;
        if (!std::getline(in, line)) {
            out << "\n";
            break; // end of input (Ctrl+Z then Enter on Windows, Ctrl+D on Linux)
        }
        if (!line.empty() && line.back() == '\r') {
            line.pop_back(); // input with Windows line endings
        }
        if (line == "q" || line == "quit") {
            break;
        }
        if (line.empty()) {
            continue;
        }

        int width = 0;
        int height = 0;
        if (!parseSize(line, input.height, width, height)) {
            out << "Please type 'W H', 'W' or 'q'.\n";
            continue;
        }

        try {
            const auto start = std::chrono::steady_clock::now();
            const Image result = carve(input, width, height, method, {}, threads);
            const auto end = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            const std::string path = outputNameForSize(outputPath, result.width, result.height);
            saveImage(result, path);
            ++saved;
            out << "Carved to " << result.width << "x" << result.height << " in " << ms << " ms, saved "
                << path << "\n";
        } catch (const std::exception& e) {
            // e.g. a size bigger than the original: report it and keep going.
            out << "Error: " << e.what() << "\n";
        }
    }
    out << "Saved " << saved << " image" << (saved == 1 ? "" : "s") << ".\n";
    return saved;
}

} // namespace seam
