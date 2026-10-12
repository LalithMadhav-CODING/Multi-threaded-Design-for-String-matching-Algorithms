#include "parallel_bm.h"
#include "boyer_moore.h"
#include <algorithm>
#include <chrono>

namespace bm {

namespace {
using Clock = std::chrono::steady_clock;

double secondsBetween(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double>(b - a).count();
}
} // namespace

std::vector<size_t> searchParallel(
    std::string_view text,
    std::string_view pattern,
    int num_threads,
    ThreadPool& pool,
    PhaseTimes* times,
    int tasks_per_thread)
{
    if (num_threads <= 0) num_threads = 1;
    if (tasks_per_thread <= 0) tasks_per_thread = 1;
    int num_tasks = num_threads * tasks_per_thread;

    auto t0 = Clock::now();

    std::vector<int> badChar = buildBadCharTable(pattern);
    std::vector<int> goodSuffix = buildGoodSuffixTable(pattern);

    auto t1 = Clock::now();

    std::vector<std::future<std::vector<size_t>>> futures;
    futures.reserve(num_tasks);
    size_t text_len = text.length();
    size_t m = pattern.length();
    size_t chunk_size = text_len / num_tasks;

    for (int i = 0; i < num_tasks; ++i) {
        size_t start_idx = i * chunk_size;
        size_t end_idx = (i == num_tasks - 1) ? text_len : (i + 1) * chunk_size + m - 1;
        if (end_idx > text_len) end_idx = text_len;

        futures.push_back(pool.enqueue([&, start_idx, end_idx]() {
            return searchChunk(text, pattern, badChar, goodSuffix, start_idx, end_idx);
        }));
    }

    auto t2 = Clock::now();

    std::vector<std::vector<size_t>> partial;
    partial.reserve(num_tasks);
    for (auto& f : futures) {
        partial.push_back(f.get());
    }

    auto t3 = Clock::now();

    std::vector<size_t> all_occurrences;
    for (auto& local_results : partial) {
        all_occurrences.insert(all_occurrences.end(), local_results.begin(), local_results.end());
    }

    auto t4 = Clock::now();

    std::sort(all_occurrences.begin(), all_occurrences.end());
    auto last = std::unique(all_occurrences.begin(), all_occurrences.end());
    all_occurrences.erase(last, all_occurrences.end());

    auto t5 = Clock::now();

    if (times) {
        times->preprocess = secondsBetween(t0, t1);
        times->dispatch = secondsBetween(t1, t2);
        times->search = secondsBetween(t2, t3);
        times->merge = secondsBetween(t3, t5);
        times->sort = secondsBetween(t4, t5);
    }

    return all_occurrences;
}

} // namespace bm
