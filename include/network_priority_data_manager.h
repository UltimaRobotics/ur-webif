#ifndef NETWORK_PRIORITY_DATA_MANAGER_H
#define NETWORK_PRIORITY_DATA_MANAGER_H

#include <string>
#include <nlohmann/json.hpp>
#include <vector>

class NetworkPriorityDataManager {
public:
    NetworkPriorityDataManager();

    // Interface management
    nlohmann::json getInterfaces();
    nlohmann::json getInterface(const std::string& interfaceId);
    bool updateInterface(const std::string& interfaceId, const nlohmann::json& interfaceData);
    bool updateInterfacePriority(const std::string& interfaceId, int newPriority);
    bool toggleInterface(const std::string& interfaceId, bool enabled);

    // Routing rules management
    nlohmann::json getRoutingRules();
    nlohmann::json getRoutingRule(const std::string& ruleId);
    bool addRoutingRule(const nlohmann::json& ruleData);
    bool updateRoutingRule(const std::string& ruleId, const nlohmann::json& ruleData);
    bool deleteRoutingRule(const std::string& ruleId);
    bool updateRulePriority(const std::string& ruleId, int newPriority);

    // Bulk operations
    bool reorderInterfaces(const std::vector<std::string>& orderedIds);
    bool moveInterface(const std::string& interfaceId, const std::string& direction);
    bool reorderRoutingRules(const std::vector<std::string>& orderedIds);
    bool moveRoutingRule(const std::string& ruleId, const std::string& direction);
    bool applyChanges();
    bool resetToDefaults();

    // Status and statistics
    nlohmann::json getStatistics();
    nlohmann::json getNetworkStatus();

private:
    std::string interfacesFilePath;
    std::string routingRulesFilePath;

    nlohmann::json loadInterfacesData();
    nlohmann::json loadRoutingRulesData();
    bool saveInterfacesData(const nlohmann::json& data);
    bool saveRoutingRulesData(const nlohmann::json& data);
    void updateStatistics(nlohmann::json& data);
    std::string generateRuleId();
};

#endif