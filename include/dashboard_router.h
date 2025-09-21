#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include "route_processors.h"
#include "utils_router.h"
#include "wired_router.h"
#include "routers/LicenseRouter.h"

using json = nlohmann::json;

/**
 * Dashboard Router
 * Handles all dashboard and system-related endpoints:
 * - /api/dashboard-data
 * - /api/system-data
 * - /api/system-component
 * - /api/network-component
 * - /api/cellular-component
 */
class DashboardRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;
    using StructuredRouteHandler = std::function<ApiResponse(const ApiRequest&)>;

    DashboardRouter();
    ~DashboardRouter() = default;

    // Initialize with dependencies
    void initialize(std::shared_ptr<RouteProcessors> route_processors);

    // Register all dashboard routes with the server
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                       std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler);

private:
    std::shared_ptr<RouteProcessors> route_processors_;

    // Route handlers
    std::string handleDashboardData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    ApiResponse handleSystemData(const ApiRequest& request);
    ApiResponse handleSystemComponent(const ApiRequest& request);
    ApiResponse handleNetworkComponent(const ApiRequest& request);
    ApiResponse handleCellularComponent(const ApiRequest& request);

    UtilsRouter utils_router_;
    WiredRouter wired_router_;
    LicenseRouter license_router_;
};