# Algo-Assignment2: Image Resizing by Seam Carving

Content-aware image resizing (Avidan & Shamir, 2007). Instead of scaling or cropping, the
program repeatedly removes the **seam** (a connected path of pixels, one per row or column)
that has the lowest total energy, so important content such as edges and objects is kept.

The seam is found in three ways so they can be compared:

| Method | File | Status |
|---|---|---|
| Dynamic programming | `src/seam_dp.cpp` | Done |
| Greedy (row-by-row choice) | `src/seam_greedy.cpp` | In progress |
| Graph cut (min-cut / max-flow) | `src/seam_graphcut.cpp` | In progress |

## How it works

1. **Energy map** (`src/energy.cpp`): the Sobel gradient magnitude of the grayscale image.
   Flat areas get low energy and edges get high energy.
2. **Find the cheapest seam** (`src/seam_dp.cpp`): dynamic programming over a cost table
   `M[i][j] = e[i][j] + min(M[i-1][j-1], M[i-1][j], M[i-1][j+1])`, then a trace back from
   the smallest value in the bottom row.
3. **Remove and repeat** (`src/carver.cpp`): remove the seam, recompute the energy and repeat
   until the target width is reached. Horizontal seams are found as vertical seams of the
   transposed image.

## Building

Requirements: a C++17 compiler (Visual Studio 2022/2026, GCC or Clang), CMake 3.20 or newer,
and an internet connection the first time (CMake downloads the stb image headers).

**Visual Studio:** File → Open → Folder, choose this repository, then build `seamcarve.exe`.

**Command line:**

```
cmake -S . -B build
cmake --build build --config Release
```

## Running

```
seamcarve <input> <output> [--width W] [--height H] [--method dp|greedy|graphcut]
```

Example: shrink a photo to 400x300 using dynamic programming:

```
build/Release/seamcarve images/tower.jpg results/tower_dp.png --width 400 --height 300 --method dp
```

Input can be PNG, JPG, BMP, PPM and other common formats. Output can be PNG, JPG or BMP.
The program prints the new size and how long the carving took.

## Testing

```
ctest --test-dir build -C Release --output-on-failure
```

The tests (`tests/test_main.cpp`) include a brute-force check: on 500 random small grids the DP
seam must cost exactly as little as the best seam found by trying every possible seam.
GitHub Actions runs the tests on Windows and Linux for every push.

## Project layout

```
include/seam/   headers (Image, Matrix, energy, seam finders, carver)
src/            implementation and the seamcarve command-line tool
tests/          unit tests
.github/        CI workflow
```

## References

- S. Avidan and A. Shamir, "Seam Carving for Content-Aware Image Resizing", SIGGRAPH 2007.
- M. Rubinstein, A. Shamir and S. Avidan, "Improved Seam Carving for Video Retargeting", SIGGRAPH 2008.
- [Seam carving (Wikipedia)](https://en.wikipedia.org/wiki/Seam_carving)
- [Real-world dynamic programming: seam carving (Avik Das)](https://avikdas.com/2019/05/14/real-world-dynamic-programming-seam-carving.html)
- [stb single-file libraries](https://github.com/nothings/stb), used for reading and writing image files
