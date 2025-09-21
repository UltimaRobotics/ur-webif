
#ifndef FIREWALL_HANDLER_H
#define FIREWALL_HANDLER_H

#include <nlohmann/json.hpp>
#include <string>

class FirewallHandler {
public:
    FirewallHandler();
    
    // Data management
    nlohmann::json getFirewallRules();
    nlohmann::json getFirewallRule(const std::string& ruleId);
    bool createFirewallRule(const nlohmann::json& ruleData);
    bool updateFirewallRule(const std::string& ruleId, const nlohmann::json& ruleData);
    bool deleteFirewallRule(const std::string& ruleId);
    bool toggleFirewallRule(const std::string& ruleId);
    
    // HTTP request handlers
    std::string handleFirewallRuleRequest(const std::string& method, const std::string& path, const std::string& body);
    
private:
    std::string basePath;
    nlohmann::json firewallRulesData;
    
    // Data operations
    void loadFirewallRules();
    bool saveFirewallRules();
    
    // HTTP response handlers
    std::string handleGetFirewallRules();
    std::string handleGetFirewallRule(const std::string& ruleId);
    std::string handleCreateFirewallRule(const std::string& body);
    std::string handleUpdateFirewallRule(const std::string& ruleId, const std::string& body);
    std::string handleDeleteFirewallRule(const std::string& ruleId);
    
    // Utility methods
    bool saveJsonToFile(const nlohmann::json& data, const std::string& filename);
    std::string getCurrentTimestamp();
    std::string createSuccessResponse(const nlohmann::json& data, const std::string& message = "");
    std::string createErrorResponse(const std::string& error, int code = 400);
};

#endif
