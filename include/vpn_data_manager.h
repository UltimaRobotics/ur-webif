#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class VpnDataManager {
public:
    static VpnDataManager& getInstance();

    // VPN Profiles operations
    json getVpnProfiles();
    bool saveVpnProfiles(const json& profiles);
    bool addVpnProfile(const json& profile);
    bool updateVpnProfile(const std::string& profileId, const json& profile);
    bool deleteVpnProfile(const std::string& profileId);
    json getVpnProfile(const std::string& profileId);

    // Routing Rules operations
    json getRoutingRules();
    bool saveRoutingRules(const json& rules);
    bool addRoutingRule(const json& rule);
    bool updateRoutingRule(const std::string& ruleId, const json& rule);
    bool deleteRoutingRule(const std::string& ruleId);
    json getRoutingRule(const std::string& ruleId);

    // Security Settings operations
    json getSecuritySettings();
    bool saveSecuritySettings(const json& settings);
    bool updateSecuritySetting(const std::string& settingPath, const json& value);

    // Initialize file paths with configurable data directory
    static void initializePaths();

    // Method to update file paths based on configuration
    static void setConfigPath(const std::string& dataDir);

private:
    VpnDataManager() = default;

    static std::string VPN_PROFILES_FILE;
    static std::string ROUTING_RULES_FILE;
    static std::string SECURITY_SETTINGS_FILE;

    json loadDataFromFile(const std::string& filePath);
    bool saveDataToFile(const std::string& filePath, const json& data);
    std::string generateUniqueId(const std::string& prefix);
    std::string getCurrentTimestamp();
};