#include "seam/parallel.hpp"

// _OPENMP is defined by the compiler when OpenMP is switched on (/openmp or -fopenmp).
#ifdef _OPENMP
#include <omp.h>
#endif

namespace seam {

bool hasOpenMP() {
#ifdef _OPENMP
    return true;
#else
    return false;
#endif
}

int availableThreads() {
#ifdef _OPENMP
    return omp_get_max_threads();
#else
    return 1;
#endif
}

} // namespace seam
