#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <filesystem>
#include "boyer_moore.h"
#include "parallel_bm.h"
#include "thread_pool.h"
#include "utils.h"
#include "fuzzy_search.h"
#include "simd_search.h"
#include "aho_corasick.h"

#ifdef USE_CUDA
#include "cuda_search.h"
#endif

namespace fs = std::filesystem;

// Wrapper for running Boyer-Moore
double runBoyerMoore(const std::string& text, const std::string& pattern, int num_threads, ThreadPool& pool, size_t& out_count) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<size_t> all_occurrences = bm::searchParallel(text, pattern, num_threads, pool);

    auto end_time = std::chrono::high_resolution_clock::now();
    out_count = all_occurrences.size();
    return std::chrono::duration<double>(end_time - start_time).count();
}

// Wrapper for running Fuzzy Bitap Match
double runFuzzy(const std::string& text, const std::string& pattern, int max_errors, int num_threads, ThreadPool& pool, size_t& out_count) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<std::vector<size_t>>> futures;
    size_t text_len = text.length();
    size_t m = pattern.length();
    size_t chunk_size = text_len / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t start_idx = i * chunk_size;
        size_t end_idx = (i == num_threads - 1) ? text_len : (i + 1) * chunk_size + m - 1;
        if (end_idx > text_len) end_idx = text_len;

        futures.push_back(pool.enqueue(fuzzy::searchChunkFuzzy, &text, &pattern, max_errors, start_idx, end_idx));
    }

    std::vector<size_t> all_occurrences;
    for (auto& f : futures) {
        auto local_results = f.get();
        all_occurrences.insert(all_occurrences.end(), local_results.begin(), local_results.end());
    }

    std::sort(all_occurrences.begin(), all_occurrences.end());
    auto last = std::unique(all_occurrences.begin(), all_occurrences.end());
    all_occurrences.erase(last, all_occurrences.end());

    auto end_time = std::chrono::high_resolution_clock::now();
    out_count = all_occurrences.size();
    return std::chrono::duration<double>(end_time - start_time).count();
}

// Wrapper for running SIMD Match
double runSIMD(const std::string& text, const std::string& pattern, int num_threads, ThreadPool& pool, size_t& out_count) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::future<std::vector<size_t>>> futures;
    size_t text_len = text.length();
    size_t m = pattern.length();
    size_t chunk_size = text_len / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t start_idx = i * chunk_size;
        size_t end_idx = (i == num_threads - 1) ? text_len : (i + 1) * chunk_size + m - 1;
        if (end_idx > text_len) end_idx = text_len;

        futures.push_back(pool.enqueue(simd::searchChunkSIMD, &text, &pattern, start_idx, end_idx));
    }

    std::vector<size_t> all_occurrences;
    for (auto& f : futures) {
        auto local_results = f.get();
        all_occurrences.insert(all_occurrences.end(), local_results.begin(), local_results.end());
    }

    std::sort(all_occurrences.begin(), all_occurrences.end());
    auto last = std::unique(all_occurrences.begin(), all_occurrences.end());
    all_occurrences.erase(last, all_occurrences.end());

    auto end_time = std::chrono::high_resolution_clock::now();
    out_count = all_occurrences.size();
    return std::chrono::duration<double>(end_time - start_time).count();
}

// Wrapper for running Aho-Corasick Multi-Pattern Search
double runAhoCorasick(const std::string& text, const std::string& pattern, int num_threads, ThreadPool& pool, size_t& out_count) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // We can simulate a multi-pattern search by deriving a few mutations of the pattern
    std::vector<std::string> patterns = {
        pattern,
        pattern.substr(0, pattern.length() - 1) + "A", // mutation 1
        pattern.substr(0, pattern.length() - 1) + "T"  // mutation 2
    };
    
    aho::AhoCorasick ac(patterns);

    std::vector<std::future<std::vector<size_t>>> futures;
    size_t text_len = text.length();
    size_t m = ac.maxPatternLength();
    size_t chunk_size = text_len / num_threads;

    for (int i = 0; i < num_threads; ++i) {
        size_t start_idx = i * chunk_size;
        size_t end_idx = (i == num_threads - 1) ? text_len : (i + 1) * chunk_size + m - 1;
        if (end_idx > text_len) end_idx = text_len;

        futures.push_back(pool.enqueue([&ac, &text, start_idx, end_idx]() {
            return ac.searchChunk(&text, start_idx, end_idx);
        }));
    }

    std::vector<size_t> all_occurrences;
    for (auto& f : futures) {
        auto local_results = f.get();
        all_occurrences.insert(all_occurrences.end(), local_results.begin(), local_results.end());
    }

    std::sort(all_occurrences.begin(), all_occurrences.end());
    auto last = std::unique(all_occurrences.begin(), all_occurrences.end());
    all_occurrences.erase(last, all_occurrences.end());

    auto end_time = std::chrono::high_resolution_clock::now();
    out_count = all_occurrences.size();
    return std::chrono::duration<double>(end_time - start_time).count();
}

void runTestSuite(const std::string& filepath, const std::string& pattern, int num_threads, const std::string& algo) {
    if (num_threads <= 0) {
        num_threads = 1;
    }

    std::cout << "\nLoading dataset..." << std::endl;
    std::string text = utils::readFile(filepath);
    if (text.empty()) {
        std::cerr << "Text is empty or file not found." << std::endl;
        return;
    }
    std::cout << "Text loaded. Length: " << text.length() << " characters." << std::endl;
    std::cout << "Pattern: '" << pattern << "' | Threads: " << num_threads << std::endl;
    std::cout << "======================================" << std::endl;

    ThreadPool pool(num_threads);

    auto runAndReport = [&](const std::string& name, auto func) {
        std::cout << "[Running] " << name << "..." << std::flush;
        size_t count = 0;
        double time_taken = func(count);
        std::cout << " Done!" << std::endl;
        std::cout << "  -> Found: " << count << " occurrences" << std::endl;
        std::cout << "  -> Time:  " << time_taken << " seconds" << std::endl;
        std::cout << "--------------------------------------" << std::endl;
    };

    if (algo == "bm" || algo == "all") {
        runAndReport("Boyer-Moore (Exact)", [&](size_t& count) {
            return runBoyerMoore(text, pattern, num_threads, pool, count);
        });
    }
    
    if (algo == "fuzzy" || algo == "all") {
        int max_errors = 1; // You can change this to 2, 3, etc.
        runAndReport("Fuzzy Bitap (k=" + std::to_string(max_errors) + ")", [&](size_t& count) {
            if (pattern.length() > 64) {
                std::cerr << " Pattern too long for Bitap (<64 chars required) ";
                return 0.0;
            }
            return runFuzzy(text, pattern, max_errors, num_threads, pool, count);
        });
    }

    if (algo == "simd" || algo == "all") {
        runAndReport("AVX2 SIMD (Exact)", [&](size_t& count) {
            return runSIMD(text, pattern, num_threads, pool, count);
        });
    }

    if (algo == "aho" || algo == "all") {
        runAndReport("Aho-Corasick (Multi-Pattern)", [&](size_t& count) {
            return runAhoCorasick(text, pattern, num_threads, pool, count);
        });
    }

    if (algo == "cuda" || algo == "all") {
#ifdef USE_CUDA
        runAndReport("CUDA GPU (Exact)", [&](size_t& count) {
            return cuda_search::runCUDA(text, pattern, count);
        });
#else
        std::cout << "[Skipping] CUDA GPU (Exact) - Not compiled with USE_CUDA" << std::endl;
        std::cout << "--------------------------------------" << std::endl;
#endif
    }
}

void interactiveMenu() {
    std::cout << "\n=== String Matching Test Suite ===\n";
    std::cout << "Available Datasets (in ../datasets/):\n";
    
    std::vector<std::string> datasets;
    int idx = 1;
    for (const auto& entry : fs::directory_iterator("../datasets/")) {
        if (entry.is_regular_file() && entry.path().extension() == ".fna") {
            datasets.push_back(entry.path().string());
            std::cout << idx++ << ". " << entry.path().filename().string() << " (" << entry.file_size() / (1024*1024) << " MB)\n";
        }
    }
    
    if (datasets.empty()) {
        std::cout << "No datasets found in ../datasets/ directory.\n";
        return;
    }
    
    int selection;
    std::cout << "\nSelect dataset (1-" << datasets.size() << "): ";
    std::cin >> selection;
    if (selection < 1 || selection > (int)datasets.size()) {
        std::cout << "Invalid selection.\n";
        return;
    }
    std::string filepath = datasets[selection - 1];
    
    std::string pattern;
    std::cout << "Enter pattern to search (e.g. ACGT): ";
    std::cin >> pattern;
    
    int num_threads;
    std::cout << "Enter number of threads (0 for max hardware threads): ";
    std::cin >> num_threads;
    if (num_threads <= 0) {
        num_threads = std::thread::hardware_concurrency();
    }
    
    std::cout << "\nAlgorithms:\n";
    std::cout << "1. Boyer-Moore (Exact)\n";
    std::cout << "2. Fuzzy Bitap (Approximate)\n";
    std::cout << "3. AVX2 SIMD (Exact)\n";
    std::cout << "4. Aho-Corasick (Multi-Pattern)\n";
    std::cout << "5. CUDA GPU (Exact)\n";
    std::cout << "6. Run All Benchmarks\n";
    std::cout << "Select algorithm (1-6): ";
    
    int algo_sel;
    std::cin >> algo_sel;
    
    std::string algo = "all";
    if (algo_sel == 1) algo = "bm";
    else if (algo_sel == 2) algo = "fuzzy";
    else if (algo_sel == 3) algo = "simd";
    else if (algo_sel == 4) algo = "aho";
    else if (algo_sel == 5) algo = "cuda";
    else if (algo_sel == 6) algo = "all";
    else {
        std::cout << "Invalid algorithm selection.\n";
        return;
    }
    
    runTestSuite(filepath, pattern, num_threads, algo);
}

int main(int argc, char* argv[]) {
    // If no arguments provided, launch the interactive menu
    if (argc < 2) {
        interactiveMenu();
        return 0;
    }

    if (argc < 3) {
        std::cerr << "Usage (CLI): " << argv[0] << " <filepath> <pattern> [num_threads] [--algo <algo>]" << std::endl;
        std::cerr << "Algorithms: bm, fuzzy, simd, aho, cuda, all" << std::endl;
        return 1;
    }

    std::string filepath = argv[1];
    std::string pattern = argv[2];
    int num_threads = std::thread::hardware_concurrency();
    std::string algo = "bm";

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--algo" && i + 1 < argc) {
            algo = argv[++i];
        } else {
            try { num_threads = std::stoi(arg); } catch(...) {}
        }
    }

    runTestSuite(filepath, pattern, num_threads, algo);
    return 0;
}
