#pragma once

#include <cstdint>
#include <cstddef>

namespace packet_info
{
    struct PacketHeader 
    {
        uint32_t seq_number;
        uint32_t seq_total;
        uint8_t type;
        std::byte id[8];
    } __attribute__((packed));

    inline constexpr int MAX_PACKET_SIZE = 1472;
    inline constexpr int HEADER_SIZE = sizeof(PacketHeader::seq_number)
                                     + sizeof(PacketHeader::seq_total)
                                     + sizeof(PacketHeader::type)
                                     + sizeof(PacketHeader::id);
    inline constexpr int DATA_SIZE = MAX_PACKET_SIZE - HEADER_SIZE;

}

