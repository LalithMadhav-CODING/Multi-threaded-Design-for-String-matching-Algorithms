// Measures how fast p threads can stream through memory, as an upper bound
// for any scan-based algorithm such as Boyer-Moore.
//
// Usage: membw [bytes] (default 4 GB)

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

uint64_t sumRange(const uint64_t* data, size_t begin, size_t end) {
    uint64_t a = 0, b = 0, c = 0, d = 0;
    size_t i = begin;
    for (; i + 4 <= end; i += 4) {
        a += data[i];
        b += data[i + 1];
        c += data[i + 2];
        d += data[i + 3];
    }
    for (; i < end; ++i) a += data[i];
    return a + b + c + d;
}

} // namespace

int main(int argc, char* argv[]) {
    size_t bytes = argc > 1 ? static_cast<size_t>(std::stod(argv[1])) : 4000000000ULL;
    size_t words = bytes / sizeof(uint64_t);
    std::vector<uint64_t> data(words);
    for (size_t i = 0; i < words; ++i) data[i] = i * 2654435761ULL;

    std::cout << "threads,median_gb_per_s\n";
    for (int p : {1, 2, 4, 8, 12, 16, 20, 24}) {
        std::vector<double> rates;
        for (int rep = 0; rep < 5; ++rep) {
            std::vector<uint64_t> sums(p);
            std::vector<std::thread> threads;
            size_t chunk = words / p;
            auto start = Clock::now();
            for (int t = 0; t < p; ++t) {
                size_t begin = t * chunk;
                size_t end = (t == p - 1) ? words : begin + chunk;
                threads.emplace_back([&, t, begin, end] { sums[t] = sumRange(data.data(), begin, end); });
            }
            for (auto& th : threads) th.join();
            double seconds = std::chrono::duration<double>(Clock::now() - start).count();
            uint64_t total = 0;
            for (uint64_t s : sums) total += s;
            if (total == 42) std::cerr << "";  // keep the sums alive
            rates.push_back(bytes / seconds / 1e9);
        }
        std::sort(rates.begin(), rates.end());
        std::cout << p << ',' << rates[rates.size() / 2] << std::endl;
    }
    return 0;
}
