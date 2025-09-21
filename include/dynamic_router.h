#ifndef DYNAMIC_ROUTER_H
#define DYNAMIC_ROUTER_H

#include <string>
#include <map>
#include <vector>
#include <functional>
#include <regex>
#include <memory>

struct RouteMatch {
    bool matched = false;
    std::map<std::string, std::string> path_params;
    std::map<std::string, std::string> query_params;
    std::string matched_pattern;
};

struct RoutePattern {
    std::string pattern;
    std::regex regex_pattern;
    std::vector<std::string> param_names;
    std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)> handler;
};

class DynamicRouter {
public:
    DynamicRouter();
    ~DynamicRouter() = default;

    // Register routes with dynamic patterns
    void addRoute(const std::string& pattern, 
                  std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)> handler);
    
    // Static route registration (for backward compatibility)
    void addStaticRoute(const std::string& path,
                       std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)> handler);
    
    // Process a request through dynamic routing
    std::string processRequest(const std::string& method, const std::string& path, 
                              const std::map<std::string, std::string>& query_params, 
                              const std::string& body);
    
    // Check if a path matches any registered routes
    RouteMatch findMatch(const std::string& path) const;
    
    // Get all registered route patterns
    std::vector<std::string> getRegisteredPatterns() const;

private:
    std::vector<RoutePattern> dynamic_routes_;
    std::map<std::string, std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>> static_routes_;
    
    // Convert route pattern to regex
    std::pair<std::regex, std::vector<std::string>> compilePattern(const std::string& pattern);
    
    // Extract path parameters from matched URL
    std::map<std::string, std::string> extractPathParams(const std::string& path, const RoutePattern& route) const;
    
    // Utility functions
    std::string escapeRegex(const std::string& input);
    std::vector<std::string> splitPath(const std::string& path);
};

#endif // DYNAMIC_ROUTER_H