#pragma once

#include "api_request.h"
#include "credential_manager.h"
#include "source-page-data.hpp"
#include "dashboard_globals.h"
#include <memory>

/**
 * Collection of route processors for different API endpoints
 */
class RouteProcessors {
public:
    RouteProcessors();
    ~RouteProcessors();
    
    // Initialize processors with dependencies
    void initialize(std::shared_ptr<CredentialManager> credential_manager);
    
    // Login mechanism processor
    ApiResponse processLoginRequest(const ApiRequest& request);
    
    // Logout mechanism processor
    ApiResponse processLogoutRequest(const ApiRequest& request);
    
    // Session validation processor
    ApiResponse processSessionValidation(const ApiRequest& request);

    // Change password processor
    ApiResponse processChangePassword(const ApiRequest& request);
    
    // Health check processor
    ApiResponse processHealthCheck(const ApiRequest& request);
    
    // Echo service processor
    ApiResponse processEchoRequest(const ApiRequest& request);
    
    // File upload processor
    ApiResponse processFileUpload(const ApiRequest& request);
    
    // System data processor
    ApiResponse processSystemDataRequest(const ApiRequest& request);
    
    // Dashboard data processor (uses global dashboard system)
    ApiResponse processDashboardDataRequest(const ApiRequest& request);
    
    // Isolated data processors for specific components
    ApiResponse processSystemComponentRequest(const ApiRequest& request);
    ApiResponse processNetworkComponentRequest(const ApiRequest& request);
    ApiResponse processCellularComponentRequest(const ApiRequest& request);
    
private:
    std::shared_ptr<CredentialManager> credential_manager_;
    std::unique_ptr<SourcePageDataManager> system_data_manager_;
    
    // Helper methods
    bool validateLoginData(const nlohmann::json& data);
    bool isValidSession(const std::string& token);
    // Note: Session generation is now handled by CredentialManager
};