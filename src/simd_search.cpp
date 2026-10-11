#include "simd_search.h"
#include <immintrin.h>
#include <cstdint>

namespace simd {

std::vector<size_t> searchChunkSIMD(
    const std::string* text, 
    const std::string* pattern, 
    size_t start_idx, 
    size_t end_idx) 
{
    std::vector<size_t> local_occurrences;
    int m = pattern->length();
    size_t n = end_idx;

    if (m == 0 || n == 0 || start_idx >= end_idx) {
        return local_occurrences;
    }

    const char* t = text->data();
    const char* p = pattern->data();
    
    char first_char = p[0];
    char last_char = p[m - 1];

    // Broadcast the first and last characters into 256-bit vectors
    __m256i v_first = _mm256_set1_epi8(first_char);
    __m256i v_last = _mm256_set1_epi8(last_char);

    size_t i = start_idx;
    
    // Process 32 bytes at a time
    for (; i + 32 + m <= end_idx; i += 32) {
        // Load 32 bytes from text
        __m256i block_first = _mm256_loadu_si256((const __m256i*)(t + i));
        __m256i block_last  = _mm256_loadu_si256((const __m256i*)(t + i + m - 1));

        // Compare blocks against pattern characters
        __m256i eq_first = _mm256_cmpeq_epi8(block_first, v_first);
        __m256i eq_last  = _mm256_cmpeq_epi8(block_last, v_last);

        // Bitwise AND the results (both first and last char must match)
        __m256i match = _mm256_and_si256(eq_first, eq_last);

        // Extract match mask (each bit represents a byte)
        uint32_t mask = _mm256_movemask_epi8(match);

        // Iterate through all 1-bits in the mask
        while (mask != 0) {
            int bit_pos = __builtin_ctz(mask); // count trailing zeros
            
            // Verify the inner string (if pattern is longer than 2)
            bool is_match = true;
            for (int j = 1; j < m - 1; ++j) {
                if (t[i + bit_pos + j] != p[j]) {
                    is_match = false;
                    break;
                }
            }
            if (is_match) {
                local_occurrences.push_back(i + bit_pos);
            }
            
            // Clear the lowest set bit
            mask &= (mask - 1);
        }
    }

    // Scalar fallback for remaining characters
    for (; i <= end_idx - m; ++i) {
        bool is_match = true;
        for (int j = 0; j < m; ++j) {
            if (t[i + j] != p[j]) {
                is_match = false;
                break;
            }
        }
        if (is_match) {
            local_occurrences.push_back(i);
        }
    }

    return local_occurrences;
}

} // namespace simd
