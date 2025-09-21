
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "web_server.h"
#include <string>
#include <memory>

class ConfigManager {
public:
    static ConfigManager& getInstance();
    
    void setConfig(const ServerConfig& config);
    const ServerConfig& getConfig() const;
    
    std::string getDataDirectory() const;
    std::string getDataPath(const std::string& relativePath) const;
    
private:
    ConfigManager() = default;
    ServerConfig config_;
};

#endif // CONFIG_MANAGER_H
