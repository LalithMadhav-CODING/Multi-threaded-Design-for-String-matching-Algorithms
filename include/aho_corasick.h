#ifndef AHO_CORASICK_H
#define AHO_CORASICK_H

#include <vector>
#include <string>
#include <queue>
#include <algorithm>

namespace aho {

struct TrieNode {
    int children[256];
    int fail;
    std::vector<int> output; // Pattern indices ending at this node

    TrieNode() {
        std::fill(std::begin(children), std::end(children), -1);
        fail = -1;
    }
};

class AhoCorasick {
public:
    AhoCorasick(const std::vector<std::string>& patterns);
    
    // Returns indices where a pattern STARTS
    std::vector<size_t> searchChunk(const std::string* text, size_t start_idx, size_t end_idx) const;

    // Length of the longest pattern (chunks must overlap by this minus one)
    size_t maxPatternLength() const { return max_len; }

private:
    std::vector<TrieNode> nodes;
    std::vector<size_t> pattern_lengths;
    size_t max_len = 0;
    void buildTrie(const std::vector<std::string>& patterns);
    void buildFailureLinks();
};

} // namespace aho

#endif // AHO_CORASICK_H
