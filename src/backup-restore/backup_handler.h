
#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class BackupHandler {
public:
    BackupHandler();
    ~BackupHandler();

    // Main backup operations
    std::string createBackup(const json& backupConfig);
    std::string createTemporaryBackup(const json& backupConfig);
    json getTemporaryBackupDetails(const std::string& tempBackupPath);
    std::string restoreBackup(const std::string& backupFilePath, const json& restoreConfig);
    
    // Backup type handlers
    std::string createFullBackup(const json& config);
    std::string createPartialBackup(const json& config);
    std::string createNetworkConfigBackup(const json& config);
    std::string createLicenseBackup(const json& config);
    std::string createAuthenticationBackup(const json& config);
    
    // Utility functions
    std::vector<std::string> getBackupFiles(const std::string& backupType);
    std::string calculateDirectorySize(const std::vector<std::string>& files);
    bool validateBackupIntegrity(const std::string& backupFilePath);
    
private:
    std::string m_dataPath;
    std::string m_backupPath;
    std::string m_tempPath;
    json m_backupConfig;
    int m_maxBackupCount;
    int m_maxBackupAgeDays;
    bool m_autoCleanup;
    int m_compressionLevel;
    
    // Core operations
    std::string compressFiles(const std::vector<std::string>& files, const std::string& outputPath);
    std::string encryptBackup(const std::string& filePath, const std::string& password);
    std::string decryptBackup(const std::string& filePath, const std::string& password);
    
    // Integrity checking
    std::string calculateSHA256(const std::string& filePath);
    std::string calculateSHA256FromData(const std::vector<uint8_t>& data);
    bool verifyIntegrity(const std::string& filePath, const std::string& expectedHash);
    
    // File operations
    std::vector<uint8_t> readFileToBytes(const std::string& filePath);
    bool writeFileFromBytes(const std::string& filePath, const std::vector<uint8_t>& data);
    bool copyFile(const std::string& source, const std::string& destination);
    std::string generateTimestamp();
    std::string formatFileSize(size_t bytes);
    
    // Configuration helpers
    json loadBackupConfig();
    void loadServerConfig();
    void setDefaultConfig();
    std::vector<std::string> getFilesByCategory(const std::string& category);
    std::string getBackupOutputPath(const std::string& backupType, bool encrypted);
    
    // Temporary backup helpers
    std::string createTemporaryFullBackup(const json& config, const std::string& outputPath);
    std::string createTemporaryPartialBackup(const json& config, const std::string& outputPath);
};
