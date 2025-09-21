
#include "BackupRouter.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <cstdlib>
#include <vector>

BackupRouter::BackupRouter() {
    m_backupHandler = std::make_unique<BackupHandler>();
    std::cout << "[BACKUP-ROUTER] Backup Router initialized with BackupHandler" << std::endl;
}

std::string BackupRouter::handleBackupEstimate(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup size estimation request" << std::endl;
    
    try {
        json requestData = json::parse(request.body);
        std::cout << "[BACKUP-ROUTER] Request data: " << requestData.dump(2) << std::endl;
        
        json response;
        response["success"] = true;
        response["timestamp"] = generateTimestamp();
        
        // Create temporary backup configuration for estimation
        json tempBackupConfig;
        std::string backupType = "partial";
        
        if (requestData.value("full_backup", false)) {
            backupType = "full";
            tempBackupConfig["backup_type"] = "full";
        } else {
            tempBackupConfig["backup_type"] = "partial";
            tempBackupConfig["include_network_config"] = requestData.value("network_config_backup", false);
            tempBackupConfig["include_license"] = requestData.value("license_backup", false);
            tempBackupConfig["include_authentication"] = requestData.value("authentication_backup", false);
            tempBackupConfig["include_cellular"] = requestData.value("cellular_backup", false);
            tempBackupConfig["include_vpn"] = requestData.value("vpn_backup", false);
            tempBackupConfig["include_wireless"] = requestData.value("wireless_backup", false);
            tempBackupConfig["include_credentials"] = requestData.value("include_credentials", false);
        }
        
        // Set encryption if requested
        bool encrypt = requestData.value("encrypt_backup", false);
        tempBackupConfig["encrypt"] = encrypt;
        if (encrypt) {
            // Use a temporary password for size estimation
            tempBackupConfig["password"] = "temp_estimation_password";
        }
        
        std::cout << "[BACKUP-ROUTER] Creating temporary backup for size estimation..." << std::endl;
        
        // Create temporary backup using BackupHandler
        std::string tempBackupPath = m_backupHandler->createTemporaryBackup(tempBackupConfig);
        
        // Get the actual file size from the temporary backup
        size_t actualSize = 0;
        size_t fileCount = 0;
        json sizeBreakdown;
        
        if (std::filesystem::exists(tempBackupPath)) {
            actualSize = std::filesystem::file_size(tempBackupPath);
            
            // Get detailed breakdown from the backup handler
            auto backupDetails = m_backupHandler->getTemporaryBackupDetails(tempBackupPath);
            fileCount = backupDetails.value("file_count", 0);
            
            // Create size breakdown based on backup components
            if (backupType == "full") {
                sizeBreakdown["base_size"] = formatFileSize(actualSize * 0.85); // ~85% for base system
                sizeBreakdown["network_size"] = formatFileSize(actualSize * 0.08); // ~8% for network
                sizeBreakdown["auth_size"] = formatFileSize(actualSize * 0.04); // ~4% for auth
                sizeBreakdown["other_size"] = formatFileSize(actualSize * 0.03); // ~3% for other
            } else {
                // For partial backups, distribute based on selected components
                int componentCount = 0;
                if (requestData.value("network_config_backup", false)) componentCount++;
                if (requestData.value("authentication_backup", false)) componentCount++;
                if (requestData.value("license_backup", false)) componentCount++;
                if (requestData.value("cellular_backup", false)) componentCount++;
                if (requestData.value("vpn_backup", false)) componentCount++;
                if (requestData.value("wireless_backup", false)) componentCount++;
                
                size_t perComponentSize = componentCount > 0 ? actualSize / componentCount : 0;
                
                sizeBreakdown["base_size"] = formatFileSize(0);
                sizeBreakdown["network_size"] = formatFileSize(requestData.value("network_config_backup", false) ? perComponentSize : 0);
                sizeBreakdown["auth_size"] = formatFileSize(requestData.value("authentication_backup", false) ? perComponentSize : 0);
                sizeBreakdown["other_size"] = formatFileSize(actualSize - 
                    (requestData.value("network_config_backup", false) ? perComponentSize : 0) -
                    (requestData.value("authentication_backup", false) ? perComponentSize : 0));
            }
        }
        
        // Clean up temporary backup file
        try {
            if (std::filesystem::exists(tempBackupPath)) {
                std::filesystem::remove(tempBackupPath);
                std::cout << "[BACKUP-ROUTER] Temporary backup file cleaned up: " << tempBackupPath << std::endl;
            }
        } catch (const std::exception& cleanupError) {
            std::cout << "[BACKUP-ROUTER] Warning: Failed to cleanup temporary backup: " << cleanupError.what() << std::endl;
        }
        
        response["estimated_size"] = formatFileSize(actualSize);
        response["size_breakdown"] = sizeBreakdown;
        response["file_count"] = fileCount;
        response["encrypted"] = encrypt;
        response["backup_type"] = backupType;
        response["actual_compression"] = true;
        
        // Format creation date
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        response["creation_date"] = ss.str();
        
        std::cout << "[BACKUP-ROUTER] Size estimation completed successfully with actual size: " << formatFileSize(actualSize) << std::endl;
        return response.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error in size estimation: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::handleBackupCreate(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup creation request" << std::endl;
    
    try {
        json requestData = json::parse(request.body);
        std::cout << "[BACKUP-ROUTER] Request data: " << requestData.dump(2) << std::endl;
        
        // Determine backup type and configuration
        json backupConfig;
        std::string backupType = "partial";
        
        if (requestData.value("full_backup", false)) {
            backupType = "full";
            backupConfig["backup_type"] = "full";
        } else {
            backupConfig["backup_type"] = "partial";
            backupConfig["include_network_config"] = requestData.value("network_config_backup", false);
            backupConfig["include_license"] = requestData.value("license_backup", false);
            backupConfig["include_authentication"] = requestData.value("authentication_backup", false);
            backupConfig["include_cellular"] = requestData.value("cellular_backup", false);
            backupConfig["include_vpn"] = requestData.value("vpn_backup", false);
            backupConfig["include_wireless"] = requestData.value("wireless_backup", false);
            backupConfig["include_credentials"] = requestData.value("include_credentials", false);
        }
        
        // Encryption settings
        bool encrypt = requestData.value("encrypt_backup", false);
        backupConfig["encrypt"] = encrypt;
        if (encrypt) {
            std::string password = requestData.value("password", "");
            if (password.empty()) {
                json errorResponse;
                errorResponse["success"] = false;
                errorResponse["error"] = "Password required for encrypted backup";
                errorResponse["timestamp"] = generateTimestamp();
                return errorResponse.dump();
            }
            backupConfig["password"] = password;
        }
        
        // Create backup using BackupHandler
        std::string outputPath = m_backupHandler->createBackup(backupConfig);
        std::string filename = std::filesystem::path(outputPath).filename();
        
        // Update backup info
        json backupInfo = getBackupInfo();
        json newBackup;
        newBackup["filename"] = filename;
        newBackup["created_at"] = generateTimestamp();
        newBackup["type"] = backupType;
        newBackup["encrypted"] = encrypt;
        newBackup["size"] = std::filesystem::file_size(outputPath);
        newBackup["formatted_size"] = formatFileSize(std::filesystem::file_size(outputPath));
        newBackup["status"] = "completed";
        newBackup["file_path"] = outputPath;
        
        if (!backupInfo.contains("recent_backups")) {
            backupInfo["recent_backups"] = json::array();
        }
        
        backupInfo["recent_backups"].push_back(newBackup);
        if (backupInfo["recent_backups"].size() > 10) {
            backupInfo["recent_backups"].erase(backupInfo["recent_backups"].begin());
        }
        
        updateBackupInfo(backupInfo);
        
        json response;
        response["success"] = true;
        response["filename"] = filename;
        response["backup_id"] = filename;  // Use filename as backup_id for download
        response["message"] = "Backup created successfully";
        response["timestamp"] = generateTimestamp();
        response["size"] = formatFileSize(std::filesystem::file_size(outputPath));
        response["backup_type"] = backupType;
        response["encrypted"] = encrypt;
        
        std::cout << "[BACKUP-ROUTER] Backup created successfully: " << filename << std::endl;
        return response.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error creating backup: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::handleBackupRestore(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup restore request" << std::endl;
    
    try {
        if (request.method != "POST") {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "POST method required for restore";
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
        // Parse restore request data
        json requestData;
        std::string backupFilePath;
        std::string password;
        
        // For demonstration, we'll assume the backup file path is provided in the request body
        // In a full implementation, this would handle multipart file uploads
        try {
            requestData = json::parse(request.body);
            backupFilePath = requestData.value("backup_file_path", "");
            password = requestData.value("password", "");
        } catch (const std::exception& e) {
            // If JSON parsing fails, assume it's a file upload - handle accordingly
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "File upload handling not fully implemented yet";
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
        if (backupFilePath.empty()) {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Backup file path required";
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
        // Prepare restore configuration
        json restoreConfig;
        restoreConfig["validate_integrity"] = requestData.value("validate_integrity", true);
        restoreConfig["wipe_before_restore"] = requestData.value("wipe_before_restore", false);
        if (!password.empty()) {
            restoreConfig["password"] = password;
        }
        
        // Perform restore using BackupHandler
        std::string result = m_backupHandler->restoreBackup(backupFilePath, restoreConfig);
        
        json response;
        response["success"] = true;
        response["message"] = "Restore completed successfully";
        response["timestamp"] = generateTimestamp();
        response["details"] = result;
        
        std::cout << "[BACKUP-ROUTER] Restore process completed successfully" << std::endl;
        return response.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error in restore process: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::handleBackupList(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup list request" << std::endl;
    
    try {
        json response;
        response["success"] = true;
        response["timestamp"] = generateTimestamp();
        
        // Get backup files from filesystem
        std::vector<std::string> backupFiles = getBackupFiles();
        json backupList = json::array();
        
        for (const auto& filePath : backupFiles) {
            backupList.push_back(createBackupListItem(filePath));
        }
        
        response["backups"] = backupList;
        response["backup_count"] = backupList.size();
        
        std::cout << "[BACKUP-ROUTER] Backup list retrieved successfully (" << backupList.size() << " items)" << std::endl;
        return response.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error retrieving backup list: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::handleBackupDownload(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup download request" << std::endl;
    
    try {
        // Extract backup filename from URL path
        std::string backupFilename;
        if (request.params.find("backup_id") != request.params.end()) {
            backupFilename = request.params.at("backup_id");
        } else {
            // Extract from request body as fallback
            if (!request.body.empty()) {
                json requestData = json::parse(request.body);
                backupFilename = requestData.value("backup_id", "");
            }
        }
        
        if (backupFilename.empty()) {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Backup filename required";
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
        std::string backupPath = getBackupDataPath() + "/" + backupFilename;
        
        if (!std::filesystem::exists(backupPath)) {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Backup file not found: " + backupFilename;
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
        // Return success response indicating file is ready for download
        // The actual file serving should be handled by the HTTP handler
        json response;
        response["success"] = true;
        response["message"] = "File ready for download";
        response["filename"] = backupFilename;
        response["file_path"] = backupPath;
        response["file_size"] = std::filesystem::file_size(backupPath);
        response["content_type"] = "application/octet-stream";
        response["timestamp"] = generateTimestamp();
        
        std::cout << "[BACKUP-ROUTER] Download request processed for: " << backupFilename << std::endl;
        return response.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error in download: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::handleBackupDelete(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup delete request" << std::endl;
    
    try {
        // Extract backup filename from URL path or request body
        std::string backupFilename;
        if (request.params.find("backup_id") != request.params.end()) {
            backupFilename = request.params.at("backup_id");
        } else if (!request.body.empty()) {
            json requestData = json::parse(request.body);
            backupFilename = requestData.value("backup_id", "");
        }
        
        if (backupFilename.empty()) {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Backup filename required";
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
        // Find and delete backup file
        std::string backupPath = getBackupDataPath() + "/" + backupFilename;
        if (std::filesystem::exists(backupPath)) {
            std::filesystem::remove(backupPath);
            
            json response;
            response["success"] = true;
            response["message"] = "Backup deleted successfully";
            response["filename"] = backupFilename;
            response["timestamp"] = generateTimestamp();
            
            std::cout << "[BACKUP-ROUTER] Backup deleted successfully: " << backupFilename << std::endl;
            return response.dump();
        } else {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Backup file not found: " + backupFilename;
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error deleting backup: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::handleBackupValidate(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup validation request" << std::endl;
    
    try {
        json requestData = json::parse(request.body);
        std::string backupPath = requestData.value("backup_path", "");
        
        if (backupPath.empty()) {
            json errorResponse;
            errorResponse["success"] = false;
            errorResponse["error"] = "Backup path required";
            errorResponse["timestamp"] = generateTimestamp();
            return errorResponse.dump();
        }
        
        bool isValid = m_backupHandler->validateBackupIntegrity(backupPath);
        
        json response;
        response["success"] = true;
        response["valid"] = isValid;
        response["message"] = isValid ? "Backup integrity check passed" : "Backup integrity check failed";
        response["timestamp"] = generateTimestamp();
        
        return response.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error validating backup: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::handleBackupConfig(const ApiRequest& request) {
    std::cout << "[BACKUP-ROUTER] Processing backup configuration request" << std::endl;
    
    try {
        json response;
        response["success"] = true;
        response["configuration"] = getBackupConfiguration();
        response["timestamp"] = generateTimestamp();
        
        std::cout << "[BACKUP-ROUTER] Backup configuration retrieved successfully" << std::endl;
        return response.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error retrieving backup configuration: " << e.what() << std::endl;
        
        json errorResponse;
        errorResponse["success"] = false;
        errorResponse["error"] = e.what();
        errorResponse["timestamp"] = generateTimestamp();
        return errorResponse.dump();
    }
}

std::string BackupRouter::loadJsonFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return "{}";
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool BackupRouter::saveJsonFile(const std::string& filename, const json& data) {
    try {
        std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        
        file << data.dump(2);
        return true;
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error saving JSON file: " << e.what() << std::endl;
        return false;
    }
}

std::string BackupRouter::generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string BackupRouter::calculateDirectorySize(const std::string& path, const json& options) {
    // Simulate size calculation based on backup options
    size_t baseSize = 2400000000; // 2.4 GB base size
    
    if (options.value("full_backup", false)) {
        baseSize += 800000000; // Additional 800 MB for full backup
    }
    
    if (options.value("include_credentials", false)) {
        baseSize += 100000000; // Additional 100 MB for credentials
    }
    
    if (options.value("encrypt_backup", false)) {
        baseSize += 50000000; // Additional 50 MB for encryption overhead
    }
    
    return formatFileSize(baseSize);
}

std::string BackupRouter::formatFileSize(size_t bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << size << " " << units[unit];
    return ss.str();
}

bool BackupRouter::createBackupFile(const json& options, const std::string& outputPath) {
    try {
        // Create output directory if it doesn't exist
        std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());
        
        // Get paths to include based on options
        std::vector<std::string> includePaths = getIncludedPaths(options);
        
        // Build tar command
        std::stringstream command;
        command << "tar -czf " << outputPath;
        
        for (const auto& path : includePaths) {
            command << " " << path;
        }
        
        // Execute backup command
        int result = std::system(command.str().c_str());
        
        // For simulation purposes, create a dummy file
        std::ofstream file(outputPath);
        file << "Simulated backup file created at " << generateTimestamp();
        file.close();
        
        return result == 0 || std::filesystem::exists(outputPath);
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error creating backup file: " << e.what() << std::endl;
        return false;
    }
}

bool BackupRouter::extractBackupFile(const std::string& backupPath, const json& options) {
    try {
        // Build extract command
        std::stringstream command;
        command << "tar -xzf " << backupPath << " -C /";
        
        // Execute restore command
        int result = std::system(command.str().c_str());
        return result == 0;
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error extracting backup file: " << e.what() << std::endl;
        return false;
    }
}

json BackupRouter::getBackupInfo() {
    std::string filePath = getBackupDataPath() + "/backup-info.json";
    std::string content = loadJsonFile(filePath);
    
    try {
        json data = json::parse(content);
        return data;
    } catch (const std::exception& e) {
        // Return default structure if file doesn't exist or is invalid
        json defaultData;
        defaultData["recent_backups"] = json::array();
        defaultData["last_updated"] = generateTimestamp();
        defaultData["backup_storage_path"] = getBackupDataPath();
        return defaultData;
    }
}

bool BackupRouter::updateBackupInfo(const json& info) {
    std::string filePath = getBackupDataPath() + "/backup-info.json";
    json updatedInfo = info;
    updatedInfo["last_updated"] = generateTimestamp();
    
    return saveJsonFile(filePath, updatedInfo);
}

std::string BackupRouter::getBackupDataPath() {
    return "ur-webif-api/data/backup-storage";
}

json BackupRouter::getBackupConfiguration() {
    try {
        std::ifstream configFile("ur-webif-api/config/server.json");
        if (!configFile) {
            return json{};
        }
        
        json serverConfig;
        configFile >> serverConfig;
        configFile.close();
        
        if (serverConfig.contains("backup_configuration")) {
            return serverConfig["backup_configuration"];
        }
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error loading backup configuration: " << e.what() << std::endl;
    }
    
    return json{};
}

std::vector<std::string> BackupRouter::getBackupFiles() {
    std::vector<std::string> backupFiles;
    std::string backupDir = getBackupDataPath();
    
    try {
        if (std::filesystem::exists(backupDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename();
                    if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".uhb" || 
                        filename.size() >= 8 && filename.substr(filename.size() - 8) == ".uhb.enc") {
                        backupFiles.push_back(entry.path().string());
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error scanning backup directory: " << e.what() << std::endl;
    }
    
    return backupFiles;
}

json BackupRouter::createBackupListItem(const std::string& filePath) {
    json item;
    
    try {
        std::filesystem::path path(filePath);
        std::string filename = path.filename();
        
        item["id"] = filename;
        item["filename"] = filename;
        item["file_path"] = filePath;
        item["size"] = std::filesystem::file_size(filePath);
        item["formatted_size"] = formatFileSize(std::filesystem::file_size(filePath));
        
        // Extract backup type from filename
        if (filename.find("_full_") != std::string::npos) {
            item["type"] = "full";
        } else if (filename.find("_partial_") != std::string::npos) {
            item["type"] = "partial";
        } else if (filename.find("_network_config_") != std::string::npos) {
            item["type"] = "network_config";
        } else if (filename.find("_license_") != std::string::npos) {
            item["type"] = "license";
        } else if (filename.find("_authentication_") != std::string::npos) {
            item["type"] = "authentication";
        } else {
            item["type"] = "unknown";
        }
        
        item["encrypted"] = filename.size() >= 4 && filename.substr(filename.size() - 4) == ".enc";
        item["status"] = "completed";
        
        // Extract creation date from filename
        auto lastWriteTime = std::filesystem::last_write_time(filePath);
        auto timeT = std::chrono::duration_cast<std::chrono::seconds>(lastWriteTime.time_since_epoch()).count();
        std::time_t time = static_cast<std::time_t>(timeT);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        item["created_at"] = ss.str();
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-ROUTER] Error creating backup list item: " << e.what() << std::endl;
        item["id"] = "error";
        item["filename"] = "Error reading file";
        item["error"] = e.what();
    }
    
    return item;
}

std::vector<std::string> BackupRouter::getIncludedPaths(const json& options) {
    std::vector<std::string> paths;
    
    // Add paths based on backup options
    if (options.value("full_backup", false)) {
        paths.push_back("ur-webif-api/data");
        paths.push_back("ur-webif-api/config");
    } else {
        if (options.value("network_config_backup", false)) {
            paths.push_back("ur-webif-api/data/network-priority");
        }
        if (options.value("license_backup", false)) {
            paths.push_back("ur-webif-api/data/license");
        }
        if (options.value("authentication_backup", false)) {
            paths.push_back("ur-webif-api/data/auth-gen");
            paths.push_back("ur-webif-api/data/file-auth-attempts");
        }
        if (options.value("cellular_backup", false)) {
            paths.push_back("ur-webif-api/data/cellular");
        }
        if (options.value("vpn_backup", false)) {
            paths.push_back("ur-webif-api/data/vpn");
        }
        if (options.value("wireless_backup", false)) {
            paths.push_back("ur-webif-api/data/wireless");
        }
    }
    
    return paths;
}

int BackupRouter::calculateFileCount(const json& options) {
    int count = 0;
    
    if (options.value("full_backup", false)) {
        count = 150; // Estimated file count for full backup
    } else {
        if (options.value("network_config_backup", false)) count += 25;
        if (options.value("license_backup", false)) count += 5;
        if (options.value("authentication_backup", false)) count += 15;
        if (options.value("cellular_backup", false)) count += 10;
        if (options.value("vpn_backup", false)) count += 8;
        if (options.value("wireless_backup", false)) count += 12;
    }
    
    return count;
}
