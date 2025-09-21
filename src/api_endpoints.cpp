#include "api_endpoints.h"
#include "web_server.h"
#include "http_event_handler.h"
#include "route_processors.h"
#include "endpoint_logger.h"
#include "routers/VpnRouter.h"
#include "routers/WirelessRouter.h"
#include "routers/NetworkPriorityRouter.h"
#include "routers/NetworkUtilityRouter.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>

using json = nlohmann::json;

void ApiEndpoints::configureEndpoints(WebServer& server,
                                     std::shared_ptr<HttpEventHandler> event_handler,
                                     std::shared_ptr<RouteProcessors> route_processors) {
    ENDPOINT_LOG("utils", "Configuring API endpoints...");

    configureAuthEndpoints(server, event_handler);
    configureDashboardEndpoints(server, route_processors);
    configureUtilityEndpoints(server, event_handler);
    configureWiredNetworkEndpoints(server);
    configureWirelessNetworkEndpoints(server);
    configureNetworkUtilityEndpoints(server);

    printEndpointSummary();
}

void ApiEndpoints::configureAuthEndpoints(WebServer& server,
                                         std::shared_ptr<HttpEventHandler> event_handler) {
    // Login endpoint - Event-driven processing
    server.addRouteHandler("/api/login", [event_handler](const std::string& method,
                                            const std::map<std::string, std::string>& params,
                                            const std::string& body) {
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
            auto response = event_handler->processEvent(event_request.dump(), params);

            // Log the complete API response
            ENDPOINT_LOG("auth", "[LOGIN-API] Processing result: " + std::to_string(static_cast<int>(response.result)));
            ENDPOINT_LOG("auth", "[LOGIN-API] Response data: " + response.data.dump(2));
            ENDPOINT_LOG("auth", "[LOGIN-API] Response message: " + response.message);
            ENDPOINT_LOG("auth", "[LOGIN-API] HTTP status: " + std::to_string(response.http_status));

            return response.data.dump();

        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Invalid request format"}, {"error", e.what()}};
            return error.dump();
        }
    });

    // Logout endpoint - Event-driven processing
    server.addRouteHandler("/api/logout", [event_handler](const std::string& method,
                                             const std::map<std::string, std::string>& params,
                                             const std::string& body) {
        if (method != "POST") {
            json error = {{"success", false}, {"message", "Method not allowed"}};
            return error.dump();
        }

        json event_request = {
            {"type", "logout_request"},
            {"payload", json::parse(body.empty() ? "{}" : body)},
            {"source", {{"component", "user-session"}, {"action", "logout"}}}
        };

        auto response = event_handler->processEvent(event_request.dump(), params);
        json response_json = {
            {"success", response.result == HttpEventHandler::EventResult::SUCCESS},
            {"message", response.message},
            {"data", response.data}
        };
        return response_json.dump();
    });

    // File authentication endpoint
    server.addRouteHandler("/api/file-auth", [event_handler](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) -> std::string {
        ENDPOINT_LOG_INFO("auth", "File authentication endpoint called with method: " + method);

        if (method != "POST") {
            json error_response = {
                {"success", false},
                {"message", "Method not allowed"},
                {"status_code", 405}
            };
            return error_response.dump();
        }

        // Parse the request to create event data
        try {
            json request_data = json::parse(body);

            // Create event for file authentication
            json event_request = {
                {"type", "file_authentication"},
                {"payload", request_data},
                {"source", {
                    {"component", "file-upload"},
                    {"action", "authenticate"}
                }}
            };

            auto response = event_handler->processEvent(event_request.dump(), params);

            json api_response = {
                {"success", response.result == HttpEventHandler::EventResult::SUCCESS},
                {"message", response.message},
                {"data", response.data},
                {"session_token", response.session_token}
            };

            return api_response.dump();

        } catch (const std::exception& e) {
            json error_response = {
                {"success", false},
                {"message", "Invalid request format"},
                {"error", e.what()}
            };
            return error_response.dump();
        }
    });

    // Echo endpoint for testing
    server.addRouteHandler("/api/echo", [event_handler](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) -> std::string {
        ENDPOINT_LOG_INFO("utils", "Echo endpoint called with method: " + method);

        auto response = event_handler->processEvent(body, params);
        return response.data.dump();
    });
}

void ApiEndpoints::configureDashboardEndpoints(WebServer& server,
                                              std::shared_ptr<RouteProcessors> route_processors) {
    // Dashboard data endpoint for system information
    server.addRouteHandler("/api/dashboard-data", [route_processors](const std::string& method,
                                                   const std::map<std::string, std::string>& params,
                                                   const std::string& body) -> std::string {
        ENDPOINT_LOG("dashboard", "[DASHBOARD-API] Processing dashboard data request, method: " + method);

        // Create API request structure
        ApiRequest request;
        request.method = method;
        request.route = "/api/dashboard-data";
        request.body = body;
        request.headers = {};

        // Parse JSON body if present
        if (!body.empty()) {
            try {
                request.json_data = json::parse(body);
                request.is_json_valid = true;
                ENDPOINT_LOG("dashboard", "[DASHBOARD-API] Request body: " + request.json_data.dump(2));
            } catch (const std::exception& e) {
                request.is_json_valid = false;
                ENDPOINT_LOG("dashboard", "[DASHBOARD-API] Invalid JSON in request body: " + std::string(e.what()));
            }
        }

        // Process request using route processors
        auto response = route_processors->processDashboardDataRequest(request);

        ENDPOINT_LOG("dashboard", "[DASHBOARD-API] Response status: " + std::to_string(response.http_status));
        ENDPOINT_LOG("dashboard", "[DASHBOARD-API] Response data: " + response.data.dump(2));

        return response.data.dump();
    });
}

void ApiEndpoints::configureUtilityEndpoints(WebServer& server,
                                            std::shared_ptr<HttpEventHandler> event_handler) {
    // Health check endpoint (replaces WebSocket ping/pong) - Simplified for testing
    server.addRouteHandler("/api/health", [](const std::string& method,
                                          const std::map<std::string, std::string>& params,
                                          const std::string& body) {
        ENDPOINT_LOG("utils", "Health endpoint called with method: " + method);
        return std::string("{\"status\":\"ok\",\"message\":\"Server is healthy\"}");
    });

    // Echo endpoint - Event-driven processing
    server.addRouteHandler("/api/echo", [event_handler](const std::string& method,
                                          const std::map<std::string, std::string>& params,
                                          const std::string& body) {
        if (method != "POST") {
            json error = {{"success", false}, {"message", "Method not allowed"}};
            return error.dump();
        }

        // Handle potentially malformed JSON for echo endpoint
        json payload;
        try {
            payload = json::parse(body.empty() ? "{}" : body);
        } catch (const std::exception& e) {
            payload = {{"raw_data", body}};
        }

        json event_request = {
            {"type", "echo_test"},
            {"payload", payload},
            {"source", {{"component", "test-client"}, {"action", "echo"}}}
        };

        auto response = event_handler->processEvent(event_request.dump(), params);
        json response_json = {
            {"success", response.result == HttpEventHandler::EventResult::SUCCESS},
            {"message", response.message},
            {"data", response.data}
        };
        return response_json.dump();
    });

    // Server statistics endpoint (replaces WebSocket get_stats)
    server.addRouteHandler("/api/stats", [](const std::string& method,
                                           const std::map<std::string, std::string>& params,
                                           const std::string& body) {
        json response;
        response["service"] = "ur-webif-api";
        response["version"] = "3.0.0";
        response["architecture"] = "http-based";
        response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        response["server_uptime"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        response["active_http_connections"] = 1; // Simplified for HTTP
        response["total_requests"] = 1; // Would be tracked in real implementation
        response["status"] = "healthy";

        return response.dump();
    });

    // Broadcast endpoint (replaces WebSocket broadcast)
    server.addRouteHandler("/api/broadcast", [](const std::string& method,
                                               const std::map<std::string, std::string>& params,
                                               const std::string& body) {
        if (method != "POST") {
            json error;
            error["success"] = false;
            error["message"] = "Method not allowed";
            return error.dump();
        }

        try {
            auto json_body = json::parse(body);

            json response;
            response["type"] = "broadcast_response";
            response["success"] = true;
            response["message"] = "Message would be broadcast to all connected clients";
            response["original_message"] = json_body.value("data", "");
            response["broadcast_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            response["recipients"] = 0; // In HTTP model, we don't maintain persistent connections

            return response.dump();

        } catch (const std::exception& e) {
            json error;
            error["success"] = false;
            error["message"] = "Invalid request format";
            return error.dump();
        }
    });

    // Event endpoint for frontend event tracking
    server.addRouteHandler("/api/event", [](const std::string& method,
                                          const std::map<std::string, std::string>& params,
                                          const std::string& body) {
        if (method != "POST") {
            json error = {{"success", false}, {"message", "Method not allowed"}};
            return error.dump();
        }

        json response = {
            {"success", true},
            {"message", "Event received and processed"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"processed", true}
        };

        // Log the event data if valid JSON
        try {
            if (!body.empty()) {
                auto event_data = json::parse(body);
                response["event_type"] = event_data.value("type", "unknown");
                response["element_id"] = event_data.value("elementId", "unknown");
                ENDPOINT_LOG("utils", "[EVENT-TRACKER] Event received: " + event_data.dump(2));
            }
        } catch (const std::exception& e) {
            ENDPOINT_LOG("utils", "[EVENT-TRACKER] Invalid JSON event data: " + body);
            response["event_type"] = "invalid_json";
        }

        return response.dump();
    });

    // Server-Sent Events endpoint for real-time updates
    server.addRouteHandler("/api/events", [](const std::string& method,
                                            const std::map<std::string, std::string>& params,
                                            const std::string& body) {
        if (method != "GET") {
            json error;
            error["success"] = false;
            error["message"] = "Method not allowed. Use GET for Server-Sent Events.";
            return error.dump();
        }

        // This endpoint would implement Server-Sent Events in a full implementation
        // For now, return info about the SSE endpoint
        json response;
        response["message"] = "Server-Sent Events endpoint";
        response["instructions"] = "Connect using EventSource API for real-time updates";
        response["events_supported"] = {"login_status", "system_notifications", "heartbeat"};

        return response.dump();
    });
}

void ApiEndpoints::configureWiredNetworkEndpoints(WebServer& server) {
    // Wired network status endpoint
    server.addRouteHandler("/api/network/wired/status", [](const std::string& method,
                                                        const std::map<std::string, std::string>& params,
                                                        const std::string& body) {
        ENDPOINT_LOG("wired", "[WIRED-STATUS] " + method + " request received");

        if (method == "GET") {
            // Return current wired connection status based on wired-attributes.md structure
            json response = {
                {"success", true},
                {"data", {
                    {"status", "CONNECTED"},
                    {"connection_type", "DHCP Automatic"},
                    {"ip_address", "192.168.1.147"},
                    {"mac_address", "00:1B:44:11:3A:B7"},
                    {"link_speed", "1 Gbps"},
                    {"duplex_mode", "Full Duplex"},
                    {"uptime", "2d 14h 32m"},
                    {"gateway", "192.168.1.1"}
                }},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };

            ENDPOINT_LOG("wired", "[WIRED-STATUS] Status data returned");
            return response.dump();
        } else if (method == "POST") {
            // Handle status action requests (reconnect, renew IP)
            try {
                if (!body.empty()) {
                    auto request_data = json::parse(body);
                    std::string action = request_data.value("action", "");

                    ENDPOINT_LOG("wired", "[WIRED-STATUS] Action request: " + action);
                    ENDPOINT_LOG("wired", "[WIRED-STATUS] Full request: " + request_data.dump(2));

                    json response = {
                        {"success", true},
                        {"message", "Action " + action + " initiated"},
                        {"action", action},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };

                    return response.dump();
                }
            } catch (const std::exception& e) {
                ENDPOINT_LOG("wired", "[WIRED-STATUS] JSON parse error: " + std::string(e.what()));
            }

            json error = {{"success", false}, {"message", "Invalid request format"}};
            return error.dump();
        }

        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    });

    // Wired network configuration endpoint
    server.addRouteHandler("/api/network/wired/configure", [](const std::string& method,
                                                           const std::map<std::string, std::string>& params,
                                                           const std::string& body) {
        ENDPOINT_LOG("wired", "[WIRED-CONFIGURE] " + method + " request received");

        if (method == "GET") {
            // Return current basic configuration
            json response = {
                {"success", true},
                {"data", {
                    {"ethernet_enabled", true},
                    {"connection_mode", "dhcp"},
                    {"interface_priority", "medium"},
                    {"static_config", {
                        {"ip_address", ""},
                        {"subnet_mask", ""},
                        {"gateway", ""},
                        {"dns_primary", ""},
                        {"dns_secondary", ""},
                        {"domain_suffix", ""}
                    }}
                }},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };

            ENDPOINT_LOG("wired", "[WIRED-CONFIGURE] Configuration data returned");
            return response.dump();
        } else if (method == "POST") {
            // Handle configuration updates
            try {
                if (!body.empty()) {
                    auto config_data = json::parse(body);

                    ENDPOINT_LOG("wired", "[WIRED-CONFIGURE] Configuration update request: " + config_data.dump(2));

                    json response = {
                        {"success", true},
                        {"message", "Configuration updated successfully"},
                        {"applied_config", config_data},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };

                    return response.dump();
                }
            } catch (const std::exception& e) {
                ENDPOINT_LOG("wired", "[WIRED-CONFIGURE] JSON parse error: " + std::string(e.what()));
            }

            json error = {{"success", false}, {"message", "Invalid configuration format"}};
            return error.dump();
        }

        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    });

    // Wired network profiles endpoint
    server.addRouteHandler("/api/network/wired/profiles", [](const std::string& method,
                                                          const std::map<std::string, std::string>& params,
                                                          const std::string& body) {
        ENDPOINT_LOG("wired", "[WIRED-PROFILES] " + method + " request received");

        if (method == "GET") {
            // Return current connection profiles
            json response = {
                {"success", true},
                {"data", {
                    {"profiles", {
                        {
                            {"id", "home-network"},
                            {"name", "Home Network"},
                            {"type", "DHCP"},
                            {"ip_address", "192.168.1.147"},
                            {"gateway", "192.168.1.1"},
                            {"is_active", true},
                            {"status", "Active"}
                        },
                        {
                            {"id", "office-network"},
                            {"name", "Office Network"},
                            {"type", "Static"},
                            {"ip_address", "10.0.0.100"},
                            {"gateway", "10.0.0.1"},
                            {"is_active", false},
                            {"status", "Inactive"}
                        },
                        {
                            {"id", "guest-network"},
                            {"name", "Guest Network"},
                            {"type", "DHCP"},
                            {"ip_address", "172.16.1.50"},
                            {"gateway", "172.16.1.1"},
                            {"is_active", false},
                            {"status", "Inactive"}
                        }
                    }}
                }},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };

            ENDPOINT_LOG("wired", "[WIRED-PROFILES] Profile data returned");
            return response.dump();
        } else if (method == "POST") {
            // Handle profile management operations
            try {
                if (!body.empty()) {
                    auto request_data = json::parse(body);
                    std::string action = request_data.value("action", "");

                    ENDPOINT_LOG("wired", "[WIRED-PROFILES] Profile action: " + action);
                    ENDPOINT_LOG("wired", "[WIRED-PROFILES] Full request: " + request_data.dump(2));

                    json response = {
                        {"success", true},
                        {"message", "Profile " + action + " completed"},
                        {"action", action},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };

                    if (action == "create" && request_data.contains("profile")) {
                        response["created_profile"] = request_data["profile"];
                    } else if (action == "activate" && request_data.contains("profile_id")) {
                        response["activated_profile_id"] = request_data["profile_id"];
                    } else if (action == "delete" && request_data.contains("profile_id")) {
                        response["deleted_profile_id"] = request_data["profile_id"];
                    }

                    return response.dump();
                }
            } catch (const std::exception& e) {
                ENDPOINT_LOG("wired", "[WIRED-PROFILES] JSON parse error: " + std::string(e.what()));
            }

            json error = {{"success", false}, {"message", "Invalid profile request format"}};
            return error.dump();
        }

        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    });

    // Wired network advanced settings endpoint
    server.addRouteHandler("/api/network/advanced", [](const std::string& method,
                                                          const std::map<std::string, std::string>& params,
                                                          const std::string& body) {
        ENDPOINT_LOG("wired", "[WIRED-ADVANCED] " + method + " request received");

        if (method == "GET") {
            // Return current advanced settings
            json response = {
                {"success", true},
                {"data", {
                    {"mtu_size", 1500},
                    {"duplex_mode", "auto"},
                    {"additional_ips", {
                        {
                            {"address", "192.168.2.100/24"},
                            {"id", "additional-ip-1"}
                        },
                        {
                            {"address", "10.0.0.50/8"},
                            {"id", "additional-ip-2"}
                        }
                    }}
                }},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };

            ENDPOINT_LOG("wired", "[WIRED-ADVANCED] Advanced settings data returned");
            return response.dump();
        } else if (method == "POST") {
            // Handle advanced settings updates
            try {
                if (!body.empty()) {
                    auto settings_data = json::parse(body);

                    ENDPOINT_LOG("wired", "[WIRED-ADVANCED] Advanced settings update: " + settings_data.dump(2));

                    json response = {
                        {"success", true},
                        {"message", "Advanced settings updated successfully"},
                        {"applied_settings", settings_data},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };

                    return response.dump();
                }
            } catch (const std::exception& e) {
                ENDPOINT_LOG("wired", "[WIRED-ADVANCED] JSON parse error: " + std::string(e.what()));
            }

            json error = {{"success", false}, {"message", "Invalid advanced settings format"}};
            return error.dump();
        }

        json error = {{"success", false}, {"message", "Method not allowed"}};
        return error.dump();
    });
}

void ApiEndpoints::configureWirelessNetworkEndpoints(WebServer& server) {
    // Note: WirelessRouter and NetworkPriorityRouter instances are already created and 
    // their routes registered in main.cpp. This function now just logs the configuration.

    ENDPOINT_LOG("wireless", "Wireless network endpoints already configured in main.cpp");
    ENDPOINT_LOG("network-priority", "Network priority endpoints already configured in main.cpp");
}

void ApiEndpoints::configureNetworkUtilityEndpoints(WebServer& server) {
    // Note: NetworkUtilityRouter instance is already created and 
    // its routes registered in main.cpp. This function now just logs the configuration.

    ENDPOINT_LOG("network-utility", "Network utility endpoints already configured in main.cpp");
}


void ApiEndpoints::printEndpointSummary() {
    ENDPOINT_LOG("utils", "📡 HTTP API endpoints configured:");
    ENDPOINT_LOG("utils", "   POST /api/login - User authentication");
    ENDPOINT_LOG("utils", "   POST /api/logout - User logout");
    ENDPOINT_LOG("utils", "   POST /api/validate-session - Session validation");
    ENDPOINT_LOG("utils", "   POST /api/echo - Echo service");
    ENDPOINT_LOG("utils", "   POST /api/broadcast - Message broadcasting");
    ENDPOINT_LOG("utils", "   POST /api/event - Frontend event tracking");
    ENDPOINT_LOG("utils", "   POST /api/file-auth - File content authentication");
    ENDPOINT_LOG("utils", "   GET /api/health - Health check");
    ENDPOINT_LOG("utils", "   GET /api/stats - Server statistics");
    ENDPOINT_LOG("utils", "   GET /api/system-data - Live system information");
    ENDPOINT_LOG("utils", "   GET/POST /api/dashboard-data - Global dashboard data with real-time updates");
    ENDPOINT_LOG("utils", "   GET/POST /api/system-component - Isolated system data");
    ENDPOINT_LOG("utils", "   GET/POST /api/network-component - Isolated network data");
    ENDPOINT_LOG("utils", "   GET/POST /api/cellular-component - Isolated cellular data");
    ENDPOINT_LOG("utils", "   GET/POST /api/network/wired/status - Wired connection status and actions");
    ENDPOINT_LOG("utils", "   GET/POST /api/network/wired/configure - Basic wired network configuration");
    ENDPOINT_LOG("utils", "   GET/POST /api/network/wired/profiles - Connection profile management");
    ENDPOINT_LOG("utils", "   GET/POST /api/network/wired/advanced - Advanced wired network settings");
    ENDPOINT_LOG("utils", "   GET/POST /api/wireless/status - Wireless connection status and actions");
    ENDPOINT_LOG("utils", "   GET/POST /api/wireless/radio - Radio control operations");
    ENDPOINT_LOG("utils", "   GET/POST /api/wireless/scan - Network scanning");
    ENDPOINT_LOG("utils", "   GET/POST /api/wireless/connect - Network connection operations");
    ENDPOINT_LOG("utils", "   GET/POST /api/wireless/saved-networks - Saved network management");
    ENDPOINT_LOG("utils", "   POST /api/wireless/ap/start - Start access point mode");
    ENDPOINT_LOG("utils", "   POST /api/wireless/ap/stop - Stop access point mode");
    ENDPOINT_LOG("utils", "   POST /api/wireless/events - Wireless event processing");
    ENDPOINT_LOG("utils", "   GET/POST /api/cellular/status - Cellular connection status and information");
    ENDPOINT_LOG("utils", "   GET/POST /api/cellular/config - Cellular configuration management");
    ENDPOINT_LOG("utils", "   POST /api/cellular/connect - Initiate cellular connection");
    ENDPOINT_LOG("utils", "   POST /api/cellular/disconnect - Disconnect cellular connection");
    ENDPOINT_LOG("utils", "   POST /api/cellular/scan - Scan for available cellular networks");
    ENDPOINT_LOG("utils", "   POST /api/cellular/events - Cellular event processing");
    ENDPOINT_LOG("utils", "   GET /api/events - Server-Sent Events (SSE)");
    ENDPOINT_LOG("utils", "   GET/POST /api/network-priority/interfaces - Network interface priority management");
    ENDPOINT_LOG("utils", "   GET/POST/PUT/DELETE /api/network-priority/routing-rules - Routing rules management");
    ENDPOINT_LOG("utils", "   POST /api/network-priority/reorder-interfaces - Reorder interface priorities");
    ENDPOINT_LOG("utils", "   POST /api/network-priority/reorder-rules - Reorder routing rule priorities");
    ENDPOINT_LOG("utils", "   POST /api/network-priority/apply - Apply network priority changes");
    ENDPOINT_LOG("utils", "   POST /api/network-priority/reset - Reset network priority to defaults");
    ENDPOINT_LOG("utils", "   GET /api/network-priority/status - Get network priority status");
    ENDPOINT_LOG("utils", "   POST /api/network-priority/events - Network priority event processing");
    ENDPOINT_LOG("utils", "   GET/POST /api/network-utility/iperf3 - iperf3 network performance test");
    ENDPOINT_LOG("utils", "   GET/POST /api/network-utility/dns-lookup - DNS lookup utility");
    ENDPOINT_LOG("utils", "   GET/POST /api/network-utility/ping - Ping network utility");
    ENDPOINT_LOG("utils", "   GET/POST /api/network-utility/traceroute - Traceroute network utility");

    ENDPOINT_LOG("utils", "🔧 HTTP-based event system features:");
    ENDPOINT_LOG("utils", "   ✅ RESTful API endpoints for all operations");
    ENDPOINT_LOG("utils", "   ✅ JSON request/response format");
    ENDPOINT_LOG("utils", "   ✅ HTTP session management");
    ENDPOINT_LOG("utils", "   ✅ Request timeout handling");
    ENDPOINT_LOG("utils", "   ✅ Error handling and validation");
}