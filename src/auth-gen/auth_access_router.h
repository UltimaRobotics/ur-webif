
#ifndef AUTH_ACCESS_ROUTER_H
#define AUTH_ACCESS_ROUTER_H

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include "auth_access_generator.h"
#include "credential_manager.h"

using json = nlohmann::json;

/**
 * Auth Access Router
 * Handles HTTP endpoints for auth access file generation and download
 */
class AuthAccessRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;

    AuthAccessRouter();
    ~AuthAccessRouter() = default;

    // Initialize with dependencies
    void initialize(std::shared_ptr<CredentialManager> credential_manager);

    // Register all auth access routes
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler);

private:
    std::shared_ptr<AuthAccessGenerator> auth_generator_;
    std::shared_ptr<CredentialManager> credential_manager_;

    // Route handlers
    std::string handleGenerateAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleDownloadAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleListAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleDeleteAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleRevokeAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleValidateAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

    // Utility methods
    bool validateSession(const std::map<std::string, std::string>& params, std::string& username);
    std::string extractSessionToken(const std::map<std::string, std::string>& params);
    json createErrorResponse(const std::string& message, int status_code = 400);
    json createSuccessResponse(const std::string& message, const json& data = json::object());
};

#endif // AUTH_ACCESS_ROUTER_H
