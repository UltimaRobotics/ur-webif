#include "VpnRouter.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>
#include <fstream> // Required for file operations
#include <filesystem> // Required for directory operations
#include "../../mecanisms/vpn-parser/vpn_parser_manager.hpp"

// Forward declaration for VpnDataManager
class VpnDataManager;

// Define the path for VPN data directory
const std::string VPN_DATA_DIR = "ur-webif-api/data/vpn/";

// Helper function to create directories if they don't exist
void create_directories(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }
}

// Helper function to read JSON data from a file
json read_json_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (file.is_open()) {
        json data;
        file >> data;
        return data;
    }
    return json(); // Return empty JSON if file not found or error
}

// Helper function to write JSON data to a file
void write_json_file(const std::string& filepath, const json& data) {
    std::ofstream file(filepath);
    if (file.is_open()) {
        file << data.dump(4); // Use dump(4) for pretty printing
    }
}

// VPN Data Manager Class
class VpnDataManager {
private:
    std::string routingRulesPath = VPN_DATA_DIR + "routing-rules.json";
    std::string secSettingsPath = VPN_DATA_DIR + "sec-settings.json";
    std::string vpnProfilesPath = VPN_DATA_DIR + "vpn-profiles.json";

public:
    VpnDataManager() {
        create_directories(VPN_DATA_DIR);
        // Ensure default files exist if they don't
        if (!std::filesystem::exists(routingRulesPath)) {
            json defaultRules;
            defaultRules["routing_rules"] = json::array();
            write_json_file(routingRulesPath, defaultRules);
        }
        if (!std::filesystem::exists(secSettingsPath)) {
            json defaultSettings;
            defaultSettings["security_settings"] = json::object();
            write_json_file(secSettingsPath, defaultSettings);
        }
        if (!std::filesystem::exists(vpnProfilesPath)) {
            json defaultProfiles;
            defaultProfiles["vpn_profiles"] = json::array();
            write_json_file(vpnProfilesPath, defaultProfiles);
        }
    }

    // --- Routing Rules ---
    json getRoutingRules() {
        json data = read_json_file(routingRulesPath);
        return data.contains("routing_rules") ? data["routing_rules"] : json::array();
    }

    void addRoutingRule(const json& rule) {
        json data = read_json_file(routingRulesPath);
        json rules = data.contains("routing_rules") ? data["routing_rules"] : json::array();
        rules.push_back(rule);
        json updatedData;
        updatedData["routing_rules"] = rules;
        write_json_file(routingRulesPath, updatedData);
    }

    void deleteRoutingRule(const std::string& ruleId) {
        json data = read_json_file(routingRulesPath);
        json rules = data.contains("routing_rules") ? data["routing_rules"] : json::array();
        for (auto it = rules.begin(); it != rules.end(); ++it) {
            if (it->value("id", "") == ruleId) {
                rules.erase(it);
                break;
            }
        }
        json updatedData;
        updatedData["routing_rules"] = rules;
        write_json_file(routingRulesPath, updatedData);
    }

    void updateRoutingRule(const std::string& ruleId, const json& updatedRule) {
        json data = read_json_file(routingRulesPath);
        json rules = data.contains("routing_rules") ? data["routing_rules"] : json::array();
        for (auto& rule : rules) {
            if (rule.value("id", "") == ruleId) {
                rule = updatedRule; // Replace the entire rule
                break;
            }
        }
        json updatedData;
        updatedData["routing_rules"] = rules;
        write_json_file(routingRulesPath, updatedData);
    }

    // --- Security Settings ---
    json getSecuritySettings() {
        return read_json_file(secSettingsPath);
    }

    void updateSecuritySettings(const json& settings) {
        write_json_file(secSettingsPath, settings);
    }

    // --- VPN Profiles ---
    json getVpnProfiles() {
        json data = read_json_file(vpnProfilesPath);
        return data.contains("vpn_profiles") ? data["vpn_profiles"] : json::array();
    }

    void addVpnProfile(const json& profile) {
        json data = read_json_file(vpnProfilesPath);
        json profiles = data.contains("vpn_profiles") ? data["vpn_profiles"] : json::array();
        profiles.push_back(profile);
        json updatedData;
        updatedData["vpn_profiles"] = profiles;
        write_json_file(vpnProfilesPath, updatedData);
    }

    void deleteVpnProfile(const std::string& profileId) {
        json data = read_json_file(vpnProfilesPath);
        json profiles = data.contains("vpn_profiles") ? data["vpn_profiles"] : json::array();
        for (auto it = profiles.begin(); it != profiles.end(); ++it) {
            if (it->value("id", "") == profileId) {
                profiles.erase(it);
                break;
            }
        }
        json updatedData;
        updatedData["vpn_profiles"] = profiles;
        write_json_file(vpnProfilesPath, updatedData);
    }

    void updateVpnProfile(const std::string& profileId, const json& updatedProfile) {
        json data = read_json_file(vpnProfilesPath);
        json profiles = data.contains("vpn_profiles") ? data["vpn_profiles"] : json::array();
        for (auto& profile : profiles) {
            if (profile.value("id", "") == profileId) {
                profile = updatedProfile; // Replace the entire profile
                break;
            }
        }
        json updatedData;
        updatedData["vpn_profiles"] = profiles;
        write_json_file(vpnProfilesPath, updatedData);
    }
};

// Global instance of VpnDataManager
VpnDataManager vpnDataManager;

using json = nlohmann::json;

VpnRouter::VpnRouter() {
    std::cout << "VpnRouter: Initializing VPN router..." << std::endl;
}

VpnRouter::~VpnRouter() {
    std::cout << "VpnRouter: Cleaning up VPN router..." << std::endl;
}

void VpnRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler) {
    std::cout << "VpnRouter: Registering VPN routes..." << std::endl;

    // VPN status and configuration endpoints
    addRouteHandler("/api/vpn/status", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnStatus(method, params, body);
    });

    addRouteHandler("/api/vpn/config", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnConfig(method, params, body);
    });

    // VPN connection management
    addRouteHandler("/api/vpn/connect", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnConnect(method, params, body);
    });

    addRouteHandler("/api/vpn/disconnect", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnDisconnect(method, params, body);
    });

    // VPN profile management
    addRouteHandler("/api/vpn/profiles", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnProfiles(method, params, body);
    });

    // Deprecated: Use POST to /api/vpn/profiles for create/update
    // addRouteHandler("/api/vpn/profiles/create", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    //     return this->handleVpnProfileCreate(method, params, body);
    // });

    // Deprecated: Use DELETE to /api/vpn/profiles/{id} or POST to /api/vpn/profiles/delete
    // addRouteHandler("/api/vpn/profiles/delete", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    //     return this->handleVpnProfileDelete(method, params, body);
    // });

    // VPN events and monitoring
    addRouteHandler("/api/vpn/events", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnEvents(method, params, body);
    });

    addRouteHandler("/api/vpn/logs", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnLogs(method, params, body);
    });

    // Additional VPN endpoints for frontend compatibility
    addRouteHandler("/api/vpn/active", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnActive(method, params, body);
    });

    // Routing Rules Endpoint
    addRouteHandler("/api/vpn/routing-rules", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnRoutingRules(method, params, body);
    });

    // Security Settings Endpoint
    addRouteHandler("/api/vpn/sec-settings", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnSecuritySettings(method, params, body);
    });

    // VPN Parser Endpoints
    addRouteHandler("/api/vpn-parser/openvpn-parser", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleOpenVPNParser(method, params, body);
    });

    addRouteHandler("/api/vpn-parser/ikev2-parser", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleIKEv2Parser(method, params, body);
    });

    addRouteHandler("/api/vpn-parser/wireguard-parser", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleWireGuardParser(method, params, body);
    });

    // Centralized VPN Parser Endpoint
    addRouteHandler("/api/vpn-parser/parse", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleVpnParseConfig(method, params, body);
    });

    std::cout << "VpnRouter: All VPN routes registered successfully" << std::endl;
}

// VPN Status Endpoint - GET/POST
std::string VpnRouter::handleVpnStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET" || method == "POST") {
            json statusData = getVpnStatus();

            response = {
                {"success", true},
                {"vpn", statusData},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()},
                {"response_time_ms", 0.0}
            };
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET", "POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// VPN Configuration Endpoint - GET/POST/PUT
std::string VpnRouter::handleVpnConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET") {
            json configData = getVpnConfiguration();
            response = {
                {"success", true},
                {"config", configData},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else if (method == "POST" || method == "PUT") {
            json requestData = json::parse(body);

            // Simulate configuration update
            response = {
                {"success", true},
                {"message", "VPN configuration updated successfully"},
                {"config_updated", true},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET", "POST", "PUT"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// VPN Connect Endpoint - POST
std::string VpnRouter::handleVpnConnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "POST") {
            json requestData = json::parse(body);
            std::string profileId = requestData.value("profile_id", "");

            if (!profileId.empty()) {
                json connectResult = connectToVpn(profileId);

                response = {
                    {"success", true},
                    {"message", "VPN connection initiated"},
                    {"connection_status", "connecting"},
                    {"profile_id", profileId},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                };
            } else {
                response = {
                    {"success", false},
                    {"error", "Missing profile_id parameter"},
                    {"required_fields", {"profile_id"}}
                };
            }
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// VPN Disconnect Endpoint - POST
std::string VpnRouter::handleVpnDisconnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "POST") {
            json disconnectResult = disconnectVpn();

            response = {
                {"success", true},
                {"message", "VPN disconnection initiated"},
                {"connection_status", "disconnecting"},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// VPN Profiles Endpoint - GET/POST/PUT/DELETE
std::string VpnRouter::handleVpnProfiles(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET") {
            json profilesData = vpnDataManager.getVpnProfiles();

            response = {
                {"success", true},
                {"profiles", profilesData},
                {"total_profiles", profilesData.size()},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else if (method == "POST") {
            json requestData = json::parse(body);

            // Ensure required fields are present
            if (!requestData.contains("name") || !requestData.contains("server")) {
                response = {
                    {"success", false},
                    {"error", "Missing required fields: name and server"},
                    {"required_fields", {"name", "server"}}
                };
                return response.dump();
            }

            // Add default values if not present
            if (!requestData.contains("id")) {
                requestData["id"] = "profile_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
            }
            if (!requestData.contains("created_date")) {
                auto now = std::chrono::system_clock::now();
                auto time_t = std::chrono::system_clock::to_time_t(now);
                std::stringstream ss;
                ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
                requestData["created_date"] = ss.str();
            }
            if (!requestData.contains("last_used")) {
                requestData["last_used"] = "Never";
            }
            if (!requestData.contains("status")) {
                requestData["status"] = "Ready";
            }
            if (!requestData.contains("auth_method")) {
                requestData["auth_method"] = "Certificate";
            }
            if (!requestData.contains("encryption")) {
                requestData["encryption"] = "AES-256";
            }

            // Store configuration file content if provided
            if (requestData.contains("config_file_content")) {
                requestData["config_file_content"] = requestData["config_file_content"];
                requestData["config_file_name"] = requestData.value("config_file_name", "config.conf");
                requestData["config_parsed_from"] = requestData.value("config_parsed_from", "manual");
                requestData["config_upload_timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
            }

            // Add connection statistics structure
            if (!requestData.contains("connection_stats")) {
                requestData["connection_stats"] = {
                    {"successful_connections", 0},
                    {"failed_connections", 0},
                    {"total_connections", 0},
                    {"total_data_transferred", "0MB"},
                    {"average_connection_time", 0}
                };
            }

            vpnDataManager.addVpnProfile(requestData);
            response = {
                {"success", true},
                {"message", "VPN profile created successfully"},
                {"profile", requestData},
                {"config_stored", requestData.contains("config_file_content")},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else if (method == "PUT") {
            try {
                json requestData = json::parse(body);
                std::string profileId = requestData.value("id", "");

                if (profileId.empty()) {
                    response["success"] = false;
                    response["error"] = "Missing profile id for update";
                    return response.dump();
                }

                // Update the profile
                vpnDataManager.updateVpnProfile(profileId, requestData);
                response["success"] = true;
                response["message"] = "VPN profile updated successfully";
                response["profile_id"] = profileId;

            } catch (const std::exception& e) {
                response["success"] = false;
                response["error"] = "Failed to update VPN profile: " + std::string(e.what());
            }
        } else if (method == "DELETE") {
            try {
                json requestData = json::parse(body);
                std::string profileId = requestData.value("profile_id", "");

                if (profileId.empty()) {
                    response["success"] = false;
                    response["error"] = "Missing profile_id parameter";
                    return response.dump();
                }

                vpnDataManager.deleteVpnProfile(profileId);
                response["success"] = true;
                response["message"] = "VPN profile deleted successfully";
                response["profile_id"] = profileId;

            } catch (const std::exception& e) {
                response["success"] = false;
                response["error"] = "Failed to delete VPN profile: " + std::string(e.what());
            }
        }
        else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET", "POST", "PUT", "DELETE"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}


// VPN Events Endpoint - GET/POST
std::string VpnRouter::handleVpnEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET" || method == "POST") {
            json eventsData = getVpnEvents();

            response = {
                {"success", true},
                {"events", eventsData},
                {"event_count", eventsData.size()},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET", "POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// VPN Logs Endpoint - GET
std::string VpnRouter::handleVpnLogs(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET") {
            json logsData = getVpnLogs();

            response = {
                {"success", true},
                {"logs", logsData},
                {"log_count", logsData.size()},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// Helper function to get active VPN connections
json VpnRouter::getVpnStatus() {
    return {
        {"is_connected", false},
        {"connection_status", "Disconnected"},
        {"current_profile", ""},
        {"server_location", "N/A"},
        {"server_ip", "N/A"},
        {"local_ip", "N/A"},
        {"public_ip", "N/A"},
        {"connection_time", 0},
        {"bytes_sent", 0},
        {"bytes_received", 0},
        {"protocol", "N/A"},
        {"encryption", "N/A"},
        {"dns_servers", json::array()},
        {"last_connected", "Never"},
        {"auto_connect", false},
        {"kill_switch", false}
    };
}

// VPN Active Connections Endpoint - GET
std::string VpnRouter::handleVpnActive(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET") {
            json activeData = getVpnActiveConnections();

            response = {
                {"success", true},
                {"active_connections", activeData},
                {"connection_count", activeData.size()},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// VPN Routing Rules Endpoint - GET/POST/PUT/DELETE
std::string VpnRouter::handleVpnRoutingRules(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET") {
            json rulesData = vpnDataManager.getRoutingRules();

            response = {
                {"success", true},
                {"routing_rules", rulesData},
                {"rules_count", rulesData.size()},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else if (method == "POST") {
            json requestData = json::parse(body);

            // Check if this is a delete action
            if (requestData.contains("action") && requestData["action"] == "delete") {
                std::string ruleId = requestData.value("rule_id", "");
                if (!ruleId.empty()) {
                    vpnDataManager.deleteRoutingRule(ruleId);
                    response = {
                        {"success", true},
                        {"message", "Routing rule deleted successfully"},
                        {"rule_id", ruleId},
                        {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };
                } else {
                    response = {
                        {"success", false},
                        {"error", "Missing rule_id parameter"},
                        {"required_fields", {"rule_id"}}
                    };
                }
            } else {
                // Add new routing rule
                // Ensure required fields are present
                if (!requestData.contains("name") || !requestData.contains("source_value")) {
                    response = {
                        {"success", false},
                        {"error", "Missing required fields: name and source_value"},
                        {"required_fields", {"name", "source_value"}}
                    };
                    return response.dump();
                }

                // Add default values if not present
                if (!requestData.contains("id")) {
                    requestData["id"] = "rule_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());
                }
                if (!requestData.contains("created_date")) {
                    auto now = std::chrono::system_clock::now();
                    auto time_t = std::chrono::system_clock::to_time_t(now);
                    std::stringstream ss;
                    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
                    requestData["created_date"] = ss.str();
                }
                if (!requestData.contains("last_modified")) {
                    auto now = std::chrono::system_clock::now();
                    auto time_t = std::chrono::system_clock::to_time_t(now);
                    std::stringstream ss;
                    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d");
                    requestData["last_modified"] = ss.str();
                }
                if (!requestData.contains("type")) {
                    requestData["type"] = "tunnel_all";
                }
                if (!requestData.contains("gateway")) {
                    requestData["gateway"] = "VPN Server";
                }

                vpnDataManager.addRoutingRule(requestData);
                response = {
                    {"success", true},
                    {"message", "Routing rule created successfully"},
                    {"rule", requestData},
                    {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                };
            }
        } else if (method == "PUT") {
            try {
                json requestData = json::parse(body);
                std::string ruleId = requestData.value("id", "");

                if (ruleId.empty()) {
                    response["success"] = false;
                    response["error"] = "Missing rule id for update";
                    return response.dump();
                }

                // Update the rule
                vpnDataManager.updateRoutingRule(ruleId, requestData);
                response["success"] = true;
                response["message"] = "Routing rule updated successfully";
                response["rule_id"] = ruleId;

            } catch (const std::exception& e) {
                response["success"] = false;
                response["error"] = "Failed to update routing rule: " + std::string(e.what());
            }
        } else if (method == "DELETE") {
            try {
                if (body.empty()) {
                    response["success"] = false;
                    response["error"] = "Missing request body";
                    return response.dump();
                }

                json requestData = json::parse(body);
                std::string ruleId = requestData.value("rule_id", "");

                if (ruleId.empty()) {
                    response["success"] = false;
                    response["error"] = "Missing rule_id parameter";
                    return response.dump();
                }

                vpnDataManager.deleteRoutingRule(ruleId);
                response["success"] = true;
                response["message"] = "Routing rule deleted successfully";
                response["rule_id"] = ruleId;

            } catch (const std::exception& e) {
                response["success"] = false;
                response["error"] = "Failed to delete routing rule: " + std::string(e.what());
            }
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET", "POST", "PUT", "DELETE"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// VPN Security Settings Endpoint - GET/PUT
std::string VpnRouter::handleVpnSecuritySettings(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "GET") {
            json settingsData = vpnDataManager.getSecuritySettings();
            response = {
                {"success", true},
                {"settings", settingsData},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else if (method == "PUT") {
            json requestData = json::parse(body);
            vpnDataManager.updateSecuritySettings(requestData);
            response = {
                {"success", true},
                {"message", "Security settings updated successfully"},
                {"settings", requestData},
                {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"GET", "PUT"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Internal server error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// Helper function to get active VPN connections
json VpnRouter::getVpnActiveConnections() {
    return json::array({
        {
            {"id", "connection_1"},
            {"profile_name", "Home Office VPN"},
            {"profile_id", "profile_1"},
            {"status", "disconnected"},
            {"server", "vpn.company.com"},
            {"protocol", "OpenVPN"},
            {"connected_since", ""},
            {"bytes_sent", 0},
            {"bytes_received", 0},
            {"last_activity", "Never"}
        }
    });
}

// Placeholder for actual VPN operations
json VpnRouter::getVpnConfiguration() {
    // In a real scenario, this would read from a config file or system settings
    return {
        {"auto_start", false},
        {"auto_connect", false},
        {"kill_switch", true},
        {"dns_leak_protection", true},
        {"ipv6_disable", false},
        {"reconnect_attempts", 3},
        {"connection_timeout", 30}
    };
}

json VpnRouter::connectToVpn(const std::string& profileId) {
    // Placeholder for actual connection logic
    return {
        {"profile_id", profileId},
        {"connection_initiated", true},
        {"status", "connecting"},
        {"estimated_time", 15}
    };
}

json VpnRouter::disconnectVpn() {
    // Placeholder for actual disconnection logic
    return {
        {"disconnection_initiated", true},
        {"status", "disconnecting"},
        {"estimated_time", 5}
    };
}

json VpnRouter::getVpnEvents() {
    // Placeholder for fetching actual VPN events
    return json::array();
}

json VpnRouter::getVpnLogs() {
    // Placeholder for fetching actual VPN logs
    return json::array();
}

// VPN Parser Helper Methods
json VpnRouter::parseOpenVPNConfig(const std::string& configContent) {
    json result;
    json profileData;

    try {
        std::istringstream stream(configContent);
        std::string line;

        // Initialize default values
        profileData["protocol"] = "OpenVPN";
        profileData["port"] = 1194;
        profileData["auth_method"] = "Certificate";
        profileData["auto_connect"] = false;

        // Parse line by line
        while (std::getline(stream, line)) {
            // Remove leading/trailing whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#') continue;

            std::istringstream lineStream(line);
            std::string directive;
            lineStream >> directive;

            if (directive == "remote") {
                std::string server, port;
                lineStream >> server >> port;
                profileData["server"] = server;
                if (!port.empty()) {
                    profileData["port"] = std::stoi(port);
                }
            }
            else if (directive == "proto") {
                std::string proto;
                lineStream >> proto;
                profileData["protocol"] = "OpenVPN (" + proto + ")";
            }
            else if (directive == "port") {
                std::string port;
                lineStream >> port;
                profileData["port"] = std::stoi(port);
            }
            else if (directive == "auth-user-pass") {
                profileData["auth_method"] = "Username/Password";
            }
            else if (directive == "cipher") {
                std::string cipher;
                lineStream >> cipher;
                profileData["encryption"] = cipher;
            }
        }

        // Generate profile name if not set
        if (!profileData.contains("name") || profileData["name"].empty()) {
            std::string serverName = profileData.value("server", "unknown");
            profileData["name"] = "OpenVPN (" + serverName + ")";
        }

        result["success"] = true;
        result["success"] = true;
        result["protocol_detected"] = "OpenVPN";
        result["profile_data"] = profileData;

    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Failed to parse OpenVPN configuration: " + std::string(e.what());
        result["protocol_detected"] = "OpenVPN";
    }

    return result;
}

json VpnRouter::parseIKEv2Config(const std::string& configContent) {
    json result;
    json profileData;

    try {
        std::istringstream stream(configContent);
        std::string line;

        // Initialize default values
        profileData["protocol"] = "IKEv2";
        profileData["port"] = 500;
        profileData["auth_method"] = "Certificate";
        profileData["auto_connect"] = false;

        // Parse line by line
        while (std::getline(stream, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#') continue;

            if (line.find("right=") == 0) {
                std::string server = line.substr(6);
                profileData["server"] = server;
            }
            else if (line.find("leftid=") == 0) {
                std::string username = line.substr(7);
                profileData["username"] = username;
            }
            else if (line.find("conn ") == 0) {
                std::string connName = line.substr(5);
                profileData["name"] = connName + " (IKEv2)";
            }
        }

        // Generate profile name if not set
        if (!profileData.contains("name") || profileData["name"].empty()) {
            std::string serverName = profileData.value("server", "unknown");
            profileData["name"] = "IKEv2 (" + serverName + ")";
        }

        result["success"] = true;
        result["success"] = true;
        result["protocol_detected"] = "IKEv2";
        result["profile_data"] = profileData;

    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Failed to parse IKEv2 configuration: " + std::string(e.what());
        result["protocol_detected"] = "IKEv2";
    }

    return result;
}

json VpnRouter::parseWireGuardConfig(const std::string& configContent) {
    json result;
    json profileData;

    try {
        std::istringstream stream(configContent);
        std::string line;

        // Initialize default values
        profileData["protocol"] = "WireGuard";
        profileData["port"] = 51820;
        profileData["auth_method"] = "Pre-shared Key";
        profileData["auto_connect"] = false;

        bool inInterface = false;
        bool inPeer = false;

        // Parse line by line
        while (std::getline(stream, line)) {
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);

            if (line.empty() || line[0] == '#') continue;

            if (line == "[Interface]") {
                inInterface = true;
                inPeer = false;
            }
            else if (line == "[Peer]") {
                inInterface = false;
                inPeer = true;
            }
            else if (inPeer && line.find("Endpoint = ") == 0) {
                std::string endpoint = line.substr(11);
                size_t colonPos = endpoint.find(':');
                if (colonPos != std::string::npos) {
                    profileData["server"] = endpoint.substr(0, colonPos);
                    profileData["port"] = std::stoi(endpoint.substr(colonPos + 1));
                } else {
                    profileData["server"] = endpoint;
                }
            }
        }

        // Generate profile name if not set
        if (!profileData.contains("name") || profileData["name"].empty()) {
            std::string serverName = profileData.value("server", "unknown");
            profileData["name"] = "WireGuard (" + serverName + ")";
        }

        result["success"] = true;
        result["success"] = true;
        result["protocol_detected"] = "WireGuard";
        result["profile_data"] = profileData;

    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Failed to parse WireGuard configuration: " + std::string(e.what());
        result["protocol_detected"] = "WireGuard";
    }

    return result;
}

// VPN Parser Endpoint Implementations
std::string VpnRouter::handleOpenVPNParser(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "POST") {
            json requestData = json::parse(body);

            if (requestData.contains("config_content")) {
                std::string configContent = requestData["config_content"];
                std::cout << "[VPN-PARSER] Processing OpenVPN config, length: " << configContent.length() << std::endl;

                // Simple OpenVPN parser implementation
                json parseResult = parseOpenVPNConfig(configContent);

                response = parseResult;
                response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                response["parser_type"] = "OpenVPN";
            } else {
                response = {
                    {"success", false},
                    {"error", "Missing config_content parameter"},
                    {"required_fields", {"config_content"}}
                };
            }
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Parser error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

std::string VpnRouter::handleIKEv2Parser(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "POST") {
            json requestData = json::parse(body);

            if (requestData.contains("config_content")) {
                std::string configContent = requestData["config_content"];
                std::cout << "[VPN-PARSER] Processing IKEv2 config, length: " << configContent.length() << std::endl;

                // Simple IKEv2 parser implementation
                json parseResult = parseIKEv2Config(configContent);

                response = parseResult;
                response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                response["parser_type"] = "IKEv2";
            } else {
                response = {
                    {"success", false},
                    {"error", "Missing config_content parameter"},
                    {"required_fields", {"config_content"}}
                };
            }
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Parser error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

std::string VpnRouter::handleWireGuardParser(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "POST") {
            json requestData = json::parse(body);

            if (requestData.contains("config_content")) {
                std::string configContent = requestData["config_content"];
                std::cout << "[VPN-PARSER] Processing WireGuard config, length: " << configContent.length() << std::endl;

                // Simple WireGuard parser implementation
                json parseResult = parseWireGuardConfig(configContent);

                response = parseResult;
                response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                response["parser_type"] = "WireGuard";
            } else {
                response = {
                    {"success", false},
                    {"error", "Missing config_content parameter"},
                    {"required_fields", {"config_content"}}
                };
            }
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Parser error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// Centralized VPN Parser Handler using VPN Parser Manager
std::string VpnRouter::handleVpnParseConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    json response;

    try {
        if (method == "POST") {
            json requestData = json::parse(body);

            if (requestData.contains("config_content")) {
                std::string configContent = requestData["config_content"];
                std::cout << "[VPN-PARSER] Processing VPN config using centralized parser, length: " << configContent.length() << std::endl;

                // Use the VPN Parser Manager for intelligent parsing
                json parseResult = VPNParserManager::parseVPNConfig(configContent);

                response = parseResult;
                response["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                response["parser_type"] = "Centralized VPN Parser Manager";
            } else {
                response = {
                    {"success", false},
                    {"error", "Missing config_content parameter"},
                    {"required_fields", {"config_content"}}
                };
            }
        } else {
            response = {
                {"success", false},
                {"error", "Method not allowed"},
                {"supported_methods", {"POST"}}
            };
        }
    } catch (const std::exception& e) {
        response = {
            {"success", false},
            {"error", "Parser error"},
            {"details", e.what()}
        };
    }

    return response.dump();
}

// Helper function to get active VPN connections