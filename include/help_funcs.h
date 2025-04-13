#pragma once

#include <cstdint>

#include <iostream>
#include <fstream>
#include <string>
#include <optional>
#include <sys/stat.h>
#include <unistd.h>
#include <array>
#include <openssl/sha.h>

namespace checksum
{
    uint32_t crc32c(uint32_t crc, const unsigned char* buf, size_t len);
}

namespace filehash
{
    std::optional<std::array<std::byte, 8>> generate_8_size_file_id(const std::string& filepath);
}