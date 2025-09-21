#ifndef LOGIN_HANDLER_HPP
#define LOGIN_HANDLER_HPP

#include "login_manager.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>

class LoginHandler {
public:
    using ResponseCallback = std::function<void(const std::string&)>;
    
    LoginHandler(std::shared_ptr<LoginManager> login_manager);
    
    // WebSocket message processing
    void handleLoginMessage(const nlohmann::json& message, 
                           const std::string& client_id, 
                           const std::string& client_ip,
                           ResponseCallback response_callback);
    
    void handleLogoutMessage(const nlohmann::json& message,
                            const std::string& client_id,
                            ResponseCallback response_callback);
    
    void handleSessionValidation(const nlohmann::json& message,
                                const std::string& client_id,
                                ResponseCallback response_callback);
    
    // Utility functions
    bool isAuthenticated(const std::string& session_token);
    std::string getUserFromSession(const std::string& session_token);
    
    // Periodic maintenance
    void performMaintenance();

private:
    std::shared_ptr<LoginManager> login_manager_;
    
    // Message builders
    nlohmann::json buildLoginResponse(const LoginManager::LoginResult& result);
    nlohmann::json buildLogoutResponse(bool success);
    nlohmann::json buildSessionValidationResponse(bool valid, const std::string& username = "");
    nlohmann::json buildErrorResponse(const std::string& error_message);
    
    // Input validation
    bool validateLoginInput(const nlohmann::json& message);
    std::string sanitizeInput(const std::string& input);
};

#endif // LOGIN_HANDLER_HPP