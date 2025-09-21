
#include "NetworkPriorityRouter.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <chrono>

NetworkPriorityRouter::NetworkPriorityRouter() {
}

void NetworkPriorityRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> registerFunc) {
    std::vector<std::string> routes = {
        "/api/network-priority/interfaces",
        "/api/network-priority/routing-rules", 
        "/api/network-priority/reorder-interfaces",
        "/api/network-priority/reorder-rules",
        "/api/network-priority/apply",
        "/api/network-priority/reset", 
        "/api/network-priority/status",
        "/api/network-priority/events"
    };

    for (const auto& route : routes) {
        registerFunc(route, [this, route](const std::string& method,
                                        const std::map<std::string, std::string>& params,
                                        const std::string& body) -> std::string {
            return this->handleRequest(method, route, body);
        });
    }
    
    // Register individual routing rule endpoints with regex pattern
    registerFunc("/api/network-priority/routing-rules/.*", [this](const std::string& method,
                                                                  const std::map<std::string, std::string>& params,
                                                                  const std::string& body) -> std::string {
        // Extract the full path from params if available, otherwise construct it
        std::string fullPath;
        if (params.find("path") != params.end()) {
            fullPath = params.at("path");
        } else if (params.find("1") != params.end()) {
            // Fallback: construct path from captured group
            fullPath = "/api/network-priority/routing-rules/" + params.at("1");
        } else {
            // Last resort fallback
            fullPath = "/api/network-priority/routing-rules/unknown";
        }
        std::cout << "[NETWORK-PRIORITY-ROUTER] Individual rule endpoint called with path: " << fullPath << std::endl;
        std::cout << "[NETWORK-PRIORITY-ROUTER] Method: " << method << ", Available params: ";
        for (const auto& param : params) {
            std::cout << param.first << "=" << param.second << " ";
        }
        std::cout << std::endl;
        return this->handleRequest(method, fullPath, body);
    });
    
    // Register individual interface endpoints with regex pattern  
    registerFunc("/api/network-priority/interfaces/.*", [this](const std::string& method,
                                                               const std::map<std::string, std::string>& params,
                                                               const std::string& body) -> std::string {
        // Extract the full path from params if available, otherwise construct it
        std::string fullPath = "/api/network-priority/interfaces/";
        if (params.find("path") != params.end()) {
            fullPath = params.at("path");
        }
        return this->handleRequest(method, fullPath, body);
    });
}

std::string NetworkPriorityRouter::handleRequest(const std::string& method, const std::string& route, const std::string& body) {
    std::cout << "[NETWORK-PRIORITY-ROUTER] Processing " << method << " request for route: " << route << std::endl;
    
    try {
        nlohmann::json requestData;
        if (!body.empty()) {
            try {
                requestData = nlohmann::json::parse(body);
                std::cout << "[NETWORK-PRIORITY-ROUTER] Request data: " << requestData.dump(2) << std::endl;
            } catch (const std::exception& e) {
                std::cout << "[NETWORK-PRIORITY-ROUTER] Invalid JSON in request body: " << e.what() << std::endl;
                return createErrorResponse("Invalid JSON format");
            }
        }
        
        std::cout << "[NETWORK-PRIORITY-ROUTER] Checking route patterns for: " << route << std::endl;

        // Handle interfaces endpoints
        if (route == "/api/network-priority/interfaces") {
            if (method == "GET") {
                return handleGetInterfaces();
            } else if (method == "PUT" && requestData.contains("interfaceId")) {
                return handleUpdateInterface(requestData["interfaceId"], requestData);
            }
        }
        
        // Handle individual interface endpoints
        std::regex interfacePattern(R"(/api/network-priority/interfaces/([^/]+))");
        std::smatch interfaceMatch;
        std::string routeStr = route;
        if (std::regex_search(routeStr, interfaceMatch, interfacePattern)) {
            std::string interfaceId = interfaceMatch[1];
            if (method == "GET") {
                return handleGetInterface(interfaceId);
            } else if (method == "PUT") {
                return handleUpdateInterface(interfaceId, requestData);
            }
        }

        // Handle routing rules endpoints
        if (route == "/api/network-priority/routing-rules") {
            if (method == "GET") {
                return handleGetRoutingRules();
            } else if (method == "POST") {
                return handleAddRoutingRule(requestData);
            }
        }

        // Handle individual routing rule endpoints
        std::regex rulePattern(R"(/api/network-priority/routing-rules/([^/]+)$)");
        std::smatch ruleMatch;
        if (std::regex_match(routeStr, ruleMatch, rulePattern)) {
            std::string ruleId = ruleMatch[1];
            std::cout << "[NETWORK-PRIORITY-ROUTER] Matched routing rule endpoint, ruleId: '" << ruleId << "', method: " << method << std::endl;
            
            if (method == "GET") {
                std::cout << "[NETWORK-PRIORITY-ROUTER] Handling GET for rule: " << ruleId << std::endl;
                return handleGetRoutingRule(ruleId);
            } else if (method == "PUT") {
                std::cout << "[NETWORK-PRIORITY-ROUTER] Handling PUT for rule: " << ruleId << std::endl;
                return handleUpdateRoutingRule(ruleId, requestData);
            } else if (method == "DELETE") {
                std::cout << "[NETWORK-PRIORITY-ROUTER] Handling DELETE for rule: " << ruleId << std::endl;
                return handleDeleteRoutingRule(ruleId);
            }
        } else {
            std::cout << "[NETWORK-PRIORITY-ROUTER] No routing rule pattern match for route: " << route << std::endl;
        }

        // Handle reorder interfaces
        if (route == "/api/network-priority/reorder-interfaces" && method == "POST") {
            return handleReorderInterfaces(requestData);
        }

        // Handle reorder rules
        if (route == "/api/network-priority/reorder-rules" && method == "POST") {
            return handleReorderRules(requestData);
        }

        // Handle apply changes
        if (route == "/api/network-priority/apply" && method == "POST") {
            return handleApplyChanges();
        }

        // Handle reset to defaults
        if (route == "/api/network-priority/reset" && method == "POST") {
            return handleResetDefaults();
        }

        // Handle status
        if (route == "/api/network-priority/status" && method == "GET") {
            return handleGetStatus();
        }

        // Handle events
        if (route == "/api/network-priority/events" && method == "POST") {
            return handleEvents(requestData);
        }

        return createErrorResponse("Endpoint not found or method not allowed", 404);

    } catch (const std::exception& e) {
        std::cout << "[NETWORK-PRIORITY-ROUTER] Exception in handleRequest: " << e.what() << std::endl;
        return createErrorResponse("Internal server error: " + std::string(e.what()), 500);
    }
}

std::string NetworkPriorityRouter::handleGetInterfaces() {
    auto interfaces = dataManager.getInterfaces();
    if (interfaces.empty()) {
        return createErrorResponse("Failed to load interfaces data");
    }
    return createSuccessResponse(interfaces, "Interfaces retrieved successfully");
}

std::string NetworkPriorityRouter::handleGetInterface(const std::string& interfaceId) {
    auto interface = dataManager.getInterface(interfaceId);
    if (interface.empty()) {
        return createErrorResponse("Interface not found", 404);
    }
    return createSuccessResponse(interface, "Interface retrieved successfully");
}

std::string NetworkPriorityRouter::handleUpdateInterface(const std::string& interfaceId, const nlohmann::json& data) {
    bool success = dataManager.updateInterface(interfaceId, data);
    if (success) {
        auto updatedInterface = dataManager.getInterface(interfaceId);
        return createSuccessResponse(updatedInterface, "Interface updated successfully");
    }
    return createErrorResponse("Failed to update interface");
}

std::string NetworkPriorityRouter::handleGetRoutingRules() {
    auto rules = dataManager.getRoutingRules();
    if (rules.empty()) {
        return createErrorResponse("Failed to load routing rules data");
    }
    return createSuccessResponse(rules, "Routing rules retrieved successfully");
}

std::string NetworkPriorityRouter::handleGetRoutingRule(const std::string& ruleId) {
    auto rule = dataManager.getRoutingRule(ruleId);
    if (rule.empty()) {
        return createErrorResponse("Routing rule not found", 404);
    }
    return createSuccessResponse(rule, "Routing rule retrieved successfully");
}

std::string NetworkPriorityRouter::handleAddRoutingRule(const nlohmann::json& data) {
    std::cout << "[NETWORK-PRIORITY-ROUTER] Adding routing rule with data: " << data.dump(2) << std::endl;
    
    // Validate required fields
    if (!data.contains("name") || data["name"].empty()) {
        return createErrorResponse("Rule name is required");
    }
    if (!data.contains("destination_network") || data["destination_network"].empty()) {
        return createErrorResponse("Destination network is required");
    }
    if (!data.contains("gateway") || data["gateway"].empty()) {
        return createErrorResponse("Gateway is required");
    }
    if (!data.contains("interface") || data["interface"].empty()) {
        return createErrorResponse("Interface is required");
    }
    
    bool success = dataManager.addRoutingRule(data);
    if (success) {
        auto rules = dataManager.getRoutingRules();
        std::cout << "[NETWORK-PRIORITY-ROUTER] Routing rule added successfully" << std::endl;
        return createSuccessResponse(rules, "Routing rule added successfully");
    }
    
    std::cout << "[NETWORK-PRIORITY-ROUTER] Failed to add routing rule" << std::endl;
    return createErrorResponse("Failed to add routing rule");
}

std::string NetworkPriorityRouter::handleUpdateRoutingRule(const std::string& ruleId, const nlohmann::json& data) {
    bool success = dataManager.updateRoutingRule(ruleId, data);
    if (success) {
        auto updatedRule = dataManager.getRoutingRule(ruleId);
        return createSuccessResponse(updatedRule, "Routing rule updated successfully");
    }
    return createErrorResponse("Failed to update routing rule");
}

std::string NetworkPriorityRouter::handleDeleteRoutingRule(const std::string& ruleId) {
    std::cout << "[NETWORK-PRIORITY-ROUTER] Attempting to delete routing rule with ID: '" << ruleId << "'" << std::endl;
    
    // URL decode the rule ID if necessary
    std::string decodedRuleId = ruleId;
    // Simple URL decoding for common cases (you might want to add more comprehensive decoding)
    size_t pos = 0;
    while ((pos = decodedRuleId.find("%20", pos)) != std::string::npos) {
        decodedRuleId.replace(pos, 3, " ");
        pos += 1;
    }
    
    std::cout << "[NETWORK-PRIORITY-ROUTER] Decoded rule ID: '" << decodedRuleId << "'" << std::endl;
    
    bool success = dataManager.deleteRoutingRule(decodedRuleId);
    if (success) {
        std::cout << "[NETWORK-PRIORITY-ROUTER] Successfully deleted routing rule: " << decodedRuleId << std::endl;
        auto rules = dataManager.getRoutingRules();
        return createSuccessResponse(rules, "Routing rule deleted successfully");
    }
    
    std::cout << "[NETWORK-PRIORITY-ROUTER] Failed to delete routing rule: " << decodedRuleId << std::endl;
    return createErrorResponse("Failed to delete routing rule: Rule not found or could not be removed", 404);
}

std::string NetworkPriorityRouter::handleReorderInterfaces(const nlohmann::json& data) {
    if (!data.contains("interfaceId") || !data.contains("direction")) {
        return createErrorResponse("Missing interfaceId or direction");
    }

    std::string interfaceId = data["interfaceId"];
    std::string direction = data["direction"];

    bool success = dataManager.moveInterface(interfaceId, direction);
    if (success) {
        auto interfaces = dataManager.getInterfaces();
        // Check if interface exists in the data
        bool interfaceFound = false;
        if (interfaces.contains("interfaces")) {
            for (const auto& interface : interfaces["interfaces"]) {
                if (interface.contains("id") && interface["id"] == interfaceId) {
                    interfaceFound = true;
                    break;
                }
            }
        }
        
        if (interfaceFound) {
            return createSuccessResponse(interfaces, "Interface reordered successfully");
        } else {
            return createSuccessResponse(interfaces, "Failed to reorder interface");
        }
    }
    return createErrorResponse("Failed to reorder interface");
}

std::string NetworkPriorityRouter::handleReorderRules(const nlohmann::json& data) {
    if (!data.contains("ruleId") || !data.contains("direction")) {
        return createErrorResponse("Missing ruleId or direction");
    }

    std::string ruleId = data["ruleId"];
    std::string direction = data["direction"];

    bool success = dataManager.moveRoutingRule(ruleId, direction);
    if (success) {
        auto rules = dataManager.getRoutingRules();
        // Check if rule exists in the data
        bool ruleFound = false;
        if (rules.contains("routing_rules")) {
            for (const auto& rule : rules["routing_rules"]) {
                if (rule.contains("id") && rule["id"] == ruleId) {
                    ruleFound = true;
                    break;
                }
            }
        }
        
        if (ruleFound) {
            return createSuccessResponse(rules, "Routing rule reordered successfully");
        } else {
            return createSuccessResponse(rules, "Failed to reorder routing rule");
        }
    }
    return createErrorResponse("Failed to reorder routing rule");
}

std::string NetworkPriorityRouter::handleApplyChanges() {
    bool success = dataManager.applyChanges();
    if (success) {
        nlohmann::json response = {
            {"applied", true},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return createSuccessResponse(response, "Changes applied successfully");
    }
    return createErrorResponse("Failed to apply changes");
}

std::string NetworkPriorityRouter::handleResetDefaults() {
    bool success = dataManager.resetToDefaults();
    if (success) {
        auto interfaces = dataManager.getInterfaces();
        return createSuccessResponse(interfaces, "Reset to defaults successfully");
    }
    return createErrorResponse("Failed to reset to defaults");
}

std::string NetworkPriorityRouter::handleGetStatus() {
    auto status = dataManager.getNetworkStatus();
    return createSuccessResponse(status, "Network status retrieved successfully");
}

std::string NetworkPriorityRouter::handleEvents(const nlohmann::json& eventData) {
    std::cout << "[NETWORK-PRIORITY] Processing event: " << eventData.dump() << std::endl;
    
    nlohmann::json response = {
        {"success", true},
        {"processed", true},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"message", "Event received and processed"}
    };
    
    if (eventData.contains("element_id")) {
        response["element_id"] = eventData["element_id"];
    }
    if (eventData.contains("event_type")) {
        response["event_type"] = eventData["event_type"];
    }
    
    return response.dump();
}

std::string NetworkPriorityRouter::createSuccessResponse(const nlohmann::json& data, const std::string& message) {
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

std::string NetworkPriorityRouter::createErrorResponse(const std::string& message, int statusCode) {
    nlohmann::json response = {
        {"success", false},
        {"message", message},
        {"status_code", statusCode},
        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    return response.dump();
}
