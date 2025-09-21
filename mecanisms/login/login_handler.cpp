#include "login_handler.hpp"
#include <regex>
#include <algorithm>
#include <cctype>

LoginHandler::LoginHandler(std::shared_ptr<LoginManager> login_manager) 
    : login_manager_(login_manager) {
}

void LoginHandler::handleLoginMessage(const nlohmann::json& message, 
                                     const std::string& client_id, 
                                     const std::string& client_ip,
                                     ResponseCallback response_callback) {
    try {
        // Validate input
        if (!validateLoginInput(message)) {
            auto error_response = buildErrorResponse("Invalid login data format");
            response_callback(error_response.dump());
            return;
        }
        
        // Extract credentials
        std::string username = sanitizeInput(message["payload"]["username"].get<std::string>());
        std::string password = message["payload"]["password"].get<std::string>();
        bool remember_me = message["payload"].value("remember_me", false);
        
        // Perform authentication
        auto result = login_manager_->authenticate(username, password, remember_me, client_ip);
        
        // Build and send response
        auto login_response = buildLoginResponse(result);
        login_response["client_id"] = client_id;
        login_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        response_callback(login_response.dump());
        
    } catch (const std::exception& e) {
        auto error_response = buildErrorResponse("Login processing error: " + std::string(e.what()));
        response_callback(error_response.dump());
    }
}

void LoginHandler::handleLogoutMessage(const nlohmann::json& message,
                                      const std::string& client_id,
                                      ResponseCallback response_callback) {
    try {
        std::string session_token = "";
        
        if (message["payload"].contains("session_token")) {
            session_token = sanitizeInput(message["payload"]["session_token"].get<std::string>());
        }
        
        bool success = false;
        if (!session_token.empty()) {
            success = login_manager_->logout(session_token);
        }
        
        auto logout_response = buildLogoutResponse(success);
        logout_response["client_id"] = client_id;
        logout_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        response_callback(logout_response.dump());
        
    } catch (const std::exception& e) {
        auto error_response = buildErrorResponse("Logout processing error: " + std::string(e.what()));
        response_callback(error_response.dump());
    }
}

void LoginHandler::handleSessionValidation(const nlohmann::json& message,
                                          const std::string& client_id,
                                          ResponseCallback response_callback) {
    try {
        std::string session_token = "";
        
        if (message["payload"].contains("session_token")) {
            session_token = sanitizeInput(message["payload"]["session_token"].get<std::string>());
        }
        
        bool valid = false;
        std::string username = "";
        
        if (!session_token.empty()) {
            valid = login_manager_->validateSession(session_token);
            if (valid) {
                username = login_manager_->getUsernameFromSession(session_token);
            }
        }
        
        auto validation_response = buildSessionValidationResponse(valid, username);
        validation_response["client_id"] = client_id;
        validation_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        response_callback(validation_response.dump());
        
    } catch (const std::exception& e) {
        auto error_response = buildErrorResponse("Session validation error: " + std::string(e.what()));
        response_callback(error_response.dump());
    }
}

bool LoginHandler::isAuthenticated(const std::string& session_token) {
    if (session_token.empty()) {
        return false;
    }
    
    return login_manager_->validateSession(session_token);
}

std::string LoginHandler::getUserFromSession(const std::string& session_token) {
    return login_manager_->getUsernameFromSession(session_token);
}

void LoginHandler::performMaintenance() {
    login_manager_->cleanupExpiredSessions();
}

nlohmann::json LoginHandler::buildLoginResponse(const LoginManager::LoginResult& result) {
    nlohmann::json response;
    response["type"] = "login_response";
    response["success"] = result.success;
    response["message"] = result.message;
    
    if (result.success) {
        response["payload"]["session_token"] = result.session_token;
        response["payload"]["authenticated"] = true;
    } else {
        response["payload"]["remaining_attempts"] = result.remaining_attempts;
        response["payload"]["authenticated"] = false;
    }
    
    return response;
}

nlohmann::json LoginHandler::buildLogoutResponse(bool success) {
    nlohmann::json response;
    response["type"] = "logout_response";
    response["success"] = success;
    response["message"] = success ? "Logout successful" : "Logout failed";
    response["payload"]["authenticated"] = false;
    
    return response;
}

nlohmann::json LoginHandler::buildSessionValidationResponse(bool valid, const std::string& username) {
    nlohmann::json response;
    response["type"] = "session_validation_response";
    response["success"] = valid;
    response["message"] = valid ? "Session is valid" : "Session is invalid or expired";
    response["payload"]["authenticated"] = valid;
    
    if (valid && !username.empty()) {
        response["payload"]["username"] = username;
    }
    
    return response;
}

nlohmann::json LoginHandler::buildErrorResponse(const std::string& error_message) {
    nlohmann::json response;
    response["type"] = "login_error";
    response["success"] = false;
    response["message"] = error_message;
    response["payload"]["authenticated"] = false;
    
    return response;
}

bool LoginHandler::validateLoginInput(const nlohmann::json& message) {
    try {
        // Check required fields
        if (!message.contains("payload")) {
            return false;
        }
        
        const auto& payload = message["payload"];
        
        if (!payload.contains("username") || !payload.contains("password")) {
            return false;
        }
        
        // Check data types
        if (!payload["username"].is_string() || !payload["password"].is_string()) {
            return false;
        }
        
        // Basic validation
        std::string username = payload["username"].get<std::string>();
        std::string password = payload["password"].get<std::string>();
        
        if (username.empty() || password.empty()) {
            return false;
        }
        
        // Username validation (alphanumeric + underscore + dash)
        std::regex username_regex("^[a-zA-Z0-9_-]+$");
        if (!std::regex_match(username, username_regex)) {
            return false;
        }
        
        // Password length validation
        if (password.length() < 3 || password.length() > 128) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

std::string LoginHandler::sanitizeInput(const std::string& input) {
    std::string sanitized = input;
    
    // Remove control characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(), 
                                  [](unsigned char c) { return std::iscntrl(c); }), 
                   sanitized.end());
    
    // Trim whitespace
    sanitized.erase(sanitized.begin(), std::find_if(sanitized.begin(), sanitized.end(), 
                                                    [](unsigned char ch) { return !std::isspace(ch); }));
    sanitized.erase(std::find_if(sanitized.rbegin(), sanitized.rend(), 
                                [](unsigned char ch) { return !std::isspace(ch); }).base(), 
                   sanitized.end());
    
    // Limit length
    if (sanitized.length() > 256) {
        sanitized = sanitized.substr(0, 256);
    }
    
    return sanitized;
}