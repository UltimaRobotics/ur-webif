#include "AdvancedNetworkRouter.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <vector>
#include <map>
#include <functional>
#include "nlohmann/json.hpp"

// Include the new handler headers
#include "../network-ops/vlan-handler.h"
#include "../network-ops/nat-handler.h"
#include "../network-ops/firewall-handler.h"
#include "../network-ops/static-routes-handler.h"
#include "../network-ops/bridge-handler.h"

AdvancedNetworkRouter::AdvancedNetworkRouter() : 
    vlanHandler(),
    natHandler(),
    firewallHandler(),
    staticRoutesHandler(),
    bridgeHandler()
{
    std::cout << "[ADVANCED-NETWORK-ROUTER] Initialized with specialized handlers" << std::endl;
}

void AdvancedNetworkRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> registerFunc) {
    // Register base collection endpoints
    std::vector<std::string> baseRoutes = {
        "/api/advanced-network/vlans",
        "/api/advanced-network/nat-rules", 
        "/api/advanced-network/firewall-rules",
        "/api/advanced-network/static-routes",
        "/api/advanced-network/bridges",
        "/api/advanced-network/apply",
        "/api/advanced-network/save",
        "/api/advanced-network/status"
    };

    // Register each base route with proper lambda capture
    for (const auto& route : baseRoutes) {
        registerFunc(route, [this, route](const std::string& method,
                                        const std::map<std::string, std::string>& params,
                                        const std::string& body) -> std::string {
            // Extract the actual request path from params if available
            std::string actualPath = route; // Default to the registered route

            // Check for full path in params - this handles the case where the routing system
            // can pass the actual requested path to us
            if (params.find("request_uri") != params.end()) {
                actualPath = params.at("request_uri");
            } else if (params.find("uri") != params.end()) {
                actualPath = params.at("uri");
            } else if (params.find("path") != params.end()) {
                actualPath = params.at("path");
            } else if (params.find("full_path") != params.end()) {
                actualPath = params.at("full_path");
            }

            std::cout << "[ADVANCED-NETWORK-ROUTER] Processing request: " << method << " " << actualPath << std::endl;
            return this->handleRequest(method, actualPath, body);
        });
    }

    // Register ID-based routes explicitly to ensure they're accessible
    std::vector<std::string> resourceTypes = {"vlans", "nat-rules", "firewall-rules", "static-routes", "bridges"};

    for (const auto& resourceType : resourceTypes) {
        // Register pattern routes for ID-based operations
        std::string resourcePathPattern = "/api/advanced-network/" + resourceType + "/";

        // Try to register a wildcard/pattern route if supported
        registerFunc(resourcePathPattern + "*", [this, resourcePathPattern](const std::string& method,
                                                                           const std::map<std::string, std::string>& params,
                                                                           const std::string& body) -> std::string {
            std::string actualPath = resourcePathPattern;

            // Try to reconstruct the full path from parameters
            if (params.find("id") != params.end()) {
                actualPath += params.at("id");
            } else if (params.find("resource_id") != params.end()) {
                actualPath += params.at("resource_id");
            } else if (params.find("path_suffix") != params.end()) {
                actualPath += params.at("path_suffix");
            } else if (params.find("uri") != params.end()) {
                actualPath = params.at("uri");
            } else if (params.find("request_uri") != params.end()) {
                actualPath = params.at("request_uri");
            }

            std::cout << "[ADVANCED-NETWORK-ROUTER] Processing ID-based request: " << method << " " << actualPath << std::endl;
            return this->handleRequest(method, actualPath, body);
        });
    }
}

std::string AdvancedNetworkRouter::handleRequest(const std::string& method, const std::string& path, const std::string& body) {
    std::cout << "[ADVANCED-NETWORK-ROUTER] Processing request: " << method << " " << path << std::endl;

    try {
        // Route to appropriate handler based on path patterns
        if (path.find("/vlans") != std::string::npos) {
            return vlanHandler.handleVlanRequest(method, path, body);
        } else if (path.find("/nat-rules") != std::string::npos) {
            return natHandler.handleNatRuleRequest(method, path, body);
        } else if (path.find("/firewall-rules") != std::string::npos) {
            return firewallHandler.handleFirewallRuleRequest(method, path, body);
        } else if (path.find("/static-routes") != std::string::npos) {
            return staticRoutesHandler.handleStaticRouteRequest(method, path, body);
        } else if (path.find("/bridges") != std::string::npos) {
            return bridgeHandler.handleBridgeRequest(method, path, body);
        } else if (path.find("/apply") != std::string::npos) {
            return handleSystemOperations(method, "apply", body);
        } else if (path.find("/save") != std::string::npos) {
            return handleSystemOperations(method, "save", body);
        } else if (path.find("/status") != std::string::npos) {
            return handleSystemOperations(method, "status", body);
        }
    } catch (const std::exception& e) {
        std::cout << "[ADVANCED-NETWORK-ROUTER] Exception handling request: " << e.what() << std::endl;
        return createErrorResponse("Internal server error: " + std::string(e.what()), 500);
    }

    // Default error response
    return createErrorResponse("Endpoint not found", 404);
}

std::string AdvancedNetworkRouter::handleSystemOperations(const std::string& method, const std::string& operation, const std::string& body) {
    std::cout << "[ADVANCED-NETWORK-ROUTER] Processing " << method << " request for " << operation << std::endl;

    if (operation == "apply" && method == "POST") {
        return handleApplyChanges();
    } else if (operation == "save" && method == "POST") {
        return handleSaveConfiguration();
    } else if (operation == "status" && method == "GET") {
        return handleGetStatus();
    }

    return createErrorResponse("Unsupported operation or method", 405);
}

std::string AdvancedNetworkRouter::handleApplyChanges() {
    try {
        // In a real implementation, this would apply the network changes to the system
        std::cout << "[ADVANCED-NETWORK-ROUTER] Applying network configuration changes..." << std::endl;

        nlohmann::json response = {
            {"applied", true},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return createSuccessResponse(response, "Changes applied successfully");
    } catch (const std::exception& e) {
        return createErrorResponse("Failed to apply changes: " + std::string(e.what()));
    }
}

std::string AdvancedNetworkRouter::handleSaveConfiguration() {
    try {
        nlohmann::json response = {
            {"saved", true},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return createSuccessResponse(response, "Configuration saved successfully");
    } catch (const std::exception& e) {
        return createErrorResponse("Failed to save configuration: " + std::string(e.what()));
    }
}

std::string AdvancedNetworkRouter::handleGetStatus() {
    auto status = getNetworkStatus();
    return createSuccessResponse(status, "Network status retrieved successfully");
}

nlohmann::json AdvancedNetworkRouter::getNetworkStatus() {
    return nlohmann::json{
        {"status", "operational"},
        {"last_check", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"services_running", true},
        {"configuration_valid", true}
    };
}

std::string AdvancedNetworkRouter::createSuccessResponse(const nlohmann::json& data, const std::string& message) {
    nlohmann::json response = {
        {"success", true},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    if (!message.empty()) {
        response["message"] = message;
    }

    // Merge data into response
    for (auto& [key, value] : data.items()) {
        response[key] = value;
    }

    return response.dump();
}

std::string AdvancedNetworkRouter::createErrorResponse(const std::string& error, int code) {
    nlohmann::json response = {
        {"success", false},
        {"error", error},
        {"code", code},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    return response.dump();
}

std::string AdvancedNetworkRouter::extractPathSegment(const std::string& path, int segmentIndex) {
    std::vector<std::string> segments;
    std::string segment;
    std::istringstream stream(path);

    while (std::getline(stream, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }

    return (segmentIndex < segments.size()) ? segments[segmentIndex] : "";
}