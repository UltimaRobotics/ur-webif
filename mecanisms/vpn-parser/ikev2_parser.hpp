
#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class IKEv2Parser {
public:
    static bool isIKEv2Config(const std::string& configContent);
    static json parseConfig(const std::string& configContent);

private:
    static std::string extractValue(const std::string& content, const std::string& key);
    static std::string extractServer(const std::string& content);
    static int extractPort(const std::string& content);
    static std::string extractUsername(const std::string& content);
    static std::string extractProfileName(const std::string& content);
};
