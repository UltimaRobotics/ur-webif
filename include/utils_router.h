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
 * Utilities Router
 * Handles all utility endpoints:
 * - /api/health
 * - /api/echo
 * - /api/stats
 * - /api/broadcast
 * - /api/event
 */
class UtilsRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;
    using StructuredRouteHandler = std::function<ApiResponse(const ApiRequest&)>;

    UtilsRouter();
    ~UtilsRouter() = default;

    // Initialize with dependencies
    void initialize(std::shared_ptr<HttpEventHandler> event_handler, 
                   std::shared_ptr<RouteProcessors> route_processors);

    // Register all utility routes with the server
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                       std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler);

private:
    std::shared_ptr<HttpEventHandler> event_handler_;
    std::shared_ptr<RouteProcessors> route_processors_;

    // Route handlers
    std::string handleHealth(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleEcho(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleStats(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleBroadcast(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleEvent(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    ApiResponse handleEchoStructured(const ApiRequest& request);
    ApiResponse handleHealthStructured(const ApiRequest& request);
};