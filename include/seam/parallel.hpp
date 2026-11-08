#pragma once

namespace seam {

// Multithreading support (assignment item 5d, "does multithreading help?").
// The project uses OpenMP when the compiler supports it. Without OpenMP everything
// still works, but always on a single thread.

// True if this build was compiled with OpenMP.
bool hasOpenMP();

// Number of threads OpenMP can use on this computer (1 without OpenMP).
int availableThreads();

} // namespace seam
