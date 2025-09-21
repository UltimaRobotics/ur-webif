#pragma once

#include <string>
#include <map>
#include <chrono>
#include <iostream>
#include <functional>
#include <nlohmann/json.hpp>

/**
 * Structure containing complete API request information
 */
struct ApiRequest {
    // Request identification
    std::string route;              // API route (e.g., "/api/login")
    std::string method;             // HTTP method (GET, POST, etc.)
    std::string source_ip;          // Client IP address
    std::string user_agent;         // Client user agent

    // Request content
    std::string body;               // Raw request body
    nlohmann::json json_data;       // Parsed JSON data (if valid JSON)
    std::map<std::string, std::string> params;  // URL parameters
    std::map<std::string, std::string> headers;     // Request headers

    // Request metadata
    std::chrono::system_clock::time_point timestamp;  // Request timestamp
    std::string request_id;         // Unique request identifier
    size_t content_length;          // Content length
    bool is_json_valid;             // Whether JSON parsing succeeded

    // Constructor
    ApiRequest() : content_length(0), is_json_valid(false) {
        timestamp = std::chrono::system_clock::now();
    }

    // Helper method to generate unique request ID
    void generateRequestId() {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            timestamp.time_since_epoch()).count();
        request_id = "req_" + std::to_string(now) + "_" + std::to_string(rand() % 10000);
    }

    // Helper method to parse JSON safely
    void parseJsonBody() {
        is_json_valid = false;
        if (!body.empty() && (body[0] == '{' || body[0] == '[')) {
            try {
                json_data = nlohmann::json::parse(body);
                is_json_valid = true;
            } catch (const std::exception& e) {
                // JSON parsing failed, keep is_json_valid false
            }
        }
    }

    // Method to log request details
    void logRequest() const {
        std::cout << "=== API Request Details ===" << std::endl;
        std::cout << "Request ID: " << request_id << std::endl;
        std::cout << "Route: " << route << std::endl;
        std::cout << "Method: " << method << std::endl;
        std::cout << "Source IP: " << source_ip << std::endl;
        std::cout << "Content Length: " << content_length << " bytes" << std::endl;

        if (!params.empty()) {
            std::cout << "Parameters: ";
            for (const auto& param : params) {
                std::cout << param.first << "=" << param.second << " ";
            }
            std::cout << std::endl;
        }

        if (!body.empty()) {
            std::cout << "Request Body: " << body << std::endl;

            if (is_json_valid) {
                std::cout << "Request JSON (formatted):" << std::endl;
                std::cout << json_data.dump(2) << std::endl;
            }
        }
        std::cout << "=========================" << std::endl;
    }
};

/**
 * Structure for API response
 */
struct ApiResponse {
    std::string body;
    int status_code;
    int http_status; // Added missing http_status member
    nlohmann::json data; // Added missing data member
    std::map<std::string, std::string> headers;
    std::string content_type; // Added missing content_type member
    bool success;

    ApiResponse() : status_code(200), http_status(200), content_type("application/json"), success(true) {} // Initialize http_status and content_type

    // Helper to set JSON response
    void setJsonResponse(const nlohmann::json& json_data, int http_code = 200) {
        data = json_data; // Assign to data member
        body = json_data.dump();
        status_code = http_code;
        http_status = http_code; // Set http_status
        headers["Content-Type"] = "application/json";
        success = (http_code >= 200 && http_code < 300);
    }

    // Helper to set error response
    void setErrorResponse(const std::string& message, int code = 500) {
        status_code = code;
        http_status = code; // Set http_status for error as well
        content_type = "application/json";
        nlohmann::json error_json = {
            {"success", false},
            {"error", message},
            {"status_code", code}
        };
        body = error_json.dump();
        success = false;
    }
};

/**
 * Route processor function type
 */
using RouteProcessor = std::function<ApiResponse(const ApiRequest&)>;