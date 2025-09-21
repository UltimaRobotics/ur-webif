
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <chrono>

namespace FirmwareUpdate {

enum class TftpError {
    NONE = 0,
    TIMEOUT = 1,
    NETWORK_ERROR = 2,
    FILE_NOT_FOUND = 3,
    ACCESS_VIOLATION = 4,
    DISK_FULL = 5,
    ILLEGAL_OPERATION = 6,
    UNKNOWN_TID = 7,
    FILE_EXISTS = 8,
    NO_SUCH_USER = 9,
    INVALID_RESPONSE = 10,
    CONNECTION_FAILED = 11
};

struct TftpProgress {
    size_t bytes_transferred;
    size_t total_bytes;
    double percentage;
    std::chrono::milliseconds elapsed_time;
    std::string status_message;
};

class TftpClient {
public:
    using ProgressCallback = std::function<void(const TftpProgress&)>;
    using CompletionCallback = std::function<void(TftpError error, const std::string& message)>;

    TftpClient();
    ~TftpClient();

    // Configuration
    void set_server_address(const std::string& address);
    void set_server_port(uint16_t port);
    void set_timeout(std::chrono::milliseconds timeout);
    void set_retries(int retries);
    void set_block_size(uint16_t block_size);

    // Callbacks
    void set_progress_callback(ProgressCallback callback);
    void set_completion_callback(CompletionCallback callback);

    // Operations
    bool test_connection();
    bool download_file(const std::string& remote_filename, std::vector<uint8_t>& buffer);
    bool download_file_to_path(const std::string& remote_filename, const std::string& local_path);
    void cancel_operation();

    // Status
    bool is_connected() const;
    bool is_downloading() const;
    TftpError get_last_error() const;
    std::string get_last_error_message() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

// Utility functions
std::string tftp_error_to_string(TftpError error);
bool is_valid_tftp_filename(const std::string& filename);
bool is_valid_ip_address(const std::string& address);

} // namespace FirmwareUpdate
