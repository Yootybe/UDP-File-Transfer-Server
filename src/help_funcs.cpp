#include "help_funcs.h"

namespace checksum 
{

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len)
{
    int k;

    crc = ~crc;
    while (len--)
    {
        crc ^= *buf++;
        for (k = 0; k < 8; k++)
        {
            crc = crc & 1 ? (crc >> 1) ^ 0x82f63b78 : crc >> 1;
        }
    }
    return ~crc;
}

} // namespace checksum

// NOTE: Not using by server, may be moved to client code
namespace filehash
{

// Use filepath, file size, info about permissions and owners to generate unique 8 chars file id
std::optional<std::array<std::byte, 8>> generate_8_size_file_id(const std::string& filepath)
{
    struct stat file_info{};
    if (stat(filepath.c_str(), &file_info) != 0)
    {
        std::cerr << "Error getting file info for '" << filepath << "'" << std::endl;
        return std::nullopt;
    }

    const off_t file_size = file_info.st_size;
    const uid_t owner_id = file_info.st_uid;
    const mode_t permissions = file_info.st_mode;

    const std::string data = filepath
                           + std::to_string(file_size)
                           + std::to_string(owner_id)
                           + std::to_string(permissions);

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256_ctx;
    if (!SHA256_Init(&sha256_ctx) ||
        !SHA256_Update(&sha256_ctx, data.c_str(), data.size()) ||
        !SHA256_Final(hash, &sha256_ctx))
    {
        std::cerr << "Error generating SHA256 hash" << std::endl;
        return std::nullopt;
    }

    std::array<std::byte, 8> id{};
    for (int i = 0; i < 8; ++i)
    {
        id[i] = static_cast<std::byte>(hash[i]);
    }

    return id;
}

} // namespace filehash
