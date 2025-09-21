#include "dashboard_router.h"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>

DashboardRouter::DashboardRouter() {
    // Constructor initialization
}

void DashboardRouter::initialize(std::shared_ptr<RouteProcessors> route_processors) {
    route_processors_ = route_processors;
    ENDPOINT_LOG_INFO("dashboard", "DashboardRouter: Initialized with route processors");
}

void DashboardRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                                   std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler) {
    ENDPOINT_LOG_INFO("dashboard", "DashboardRouter: Registering dashboard and system routes...");

    // Register dashboard data route (mixed handler - both direct and structured)
    addRouteHandler("/api/dashboard-data", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleDashboardData(method, params, body);
    });

    // Register system data routes (structured)
    addStructuredRouteHandler("/api/system-data", [this](const ApiRequest& request) {
        return this->handleSystemData(request);
    });

    addStructuredRouteHandler("/api/system-component", [this](const ApiRequest& request) {
        return this->handleSystemComponent(request);
    });

    addStructuredRouteHandler("/api/network-component", [this](const ApiRequest& request) {
        return this->handleNetworkComponent(request);
    });

    addStructuredRouteHandler("/api/cellular-component", [this](const ApiRequest& request) {
        return this->handleCellularComponent(request);
    });

    // Register utility routes
    utils_router_.registerRoutes(addRouteHandler, addStructuredRouteHandler);

    // Register license routes
    license_router_.registerRoutes(addRouteHandler, addStructuredRouteHandler);

    ENDPOINT_LOG_INFO("dashboard", "DashboardRouter: Dashboard and system routes registered successfully");
}

std::string DashboardRouter::handleDashboardData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("dashboard", "Processing dashboard data request, method: " + method);

    if (!route_processors_) {
        ENDPOINT_LOG_ERROR("dashboard", "Route processors not initialized");
        json error = {{"success", false}, {"message", "Internal server error"}};
        return error.dump();
    }

    // Create API request structure for the route processor
    ApiRequest request;
    request.method = method;
    request.route = "/api/dashboard-data";
    request.body = body;
    request.headers = {};

    // Parse JSON body if present
    if (!body.empty()) {
        try {
            request.json_data = json::parse(body);
            request.is_json_valid = true;
            ENDPOINT_LOG_INFO("dashboard", "Request body: " + request.json_data.dump(2));
        } catch (const std::exception& e) {
            ENDPOINT_LOG_WARNING("dashboard", "Failed to parse JSON body: " + std::string(e.what()));
            request.is_json_valid = false;
        }
    }

    // Process using structured route processor
    auto response = route_processors_->processDashboardDataRequest(request);

    ENDPOINT_LOG_INFO("dashboard", "Dashboard data processed, success: " + std::to_string(response.success));

    // Convert structured response back to JSON string
    json response_json = {
        {"success", response.success},
        {"data", response.data}
    };

    // Add message if it exists in the data
    if (response.data.contains("message")) {
        response_json["message"] = response.data["message"];
    }

    // Add any additional response fields
    if (response.data.contains("auto_update_enabled")) {
        response_json = response.data; // Use the data directly if it's a complete dashboard response
    }

    return response_json.dump();
}

ApiResponse DashboardRouter::handleSystemData(const ApiRequest& request) {
    std::cout << "[DASHBOARD-ROUTER] Processing system data request" << std::endl;

    if (!route_processors_) {
        std::cout << "[DASHBOARD-ROUTER] Error: Route processors not initialized" << std::endl;
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }

    auto response = route_processors_->processSystemDataRequest(request);
    std::cout << "[DASHBOARD-ROUTER] System data processed, success: " << response.success << std::endl;

    return response;
}

ApiResponse DashboardRouter::handleSystemComponent(const ApiRequest& request) {
    std::cout << "[DASHBOARD-ROUTER] Processing system component request" << std::endl;

    if (!route_processors_) {
        std::cout << "[DASHBOARD-ROUTER] Error: Route processors not initialized" << std::endl;
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }

    auto response = route_processors_->processSystemComponentRequest(request);
    std::cout << "[DASHBOARD-ROUTER] System component processed, success: " << response.success << std::endl;

    return response;
}

ApiResponse DashboardRouter::handleNetworkComponent(const ApiRequest& request) {
    std::cout << "[DASHBOARD-ROUTER] Processing network component request" << std::endl;

    if (!route_processors_) {
        std::cout << "[DASHBOARD-ROUTER] Error: Route processors not initialized" << std::endl;
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }

    auto response = route_processors_->processNetworkComponentRequest(request);
    std::cout << "[DASHBOARD-ROUTER] Network component processed, success: " << response.success << std::endl;

    return response;
}

ApiResponse DashboardRouter::handleCellularComponent(const ApiRequest& request) {
    std::cout << "[DASHBOARD-ROUTER] Processing cellular component request" << std::endl;

    if (!route_processors_) {
        std::cout << "[DASHBOARD-ROUTER] Error: Route processors not initialized" << std::endl;
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }

    auto response = route_processors_->processCellularComponentRequest(request);
    std::cout << "[DASHBOARD-ROUTER] Cellular component processed, success: " << response.success << std::endl;

    return response;
}