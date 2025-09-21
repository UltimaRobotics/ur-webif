#pragma once

#include <string>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Wired Network Router
 * Handles all wired network configuration endpoints:
 * - /api/network/wired/status
 * - /api/network/wired/configure
 * - /api/network/wired/profiles
 * - /api/network/wired/advanced
 * - /api/network/wired/events
 */
class WiredRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;

    WiredRouter();
    ~WiredRouter() = default;

    // Register all wired network routes with the server
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler);

private:
    // Route handlers
    std::string handleWiredStatus(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleWiredConfigure(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleWiredProfiles(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleWiredAdvanced(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    std::string handleWiredEvents(const std::string& method, const std::map<std::string, std::string>& params, const std::string& body);
    
    // Profile creation handler
    std::string handleProfileCreation(const json& request_data);
};