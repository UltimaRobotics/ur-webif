#pragma once

#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>
#include "../include/api_request.h"

using json = nlohmann::json;

class LicenseRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;
    using StructuredRouteHandler = std::function<ApiResponse(const ApiRequest&)>;

    LicenseRouter();
    ~LicenseRouter();

    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                       std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler);

private:
    // Core endpoint handlers following integration guide pattern
    std::string handleLicenseStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleLicenseActivate(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleLicenseValidate(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleLicenseConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleLicenseEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);

    // License data processing methods
    json getLicenseStatus();
    json getLicenseConfiguration();
    json processLicenseActivation(const json& activationData);
    json processLicenseValidation(const json& validationData);
    json getLicenseEvents();

    // License action handlers
    json handleLicenseKeyActivation(const json& activationContent);
    json handleFileUploadActivation(const json& activationContent);
    json handleOnlineActivation(const json& activationContent);

    // File operation helpers
    json loadJsonFromFile(const std::string& filename);
    bool saveJsonToFile(const std::string& filename, const json& data);

    // Helper methods following integration guide pattern
    json createSuccessResponse(const json& data);
    json createErrorResponse(const std::string& message, int code = 500);
    bool validateRequestData(const json& data);
    bool validateLicenseKey(const std::string& licenseKey);
    bool validateLicenseFile(const std::string& fileContent);

    // Utility functions
    std::string formatDate(const std::string& timestamp_str);
    std::string getStatusColor(long long remaining_days, bool is_active);
    long long getCurrentTimestamp();
    void logLicenseEvent(const std::string& eventType, const std::string& message, const json& details);
    void initializeDefaultLicenseFiles();
    std::string base64_decode(const std::string& s);

    // License management state
    std::string current_license_status;
    std::string current_license_id;
    std::string current_license_type;
    int64_t license_start_timestamp;
    int64_t license_expiry_timestamp;
    bool signature_validation_enabled;
    std::string data_directory;
};