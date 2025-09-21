#include "LicenseRouter.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <filesystem>
#include "license_data_structure.h"
#include "../include/api_request.h" // Assuming this header is still needed for general API utilities

using json = nlohmann::json;

// Constructor
LicenseRouter::LicenseRouter() {
    signature_validation_enabled = true;
    data_directory = "data/license/";
    std::cout << "LicenseRouter: Initializing with data directory: " << data_directory << std::endl;

    // Ensure data directory exists and initialize default license files if they don't exist
    std::filesystem::create_directories(data_directory);
    initializeDefaultLicenseFiles();
}

// Destructor
LicenseRouter::~LicenseRouter() {
    std::cout << "LicenseRouter: Cleaning up license router..." << std::endl;
}

// Helper function to get current timestamp in seconds
long long LicenseRouter::getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Helper function to format date from timestamp
std::string LicenseRouter::formatDate(const std::string& timestamp_str) {
    if (timestamp_str.empty()) return "N/A";
    try {
        long long timestamp = std::stoll(timestamp_str);
        std::time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::from_time_t(timestamp));
        std::tm tm = *std::localtime(&tt);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
        return buffer;
    } catch (const std::exception& e) {
        std::cerr << "[LICENSE] Error formatting date: " << e.what() << std::endl;
        return "Invalid Date";
    }
}

// Helper function to determine status color
std::string LicenseRouter::getStatusColor(long long remaining_days, bool is_active) {
    if (!is_active) return "red";
    if (remaining_days <= 0) return "red";
    if (remaining_days < 30) return "orange";
    return "green";
}

// Helper function to log license events
void LicenseRouter::logLicenseEvent(const std::string& event_type, const std::string& message, const json& details) {
    long long timestamp = getCurrentTimestamp();
    json event_log_entry = {
        {"event_id", std::to_string(timestamp) + "-" + std::to_string(rand() % 1000)},
        {"event_type", event_type},
        {"timestamp", timestamp},
        {"message", message},
        {"details", details}
    };

    json event_data = loadJsonFromFile("license-events.json");
    if (event_data.empty() || !event_data.contains("events")) {
        event_data = json::object();
        event_data["events"] = json::array();
    }

    event_data["events"].push_back(event_log_entry);
    saveJsonToFile("license-events.json", event_data);
}

// Helper function to load JSON from file
json LicenseRouter::loadJsonFromFile(const std::string& filename) {
    std::string filepath = data_directory + filename;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[LICENSE] Failed to open file: " << filepath << std::endl;
        return json::object();
    }

    json data;
    try {
        file >> data;
    } catch (const std::exception& e) {
        std::cerr << "[LICENSE] JSON parse error in " << filepath << ": " << e.what() << std::endl;
        return json::object();
    }
    return data;
}

// Helper function to save JSON to file
bool LicenseRouter::saveJsonToFile(const std::string& filename, const json& data) {
    std::string filepath = data_directory + filename;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[LICENSE] Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    try {
        file << data.dump(2);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[LICENSE] JSON write error to " << filepath << ": " << e.what() << std::endl;
        return false;
    }
}

// Initialize default license files if they don't exist
void LicenseRouter::initializeDefaultLicenseFiles() {
    // Initialize license-status.json
    if (!std::filesystem::exists(data_directory + "license-status.json")) {
        json default_status = {
            {"license_info", {
                {"license_type", "Enterprise"},
                {"license_id", "LIC-DEFAULT-2024-XYZ1"},
                {"activation_status", "Inactive"},
                {"start_date", std::to_string(getCurrentTimestamp())},
                {"expiry_date", std::to_string(getCurrentTimestamp() + (365 * 24 * 3600))}, // 1 year from now
                {"remaining_days", 365},
                {"health_score", 0}
            }},
            {"validity_info", {
                {"is_active", false},
                {"is_expired", false},
                {"validation_interval_days", 7}
            }},
            {"enabled_features", json::array()},
            {"last_updated", getCurrentTimestamp()}
        };
        saveJsonToFile("license-status.json", default_status);
        std::cout << "[LICENSE] Created default license-status.json" << std::endl;
    }

    // Initialize license-configuration.json
    if (!std::filesystem::exists(data_directory + "license-configuration.json")) {
        json default_config = {
            {"signature_validation_enabled", signature_validation_enabled},
            {"auto_renewal_enabled", false},
            {"validation_interval_days", 7},
            {"backup_license_servers", json::array({"https://license1.example.com", "https://license2.example.com"})},
            {"activation_methods", json::array({"license_key", "file_upload", "online_activation"})},
            {"supported_file_types", json::array({".lic", ".dat", ".key"})},
            {"max_file_size_mb", 5},
            {"timeout_seconds", 30},
            {"retry_attempts", 3},
            {"log_level", "info"}
        };
        saveJsonToFile("license-configuration.json", default_config);
        std::cout << "[LICENSE] Created default license-configuration.json" << std::endl;
    }

    // Initialize license-events.json
    if (!std::filesystem::exists(data_directory + "license-events.json")) {
        json default_events = {
            {"events", json::array()}
        };
        saveJsonToFile("license-events.json", default_events);
        std::cout << "[LICENSE] Created default license-events.json" << std::endl;
    }
}


void LicenseRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler,
                                   std::function<void(const std::string&, StructuredRouteHandler)> addStructuredRouteHandler) {
    std::cout << "LicenseRouter: Registering license routes matching frontend expectations..." << std::endl;

    // License status endpoint - GET/POST for loading license data
    addRouteHandler("/api/license/status", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleLicenseStatus(method, params, body);
    });

    // License activation endpoint - POST for all activation methods
    addRouteHandler("/api/license/activate", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleLicenseActivate(method, params, body);
    });

    // License validation endpoint - POST
    addRouteHandler("/api/license/validate", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleLicenseValidate(method, params, body);
    });

    // License configuration endpoint - GET/POST/PUT
    addRouteHandler("/api/license/configure", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleLicenseConfig(method, params, body);
    });

    // License events endpoint - GET for retrieving events
    addRouteHandler("/api/license/events", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleLicenseEvents(method, params, body);
    });

    std::cout << "LicenseRouter: All license routes registered successfully" << std::endl;
}

// License status endpoint implementation
std::string LicenseRouter::handleLicenseStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[LICENSE-STATUS] Processing license status request, method: " << method << std::endl;

    if (method == "GET" || method == "POST") {
        try {
            // Handle optional parameters from POST body
            bool includeUsageStats = false;
            bool forceRefresh = false;

            if (method == "POST" && !body.empty()) {
                json requestData = json::parse(body);
                includeUsageStats = requestData.value("include_usage_stats", false);
                forceRefresh = requestData.value("force_refresh", false);
                std::cout << "[LICENSE-STATUS] POST request with include_usage_stats=" << includeUsageStats << ", force_refresh=" << forceRefresh << std::endl;
            }

            // Load license status from file
            json licenseFileData = loadJsonFromFile("license-status.json");
            if (licenseFileData.empty()) {
                return createErrorResponse("Failed to load license status data", 500).dump();
            }

            // Extract license info
            json licenseData = licenseFileData.value("license_info", json::object());
            json validityInfo = licenseFileData.value("validity_info", json::object());

            // Extract and safely convert numeric values
            long long remainingDays = 0;
            long long healthScore = 0;
            long long validationDays = 7;

            // Safe conversion for remaining_days
            auto remainingDaysValue = licenseData.value("remaining_days", json(0));
            if (remainingDaysValue.is_string()) {
                try {
                    remainingDays = std::stoll(remainingDaysValue.get<std::string>());
                } catch (const std::exception& e) {
                    std::cerr << "[LICENSE-STATUS] Error parsing remaining_days: " << e.what() << std::endl;
                    remainingDays = 0;
                }
            } else if (remainingDaysValue.is_number()) {
                remainingDays = remainingDaysValue.get<long long>();
            }

            // Safe conversion for health_score
            auto healthScoreValue = licenseData.value("health_score", json(0));
            if (healthScoreValue.is_string()) {
                try {
                    healthScore = std::stoll(healthScoreValue.get<std::string>());
                } catch (const std::exception& e) {
                    std::cerr << "[LICENSE-STATUS] Error parsing health_score: " << e.what() << std::endl;
                    healthScore = 0;
                }
            } else if (healthScoreValue.is_number()) {
                healthScore = healthScoreValue.get<long long>();
            }

            // Safe conversion for validation_interval_days
            auto validationDaysValue = validityInfo.value("validation_interval_days", json(7));
            if (validationDaysValue.is_string()) {
                try {
                    validationDays = std::stoll(validationDaysValue.get<std::string>());
                } catch (const std::exception& e) {
                    std::cerr << "[LICENSE-STATUS] Error parsing validation_interval_days: " << e.what() << std::endl;
                    validationDays = 7;
                }
            } else if (validationDaysValue.is_number()) {
                validationDays = validationDaysValue.get<long long>();
            }

            // Safe license ID extraction for masking
            std::string licenseId = licenseData.value("license_id", "XXXX");
            std::string maskedLicenseId = "••••-••••-••••-" + licenseId.substr(std::max(0, (int)licenseId.length() - 4));

            // Format the response data to match frontend expectations
            json responseData = {
                {"license_type", licenseData.value("license_type", "Unknown")},
                {"license_id", maskedLicenseId},
                {"activation_status", licenseData.value("activation_status", "Unknown")},
                {"start_date", formatDate(licenseData.value("start_date", ""))},
                {"expiry_date", formatDate(licenseData.value("expiry_date", ""))},
                {"remaining_days", remainingDays},
                {"health_score", healthScore},
                {"is_active", validityInfo.value("is_active", false)},
                {"is_expired", validityInfo.value("is_expired", true)},
                {"status_color", getStatusColor(remainingDays, validityInfo.value("is_active", false))},
                {"validation_days", validationDays},
                {"enabled_features", licenseFileData.value("enabled_features", json::array())}
            };

            json response = createSuccessResponse({
                {"license", responseData},
                {"data_available", true},
                {"last_update", licenseFileData.value("last_updated", getCurrentTimestamp())},
                {"force_refresh", forceRefresh},
                {"timestamp", getCurrentTimestamp()}
            });

            std::cout << "[LICENSE-STATUS] License status data returned successfully" << std::endl;
            return response.dump();

        } catch (const json::parse_error& e) {
            std::cerr << "[LICENSE-STATUS] JSON parse error: " << e.what() << std::endl;
            return createErrorResponse("Invalid request body format", 400).dump();
        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-STATUS] Error: " << e.what() << std::endl;
            return createErrorResponse("Failed to retrieve license status", 500).dump();
        }
    }

    return createErrorResponse("Method not allowed. Use GET or POST for license status.", 405).dump();
}

// License activation endpoint implementation
std::string LicenseRouter::handleLicenseActivate(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[LICENSE-ACTIVATE] Processing license activation request, method: " << method << std::endl;

    if (method == "POST") {
        try {
            if (body.empty()) {
                return createErrorResponse("Request body is required for activation", 400).dump();
            }

            json requestData = json::parse(body);

            std::string activationMethod = requestData.value("activation_method", "");
            
            if (activationMethod.empty()) {
                return createErrorResponse("activation_method is required", 400).dump();
            }

            // Process the activation based on the method
            json activationResult = processLicenseActivation(requestData);

            if (activationResult.value("success", false)) {
                // Update license status file upon successful activation
                json licenseStatusData = loadJsonFromFile("license-status.json");
                if (!licenseStatusData.empty()) {
                    licenseStatusData["license_info"]["license_id"] = activationResult.value("license_id", licenseStatusData["license_info"].value("license_id", "UNKNOWN"));
                    licenseStatusData["license_info"]["license_type"] = activationResult.value("license_type", licenseStatusData["license_info"].value("license_type", "UNKNOWN"));
                    licenseStatusData["license_info"]["start_date"] = activationResult.value("activation_timestamp", licenseStatusData["license_info"].value("start_date", "0"));
                    licenseStatusData["license_info"]["expiry_date"] = activationResult.value("expiry_timestamp", licenseStatusData["license_info"].value("expiry_date", "0"));
                    
                    // Safely handle remaining_days conversion
                    auto remainingDaysFromResult = activationResult.value("remaining_days", json(365));
                    if (remainingDaysFromResult.is_number()) {
                        licenseStatusData["license_info"]["remaining_days"] = remainingDaysFromResult.get<long long>();
                    } else {
                        licenseStatusData["license_info"]["remaining_days"] = 365;
                    }
                    
                    licenseStatusData["license_info"]["activation_status"] = "Active";
                    licenseStatusData["validity_info"]["is_active"] = true;
                    licenseStatusData["validity_info"]["is_expired"] = false;
                    licenseStatusData["last_updated"] = getCurrentTimestamp();

                    saveJsonToFile("license-status.json", licenseStatusData);
                } else {
                     std::cerr << "[LICENSE-ACTIVATE] Warning: Could not load license-status.json to update after activation." << std::endl;
                }

                // Log activation event
                logLicenseEvent("activation_success", "License activated successfully", activationResult);

                json response = createSuccessResponse({
                    {"activation_successful", true},
                    {"license_status", "Active"},
                    {"message", "License activated successfully"},
                    {"activation_details", activationResult}
                });

                std::cout << "[LICENSE-ACTIVATE] License activation completed successfully" << std::endl;
                return response.dump();
            } else {
                std::string errorMessage = activationResult.value("message", "Unknown activation error");
                logLicenseEvent("activation_failed", errorMessage, activationResult);
                return createErrorResponse("Activation failed: " + errorMessage, 400).dump();
            }

        } catch (const json::parse_error& e) {
            std::cerr << "[LICENSE-ACTIVATE] JSON parse error: " << e.what() << std::endl;
            return createErrorResponse("Invalid request body format", 400).dump();
        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-ACTIVATE] Exception: " << e.what() << std::endl;
            logLicenseEvent("activation_exception", "License activation processing failed", {{"error", e.what()}});
            return createErrorResponse("License activation processing failed", 500).dump();
        }
    }

    return createErrorResponse("Method not allowed. Use POST for license activation.", 405).dump();
}

// License validation endpoint implementation
std::string LicenseRouter::handleLicenseValidate(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[LICENSE-VALIDATE] Processing license validation request, method: " << method << std::endl;

    if (method == "POST") {
        try {
            json validationRequestData;
            if (!body.empty()) {
                validationRequestData = json::parse(body);
            }

            // Perform license validation
            json validationResult = processLicenseValidation(validationRequestData);

            // Update status file if validation is performed
            if (!validationResult.empty()) {
                json licenseStatusData = loadJsonFromFile("license-status.json");
                if (!licenseStatusData.empty()) {
                    licenseStatusData["validity_info"]["is_active"] = validationResult.value("is_valid", false);
                    licenseStatusData["validity_info"]["is_expired"] = !validationResult.value("is_valid", false);
                    
                    // Safely handle days_remaining conversion
                    auto daysRemainingFromResult = validationResult.value("days_remaining", json(0));
                    if (daysRemainingFromResult.is_number()) {
                        licenseStatusData["license_info"]["remaining_days"] = daysRemainingFromResult.get<long long>();
                    } else {
                        licenseStatusData["license_info"]["remaining_days"] = 0;
                    }
                    
                    licenseStatusData["last_updated"] = getCurrentTimestamp();

                    // Safely get remaining days for status color calculation
                    long long remainingDaysForColor = 0;
                    auto remainingDaysValue = licenseStatusData["license_info"].value("remaining_days", json(0));
                    if (remainingDaysValue.is_number()) {
                        remainingDaysForColor = remainingDaysValue.get<long long>();
                    }

                    // Update status color based on new validity info
                    licenseStatusData["status_color"] = getStatusColor(remainingDaysForColor, licenseStatusData["validity_info"].value("is_active", false));

                    saveJsonToFile("license-status.json", licenseStatusData);
                } else {
                    std::cerr << "[LICENSE-VALIDATE] Warning: Could not load license-status.json to update after validation." << std::endl;
                }
            }

            json response = createSuccessResponse({
                {"message", "License validation completed"},
                {"validation_result", validationResult}
            });

            std::cout << "[LICENSE-VALIDATE] Validation completed successfully" << std::endl;
            return response.dump();

        } catch (const json::parse_error& e) {
            std::cerr << "[LICENSE-VALIDATE] JSON parse error: " << e.what() << std::endl;
            return createErrorResponse("Invalid request body format", 400).dump();
        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-VALIDATE] Exception: " << e.what() << std::endl;
            logLicenseEvent("validation_exception", "License validation failed", {{"error", e.what()}});
            return createErrorResponse("License validation failed", 500).dump();
        }
    }

    return createErrorResponse("Method not allowed. Use POST for license validation.", 405).dump();
}

// License configuration endpoint implementation
std::string LicenseRouter::handleLicenseConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[LICENSE-CONFIG] Processing license configuration request, method: " << method << std::endl;

    if (method == "GET") {
        try {
            json configData = loadJsonFromFile("license-configuration.json");
            if (configData.empty()) {
                return createErrorResponse("Failed to load license configuration", 500).dump();
            }

            json response = createSuccessResponse({
                {"configuration", configData}
            });

            return response.dump();

        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-CONFIG] Error retrieving configuration: " << e.what() << std::endl;
            return createErrorResponse("Failed to retrieve license configuration", 500).dump();
        }

    } else if (method == "POST" || method == "PUT") {
        try {
            if (body.empty()) {
                return createErrorResponse("Request body is required for configuration update", 400).dump();
            }

            json requestData = json::parse(body);

            // Handle specific toggle actions from frontend
            if (requestData.contains("action")) {
                std::string action = requestData["action"];
                
                if (action == "toggle_signature_validation") {
                    if (requestData.contains("enabled")) {
                        bool enabled = requestData["enabled"];
                        signature_validation_enabled = enabled;

                        json configData = loadJsonFromFile("license-configuration.json");
                        if (!configData.empty()) {
                            configData["signature_validation_enabled"] = enabled;
                            saveJsonToFile("license-configuration.json", configData);
                        }

                        logLicenseEvent("config_update", "Signature validation toggled", {{"enabled", enabled}});
                        
                        return createSuccessResponse({
                            {"message", "Signature validation setting updated"},
                            {"signature_validation_enabled", enabled},
                            {"config_updated", true}
                        }).dump();
                    }
                } else if (action == "update_configuration") {
                    // Handle bulk configuration update
                    json configData = requestData.value("configuration", json::object());
                    json currentConfig = loadJsonFromFile("license-configuration.json");
                    
                    if (currentConfig.empty()) {
                        currentConfig = json::object();
                    }

                    // Update configuration with new values
                    for (auto const& [key, val] : configData.items()) {
                        currentConfig[key] = val;
                        if (key == "signature_validation_enabled") {
                            signature_validation_enabled = val;
                        }
                    }

                    if (saveJsonToFile("license-configuration.json", currentConfig)) {
                        logLicenseEvent("config_update", "License configuration updated", {{"updated_fields", configData}});
                        
                        return createSuccessResponse({
                            {"message", "Configuration updated successfully"},
                            {"config_updated", true},
                            {"updated_data", configData}
                        }).dump();
                    } else {
                        return createErrorResponse("Failed to save configuration", 500).dump();
                    }
                } else if (action == "reset_configuration") {
                    // Reset to default configuration
                    json defaultConfig = {
                        {"signature_validation_enabled", true},
                        {"auto_renewal_enabled", false},
                        {"validation_interval_days", 7},
                        {"backup_license_servers", json::array({"https://license1.example.com", "https://license2.example.com"})},
                        {"activation_methods", json::array({"license_key", "file_upload", "online_activation"})},
                        {"supported_file_types", json::array({".lic", ".dat", ".key"})},
                        {"max_file_size_mb", 5},
                        {"timeout_seconds", 30},
                        {"retry_attempts", 3},
                        {"log_level", "info"}
                    };
                    
                    saveJsonToFile("license-configuration.json", defaultConfig);
                    signature_validation_enabled = true;
                    
                    logLicenseEvent("config_reset", "Configuration reset to defaults", defaultConfig);
                    
                    return createSuccessResponse({
                        {"message", "Configuration reset to defaults"},
                        {"config_updated", true},
                        {"configuration", defaultConfig}
                    }).dump();
                }
            }

            // General configuration update
            json currentConfig = loadJsonFromFile("license-configuration.json");
            if (currentConfig.empty()) {
                currentConfig = json::object();
            }

            for (auto const& [key, val] : requestData.items()) {
                if (key != "action") {
                    currentConfig[key] = val;
                }
            }

            if (saveJsonToFile("license-configuration.json", currentConfig)) {
                logLicenseEvent("config_update", "License configuration updated", {{"updated_fields", requestData}});
                
                return createSuccessResponse({
                    {"message", "License configuration updated successfully"},
                    {"config_updated", true},
                    {"updated_data", requestData}
                }).dump();
            } else {
                return createErrorResponse("Failed to save updated license configuration", 500).dump();
            }

        } catch (const json::parse_error& e) {
            std::cerr << "[LICENSE-CONFIG] JSON parse error: " << e.what() << std::endl;
            return createErrorResponse("Invalid request body format", 400).dump();
        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-CONFIG] Exception: " << e.what() << std::endl;
            logLicenseEvent("config_exception", "License configuration update failed", {{"error", e.what()}});
            return createErrorResponse("License configuration update failed", 500).dump();
        }
    }

    return createErrorResponse("Method not allowed. Use GET, POST, or PUT for license configuration.", 405).dump();
}

// License events endpoint implementation
std::string LicenseRouter::handleLicenseEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[LICENSE-EVENTS] Processing license events request, method: " << method << std::endl;

    if (method == "GET") {
        try {
            json eventData = loadJsonFromFile("license-events.json");
            if (eventData.empty()) {
                return createErrorResponse("Failed to load license events", 500).dump();
            }

            json response = createSuccessResponse({
                {"events", eventData.value("events", json::array())}
            });

            return response.dump();

        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-EVENTS] Error retrieving events: " << e.what() << std::endl;
            return createErrorResponse("Failed to retrieve license events", 500).dump();
        }
    } else if (method == "POST") {
        // Handle POST for logging events directly if needed, similar to logLicenseEvent
        try {
            if (body.empty()) {
                return createErrorResponse("Request body is required for event logging", 400).dump();
            }

            json eventData = json::parse(body);

            std::string eventType = eventData.value("event_type", "unknown");
            std::string message = eventData.value("message", "No message provided");
            json details = eventData.value("details", json::object());

            logLicenseEvent(eventType, message, details);

            return createSuccessResponse({
                {"message", "License event logged successfully"},
                {"event_type", eventType},
                {"logged", true}
            }).dump();

        } catch (const json::parse_error& e) {
            std::cerr << "[LICENSE-EVENTS] JSON parse error: " << e.what() << std::endl;
            return createErrorResponse("Invalid request body format", 400).dump();
        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-EVENTS] Exception: " << e.what() << std::endl;
            return createErrorResponse("License event logging failed", 500).dump();
        }
    } else if (method == "DELETE") {
        // Handle DELETE for clearing events
        try {
            json emptyEvents = {{"events", json::array()}};
            if (saveJsonToFile("license-events.json", emptyEvents)) {
                return createSuccessResponse({
                    {"message", "License events cleared successfully"},
                    {"cleared", true}
                }).dump();
            } else {
                return createErrorResponse("Failed to clear license events", 500).dump();
            }
        } catch (const std::exception& e) {
            std::cerr << "[LICENSE-EVENTS] Exception clearing events: " << e.what() << std::endl;
            return createErrorResponse("License event clearing failed", 500).dump();
        }
    }

    return createErrorResponse("Method not allowed. Use GET to retrieve events or POST to log events.", 405).dump();
}


// --- License Data Processing Methods ---

// Method to simulate processing license activation requests from different sources
json LicenseRouter::processLicenseActivation(const json& activationData) {
    std::string activationMethod = activationData.value("activation_method", "");
    json activationContent = activationData.value("activation_content", json::object());

    if (activationMethod == "license_key") {
        return handleLicenseKeyActivation(activationContent);
    } else if (activationMethod == "file_upload") {
        return handleFileUploadActivation(activationContent);
    } else if (activationMethod == "online_activation") {
        return handleOnlineActivation(activationContent);
    } else {
        return {{"success", false}, {"message", "Unsupported activation method: " + activationMethod}};
    }
}

// Method to simulate processing license validation requests
json LicenseRouter::processLicenseValidation(const json& validationData) {
    // For file-based system, we rely on the data in license-status.json for current validity
    // This method might be used for periodic background checks or external validation calls.

    json licenseStatusData = loadJsonFromFile("license-status.json");
    if (licenseStatusData.empty()) {
        return {{"success", false}, {"message", "Could not load license status for validation"}};
    }

    json licenseInfo = licenseStatusData.value("license_info", json::object());
    json validityInfo = licenseStatusData.value("validity_info", json::object());

    long long current_timestamp = getCurrentTimestamp();
    long long expiry_timestamp = 0;
    try {
        expiry_timestamp = std::stoll(licenseInfo.value("expiry_date", "0"));
    } catch (const std::exception& e) {
        std::cerr << "[LICENSE-VALIDATE] Error parsing expiry_date: " << e.what() << std::endl;
        return {{"success", false}, {"message", "Invalid expiry date format"}};
    }

    bool is_valid = validityInfo.value("is_active", false) && (current_timestamp < expiry_timestamp);
    long long days_remaining = 0;
    if (is_valid) {
        days_remaining = std::max(0LL, (expiry_timestamp - current_timestamp) / (24 * 3600));
    }

    // Update the file with latest validation info if necessary (e.g., if called periodically)
    if (!validationData.empty() || (current_timestamp % (7 * 24 * 3600) == 0)) { // Example: re-validate every 7 days
        licenseStatusData["validity_info"]["is_active"] = is_valid;
        licenseStatusData["validity_info"]["is_expired"] = !is_valid;
        licenseStatusData["license_info"]["remaining_days"] = days_remaining;
        licenseStatusData["last_updated"] = current_timestamp;
        saveJsonToFile("license-status.json", licenseStatusData);
    }

    return {
        {"is_valid", is_valid},
        {"license_id", licenseInfo.value("license_id", "N/A")},
        {"validation_timestamp", current_timestamp},
        {"expiry_timestamp", expiry_timestamp},
        {"days_remaining", days_remaining},
        {"validation_method", "file_check"},
        {"health_status", is_valid ? "healthy" : "expired"}
    };
}

// --- License Activation Method Handlers ---

json LicenseRouter::handleLicenseKeyActivation(const json& activationContent) {
    std::cout << "[LICENSE-ACTIVATE] Processing license key activation..." << std::endl;

    // Handle both direct string and object formats
    std::string licenseKey;
    if (activationContent.is_string()) {
        licenseKey = activationContent.get<std::string>();
    } else if (activationContent.is_object()) {
        licenseKey = activationContent.value("license_key", "");
    }

    if (licenseKey.empty()) {
        return {{"success", false}, {"message", "License key is required"}};
    }

    if (!validateLicenseKey(licenseKey)) {
        return {{"success", false}, {"message", "Invalid license key format"}};
    }

    // Simulate license key validation and update
    // In a real scenario, this would involve checking against a license server or a more robust local validation.

    // Mock data for a valid key
    std::string generated_license_id = "LIC-" + licenseKey.substr(licenseKey.length() - 4);
    std::string license_type = "Standard License"; // Default license type
    long long start_ts = getCurrentTimestamp();
    long long expiry_ts = start_ts + (365 * 24 * 3600); // 1 year validity

    return {
        {"success", true},
        {"message", "License key activated successfully"},
        {"license_id", generated_license_id},
        {"license_type", license_type},
        {"activation_timestamp", std::to_string(start_ts)},
        {"expiry_timestamp", std::to_string(expiry_ts)},
        {"remaining_days", 365}
    };
}

json LicenseRouter::handleFileUploadActivation(const json& activationContent) {
    std::cout << "[LICENSE-ACTIVATE] Processing file upload activation..." << std::endl;

    std::string filename = activationContent.value("filename", "");
    std::string fileContentBase64 = activationContent.value("file_content_base64", ""); // Assuming base64 encoded content
    bool validateSignature = activationContent.value("validate_signature", false);
    int fileSize = activationContent.value("file_size", 0);

    if (filename.empty() || fileContentBase64.empty()) {
        return {{"success", false}, {"message", "License file and content are required"}};
    }

    // Decode Base64 content
    std::string fileContent = base64_decode(fileContentBase64); // Assuming a base64 decode utility function

    if (fileSize > 5 * 1024 * 1024) { // 5MB limit
        return {{"success", false}, {"message", "License file is too large (max 5MB)"}};
    }

    if (!validateLicenseFile(fileContent)) {
        return {{"success", false}, {"message", "Invalid license file format or content"}};
    }

    std::cout << "[LICENSE-ACTIVATE] Processing license file: " << filename << " (size: " << fileSize << " bytes)" << std::endl;

    if (validateSignature && signature_validation_enabled) {
        std::cout << "[LICENSE-ACTIVATE] Signature validation requested and enabled. Performing check..." << std::endl;
        // Placeholder for actual signature validation logic
        bool signatureValid = true; // Assume valid for now
        if (!signatureValid) {
            return {{"success", false}, {"message", "License file signature validation failed"}};
        }
    }

    // Update license state from file content (simplified for example)
    // In a real implementation, parse the file content to extract license details.
    std::string generated_license_id = "LIC-FILE-" + std::to_string(getCurrentTimestamp()).substr(5, 4);
    std::string license_type = "File-based License";
    long long start_ts = getCurrentTimestamp();
    long long expiry_ts = start_ts + (365 * 24 * 3600); // 1 year validity

    return {
        {"success", true},
        {"message", "License file activated successfully"},
        {"license_id", generated_license_id},
        {"license_type", license_type},
        {"filename", filename},
        {"file_size", fileSize},
        {"signature_validated", validateSignature && signature_validation_enabled},
        {"activation_timestamp", std::to_string(start_ts)},
        {"expiry_timestamp", std::to_string(expiry_ts)},
        {"remaining_days", 365}
    };
}

json LicenseRouter::handleOnlineActivation(const json& activationContent) {
    std::cout << "[LICENSE-ACTIVATE] Processing online activation..." << std::endl;

    std::string productCode = activationContent.value("product_code", "");
    std::string email = activationContent.value("email", "");

    if (productCode.empty() || email.empty()) {
        return {{"success", false}, {"message", "Product code and email are required for online activation"}};
    }

    // Simulate online license lookup (e.g., calling an external service)
    std::cout << "[LICENSE-ACTIVATE] Simulating online lookup for email: " << email << ", Product code: " << productCode << std::endl;

    // Mock successful online activation response
    std::string generated_license_id = "LIC-ONLINE-" + std::to_string(std::hash<std::string>{}(email) % 10000);
    std::string license_type = "Online Activated License";
    long long start_ts = getCurrentTimestamp();
    long long expiry_ts = start_ts + (365 * 24 * 3600); // 1 year validity

    return {
        {"success", true},
        {"message", "Online activation completed successfully"},
        {"license_id", generated_license_id},
        {"product_code", productCode},
        {"email", email},
        {"license_type", license_type},
        {"activation_timestamp", std::to_string(start_ts)},
        {"expiry_timestamp", std::to_string(expiry_ts)},
        {"remaining_days", 365}
    };
}

// --- Helper methods for response generation ---

json LicenseRouter::createSuccessResponse(const json& data) {
    json response = {
        {"success", true},
        {"timestamp", getCurrentTimestamp()}
    };

    // Merge provided data into the response
    for (auto const& [key, val] : data.items()) {
        response[key] = val;
    }
    return response;
}

json LicenseRouter::createErrorResponse(const std::string& message, int code) {
    return {
        {"success", false},
        {"error", message},
        {"error_code", code},
        {"timestamp", getCurrentTimestamp()}
    };
}

// --- Validation Helpers ---

bool LicenseRouter::validateRequestData(const json& data) {
    if (data.empty()) {
        return false;
    }

    // Basic check for action and activation method/content if it's an activation request
    if (data.contains("action") && data["action"].is_string() && !data["action"].empty()) {
        if (data["action"].get<std::string>().find("activation") != std::string::npos) {
            if (!data.contains("activation_method") || !data["activation_method"].is_string() || data["activation_method"].empty() ||
                !data.contains("activation_content") || !data["activation_content"].is_object()) {
                return false;
            }
        }
    } else {
        return false; // Action is required
    }

    return true;
}

bool LicenseRouter::validateLicenseKey(const std::string& licenseKey) {
    if (licenseKey.length() < 12) return false; // Minimum length

    // Check for a basic format like XXXX-XXXX-XXXX-XXXX or similar alphanumeric pattern
    int dash_count = 0;
    for (char c : licenseKey) {
        if (c == '-') {
            dash_count++;
        } else if (!std::isalnum(c)) {
            return false; // Allow alphanumeric and dashes
        }
    }
    // Basic structure check: e.g., 3 dashes for 4 segments
    return dash_count == 3;
}

bool LicenseRouter::validateLicenseFile(const std::string& fileContent) {
    if (fileContent.length() < 20) return false; // Minimum content length

    // Check for common license file markers or patterns
    bool has_marker = (fileContent.find("LICENSE_DATA") != std::string::npos ||
                       fileContent.find("ACTIVATION_CODE") != std::string::npos ||
                       fileContent.find("-----BEGIN LICENSE-----") != std::string::npos);

    // Check for presence of typical license fields (e.g., key, expiry)
    bool has_key_like = (fileContent.find("license_key") != std::string::npos || fileContent.find("key=") != std::string::npos);
    bool has_expiry_like = (fileContent.find("expiry") != std::string::npos || fileContent.find("valid_until") != std::string::npos);

    return has_marker || (has_key_like && has_expiry_like);
}

// Dummy base64 decode function (replace with a proper implementation)
// This is a placeholder and needs a real base64 decoding library or implementation.
std::string LicenseRouter::base64_decode(const std::string& s) {
    // Example placeholder - replace with actual implementation
    // For demonstration, let's assume it just returns the string if it's not empty.
    // In a real scenario, you would use a library like OpenSSL or a custom decoder.
    if (s.empty()) return "";
    // This is NOT a real base64 decode. Replace with a proper implementation.
    return s;
}