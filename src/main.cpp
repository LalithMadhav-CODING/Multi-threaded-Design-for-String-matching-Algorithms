#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include "boyer_moore.h"
#include "thread_pool.h"
#include "utils.h"

// Worker function: Searches a specific chunk of the text. 
// Uses start_idx and end_idx to define the chunk.
std::vector<size_t> searchChunk(
    const std::string* text, 
    const std::string* pattern, 
    const std::vector<int>* badChar, 
    const std::vector<int>* goodSuffix,
    size_t start_idx, 
    size_t end_idx) 
{
    std::vector<size_t> local_occurrences;
    int m = pattern->length();
    size_t n = end_idx;

    if (m == 0 || n == 0 || m > n || start_idx >= end_idx) {
        return local_occurrences;
    }

    size_t s = start_idx; 
    while (s <= n - m) {
        int j = m - 1;

        while (j >= 0 && (*pattern)[j] == (*text)[s + j]) {
            j--;
        }

        if (j < 0) {
            // Pattern found!
            local_occurrences.push_back(s);
            s += (*goodSuffix)[0];
        } else {
            int badCharShift = j - (*badChar)[static_cast<unsigned char>((*text)[s + j])];
            int goodSuffixShift = (*goodSuffix)[j];
            s += std::max(badCharShift, goodSuffixShift);
        }
    }

    return local_occurrences;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <filepath> <pattern> [num_threads]" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    std::string pattern = argv[2];
    int num_threads = std::thread::hardware_concurrency();
    if (argc >= 4) {
        num_threads = std::stoi(argv[3]);
    }

    std::cout << "Loading dataset..." << std::endl;
    std::string text = utils::readFile(filepath);
    if (text.empty()) {
        std::cerr << "Text is empty or file not found." << std::endl;
        return 1;
    }
    std::cout << "Text loaded. Length: " << text.length() << " characters." << std::endl;

    std::cout << "Preprocessing pattern (Bad Character & Good Suffix Tables)..." << std::endl;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Shared Preprocessing Phase
    std::vector<int> badChar = bm::buildBadCharTable(pattern);
    std::vector<int> goodSuffix = bm::buildGoodSuffixTable(pattern);

    std::cout << "Starting search with " << num_threads << " threads..." << std::endl;
    ThreadPool pool(num_threads);
    std::vector<std::future<std::vector<size_t>>> futures;

    size_t text_len = text.length();
    size_t m = pattern.length();
    size_t chunk_size = text_len / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t start_idx = i * chunk_size;
        
        // Overlap Handling: The end_idx must extend by M - 1 into the next chunk 
        // to catch overlapping patterns that straddle the chunk boundaries.
        size_t end_idx = (i == num_threads - 1) ? text_len : (i + 1) * chunk_size + m - 1;
        if (end_idx > text_len) end_idx = text_len;

        futures.push_back(pool.enqueue(searchChunk, &text, &pattern, &badChar, &goodSuffix, start_idx, end_idx));
    }

    // Result Aggregation Phase
    std::vector<size_t> all_occurrences;
    for (auto& f : futures) {
        auto local_results = f.get();
        all_occurrences.insert(all_occurrences.end(), local_results.begin(), local_results.end());
    }

    // Because chunks overlap, a match exactly at the boundary could be found by two threads.
    // We remove duplicates during the merge.
    std::sort(all_occurrences.begin(), all_occurrences.end());
    auto last = std::unique(all_occurrences.begin(), all_occurrences.end());
    all_occurrences.erase(last, all_occurrences.end());

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end_time - start_time;

    std::cout << "======================================" << std::endl;
    std::cout << "Search complete." << std::endl;
    std::cout << "Found " << all_occurrences.size() << " occurrences." << std::endl;
    std::cout << "Time elapsed: " << diff.count() << " seconds." << std::endl;

    return 0;
}
