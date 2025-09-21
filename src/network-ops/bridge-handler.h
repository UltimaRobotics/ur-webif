
#ifndef BRIDGE_HANDLER_H
#define BRIDGE_HANDLER_H

#include <nlohmann/json.hpp>
#include <string>

class BridgeHandler {
public:
    BridgeHandler();
    
    // Data management
    nlohmann::json getBridges();
    nlohmann::json getBridge(const std::string& bridgeId);
    bool createBridge(const nlohmann::json& bridgeData);
    bool updateBridge(const std::string& bridgeId, const nlohmann::json& bridgeData);
    bool deleteBridge(const std::string& bridgeId);
    bool toggleBridge(const std::string& bridgeId);
    
    // HTTP request handlers
    std::string handleBridgeRequest(const std::string& method, const std::string& path, const std::string& body);
    
private:
    std::string basePath;
    nlohmann::json bridgesData;
    
    // Data operations
    void loadBridges();
    bool saveBridges();
    
    // HTTP response handlers
    std::string handleGetBridges();
    std::string handleGetBridge(const std::string& bridgeId);
    std::string handleCreateBridge(const std::string& body);
    std::string handleUpdateBridge(const std::string& bridgeId, const std::string& body);
    std::string handleDeleteBridge(const std::string& bridgeId);
    
    // Utility methods
    bool saveJsonToFile(const nlohmann::json& data, const std::string& filename);
    std::string getCurrentTimestamp();
    std::string createSuccessResponse(const nlohmann::json& data, const std::string& message = "");
    std::string createErrorResponse(const std::string& error, int code = 400);
};

#endif
