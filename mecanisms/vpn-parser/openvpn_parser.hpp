
#pragma once

#include <string>
#include <map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class OpenVPNParser {
public:
    static bool isOpenVPNConfig(const std::string& configContent);
    static json parseConfig(const std::string& configContent);

private:
    static std::map<std::string, std::string> parseConfigLines(const std::string& configContent);
    static std::string extractValue(const std::string& line);
    static std::string extractRemoteServer(const std::string& remoteLine);
    static int extractRemotePort(const std::string& remoteLine);
    static std::string extractUsername(const std::string& configContent);
    static std::string determineProtocol(const std::string& configContent);
};
