
#ifndef NETWORK_PRIORITY_ROUTER_H
#define NETWORK_PRIORITY_ROUTER_H

#include <string>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include "../../include/network_priority_data_manager.h"

class NetworkPriorityRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;

    NetworkPriorityRouter();

    void registerRoutes(std::function<void(const std::string&, RouteHandler)> registerFunc);

std::string handleRequest(const std::string& method, const std::string& route, const std::string& body);

private:
    NetworkPriorityDataManager dataManager;
    
    // Interface handlers
    std::string handleGetInterfaces();
    std::string handleGetInterface(const std::string& interfaceId);
    std::string handleUpdateInterface(const std::string& interfaceId, const nlohmann::json& data);
    
    // Routing rule handlers
    std::string handleGetRoutingRules();
    std::string handleGetRoutingRule(const std::string& ruleId);
    std::string handleAddRoutingRule(const nlohmann::json& data);
    std::string handleUpdateRoutingRule(const std::string& ruleId, const nlohmann::json& data);
    std::string handleDeleteRoutingRule(const std::string& ruleId);
    
    // Reorder handlers
    std::string handleReorderInterfaces(const nlohmann::json& data);
    std::string handleReorderRules(const nlohmann::json& data);
    
    // Action handlers
    std::string handleApplyChanges();
    std::string handleResetDefaults();
    std::string handleGetStatus();
    std::string handleEvents(const nlohmann::json& eventData);
    
    // Utility methods
    std::string createSuccessResponse(const nlohmann::json& data, const std::string& message = "");
    std::string createErrorResponse(const std::string& message, int statusCode = 400);
};

#endif // NETWORK_PRIORITY_ROUTER_H
