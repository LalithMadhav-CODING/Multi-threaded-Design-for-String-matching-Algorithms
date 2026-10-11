#include "aho_corasick.h"

namespace aho {

AhoCorasick::AhoCorasick(const std::vector<std::string>& patterns) {
    nodes.emplace_back(); // Root node at index 0
    buildTrie(patterns);
    buildFailureLinks();
}

void AhoCorasick::buildTrie(const std::vector<std::string>& patterns) {
    for (size_t p = 0; p < patterns.size(); ++p) {
        int current = 0;
        for (char c : patterns[p]) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (nodes[current].children[uc] == -1) {
                nodes[current].children[uc] = nodes.size();
                nodes.emplace_back();
            }
            current = nodes[current].children[uc];
        }
        nodes[current].output.push_back(p);
    }
}

void AhoCorasick::buildFailureLinks() {
    std::queue<int> q;
    for (int c = 0; c < 256; ++c) {
        if (nodes[0].children[c] != -1) {
            nodes[nodes[0].children[c]].fail = 0;
            q.push(nodes[0].children[c]);
        } else {
            nodes[0].children[c] = 0;
        }
    }

    while (!q.empty()) {
        int current = q.front();
        q.pop();

        for (int c = 0; c < 256; ++c) {
            int next = nodes[current].children[c];
            if (next != -1) {
                int fail = nodes[current].fail;
                while (nodes[fail].children[c] == -1) {
                    fail = nodes[fail].fail;
                    if (fail == -1) {
                        fail = 0;
                        break;
                    }
                }
                nodes[next].fail = (nodes[fail].children[c] != -1) ? nodes[fail].children[c] : 0;
                
                // Merge outputs
                nodes[next].output.insert(nodes[next].output.end(), 
                                          nodes[nodes[next].fail].output.begin(), 
                                          nodes[nodes[next].fail].output.end());
                
                q.push(next);
            }
        }
    }
}

std::vector<size_t> AhoCorasick::searchChunk(const std::string* text, size_t start_idx, size_t end_idx) const {
    std::vector<size_t> occurrences;
    int current = 0;
    
    for (size_t i = start_idx; i < end_idx; ++i) {
        unsigned char c = static_cast<unsigned char>((*text)[i]);
        
        while (current != 0 && nodes[current].children[c] == -1) {
            current = nodes[current].fail;
        }
        
        if (nodes[current].children[c] != -1) {
            current = nodes[current].children[c];
        } else {
            current = 0;
        }
        
        if (!nodes[current].output.empty()) {
            occurrences.push_back(i);
        }
    }
    return occurrences;
}

} // namespace aho
