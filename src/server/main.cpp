#include <iostream>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "help_funcs.h"
#include "packet_info.h"

// Buffer for single file
struct FileBuffer 
{
    uint32_t total_packets = 0;

    // std::unordered_map used because
    // - need quick access by package number
    // - packets can arrive in any order

    // Key - seq_number
    // Value - data of packet
    std::unordered_map<uint32_t, std::vector<std::byte>> data_packets;
};

// std::unordered_map used because
// - need quick access by package number
// - packets can arrive in any order

// Storage stores several different files that are transferred at the same time
// Key - file id (as std::string for easier hash calculation)
// Value - File buffer, the file itself in memory
std::unordered_map<std::string, FileBuffer> file_storage;


void send_ack(int sockfd, const sockaddr_in &client_addr, socklen_t addr_len,
              const packet_info::PacketHeader& header, bool complete = false, uint32_t checksum = 0)
{
    std::byte ack_packet[packet_info::MAX_PACKET_SIZE] = {};
    packet_info::PacketHeader* ack = reinterpret_cast<packet_info::PacketHeader*>(ack_packet);
    *ack = header;
    ack->type = 0;

    if (complete)
    {
        memcpy(ack_packet + sizeof(packet_info::PacketHeader), &checksum, sizeof(checksum));
    }

    auto const send_size = sizeof(packet_info::PacketHeader) + (complete ? sizeof(checksum) : 0);

    sendto(sockfd, ack_packet, send_size, 0, (sockaddr*)&client_addr, addr_len);
}

int main(int argc, char* argv[]) 
{
    int server_port = 12345;

    if (argc < 2)
    {
        std::cout << "Default server port will be used: '" << server_port << "'" << std::endl;
    }
    else
    {
        server_port = std::stoi(argv[1]);
        std::cout << "Server port: '" << server_port << "' will be used" << std::endl;
    }

    // Create UDP socket and bind on any address on used port
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server_addr{}, client_addr{};
    socklen_t addr_len = sizeof(client_addr);
    std::byte buffer[packet_info::MAX_PACKET_SIZE];

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr));

    std::cout << "Server listening on UDP port '" << server_port << "'" << std::endl;

    // Running while the power is on :)
    while (true) 
    {
        const ssize_t len = recvfrom(sockfd, buffer, packet_info::MAX_PACKET_SIZE, 0, (sockaddr*)&client_addr, &addr_len);
        if (len < (ssize_t)sizeof(packet_info::PacketHeader))
        {
            continue;
        } 

        // Check the header to determine the packet type
        packet_info::PacketHeader* header = reinterpret_cast<packet_info::PacketHeader*>(buffer);
        const std::string file_id(reinterpret_cast<char*>(header->id), 8);

        // PUT
        if (header->type == 1)
        {
            // Find the file with the required id and place the data in its buffer
            auto& file = file_storage[file_id];
            std::vector<std::byte> data(buffer + sizeof(packet_info::PacketHeader), buffer + len);
            file.total_packets = header->seq_total;
            file.data_packets[header->seq_number] = std::move(data);

            if (file.data_packets.size() == header->seq_total)
            {
                // If have received all the data, collect it together as one array
                std::vector<std::byte> full_data;
                for (uint32_t i = 0; i < header->seq_total; ++i)
                {
                    full_data.insert(full_data.end(), file.data_packets[i].begin(), file.data_packets[i].end());
                }

                // Calculate checksum and send it to client                
                const uint32_t crc = checksum::crc32c(0, reinterpret_cast<const unsigned char*>(full_data.data()), full_data.size());
                send_ack(sockfd, client_addr, addr_len, *header, true, crc);
            } 
            else 
            {
                // If have not received all the data, send a message to the client about successful recording of the packets
                send_ack(sockfd, client_addr, addr_len, *header);
            }
        }
    }

    close(sockfd);
    return 0;
}