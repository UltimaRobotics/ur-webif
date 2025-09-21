
#include "vpn_data_manager.h"
#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <filesystem>

std::string VpnDataManager::VPN_PROFILES_FILE = "";
std::string VpnDataManager::ROUTING_RULES_FILE = "";
std::string VpnDataManager::SECURITY_SETTINGS_FILE = "";

void VpnDataManager::initializePaths() {
    const std::string dataDir = ConfigManager::getInstance().getDataDirectory();
    VPN_PROFILES_FILE = dataDir + "/vpn/vpn-profiles.json";
    ROUTING_RULES_FILE = dataDir + "/vpn/routing-rules.json";
    SECURITY_SETTINGS_FILE = dataDir + "/vpn/sec-settings.json";
}

VpnDataManager& VpnDataManager::getInstance() {
    static VpnDataManager instance;
    return instance;
}

json VpnDataManager::loadDataFromFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "[VPN-DATA-MGR] Error: Could not open file " << filePath << std::endl;
            return json::object();
        }
        
        json data;
        file >> data;
        file.close();
        
        std::cout << "[VPN-DATA-MGR] Successfully loaded data from " << filePath << std::endl;
        return data;
    } catch (const std::exception& e) {
        std::cerr << "[VPN-DATA-MGR] Error loading data from " << filePath << ": " << e.what() << std::endl;
        return json::object();
    }
}

bool VpnDataManager::saveDataToFile(const std::string& filePath, const json& data) {
    try {
        // Ensure directory exists
        std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
        
        std::ofstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "[VPN-DATA-MGR] Error: Could not open file for writing " << filePath << std::endl;
            return false;
        }
        
        file << data.dump(2);
        file.close();
        
        std::cout << "[VPN-DATA-MGR] Successfully saved data to " << filePath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[VPN-DATA-MGR] Error saving data to " << filePath << ": " << e.what() << std::endl;
        return false;
    }
}

std::string VpnDataManager::generateUniqueId(const std::string& prefix) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return prefix + "_" + std::to_string(timestamp);
}

std::string VpnDataManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// VPN Profiles operations
json VpnDataManager::getVpnProfiles() {
    json data = loadDataFromFile(VPN_PROFILES_FILE);
    return data.contains("vpn_profiles") ? data["vpn_profiles"] : json::array();
}

bool VpnDataManager::saveVpnProfiles(const json& profiles) {
    json data;
    data["vpn_profiles"] = profiles;
    return saveDataToFile(VPN_PROFILES_FILE, data);
}

bool VpnDataManager::addVpnProfile(const json& profile) {
    json profiles = getVpnProfiles();
    
    json newProfile = profile;
    if (!newProfile.contains("id") || newProfile["id"].empty()) {
        newProfile["id"] = generateUniqueId("profile");
    }
    newProfile["created_date"] = getCurrentTimestamp();
    newProfile["last_used"] = "Never";
    newProfile["status"] = "Ready";
    
    if (!newProfile.contains("connection_stats")) {
        newProfile["connection_stats"] = {
            {"total_connections", 0},
            {"successful_connections", 0},
            {"failed_connections", 0},
            {"average_connection_time", 0},
            {"total_data_transferred", "0MB"}
        };
    }
    
    profiles.push_back(newProfile);
    return saveVpnProfiles(profiles);
}

bool VpnDataManager::updateVpnProfile(const std::string& profileId, const json& profile) {
    json profiles = getVpnProfiles();
    
    for (auto& p : profiles) {
        if (p["id"] == profileId) {
            // Preserve certain fields
            std::string createdDate = p.value("created_date", getCurrentTimestamp());
            json connectionStats = p.value("connection_stats", json::object());
            
            p = profile;
            p["id"] = profileId;
            p["created_date"] = createdDate;
            p["connection_stats"] = connectionStats;
            
            return saveVpnProfiles(profiles);
        }
    }
    return false;
}

bool VpnDataManager::deleteVpnProfile(const std::string& profileId) {
    json profiles = getVpnProfiles();
    
    for (auto it = profiles.begin(); it != profiles.end(); ++it) {
        if ((*it)["id"] == profileId) {
            profiles.erase(it);
            return saveVpnProfiles(profiles);
        }
    }
    return false;
}

json VpnDataManager::getVpnProfile(const std::string& profileId) {
    json profiles = getVpnProfiles();
    
    for (const auto& profile : profiles) {
        if (profile["id"] == profileId) {
            return profile;
        }
    }
    return json::object();
}

// Routing Rules operations
json VpnDataManager::getRoutingRules() {
    json data = loadDataFromFile(ROUTING_RULES_FILE);
    return data.contains("routing_rules") ? data["routing_rules"] : json::array();
}

bool VpnDataManager::saveRoutingRules(const json& rules) {
    json data;
    data["routing_rules"] = rules;
    return saveDataToFile(ROUTING_RULES_FILE, data);
}

bool VpnDataManager::addRoutingRule(const json& rule) {
    json rules = getRoutingRules();
    
    json newRule = rule;
    if (!newRule.contains("id") || newRule["id"].empty()) {
        newRule["id"] = generateUniqueId("rule");
    }
    newRule["created_date"] = getCurrentTimestamp();
    newRule["last_modified"] = getCurrentTimestamp();
    
    rules.push_back(newRule);
    return saveRoutingRules(rules);
}

bool VpnDataManager::updateRoutingRule(const std::string& ruleId, const json& rule) {
    json rules = getRoutingRules();
    
    for (auto& r : rules) {
        if (r["id"] == ruleId) {
            std::string createdDate = r.value("created_date", getCurrentTimestamp());
            
            r = rule;
            r["id"] = ruleId;
            r["created_date"] = createdDate;
            r["last_modified"] = getCurrentTimestamp();
            
            return saveRoutingRules(rules);
        }
    }
    return false;
}

bool VpnDataManager::deleteRoutingRule(const std::string& ruleId) {
    json rules = getRoutingRules();
    
    for (auto it = rules.begin(); it != rules.end(); ++it) {
        if ((*it)["id"] == ruleId) {
            rules.erase(it);
            return saveRoutingRules(rules);
        }
    }
    return false;
}

json VpnDataManager::getRoutingRule(const std::string& ruleId) {
    json rules = getRoutingRules();
    
    for (const auto& rule : rules) {
        if (rule["id"] == ruleId) {
            return rule;
        }
    }
    return json::object();
}

// Security Settings operations
json VpnDataManager::getSecuritySettings() {
    json data = loadDataFromFile(SECURITY_SETTINGS_FILE);
    return data.contains("security_settings") ? data["security_settings"] : json::object();
}

bool VpnDataManager::saveSecuritySettings(const json& settings) {
    json data;
    data["security_settings"] = settings;
    data["security_settings"]["global_settings"]["last_modified"] = getCurrentTimestamp();
    return saveDataToFile(SECURITY_SETTINGS_FILE, data);
}

bool VpnDataManager::updateSecuritySetting(const std::string& settingPath, const json& value) {
    json settings = getSecuritySettings();
    
    // Parse setting path (e.g., "global_settings.kill_switch")
    std::vector<std::string> pathComponents;
    std::stringstream ss(settingPath);
    std::string component;
    
    while (std::getline(ss, component, '.')) {
        pathComponents.push_back(component);
    }
    
    // Navigate to the setting
    json* current = &settings;
    for (size_t i = 0; i < pathComponents.size() - 1; ++i) {
        if (!current->contains(pathComponents[i])) {
            (*current)[pathComponents[i]] = json::object();
        }
        current = &(*current)[pathComponents[i]];
    }
    
    // Update the setting
    (*current)[pathComponents.back()] = value;
    
    return saveSecuritySettings(settings);
}
