#include "utils_router.h"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>

UtilsRouter::UtilsRouter() {
    // Constructor initialization
}

void UtilsRouter::initialize(std::shared_ptr<HttpEventHandler> event_handler, 
                           std::shared_ptr<RouteProcessors> route_processors) {
    event_handler_ = event_handler;
    route_processors_ = route_processors;
    ENDPOINT_LOG("utils", "UtilsRouter: Initialized with event handler and route processors");
}

void UtilsRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                               std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler) {
    ENDPOINT_LOG("utils", "UtilsRouter: Registering utility routes...");
    
    // Register utility routes
    addRouteHandler("/api/health", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleHealth(method, params, body);
    });
    
    addRouteHandler("/api/echo", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleEcho(method, params, body);
    });
    
    addRouteHandler("/api/stats", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleStats(method, params, body);
    });
    
    addRouteHandler("/api/broadcast", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleBroadcast(method, params, body);
    });
    
    addRouteHandler("/api/event", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleEvent(method, params, body);
    });
    
    // Register structured routes
    addStructuredRouteHandler("/api/echo", [this](const ApiRequest& request) {
        return this->handleEchoStructured(request);
    });
    
    addStructuredRouteHandler("/api/health", [this](const ApiRequest& request) {
        return this->handleHealthStructured(request);
    });
    
    ENDPOINT_LOG("utils", "UtilsRouter: Utility routes registered successfully");
}

std::string UtilsRouter::handleHealth(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Processing health check request, method: " + method);
    
    json response;
    response["status"] = "ok";
    response["message"] = "Server is healthy";
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    response["method"] = method;
    
    return response.dump();
}

std::string UtilsRouter::handleEcho(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Processing echo request, method: " + method);
    
    if (method != "POST") {
        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    }

    if (!event_handler_) {
        json error = {{"success", false}, {"message", "Event handler not initialized"}};
        return error.dump();
    }

    // Handle potentially malformed JSON for echo endpoint
    json payload;
    try {
        payload = json::parse(body.empty() ? "{}" : body);
    } catch (const std::exception& e) {
        payload = {{"raw_data", body}};
    }

    json event_request = {
        {"type", "echo_test"},
        {"payload", payload},
        {"source", {{"component", "test-client"}, {"action", "echo"}}}
    };

    auto response = event_handler_->processEvent(event_request.dump(), params);
    json response_json = {
        {"success", response.result == HttpEventHandler::EventResult::SUCCESS},
        {"message", response.message},
        {"data", response.data}
    };
    
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Echo processed successfully");
    return response_json.dump();
}

std::string UtilsRouter::handleStats(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Processing stats request, method: " + method);
    
    json response;
    response["service"] = "ur-webif-api";
    response["version"] = "3.0.0";
    response["architecture"] = "http-based";
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    response["server_uptime"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    response["active_http_connections"] = 1; // Simplified for HTTP
    response["total_requests"] = 1; // Would be tracked in real implementation
    response["status"] = "healthy";

    return response.dump();
}

std::string UtilsRouter::handleBroadcast(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Processing broadcast request, method: " + method);
    
    if (method != "POST") {
        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    }

    try {
        auto json_body = json::parse(body);

        json response;
        response["type"] = "broadcast_response";
        response["success"] = true;
        response["message"] = "Message would be broadcast to all connected clients";
        response["original_message"] = json_body.value("data", "");
        response["broadcast_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        response["recipients"] = 0; // In HTTP model, we don't maintain persistent connections

        ENDPOINT_LOG("utils", "[UTILS-ROUTER] Broadcast processed successfully");
        return response.dump();

    } catch (const std::exception& e) {
        json error = {{"success", false}, {"message", "Invalid request format"}};
        return error.dump();
    }
}

std::string UtilsRouter::handleEvent(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Processing event request, method: " + method);
    
    if (method != "POST") {
        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    }

    json response = {
        {"success", true},
        {"message", "Event received and processed"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"processed", true}
    };
    
    // Log the event data if valid JSON
    try {
        if (!body.empty()) {
            auto event_data = json::parse(body);
            response["event_type"] = event_data.value("type", "unknown");
            response["element_id"] = event_data.value("elementId", "unknown");
            ENDPOINT_LOG("utils", "[EVENT-TRACKER] Event received: " + event_data.dump(2));
        }
    } catch (const std::exception& e) {
        ENDPOINT_LOG("utils", "[EVENT-TRACKER] Invalid JSON event data: " + body);
        response["event_type"] = "invalid_json";
    }
    
    return response.dump();
}

ApiResponse UtilsRouter::handleEchoStructured(const ApiRequest& request) {
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Processing structured echo request");
    
    if (!route_processors_) {
        ENDPOINT_LOG_ERROR("utils", "[UTILS-ROUTER] Error: Route processors not initialized");
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }
    
    auto response = route_processors_->processEchoRequest(request);
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Structured echo processed, success: " + std::to_string(response.success));
    
    return response;
}

ApiResponse UtilsRouter::handleHealthStructured(const ApiRequest& request) {
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Processing structured health check request");
    
    if (!route_processors_) {
        ENDPOINT_LOG_ERROR("utils", "[UTILS-ROUTER] Error: Route processors not initialized");
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }
    
    auto response = route_processors_->processHealthCheck(request);
    ENDPOINT_LOG("utils", "[UTILS-ROUTER] Structured health check processed, success: " + std::to_string(response.success));
    
    return response;
}