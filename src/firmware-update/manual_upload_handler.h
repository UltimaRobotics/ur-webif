
#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace FirmwareUpdate {

struct ManualUploadInfo {
    std::string upload_id;
    std::string filename;
    std::string file_path;
    size_t file_size;
    std::string file_type;
    std::string checksum_md5;
    std::string checksum_sha256;
    std::chrono::system_clock::time_point upload_timestamp;
    bool verify_signature;
    std::string status;
    std::string error_message;
};

class ManualUploadHandler {
public:
    ManualUploadHandler();
    ~ManualUploadHandler();

    // Upload firmware file with metadata
    std::string uploadFirmwareFile(const std::vector<uint8_t>& file_data,
                                  const std::string& filename,
                                  const std::string& file_type,
                                  bool verify_signature = false);

    // Get upload information
    std::shared_ptr<ManualUploadInfo> getUploadInfo(const std::string& upload_id);

    // List all uploads
    std::vector<std::shared_ptr<ManualUploadInfo>> listUploads();

    // Delete upload
    bool deleteUpload(const std::string& upload_id);

    // Validate file
    std::string validateFirmwareFile(const std::string& filename, size_t file_size);

    // Clean up old uploads (keep only 3 most recent)
    void cleanupOldUploads();

private:
    std::string generateUploadId();
    std::string calculateMD5(const std::vector<uint8_t>& data);
    std::string calculateSHA256(const std::vector<uint8_t>& data);
    bool saveUploadMetadata(const ManualUploadInfo& info);
    std::shared_ptr<ManualUploadInfo> loadUploadMetadata(const std::string& upload_id);
    bool createUploadDirectory(const std::string& upload_id);
    std::string getUploadDirectory(const std::string& upload_id);
    std::string getManualUploadBaseDirectory();
};

} // namespace FirmwareUpdate
