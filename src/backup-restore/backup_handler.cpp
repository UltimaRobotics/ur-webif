
#include "backup_handler.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <zlib.h>

BackupHandler::BackupHandler() {
    // Load configuration from server.json
    loadServerConfig();
    
    // Set default paths if not configured
    if (m_dataPath.empty()) {
        m_dataPath = "ur-webif-api/data";
    }
    if (m_backupPath.empty()) {
        m_backupPath = "ur-webif-api/data/backup-storage";
    }
    if (m_tempPath.empty()) {
        m_tempPath = "ur-webif-api/data/backup-temp";
    }
    
    // Ensure backup directories exist
    std::filesystem::create_directories(m_backupPath);
    std::filesystem::create_directories(m_backupPath + "/temp");
    std::filesystem::create_directories(m_tempPath);
    
    std::cout << "[BACKUP-HANDLER] Initialized with data path: " << m_dataPath << std::endl;
    std::cout << "[BACKUP-HANDLER] Backup storage path: " << m_backupPath << std::endl;
    std::cout << "[BACKUP-HANDLER] Temp path: " << m_tempPath << std::endl;
}

BackupHandler::~BackupHandler() {
    // Cleanup temporary files
    try {
        std::filesystem::remove_all(m_backupPath + "/temp");
        std::filesystem::remove_all("ur-webif-api/data/backup-temp");
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-HANDLER] Warning: Failed to cleanup temp files: " << e.what() << std::endl;
    }
}

std::string BackupHandler::createBackup(const json& backupConfig) {
    std::cout << "[BACKUP-HANDLER] Creating backup with config: " << backupConfig.dump(2) << std::endl;
    
    try {
        std::string backupType = backupConfig.value("backup_type", "full");
        bool encrypt = backupConfig.value("encrypt", false);
        std::string password = backupConfig.value("password", "");
        
        std::string outputPath;
        
        if (backupType == "full") {
            outputPath = createFullBackup(backupConfig);
        } else if (backupType == "partial") {
            outputPath = createPartialBackup(backupConfig);
        } else if (backupType == "network_config") {
            outputPath = createNetworkConfigBackup(backupConfig);
        } else if (backupType == "license") {
            outputPath = createLicenseBackup(backupConfig);
        } else if (backupType == "authentication") {
            outputPath = createAuthenticationBackup(backupConfig);
        } else {
            throw std::runtime_error("Unknown backup type: " + backupType);
        }
        
        // Encrypt if requested
        if (encrypt && !password.empty()) {
            std::cout << "[BACKUP-HANDLER] Encrypting backup..." << std::endl;
            outputPath = encryptBackup(outputPath, password);
        }
        
        std::cout << "[BACKUP-HANDLER] Backup created successfully: " << outputPath << std::endl;
        return outputPath;
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-HANDLER] Error creating backup: " << e.what() << std::endl;
        throw;
    }
}

std::string BackupHandler::createTemporaryBackup(const json& backupConfig) {
    std::cout << "[BACKUP-HANDLER] Creating temporary backup for size estimation..." << std::endl;
    
    try {
        std::string backupType = backupConfig.value("backup_type", "full");
        bool encrypt = backupConfig.value("encrypt", false);
        std::string password = backupConfig.value("password", "");
        
        // Generate temporary backup filename using configured temp path
        std::filesystem::create_directories(m_tempPath);
        
        std::string timestamp = generateTimestamp();
        std::string tempFileName = "temp_backup_" + backupType + "_" + timestamp + ".uhb";
        std::string tempOutputPath = m_tempPath + "/" + tempFileName;
        
        std::string outputPath;
        
        if (backupType == "full") {
            outputPath = createTemporaryFullBackup(backupConfig, tempOutputPath);
        } else if (backupType == "partial") {
            outputPath = createTemporaryPartialBackup(backupConfig, tempOutputPath);
        } else {
            throw std::runtime_error("Unknown backup type for temporary backup: " + backupType);
        }
        
        // Encrypt if requested
        if (encrypt && !password.empty()) {
            std::cout << "[BACKUP-HANDLER] Encrypting temporary backup..." << std::endl;
            outputPath = encryptBackup(outputPath, password);
        }
        
        std::cout << "[BACKUP-HANDLER] Temporary backup created successfully: " << outputPath << std::endl;
        return outputPath;
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-HANDLER] Error creating temporary backup: " << e.what() << std::endl;
        throw;
    }
}

std::string BackupHandler::createTemporaryFullBackup(const json& config, const std::string& outputPath) {
    std::cout << "[BACKUP-HANDLER] Creating temporary full backup..." << std::endl;
    
    std::vector<std::string> files = getBackupFiles("full");
    return compressFiles(files, outputPath);
}

std::string BackupHandler::createTemporaryPartialBackup(const json& config, const std::string& outputPath) {
    std::cout << "[BACKUP-HANDLER] Creating temporary partial backup..." << std::endl;
    
    std::vector<std::string> files;
    
    // Check what components are selected
    if (config.value("include_network_config", false)) {
        auto networkFiles = getFilesByCategory("network_config");
        files.insert(files.end(), networkFiles.begin(), networkFiles.end());
    }
    
    if (config.value("include_license", false)) {
        auto licenseFiles = getFilesByCategory("license");
        files.insert(files.end(), licenseFiles.begin(), licenseFiles.end());
    }
    
    if (config.value("include_authentication", false)) {
        auto authFiles = getFilesByCategory("authentication");
        files.insert(files.end(), authFiles.begin(), authFiles.end());
    }
    
    if (config.value("include_cellular", false)) {
        auto cellularFiles = getFilesByCategory("cellular");
        files.insert(files.end(), cellularFiles.begin(), cellularFiles.end());
    }
    
    if (config.value("include_vpn", false)) {
        auto vpnFiles = getFilesByCategory("vpn");
        files.insert(files.end(), vpnFiles.begin(), vpnFiles.end());
    }
    
    if (config.value("include_wireless", false)) {
        auto wirelessFiles = getFilesByCategory("wireless");
        files.insert(files.end(), wirelessFiles.begin(), wirelessFiles.end());
    }
    
    return compressFiles(files, outputPath);
}

json BackupHandler::getTemporaryBackupDetails(const std::string& tempBackupPath) {
    std::cout << "[BACKUP-HANDLER] Extracting details from temporary backup..." << std::endl;
    
    json details;
    
    try {
        if (!std::filesystem::exists(tempBackupPath)) {
            details["file_count"] = 0;
            details["compressed_size"] = 0;
            details["error"] = "Temporary backup file not found";
            return details;
        }
        
        // Read metadata from .uhb file
        std::ifstream uhbFile(tempBackupPath, std::ios::binary);
        if (!uhbFile) {
            details["file_count"] = 0;
            details["compressed_size"] = 0;
            details["error"] = "Failed to open temporary backup file";
            return details;
        }
        
        // Read metadata header (first 1024 bytes)
        std::vector<uint8_t> header(1024);
        uhbFile.read(reinterpret_cast<char*>(header.data()), 1024);
        uhbFile.close();
        
        // Parse metadata
        std::string metadataStr(header.begin(), header.end());
        // Remove null bytes
        metadataStr.erase(std::find(metadataStr.begin(), metadataStr.end(), '\0'), metadataStr.end());
        
        json metadata = json::parse(metadataStr);
        
        details["file_count"] = metadata.value("file_count", 0);
        details["compressed_size"] = metadata.value("compressed_size", 0);
        details["version"] = metadata.value("version", "1.0");
        details["created"] = metadata.value("created", "");
        details["sha256"] = metadata.value("sha256", "");
        
        std::cout << "[BACKUP-HANDLER] Temporary backup details extracted successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-HANDLER] Error extracting temporary backup details: " << e.what() << std::endl;
        details["file_count"] = 0;
        details["compressed_size"] = 0;
        details["error"] = e.what();
    }
    
    return details;
}

std::string BackupHandler::createFullBackup(const json& config) {
    std::cout << "[BACKUP-HANDLER] Creating full backup..." << std::endl;
    
    std::vector<std::string> files = getBackupFiles("full");
    std::string outputPath = getBackupOutputPath("full", config.value("encrypt", false));
    
    return compressFiles(files, outputPath);
}

std::string BackupHandler::createPartialBackup(const json& config) {
    std::cout << "[BACKUP-HANDLER] Creating partial backup..." << std::endl;
    
    std::vector<std::string> files;
    
    // Check what components are selected
    if (config.value("include_network_config", false)) {
        auto networkFiles = getFilesByCategory("network_config");
        files.insert(files.end(), networkFiles.begin(), networkFiles.end());
    }
    
    if (config.value("include_license", false)) {
        auto licenseFiles = getFilesByCategory("license");
        files.insert(files.end(), licenseFiles.begin(), licenseFiles.end());
    }
    
    if (config.value("include_authentication", false)) {
        auto authFiles = getFilesByCategory("authentication");
        files.insert(files.end(), authFiles.begin(), authFiles.end());
    }
    
    if (config.value("include_cellular", false)) {
        auto cellularFiles = getFilesByCategory("cellular");
        files.insert(files.end(), cellularFiles.begin(), cellularFiles.end());
    }
    
    if (config.value("include_vpn", false)) {
        auto vpnFiles = getFilesByCategory("vpn");
        files.insert(files.end(), vpnFiles.begin(), vpnFiles.end());
    }
    
    if (config.value("include_wireless", false)) {
        auto wirelessFiles = getFilesByCategory("wireless");
        files.insert(files.end(), wirelessFiles.begin(), wirelessFiles.end());
    }
    
    std::string outputPath = getBackupOutputPath("partial", config.value("encrypt", false));
    return compressFiles(files, outputPath);
}

std::string BackupHandler::createNetworkConfigBackup(const json& config) {
    std::cout << "[BACKUP-HANDLER] Creating network config backup..." << std::endl;
    
    std::vector<std::string> files = getFilesByCategory("network_config");
    std::string outputPath = getBackupOutputPath("network_config", config.value("encrypt", false));
    
    return compressFiles(files, outputPath);
}

std::string BackupHandler::createLicenseBackup(const json& config) {
    std::cout << "[BACKUP-HANDLER] Creating license backup..." << std::endl;
    
    std::vector<std::string> files = getFilesByCategory("license");
    std::string outputPath = getBackupOutputPath("license", config.value("encrypt", false));
    
    return compressFiles(files, outputPath);
}

std::string BackupHandler::createAuthenticationBackup(const json& config) {
    std::cout << "[BACKUP-HANDLER] Creating authentication backup..." << std::endl;
    
    std::vector<std::string> files = getFilesByCategory("authentication");
    std::string outputPath = getBackupOutputPath("authentication", config.value("encrypt", false));
    
    return compressFiles(files, outputPath);
}

std::vector<std::string> BackupHandler::getBackupFiles(const std::string& backupType) {
    json config = loadBackupConfig();
    std::vector<std::string> files;
    
    if (backupType == "full") {
        // Include all data files for full backup
        for (const auto& category : config["backup_categories"]) {
            auto categoryFiles = getFilesByCategory(category["name"]);
            files.insert(files.end(), categoryFiles.begin(), categoryFiles.end());
        }
    }
    
    return files;
}

std::vector<std::string> BackupHandler::getFilesByCategory(const std::string& category) {
    std::vector<std::string> files;
    
    for (const auto& cat : m_backupConfig["backup_categories"]) {
        if (cat["name"] == category) {
            for (const std::string& pattern : cat["files"]) {
                std::string fullPath = pattern;
                
                // If pattern is relative, make it relative to working directory
                if (!pattern.starts_with("/") && !pattern.starts_with("./")) {
                    fullPath = "ur-webif-api/" + pattern;
                }
                
                // Handle wildcard patterns
                if (pattern.find("*") != std::string::npos) {
                    std::filesystem::path dir = std::filesystem::path(fullPath).parent_path();
                    std::string filename = std::filesystem::path(fullPath).filename();
                    
                    if (std::filesystem::exists(dir)) {
                        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                            if (entry.is_regular_file()) {
                                std::string name = entry.path().filename();
                                // Simple wildcard matching
                                if (filename == "*" || name.find(filename.substr(0, filename.find("*"))) == 0) {
                                    files.push_back(entry.path().string());
                                }
                            }
                        }
                    }
                } else {
                    if (std::filesystem::exists(fullPath)) {
                        files.push_back(fullPath);
                    }
                }
            }
            break;
        }
    }
    
    return files;
}

std::string BackupHandler::compressFiles(const std::vector<std::string>& files, const std::string& outputPath) {
    std::cout << "[BACKUP-HANDLER] Compressing " << files.size() << " files to: " << outputPath << std::endl;
    
    // Ensure temp directory exists
    std::filesystem::create_directories(m_backupPath + "/temp");
    
    // Create a temporary tar archive first
    std::string tempTarPath = m_backupPath + "/temp/backup_temp.tar";
    std::string tempGzPath = tempTarPath + ".gz";
    
    // Build tar command with relative paths
    std::stringstream tarCmd;
    tarCmd << "tar -cf " << tempTarPath;
    
    for (const auto& file : files) {
        if (std::filesystem::exists(file)) {
            // Use relative path from current directory
            std::string relativePath = file;
            if (relativePath.starts_with("/")) {
                relativePath = relativePath.substr(1); // Remove leading slash
            }
            tarCmd << " " << relativePath;
        }
    }
    
    // Log the tar command for debugging
    std::cout << "[BACKUP-HANDLER] Executing tar command: " << tarCmd.str() << std::endl;
    
    // Execute tar command
    int result = std::system(tarCmd.str().c_str());
    if (result != 0) {
        std::cout << "[BACKUP-HANDLER] Tar command failed with exit code: " << result << std::endl;
        std::cout << "[BACKUP-HANDLER] Command was: " << tarCmd.str() << std::endl;
        throw std::runtime_error("Failed to create tar archive (exit code: " + std::to_string(result) + ")");
    }
    
    // Compress with gzip
    std::stringstream gzipCmd;
    gzipCmd << "gzip -c " << tempTarPath << " > " << tempGzPath;
    result = std::system(gzipCmd.str().c_str());
    if (result != 0) {
        throw std::runtime_error("Failed to compress archive");
    }
    
    // Calculate SHA-256 hash for integrity
    std::string hash = calculateSHA256(tempGzPath);
    
    // Create final .uhb file with metadata
    json metadata;
    metadata["version"] = "1.0";
    metadata["created"] = generateTimestamp();
    metadata["file_count"] = files.size();
    metadata["sha256"] = hash;
    metadata["compressed_size"] = std::filesystem::file_size(tempGzPath);
    
    // Write metadata and compressed data to .uhb file
    std::ofstream uhbFile(outputPath, std::ios::binary);
    if (!uhbFile) {
        throw std::runtime_error("Failed to create output file: " + outputPath);
    }
    
    // Write metadata header (first 1024 bytes)
    std::string metadataStr = metadata.dump();
    std::vector<uint8_t> header(1024, 0);
    std::copy(metadataStr.begin(), metadataStr.end(), header.begin());
    uhbFile.write(reinterpret_cast<const char*>(header.data()), 1024);
    
    // Append compressed data
    std::ifstream gzFile(tempGzPath, std::ios::binary);
    uhbFile << gzFile.rdbuf();
    
    uhbFile.close();
    gzFile.close();
    
    // Cleanup temp files
    std::filesystem::remove(tempTarPath);
    std::filesystem::remove(tempGzPath);
    
    return outputPath;
}

std::string BackupHandler::encryptBackup(const std::string& filePath, const std::string& password) {
    std::cout << "[BACKUP-HANDLER] Encrypting backup file..." << std::endl;
    
    // Read the original file
    auto data = readFileToBytes(filePath);
    
    // Generate random salt and IV
    unsigned char salt[16];
    unsigned char iv[16];
    RAND_bytes(salt, sizeof(salt));
    RAND_bytes(iv, sizeof(iv));
    
    // Derive key from password using PBKDF2
    unsigned char key[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(), salt, sizeof(salt), 10000, EVP_sha256(), sizeof(key), key);
    
    // Encrypt data
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv);
    
    std::vector<uint8_t> encryptedData;
    encryptedData.resize(data.size() + AES_BLOCK_SIZE);
    
    int len;
    int encryptedLen = 0;
    
    EVP_EncryptUpdate(ctx, encryptedData.data(), &len, data.data(), data.size());
    encryptedLen += len;
    
    EVP_EncryptFinal_ex(ctx, encryptedData.data() + len, &len);
    encryptedLen += len;
    
    EVP_CIPHER_CTX_free(ctx);
    
    encryptedData.resize(encryptedLen);
    
    // Create encrypted file path
    std::string encryptedPath = filePath + ".enc";
    
    // Write encrypted file with salt and IV prepended
    std::ofstream encFile(encryptedPath, std::ios::binary);
    encFile.write(reinterpret_cast<const char*>(salt), sizeof(salt));
    encFile.write(reinterpret_cast<const char*>(iv), sizeof(iv));
    encFile.write(reinterpret_cast<const char*>(encryptedData.data()), encryptedData.size());
    encFile.close();
    
    // Remove original file
    std::filesystem::remove(filePath);
    
    return encryptedPath;
}

std::string BackupHandler::decryptBackup(const std::string& filePath, const std::string& password) {
    std::cout << "[BACKUP-HANDLER] Decrypting backup file..." << std::endl;
    
    // Read encrypted file
    std::ifstream encFile(filePath, std::ios::binary);
    if (!encFile) {
        throw std::runtime_error("Failed to open encrypted file");
    }
    
    // Read salt and IV
    unsigned char salt[16];
    unsigned char iv[16];
    encFile.read(reinterpret_cast<char*>(salt), sizeof(salt));
    encFile.read(reinterpret_cast<char*>(iv), sizeof(iv));
    
    // Read encrypted data
    std::vector<uint8_t> encryptedData((std::istreambuf_iterator<char>(encFile)), std::istreambuf_iterator<char>());
    encFile.close();
    
    // Derive key from password
    unsigned char key[32];
    PKCS5_PBKDF2_HMAC(password.c_str(), password.length(), salt, sizeof(salt), 10000, EVP_sha256(), sizeof(key), key);
    
    // Decrypt data
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key, iv);
    
    std::vector<uint8_t> decryptedData;
    decryptedData.resize(encryptedData.size());
    
    int len;
    int decryptedLen = 0;
    
    EVP_DecryptUpdate(ctx, decryptedData.data(), &len, encryptedData.data(), encryptedData.size());
    decryptedLen += len;
    
    EVP_DecryptFinal_ex(ctx, decryptedData.data() + len, &len);
    decryptedLen += len;
    
    EVP_CIPHER_CTX_free(ctx);
    
    decryptedData.resize(decryptedLen);
    
    // Create decrypted file path
    std::string decryptedPath = filePath.substr(0, filePath.length() - 4); // Remove .enc
    
    // Write decrypted data
    writeFileFromBytes(decryptedPath, decryptedData);
    
    return decryptedPath;
}

std::string BackupHandler::calculateSHA256(const std::string& filePath) {
    auto data = readFileToBytes(filePath);
    return calculateSHA256FromData(data);
}

std::string BackupHandler::calculateSHA256FromData(const std::vector<uint8_t>& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

bool BackupHandler::validateBackupIntegrity(const std::string& backupFilePath) {
    std::cout << "[BACKUP-HANDLER] Validating backup integrity..." << std::endl;
    
    try {
        // Read metadata from .uhb file
        std::ifstream uhbFile(backupFilePath, std::ios::binary);
        if (!uhbFile) {
            return false;
        }
        
        // Read metadata header
        std::vector<uint8_t> header(1024);
        uhbFile.read(reinterpret_cast<char*>(header.data()), 1024);
        
        // Parse metadata
        std::string metadataStr(header.begin(), header.end());
        // Remove null bytes
        metadataStr.erase(std::find(metadataStr.begin(), metadataStr.end(), '\0'), metadataStr.end());
        
        json metadata = json::parse(metadataStr);
        std::string expectedHash = metadata["sha256"];
        
        // Read compressed data and calculate hash
        std::vector<uint8_t> compressedData((std::istreambuf_iterator<char>(uhbFile)), std::istreambuf_iterator<char>());
        uhbFile.close();
        
        std::string actualHash = calculateSHA256FromData(compressedData);
        
        bool valid = (actualHash == expectedHash);
        std::cout << "[BACKUP-HANDLER] Integrity check: " << (valid ? "PASSED" : "FAILED") << std::endl;
        
        return valid;
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-HANDLER] Error validating integrity: " << e.what() << std::endl;
        return false;
    }
}

std::string BackupHandler::restoreBackup(const std::string& backupFilePath, const json& restoreConfig) {
    std::cout << "[BACKUP-HANDLER] Restoring backup from: " << backupFilePath << std::endl;
    
    try {
        std::string workingFile = backupFilePath;
        
        // Check if backup is encrypted
        if (workingFile.ends_with(".enc")) {
            std::string password = restoreConfig.value("password", "");
            if (password.empty()) {
                throw std::runtime_error("Password required for encrypted backup");
            }
            workingFile = decryptBackup(workingFile, password);
        }
        
        // Validate integrity if requested
        if (restoreConfig.value("validate_integrity", true)) {
            if (!validateBackupIntegrity(workingFile)) {
                throw std::runtime_error("Backup integrity check failed");
            }
        }
        
        // Extract compressed data from .uhb file
        std::ifstream uhbFile(workingFile, std::ios::binary);
        uhbFile.seekg(1024); // Skip metadata header
        
        std::string tempGzPath = m_backupPath + "/temp/restore_temp.tar.gz";
        std::ofstream gzFile(tempGzPath, std::ios::binary);
        gzFile << uhbFile.rdbuf();
        gzFile.close();
        uhbFile.close();
        
        // Extract tar.gz
        std::string tempTarPath = m_backupPath + "/temp/restore_temp.tar";
        std::stringstream gunzipCmd;
        gunzipCmd << "gunzip -c " << tempGzPath << " > " << tempTarPath;
        int result = std::system(gunzipCmd.str().c_str());
        if (result != 0) {
            throw std::runtime_error("Failed to decompress backup");
        }
        
        // Extract tar archive
        std::stringstream extractCmd;
        extractCmd << "tar -xf " << tempTarPath << " -C /";
        result = std::system(extractCmd.str().c_str());
        if (result != 0) {
            throw std::runtime_error("Failed to extract backup");
        }
        
        // Cleanup temp files
        std::filesystem::remove(tempGzPath);
        std::filesystem::remove(tempTarPath);
        
        std::cout << "[BACKUP-HANDLER] Restore completed successfully" << std::endl;
        return "Restore completed successfully";
        
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-HANDLER] Error during restore: " << e.what() << std::endl;
        throw;
    }
}

json BackupHandler::loadBackupConfig() {
    return m_backupConfig;
}

void BackupHandler::loadServerConfig() {
    try {
        std::ifstream configFile("ur-webif-api/config/server.json");
        if (!configFile) {
            std::cout << "[BACKUP-HANDLER] Warning: Could not open server.json, using defaults" << std::endl;
            setDefaultConfig();
            return;
        }
        
        json serverConfig;
        configFile >> serverConfig;
        configFile.close();
        
        if (serverConfig.contains("backup_configuration")) {
            json backupConfig = serverConfig["backup_configuration"];
            
            // Load paths
            m_dataPath = backupConfig.value("data_directory", "ur-webif-api/data");
            m_backupPath = backupConfig.value("storage_directory", "ur-webif-api/data/backup-storage");
            m_tempPath = backupConfig.value("temp_directory", "ur-webif-api/data/backup-temp");
            
            // Load backup settings
            m_maxBackupCount = backupConfig.value("max_backup_count", 10);
            m_maxBackupAgeDays = backupConfig.value("max_backup_age_days", 30);
            m_autoCleanup = backupConfig.value("auto_cleanup", true);
            m_compressionLevel = backupConfig.value("compression_level", 6);
            
            // Store the entire backup configuration
            m_backupConfig = backupConfig;
            
            std::cout << "[BACKUP-HANDLER] Loaded backup configuration from server.json" << std::endl;
        } else {
            std::cout << "[BACKUP-HANDLER] No backup_configuration found in server.json, using defaults" << std::endl;
            setDefaultConfig();
        }
    } catch (const std::exception& e) {
        std::cout << "[BACKUP-HANDLER] Error loading server config: " << e.what() << std::endl;
        setDefaultConfig();
    }
}

void BackupHandler::setDefaultConfig() {
    m_dataPath = "ur-webif-api/data";
    m_backupPath = "ur-webif-api/data/backup-storage";
    m_tempPath = "ur-webif-api/data/backup-temp";
    m_maxBackupCount = 10;
    m_maxBackupAgeDays = 30;
    m_autoCleanup = true;
    m_compressionLevel = 6;
    
    // Set default backup configuration
    m_backupConfig["backup_categories"] = json::array({
        {
            {"name", "network_config"},
            {"description", "Network configuration files"},
            {"files", {"data/network-priority/*.json"}}
        },
        {
            {"name", "license"},
            {"description", "License information"},
            {"files", {"data/license/*.json"}}
        },
        {
            {"name", "authentication"},
            {"description", "Authentication data"},
            {"files", {"data/auth-gen/*.uacc", "data/file-auth-attempts/*"}},
            {"sensitive", true}
        },
        {
            {"name", "cellular"},
            {"description", "Cellular settings"},
            {"files", {"data/cellular/*.json"}}
        },
        {
            {"name", "vpn"},
            {"description", "VPN configurations"},
            {"files", {"data/vpn/*.json"}}
        },
        {
            {"name", "wireless"},
            {"description", "Wireless settings"},
            {"files", {"data/wireless/*.json"}}
        }
    });
    
    m_backupConfig["backup_types"] = {
        {"full", {
            {"name", "Full System Backup"},
            {"includes_all_categories", true},
            {"default_encryption", false}
        }},
        {"partial", {
            {"name", "Partial Backup"},
            {"includes_all_categories", false},
            {"default_encryption", false}
        }}
    };
}

std::vector<uint8_t> BackupHandler::readFileToBytes(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to read file: " + filePath);
    }
    
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

bool BackupHandler::writeFileFromBytes(const std::string& filePath, const std::vector<uint8_t>& data) {
    // Ensure directory exists
    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
    
    std::ofstream file(filePath, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

std::string BackupHandler::generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d_%H-%M-%S");
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string BackupHandler::getBackupOutputPath(const std::string& backupType, bool encrypted) {
    std::string timestamp = generateTimestamp();
    std::string filename = "backup_" + backupType + "_" + timestamp + ".uhb";
    
    return m_backupPath + "/" + filename;
}

std::string BackupHandler::calculateDirectorySize(const std::vector<std::string>& files) {
    size_t totalSize = 0;
    
    for (const auto& file : files) {
        if (std::filesystem::exists(file)) {
            totalSize += std::filesystem::file_size(file);
        }
    }
    
    return formatFileSize(totalSize);
}

std::string BackupHandler::formatFileSize(size_t bytes) {
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
