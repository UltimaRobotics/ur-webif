
#ifndef STATIC_ROUTES_HANDLER_H
#define STATIC_ROUTES_HANDLER_H

#include <nlohmann/json.hpp>
#include <string>

class StaticRoutesHandler {
public:
    StaticRoutesHandler();
    
    // Data management
    nlohmann::json getStaticRoutes();
    nlohmann::json getStaticRoute(const std::string& routeId);
    bool createStaticRoute(const nlohmann::json& routeData);
    bool updateStaticRoute(const std::string& routeId, const nlohmann::json& routeData);
    bool deleteStaticRoute(const std::string& routeId);
    bool toggleStaticRoute(const std::string& routeId);
    
    // HTTP request handlers
    std::string handleStaticRouteRequest(const std::string& method, const std::string& path, const std::string& body);
    
private:
    std::string basePath;
    nlohmann::json staticRoutesData;
    
    // Data operations
    void loadStaticRoutes();
    bool saveStaticRoutes();
    
    // HTTP response handlers
    std::string handleGetStaticRoutes();
    std::string handleGetStaticRoute(const std::string& routeId);
    std::string handleCreateStaticRoute(const std::string& body);
    std::string handleUpdateStaticRoute(const std::string& routeId, const std::string& body);
    std::string handleDeleteStaticRoute(const std::string& routeId);
    
    // Utility methods
    bool saveJsonToFile(const nlohmann::json& data, const std::string& filename);
    std::string getCurrentTimestamp();
    std::string createSuccessResponse(const nlohmann::json& data, const std::string& message = "");
    std::string createErrorResponse(const std::string& error, int code = 400);
};

#endif
