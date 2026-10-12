#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace bm {

// Using 256 for standard ASCII characters.
// For pure DNA (A,C,G,T) this is more than sufficient.
constexpr int ALPHABET_SIZE = 256;

// Phase 1: Preprocessing Functions
// These are stateless and will be reused by all threads in Phase 2

// Computes the Bad Character heuristic table
std::vector<int> buildBadCharTable(std::string_view pattern);

// Computes the Good Suffix heuristic table
std::vector<int> buildGoodSuffixTable(std::string_view pattern);

// Searches text[start_idx, end_idx) using precomputed tables.
// Returns the starting index of every match that lies fully inside the range.
std::vector<size_t> searchChunk(
    std::string_view text,
    std::string_view pattern,
    const std::vector<int>& badChar,
    const std::vector<int>& goodSuffix,
    size_t start_idx,
    size_t end_idx);

// Phase 1: Sequential Search
// Returns a vector of all starting indices where the pattern occurs in the text.
std::vector<size_t> searchSequential(std::string_view text, std::string_view pattern);

} // namespace bm
