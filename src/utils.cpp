#include "utils.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace utils {

std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << filepath << std::endl;
        return "";
    }
    
    // For large genomic files, pre-allocating the string buffer is essential for performance
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string buffer(size, ' ');
    file.seekg(0);
    file.read(&buffer[0], size);
    
    return buffer;
}

} // namespace utils
