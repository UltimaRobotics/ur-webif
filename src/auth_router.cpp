#include "auth_router.h"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>

AuthRouter::AuthRouter() {
    // Constructor initialization
}

void AuthRouter::initialize(std::shared_ptr<HttpEventHandler> event_handler, 
                           std::shared_ptr<RouteProcessors> route_processors) {
    event_handler_ = event_handler;
    route_processors_ = route_processors;
    ENDPOINT_LOG_INFO("auth", "AuthRouter: Initialized with event handler and route processors");
}

void AuthRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                               std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler) {
    ENDPOINT_LOG_INFO("auth", "AuthRouter: Registering authentication routes...");
    
    // Register login route
    addRouteHandler("/api/login", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleLogin(method, params, body);
    });
    
    // Register logout route
    addRouteHandler("/api/logout", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleLogout(method, params, body);
    });
    
    // Register session validation route (structured)
    addStructuredRouteHandler("/api/validate-session", [this](const ApiRequest& request) {
        return this->handleSessionValidation(request);
    });

    // Register change password route (structured)
    addStructuredRouteHandler("/api/change-password", [this](const ApiRequest& request) {
        return this->handleChangePassword(request);
    });
    
    ENDPOINT_LOG_INFO("auth", "AuthRouter: Authentication routes registered successfully");
}

std::string AuthRouter::handleLogin(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("auth", "Processing login request, method: " + method);
    
    if (method != "POST") {
        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    }

    try {
        // Create login event request
        json event_request = {
            {"type", "login_attempt"},
            {"payload", json::parse(body)},
            {"source", {
                {"component", "login-form"},
                {"action", "submit"},
                {"element_id", "login-button"}
            }}
        };

        // Process using HTTP event handler
        auto response = event_handler_->processEvent(event_request.dump(), params);

        // Log the complete API response
        ENDPOINT_LOG_INFO("auth", "Login processing result: " + std::to_string(static_cast<int>(response.result)));
        ENDPOINT_LOG_INFO("auth", "Response data: " + response.data.dump(2));
        ENDPOINT_LOG_INFO("auth", "Response message: " + response.message);
        ENDPOINT_LOG_INFO("auth", "HTTP status: " + std::to_string(response.http_status));

        return response.data.dump();

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth", "Login error: " + std::string(e.what()));
        json error = {{"success", false}, {"message", "Invalid request format"}, {"error", e.what()}};
        return error.dump();
    }
}

std::string AuthRouter::handleLogout(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[AUTH-ROUTER] Processing logout request, method: " << method << std::endl;
    
    if (method != "POST") {
        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    }

    try {
        json event_request = {
            {"type", "logout_request"},
            {"payload", json::parse(body.empty() ? "{}" : body)},
            {"source", {{"component", "user-session"}, {"action", "logout"}}}
        };

        auto response = event_handler_->processEvent(event_request.dump(), params);
        json response_json = {
            {"success", response.result == HttpEventHandler::EventResult::SUCCESS},
            {"message", response.message},
            {"data", response.data}
        };
        
        std::cout << "[AUTH-ROUTER] Logout processed successfully" << std::endl;
        return response_json.dump();
        
    } catch (const std::exception& e) {
        std::cout << "[AUTH-ROUTER] Logout error: " << e.what() << std::endl;
        json error = {{"success", false}, {"message", "Invalid request format"}, {"error", e.what()}};
        return error.dump();
    }
}

ApiResponse AuthRouter::handleSessionValidation(const ApiRequest& request) {
    std::cout << "[AUTH-ROUTER] Processing session validation request" << std::endl;
    
    if (!route_processors_) {
        std::cout << "[AUTH-ROUTER] Error: Route processors not initialized" << std::endl;
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }
    
    auto response = route_processors_->processSessionValidation(request);
    std::cout << "[AUTH-ROUTER] Session validation completed, success: " << response.success << std::endl;
    
    return response;
}

ApiResponse AuthRouter::handleChangePassword(const ApiRequest& request) {
    ENDPOINT_LOG_INFO("auth", "Processing change password request");
    
    if (!route_processors_) {
        ENDPOINT_LOG_ERROR("auth", "Route processors not initialized");
        ApiResponse error_response;
        error_response.success = false;
        error_response.data["message"] = "Internal server error";
        error_response.status_code = 500;
        return error_response;
    }
    
    auto response = route_processors_->processChangePassword(request);
    ENDPOINT_LOG_INFO("auth", "Change password completed, success: " + std::to_string(response.success));
    
    return response;
}