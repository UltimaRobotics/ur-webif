
#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class WireGuardParser {
public:
    static bool isWireGuardConfig(const std::string& configContent);
    static json parseConfig(const std::string& configContent);

private:
    static std::string extractValue(const std::string& section, const std::string& key);
    static std::string getInterfaceSection(const std::string& content);
    static std::string getPeerSection(const std::string& content);
    static std::string extractServer(const std::string& endpoint);
    static int extractPort(const std::string& endpoint);
    static std::string generateProfileName(const std::string& content);
};
