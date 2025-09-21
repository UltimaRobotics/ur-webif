#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include "http_event_handler.h"
#include "route_processors.h"

using json = nlohmann::json;

/**
 * Authentication Router
 * Handles all authentication-related endpoints:
 * - /api/login
 * - /api/logout  
 * - /api/validate-session
 */
class AuthRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;
    using StructuredRouteHandler = std::function<ApiResponse(const ApiRequest&)>;

    AuthRouter();
    ~AuthRouter() = default;

    // Initialize with dependencies
    void initialize(std::shared_ptr<HttpEventHandler> event_handler, 
                   std::shared_ptr<RouteProcessors> route_processors);

    // Register all authentication routes with the server
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                       std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler);

private:
    std::shared_ptr<HttpEventHandler> event_handler_;
    std::shared_ptr<RouteProcessors> route_processors_;

    // Route handlers
    std::string handleLogin(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleLogout(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    ApiResponse handleSessionValidation(const ApiRequest& request);
    ApiResponse handleChangePassword(const ApiRequest& request);
};