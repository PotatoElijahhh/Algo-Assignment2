// seambench: measures the DP improvements (assignment item 5d) and compares DP, greedy and
// graph cut (assignment items 3, 4 and 5c) for the report.
//
//   seambench <image>      [--repeat R] [--seams K] [--max-threads T] [--csv FILE]
//   seambench --size WxH   [--repeat R] [--seams K] [--max-threads T] [--csv FILE]
//
// Section 1  energy map: time with 1, 2, 4, ... threads
// Section 2  one seam:   original DP vs memory-light DP vs multithreaded DP (time + memory)
// Section 3  full carve: remove K vertical seams with each configuration
// Sections 1-3 also check that the improved versions give the same result as the original.
// Section 4  method comparison: DP vs greedy vs graph cut. Time for one seam, the seam's cost
//            (quality: lower is better, DP is optimal) and time to remove K seams.

#include "seam/carver.hpp"
#include "seam/energy.hpp"
#include "seam/image.hpp"
#include "seam/parallel.hpp"
#include "seam/seam.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

// One line of a results table (also written to the CSV file).
struct Result {
    std::string section;
    std::string version;
    int threads = 1;
    double timeMs = 0.0;
    double speedUp = 1.0;          // compared with the first (baseline) line of the section
    std::size_t memoryBytes = 0;   // 0 = not measured for this section
    bool sameResult = true;        // same output as the original version
    double seamCost = -1.0;        // total energy of the seam found (section 4 only; -1 = not used)
};

double millisecondsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

// Memory used by the DP tables (the energy map itself is needed by every version).
std::size_t fullDpBytes(int rows, int cols) {
    return static_cast<std::size_t>(rows) * cols * sizeof(double); // one double per pixel
}
std::size_t compactDpBytes(int rows, int cols) {
    return 2 * static_cast<std::size_t>(cols) * sizeof(double)     // two rows of costs
           + static_cast<std::size_t>(rows) * cols * sizeof(std::int8_t); // 1-byte parents
}

std::string formatBytes(std::size_t bytes) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2);
    if (bytes >= 1024 * 1024) {
        out << bytes / (1024.0 * 1024.0) << " MB";
    } else {
        out << bytes / 1024.0 << " KB";
    }
    return out.str();
}

// 1, 2, 4, 8, ... up to maxThreads, plus maxThreads itself if it isn't a power of two.
std::vector<int> threadCounts(int maxThreads) {
    std::vector<int> counts;
    for (int t = 1; t <= maxThreads; t *= 2) {
        counts.push_back(t);
    }
    if (counts.back() != maxThreads) {
        counts.push_back(maxThreads);
    }
    return counts;
}

seam::Image makeTestImage(int width, int height) {
    std::mt19937 rng(42);
    std::uniform_int_distribution<int> dist(0, 255);
    seam::Image img(width, height);
    for (std::size_t i = 0; i < img.data.size(); ++i) {
        img.data[i] = static_cast<std::uint8_t>(dist(rng));
    }
    return img;
}

void printTable(const std::string& title, const std::vector<Result>& rows, bool showMemory) {
    std::cout << "\n" << title << "\n";
    std::cout << std::left << std::setw(30) << "version" << std::right << std::setw(8) << "threads"
              << std::setw(12) << "time (ms)" << std::setw(10) << "speed-up";
    if (showMemory) {
        std::cout << std::setw(14) << "DP memory";
    }
    std::cout << std::setw(14) << "same result" << "\n";
    std::cout << std::string(showMemory ? 88 : 74, '-') << "\n";

    for (const Result& r : rows) {
        std::cout << std::left << std::setw(30) << r.version << std::right << std::setw(8) << r.threads
                  << std::setw(12) << std::fixed << std::setprecision(2) << r.timeMs << std::setw(9)
                  << r.speedUp << "x";
        if (showMemory) {
            std::cout << std::setw(14) << formatBytes(r.memoryBytes);
        }
        std::cout << std::setw(14) << (r.sameResult ? "yes" : "NO") << "\n";
    }
}

void printUsage() {
    std::cerr << "Usage: seambench <image> [options]\n"
                 "       seambench --size WxH [options]   (uses a random test image)\n"
                 "Options:\n"
                 "  --repeat R       runs to average for sections 1 and 2 (default 5)\n"
                 "  --seams K        vertical seams to remove in section 3 (default 50)\n"
                 "  --max-threads T  most threads to try (default: all available)\n"
                 "  --csv FILE       also save all results to a CSV file\n"
                 "  --graphcut-max-pixels N\n"
                 "                   largest image (width*height) on which section 4 runs graph cut\n"
                 "                   (default 20000, about 140x140, because Edmonds-Karp is slow)\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    try {
        std::string imagePath;
        int sizeW = 0;
        int sizeH = 0;
        int repeat = 5;
        int seamsToRemove = 50;
        int maxThreads = seam::availableThreads();
        std::string csvPath;
        int graphCutMaxPixels = 20000;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg.rfind("--", 0) != 0) { // does not start with "--": it's the image path
                imagePath = arg;
                continue;
            }
            if (i + 1 >= argc) {
                throw std::invalid_argument("missing value after " + arg);
            }
            const std::string value = argv[++i];
            if (arg == "--size") {
                const std::size_t x = value.find('x');
                if (x == std::string::npos) {
                    throw std::invalid_argument("--size must look like 800x600");
                }
                sizeW = std::stoi(value.substr(0, x));
                sizeH = std::stoi(value.substr(x + 1));
            } else if (arg == "--repeat") {
                repeat = std::stoi(value);
            } else if (arg == "--seams") {
                seamsToRemove = std::stoi(value);
            } else if (arg == "--max-threads") {
                maxThreads = std::stoi(value);
            } else if (arg == "--csv") {
                csvPath = value;
            } else if (arg == "--graphcut-max-pixels") {
                graphCutMaxPixels = std::stoi(value);
            } else {
                throw std::invalid_argument("unknown option " + arg);
            }
        }

        if (repeat < 1 || seamsToRemove < 1 || maxThreads < 1) {
            throw std::invalid_argument("--repeat, --seams and --max-threads must be at least 1");
        }
        if (!seam::hasOpenMP()) {
            maxThreads = 1;
        }

        seam::Image img;
        std::string source;
        if (!imagePath.empty()) {
            img = seam::loadImage(imagePath);
            source = imagePath;
        } else if (sizeW > 1 && sizeH > 0) {
            img = makeTestImage(sizeW, sizeH);
            source = "random test image";
        } else {
            throw std::invalid_argument("give an image file or --size WxH");
        }
        if (seamsToRemove > img.width - 1) {
            seamsToRemove = img.width - 1;
        }

        std::cout << "seambench: " << source << ", " << img.width << "x" << img.height << " pixels\n"
                  << "OpenMP: " << (seam::hasOpenMP() ? "yes" : "no") << ", up to " << maxThreads
                  << " threads | averages over " << repeat << " runs\n";

        std::vector<Result> all;
        const std::vector<int> threadList = threadCounts(maxThreads);

        // ---------------------------------------------------------------- 1. energy map
        std::vector<Result> energyRows;
        const seam::Matrix energySerial = seam::computeEnergy(img, 1);
        for (int t : threadList) {
            seam::Matrix energy;
            const Clock::time_point start = Clock::now();
            for (int r = 0; r < repeat; ++r) {
                energy = seam::computeEnergy(img, t);
            }
            Result res;
            res.section = "energy";
            res.version = "Sobel energy map";
            res.threads = t;
            res.timeMs = millisecondsSince(start) / repeat;
            res.sameResult = (energy.values == energySerial.values);
            energyRows.push_back(res);
        }
        for (Result& r : energyRows) {
            r.speedUp = energyRows[0].timeMs / r.timeMs;
        }
        printTable("[1] Energy map (one full image)", energyRows, false);
        all.insert(all.end(), energyRows.begin(), energyRows.end());

        // ---------------------------------------------------------------- 2. one seam
        std::vector<Result> seamRows;
        const seam::Seam reference = seam::findVerticalSeamDP(energySerial);
        const int rows = energySerial.rows;
        const int cols = energySerial.cols;

        {   // original DP (full table)
            seam::Seam s;
            const Clock::time_point start = Clock::now();
            for (int r = 0; r < repeat; ++r) {
                s = seam::findVerticalSeamDP(energySerial);
            }
            Result res;
            res.section = "one seam";
            res.version = "dp (original, full table)";
            res.timeMs = millisecondsSince(start) / repeat;
            res.memoryBytes = fullDpBytes(rows, cols);
            res.sameResult = (s == reference);
            seamRows.push_back(res);
        }
        {   // memory-light DP
            seam::Seam s;
            const Clock::time_point start = Clock::now();
            for (int r = 0; r < repeat; ++r) {
                s = seam::findVerticalSeamDPCompact(energySerial);
            }
            Result res;
            res.section = "one seam";
            res.version = "dp-compact (memory-light)";
            res.timeMs = millisecondsSince(start) / repeat;
            res.memoryBytes = compactDpBytes(rows, cols);
            res.sameResult = (s == reference);
            seamRows.push_back(res);
        }
        for (int t : threadList) { // multithreaded DP
            if (t == 1) {
                continue; // 1 thread is the same as dp-compact
            }
            seam::Seam s;
            const Clock::time_point start = Clock::now();
            for (int r = 0; r < repeat; ++r) {
                s = seam::findVerticalSeamDPParallel(energySerial, t);
            }
            Result res;
            res.section = "one seam";
            res.version = "dp-parallel (multithreaded)";
            res.threads = t;
            res.timeMs = millisecondsSince(start) / repeat;
            res.memoryBytes = compactDpBytes(rows, cols);
            res.sameResult = (s == reference);
            seamRows.push_back(res);
        }
        for (Result& r : seamRows) {
            r.speedUp = seamRows[0].timeMs / r.timeMs;
        }
        printTable("[2] Find one vertical seam (energy map already computed)", seamRows, true);
        all.insert(all.end(), seamRows.begin(), seamRows.end());

        // ---------------------------------------------------------------- 3. full carve
        struct Config {
            std::string name;
            seam::Method method;
            int threads;
        };
        const std::vector<Config> configs = {
            {"dp (original)", seam::Method::DynamicProgramming, 1},
            {"dp-compact", seam::Method::DynamicProgrammingCompact, 1},
            {"dp-compact + threaded energy", seam::Method::DynamicProgrammingCompact, maxThreads},
            {"dp-parallel + threaded energy", seam::Method::DynamicProgrammingParallel, maxThreads},
        };
        const int targetWidth = img.width - seamsToRemove;
        std::vector<Result> carveRows;
        seam::Image referenceImage;
        for (const Config& c : configs) {
            const Clock::time_point start = Clock::now();
            const seam::Image out = seam::carve(img, targetWidth, img.height, c.method, {}, c.threads);
            Result res;
            res.section = "carve";
            res.version = c.name;
            res.threads = c.threads;
            res.timeMs = millisecondsSince(start);
            if (carveRows.empty()) {
                referenceImage = out;
            }
            res.sameResult = (out.data == referenceImage.data);
            carveRows.push_back(res);
        }
        for (Result& r : carveRows) {
            r.speedUp = carveRows[0].timeMs / r.timeMs;
        }
        printTable("[3] Remove " + std::to_string(seamsToRemove) + " vertical seams (" +
                       std::to_string(img.width) + " -> " + std::to_string(targetWidth) + " wide)",
                   carveRows, false);
        all.insert(all.end(), carveRows.begin(), carveRows.end());

        // ---------------------------------------------------------------- 4. method comparison
        // Every method gets the same energy map and removes the same number of seams, so the
        // numbers can be compared directly. "vs DP" is DP's time divided by the method's time.
        // "optimal" means the seam is exactly as cheap as the DP seam (DP is always optimal).
        struct MethodConfig {
            std::string name;
            seam::Method method;
        };
        const std::vector<MethodConfig> methods = {
            {"dp", seam::Method::DynamicProgramming},
            {"greedy", seam::Method::Greedy},
            {"graphcut", seam::Method::GraphCut},
        };
        const bool graphCutAllowed = img.width * img.height <= graphCutMaxPixels;
        const double dpCost = seam::seamCost(energySerial, reference);

        std::vector<Result> methodSeamRows;
        std::vector<Result> methodCarveRows;
        std::vector<std::string> notes;
        for (const MethodConfig& m : methods) {
            if (m.method == seam::Method::GraphCut && !graphCutAllowed) {
                notes.push_back(m.name + " skipped: image is larger than --graphcut-max-pixels");
                continue;
            }
            try {
                // One seam, averaged over `repeat` runs.
                seam::Seam s;
                Clock::time_point start = Clock::now();
                for (int r = 0; r < repeat; ++r) {
                    s = seam::findVerticalSeam(energySerial, m.method);
                }
                Result one;
                one.section = "method one seam";
                one.version = m.name;
                one.timeMs = millisecondsSince(start) / repeat;
                one.seamCost = seam::seamCost(energySerial, s);
                one.sameResult = (one.seamCost == dpCost); // as cheap as the optimal seam?
                methodSeamRows.push_back(one);

                // Remove K vertical seams (the energy map is recomputed after every seam).
                start = Clock::now();
                seam::carve(img, targetWidth, img.height, m.method);
                Result full;
                full.section = "method carve";
                full.version = m.name;
                full.timeMs = millisecondsSince(start);
                methodCarveRows.push_back(full);
            } catch (const std::logic_error& e) { // e.g. a method that is not implemented yet
                notes.push_back(m.name + " skipped: " + e.what());
            }
        }
        for (Result& r : methodSeamRows) {
            r.speedUp = methodSeamRows[0].timeMs / r.timeMs;
        }
        for (Result& r : methodCarveRows) {
            r.speedUp = methodCarveRows[0].timeMs / r.timeMs;
        }

        std::cout << "\n[4a] Method comparison: find one vertical seam (energy map already computed)\n";
        std::cout << std::left << std::setw(12) << "method" << std::right << std::setw(12) << "time (ms)"
                  << std::setw(10) << "vs DP" << std::setw(14) << "seam cost" << std::setw(12)
                  << "cost / DP" << std::setw(10) << "optimal" << "\n";
        std::cout << std::string(70, '-') << "\n";
        for (const Result& r : methodSeamRows) {
            const double ratio = dpCost > 0 ? r.seamCost / dpCost : 1.0;
            std::cout << std::left << std::setw(12) << r.version << std::right << std::fixed
                      << std::setprecision(4) << std::setw(12) << r.timeMs << std::setprecision(2)
                      << std::setw(9) << r.speedUp << "x" << std::setw(14) << r.seamCost << std::setw(12)
                      << ratio << std::setw(10) << (r.sameResult ? "yes" : "no") << "\n";
        }
        std::cout << "\n[4b] Method comparison: remove " << seamsToRemove << " vertical seams ("
                  << img.width << " -> " << targetWidth << " wide)\n";
        std::cout << std::left << std::setw(12) << "method" << std::right << std::setw(12) << "time (ms)"
                  << std::setw(10) << "vs DP" << "\n";
        std::cout << std::string(34, '-') << "\n";
        for (const Result& r : methodCarveRows) {
            std::cout << std::left << std::setw(12) << r.version << std::right << std::fixed
                      << std::setprecision(2) << std::setw(12) << r.timeMs << std::setw(9) << r.speedUp
                      << "x\n";
        }
        for (const std::string& note : notes) {
            std::cout << "  note: " << note << "\n";
        }
        all.insert(all.end(), methodSeamRows.begin(), methodSeamRows.end());
        all.insert(all.end(), methodCarveRows.begin(), methodCarveRows.end());

        // ---------------------------------------------------------------- CSV
        if (!csvPath.empty()) {
            std::ofstream csv(csvPath);
            if (!csv) {
                throw std::runtime_error("could not write " + csvPath);
            }
            csv << "image,width,height,section,version,threads,time_ms,speed_up,memory_bytes,same_result,seam_cost\n";
            for (const Result& r : all) {
                csv << '"' << source << "\"," << img.width << ',' << img.height << ',' << r.section << ','
                    << r.version << ',' << r.threads << ',' << r.timeMs << ',' << r.speedUp << ','
                    << r.memoryBytes << ',' << (r.sameResult ? "yes" : "no") << ',';
                if (r.seamCost >= 0) {
                    csv << r.seamCost; // only section 4 records a seam cost
                }
                csv << "\n";
            }
            std::cout << "\nSaved results to " << csvPath << "\n";
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        printUsage();
        return 1;
    }
}
