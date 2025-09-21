
#include "tftp_client.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <regex>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace FirmwareUpdate {

// TFTP Protocol Constants
constexpr uint16_t TFTP_OPCODE_RRQ = 1;    // Read Request
constexpr uint16_t TFTP_OPCODE_WRQ = 2;    // Write Request
constexpr uint16_t TFTP_OPCODE_DATA = 3;   // Data
constexpr uint16_t TFTP_OPCODE_ACK = 4;    // Acknowledgment
constexpr uint16_t TFTP_OPCODE_ERROR = 5;  // Error

constexpr uint16_t TFTP_DEFAULT_PORT = 69;
constexpr uint16_t TFTP_DEFAULT_BLOCK_SIZE = 512;
constexpr uint16_t TFTP_MAX_BLOCK_SIZE = 65464;
constexpr size_t TFTP_HEADER_SIZE = 4;
constexpr size_t TFTP_MAX_PACKET_SIZE = TFTP_MAX_BLOCK_SIZE + TFTP_HEADER_SIZE;

struct TftpPacket {
    uint16_t opcode;
    uint16_t block_number;
    std::vector<uint8_t> data;
    
    TftpPacket() : opcode(0), block_number(0) {}
    
    std::vector<uint8_t> serialize() const {
        std::vector<uint8_t> packet;
        
        // Add opcode (big-endian)
        packet.push_back((opcode >> 8) & 0xFF);
        packet.push_back(opcode & 0xFF);
        
        if (opcode == TFTP_OPCODE_RRQ || opcode == TFTP_OPCODE_WRQ) {
            // Add filename and mode
            packet.insert(packet.end(), data.begin(), data.end());
        } else if (opcode == TFTP_OPCODE_DATA || opcode == TFTP_OPCODE_ACK) {
            // Add block number (big-endian)
            packet.push_back((block_number >> 8) & 0xFF);
            packet.push_back(block_number & 0xFF);
            
            if (opcode == TFTP_OPCODE_DATA) {
                // Add data
                packet.insert(packet.end(), data.begin(), data.end());
            }
        } else if (opcode == TFTP_OPCODE_ERROR) {
            // Add error code and message
            packet.push_back((block_number >> 8) & 0xFF);  // Error code in block_number field
            packet.push_back(block_number & 0xFF);
            packet.insert(packet.end(), data.begin(), data.end());
        }
        
        return packet;
    }
    
    bool deserialize(const std::vector<uint8_t>& packet_data) {
        if (packet_data.size() < 2) return false;
        
        opcode = (packet_data[0] << 8) | packet_data[1];
        
        if (opcode == TFTP_OPCODE_DATA) {
            if (packet_data.size() < 4) return false;
            block_number = (packet_data[2] << 8) | packet_data[3];
            data.assign(packet_data.begin() + 4, packet_data.end());
        } else if (opcode == TFTP_OPCODE_ACK) {
            if (packet_data.size() < 4) return false;
            block_number = (packet_data[2] << 8) | packet_data[3];
        } else if (opcode == TFTP_OPCODE_ERROR) {
            if (packet_data.size() < 4) return false;
            block_number = (packet_data[2] << 8) | packet_data[3];  // Error code
            data.assign(packet_data.begin() + 4, packet_data.end());
        }
        
        return true;
    }
};

struct TftpClient::Impl {
    std::string server_address;
    uint16_t server_port;
    std::chrono::milliseconds timeout;
    int retries;
    uint16_t block_size;
    
    ProgressCallback progress_callback;
    CompletionCallback completion_callback;
    
    SOCKET socket_fd;
    sockaddr_in server_addr;
    sockaddr_in data_addr;
    
    std::atomic<bool> connected;
    std::atomic<bool> downloading;
    std::atomic<bool> cancelled;
    
    TftpError last_error;
    std::string last_error_message;
    
    std::mutex state_mutex;
    std::condition_variable state_cv;
    
    Impl() : server_port(TFTP_DEFAULT_PORT),
             timeout(std::chrono::milliseconds(5000)),
             retries(3),
             block_size(TFTP_DEFAULT_BLOCK_SIZE),
             socket_fd(INVALID_SOCKET),
             connected(false),
             downloading(false),
             cancelled(false),
             last_error(TftpError::NONE) {
        
        memset(&server_addr, 0, sizeof(server_addr));
        memset(&data_addr, 0, sizeof(data_addr));
        
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    
    ~Impl() {
        close_socket();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    void close_socket() {
        if (socket_fd != INVALID_SOCKET) {
            closesocket(socket_fd);
            socket_fd = INVALID_SOCKET;
        }
        connected = false;
    }
    
    bool create_socket() {
        close_socket();
        
        socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd == INVALID_SOCKET) {
            set_error(TftpError::NETWORK_ERROR, "Failed to create socket");
            return false;
        }
        
        // Set socket timeout
        #ifdef _WIN32
            DWORD timeout_ms = static_cast<DWORD>(timeout.count());
            if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
                std::cout << "[TFTP] Warning: Failed to set receive timeout" << std::endl;
            }
            if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout_ms, sizeof(timeout_ms)) == SOCKET_ERROR) {
                std::cout << "[TFTP] Warning: Failed to set send timeout" << std::endl;
            }
        #else
            struct timeval tv;
            tv.tv_sec = timeout.count() / 1000;
            tv.tv_usec = (timeout.count() % 1000) * 1000;
            if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                std::cout << "[TFTP] Warning: Failed to set receive timeout: " << strerror(errno) << std::endl;
            }
            if (setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
                std::cout << "[TFTP] Warning: Failed to set send timeout: " << strerror(errno) << std::endl;
            }
        #endif
        
        // Set socket buffer sizes
        int buffer_size = 65536; // 64KB
        setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));
        setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(buffer_size));
        
        std::cout << "[TFTP] Socket created with " << timeout.count() << "ms timeout" << std::endl;
        return true;
    }
    
    bool resolve_server() {
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        
        if (inet_pton(AF_INET, server_address.c_str(), &server_addr.sin_addr) == 1) {
            return true;
        }
        
        // Try to resolve hostname
        struct hostent* he = gethostbyname(server_address.c_str());
        if (he == nullptr) {
            set_error(TftpError::NETWORK_ERROR, "Failed to resolve server address");
            return false;
        }
        
        memcpy(&server_addr.sin_addr, he->h_addr_list[0], he->h_length);
        return true;
    }
    
    bool send_packet(const TftpPacket& packet, const sockaddr_in& addr) {
        auto data = packet.serialize();
        
        int result = sendto(socket_fd, reinterpret_cast<const char*>(data.data()), 
                           static_cast<int>(data.size()), 0,
                           reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
        
        if (result == SOCKET_ERROR) {
            set_error(TftpError::NETWORK_ERROR, "Failed to send packet");
            return false;
        }
        
        return true;
    }
    
    bool receive_packet(TftpPacket& packet, sockaddr_in& from_addr) {
        char buffer[TFTP_MAX_PACKET_SIZE];
        socklen_t addr_len = sizeof(from_addr);
        
        int result = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                             reinterpret_cast<sockaddr*>(&from_addr), &addr_len);
        
        if (result == SOCKET_ERROR) {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                set_error(TftpError::TIMEOUT, "Receive timeout");
            } else {
                set_error(TftpError::NETWORK_ERROR, "Network error: " + std::to_string(error));
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                set_error(TftpError::TIMEOUT, "Receive timeout");
            } else {
                set_error(TftpError::NETWORK_ERROR, "Network error: " + std::string(strerror(errno)));
            }
#endif
            return false;
        }
        
        if (result == 0) {
            set_error(TftpError::NETWORK_ERROR, "Connection closed by peer");
            return false;
        }
        
        std::vector<uint8_t> packet_data(buffer, buffer + result);
        return packet.deserialize(packet_data);
    }
    
    TftpPacket create_read_request(const std::string& filename) {
        TftpPacket packet;
        packet.opcode = TFTP_OPCODE_RRQ;
        
        // Add filename
        packet.data.insert(packet.data.end(), filename.begin(), filename.end());
        packet.data.push_back(0); // Null terminator
        
        // Add mode (octet/binary)
        std::string mode = "octet";
        packet.data.insert(packet.data.end(), mode.begin(), mode.end());
        packet.data.push_back(0); // Null terminator
        
        return packet;
    }
    
    TftpPacket create_ack_packet(uint16_t block_num) {
        TftpPacket packet;
        packet.opcode = TFTP_OPCODE_ACK;
        packet.block_number = block_num;
        return packet;
    }
    
    void set_error(TftpError error, const std::string& message) {
        std::lock_guard<std::mutex> lock(state_mutex);
        last_error = error;
        last_error_message = message;
        std::cout << "[TFTP] Error: " << message << std::endl;
    }
    
    void update_progress(size_t bytes_transferred, size_t total_bytes, 
                        const std::string& status, std::chrono::steady_clock::time_point start_time) {
        if (progress_callback) {
            TftpProgress progress;
            progress.bytes_transferred = bytes_transferred;
            progress.total_bytes = total_bytes;
            progress.percentage = total_bytes > 0 ? (double(bytes_transferred) / total_bytes) * 100.0 : 0.0;
            progress.elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);
            progress.status_message = status;
            
            progress_callback(progress);
        }
    }
};

TftpClient::TftpClient() : pimpl(std::make_unique<Impl>()) {}

TftpClient::~TftpClient() = default;

void TftpClient::set_server_address(const std::string& address) {
    pimpl->server_address = address;
}

void TftpClient::set_server_port(uint16_t port) {
    pimpl->server_port = port;
}

void TftpClient::set_timeout(std::chrono::milliseconds timeout) {
    pimpl->timeout = timeout;
}

void TftpClient::set_retries(int retries) {
    pimpl->retries = retries;
}

void TftpClient::set_block_size(uint16_t block_size) {
    pimpl->block_size = block_size;
}

void TftpClient::set_progress_callback(ProgressCallback callback) {
    pimpl->progress_callback = callback;
}

void TftpClient::set_completion_callback(CompletionCallback callback) {
    pimpl->completion_callback = callback;
}

bool TftpClient::test_connection() {
    std::cout << "[TFTP] Testing connection to " << pimpl->server_address << ":" << pimpl->server_port << std::endl;
    
    if (!pimpl->create_socket()) {
        return false;
    }
    
    if (!pimpl->resolve_server()) {
        return false;
    }
    
    // Create a simple read request for a non-existent file to test connectivity
    TftpPacket test_packet = pimpl->create_read_request("__test_connection__");
    
    if (!pimpl->send_packet(test_packet, pimpl->server_addr)) {
        return false;
    }
    
    // Try to receive response (should be an error, but that's OK - it means server is responding)
    TftpPacket response;
    sockaddr_in from_addr;
    
    bool received = pimpl->receive_packet(response, from_addr);
    pimpl->close_socket();
    
    if (received) {
        pimpl->connected = true;
        std::cout << "[TFTP] Connection test successful" << std::endl;
        return true;
    } else {
        pimpl->set_error(TftpError::CONNECTION_FAILED, "TFTP server not responding");
        return false;
    }
}

bool TftpClient::download_file(const std::string& remote_filename, std::vector<uint8_t>& buffer) {
    std::cout << "[TFTP] Starting download of " << remote_filename << std::endl;
    
    pimpl->downloading = true;
    pimpl->cancelled = false;
    buffer.clear();
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (!pimpl->create_socket()) {
        pimpl->downloading = false;
        return false;
    }
    
    if (!pimpl->resolve_server()) {
        pimpl->downloading = false;
        return false;
    }
    
    bool success = false;
    
    // Retry the entire download process
    for (int attempt = 0; attempt <= pimpl->retries && !pimpl->cancelled && !success; ++attempt) {
        if (attempt > 0) {
            std::cout << "[TFTP] Retry attempt " << attempt << "/" << pimpl->retries << std::endl;
            buffer.clear(); // Clear buffer for retry
            
            // Recreate socket for retry
            if (!pimpl->create_socket()) {
                continue;
            }
        }
        
        // Send read request
        TftpPacket rrq = pimpl->create_read_request(remote_filename);
        if (!pimpl->send_packet(rrq, pimpl->server_addr)) {
            std::cout << "[TFTP] Failed to send read request on attempt " << attempt << std::endl;
            continue;
        }
        
        uint16_t expected_block = 1;
        sockaddr_in data_server_addr = pimpl->server_addr;
        bool download_in_progress = true;
        size_t estimated_total_size = 0;
        
        // Main download loop for this attempt
        while (!pimpl->cancelled && download_in_progress) {
            TftpPacket response;
            sockaddr_in from_addr;
            
            if (!pimpl->receive_packet(response, from_addr)) {
                std::cout << "[TFTP] Failed to receive packet for block " << expected_block << std::endl;
                download_in_progress = false;
                break; // Timeout or error, will retry
            }
            
            // Update data server address from first response
            if (expected_block == 1) {
                data_server_addr = from_addr;
                std::cout << "[TFTP] Data server address updated" << std::endl;
            }
            
            if (response.opcode == TFTP_OPCODE_ERROR) {
                uint16_t error_code = response.block_number;
                std::string error_msg(response.data.begin(), response.data.end());
                
                TftpError tftp_error = TftpError::UNKNOWN_TID;
                switch (error_code) {
                    case 1: tftp_error = TftpError::FILE_NOT_FOUND; break;
                    case 2: tftp_error = TftpError::ACCESS_VIOLATION; break;
                    case 3: tftp_error = TftpError::DISK_FULL; break;
                    case 4: tftp_error = TftpError::ILLEGAL_OPERATION; break;
                    case 5: tftp_error = TftpError::UNKNOWN_TID; break;
                    case 6: tftp_error = TftpError::FILE_EXISTS; break;
                    case 7: tftp_error = TftpError::NO_SUCH_USER; break;
                    default: tftp_error = TftpError::INVALID_RESPONSE; break;
                }
                
                pimpl->set_error(tftp_error, "TFTP Error " + std::to_string(error_code) + ": " + error_msg);
                download_in_progress = false;
                break; // Don't retry on server errors
            }
            
            if (response.opcode != TFTP_OPCODE_DATA) {
                pimpl->set_error(TftpError::INVALID_RESPONSE, "Unexpected packet type: " + std::to_string(response.opcode));
                download_in_progress = false;
                break;
            }
            
            if (response.block_number != expected_block) {
                // Handle duplicate or out-of-order packets
                if (response.block_number < expected_block) {
                    // Duplicate packet, send ACK again
                    TftpPacket ack = pimpl->create_ack_packet(response.block_number);
                    pimpl->send_packet(ack, data_server_addr);
                }
                continue;
            }
            
            // Add data to buffer
            buffer.insert(buffer.end(), response.data.begin(), response.data.end());
            
            // Send ACK
            TftpPacket ack = pimpl->create_ack_packet(expected_block);
            if (!pimpl->send_packet(ack, data_server_addr)) {
                std::cout << "[TFTP] Failed to send ACK for block " << expected_block << std::endl;
                download_in_progress = false;
                break;
            }
            
            // Estimate total size based on block size
            if (response.data.size() == pimpl->block_size) {
                estimated_total_size = buffer.size() * 2; // Rough estimate
            } else {
                estimated_total_size = buffer.size(); // Final size
            }
            
            // Update progress with estimated total
            pimpl->update_progress(buffer.size(), estimated_total_size, "Downloading block " + std::to_string(expected_block), start_time);
            
            std::cout << "[TFTP] Received block " << expected_block << " (" << response.data.size() << " bytes)" << std::endl;
            
            // Check if this is the last packet (less than block size)
            if (response.data.size() < pimpl->block_size) {
                success = true;
                download_in_progress = false;
                break;
            }
            
            expected_block++;
        }
        
        if (success) {
            break; // Download completed successfully
        }
    }
    
    pimpl->close_socket();
    pimpl->downloading = false;
    
    if (success && !pimpl->cancelled) {
        std::cout << "[TFTP] Download completed successfully. Size: " << buffer.size() << " bytes" << std::endl;
        pimpl->update_progress(buffer.size(), buffer.size(), "Download complete", start_time);
        
        if (pimpl->completion_callback) {
            pimpl->completion_callback(TftpError::NONE, "Download completed successfully");
        }
        return true;
    } else {
        if (pimpl->cancelled) {
            pimpl->set_error(TftpError::TIMEOUT, "Download cancelled by user");
        } else if (pimpl->last_error == TftpError::NONE) {
            pimpl->set_error(TftpError::TIMEOUT, "Download failed after " + std::to_string(pimpl->retries + 1) + " attempts");
        }
        
        std::cout << "[TFTP] Download failed: " << pimpl->last_error_message << std::endl;
        
        if (pimpl->completion_callback) {
            pimpl->completion_callback(pimpl->last_error, pimpl->last_error_message);
        }
        return false;
    }
}

bool TftpClient::download_file_to_path(const std::string& remote_filename, const std::string& local_path) {
    std::vector<uint8_t> buffer;
    
    if (download_file(remote_filename, buffer)) {
        std::ofstream file(local_path, std::ios::binary);
        if (!file) {
            pimpl->set_error(TftpError::ACCESS_VIOLATION, "Cannot create local file");
            return false;
        }
        
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        file.close();
        
        if (file.good()) {
            std::cout << "[TFTP] File saved to " << local_path << std::endl;
            return true;
        } else {
            pimpl->set_error(TftpError::DISK_FULL, "Failed to write to local file");
            return false;
        }
    }
    
    return false;
}

void TftpClient::cancel_operation() {
    pimpl->cancelled = true;
    std::cout << "[TFTP] Operation cancelled" << std::endl;
}

bool TftpClient::is_connected() const {
    return pimpl->connected;
}

bool TftpClient::is_downloading() const {
    return pimpl->downloading;
}

TftpError TftpClient::get_last_error() const {
    return pimpl->last_error;
}

std::string TftpClient::get_last_error_message() const {
    return pimpl->last_error_message;
}

// Utility functions
std::string tftp_error_to_string(TftpError error) {
    switch (error) {
        case TftpError::NONE: return "No error";
        case TftpError::TIMEOUT: return "Timeout";
        case TftpError::NETWORK_ERROR: return "Network error";
        case TftpError::FILE_NOT_FOUND: return "File not found";
        case TftpError::ACCESS_VIOLATION: return "Access violation";
        case TftpError::DISK_FULL: return "Disk full";
        case TftpError::ILLEGAL_OPERATION: return "Illegal operation";
        case TftpError::UNKNOWN_TID: return "Unknown transfer ID";
        case TftpError::FILE_EXISTS: return "File already exists";
        case TftpError::NO_SUCH_USER: return "No such user";
        case TftpError::INVALID_RESPONSE: return "Invalid response";
        case TftpError::CONNECTION_FAILED: return "Connection failed";
        default: return "Unknown error";
    }
}

bool is_valid_tftp_filename(const std::string& filename) {
    if (filename.empty() || filename.length() > 255) {
        return false;
    }
    
    // Check for invalid characters
    const std::string invalid_chars = "<>:\"|?*";
    for (char c : filename) {
        if (c < 32 || invalid_chars.find(c) != std::string::npos) {
            return false;
        }
    }
    
    return true;
}

bool is_valid_ip_address(const std::string& address) {
    std::regex ip_regex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    return std::regex_match(address, ip_regex);
}

} // namespace FirmwareUpdate
