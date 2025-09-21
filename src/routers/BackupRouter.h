
#pragma once

#include "api_request.h"
#include "../backup-restore/backup_handler.h"
#include <string>
#include <nlohmann/json.hpp>
#include <memory>

using json = nlohmann::json;

class BackupRouter {
public:
    BackupRouter();
    
    // Main route handlers
    std::string handleBackupEstimate(const ApiRequest& request);
    std::string handleBackupCreate(const ApiRequest& request);
    std::string handleBackupRestore(const ApiRequest& request);
    std::string handleBackupList(const ApiRequest& request);
    std::string handleBackupDownload(const ApiRequest& request);
    std::string handleBackupDelete(const ApiRequest& request);
    std::string handleBackupValidate(const ApiRequest& request);
    std::string handleBackupConfig(const ApiRequest& request);
    
// Public method for accessing backup configuration
    json getBackupConfiguration();

private:
    std::unique_ptr<BackupHandler> m_backupHandler;
    
    // Utility methods
    std::string loadJsonFile(const std::string& filename);
    bool saveJsonFile(const std::string& filename, const json& data);
    std::string generateTimestamp();
    std::string formatFileSize(size_t bytes);
    json getBackupInfo();
    bool updateBackupInfo(const json& info);
    
    // Helper methods
    std::string getBackupDataPath();
    std::vector<std::string> getBackupFiles();
    json createBackupListItem(const std::string& filePath);
    std::string calculateDirectorySize(const std::string& path, const json& options);
    bool createBackupFile(const json& options, const std::string& outputPath);
    bool extractBackupFile(const std::string& backupPath, const json& options);
    std::vector<std::string> getIncludedPaths(const json& options);
    int calculateFileCount(const json& options);
};
