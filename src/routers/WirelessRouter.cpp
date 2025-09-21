#include "WirelessRouter.h"
#include "cellular_data_manager.h"
#include <iostream>
#include <chrono>
#include <nlohmann/json.hpp>
#include <fstream> // Required for file operations
#include <iomanip> // Required for time formatting
#include <sstream> // Required for stringstream

using json = nlohmann::json;

WirelessRouter::WirelessRouter() {
    std::cout << "WirelessRouter: Initializing wireless router..." << std::endl;
}

WirelessRouter::~WirelessRouter() {
    std::cout << "WirelessRouter: Cleaning up wireless router..." << std::endl;
}

void WirelessRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler) {
    std::cout << "WirelessRouter: Registering wireless routes matching frontend expectations..." << std::endl;

    // Exact endpoints that the frontend expects
    addRouteHandler("/api/wireless/events", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleWirelessEvents(method, params, body);
    });

    addRouteHandler("/api/wireless/status", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleWirelessStatus(method, params, body);
    });

    addRouteHandler("/api/wireless/radio", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleRadioControl(method, params, body);
    });

    addRouteHandler("/api/wireless/scan", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleNetworkScan(method, params, body);
    });

    addRouteHandler("/api/wireless/connect", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleNetworkConnect(method, params, body);
    });

    addRouteHandler("/api/wireless/saved-networks", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleSavedNetworks(method, params, body);
    });

    addRouteHandler("/api/wireless/ap/start", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleAccessPointStart(method, params, body);
    });

    addRouteHandler("/api/wireless/ap/stop", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleAccessPointStop(method, params, body);
    });

    // Cellular configuration endpoints
    addRouteHandler("/api/cellular/status", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularStatus(method, params, body);
    });

    addRouteHandler("/api/cellular/config", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularConfig(method, params, body);
    });

    addRouteHandler("/api/cellular/connect", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularConnect(method, params, body);
    });

    addRouteHandler("/api/cellular/disconnect", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularDisconnect(method, params, body);
    });

    addRouteHandler("/api/cellular/scan", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularScan(method, params, body);
    });

    addRouteHandler("/api/cellular/events", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularEvents(method, params, body);
    });

    // SIM Management endpoints
    addRouteHandler("/api/cellular/sim/unlock", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleSIMUnlock(method, params, body);
    });

    addRouteHandler("/api/cellular/reset-data", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularResetData(method, params, body);
    });

    addRouteHandler("/api/cellular/reset-config", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleCellularResetConfig(method, params, body);
    });

    // Wireless data endpoints
    addRouteHandler("/api/wireless/saved-networks-data", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleSavedNetworksData(method, params, body);
    });

    addRouteHandler("/api/wireless/available-networks-data", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleAvailableNetworksData(method, params, body);
    });

    addRouteHandler("/api/wireless/manual-connect-data", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleManualConnectData(method, params, body);
    });

    addRouteHandler("/api/wireless/ap-config-data", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleAccessPointConfigData(method, params, body);
    });

    addRouteHandler("/api/wireless/ap-status-data", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        return this->handleAccessPointStatusData(method, params, body);
    });

    std::cout << "WirelessRouter: All wireless and cellular routes registered successfully" << std::endl;
}

std::string WirelessRouter::handleWirelessEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-EVENTS] Processing wireless events request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[WIRELESS-EVENTS] POST request for event processing" << std::endl;

        json event_response;
        event_response["success"] = true;
        event_response["message"] = "Wireless event processed";
        event_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto event_data = json::parse(body);

                // Extract event details that frontend sends
                std::string event_type = event_data.value("event_type", "unknown");
                std::string element_id = event_data.value("element_id", "unknown");
                std::string page = event_data.value("page", "unknown");

                std::cout << "[WIRELESS-EVENTS] Event Type: " << event_type << std::endl;
                std::cout << "[WIRELESS-EVENTS] Element ID: " << element_id << std::endl;
                std::cout << "[WIRELESS-EVENTS] Page: " << page << std::endl;

                if (event_data.contains("event_data")) {
                    auto inner_data = event_data["event_data"];
                    if (inner_data.contains("action")) {
                        std::cout << "[WIRELESS-EVENTS] Action: " << inner_data["action"].get<std::string>() << std::endl;
                    }
                    if (inner_data.contains("state")) {
                        std::cout << "[WIRELESS-EVENTS] State: " << inner_data["state"].dump() << std::endl;
                    }
                }

                event_response["event_data"] = event_data;
                event_response["processed"] = true;
                std::cout << "[WIRELESS-EVENTS] Event processed successfully" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "[WIRELESS-EVENTS] Warning: Could not parse event data: " << e.what() << std::endl;
                event_response["processed"] = false;
                event_response["error"] = e.what();
            }
        }

        return event_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for event processing."}};
    return error.dump();
}

std::string WirelessRouter::handleWirelessStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-STATUS] Processing wireless status request, method: " << method << std::endl;

    if (method == "GET") {
        std::cout << "[WIRELESS-STATUS] GET request received" << std::endl;

        json response;
        response["success"] = true;
        response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        // Match frontend currentState structure exactly
        response["wifi_radio_enabled"] = true;
        response["active_mode"] = "client"; // 'client' or 'ap'
        response["scan_in_progress"] = false;
        response["ap_active"] = false;

        // Additional status data
        response["hardware_available"] = true;
        response["driver_loaded"] = true;
        response["interface_name"] = "wlan0";
        response["connection_status"] = "connected";
        response["current_ssid"] = "OfficeNetwork";
        response["signal_strength"] = -58;

        std::cout << "[WIRELESS-STATUS] Status data returned" << std::endl;
        return response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use GET for status."}};
    return error.dump();
}

std::string WirelessRouter::handleRadioControl(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-RADIO] Processing radio control request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[WIRELESS-RADIO] POST request for radio control" << std::endl;

        json radio_response;
        radio_response["success"] = true;
        radio_response["message"] = "Wireless radio control processed";
        radio_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto request_data = json::parse(body);

                // Extract radio control data that frontend sends
                std::string action = request_data.value("action", "unknown");
                bool enabled = request_data.value("enabled", false);

                std::cout << "[WIRELESS-RADIO] Action: " << action << std::endl;
                std::cout << "[WIRELESS-RADIO] Enabled: " << (enabled ? "true" : "false") << std::endl;

                radio_response["radio_enabled"] = enabled;
                radio_response["radio_status"] = enabled ? "enabled" : "disabled";
                radio_response["action_processed"] = action;

                std::cout << "[WIRELESS-RADIO] Radio control processed successfully" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "[WIRELESS-RADIO] Warning: Could not parse request data: " << e.what() << std::endl;
                radio_response["error"] = e.what();
            }
        }

        return radio_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for radio control."}};
    return error.dump();
}

std::string WirelessRouter::handleNetworkScan(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-SCAN] Processing network scan request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[WIRELESS-SCAN] POST request for network scan" << std::endl;

        json scan_response;
        scan_response["success"] = true;
        scan_response["message"] = "Network scan initiated";
        scan_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto request_data = json::parse(body);
                std::string action = request_data.value("action", "unknown");

                std::cout << "[WIRELESS-SCAN] Action: " << action << std::endl;

                scan_response["scan_status"] = "in_progress";
                scan_response["estimated_duration_seconds"] = 5;
                scan_response["action_processed"] = action;

                // Return available networks data
                scan_response["networks_found"] = json::array({
                    {
                        {"ssid", "HomeWiFi_5G"},
                        {"security", "WPA3"},
                        {"frequency", "5GHz"},
                        {"signal_strength_dbm", -42},
                        {"signal_bars", 4},
                        {"channel", 36}
                    },
                    {
                        {"ssid", "OfficeNetwork"},
                        {"security", "WPA2"},
                        {"frequency", "2.4GHz"},
                        {"signal_strength_dbm", -58},
                        {"signal_bars", 3},
                        {"channel", 6}
                    },
                    {
                        {"ssid", "GuestNetwork"},
                        {"security", "Open"},
                        {"frequency", "2.4GHz"},
                        {"signal_strength_dbm", -65},
                        {"signal_bars", 2},
                        {"channel", 11}
                    }
                });

                std::cout << "[WIRELESS-SCAN] Network scan processed successfully" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "[WIRELESS-SCAN] Warning: Could not parse request data: " << e.what() << std::endl;
                scan_response["error"] = e.what();
            }
        }

        return scan_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for network scan."}};
    return error.dump();
}

std::string WirelessRouter::handleNetworkConnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-CONNECT] Processing network connection request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[WIRELESS-CONNECT] POST request for network connection" << std::endl;

        json connect_response;
        connect_response["success"] = true;
        connect_response["message"] = "Network connection initiated";
        connect_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto request_data = json::parse(body);

                // Extract connection data that frontend sends
                std::string ssid = request_data.value("ssid", "unknown");
                std::string security_type = request_data.value("security_type", "unknown");
                std::string action = request_data.value("action", "unknown");

                std::cout << "[WIRELESS-CONNECT] SSID: " << ssid << std::endl;
                std::cout << "[WIRELESS-CONNECT] Security Type: " << security_type << std::endl;
                std::cout << "[WIRELESS-CONNECT] Action: " << action << std::endl;

                if (request_data.contains("password")) {
                    std::cout << "[WIRELESS-CONNECT] Password: [PROTECTED]" << std::endl;
                }

                if (request_data.contains("frequency")) {
                    std::cout << "[WIRELESS-CONNECT] Frequency: " << request_data["frequency"].get<std::string>() << std::endl;
                }

                if (request_data.contains("is_hidden_network")) {
                    bool is_hidden = request_data["is_hidden_network"].get<bool>();
                    std::cout << "[WIRELESS-CONNECT] Hidden Network: " << (is_hidden ? "Yes" : "No") << std::endl;
                }

                connect_response["connection_status"] = "attempting";
                connect_response["estimated_time_seconds"] = 10;
                connect_response["ssid"] = ssid;
                connect_response["action_processed"] = action;

                std::cout << "[WIRELESS-CONNECT] Network connection processed successfully" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "[WIRELESS-CONNECT] Warning: Could not parse request data: " << e.what() << std::endl;
                connect_response["error"] = e.what();
            }
        }

        return connect_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for network connection."}};
    return error.dump();
}

std::string WirelessRouter::handleSavedNetworks(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-SAVED] Processing saved networks request, method: " << method << std::endl;

    if (method == "GET") {
        std::cout << "[WIRELESS-SAVED] GET request for saved networks" << std::endl;

        try {
            json savedNetworksData = loadSavedNetworks();
            
            json response;
            response["success"] = true;
            response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            response["saved_networks"] = savedNetworksData["saved_networks"];
            response["last_updated"] = savedNetworksData["last_updated"];

            std::cout << "[WIRELESS-SAVED] Saved networks data loaded from file" << std::endl;
            return response.dump();
        } catch (const std::exception& e) {
            std::cout << "[WIRELESS-SAVED] Error loading saved networks: " << e.what() << std::endl;
            json error = {{"success", false}, {"message", "Failed to load saved networks"}, {"error", e.what()}};
            return error.dump();
        }
    }

    if (method == "DELETE") {
        std::cout << "[WIRELESS-SAVED] DELETE request for forgetting network" << std::endl;

        json delete_response;
        delete_response["success"] = false;
        delete_response["message"] = "Network forget failed";
        delete_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto request_data = json::parse(body);
                std::string network_id = request_data.value("network_id", "unknown");
                std::string ssid = request_data.value("ssid", "unknown");

                std::cout << "[WIRELESS-SAVED] Network ID: " << network_id << std::endl;
                std::cout << "[WIRELESS-SAVED] SSID: " << ssid << std::endl;

                // Load current saved networks
                json savedNetworksData = loadSavedNetworks();
                auto& networks = savedNetworksData["saved_networks"];
                
                // Remove the network
                auto it = std::remove_if(networks.begin(), networks.end(),
                    [&network_id, &ssid](const json& network) {
                        return network.value("network_id", "") == network_id || 
                               network.value("ssid", "") == ssid;
                    });
                
                if (it != networks.end()) {
                    networks.erase(it, networks.end());
                    savedNetworksData["last_updated"] = getCurrentTimestamp();
                    
                    if (saveSavedNetworks(savedNetworksData)) {
                        delete_response["success"] = true;
                        delete_response["message"] = "Network forgotten successfully";
                        delete_response["network_id"] = network_id;
                        delete_response["ssid"] = ssid;
                        std::cout << "[WIRELESS-SAVED] Network forgotten and saved to file" << std::endl;
                    } else {
                        delete_response["message"] = "Failed to save updated networks";
                    }
                } else {
                    delete_response["message"] = "Network not found";
                }

            } catch (const std::exception& e) {
                std::cout << "[WIRELESS-SAVED] Error: " << e.what() << std::endl;
                delete_response["error"] = e.what();
            }
        }

        return delete_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use GET or DELETE for saved networks."}};
    return error.dump();
}

std::string WirelessRouter::handleAccessPointStart(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-AP-START] Processing access point start request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[WIRELESS-AP-START] POST request for starting access point" << std::endl;

        json ap_response;
        ap_response["success"] = true;
        ap_response["message"] = "Access point start initiated";
        ap_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto request_data = json::parse(body);

                // Extract AP configuration that frontend sends
                std::string ssid = request_data.value("ssid", "Office-AP");
                std::string security_type = request_data.value("security_type", "wpa3");
                int max_clients = request_data.value("max_clients", 10);
                std::string band_selection = request_data.value("band_selection", "dual");
                std::string channel = request_data.value("channel", "auto");
                bool dhcp_enabled = request_data.value("dhcp_enabled", true);

                std::cout << "[WIRELESS-AP-START] SSID: " << ssid << std::endl;
                std::cout << "[WIRELESS-AP-START] Security Type: " << security_type << std::endl;
                std::cout << "[WIRELESS-AP-START] Max Clients: " << max_clients << std::endl;
                std::cout << "[WIRELESS-AP-START] Band Selection: " << band_selection << std::endl;
                std::cout << "[WIRELESS-AP-START] Channel: " << channel << std::endl;
                std::cout << "[WIRELESS-AP-START] DHCP Enabled: " << (dhcp_enabled ? "Yes" : "No") << std::endl;

                if (request_data.contains("password")) {
                    std::cout << "[WIRELESS-AP-START] Password: [PROTECTED]" << std::endl;
                }

                ap_response["ap_status"] = "starting";
                ap_response["estimated_startup_time_seconds"] = 15;
                ap_response["ap_config"] = {
                    {"ssid", ssid},
                    {"security_type", security_type},
                    {"max_clients", max_clients},
                    {"band_selection", band_selection},
                    {"channel", channel},
                    {"dhcp_enabled", dhcp_enabled}
                };

                std::cout << "[WIRELESS-AP-START] Access point start processed successfully" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "[WIRELESS-AP-START] Warning: Could not parse request data: " << e.what() << std::endl;
                ap_response["error"] = e.what();
            }
        }

        return ap_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for starting access point."}};
    return error.dump();
}

std::string WirelessRouter::handleAccessPointStop(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-AP-STOP] Processing access point stop request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[WIRELESS-AP-STOP] POST request for stopping access point" << std::endl;

        json ap_response;
        ap_response["success"] = true;
        ap_response["message"] = "Access point stop initiated";
        ap_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto request_data = json::parse(body);

                std::string action = request_data.value("action", "unknown");

                std::cout << "[WIRELESS-AP-STOP] Action: " << action << std::endl;

                ap_response["ap_status"] = "stopping";
                ap_response["action_processed"] = action;

                std::cout << "[WIRELESS-AP-STOP] Access point stop processed successfully" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "[WIRELESS-AP-STOP] Warning: Could not parse request data: " << e.what() << std::endl;
                ap_response["error"] = e.what();
            }
        }

        return ap_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for stopping access point."}};
    return error.dump();
}

// ================================
// CELLULAR CONFIGURATION ENDPOINTS
// ================================

std::string WirelessRouter::handleCellularStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-STATUS] Processing cellular status request, method: " << method << std::endl;

    if (method == "GET") {
        std::cout << "[CELLULAR-STATUS] GET request received - loading from data file" << std::endl;

        try {
            // Load actual data from JSON file using CellularDataManager
            auto statusData = CellularDataManager::getCurrentStatus();

            json response;
            response["success"] = true;
            response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // Add all cellular status fields to response
            for (auto& [key, value] : statusData.items()) {
                response[key] = value;
            }

            std::cout << "[CELLULAR-STATUS] Comprehensive status data loaded from separate files and returned" << std::endl;
            return response.dump();

        } catch (const std::exception& e) {
            std::cout << "[CELLULAR-STATUS] Error loading data: " << e.what() << std::endl;
            json error = {{"success", false}, {"message", "Failed to load cellular status"}, {"error", e.what()}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use GET for cellular status."}};
    return error.dump();
}

std::string WirelessRouter::handleCellularConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-CONFIG] Processing cellular configuration request, method: " << method << std::endl;

    if (method == "GET") {
        std::cout << "[CELLULAR-CONFIG] GET request - returning current configuration using CellularDataManager" << std::endl;

        try {
            auto configData = CellularDataManager::getCellularConfiguration();

            json config_response;
            config_response["success"] = true;
            config_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            config_response["configuration"] = configData;

            std::cout << "[CELLULAR-CONFIG] Configuration loaded successfully from CellularDataManager" << std::endl;
            return config_response.dump();

        } catch (const std::exception& e) {
            std::cout << "[CELLULAR-CONFIG] Error loading configuration: " << e.what() << std::endl;
            json error = {{"success", false}, {"message", "Failed to load cellular configuration"}, {"error", e.what()}};
            return error.dump();
        }

    } else if (method == "POST") {
        std::cout << "[CELLULAR-CONFIG] POST request for configuration update using CellularDataManager" << std::endl;

        if (!body.empty()) {
            try {
                // Parse the configuration data from request body
                auto config_data = json::parse(body);

                bool updateResult = CellularDataManager::updateCellularConfiguration(config_data);

                json config_response;
                if (updateResult) {
                    config_response["success"] = true;
                    config_response["message"] = "Configuration updated successfully";
                    config_response["applied_configuration"] = config_data;
                    std::cout << "[CELLULAR-CONFIG] Configuration updated successfully via CellularDataManager" << std::endl;
                } else {
                    config_response["success"] = false;
                    config_response["message"] = "Failed to save cellular configuration";
                }

                config_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                return config_response.dump();

            } catch (const std::exception& e) {
                std::cout << "[CELLULAR-CONFIG] Error parsing configuration data: " << e.what() << std::endl;
                json error_response;
                error_response["success"] = false;
                error_response["message"] = "Failed to parse cellular configuration";
                error_response["error"] = e.what();
                error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                return error_response.dump();
            }
        } else {
            json error = {{"success", false}, {"message", "Request body required for configuration update"}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use GET or POST for cellular configuration."}};
    return error.dump();
}

std::string WirelessRouter::handleCellularConnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-CONNECT] Processing cellular connect request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[CELLULAR-CONNECT] POST request for cellular connection" << std::endl;

        if (!body.empty()) {
            try {
                auto connect_data = json::parse(body);

                std::string apn = connect_data.value("apn", "");
                std::string username = connect_data.value("username", "");
                std::string auth_type = connect_data.value("auth_type", "none");
                std::string preferred_network_type = connect_data.value("preferred_network_type", "auto");
                bool save_config = connect_data.value("save_config", true);
                bool auto_connect = connect_data.value("auto_connect", false);
                bool data_roaming = connect_data.value("data_roaming", false);

                std::cout << "[CELLULAR-CONNECT] APN: " << apn << std::endl;
                std::cout << "[CELLULAR-CONNECT] Username: " << username << std::endl;
                std::cout << "[CELLULAR-CONNECT] Auth Type: " << auth_type << std::endl;
                std::cout << "[CELLULAR-CONNECT] Network Type: " << preferred_network_type << std::endl;
                std::cout << "[CELLULAR-CONNECT] Save Config: " << (save_config ? "Yes" : "No") << std::endl;

                if (connect_data.contains("password")) {
                    std::cout << "[CELLULAR-CONNECT] Password: [PROTECTED]" << std::endl;
                }

                // Validate APN requirement
                if (apn.empty()) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["message"] = "APN is required for cellular connection";
                    error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    return error_response.dump();
                }

                // Update connection status using CellularDataManager
                json connectionUpdate = {{"is_connected", false}, {"connection_status", "Connecting"}};
                bool statusUpdateResult = CellularDataManager::updateConnectionDetails(connectionUpdate);

                // Save configuration if requested
                if (save_config) {
                    json configToSave = {
                        {"apn", apn},
                        {"username", username},
                        {"auth_type", auth_type},
                        {"preferred_network_type", preferred_network_type},
                        {"auto_connect", auto_connect},
                        {"data_roaming", data_roaming}
                    };

                    if (connect_data.contains("password")) {
                        configToSave["password"] = connect_data["password"];
                    }

                    CellularDataManager::updateCellularConfiguration(configToSave);
                }

                json connect_response;
                connect_response["success"] = true;
                connect_response["message"] = "Cellular connection initiated";
                connect_response["connection_status"] = "Connecting";
                connect_response["estimated_connection_time_seconds"] = 30;
                connect_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                connect_response["connect_config"] = {
                    {"apn", apn},
                    {"username", username},
                    {"auth_type", auth_type},
                    {"preferred_network_type", preferred_network_type},
                    {"save_config", save_config}
                };

                std::cout << "[CELLULAR-CONNECT] Connection initiated and state updated via CellularDataManager" << std::endl;
                return connect_response.dump();

            } catch (const std::exception& e) {
                std::cout << "[CELLULAR-CONNECT] Error processing connect request: " << e.what() << std::endl;
                json error_response;
                error_response["success"] = false;
                error_response["message"] = "Failed to process connection request";
                error_response["error"] = e.what();
                error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                return error_response.dump();
            }
        } else {
            json error = {{"success", false}, {"message", "Request body required for connection"}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for cellular connection."}};
    return error.dump();
}

std::string WirelessRouter::handleCellularDisconnect(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-DISCONNECT] Processing cellular disconnect request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[CELLULAR-DISCONNECT] POST request for cellular disconnection" << std::endl;

        try {
            // Update connection status using CellularDataManager
            json connectionUpdate = {{"is_connected", false}, {"connection_status", "Disconnecting"}};
            bool statusUpdateResult = CellularDataManager::updateConnectionDetails(connectionUpdate);

            json disconnect_response;
            disconnect_response["success"] = true;
            disconnect_response["message"] = "Cellular disconnection initiated";
            disconnect_response["connection_status"] = "Disconnecting";
            disconnect_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (!body.empty()) {
                try {
                    auto disconnect_data = json::parse(body);

                    std::string action = disconnect_data.value("action", "disconnect");
                    bool save_state = disconnect_data.value("save_state", false);

                    std::cout << "[CELLULAR-DISCONNECT] Action: " << action << std::endl;
                    std::cout << "[CELLULAR-DISCONNECT] Save State: " << (save_state ? "Yes" : "No") << std::endl;

                    disconnect_response["action_processed"] = action;
                    disconnect_response["save_state"] = save_state;

                } catch (const std::exception& e) {
                    std::cout << "[CELLULAR-DISCONNECT] Warning: Could not parse disconnect data: " << e.what() << std::endl;
                    disconnect_response["parse_warning"] = e.what();
                }
            }

            std::cout << "[CELLULAR-DISCONNECT] Disconnection initiated and state updated via CellularDataManager" << std::endl;
            return disconnect_response.dump();

        } catch (const std::exception& e) {
            std::cout << "[CELLULAR-DISCONNECT] Error processing disconnect request: " << e.what() << std::endl;
            json error_response;
            error_response["success"] = false;
            error_response["message"] = "Failed to process disconnection request";
            error_response["error"] = e.what();
            error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return error_response.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for cellular disconnection."}};
    return error.dump();
}

std::string WirelessRouter::handleCellularScan(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-SCAN] Processing cellular scan request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[CELLULAR-SCAN] POST request for cellular network scan" << std::endl;

        try {
            // Load scan data from scan-data.json
            auto availableNetworks = CellularDataManager::getAvailableNetworks();
            auto scanStatus = CellularDataManager::getScanStatus();

            // Update scan status to in_progress initially
            json updatedScanStatus = scanStatus;
            updatedScanStatus["scan_completed"] = true;
            updatedScanStatus["scan_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            updatedScanStatus["networks_found"] = availableNetworks.size();

            // Update scan results in data file
            CellularDataManager::updateScanStatus(updatedScanStatus);

            json scan_response;
            scan_response["success"] = true;
            scan_response["message"] = "Network scan completed successfully";
            scan_response["networks_found"] = availableNetworks.size();
            scan_response["available_networks"] = availableNetworks;
            scan_response["scan_status"] = scanStatus;
            scan_response["estimated_scan_time_seconds"] = 15;

            std::cout << "[CELLULAR-SCAN] Scan completed successfully with " << availableNetworks.size() << " networks found" << std::endl;
            return scan_response.dump();

        } catch (const std::exception& e) {
            std::cout << "[CELLULAR-SCAN] Error processing scan request: " << e.what() << std::endl;
            json error_response;
            error_response["success"] = false;
            error_response["message"] = "Failed to process network scan";
            error_response["error"] = e.what();
            error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return error_response.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for cellular network scan."}};
    return error.dump();
}

std::string WirelessRouter::handleCellularEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-EVENTS] Processing cellular events request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[CELLULAR-EVENTS] POST request for event processing with data persistence" << std::endl;

        json event_response;
        event_response["success"] = true;
        event_response["message"] = "Cellular event processed";
        event_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        if (!body.empty()) {
            try {
                auto event_data = json::parse(body);

                std::string event_type = event_data.value("event_type", "unknown");
                std::string element_id = event_data.value("element_id", "unknown");
                std::string page = event_data.value("page", "unknown");

                std::cout << "[CELLULAR-EVENTS] Event Type: " << event_type << std::endl;
                std::cout << "[CELLULAR-EVENTS] Element ID: " << element_id << std::endl;
                std::cout << "[CELLULAR-EVENTS] Page: " << page << std::endl;

                if (event_data.contains("data")) {
                    auto inner_data = event_data["data"];
                    if (inner_data.contains("action")) {
                        std::string action = inner_data["action"];
                        std::cout << "[CELLULAR-EVENTS] Action: " << action << std::endl;

                        // Process different actions and persist changes via CellularDataManager
                        if (action == "connect") {
                            json connectionUpdate = {{"is_connected", false}, {"connection_status", "connecting"}};
                            CellularDataManager::updateConnectionDetails(connectionUpdate);
                            event_response["action_result"] = "connection_initiated";
                            event_response["status_update"] = {
                                {"connection_status", "connecting"},
                                {"estimated_time", 30}
                            };
                        } else if (action == "disconnect") {
                            json connectionUpdate = {{"is_connected", false}, {"connection_status", "disconnecting"}};
                            CellularDataManager::updateConnectionDetails(connectionUpdate);
                            event_response["action_result"] = "disconnection_initiated";
                            event_response["status_update"] = {
                                {"connection_status", "disconnecting"},
                                {"estimated_time", 5}
                            };
                        } else if (action == "scan_networks") {
                            // Update scan status
                            json scanUpdate = {{"scan_in_progress", true}};
                            CellularDataManager::updateScanStatus(scanUpdate);

                            event_response["action_result"] = "scan_initiated";
                            event_response["status_update"] = {
                                {"scan_status", "in_progress"},
                                {"estimated_time", 30}
                            };
                        } else if (action == "toggle_auto_connect") {
                            bool enabled = inner_data.value("enabled", false);
                            json configUpdate = {{"auto_connect", enabled}};
                            CellularDataManager::updateCellularConfiguration(configUpdate);

                            event_response["action_result"] = "auto_connect_updated";
                            event_response["status_update"] = {
                                {"auto_connect", enabled},
                                {"setting_saved", true}
                            };
                        } else if (action == "toggle_data_roaming") {
                            bool enabled = inner_data.value("enabled", false);
                            json configUpdate = {{"data_roaming", enabled}};
                            CellularDataManager::updateCellularConfiguration(configUpdate);

                            event_response["action_result"] = "data_roaming_updated";
                            event_response["status_update"] = {
                                {"data_roaming", enabled},
                                {"setting_saved", true}
                            };
                        } else if (action == "toggle_data_limit") {
                            bool enabled = inner_data.value("enabled", false);
                            json configUpdate = {{"data_limit_enabled", enabled}};
                            CellularDataManager::updateCellularConfiguration(configUpdate);

                            event_response["action_result"] = "data_limit_updated";
                            event_response["status_update"] = {
                                {"data_limit_enabled", enabled},
                                {"setting_saved", true}
                            };
                        } else if (action == "change_network_type") {
                            std::string network_type = inner_data.value("network_type", "auto");
                            json configUpdate = {{"preferred_network_type", network_type}};
                            CellularDataManager::updateCellularConfiguration(configUpdate);

                            event_response["action_result"] = "network_type_updated";
                            event_response["status_update"] = {
                                {"preferred_network_type", network_type},
                                {"setting_saved", true}
                            };
                        } else if (action == "update_configuration") {
                            if (inner_data.contains("config")) {
                                auto config = inner_data["config"];
                                CellularDataManager::updateCellularConfiguration(config);
                                event_response["action_result"] = "configuration_updated";
                            }
                            event_response["status_update"] = {
                                {"config_saved", true},
                                {"restart_required", false}
                            };
                        } else {
                            event_response["action_result"] = "action_processed";
                            event_response["status_update"] = {
                                {"processed", true}
                            };
                        }

                        // Add common event metadata
                        event_response["metadata"] = {
                            {"component", "cellular-config"},
                            {"action_type", action},
                            {"processing_time_ms", 15},
                            {"success_rate", 100.0},
                            {"data_persisted", true}
                        };
                    }
                }

                event_response["event_processed"] = true;
                event_response["event_id"] = event_type + "_" + element_id + "_" + std::to_string(static_cast<uint64_t>(event_response["timestamp"]));

                std::cout << "[CELLULAR-EVENTS] Event processing completed successfully with data persistence" << std::endl;

            } catch (const std::exception& e) {
                std::cout << "[CELLULAR-EVENTS] Error processing event: " << e.what() << std::endl;
                event_response["processed"] = false;
                event_response["error"] = e.what();
                event_response["success"] = false;
            }
        }

        return event_response.dump();
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for cellular events."}};
    return error.dump();
}


// ================================
// SIM MANAGEMENT ENDPOINTS
// ================================

std::string WirelessRouter::handleSIMUnlock(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[SIM-UNLOCK] Processing SIM unlock request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[SIM-UNLOCK] POST request for SIM PIN unlock" << std::endl;

        if (!body.empty()) {
            try {
                auto unlock_data = json::parse(body);
                std::string action = unlock_data.value("action", "unknown");
                std::string pin = unlock_data.value("pin", "");

                std::cout << "[SIM-UNLOCK] Action: " << action << std::endl;
                std::cout << "[SIM-UNLOCK] PIN: [PROTECTED]" << std::endl;

                // Validate PIN
                if (pin.empty() || pin.length() < 4 || pin.length() > 8) {
                    json error_response;
                    error_response["success"] = false;
                    error_response["message"] = "Invalid PIN format";
                    error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    return error_response.dump();
                }

                // Update SIM status using CellularDataManager
                json simUpdate = {{"pin_status", "Unlocked"}, {"sim_status", "Ready"}};
                CellularDataManager::updateSimInformation(simUpdate);

                json unlock_response;
                unlock_response["success"] = true;
                unlock_response["message"] = "SIM PIN unlock successful";
                unlock_response["pin_status"] = "Unlocked";
                unlock_response["sim_status"] = "Ready";
                unlock_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();

                std::cout << "[SIM-UNLOCK] SIM unlock processed successfully and status updated" << std::endl;
                return unlock_response.dump();

            } catch (const std::exception& e) {
                std::cout << "[SIM-UNLOCK] Error processing unlock request: " << e.what() << std::endl;
                json error_response;
                error_response["success"] = false;
                error_response["message"] = "Failed to process SIM unlock";
                error_response["error"] = e.what();
                error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                return error_response.dump();
            }
        } else {
            json error = {{"success", false}, {"message", "Request body required for SIM unlock"}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for SIM unlock."}};
    return error.dump();
}

std::string WirelessRouter::handleCellularResetData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-RESET-DATA] Processing data reset request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[CELLULAR-RESET-DATA] POST request for data usage reset" << std::endl;

        try {
            // Reset data usage using CellularDataManager
            json connectionUpdate = {
                {"data_usage_mb", 0},
                {"connection_time_seconds", 0}
            };

            bool saveResult = CellularDataManager::updateConnectionDetails(connectionUpdate);

            json reset_response;
            if (saveResult) {
                reset_response["success"] = true;
                reset_response["message"] = "Data usage reset successfully";
                reset_response["data_usage_mb"] = 0;
                reset_response["connection_time_seconds"] = 0;
            } else {
                reset_response["success"] = false;
                reset_response["message"] = "Failed to save reset data";
            }

            reset_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            std::cout << "[CELLULAR-RESET-DATA] Data reset completed with result: " << (saveResult ? "Success" : "Failed") << std::endl;
            return reset_response.dump();

        } catch (const std::exception& e) {
            std::cout << "[CELLULAR-RESET-DATA] Error processing reset request: " << e.what() << std::endl;
            json error_response;
            error_response["success"] = false;
            error_response["message"] = "Failed to reset data usage";
            error_response["error"] = e.what();
            error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return error_response.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for data reset."}};
    return error.dump();
}

std::string WirelessRouter::handleCellularResetConfig(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[CELLULAR-RESET-CONFIG] Processing config reset request, method: " << method << std::endl;

    if (method == "POST") {
        std::cout << "[CELLULAR-RESET-CONFIG] POST request for configuration reset" << std::endl;

        try {
            // Reset configuration to defaults using CellularDataManager
            json defaultConfig = {
                {"apn", ""},
                {"username", ""},
                {"password", ""},
                {"preferred_network_type", "auto"},
                {"auto_connect", false},
                {"data_roaming", false},
                {"data_limit_mb", 0},
                {"data_limit_enabled", false},
                {"auth_type", "none"},
                {"network_selection", "automatic"}
            };

            bool updateResult = CellularDataManager::updateCellularConfiguration(defaultConfig);

            json reset_response;
            if (updateResult) {
                reset_response["success"] = true;
                reset_response["message"] = "Configuration reset to defaults successfully";
                reset_response["default_configuration"] = defaultConfig;
            } else {
                reset_response["success"] = false;
                reset_response["message"] = "Failed to reset configuration";
            }

            reset_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            std::cout << "[CELLULAR-RESET-CONFIG] Configuration reset completed with result: " << (updateResult ? "Success" : "Failed") << std::endl;
            return reset_response.dump();

        } catch (const std::exception& e) {
            std::cout << "[CELLULAR-RESET-CONFIG] Error processing reset request: " << e.what() << std::endl;
            json error_response;
            error_response["success"] = false;
            error_response["message"] = "Failed to reset configuration";
            error_response["error"] = e.what();
            error_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            return error_response.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed. Use POST for config reset."}};
    return error.dump();
}

// ================================
// WIRELESS DATA MANAGEMENT METHODS
// ================================

json WirelessRouter::loadSavedNetworks() {
    std::ifstream file("data/wireless/SavedNetworks.json");
    if (!file.is_open()) {
        std::cout << "[WIRELESS-DATA] SavedNetworks.json not found, creating default" << std::endl;
        json defaultData = {
            {"saved_networks", json::array()},
            {"last_updated", getCurrentTimestamp()}
        };
        saveSavedNetworks(defaultData);
        return defaultData;
    }
    
    json data;
    file >> data;
    file.close();
    return data;
}

bool WirelessRouter::saveSavedNetworks(const json& networks) {
    try {
        std::ofstream file("data/wireless/SavedNetworks.json");
        if (!file.is_open()) {
            std::cout << "[WIRELESS-DATA] Failed to open SavedNetworks.json for writing" << std::endl;
            return false;
        }
        file << networks.dump(4);
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cout << "[WIRELESS-DATA] Error saving SavedNetworks.json: " << e.what() << std::endl;
        return false;
    }
}

json WirelessRouter::loadAvailableNetworks() {
    std::ifstream file("data/wireless/AvailableNetworks.json");
    if (!file.is_open()) {
        json defaultData = {
            {"available_networks", json::array()},
            {"scan_timestamp", getCurrentTimestamp()},
            {"scan_duration_seconds", 0},
            {"networks_found", 0}
        };
        saveAvailableNetworks(defaultData);
        return defaultData;
    }
    
    json data;
    file >> data;
    file.close();
    return data;
}

bool WirelessRouter::saveAvailableNetworks(const json& networks) {
    try {
        std::ofstream file("data/wireless/AvailableNetworks.json");
        if (!file.is_open()) {
            return false;
        }
        file << networks.dump(4);
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cout << "[WIRELESS-DATA] Error saving AvailableNetworks.json: " << e.what() << std::endl;
        return false;
    }
}

json WirelessRouter::loadManualConnections() {
    std::ifstream file("data/wireless/ManualConnect.json");
    if (!file.is_open()) {
        json defaultData = {
            {"manual_connections", json::array()},
            {"last_updated", getCurrentTimestamp()}
        };
        saveManualConnection(defaultData);
        return defaultData;
    }
    
    json data;
    file >> data;
    file.close();
    return data;
}

bool WirelessRouter::saveManualConnection(const json& connection) {
    try {
        std::ofstream file("data/wireless/ManualConnect.json");
        if (!file.is_open()) {
            return false;
        }
        file << connection.dump(4);
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cout << "[WIRELESS-DATA] Error saving ManualConnect.json: " << e.what() << std::endl;
        return false;
    }
}

json WirelessRouter::loadAccessPointConfig() {
    std::ifstream file("data/wireless/AccessPointConfiguration.json");
    if (!file.is_open()) {
        json defaultData = {
            {"access_point_config", {
                {"ssid", "UR-WebIF-AP"},
                {"password", ""},
                {"security_type", "WPA3"},
                {"channel", 6},
                {"band_selection", "dual"},
                {"max_clients", 10},
                {"dhcp_enabled", true},
                {"dhcp_start_ip", "192.168.50.10"},
                {"dhcp_end_ip", "192.168.50.254"},
                {"ip_mode", "nat"},
                {"ap_ip_address", "192.168.50.1"},
                {"subnet_mask", "255.255.255.0"}
            }},
            {"last_updated", getCurrentTimestamp()}
        };
        saveAccessPointConfig(defaultData);
        return defaultData;
    }
    
    json data;
    file >> data;
    file.close();
    return data;
}

bool WirelessRouter::saveAccessPointConfig(const json& config) {
    try {
        std::ofstream file("data/wireless/AccessPointConfiguration.json");
        if (!file.is_open()) {
            return false;
        }
        file << config.dump(4);
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cout << "[WIRELESS-DATA] Error saving AccessPointConfiguration.json: " << e.what() << std::endl;
        return false;
    }
}

json WirelessRouter::loadAccessPointStatus() {
    std::ifstream file("data/wireless/AccessPointStatus.json");
    if (!file.is_open()) {
        json defaultData = {
            {"access_point_status", {
                {"is_active", false},
                {"ssid", ""},
                {"clients_connected", 0},
                {"channel", 0},
                {"band", ""},
                {"uptime_seconds", 0},
                {"data_transmitted_mb", 0},
                {"data_received_mb", 0},
                {"connected_clients", json::array()}
            }},
            {"last_updated", getCurrentTimestamp()}
        };
        saveAccessPointStatus(defaultData);
        return defaultData;
    }
    
    json data;
    file >> data;
    file.close();
    return data;
}

bool WirelessRouter::saveAccessPointStatus(const json& status) {
    try {
        std::ofstream file("data/wireless/AccessPointStatus.json");
        if (!file.is_open()) {
            return false;
        }
        file << status.dump(4);
        file.close();
        return true;
    } catch (const std::exception& e) {
        std::cout << "[WIRELESS-DATA] Error saving AccessPointStatus.json: " << e.what() << std::endl;
        return false;
    }
}

std::string WirelessRouter::generateNetworkId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "network_" + std::to_string(timestamp);
}

std::string WirelessRouter::generateConnectionId() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return "manual_" + std::to_string(timestamp);
}

std::string WirelessRouter::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return ss.str();
}

// ================================
// NEW WIRELESS DATA ENDPOINTS
// ================================

std::string WirelessRouter::handleSavedNetworksData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-SAVED-DATA] Processing saved networks data request, method: " << method << std::endl;

    if (method == "GET") {
        try {
            json data = loadSavedNetworks();
            json response = {
                {"success", true},
                {"data", data},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            return response.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to load saved networks"}, {"error", e.what()}};
            return error.dump();
        }
    } else if (method == "POST") {
        try {
            json requestData = json::parse(body);
            json currentData = loadSavedNetworks();
            
            if (requestData.contains("action")) {
                std::string action = requestData["action"];
                
                if (action == "add_network") {
                    // Add new saved network
                    json newNetwork = {
                        {"network_id", generateNetworkId()},
                        {"ssid", requestData.value("ssid", "")},
                        {"security_type", requestData.value("security_type", "WPA2")},
                        {"password", requestData.value("password", "")},
                        {"auto_connect", requestData.value("auto_connect", false)},
                        {"last_connected", getCurrentTimestamp()},
                        {"connection_priority", currentData["saved_networks"].size() + 1},
                        {"status", "Available"},
                        {"frequency", requestData.value("frequency", "2.4GHz")},
                        {"hidden", requestData.value("hidden", false)}
                    };
                    
                    currentData["saved_networks"].push_back(newNetwork);
                    currentData["last_updated"] = getCurrentTimestamp();
                    
                    if (saveSavedNetworks(currentData)) {
                        json response = {
                            {"success", true},
                            {"message", "Network saved successfully"},
                            {"network_id", newNetwork["network_id"]},
                            {"data", currentData}
                        };
                        return response.dump();
                    }
                }
            }
            
            json error = {{"success", false}, {"message", "Invalid action or failed to save"}};
            return error.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to process request"}, {"error", e.what()}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed"}};
    return error.dump();
}

std::string WirelessRouter::handleAvailableNetworksData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-AVAILABLE-DATA] Processing available networks data request, method: " << method << std::endl;

    if (method == "GET") {
        try {
            json data = loadAvailableNetworks();
            json response = {
                {"success", true},
                {"data", data},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            return response.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to load available networks"}, {"error", e.what()}};
            return error.dump();
        }
    } else if (method == "POST") {
        try {
            json requestData = json::parse(body);
            
            if (requestData.contains("scan_results")) {
                json newData = {
                    {"available_networks", requestData["scan_results"]},
                    {"scan_timestamp", getCurrentTimestamp()},
                    {"scan_duration_seconds", requestData.value("scan_duration", 5)},
                    {"networks_found", requestData["scan_results"].size()}
                };
                
                if (saveAvailableNetworks(newData)) {
                    json response = {
                        {"success", true},
                        {"message", "Scan results saved successfully"},
                        {"data", newData}
                    };
                    return response.dump();
                }
            }
            
            json error = {{"success", false}, {"message", "Invalid scan results or failed to save"}};
            return error.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to process request"}, {"error", e.what()}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed"}};
    return error.dump();
}

std::string WirelessRouter::handleManualConnectData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-MANUAL-DATA] Processing manual connect data request, method: " << method << std::endl;

    if (method == "GET") {
        try {
            json data = loadManualConnections();
            json response = {
                {"success", true},
                {"data", data},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            return response.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to load manual connections"}, {"error", e.what()}};
            return error.dump();
        }
    } else if (method == "POST") {
        try {
            json requestData = json::parse(body);
            json currentData = loadManualConnections();
            
            json newConnection = {
                {"connection_id", generateConnectionId()},
                {"ssid", requestData.value("ssid", "")},
                {"security_type", requestData.value("security_type", "WPA2")},
                {"password", requestData.value("password", "")},
                {"ip_mode", requestData.value("ip_mode", "dhcp")},
                {"static_ip", requestData.value("static_ip", "")},
                {"subnet_mask", requestData.value("subnet_mask", "")},
                {"gateway", requestData.value("gateway", "")},
                {"dns_server", requestData.value("dns_server", "")},
                {"hidden_network", requestData.value("hidden_network", false)},
                {"connection_timestamp", getCurrentTimestamp()},
                {"connection_status", requestData.value("connection_status", "pending")},
                {"error_message", requestData.value("error_message", "")}
            };
            
            currentData["manual_connections"].push_back(newConnection);
            currentData["last_updated"] = getCurrentTimestamp();
            
            if (saveManualConnection(currentData)) {
                json response = {
                    {"success", true},
                    {"message", "Manual connection saved successfully"},
                    {"connection_id", newConnection["connection_id"]},
                    {"data", currentData}
                };
                return response.dump();
            }
            
            json error = {{"success", false}, {"message", "Failed to save manual connection"}};
            return error.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to process request"}, {"error", e.what()}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed"}};
    return error.dump();
}

std::string WirelessRouter::handleAccessPointConfigData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-AP-CONFIG-DATA] Processing AP config data request, method: " << method << std::endl;

    if (method == "GET") {
        try {
            json data = loadAccessPointConfig();
            json response = {
                {"success", true},
                {"data", data},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            return response.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to load AP config"}, {"error", e.what()}};
            return error.dump();
        }
    } else if (method == "POST") {
        try {
            json requestData = json::parse(body);
            json currentData = loadAccessPointConfig();
            
            // Update config with provided data
            for (auto& [key, value] : requestData.items()) {
                if (key != "action") {
                    currentData["access_point_config"][key] = value;
                }
            }
            currentData["last_updated"] = getCurrentTimestamp();
            
            if (saveAccessPointConfig(currentData)) {
                json response = {
                    {"success", true},
                    {"message", "AP configuration saved successfully"},
                    {"data", currentData}
                };
                return response.dump();
            }
            
            json error = {{"success", false}, {"message", "Failed to save AP configuration"}};
            return error.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to process request"}, {"error", e.what()}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed"}};
    return error.dump();
}

std::string WirelessRouter::handleAccessPointStatusData(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
    std::cout << "[WIRELESS-AP-STATUS-DATA] Processing AP status data request, method: " << method << std::endl;

    if (method == "GET") {
        try {
            json data = loadAccessPointStatus();
            json response = {
                {"success", true},
                {"data", data},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            return response.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to load AP status"}, {"error", e.what()}};
            return error.dump();
        }
    } else if (method == "POST") {
        try {
            json requestData = json::parse(body);
            json currentData = loadAccessPointStatus();
            
            // Update status with provided data
            for (auto& [key, value] : requestData.items()) {
                if (key != "action") {
                    currentData["access_point_status"][key] = value;
                }
            }
            currentData["last_updated"] = getCurrentTimestamp();
            
            if (saveAccessPointStatus(currentData)) {
                json response = {
                    {"success", true},
                    {"message", "AP status updated successfully"},
                    {"data", currentData}
                };
                return response.dump();
            }
            
            json error = {{"success", false}, {"message", "Failed to save AP status"}};
            return error.dump();
        } catch (const std::exception& e) {
            json error = {{"success", false}, {"message", "Failed to process request"}, {"error", e.what()}};
            return error.dump();
        }
    }

    json error = {{"success", false}, {"message", "Method not allowed"}};
    return error.dump();
}