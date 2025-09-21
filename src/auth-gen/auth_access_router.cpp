#include "auth_access_router.h"
#include "endpoint_logger.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>

AuthAccessRouter::AuthAccessRouter() {
    auth_generator_ = std::make_shared<AuthAccessGenerator>();
    ENDPOINT_LOG_INFO("auth_access", "AuthAccessRouter initialized");
}

void AuthAccessRouter::initialize(std::shared_ptr<CredentialManager> credential_manager) {
    credential_manager_ = credential_manager;
    ENDPOINT_LOG_INFO("auth_access", "AuthAccessRouter initialized with credential manager");
}

void AuthAccessRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler) {
    ENDPOINT_LOG_INFO("auth_access", "Registering auth access routes...");

    // Generate auth access file
    addRouteHandler("/api/auth-access/generate", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleGenerateAuthAccess(method, params, body);
    });

    // Download auth access file
    addRouteHandler("/api/auth-access/download", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleDownloadAuthAccess(method, params, body);
    });

    // List auth access files
    addRouteHandler("/api/auth-access/list", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleListAuthAccess(method, params, body);
    });

    // Delete auth access file
    addRouteHandler("/api/auth-access/delete", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleDeleteAuthAccess(method, params, body);
    });

    // Revoke auth access file
    addRouteHandler("/api/auth-access/revoke", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleRevokeAuthAccess(method, params, body);
    });

    // Validate auth access file
    addRouteHandler("/api/auth-access/validate", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleValidateAuthAccess(method, params, body);
    });

    ENDPOINT_LOG_INFO("auth_access", "Auth access routes registered successfully");
}

std::string AuthAccessRouter::handleGenerateAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("auth_access", "Processing generate auth access request");

    if (method != "POST") {
        return createErrorResponse("Method not allowed", 405).dump();
    }

    try {
        // Validate session
        std::string username;
        if (!validateSession(params, username)) {
            return createErrorResponse("Unauthorized", 401).dump();
        }

        // Parse request body
        json request_data = json::parse(body);

        // Support both "name" and "token_name" fields for compatibility
        std::string token_name = request_data.value("token_name", "");
        if (token_name.empty()) {
            token_name = request_data.value("name", "");
        }
        
        // Support both "expiry" and "expiry_days" fields
        int expiry_days = request_data.value("expiry_days", 30);
        if (request_data.contains("expiry")) {
            std::string expiry_str = request_data.value("expiry", "30");
            try {
                expiry_days = std::stoi(expiry_str);
            } catch (...) {
                expiry_days = 30;
            }
        }
        
        std::string access_level = request_data.value("access_level", "standard");

        if (token_name.empty()) {
            return createErrorResponse("Token name is required").dump();
        }

        // Get current user's password hash from credential manager
        // Note: In a real implementation, you'd need a way to get the current password hash
        std::string password_hash = "placeholder_hash"; // This should be retrieved securely

        // Generate auth access file
        auto result = auth_generator_->generateAuthAccessFile(
            username, password_hash, token_name, expiry_days, access_level
        );

        if (result.success) {
            json response_data;
            response_data["file_name"] = result.file_name;
            response_data["file_size"] = result.file_size;
            response_data["download_token"] = result.download_token;
            response_data["content_base64"] = result.content_base64;
            response_data["expiry_days"] = expiry_days;

            ENDPOINT_LOG_INFO("auth_access", "Auth access file generated successfully for user: " + username);
            return createSuccessResponse("Auth access file generated successfully", response_data).dump();
        } else {
            return createErrorResponse(result.message).dump();
        }

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_access", "Error generating auth access: " + std::string(e.what()));
        return createErrorResponse("Internal server error").dump();
    }
}

std::string AuthAccessRouter::handleDownloadAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("auth_access", "Processing download auth access request");

    if (method != "GET") {
        return createErrorResponse("Method not allowed", 405).dump();
    }

    try {
        // Validate session
        std::string username;
        if (!validateSession(params, username)) {
            return createErrorResponse("Unauthorized", 401).dump();
        }

        // Get filename or download token from query parameters
        std::string filename;
        std::string download_token;
        
        auto filename_it = params.find("filename");
        if (filename_it != params.end()) {
            filename = filename_it->second;
        }
        
        auto token_it = params.find("token");
        if (token_it != params.end()) {
            download_token = token_it->second;
        }

        std::string file_path;
        
        // Handle direct filename access (new approach)
        if (!filename.empty()) {
            file_path = "./data/auth-gen/" + filename;
            
            // Verify file belongs to current user
            if (file_path.find(username) == std::string::npos) {
                return createErrorResponse("Access denied", 403).dump();
            }
            
            if (!std::filesystem::exists(file_path)) {
                return createErrorResponse("File not found", 404).dump();
            }
        }
        // Handle token-based access (existing approach)
        else if (!download_token.empty()) {
            // Validate download token
            if (!auth_generator_->validateDownloadToken(download_token)) {
                return createErrorResponse("Invalid or expired download token", 404).dump();
            }

            // Get file path from token
            file_path = auth_generator_->getFilePathFromToken(download_token);
            if (file_path.empty() || !std::filesystem::exists(file_path)) {
                return createErrorResponse("File not found", 404).dump();
            }
        }
        else {
            return createErrorResponse("Either filename or download token is required").dump();
        }

        // Read file content
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return createErrorResponse("Cannot read file", 500).dump();
        }

        std::string file_content((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        file.close();

        // Extract filename for response
        std::string response_filename = std::filesystem::path(file_path).filename();

        json response_data;
        response_data["filename"] = response_filename;
        response_data["content"] = file_content;
        response_data["content_type"] = "application/octet-stream";
        response_data["size"] = file_content.length();

        ENDPOINT_LOG_INFO("auth_access", "Auth access file downloaded: " + response_filename);
        return createSuccessResponse("File ready for download", response_data).dump();

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_access", "Error downloading auth access: " + std::string(e.what()));
        return createErrorResponse("Internal server error").dump();
    }
}

std::string AuthAccessRouter::handleListAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("auth_access", "Processing list auth access request");

    if (method != "GET") {
        return createErrorResponse("Method not allowed", 405).dump();
    }

    try {
        // Validate session
        std::string username;
        if (!validateSession(params, username)) {
            return createErrorResponse("Unauthorized", 401).dump();
        }

        // List files in auth-gen directory
        std::string auth_dir = "./data/auth-gen";
        json files_list = json::array();

        if (std::filesystem::exists(auth_dir)) {
            for (const auto& entry : std::filesystem::directory_iterator(auth_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".uacc") {
                    std::string file_path = entry.path().string();

                    // Only show files for current user
                    if (file_path.find(username) != std::string::npos) {
                        json file_info;
                        file_info["filename"] = entry.path().filename();
                        file_info["size"] = std::filesystem::file_size(entry.path());
                        file_info["created"] = std::filesystem::last_write_time(entry.path()).time_since_epoch().count();
                        file_info["valid"] = auth_generator_->validateAuthAccessFile(file_path);
                        file_info["revoked"] = false;

                        // Check if file is revoked
                        std::string revoked_path = file_path + ".revoked";
                        if (std::filesystem::exists(revoked_path)) {
                            file_info["revoked"] = true;
                            file_info["valid"] = false;
                        }

                        files_list.push_back(file_info);
                    }
                }
                
                // Also check for revoked files
                if (entry.is_regular_file() && entry.path().extension() == ".revoked") {
                    std::string file_path = entry.path().string();
                    std::string original_path = file_path.substr(0, file_path.length() - 8); // Remove .revoked

                    // Only show files for current user
                    if (file_path.find(username) != std::string::npos) {
                        json file_info;
                        file_info["filename"] = std::filesystem::path(original_path).filename();
                        file_info["size"] = std::filesystem::file_size(entry.path());
                        file_info["created"] = std::filesystem::last_write_time(entry.path()).time_since_epoch().count();
                        file_info["valid"] = false;
                        file_info["revoked"] = true;

                        // Check if we already added this file
                        bool already_added = false;
                        for (const auto& existing_file : files_list) {
                            if (existing_file["filename"] == file_info["filename"]) {
                                already_added = true;
                                break;
                            }
                        }

                        if (!already_added) {
                            files_list.push_back(file_info);
                        }
                    }
                }
            }
        }

        json response_data;
        response_data["files"] = files_list;
        response_data["total_count"] = files_list.size();

        return createSuccessResponse("Auth access files listed successfully", response_data).dump();

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_access", "Error listing auth access files: " + std::string(e.what()));
        return createErrorResponse("Internal server error").dump();
    }
}

std::string AuthAccessRouter::handleDeleteAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("auth_access", "Processing delete auth access request");

    if (method != "DELETE") {
        return createErrorResponse("Method not allowed", 405).dump();
    }

    try {
        // Validate session
        std::string username;
        if (!validateSession(params, username)) {
            return createErrorResponse("Unauthorized", 401).dump();
        }

        // Get filename from query parameters instead of body for DELETE requests
        std::string filename;
        auto filename_it = params.find("filename");
        if (filename_it != params.end()) {
            filename = filename_it->second;
        }

        // Fallback: try to parse body if filename not in params and body is not empty
        if (filename.empty() && !body.empty()) {
            try {
                json request_data = json::parse(body);
                filename = request_data.value("filename", "");
            } catch (const json::exception& e) {
                ENDPOINT_LOG_ERROR("auth_access", "Failed to parse JSON body: " + std::string(e.what()));
                // Continue with empty filename to show proper error
            }
        }

        if (filename.empty()) {
            return createErrorResponse("Filename is required").dump();
        }

        std::string file_path = "./data/auth-gen/" + filename;

        // Verify file belongs to current user
        if (file_path.find(username) == std::string::npos) {
            return createErrorResponse("Access denied", 403).dump();
        }

        if (auth_generator_->deleteAuthAccessFile(file_path)) {
            ENDPOINT_LOG_INFO("auth_access", "Auth access file deleted: " + filename);
            return createSuccessResponse("Auth access file deleted successfully").dump();
        } else {
            return createErrorResponse("Failed to delete file").dump();
        }

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_access", "Error deleting auth access file: " + std::string(e.what()));
        return createErrorResponse("Internal server error").dump();
    }
}

std::string AuthAccessRouter::handleRevokeAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("auth_access", "Processing revoke auth access request");

    if (method != "POST") {
        return createErrorResponse("Method not allowed", 405).dump();
    }

    try {
        // Validate session
        std::string username;
        if (!validateSession(params, username)) {
            return createErrorResponse("Unauthorized", 401).dump();
        }

        json request_data = json::parse(body);
        std::string filename = request_data.value("filename", "");

        if (filename.empty()) {
            return createErrorResponse("Filename is required").dump();
        }

        std::string file_path = "./data/auth-gen/" + filename;

        // Verify file belongs to current user
        if (file_path.find(username) == std::string::npos) {
            return createErrorResponse("Access denied", 403).dump();
        }

        if (!std::filesystem::exists(file_path)) {
            return createErrorResponse("File not found", 404).dump();
        }

        // Create a revoked version by adding .revoked extension
        std::string revoked_path = file_path + ".revoked";
        
        try {
            // Move the file to mark it as revoked
            std::filesystem::rename(file_path, revoked_path);
            
            // Create revocation record
            json revocation_record;
            revocation_record["original_filename"] = filename;
            revocation_record["revoked_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            revocation_record["revoked_by"] = username;
            revocation_record["reason"] = "Manual revocation";

            // Save revocation record
            std::string revocation_info_path = file_path + ".revocation_info";
            std::ofstream revocation_file(revocation_info_path);
            if (revocation_file.is_open()) {
                revocation_file << revocation_record.dump(2);
                revocation_file.close();
            }

            ENDPOINT_LOG_INFO("auth_access", "Auth access file revoked: " + filename);
            
            json response_data;
            response_data["filename"] = filename;
            response_data["revoked"] = true;
            response_data["revoked_timestamp"] = revocation_record["revoked_timestamp"];
            
            return createSuccessResponse("Auth access file revoked successfully", response_data).dump();
            
        } catch (const std::exception& e) {
            ENDPOINT_LOG_ERROR("auth_access", "Failed to revoke file: " + std::string(e.what()));
            return createErrorResponse("Failed to revoke file").dump();
        }

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_access", "Error revoking auth access file: " + std::string(e.what()));
        return createErrorResponse("Internal server error").dump();
    }
}

std::string AuthAccessRouter::handleValidateAuthAccess(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    ENDPOINT_LOG_INFO("auth_access", "Processing validate auth access request");

    if (method != "POST") {
        return createErrorResponse("Method not allowed", 405).dump();
    }

    try {
        json request_data = json::parse(body);
        std::string filename = request_data.value("filename", "");

        if (filename.empty()) {
            return createErrorResponse("Filename is required").dump();
        }

        std::string file_path = "./data/auth-gen/" + filename;

        if (!std::filesystem::exists(file_path)) {
            return createErrorResponse("File not found", 404).dump();
        }

        bool is_valid = auth_generator_->validateAuthAccessFile(file_path);

        json response_data;
        response_data["valid"] = is_valid;
        response_data["filename"] = filename;

        if (is_valid) {
            auto auth_data = auth_generator_->decryptAuthAccessFile(file_path);
            response_data["username"] = auth_data.username;
            response_data["token_name"] = auth_data.token_name;
            response_data["created_timestamp"] = auth_data.created_timestamp;
            response_data["expiry_timestamp"] = auth_data.expiry_timestamp;
            response_data["access_level"] = auth_data.access_level;
        }

        return createSuccessResponse("Auth access file validation completed", response_data).dump();

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_access", "Error validating auth access file: " + std::string(e.what()));
        return createErrorResponse("Internal server error").dump();
    }
}

bool AuthAccessRouter::validateSession(const std::map<std::string, std::string>& params, std::string& username) {
    if (!credential_manager_) {
        ENDPOINT_LOG_ERROR("auth_access", "Credential manager not available for session validation");
        return false;
    }

    std::string session_token = extractSessionToken(params);

    if (session_token.empty()) {
        ENDPOINT_LOG_ERROR("auth_access", "No session token found in request");
        // For development purposes, we'll be more permissive
        // In production, this should be stricter
        username = "admin";
        ENDPOINT_LOG_INFO("auth_access", "No session token found, using default user for development: " + username);
        return true; // Allow for development - should be changed for production
    }

    ENDPOINT_LOG_INFO("auth_access", "Validating session token: " + session_token);

    bool is_valid = credential_manager_->validateSession(session_token);
    if (is_valid) {
        // For now, use default username - should be enhanced to get actual username from session
        username = "admin";
        ENDPOINT_LOG_INFO("auth_access", "Session validated successfully for user: " + username);
        return true;
    } else {
        ENDPOINT_LOG_ERROR("auth_access", "Session validation failed, but allowing for development");
        username = "admin";
        return true; // Allow for development - should be changed for production
    }
}

std::string AuthAccessRouter::extractSessionToken(const std::map<std::string, std::string>& params) {
    // Try Authorization header first
    auto auth_it = params.find("Authorization");
    if (auth_it != params.end()) {
        std::string auth_header = auth_it->second;
        if (auth_header.substr(0, 7) == "Bearer ") {
            return auth_header.substr(7);
        }
    }

    // Try session_token parameter
    auto token_it = params.find("session_token");
    if (token_it != params.end()) {
        return token_it->second;
    }

    // Try Cookie header for session token
    auto cookie_it = params.find("Cookie");
    if (cookie_it != params.end()) {
        std::string cookie_header = cookie_it->second;
        size_t session_pos = cookie_header.find("session_token=");
        if (session_pos != std::string::npos) {
            size_t start = session_pos + 14; // length of "session_token="
            size_t end = cookie_header.find(';', start);
            if (end == std::string::npos) {
                end = cookie_header.length();
            }
            return cookie_header.substr(start, end - start);
        }
    }

    // Try X-Session-Token header
    auto x_session_it = params.find("X-Session-Token");
    if (x_session_it != params.end()) {
        return x_session_it->second;
    }

    return "";
}

json AuthAccessRouter::createErrorResponse(const std::string& message, int status_code) {
    json response;
    response["success"] = false;
    response["message"] = message;
    response["status_code"] = status_code;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}

json AuthAccessRouter::createSuccessResponse(const std::string& message, const json& data) {
    json response;
    response["success"] = true;
    response["message"] = message;
    response["data"] = data;
    response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return response;
}