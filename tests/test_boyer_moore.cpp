#include "boyer_moore.h"
#include <iostream>
#include <cassert>

void test_basic_match() {
    std::string text = "AABAACAADAABAABA";
    std::string pattern = "AABA";
    auto results = bm::searchSequential(text, pattern);
    
    assert(results.size() == 3);
    assert(results[0] == 0);
    assert(results[1] == 9);
    assert(results[2] == 12);
    std::cout << "test_basic_match passed!\n";
}

void test_no_match() {
    std::string text = "AABAACAADAABAABA";
    std::string pattern = "XYZ";
    auto results = bm::searchSequential(text, pattern);
    
    assert(results.empty());
    std::cout << "test_no_match passed!\n";
}

void test_dna_sequence() {
    std::string text = "CGTACGTACGTACGTACGTACGTACGTACG";
    std::string pattern = "TACG";
    auto results = bm::searchSequential(text, pattern);
    
    assert(results.size() == 7);
    assert(results[0] == 2);
    assert(results[6] == 26);
    std::cout << "test_dna_sequence passed!\n";
}

int main() {
    std::cout << "Running Phase 1 Sequential Tests...\n";
    test_basic_match();
    test_no_match();
    test_dna_sequence();
    std::cout << "All sequential tests passed successfully!\n";
    return 0;
}
