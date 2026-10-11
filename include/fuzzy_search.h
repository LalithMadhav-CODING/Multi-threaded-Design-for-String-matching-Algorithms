#ifndef FUZZY_SEARCH_H
#define FUZZY_SEARCH_H

#include <vector>
#include <string>
#include <cstdint>

namespace fuzzy {

// Performs a Bitap (Shift-Or) search with up to max_errors (Levenshtein distance)
std::vector<size_t> searchChunkFuzzy(
    const std::string* text, 
    const std::string* pattern, 
    int max_errors,
    size_t start_idx, 
    size_t end_idx);

} // namespace fuzzy

#endif // FUZZY_SEARCH_H
