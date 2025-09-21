#ifndef API_ENDPOINTS_H
#define API_ENDPOINTS_H

#include <string>
#include <map>
#include <memory>

// Forward declarations
class WebServer;
class HttpEventHandler;
class RouteProcessors;

/**
 * @brief API Endpoints Manager
 * 
 * This class manages all HTTP API endpoint handlers and ensures proper logging
 * according to the JSON configuration rules. All logging is handled through
 * the EndpointLogger system with appropriate category filtering.
 * 
 * Logging Categories Used:
 * - "auth": Authentication endpoints (login, logout, validate-session)
 * - "dashboard": Dashboard data and system information endpoints
 * - "utils": Utility endpoints (health, stats, echo, events)
 * - "wired": Wired network configuration endpoints
 * - "wireless": Wireless network configuration endpoints
 * - "websocket": WebSocket-related functionality (currently disabled)
 */
class ApiEndpoints {
public:
    /**
     * @brief Configure all API endpoint handlers on the given server
     * 
     * @param server Reference to the WebServer instance
     * @param event_handler Shared pointer to the HTTP event handler
     * @param route_processors Shared pointer to the route processors
     */
    static void configureEndpoints(WebServer& server, 
                                 std::shared_ptr<HttpEventHandler> event_handler,
                                 std::shared_ptr<RouteProcessors> route_processors);

private:
    /**
     * @brief Configure authentication-related endpoints
     * Logs to "auth" category - controlled by endpoint_logging.auth in config
     */
    static void configureAuthEndpoints(WebServer& server, 
                                     std::shared_ptr<HttpEventHandler> event_handler);
    
    /**
     * @brief Configure dashboard and system data endpoints  
     * Logs to "dashboard" category - controlled by endpoint_logging.dashboard in config
     */
    static void configureDashboardEndpoints(WebServer& server,
                                          std::shared_ptr<RouteProcessors> route_processors);
    
    /**
     * @brief Configure utility endpoints (health, stats, echo, events)
     * Logs to "utils" category - controlled by endpoint_logging.utils in config
     */
    static void configureUtilityEndpoints(WebServer& server,
                                        std::shared_ptr<HttpEventHandler> event_handler);
    
    /**
     * @brief Configure wired network management endpoints
     * Logs to "wired" category - controlled by endpoint_logging.wired in config
     */
    static void configureWiredNetworkEndpoints(WebServer& server);
    
    /**
     * @brief Configure wireless network management endpoints
     * Logs to "wireless" category - controlled by endpoint_logging.wireless in config
     * 
     * Wireless endpoints include:
     * - /api/wireless/events: Process wireless page user interaction events (POST)
     * - /api/wireless/status: Get current wireless configuration and status (GET)
     * - /api/wireless/radio: Control wireless radio on/off state (POST)
     * - /api/wireless/scan: Trigger wireless network scanning (POST)
     * - /api/wireless/connect: Process network connection requests (POST)
     * - /api/wireless/saved-networks: Manage saved network profiles (GET/DELETE)
     * - /api/wireless/ap/start: Start access point with configuration (POST)
     * - /api/wireless/ap/stop: Stop running access point (POST)
     */
    static void configureWirelessNetworkEndpoints(WebServer& server);
    
    /**
     * @brief Configure network utility endpoints
     * Logs to "network-utility" category for diagnostic tools and server management
     * 
     * Network utility endpoints include:
     * - /api/network-utility/servers/status: Get server status information (GET)
     * - /api/network-utility/servers/test: Test individual server connectivity (POST)
     * - /api/network-utility/bandwidth/start: Start bandwidth tests (POST)
     * - /api/network-utility/ping/start: Start ping tests (POST)
     * - /api/network-utility/traceroute/start: Start traceroute tests (POST)
     * - /api/network-utility/dns/lookup: Perform DNS lookups (POST)
     */
    static void configureNetworkUtilityEndpoints(WebServer& server);
    
    /**
     * @brief Print endpoint configuration summary
     * Logs to "utils" category for startup information
     */
    static void printEndpointSummary();
};

#endif // API_ENDPOINTS_H