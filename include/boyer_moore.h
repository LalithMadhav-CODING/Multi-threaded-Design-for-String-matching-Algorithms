#pragma once

#include <string>
#include <vector>

namespace bm {

// Using 256 for standard ASCII characters. 
// For pure DNA (A,C,G,T) this is more than sufficient.
constexpr int ALPHABET_SIZE = 256;

// Phase 1: Preprocessing Functions
// These are stateless and will be reused by all threads in Phase 2

// Computes the Bad Character heuristic table
std::vector<int> buildBadCharTable(const std::string& pattern);

// Computes the Good Suffix heuristic table
std::vector<int> buildGoodSuffixTable(const std::string& pattern);

// Phase 1: Sequential Search
// Returns a vector of all starting indices where the pattern occurs in the text.
std::vector<size_t> searchSequential(const std::string& text, const std::string& pattern);

} // namespace bm
