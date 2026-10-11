#include "fuzzy_search.h"

namespace fuzzy {

std::vector<size_t> searchChunkFuzzy(
    const std::string* text, 
    const std::string* pattern, 
    int max_errors,
    size_t start_idx, 
    size_t end_idx) 
{
    std::vector<size_t> local_occurrences;
    int m = pattern->length();
    size_t n = end_idx;

    // Bitap limits pattern length to word size (64 bits)
    if (m == 0 || n == 0 || m > 64 || start_idx >= end_idx) {
        return local_occurrences;
    }

    // Precompute pattern mask
    // pattern_mask[c] has 0 at position i if pattern[i] == c, else 1
    uint64_t pattern_mask[256];
    for (int i = 0; i < 256; ++i) {
        pattern_mask[i] = ~0ULL;
    }
    for (int i = 0; i < m; ++i) {
        pattern_mask[static_cast<unsigned char>((*pattern)[i])] &= ~(1ULL << i);
    }

    // State arrays for errors 0 to max_errors
    std::vector<uint64_t> R(max_errors + 1, ~0ULL);
    
    for (size_t i = start_idx; i < end_idx; ++i) {
        uint64_t old_Rd1 = R[0];
        
        // Exact match state
        R[0] |= pattern_mask[static_cast<unsigned char>((*text)[i])];
        R[0] <<= 1;

        // States with up to max_errors
        for (int d = 1; d <= max_errors; ++d) {
            uint64_t tmp = R[d];
            
            // Levenshtein transitions:
            // 1. Match/Mismatch: (R[d] | mask) << 1
            // 2. Substitution: old_Rd1 << 1
            // 3. Insertion: old_Rd1
            // 4. Deletion: R[d-1] << 1
            
            R[d] = ((R[d] | pattern_mask[static_cast<unsigned char>((*text)[i])]) << 1) 
                 & (old_Rd1 << 1) 
                 & (old_Rd1) 
                 & (R[d-1] << 1);
                 
            old_Rd1 = tmp;
        }

        // Check if pattern matched within max_errors
        // The match bit is the m-th bit.
        if (0 == (R[max_errors] & (1ULL << m))) {
            // Found a match ending at i. Approximate start is i - m + 1
            size_t start_pos = i >= (size_t)(m - 1) ? i - m + 1 : 0;
            // Only report matches that start within our primary chunk domain to avoid cross-chunk duplicates
            if (start_pos >= start_idx) {
                local_occurrences.push_back(start_pos);
            }
        }
    }

    return local_occurrences;
}

} // namespace fuzzy
