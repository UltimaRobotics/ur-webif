
#include "config_manager.h"
#include <filesystem>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

void ConfigManager::setConfig(const ServerConfig& config) {
    config_ = config;
}

const ServerConfig& ConfigManager::getConfig() const {
    return config_;
}

std::string ConfigManager::getDataDirectory() const {
    return config_.data_directory;
}

std::string ConfigManager::getDataPath(const std::string& relativePath) const {
    if (relativePath.empty()) {
        return config_.data_directory;
    }
    
    // Remove leading slash if present to avoid double slashes
    std::string cleanPath = relativePath;
    if (cleanPath.front() == '/') {
        cleanPath = cleanPath.substr(1);
    }
    
    return config_.data_directory + "/" + cleanPath;
}
