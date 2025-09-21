
#ifndef NAT_HANDLER_H
#define NAT_HANDLER_H

#include <nlohmann/json.hpp>
#include <string>

class NatHandler {
public:
    NatHandler();
    
    // Data management
    nlohmann::json getNatRules();
    nlohmann::json getNatRule(const std::string& ruleId);
    bool createNatRule(const nlohmann::json& ruleData);
    bool updateNatRule(const std::string& ruleId, const nlohmann::json& ruleData);
    bool deleteNatRule(const std::string& ruleId);
    bool toggleNatRule(const std::string& ruleId);
    
    // HTTP request handlers
    std::string handleNatRuleRequest(const std::string& method, const std::string& path, const std::string& body);
    
private:
    std::string basePath;
    nlohmann::json natRulesData;
    
    // Data operations
    void loadNatRules();
    bool saveNatRules();
    
    // HTTP response handlers
    std::string handleGetNatRules();
    std::string handleGetNatRule(const std::string& ruleId);
    std::string handleCreateNatRule(const std::string& body);
    std::string handleUpdateNatRule(const std::string& ruleId, const std::string& body);
    std::string handleDeleteNatRule(const std::string& ruleId);
    
    // Utility methods
    bool saveJsonToFile(const nlohmann::json& data, const std::string& filename);
    std::string getCurrentTimestamp();
    std::string createSuccessResponse(const nlohmann::json& data, const std::string& message = "");
    std::string createErrorResponse(const std::string& error, int code = 400);
};

#endif
