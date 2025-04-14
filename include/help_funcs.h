#pragma once

#include <cstdint>
#include <unistd.h>

namespace checksum
{
    uint32_t crc32c(uint32_t crc, const unsigned char* buf, size_t len);
}