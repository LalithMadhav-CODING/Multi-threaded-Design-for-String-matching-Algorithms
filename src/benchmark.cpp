// Benchmark driver for the parallelism analysis of Boyer-Moore.
//
// For every (pattern, input size, thread count) it measures the sequential
// baseline and the thread-pool version, repeats each measurement, and writes
// medians plus a per-phase breakdown to a CSV file.
//
// Usage: bm_bench <dataset.fna> <out.csv> [--quick] [--name NAME]

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include "boyer_moore.h"
#include "parallel_bm.h"
#include "thread_pool.h"
#include "utils.h"

namespace {

using Clock = std::chrono::steady_clock;

double secondsSince(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// Keeps only the nucleotide sequence of a FASTA file, in place:
// drops header lines, line breaks and unknown bases (N), and upper-cases
// soft-masked (lowercase) regions so every base is searchable.
void normalizeFasta(std::string& data) {
    size_t out = 0;
    bool in_header = false;
    bool line_start = true;
    for (size_t i = 0; i < data.size(); ++i) {
        char c = data[i];
        if (line_start && c == '>') in_header = true;
        if (c == '\n') {
            in_header = false;
            line_start = true;
            continue;
        }
        line_start = false;
        if (in_header || c == '\r') continue;
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (c == 'N') continue;
        data[out++] = c;
    }
    data.resize(out);
    data.shrink_to_fit();
}

// More repetitions for small inputs, where timings are short and noisy
int repetitionsFor(size_t size, bool quick) {
    if (quick) return 5;
    if (size <= 1000000ULL) return 50;
    if (size <= 100000000ULL) return 10;
    if (size <= 1000000000ULL) return 5;
    return 3;
}

struct Summary {
    double med = 0, min = 0, max = 0;
};

Summary summarize(const std::vector<double>& v) {
    Summary s;
    s.med = median(v);
    s.min = *std::min_element(v.begin(), v.end());
    s.max = *std::max_element(v.begin(), v.end());
    return s;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <dataset.fna> <out.csv> [--quick] [--name NAME]" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    std::string out_path = argv[2];
    bool quick = false;
    std::string name = std::filesystem::path(filepath).stem().string();
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--quick") quick = true;
        else if (arg == "--name" && i + 1 < argc) name = argv[++i];
    }

    std::cerr << "Loading " << filepath << "..." << std::endl;
    std::string data = utils::readFile(filepath);
    if (data.empty()) {
        std::cerr << "Text is empty or file not found." << std::endl;
        return 1;
    }
    size_t raw_size = data.size();
    normalizeFasta(data);
    std::cerr << "Raw " << raw_size << " bytes -> " << data.size()
              << " bytes of sequence after normalization." << std::endl;

    // Input sizes: prefixes of the sequence, denser around the expected crossover
    std::vector<size_t> sizes;
    const std::vector<size_t> candidates = {
        1000ULL, 10000ULL, 100000ULL, 300000ULL, 1000000ULL, 3000000ULL,
        10000000ULL, 30000000ULL, 100000000ULL, 300000000ULL, 1000000000ULL,
        2000000000ULL, 4000000000ULL, 8000000000ULL};
    for (size_t s : candidates) {
        if (quick && s > 10000000ULL) break;
        if (s < data.size()) sizes.push_back(s);
    }
    if (!quick || data.size() <= 10000000ULL) sizes.push_back(data.size());

    std::vector<int> thread_counts = quick
        ? std::vector<int>{1, 2, 4, 8, 24}
        : std::vector<int>{1, 2, 4, 8, 12, 16, 20, 24, 32, 48};

    // Short pattern: many matches. Long pattern: few matches, larger skips.
    const std::vector<std::string> patterns = {
        "GATC",
        "ACGTTGCATGCAGTCAGTACGATCGATCGTAG"};

    // Thread-spawn overhead: create and destroy a pool of p threads
    std::vector<double> pool_cost(thread_counts.size());
    for (size_t t = 0; t < thread_counts.size(); ++t) {
        std::vector<double> samples;
        for (int r = 0; r < 20; ++r) {
            auto start = Clock::now();
            { ThreadPool pool(thread_counts[t]); }
            samples.push_back(secondsSince(start));
        }
        pool_cost[t] = median(samples);
    }

    bool exists = std::filesystem::exists(out_path) && std::filesystem::file_size(out_path) > 0;
    std::ofstream csv(out_path, std::ios::app);
    if (!csv) {
        std::cerr << "Cannot open " << out_path << std::endl;
        return 1;
    }
    if (!exists) {
        csv << "dataset,algo,pattern,size_bytes,threads,rep_count,median_s,min_s,max_s,"
               "t_pre,t_pool,t_dispatch,t_search,t_merge,t_sort,matches,correct\n";
    }
    csv.precision(9);

    bool all_correct = true;
    for (const std::string& pattern : patterns) {
        for (size_t size : sizes) {
            std::string_view text(data.data(), size);
            int reps = repetitionsFor(size, quick);

            // Sequential baseline (no thread pool at all)
            std::vector<size_t> expected = bm::searchSequential(text, pattern); // warm-up
            std::vector<double> seq_times;
            for (int r = 0; r < reps; ++r) {
                auto start = Clock::now();
                expected = bm::searchSequential(text, pattern);
                seq_times.push_back(secondsSince(start));
            }
            Summary seq = summarize(seq_times);
            csv << name << ",sequential," << pattern << ',' << size << ",1," << reps << ','
                << seq.med << ',' << seq.min << ',' << seq.max << ",0,0,0," << seq.med << ",0,0,"
                << expected.size() << ",1\n";

            for (size_t t = 0; t < thread_counts.size(); ++t) {
                int p = thread_counts[t];
                ThreadPool pool(p);

                bm::PhaseTimes phases;
                std::vector<size_t> found = bm::searchParallel(text, pattern, p, pool, &phases); // warm-up
                bool correct = (found == expected);

                std::vector<double> total, pre, dispatch, search, merge, sort;
                for (int r = 0; r < reps; ++r) {
                    auto start = Clock::now();
                    found = bm::searchParallel(text, pattern, p, pool, &phases);
                    total.push_back(secondsSince(start));
                    pre.push_back(phases.preprocess);
                    dispatch.push_back(phases.dispatch);
                    search.push_back(phases.search);
                    merge.push_back(phases.merge);
                    sort.push_back(phases.sort);
                }
                correct = correct && (found == expected);
                all_correct = all_correct && correct;

                Summary par = summarize(total);
                csv << name << ",parallel," << pattern << ',' << size << ',' << p << ',' << reps << ','
                    << par.med << ',' << par.min << ',' << par.max << ','
                    << median(pre) << ',' << pool_cost[t] << ',' << median(dispatch) << ','
                    << median(search) << ',' << median(merge) << ',' << median(sort) << ','
                    << found.size() << ',' << (correct ? 1 : 0) << '\n';
            }
            csv.flush();

            std::cerr << "  pattern=" << pattern.size() << " chars  size=" << size
                      << "  seq=" << seq.med << "s  matches=" << expected.size() << std::endl;
        }
    }

    if (!all_correct) {
        std::cerr << "ERROR: some parallel results differ from the sequential baseline." << std::endl;
        return 2;
    }
    std::cerr << "Done. Results appended to " << out_path << std::endl;
    return 0;
}
