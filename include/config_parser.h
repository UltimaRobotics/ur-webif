#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <string>
#include <nlohmann/json.hpp>
#include "web_server.h"

class ConfigParser {
public:
    static bool parseConfig(const std::string& config_file, ServerConfig& config);
    static bool saveConfig(const std::string& config_file, const ServerConfig& config);
    static ServerConfig getDefaultConfig();
    
private:
    static void validateConfig(ServerConfig& config);
};

#endif // CONFIG_PARSER_H