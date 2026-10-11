#ifndef AHO_CORASICK_H
#define AHO_CORASICK_H

#include <vector>
#include <string>
#include <queue>

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
    
    // Returns indices where a pattern ENDS
    std::vector<size_t> searchChunk(const std::string* text, size_t start_idx, size_t end_idx) const;

private:
    std::vector<TrieNode> nodes;
    void buildTrie(const std::vector<std::string>& patterns);
    void buildFailureLinks();
};

} // namespace aho

#endif // AHO_CORASICK_H
