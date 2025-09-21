#pragma once

#include <string>
#include <map>
#include <functional>
#include <memory> // Include for std::shared_ptr
#include <nlohmann/json.hpp>

// Forward declaration for VpnDataManager
class VpnDataManager;

using json = nlohmann::json;

class VpnRouter {
public:
    // Define the type for route handlers for better readability
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;

    VpnRouter();
    ~VpnRouter();

    // Route registration
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler);

    // Route handlers
    std::string handleVpnStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnConnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnDisconnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

private:
    // VPN Parser Helper Methods
    json parseOpenVPNConfig(const std::string& configContent);
    json parseIKEv2Config(const std::string& configContent);
    json parseWireGuardConfig(const std::string& configContent);


    std::string handleVpnProfiles(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleRoutingRules(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnSecuritySettings(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnLogs(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnActive(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnRoutingRules(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

    // VPN Parser Endpoints
    std::string handleOpenVPNParser(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleIKEv2Parser(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleWireGuardParser(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

    // Centralized VPN Parser Handler using VPN Parser Manager
    std::string handleVpnParseConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

    // Helper methods for VPN operations
    nlohmann::json getVpnConfiguration();
    nlohmann::json getVpnStatus();
    nlohmann::json connectToVpn(const std::string& profileId);
    nlohmann::json disconnectVpn();
    nlohmann::json getVpnEvents();
    nlohmann::json getVpnLogs();
    nlohmann::json getVpnActiveConnections();

private:
    // VPN status and configuration endpoints
    // Note: These are now private members and their declarations are handled within the class definition.
    // The original declarations in the private section of the class will be replaced by the changes.

    // VPN connection management
    // Note: These are now private members and their declarations are handled within the class definition.

    // VPN profile management
    // Note: These are now private members and their declarations are handled within the class definition.

    // VPN events and monitoring
    // Note: These are now private members and their declarations are handled within the class definition.

    // Additional VPN endpoints for frontend compatibility
    // Note: These are now private members and their declarations are handled within the class definition.

    // Helper functions for VPN operations
    // Note: These are now private members and their declarations are handled within the class definition.

    std::shared_ptr<VpnDataManager> vpn_data_manager_;
};