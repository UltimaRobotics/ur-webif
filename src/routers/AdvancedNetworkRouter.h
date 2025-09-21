#ifndef ADVANCED_NETWORK_ROUTER_H
#define ADVANCED_NETWORK_ROUTER_H

#include <string>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
#include "../network-ops/vlan-handler.h"
#include "../network-ops/bridge-handler.h"
#include "../network-ops/nat-handler.h"
#include "../network-ops/firewall-handler.h"
#include "../network-ops/static-routes-handler.h"

class AdvancedNetworkRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;

    AdvancedNetworkRouter();

    std::string handleRequest(const std::string& method, const std::string& path, 
                             const std::string& body = "");

    void registerRoutes(std::function<void(const std::string&, std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>)> registerFunc);

private:
    // Specialized handlers
    VlanHandler vlanHandler;
    BridgeHandler bridgeHandler;
    NatHandler natHandler;
    FirewallHandler firewallHandler;
    StaticRoutesHandler staticRoutesHandler;

    // System operation handlers
    std::string handleSystemOperations(const std::string& method, const std::string& operation, const std::string& body);
    std::string handleApplyChanges();
    std::string handleSaveConfiguration();
    std::string handleGetStatus();

    // Utility methods
    std::string createSuccessResponse(const nlohmann::json& data, const std::string& message = "");
    std::string createErrorResponse(const std::string& error, int code = 400);
    std::string extractPathSegment(const std::string& path, int segment);
    nlohmann::json getNetworkStatus();
};

#endif