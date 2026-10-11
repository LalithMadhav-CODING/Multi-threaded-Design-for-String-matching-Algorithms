#ifndef SIMD_SEARCH_H
#define SIMD_SEARCH_H

#include <vector>
#include <string>

namespace simd {

// Performs string matching using AVX2 SIMD instructions
std::vector<size_t> searchChunkSIMD(
    const std::string* text, 
    const std::string* pattern, 
    size_t start_idx, 
    size_t end_idx);

} // namespace simd

#endif // SIMD_SEARCH_H
