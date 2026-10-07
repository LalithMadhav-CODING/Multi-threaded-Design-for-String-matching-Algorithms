#include "boyer_moore.h"
#include <algorithm>

namespace bm {

std::vector<int> buildBadCharTable(const std::string& pattern) {
    std::vector<int> badChar(ALPHABET_SIZE, -1);
    // Initialize all occurrences as -1
    // Then fill the actual value of last occurrence of a character
    for (int i = 0; i < static_cast<int>(pattern.length()); ++i) {
        badChar[static_cast<unsigned char>(pattern[i])] = i;
    }
    return badChar;
}

std::vector<int> buildGoodSuffixTable(const std::string& pattern) {
    int m = pattern.length();
    std::vector<int> goodSuffix(m, m);
    std::vector<int> suffixes(m, 0);

    // Compute suffix lengths (similar to Z-algorithm but right-to-left)
    int f = 0, g = m - 1;
    suffixes[m - 1] = m;
    for (int i = m - 2; i >= 0; --i) {
        if (i > g && suffixes[i + m - 1 - f] < i - g) {
            suffixes[i] = suffixes[i + m - 1 - f];
        } else {
            if (i < g) g = i;
            f = i;
            while (g >= 0 && pattern[g] == pattern[g + m - 1 - f]) {
                --g;
            }
            suffixes[i] = f - g;
        }
    }

    // Populate Good Suffix table using the suffixes array
    int j = 0;
    for (int i = m - 1; i >= 0; --i) {
        if (suffixes[i] == i + 1) {
            for (; j < m - 1 - i; ++j) {
                if (goodSuffix[j] == m) {
                    goodSuffix[j] = m - 1 - i;
                }
            }
        }
    }
    for (int i = 0; i <= m - 2; ++i) {
        goodSuffix[m - 1 - suffixes[i]] = m - 1 - i;
    }

    return goodSuffix;
}

std::vector<size_t> searchSequential(const std::string& text, const std::string& pattern) {
    std::vector<size_t> occurrences;
    int n = text.length();
    int m = pattern.length();

    if (m == 0 || n == 0 || m > n) {
        return occurrences;
    }

    // Phase 1 preprocessing
    std::vector<int> badChar = buildBadCharTable(pattern);
    std::vector<int> goodSuffix = buildGoodSuffixTable(pattern);

    int s = 0; // s is shift of the pattern with respect to text
    while (s <= n - m) {
        int j = m - 1;

        // Keep reducing index j of pattern while characters match
        while (j >= 0 && pattern[j] == text[s + j]) {
            j--;
        }

        if (j < 0) {
            // Pattern found
            occurrences.push_back(s);
            // Shift the pattern so that the next character aligns with the last occurrence
            s += goodSuffix[0];
        } else {
            // Pattern doesn't match: calculate max shift between Bad Character and Good Suffix heuristics
            int badCharShift = j - badChar[static_cast<unsigned char>(text[s + j])];
            int goodSuffixShift = goodSuffix[j];
            s += std::max(badCharShift, goodSuffixShift);
        }
    }

    return occurrences;
}

} // namespace bm
