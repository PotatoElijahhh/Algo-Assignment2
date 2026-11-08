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

## Requirements

- **Windows:** Visual Studio 2026 (or 2022) with the **Desktop development with C++** workload.
  This includes the compiler and CMake, so nothing else is needed.
- **Linux / macOS:** a C++17 compiler (GCC or Clang) and CMake 3.20 or newer.
- An internet connection for the **first** build only: CMake downloads the stb image headers.

## Building

### Option A: Visual Studio (Windows)

1. **File → Open → Folder** and choose this repository folder.
2. Wait for the Output window to show `CMake generation finished`. The first time, this
   includes downloading stb.
3. In the configuration drop-down on the toolbar, choose **x64-Release**. (If only x64-Debug
   exists, choose *Manage Configurations*, click **+** and add `x64-Release`.) Use Release when
   measuring running times, because Debug builds are much slower.
4. **Build → Build All** (`Ctrl+Shift+B`).

The programs are written to `out/build/x64-Release/` (`seamcarve.exe` and `seam_tests.exe`).

### Option B: Command line

On Windows, open **Developer PowerShell for VS** (Start menu, or **View → Terminal** inside
Visual Studio) so that `cmake` is available. Then, from the repository folder:

```
cmake -S . -B build
cmake --build build --config Release
```

The programs are written to:

| Platform | Location |
|---|---|
| Windows (Visual Studio generator) | `build\Release\seamcarve.exe`, `build\Release\seam_tests.exe` |
| Linux / macOS | `build/seamcarve`, `build/seam_tests` |

## Running the program

```
seamcarve <input> <output> [--width W] [--height H] [--method dp|greedy|graphcut]
```

| Option | Meaning | Default |
|---|---|---|
| `<input>` | Image to resize (PNG, JPG, BMP, PPM, ...) | required |
| `<output>` | Where to save the result (`.png`, `.jpg` or `.bmp`) | required |
| `--width W` | Target width in pixels (must not be larger than the input) | keep current width |
| `--height H` | Target height in pixels (must not be larger than the input) | keep current height |
| `--method M` | Seam-finding method: `dp`, `greedy` or `graphcut` | `dp` |

The program only **shrinks** images, and the output folder must already exist.

### Example (Windows, Developer PowerShell)

Put a test photo in an `images` folder, then:

```
mkdir results
.\build\Release\seamcarve.exe images\tower.jpg results\tower_dp.png --width 400 --height 300 --method dp
```

### Example (Linux / macOS)

```
mkdir -p results
./build/seamcarve images/tower.jpg results/tower_dp.png --width 400 --height 300 --method dp
```

### What you should see

The program prints one line, for example:

```
Carved 640x427 -> 400x300 using dp in 1873 ms
```

The resized image is saved to `results/tower_dp.png`. The sizes and time depend on your photo and
computer. To compare methods, run the same command with `--method greedy` or
`--method graphcut` and compare the printed times and the output images. Graph cut is slow, so
try it on small images (about 100x100) first.

If something is wrong (missing file, target size larger than the image, unknown option), the
program prints `Error: ...` with the reason, shows the usage text and exits with code 1.

## Testing

### Run all tests

**Visual Studio:** **Test → Test Explorer**, then click **Run All Tests**. The `seam_tests` test
should turn green.

**Command line** (after building):

```
ctest --test-dir build -C Release --output-on-failure
```

Expected result:

```
100% tests passed, 0 tests failed out of 1
```

### See each individual test

Run the test program directly to see every test by name:

```
.\build\Release\seam_tests.exe        (Windows)
./build/seam_tests                    (Linux / macOS)
```

```
[PASS] DP finds known seam
[PASS] DP matches brute force
...
All tests passed
```

If a check fails, the line is marked `[FAIL]` and the file and line number of the failing
`CHECK` are printed above it.

### What the tests check

| Test | What it proves |
|---|---|
| DP finds known seam | The DP returns the expected seam on a hand-checked 4x4 grid |
| DP matches brute force | On 500 random small grids, the DP seam costs exactly as little as the best seam found by trying **every** possible seam |
| DP single column and single row | Edge cases with one column or one row |
| isValidSeam | Seams that jump more than one column, go out of range or have the wrong length are rejected |
| energy is zero for flat image | A single-colour image has no edges, so all energy is 0 |
| energy is high on edges | Energy appears only next to a black/white boundary |
| transpose round trip | Transposing twice gives back the original image |
| remove vertical seam | Removing a seam keeps exactly the right pixels |
| carve reaches target size | The output has the requested width and height |
| carve keeps high-energy object | Seams go around a detailed object instead of through it |
| carve rejects invalid targets | Sizes of 0 or larger than the image throw an error |
| callback sees every seam | Every removed seam (vertical and horizontal) is reported, for visualisation |

### Adding a test

1. In `tests/test_main.cpp`, write a function `void testSomething()` that uses `CHECK(condition)`.
2. Add `{"something", testSomething},` to the `tests` list in `main()`.
3. Rebuild and run the tests.

### Continuous integration

GitHub Actions (`.github/workflows/ci.yml`) builds the project and runs the tests on **Windows
and Ubuntu** for every push and pull request. Results are shown in the repository's **Actions**
tab and on each pull request. Only merge a pull request into `main` when both checks are green.

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
