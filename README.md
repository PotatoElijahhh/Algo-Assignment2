# Algo-Assignment2: Image Resizing by Seam Carving

Content-aware image resizing (Avidan & Shamir, 2007). Instead of scaling or cropping, the
program repeatedly removes the **seam** (a connected path of pixels, one per row or column)
that has the lowest total energy, so important content such as edges and objects is kept.

The seam is found in three ways so they can be compared:

| Method | File | Status |
|---|---|---|
| Dynamic programming | `src/seam_dp.cpp` | Done |
| Greedy (row-by-row choice) | `src/seam_greedy.cpp` | In progress |
| Graph cut (min-cut / max-flow) | `src/graphcut_flow.cpp` (graph + max-flow), `src/seam_graphcut.cpp` (seam extraction) | Seam extraction done, graph + max-flow in progress |

It also includes two **improvements** to the dynamic-programming version (assignment item 5d),
both of which give exactly the same result as the original:

| Improvement | File | What it changes |
|---|---|---|
| Memory-light DP (`dp-compact`) | `src/seam_dp_improved.cpp` | Keeps only 2 rows of costs plus a 1-byte parent table instead of the full cost table, which is about 8x less memory |
| Multithreading (`--threads`, `dp-parallel`) | `src/energy.cpp`, `src/seam_dp_improved.cpp` | Uses several CPU threads (OpenMP) for the energy map and for each row of the DP |

It also has **visualisation and interactive resizing** (bonus items 7a and 7b): step-by-step
frames, all removed seams drawn on the original image, a console mode that keeps asking for new
sizes, and an optional window with width/height sliders. See
[Visualisation and interactive resizing](#visualisation-and-interactive-resizing-bonus).

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
| Windows (Visual Studio generator) | `build\Release\seamcarve.exe`, `build\Release\seambench.exe`, `build\Release\seam_tests.exe` |
| Linux / macOS | `build/seamcarve`, `build/seambench`, `build/seam_tests` |

Multithreading uses OpenMP, which comes with Visual Studio and GCC. If the compiler has no
OpenMP (for example Apple Clang), everything still builds and simply runs on one thread.

## Running the program

```
seamcarve <input> <output> [--width W] [--height H] [--method M] [--threads N]
          [--frames DIR] [--frame-every N] [--overlay FILE] [--interactive]
```

| Option | Meaning | Default |
|---|---|---|
| `<input>` | Image to resize (PNG, JPG, BMP, PPM, ...) | required |
| `<output>` | Where to save the result (`.png`, `.jpg` or `.bmp`) | required |
| `--width W` | Target width in pixels (must not be larger than the input) | keep current width |
| `--height H` | Target height in pixels (must not be larger than the input) | keep current height |
| `--method M` | Seam-finding method (see below) | `dp` |
| `--threads N` | Number of CPU threads for the energy map, and for the DP with `dp-parallel` | `1` |
| `--frames DIR` | Save a PNG of every step into folder `DIR` (see below) | off |
| `--frame-every N` | With `--frames`, only save every N-th seam | `1` |
| `--overlay FILE` | Save the original image with every removed seam painted | off |
| `--interactive` | Keep asking for new sizes instead of using `--width`/`--height` | off |

| Method | Description |
|---|---|
| `dp` | Dynamic programming with the full cost table (original version) |
| `dp-compact` | Memory-light dynamic programming, same result as `dp` |
| `dp-parallel` | Memory-light dynamic programming with each row split across `--threads` threads, same result as `dp` |
| `greedy` | Greedy row-by-row choice |
| `graphcut` | Minimum cut / max-flow |

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
Carved 640x427 -> 400x300 using dp (1 thread) in 1873 ms
```

The resized image is saved to `results/tower_dp.png`. The sizes and time depend on your photo and
computer. To compare methods, run the same command with `--method greedy` or
`--method graphcut` and compare the printed times and the output images. Graph cut is slow, so
try it on small images (about 100x100) first.

If something is wrong (missing file, target size larger than the image, unknown option), the
program prints `Error: ...` with the reason, shows the usage text and exits with code 1.

## Visualisation and interactive resizing (bonus)

All of this lives in `src/visualise.cpp`, `src/interactive.cpp`, `src/seam_order.cpp` and
`src/gui_main.cpp`. It uses the callback that `carve()` calls just before removing each seam, so
it works with every seam-finding method and never changes the result.

### Step-by-step frames (`--frames`)

```
seamcarve images/balloon.jpg results/balloon_small.png --width 380 --height 280 --frames frames --frame-every 10
```

Saves `frames/frame_0001_v.png`, `frame_0011_v.png`, ..., `frame_0345_final.png`. Each frame is the
image just before a seam is removed, with that seam painted **red** (vertical) or **blue**
(horizontal). Frames are padded with dark grey to the original size, so they line up when played
as an animation. The last frame is the finished result. Leave out `--frame-every` to save every
seam. (`frames/` is ignored by Git.)

### All seams on the original image (`--overlay`)

```
seamcarve images/balloon.jpg results/balloon_small.png --width 380 --height 280 --overlay results/balloon_overlay.png
```

Saves the **original** image with every removed pixel painted: red = removed by a vertical seam,
blue = removed by a horizontal seam. After a few seams the pixels have shifted, so `SeamTracker`
keeps the original position of every remaining pixel and removes the same entries each time a
seam is removed (O(W·H) per seam, the same as removing the seam itself). `--frames` and
`--overlay` can be used together.

### Interactive console mode (`--interactive`)

```
seamcarve images/balloon.jpg results/balloon.png --interactive
```

```
Interactive mode. Original size: 640x364
> 500 300
Carved to 500x300 in 548 ms, saved results/balloon_500x300.png
> 380
Carved to 380x364 in 572 ms, saved results/balloon_380x364.png
> q
Saved 2 images.
```

Type `W H`, just `W` (keeps the height) or `q`. Every size is carved from the **original**, so sizes
can be tried in any order, and each one is saved as `<output>_WxH`. A size that is too big or
input that is not a number prints a message and asks again.

### Slider window (`seamgui`, optional)

A window with **width and height sliders that update instantly**, a "show removed seams" switch,
a method choice and a Save button. It needs Dear ImGui and GLFW, so it is **off by default**;
normal builds and CI do not download or build it.

**Turn it on in Visual Studio:** **Project → CMake Settings**, select **x64-Release**, tick
`SEAM_BUILD_GUI` under *CMake variables and cache* (or set *CMake command arguments* to
`-DSEAM_BUILD_GUI=ON`), save, then build. **Command line:**

```
cmake -S . -B build -DSEAM_BUILD_GUI=ON
cmake --build build --config Release
```

Run it with `seamgui <input> <output>`; **Save** writes `<output>_WxH`. On Linux the X11 and
OpenGL development packages are needed (e.g. `libx11-dev libxrandr-dev libxinerama-dev
libxcursor-dev libxi-dev libgl-dev`).

**Why the sliders are instant:** carving to width w always removes the *first* W − w seams of
the same sequence, because each step removes the current cheapest seam whatever the target is.
So when the image is opened, the program carves once all the way down in a background thread
and records, for every pixel, the step at which it was removed (`computeVerticalSeamOrder`,
the "multi-size image" idea from Avidan & Shamir). Any width is then "keep every pixel removed
after step W − w", which is O(W·H) with no energy map or DP. When the width slider is released,
the same is done for horizontal seams at that width, and the height slider becomes instant too.
The tests check that this gives exactly the same pixels as `carve()` for every size.

## Measuring the improvements (`seambench`)

`seambench` measures the two DP improvements and checks that they give the same results as the
original version:

```
seambench <image>    [--repeat R] [--seams K] [--max-threads T] [--csv FILE]
seambench --size WxH [--repeat R] [--seams K] [--max-threads T] [--csv FILE]
```

| Option | Meaning | Default |
|---|---|---|
| `<image>` or `--size WxH` | A photo, or a random test image of the given size | required |
| `--repeat R` | Runs to average in sections 1 and 2 | `5` |
| `--seams K` | Vertical seams to remove in section 3 | `50` |
| `--max-threads T` | Most threads to try | all available |
| `--csv FILE` | Also save every result to a CSV file (for charts in Excel) | off |

It prints three tables:

1. **Energy map:** time with 1, 2, 4, ... threads, and the speed-up over 1 thread.
2. **One seam:** the original DP, the memory-light DP and the multithreaded DP, with time,
   speed-up and the memory used by the DP tables.
3. **Full carve:** time to remove K seams with each configuration.

The last column, `same result`, must say `yes` on every line.

Example (Windows):

```
.\build\Release\seambench.exe images\tower.jpg --seams 50 --csv results\bench_tower.csv
```

Use a **Release** build for meaningful times, and close other heavy programs while it runs.

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
| compact DP matches original DP | The memory-light DP returns exactly the same seam as the original on 300 random grids, including ties |
| parallel DP matches original DP | The multithreaded DP (4 threads) returns exactly the same seam as the original |
| parallel energy matches serial | The multithreaded energy map is exactly equal to the single-threaded one |
| all DP versions carve the same image | Carving with `dp`, `dp-compact` and `dp-parallel` gives identical images |
| paint vertical / horizontal seam | Exactly the seam's pixels change colour, nothing else |
| paint seam rejects bad seams | Seams of the wrong length, with jumps or out of range are rejected |
| paint every seam from carve | Every seam `carve()` reports can be painted, one pixel per row (or column) |
| pad to size | Frames are placed top-left on a background of the original size |
| frame recorder saves every N-th seam | With `--frame-every 2`, exactly frames 1, 3, 5 and the final one are saved, all at the original size |
| frame recorder rejects bad 'every' | `--frame-every 0` is rejected |
| first frame shows the first seam | The red pixels of the first saved PNG are exactly the DP seam of the original image |
| tracker follows every pixel | After carving, every pixel of the result equals the original pixel `SeamTracker` says it came from (4 sizes) |
| tracker counts removed pixels | Removed pixels = W·H − w·h, and no pixel is removed twice |
| overlay colours removed pixels | The overlay has exactly (removed columns × H) red, (removed rows × w) blue and w·h unchanged pixels |
| tracker rejects wrong size | Recording a seam for an image of the wrong size throws |
| output name for size | `results/balloon.png` at 300x200 is saved as `results/balloon_300x200.png` |
| interactive carves each request | A scripted session saves the right sizes, with the same pixels as `carve()` |
| interactive keeps going after bad input | Junk input and out-of-range sizes print a message and the loop continues |
| vertical order matches carve | Resizing with the precomputed seam order equals `carve()` for **every** width |
| order has one pixel per row per step | Each vertical seam removes exactly one pixel in every row |
| width then height order matches carve | The same for width + height combinations (what the slider window does) |
| paint removed pixels matches tracker | The window's live seam picture equals the `--overlay` picture |
| seam order progress and range | Progress is reported once per seam, and widths outside the computed range are rejected |
| graph cut: source side follows residual edges | The BFS from the source uses only edges with residual capacity left, including reverse edges |
| graph cut: extract reads rightmost source pixel | In each row the seam pixel is the rightmost pixel on the source side of the cut |
| graph cut: extract edge columns | Seams on the first column and on the last allowed column (`cols - 2`) |
| graph cut: tiny residual counts as saturated | Rounding leftovers (e.g. `1e-12`) do not count as capacity |
| graph cut: rejects ... (5 tests) | A residual graph that is not a valid seam cut (sink still reachable, a row cut twice or not at all, seam not connected, wrong size) throws an error instead of returning a wrong seam |
| graph cut: single column | A one-column image needs no cut |
| graph cut: matches brute force | On 500 random grids the graph-cut seam costs exactly as little as the best seam found by brute force over columns `0 .. cols-2` |
| graph cut: max flow equals seam cost | Max-flow min-cut theorem: the flow value equals the cost of the extracted seam |
| graph cut: finds known seam | Same hand-checked 4x4 grid as the DP test |
| graph cut: matches DP when last column blocked | With an expensive last column, graph cut and DP find seams of equal cost |
| graph cut: Sobel energy | Same as above on real (non-integer) Sobel energies |
| graph cut: special shapes | Two columns, one row, all-zero and all-equal energies |
| graph cut: carve reaches target size / keeps high-energy object | Carving with `--method graphcut` works end to end |

The end-to-end graph cut tests need `buildSeamGraph` and `maxFlow` from `src/graphcut_flow.cpp`.
Until those are implemented they print `[SKIP]` instead of failing, and run automatically once
they are.

### Graph cut: how the two parts fit together

`include/seam/graphcut.hpp` defines the `FlowGraph` shared by both parts (pixel `(i, j)` is node
`i * cols + j`, then the source and the sink; edges are stored in pairs so `edges[e ^ 1]` is the
reverse of `edges[e]`, and `cap` holds the residual capacity).

1. `buildSeamGraph(energy)` builds the graph from the assignment (item 4).
2. `maxFlow(g)` runs max-flow and leaves the residual capacities in `g`.
3. `extractSeam(g)` runs a BFS from the source in the residual graph to find the source side
   `S` of the minimum cut, then takes the rightmost pixel of `S` in each row. It checks that the
   result is a valid seam and throws `std::logic_error` if the graph or flow is wrong.

The extraction is `O(V + E) = O(rows * cols)`, small next to max-flow. Because the last column is
tied to the sink with infinite capacity, the graph-cut seam never uses the last column.

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
include/seam/   headers (Image, Matrix, energy, seam finders, carver, parallel, visualise,
                interactive, seam_order)
src/            implementation, the seamcarve tool (main.cpp), seambench (bench.cpp) and the
                optional slider window (gui_main.cpp)
tests/          unit tests
.github/        CI workflow
```

## References

- S. Avidan and A. Shamir, "Seam Carving for Content-Aware Image Resizing", SIGGRAPH 2007.
- M. Rubinstein, A. Shamir and S. Avidan, "Improved Seam Carving for Video Retargeting", SIGGRAPH 2008.
- [Seam carving (Wikipedia)](https://en.wikipedia.org/wiki/Seam_carving)
- [Real-world dynamic programming: seam carving (Avik Das)](https://avikdas.com/2019/05/14/real-world-dynamic-programming-seam-carving.html)
- [stb single-file libraries](https://github.com/nothings/stb), used for reading and writing image files
- [Dear ImGui](https://github.com/ocornut/imgui) and [GLFW](https://www.glfw.org/), used only by the optional slider window
