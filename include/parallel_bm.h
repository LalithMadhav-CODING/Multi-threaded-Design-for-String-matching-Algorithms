#pragma once

#include <string_view>
#include <vector>
#include "thread_pool.h"

namespace bm {

// Wall-clock seconds spent in each phase of one parallel search
struct PhaseTimes {
    double preprocess = 0.0; // building bad-character / good-suffix tables
    double dispatch = 0.0;   // splitting the text and enqueueing one task per thread
    double search = 0.0;     // waiting until every task has finished
    double merge = 0.0;      // concatenating results, sorting and removing duplicates
    double sort = 0.0;       // the sort + dedupe part of merge
};

// Data-parallel Boyer-Moore: splits the text into num_threads * tasks_per_thread
// chunks that overlap by (pattern length - 1) and searches them on the pool.
// More tasks than threads lets faster cores pick up extra chunks.
// Returns all match positions in ascending order.
std::vector<size_t> searchParallel(
    std::string_view text,
    std::string_view pattern,
    int num_threads,
    ThreadPool& pool,
    PhaseTimes* times = nullptr,
    int tasks_per_thread = 1);

} // namespace bm
