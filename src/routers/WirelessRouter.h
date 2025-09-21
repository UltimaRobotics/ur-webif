#pragma once

#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class WirelessRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;

    WirelessRouter();
    ~WirelessRouter();

    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler);

private:
    // Wireless endpoint handlers matching actual implementation
    std::string handleWirelessEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleWirelessStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleRadioControl(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleNetworkScan(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleNetworkConnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleSavedNetworks(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleAccessPointStart(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleAccessPointStop(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

    // Cellular configuration endpoints
    std::string handleCellularStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleCellularConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleCellularConnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleCellularDisconnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleCellularScan(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleCellularEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    
    // SIM Management endpoints
    std::string handleSIMUnlock(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleCellularResetData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleCellularResetConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

    // VPN configuration endpoints
    std::string handleVpnStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnConnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnDisconnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnProfiles(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleVpnEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

private:
    // Wireless data management methods
    json loadSavedNetworks();
    bool saveSavedNetworks(const json& networks);
    json loadAvailableNetworks();
    bool saveAvailableNetworks(const json& networks);
    json loadManualConnections();
    bool saveManualConnection(const json& connection);
    json loadAccessPointConfig();
    bool saveAccessPointConfig(const json& config);
    json loadAccessPointStatus();
    bool saveAccessPointStatus(const json& status);
    
    // Helper methods
    std::string generateNetworkId();
    std::string generateConnectionId();
    std::string getCurrentTimestamp();
    
    // New data endpoint handlers
    std::string handleSavedNetworksData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleAvailableNetworksData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleManualConnectData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleAccessPointConfigData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleAccessPointStatusData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
};