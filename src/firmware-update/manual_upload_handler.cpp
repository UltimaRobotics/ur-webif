#include "manual_upload_handler.h"
#include "config_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <random>
#include <algorithm>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace FirmwareUpdate {

ManualUploadHandler::ManualUploadHandler() {
    std::cout << "[MANUAL-UPLOAD] Handler initialized" << std::endl;

    // Ensure base directory exists
    std::string base_dir = getManualUploadBaseDirectory();
    try {
        if (!std::filesystem::exists(base_dir)) {
            std::filesystem::create_directories(base_dir);
            std::cout << "[MANUAL-UPLOAD] Base directory created: " << base_dir << std::endl;
        } else {
            std::cout << "[MANUAL-UPLOAD] Base directory already exists: " << base_dir << std::endl;
        }
        
        // Also ensure the downloads directory exists for consistency
        std::string downloads_dir = ConfigManager::getInstance().getDataPath("firmware/downloads");
        if (!std::filesystem::exists(downloads_dir)) {
            std::filesystem::create_directories(downloads_dir);
            std::cout << "[MANUAL-UPLOAD] Downloads directory created: " << downloads_dir << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Error creating directories: " << e.what() << std::endl;
    }
}

ManualUploadHandler::~ManualUploadHandler() {
    std::cout << "[MANUAL-UPLOAD] Handler destroyed" << std::endl;
}

std::string ManualUploadHandler::uploadFirmwareFile(const std::vector<uint8_t>& file_data,
                                                   const std::string& filename,
                                                   const std::string& file_type,
                                                   bool verify_signature) {
    std::cout << "[MANUAL-UPLOAD] Starting upload: " << filename << " (" << file_data.size() << " bytes)" << std::endl;

    // Validate file
    std::string validation_error = validateFirmwareFile(filename, file_data.size());
    if (!validation_error.empty()) {
        std::cout << "[MANUAL-UPLOAD] Validation failed: " << validation_error << std::endl;
        return "";
    }

    // Generate upload ID
    std::string upload_id = generateUploadId();
    std::cout << "[MANUAL-UPLOAD] Generated upload ID: " << upload_id << std::endl;

    // Create upload directory
    if (!createUploadDirectory(upload_id)) {
        std::cout << "[MANUAL-UPLOAD] Failed to create upload directory for: " << upload_id << std::endl;
        return "";
    }

    // Create upload info
    ManualUploadInfo info;
    info.upload_id = upload_id;
    info.filename = filename;
    info.file_path = getUploadDirectory(upload_id) + "/" + filename;
    info.file_size = file_data.size();
    info.file_type = file_type;
    info.checksum_md5 = calculateMD5(file_data);
    info.checksum_sha256 = calculateSHA256(file_data);
    info.upload_timestamp = std::chrono::system_clock::now();
    info.verify_signature = verify_signature;
    info.status = "completed";
    info.error_message = "";

    try {
        // Clean up old uploads (keep only 3 most recent)
        cleanupOldUploads();
        
        // Debug: Check if directory exists
        std::string upload_dir = getUploadDirectory(upload_id);
        std::cout << "[MANUAL-UPLOAD] Upload directory: " << upload_dir << std::endl;
        std::cout << "[MANUAL-UPLOAD] File path: " << info.file_path << std::endl;
        std::cout << "[MANUAL-UPLOAD] Directory exists: " << std::filesystem::exists(upload_dir) << std::endl;
        
        // Save firmware file
        std::ofstream file(info.file_path, std::ios::binary);
        if (!file) {
            std::cout << "[MANUAL-UPLOAD] Failed to create file: " << info.file_path << std::endl;
            std::cout << "[MANUAL-UPLOAD] Error: " << strerror(errno) << std::endl;
            return "";
        }

        std::cout << "[MANUAL-UPLOAD] Writing " << file_data.size() << " bytes to file" << std::endl;
        file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
        file.close();
        
        // Verify file was created
        if (std::filesystem::exists(info.file_path)) {
            auto file_size = std::filesystem::file_size(info.file_path);
            std::cout << "[MANUAL-UPLOAD] File created successfully, size: " << file_size << " bytes" << std::endl;
        } else {
            std::cout << "[MANUAL-UPLOAD] File was not created: " << info.file_path << std::endl;
            return "";
        }

        // Save metadata
        if (!saveUploadMetadata(info)) {
            std::cout << "[MANUAL-UPLOAD] Failed to save metadata for: " << upload_id << std::endl;
            // Clean up file
            std::filesystem::remove(info.file_path);
            return "";
        }

        std::cout << "[MANUAL-UPLOAD] Upload completed successfully: " << upload_id << std::endl;
        return upload_id;

    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Upload error: " << e.what() << std::endl;
        info.status = "failed";
        info.error_message = e.what();
        saveUploadMetadata(info);
        return "";
    }
}

std::shared_ptr<ManualUploadInfo> ManualUploadHandler::getUploadInfo(const std::string& upload_id) {
    return loadUploadMetadata(upload_id);
}

std::vector<std::shared_ptr<ManualUploadInfo>> ManualUploadHandler::listUploads() {
    std::vector<std::shared_ptr<ManualUploadInfo>> uploads;

    try {
        std::string base_dir = getManualUploadBaseDirectory();

        for (const auto& entry : std::filesystem::directory_iterator(base_dir)) {
            if (entry.is_directory()) {
                std::string upload_id = entry.path().filename().string();
                auto upload_info = loadUploadMetadata(upload_id);
                if (upload_info) {
                    uploads.push_back(upload_info);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Error listing uploads: " << e.what() << std::endl;
    }

    return uploads;
}

bool ManualUploadHandler::deleteUpload(const std::string& upload_id) {
    try {
        std::string upload_dir = getUploadDirectory(upload_id);
        std::filesystem::remove_all(upload_dir);
        std::cout << "[MANUAL-UPLOAD] Upload deleted: " << upload_id << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Error deleting upload " << upload_id << ": " << e.what() << std::endl;
        return false;
    }
}

std::string ManualUploadHandler::validateFirmwareFile(const std::string& filename, size_t file_size) {
    const size_t MAX_FILE_SIZE = 32 * 1024 * 1024; // 32MB
    const std::vector<std::string> ALLOWED_EXTENSIONS = {".bin", ".img", ".trx"};

    if (file_size == 0) {
        return "File is empty";
    }

    if (file_size > MAX_FILE_SIZE) {
        return "File too large (maximum 32MB)";
    }

    // Check file extension
    std::string lower_filename = filename;
    std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);

    bool valid_extension = false;
    for (const auto& ext : ALLOWED_EXTENSIONS) {
        if (lower_filename.size() >= ext.size() && 
            lower_filename.compare(lower_filename.size() - ext.size(), ext.size(), ext) == 0) {
            valid_extension = true;
            break;
        }
    }

    if (!valid_extension) {
        return "Invalid file extension (allowed: .bin, .img, .trx)";
    }

    return ""; // Valid
}

std::string ManualUploadHandler::generateUploadId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);

    std::ostringstream oss;
    oss << "manual_" << timestamp << "_" << dis(gen);
    return oss.str();
}

std::string FirmwareUpdate::ManualUploadHandler::calculateMD5(const std::vector<unsigned char>& data) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }

    if (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(ctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }

    EVP_MD_CTX_free(ctx);

    std::stringstream ss;
    for (unsigned int i = 0; i < hash_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string ManualUploadHandler::calculateSHA256(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }

    return oss.str();
}

bool ManualUploadHandler::saveUploadMetadata(const ManualUploadInfo& info) {
    try {
        json metadata;
        metadata["upload_id"] = info.upload_id;
        metadata["filename"] = info.filename;
        metadata["file_path"] = info.file_path;
        metadata["file_size"] = info.file_size;
        metadata["file_type"] = info.file_type;
        metadata["checksum_md5"] = info.checksum_md5;
        metadata["checksum_sha256"] = info.checksum_sha256;
        metadata["upload_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            info.upload_timestamp.time_since_epoch()).count();
        metadata["verify_signature"] = info.verify_signature;
        metadata["status"] = info.status;
        metadata["error_message"] = info.error_message;

        std::string metadata_path = getUploadDirectory(info.upload_id) + "/metadata.json";
        std::ofstream file(metadata_path);
        if (!file) {
            return false;
        }

        file << metadata.dump(2);
        file.close();

        std::cout << "[MANUAL-UPLOAD] Metadata saved: " << metadata_path << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Error saving metadata: " << e.what() << std::endl;
        return false;
    }
}

std::shared_ptr<ManualUploadInfo> ManualUploadHandler::loadUploadMetadata(const std::string& upload_id) {
    try {
        std::string metadata_path = getUploadDirectory(upload_id) + "/metadata.json";
        std::ifstream file(metadata_path);
        if (!file) {
            return nullptr;
        }

        json metadata;
        file >> metadata;
        file.close();

        auto info = std::make_shared<ManualUploadInfo>();
        info->upload_id = metadata.value("upload_id", "");
        info->filename = metadata.value("filename", "");
        info->file_path = metadata.value("file_path", "");
        info->file_size = metadata.value("file_size", 0);
        info->file_type = metadata.value("file_type", "");
        info->checksum_md5 = metadata.value("checksum_md5", "");
        info->checksum_sha256 = metadata.value("checksum_sha256", "");

        uint64_t timestamp_ms = metadata.value("upload_timestamp", 0);
        info->upload_timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms));

        info->verify_signature = metadata.value("verify_signature", false);
        info->status = metadata.value("status", "unknown");
        info->error_message = metadata.value("error_message", "");

        return info;

    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Error loading metadata for " << upload_id << ": " << e.what() << std::endl;
        return nullptr;
    }
}

bool ManualUploadHandler::createUploadDirectory(const std::string& upload_id) {
    try {
        std::string upload_dir = getUploadDirectory(upload_id);
        std::filesystem::create_directories(upload_dir);
        return true;
    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Error creating upload directory: " << e.what() << std::endl;
        return false;
    }
}

std::string ManualUploadHandler::getUploadDirectory(const std::string& upload_id) {
    return getManualUploadBaseDirectory() + "/" + upload_id;
}

std::string ManualUploadHandler::getManualUploadBaseDirectory() {
    return ConfigManager::getInstance().getDataPath("firmware/manual-upload");
}

void ManualUploadHandler::cleanupOldUploads() {
    const size_t MAX_UPLOADS = 3;
    
    try {
        std::string base_dir = getManualUploadBaseDirectory();
        
        // Get all upload directories with their timestamps
        std::vector<std::pair<std::chrono::system_clock::time_point, std::string>> uploads;
        
        for (const auto& entry : std::filesystem::directory_iterator(base_dir)) {
            if (entry.is_directory()) {
                std::string upload_id = entry.path().filename().string();
                auto upload_info = loadUploadMetadata(upload_id);
                if (upload_info) {
                    uploads.push_back({upload_info->upload_timestamp, upload_id});
                }
            }
        }
        
        // Sort by timestamp (newest first)
        std::sort(uploads.begin(), uploads.end(), 
                  [](const auto& a, const auto& b) {
                      return a.first > b.first;
                  });
        
        // Delete old uploads if we have more than MAX_UPLOADS
        if (uploads.size() > MAX_UPLOADS) {
            for (size_t i = MAX_UPLOADS; i < uploads.size(); ++i) {
                std::string upload_id = uploads[i].second;
                std::cout << "[MANUAL-UPLOAD] Cleaning up old upload: " << upload_id << std::endl;
                deleteUpload(upload_id);
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "[MANUAL-UPLOAD] Error during cleanup: " << e.what() << std::endl;
    }
}

} // namespace FirmwareUpdate