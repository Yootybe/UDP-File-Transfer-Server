#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <thread>
#include <unordered_map>
#include <vector>
#include <sys/stat.h>

#include "help_funcs.h"
#include "packet_info.h"

// Use filepath, file size, info about permissions and owners to generate unique 8 chars file id
std::optional<std::array<std::byte, 8>> generate_8_size_file_id(const std::string& filepath)
{
    struct stat file_info;
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

    std::hash<std::string> hasher;
    const size_t hashed = hasher(data);

    std::array<std::byte, 8> id;
    for (size_t i = 0; i < 8; ++i)
    {
        id[i] = static_cast<std::byte>((hashed >> (i * 8)) & 0xFF);
    }

    return id;
}

struct Packet
{
    packet_info::PacketHeader header;
    std::vector<std::byte> data;

    std::vector<std::byte> serialize() const
    {
        std::vector<std::byte> buffer(packet_info::MAX_PACKET_SIZE);
        memcpy(buffer.data(), &header, sizeof(packet_info::PacketHeader));
        memcpy(buffer.data() + packet_info::HEADER_SIZE, data.data(), data.size());
        return buffer;
    }
};


bool receive_ack(int sock, uint32_t& acked_seq, uint32_t& acked_total, std::byte* id_out, uint32_t& checksum_out, bool& is_final_ack)
{
    std::byte buffer[packet_info::MAX_PACKET_SIZE];
    sockaddr_in from{};
    socklen_t fromlen = sizeof(from);
    ssize_t len = recvfrom(sock, buffer, sizeof(buffer), MSG_DONTWAIT, (sockaddr*)&from, &fromlen);

    if (len <= 0)
    {
        return false;
    }

    const size_t packet_header_id_offset = sizeof(packet_info::PacketHeader::seq_number)
                                         + sizeof(packet_info::PacketHeader::seq_total)
                                         + sizeof(packet_info::PacketHeader::type);

    memcpy(&acked_seq, buffer, sizeof(packet_info::PacketHeader::seq_number));
    memcpy(&acked_total, buffer + sizeof(packet_info::PacketHeader::seq_number), sizeof(packet_info::PacketHeader::seq_total));
    memcpy(id_out, buffer + packet_header_id_offset, sizeof(packet_info::PacketHeader::id));
    is_final_ack = false;

    if (len > packet_info::HEADER_SIZE)
    {
        const size_t checksum_size = sizeof(uint32_t);
        memcpy(&checksum_out, buffer + packet_info::HEADER_SIZE, checksum_size);
        is_final_ack = true;
    }

    return true;
}

int main(int argc, char* argv[])
{
    // Use localhost. This can be either a user-set variable
    const char* server_ip = "127.0.0.1";

    std::string filepath;
    int server_port = 12345;

    if (argc < 2)
    {
        std::cerr << "It is necessary to define the path to the file as the first argument of the program" << std::endl;
        return 1;
    }
    filepath = argv[1];

    if (argc < 3)
    {
        std::cout << "Default server port will be used: '" << server_port << "'" << std::endl;
    }
    else
    {
        server_port = std::stoi(argv[2]);
        std::cout << "Server port: '" << server_port << "' will be used" << std::endl;
    }

    // Generate file ID
    const auto opt_file_id = generate_8_size_file_id(filepath);
    if (!opt_file_id)
    {
        std::cerr << "An error occurred while creating the file ID" << std::endl;
        return 1;
    }
    std::byte file_id[sizeof(packet_info::PacketHeader::id)];
    memcpy(file_id, opt_file_id.value().data(), sizeof(packet_info::PacketHeader::id));

    // Open file and calculate number of packets
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file)
    {
        std::cerr << "Failed to open file. Error: " << std::strerror(errno) << std::endl;
        return errno;
    }

    const uint64_t file_stream_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> file_data(file_stream_size);
    if (!file.read(reinterpret_cast<char*>(file_data.data()), file_stream_size))
    {
        std::cerr << "Failed to read file. Error: " << std::strerror(errno) << std::endl;
        return errno;
    }

    const size_t file_size = file_data.size();
    const uint32_t total_packets = (file_size + packet_info::DATA_SIZE - 1) / packet_info::DATA_SIZE;

    // Split file into packets
    std::vector<Packet> packets;
    for (uint32_t i = 0; i < total_packets; ++i)
    {
        Packet pkt{};
        pkt.header.seq_number = i;
        pkt.header.seq_total = total_packets;
        pkt.header.type = 1; // PUT
        memcpy(pkt.header.id, file_id, sizeof(packet_info::PacketHeader::id));

        const size_t start = i * packet_info::DATA_SIZE;
        const size_t end = std::min(start + packet_info::DATA_SIZE, file_size);
        pkt.data.assign(file_data.begin() + start, file_data.begin() + end);

        packets.push_back(std::move(pkt));
    }

    // Shuffle pakets
    // NOTE: This could be a separate function placed in 'help_funcs'
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(packets.begin(), packets.end(), g);

    bool shuffle_successful = false;
    for (uint32_t i = 0; i < total_packets; ++i)
    {
        if (packets[i].header.seq_number != i)
        {
            shuffle_successful = true;
            break;
        }
    }

    if (!shuffle_successful)
    {
        std::cerr << "Failed to shuffle packets" << std::endl;
        return 1;
    }

    std::cout << "Packets shuffled successfully" << std::endl;

    // Create UDP socket
    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        std::cerr << "Failed to create UDP socket. Error: " << std::strerror(errno) << std::endl;
        return errno;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // The server may send the same ACK packet multiple times
    // But only unique ones should be taken into account
    std::set<uint32_t> acked;

    // Write data packets (PUT) in a convenient form for sending to the server
    std::unordered_map<uint32_t, Packet> seq_to_packet;
    for (const auto& pkt : packets)
    {
        seq_to_packet[pkt.header.seq_number] = pkt;
    }

    // For each sent data packet (PUT), we expect an ACK packet from the server.
    while (acked.size() < total_packets)
    {
        for (const auto& [seq, pkt] : seq_to_packet)
        {
            if (acked.count(seq))
            {
                continue;
            }

            auto buffer = pkt.serialize();
            sendto(sock, buffer.data(), packet_info::HEADER_SIZE + pkt.data.size(), 0, (sockaddr*)&server_addr, sizeof(server_addr));
        }

        // Wait a little while until the server processes the data and sends ACK packets.
        // NOTE: Wait time can be made a configurable parameter
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        while(true) 
        {
            uint32_t ack_seq, ack_total, server_checksum;
            std::byte recv_file_id[sizeof(packet_info::PacketHeader::id)];
            bool is_final;
            if (!receive_ack(sock, ack_seq, ack_total, recv_file_id, server_checksum, is_final))
            {
                break;
            }
            if (std::memcmp(file_id, recv_file_id, sizeof(packet_info::PacketHeader::id)) == 0)
            {
                acked.insert(ack_seq);
                if (is_final && ack_total == total_packets)
                {
                    const uint32_t local_checksum = checksum::crc32c(0, reinterpret_cast<const unsigned char*>(file_data.data()), file_data.size());
                    std::cout << "Final ACK received. " 
                              << "Local CRC: " << std::hex << local_checksum
                              << ", "
                              << "Server CRC: " << std::hex << server_checksum
                              << std::endl;
                    if (local_checksum == server_checksum)
                    {
                        std::cout << "Local and Server checksum matches. File transfer successful" << std::endl;
                    }
                    else
                    {
                        std::cerr << "Error: Local and Server checksum mismatch" << std::endl;
                    }
                    close(sock);
                    return 0;
                }
            }
        }
    }

    std::cerr << "Transfer incomplete" << std::endl;
    close(sock);
    return 1;
}
