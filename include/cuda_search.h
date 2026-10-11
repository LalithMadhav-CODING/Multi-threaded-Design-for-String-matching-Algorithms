#ifndef CUDA_SEARCH_H
#define CUDA_SEARCH_H

#include <string>

namespace cuda_search {
    // Runs the CUDA exact string matching and returns elapsed seconds
    double runCUDA(const std::string& text, const std::string& pattern, size_t& out_count);
}

#endif // CUDA_SEARCH_H
