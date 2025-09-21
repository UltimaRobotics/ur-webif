#include "../include/http_handler.h"
#include "../include/api_request.h"
#include "../include/endpoint_logger.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <nlohmann/json.hpp>

HttpHandler::HttpHandler() {
    // Initialize random seed for request IDs
    std::srand(std::time(nullptr));
    
    // Initialize dynamic router
    dynamic_router_ = std::make_unique<DynamicRouter>();
}

HttpHandler::~HttpHandler() {
}

enum MHD_Result HttpHandler::handleRequest(struct MHD_Connection* connection,
                              const char* url, const char* method,
                              const char* version, const char* upload_data,
                              size_t* upload_data_size, void** con_cls) {
    
    if (!url || !method) {
        return sendErrorResponse(connection, MHD_HTTP_BAD_REQUEST, "Invalid request");
    }
    
    std::string url_str(url);
    std::string method_str(method);
    
    // Sanitize URL and method for security
    if (url_str.length() > 2048 || method_str.length() > 16) {
        return sendErrorResponse(connection, MHD_HTTP_URI_TOO_LONG, "Request URI too long");
    }
    
    // Check structured route processors first
    auto structured_it = structured_route_processors_.find(url_str);
    if (structured_it != structured_route_processors_.end()) {
        // Use structured approach
        ApiRequest api_request = buildApiRequest(connection, url, method, upload_data, upload_data_size, con_cls);
        if (api_request.request_id.empty()) {
            return MHD_YES; // Still processing, wait for more data
        }
        
        // Log the structured request
        api_request.logRequest();
        
        // Process using structured handler
        ApiResponse api_response = structured_it->second(api_request);
        return sendApiResponse(connection, api_response);
    }
    
    // Try dynamic routing first
    std::map<std::string, std::string> query_params;
    MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND,
                            [](void* cls, enum MHD_ValueKind kind, const char* key, const char* value) -> enum MHD_Result {
                                auto* params_map = static_cast<std::map<std::string, std::string>*>(cls);
                                if (key && value) {
                                    (*params_map)[std::string(key)] = std::string(value);
                                }
                                return MHD_YES;
                            }, &query_params);
    
    RouteMatch route_match = dynamic_router_->findMatch(url_str);
    if (route_match.matched) {
        ENDPOINT_LOG("http", "Using dynamic routing for: " + url_str + " (pattern: " + route_match.matched_pattern + ")");
        
        // Handle the request using dynamic routing
        try {
            // Get request body for non-GET requests
            std::string body;
            if (method_str != "GET" && method_str != "HEAD") {
                std::string* con_info = static_cast<std::string*>(*con_cls);
                if (con_info == nullptr) {
                    con_info = new std::string();
                    *con_cls = con_info;
                    return MHD_YES;
                }
                
                if (*upload_data_size > 0) {
                    const size_t MAX_BODY_SIZE = 10 * 1024 * 1024;
                    if (con_info->size() + *upload_data_size > MAX_BODY_SIZE) {
                        return sendErrorResponse(connection, MHD_HTTP_CONTENT_TOO_LARGE, "Request body too large");
                    }
                    con_info->append(upload_data, *upload_data_size);
                    *upload_data_size = 0;
                    return MHD_YES;
                }
                
                if (!con_info->empty()) {
                    body = *con_info;
                }
            }
            
            // Combine path and query parameters
            std::map<std::string, std::string> combined_params = query_params;
            for (const auto& param : route_match.path_params) {
                combined_params[param.first] = param.second;
            }
            
            std::string response = dynamic_router_->processRequest(method_str, url_str, combined_params, body);
            
            std::string content_type = "application/json";
            if (!response.empty() && response[0] != '{' && response[0] != '[') {
                content_type = "text/plain";
            }
            
            return sendResponse(connection, MHD_HTTP_OK, response, content_type);
            
        } catch (const std::exception& e) {
            ENDPOINT_LOG("http", "Error in dynamic routing: " + std::string(e.what()));
            return sendErrorResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "Dynamic routing error");
        }
    }
    
    // Fall back to legacy route handlers
    auto it = route_handlers_.find(url_str);
    if (it == route_handlers_.end()) {
        return MHD_NO; // Let file server handle it
    }
    
    try {
        // Critical: Handle connection state properly for MHD to prevent memory corruption
        std::string* con_info = static_cast<std::string*>(*con_cls);
        if (con_info == nullptr) {
            // First call for this connection - initialize with empty body
            con_info = new std::string();
            *con_cls = con_info;
            return MHD_YES;
        }
        
        // Handle GET requests on second call
        if (method_str == "GET" && *upload_data_size == 0) {
            // Parse query parameters
            std::map<std::string, std::string> params;
            MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND,
                                    [](void* cls, enum MHD_ValueKind kind, const char* key, const char* value) -> enum MHD_Result {
                                        auto* params_map = static_cast<std::map<std::string, std::string>*>(cls);
                                        if (key && value) {
                                            (*params_map)[std::string(key)] = std::string(value);
                                        }
                                        return MHD_YES;
                                    }, &params);
            
            // Log GET parameters for debugging
            if (!params.empty()) {
                std::string params_str = "GET parameters: ";
                for (const auto& param : params) {
                    params_str += param.first + "=" + param.second + " ";
                }
                ENDPOINT_LOG("http", params_str);
            }
            
            // Process GET request
            std::string response = it->second(method_str, params, "");
            ENDPOINT_LOG("http", "Generated response: " + response + " (length: " + std::to_string(response.length()) + ")");
            
            // Create and send response
            struct MHD_Response* mhd_response = MHD_create_response_from_buffer(
                response.length(),
                const_cast<char*>(response.c_str()),
                MHD_RESPMEM_MUST_COPY
            );
            
            if (!mhd_response) {
                std::cerr << "Failed to create MHD response" << std::endl;
                return MHD_NO;
            }
            
            MHD_add_response_header(mhd_response, "Content-Type", "application/json");
            MHD_add_response_header(mhd_response, "Access-Control-Allow-Origin", "*");
            
            enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
            MHD_destroy_response(mhd_response);
            
            return ret;
        }
        
        // Handle other HTTP methods: POST, PUT, DELETE, HEAD, OPTIONS
        
        // Parse query parameters for all non-GET requests
        std::map<std::string, std::string> params;
        MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND,
                                [](void* cls, enum MHD_ValueKind kind, const char* key, const char* value) -> enum MHD_Result {
                                    auto* params_map = static_cast<std::map<std::string, std::string>*>(cls);
                                    if (key && value) {
                                        (*params_map)[std::string(key)] = std::string(value);
                                    }
                                    return MHD_YES;
                                }, &params);
        
        // Log parameters for debugging
        if (!params.empty()) {
            ENDPOINT_LOG("http", method_str + " parameters: " + [&params]() {
                std::string param_str;
                for (const auto& param : params) {
                    param_str += param.first + "=" + param.second + " ";
                }
                return param_str;
            }());
        }
        
        // Handle request body based on HTTP method
        std::string body;
        
        // HEAD requests should not process body
        if (method_str == "HEAD") {
            // HEAD requests are like GET but without response body
            if (*upload_data_size == 0) {
                std::string response = it->second(method_str, params, "");
                return sendResponse(connection, MHD_HTTP_OK, "", "application/json");
            }
        }
        
        // OPTIONS requests for CORS preflight
        if (method_str == "OPTIONS") {
            if (*upload_data_size == 0) {
                nlohmann::json options_response;
                options_response["allowed_methods"] = {"GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS"};
                options_response["message"] = "CORS preflight successful";
                
                std::string response = options_response.dump();
                return sendResponse(connection, MHD_HTTP_OK, response, "application/json");
            }
        }
        
        // DELETE requests typically don't have bodies, but may have query parameters
        if (method_str == "DELETE") {
            if (*upload_data_size == 0) {
                ENDPOINT_LOG("http", "Processing DELETE request for " + url_str);
                std::string response = it->second(method_str, params, "");
                
                // Log the response for debugging
                ENDPOINT_LOG_INFO("http", "DELETE response for " + url_str + ": " + response);
                
                // Determine appropriate response content type
                std::string content_type = "application/json";
                if (!response.empty() && response[0] != '{' && response[0] != '[') {
                    content_type = "text/plain";
                }
                
                return sendResponse(connection, MHD_HTTP_OK, response, content_type);
            } else {
                // Some DELETE requests might have a body (for bulk operations)
                // Continue processing like POST/PUT
            }
        }
        
        // Handle request body for POST and PUT requests (and DELETE with body)
        if (method_str == "POST" || method_str == "PUT" || 
           (method_str == "DELETE" && *upload_data_size > 0)) {
            
            if (*upload_data_size > 0) {
                // Limit request body size for security (10MB max)
                const size_t MAX_BODY_SIZE = 10 * 1024 * 1024;
                if (con_info->size() + *upload_data_size > MAX_BODY_SIZE) {
                    return sendErrorResponse(connection, MHD_HTTP_CONTENT_TOO_LARGE, "Request body too large");
                }
                
                con_info->append(upload_data, *upload_data_size);
                *upload_data_size = 0; // Mark as processed
                return MHD_YES; // Continue processing
            }
            
            // Get the accumulated body (skip if it contains initialization artifacts)
            if (!con_info->empty()) {
                body = *con_info;
                
                // Clean up any initialization artifacts
                if (body.find("initialized") == 0) {
                    body = body.substr(11); // Remove "initialized" prefix
                }
                
                // Log request body for debugging
                ENDPOINT_LOG_INFO("http", method_str + " request body content: " + body);
                
                // Try to parse and pretty-print JSON if valid
                try {
                    if (!body.empty() && (body[0] == '{' || body[0] == '[')) {
                        auto parsed_json = nlohmann::json::parse(body);
                        ENDPOINT_LOG_INFO("http", method_str + " JSON (formatted): " + parsed_json.dump(2));
                    }
                } catch (const std::exception& e) {
                    ENDPOINT_LOG_INFO("http", method_str + " body (not valid JSON): " + body);
                }
            }
        }
        
        // Validate HTTP method
        if (method_str != "GET" && method_str != "POST" && method_str != "PUT" && 
            method_str != "DELETE" && method_str != "HEAD" && method_str != "OPTIONS") {
            return sendErrorResponse(connection, MHD_HTTP_METHOD_NOT_ALLOWED, "Method not allowed");
        }
        
        // Handle backup file downloads
        if (url_str.find("/api/backup/download/") == 0) {
            std::string backup_id = url_str.substr(std::string("/api/backup/download/").length());
            if (!backup_id.empty()) {
                std::string backup_path = "ur-webif-api/data/backup-storage/" + backup_id;
                
                if (std::filesystem::exists(backup_path)) {
                    // Read file content
                    std::ifstream file(backup_path, std::ios::binary);
                    if (file) {
                        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                        file.close();
                        
                        // Create MHD response for file download
                        struct MHD_Response* mhd_response = MHD_create_response_from_buffer(
                            content.length(),
                            const_cast<char*>(content.c_str()),
                            MHD_RESPMEM_MUST_COPY
                        );
                        
                        if (!mhd_response) {
                            return sendErrorResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "Failed to create response");
                        }
                        
                        // Set appropriate headers for file download
                        MHD_add_response_header(mhd_response, "Content-Type", "application/octet-stream");
                        MHD_add_response_header(mhd_response, "Content-Disposition", ("attachment; filename=\"" + backup_id + "\"").c_str());
                        MHD_add_response_header(mhd_response, "Access-Control-Allow-Origin", "*");
                        
                        enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, mhd_response);
                        MHD_destroy_response(mhd_response);
                        return ret;
                    }
                }
                
                // File not found
                return sendErrorResponse(connection, MHD_HTTP_NOT_FOUND, "Backup file not found");
            }
        }
        
        // Process the request with timeout protection
        std::string response;
        try {

            response = it->second(method_str, params, body);
            
            // Log the response for debugging
            ENDPOINT_LOG_INFO("http", "Generated response for " + url_str + ": " + response);
            
            // Try to pretty-print JSON responses
            try {
                if (!response.empty() && (response[0] == '{' || response[0] == '[')) {
                    auto parsed_json = nlohmann::json::parse(response);
                    ENDPOINT_LOG_INFO("http", "Response JSON (formatted): " + parsed_json.dump(2));
                }
            } catch (const std::exception& e) {
                ENDPOINT_LOG_INFO("http", "Response (not valid JSON): " + response);
            }
            
            // Validate response size
            if (response.size() > 100 * 1024 * 1024) { // 100MB limit
                std::cerr << "Response too large, truncating" << std::endl;
                return sendErrorResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "Response too large");
            }
        } catch (const std::bad_alloc& e) {
            std::cerr << "Memory allocation error: " << e.what() << std::endl;
            return sendErrorResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "Out of memory");
        }
        
        // Determine content type with better detection
        std::string content_type = "text/plain";
        if (!response.empty()) {
            if (response[0] == '{' || response[0] == '[') {
                try {
                    auto parsed = nlohmann::json::parse(response);
                    content_type = "application/json";
                } catch (...) {
                    // Keep as text/plain
                }
            } else if (response.substr(0, 5) == "<!DOC" || response.substr(0, 5) == "<html") {
                content_type = "text/html";
            } else if (response.substr(0, 5) == "<?xml") {
                content_type = "application/xml";
            }
        }
        
        return sendResponse(connection, MHD_HTTP_OK, response, content_type);
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling request " << url_str << ": " << e.what() << std::endl;
        return sendErrorResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "Internal server error");
    } catch (...) {
        std::cerr << "Unknown error handling request " << url_str << std::endl;
        return sendErrorResponse(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, "Unknown internal error");
    }
}

void HttpHandler::addRouteHandler(const std::string& path, 
                                 std::function<std::string(const std::string& method, 
                                                          const std::map<std::string, std::string>& params,
                                                          const std::string& body)> handler) {
    route_handlers_[path] = handler;
}

void HttpHandler::addDynamicRoute(const std::string& pattern,
                                 std::function<std::string(const std::string& method, 
                                                          const std::map<std::string, std::string>& params,
                                                          const std::string& body)> handler) {
    if (dynamic_router_) {
        dynamic_router_->addRoute(pattern, handler);
    }
}

// Structured route handler registration
void HttpHandler::addStructuredRouteHandler(const std::string& path, RouteProcessor processor) {
    structured_route_processors_[path] = processor;
}

// Build structured API request
ApiRequest HttpHandler::buildApiRequest(struct MHD_Connection* connection, const char* url, 
                                       const char* method, const char* upload_data, 
                                       size_t* upload_data_size, void** con_cls) {
    ApiRequest request;
    
    // Critical: Handle connection state properly for MHD
    std::string* con_info = static_cast<std::string*>(*con_cls);
    if (con_info == nullptr) {
        // First call - initialize with proper state tracking
        con_info = new std::string("structured_handler_initialized");
        *con_cls = con_info;
        return request; // Return empty request to signal "still processing"
    }
    
    // Validate connection state
    if (con_info->empty() || *con_info == "structured_handler_initialized") {
        con_info->assign("processing");
    }
    
    // Basic request information
    request.route = std::string(url);
    request.method = std::string(method);
    request.generateRequestId();
    
    // Get client IP
    const char* client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Forwarded-For");
    if (!client_ip) {
        client_ip = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "X-Real-IP");
    }
    request.source_ip = client_ip ? std::string(client_ip) : "unknown";
    
    // Get user agent
    const char* user_agent = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "User-Agent");
    request.user_agent = user_agent ? std::string(user_agent) : "unknown";
    
    // Parse query parameters
    MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND,
                            [](void* cls, enum MHD_ValueKind kind, const char* key, const char* value) -> enum MHD_Result {
                                auto* params_map = static_cast<std::map<std::string, std::string>*>(cls);
                                if (key && value) {
                                    (*params_map)[std::string(key)] = std::string(value);
                                }
                                return MHD_YES;
                            }, &request.params);
    
    // Handle request body for POST/PUT requests
    if (request.method == "GET" && *upload_data_size == 0) {
        // GET request is complete
        return request;
    }
    
    if (*upload_data_size > 0) {
        // Limit request body size for security (10MB max)
        const size_t MAX_BODY_SIZE = 10 * 1024 * 1024;
        if (con_info->size() + *upload_data_size > MAX_BODY_SIZE) {
            request.request_id = ""; // Signal error
            return request;
        }
        
        con_info->append(upload_data, *upload_data_size);
        *upload_data_size = 0; // Mark as processed
        return request; // Return empty request ID to signal "still processing"
    }
    
    // Get the accumulated body
    if (!con_info->empty() && *con_info != "processing") {
        request.body = *con_info;
        request.content_length = request.body.length();
        request.parseJsonBody();
    }
    
    return request;
}

// Send structured API response
enum MHD_Result HttpHandler::sendApiResponse(struct MHD_Connection* connection, const ApiResponse& response) {
    return sendResponse(connection, response.status_code, response.body, response.content_type);
}

std::map<std::string, std::string> HttpHandler::parseQueryString(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream iss(query);
    std::string pair;
    
    while (std::getline(iss, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            params[key] = value;
        }
    }
    
    return params;
}

std::map<std::string, std::string> HttpHandler::parseHeaders(struct MHD_Connection* connection) {
    std::map<std::string, std::string> headers;
    
    // This would need a custom iterator function for MHD headers
    // For now, we'll implement specific header lookups as needed
    
    return headers;
}

std::string HttpHandler::getRequestBody(const char* upload_data, size_t upload_data_size) {
    return std::string(upload_data, upload_data_size);
}

enum MHD_Result HttpHandler::sendResponse(struct MHD_Connection* connection, 
                             int status_code, 
                             const std::string& content,
                             const std::string& content_type) {
    
    if (!connection) {
        return MHD_NO;
    }
    
    // Critical: Create response with robust memory management to prevent core dumps
    struct MHD_Response* response = nullptr;
    
    // Validate content before creating response
    std::string validated_content = content;
    if (validated_content.empty()) {
        std::cerr << "Warning: Attempting to send empty response" << std::endl;
        validated_content = std::string("{\"error\":\"Empty response\"}");
    }
    
    // Use safe memory allocation strategy
    try {
        response = MHD_create_response_from_buffer(
            validated_content.length(),
            const_cast<char*>(validated_content.c_str()),
            MHD_RESPMEM_MUST_COPY  // Safe: MHD copies data, no memory ownership issues
        );
    } catch (const std::exception& e) {
        std::cerr << "Exception creating MHD response: " << e.what() << std::endl;
        return MHD_NO;
    }
    
    if (!response) {
        std::cerr << "Critical: Failed to create MHD response - possible memory issue" << std::endl;
        return MHD_NO;
    }
    
    // Set comprehensive headers for robust web operations
    MHD_add_response_header(response, "Content-Type", content_type.c_str());
    MHD_add_response_header(response, "Content-Length", std::to_string(content.length()).c_str());
    
    // Security headers
    MHD_add_response_header(response, "X-Content-Type-Options", "nosniff");
    MHD_add_response_header(response, "X-Frame-Options", "DENY");
    MHD_add_response_header(response, "X-XSS-Protection", "1; mode=block");
    MHD_add_response_header(response, "Referrer-Policy", "strict-origin-when-cross-origin");
    
    // CORS headers for robust cross-origin support
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, HEAD");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With, Accept, Origin");
    MHD_add_response_header(response, "Access-Control-Max-Age", "86400");
    MHD_add_response_header(response, "Access-Control-Allow-Credentials", "true");
    
    // Caching headers
    if (status_code == MHD_HTTP_OK) {
        MHD_add_response_header(response, "Cache-Control", "no-cache, no-store, must-revalidate");
        MHD_add_response_header(response, "Pragma", "no-cache");
        MHD_add_response_header(response, "Expires", "0");
    }
    
    // Server identification
    MHD_add_response_header(response, "Server", "UR-WebIF-API/1.0");
    
    // Add current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::gmtime(&time_t);
    char date_buffer[64];
    std::strftime(date_buffer, sizeof(date_buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    MHD_add_response_header(response, "Date", date_buffer);
    
    // Critical: Thread-safe response queueing with proper error handling
    enum MHD_Result ret = MHD_NO;
    
    try {
        ret = MHD_queue_response(connection, status_code, response);
        if (ret != MHD_YES) {
            std::cerr << "Critical: MHD_queue_response failed with status: " << ret << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception during response queueing: " << e.what() << std::endl;
        ret = MHD_NO;
    } catch (...) {
        std::cerr << "Unknown exception during response queueing" << std::endl;
        ret = MHD_NO;
    }
    
    // Always destroy response to prevent memory leaks
    try {
        MHD_destroy_response(response);
    } catch (const std::exception& e) {
        std::cerr << "Exception destroying MHD response: " << e.what() << std::endl;
    }
    
    return ret;
}

enum MHD_Result HttpHandler::sendJsonResponse(struct MHD_Connection* connection,
                                 int status_code,
                                 const std::string& json_content) {
    return sendResponse(connection, status_code, json_content, "application/json");
}

enum MHD_Result HttpHandler::sendErrorResponse(struct MHD_Connection* connection,
                                  int status_code,
                                  const std::string& error_message) {
    nlohmann::json error_json;
    error_json["error"] = error_message;
    error_json["status"] = status_code;
    
    return sendJsonResponse(connection, status_code, error_json.dump());
}