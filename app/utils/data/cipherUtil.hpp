#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>

namespace CipherUtil {
    // XOR simétrico: la misma operación cifra y descifra.
    inline std::vector<unsigned char> xorTransform(const unsigned char* data, size_t size, unsigned char key) {
        std::vector<unsigned char> result(size);
        std::transform(data, data + size, result.begin(),
            [key](unsigned char byte) { return static_cast<unsigned char>(byte ^ key); });
        return result;
    }
}