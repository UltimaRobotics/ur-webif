
#ifndef VLAN_HANDLER_H
#define VLAN_HANDLER_H

#include <nlohmann/json.hpp>
#include <string>

class VlanHandler {
public:
    VlanHandler();
    
    // Data management
    nlohmann::json getVlans();
    nlohmann::json getVlan(const std::string& vlanId);
    bool createVlan(const nlohmann::json& vlanData);
    bool updateVlan(const std::string& vlanId, const nlohmann::json& vlanData);
    bool deleteVlan(const std::string& vlanId);
    bool toggleVlan(const std::string& vlanId);
    
    // HTTP request handlers
    std::string handleVlanRequest(const std::string& method, const std::string& path, const std::string& body);
    
private:
    std::string basePath;
    nlohmann::json vlansData;
    
    // Data operations
    void loadVlans();
    bool saveVlans();
    
    // HTTP response handlers
    std::string handleGetVlans();
    std::string handleGetVlan(const std::string& vlanId);
    std::string handleCreateVlan(const std::string& body);
    std::string handleUpdateVlan(const std::string& vlanId, const std::string& body);
    std::string handleDeleteVlan(const std::string& vlanId);
    
    // Utility methods
    bool saveJsonToFile(const nlohmann::json& data, const std::string& filename);
    std::string getCurrentTimestamp();
    std::string createSuccessResponse(const nlohmann::json& data, const std::string& message = "");
    std::string createErrorResponse(const std::string& error, int code = 400);
};

#endif
