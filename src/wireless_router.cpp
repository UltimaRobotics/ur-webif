#include "wireless_router.h"
#include <iostream>

void WirelessRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRoute) {
    std::cout << "WirelessRouter: Registering wireless routes..." << std::endl;
    
    // Register wireless status endpoint
    addRoute("/api/wireless/status", [this](const std::string& method, const std::string& url, 
                                           const std::map<std::string, std::string>& headers,
                                           const std::string& body) -> HttpResponse {
        return this->handleStatus(method, url, headers, body);
    });
    
    // Register wireless scan endpoint
    addRoute("/api/wireless/scan", [this](const std::string& method, const std::string& url, 
                                         const std::map<std::string, std::string>& headers,
                                         const std::string& body) -> HttpResponse {
        return this->handleScan(method, url, headers, body);
    });
    
    // Register wireless events endpoint
    addRoute("/api/wireless/events", [this](const std::string& method, const std::string& url, 
                                           const std::map<std::string, std::string>& headers,
                                           const std::string& body) -> HttpResponse {
        return this->handleEvents(method, url, headers, body);
    });
    
    std::cout << "WirelessRouter: Wireless routes registered successfully" << std::endl;
}

HttpResponse WirelessRouter::handleStatus(const std::string& method, const std::string& url,
                                        const std::map<std::string, std::string>& headers,
                                        const std::string& body) {
    HttpResponse response;
    response.status = 200;
    response.content_type = "application/json";
    response.body = R"({"status": "enabled", "mode": "client", "connected": false})";
    return response;
}

HttpResponse WirelessRouter::handleScan(const std::string& method, const std::string& url,
                                      const std::map<std::string, std::string>& headers,
                                      const std::string& body) {
    HttpResponse response;
    response.status = 200;
    response.content_type = "application/json";
    response.body = R"({"networks": []})";
    return response;
}

HttpResponse WirelessRouter::handleEvents(const std::string& method, const std::string& url,
                                        const std::map<std::string, std::string>& headers,
                                        const std::string& body) {
    // Log the wireless event for testing
    std::cout << "[WIRELESS-EVENT] Received: " << method << " " << url << std::endl;
    std::cout << "[WIRELESS-EVENT] Body: " << body << std::endl;
    
    HttpResponse response;
    response.status = 200;
    response.content_type = "application/json";
    response.body = R"({"success": true, "message": "Event received"})";
    return response;
}