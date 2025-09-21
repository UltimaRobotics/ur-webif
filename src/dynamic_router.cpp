#include "../include/dynamic_router.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include "endpoint_logger.h"

DynamicRouter::DynamicRouter() {
    ENDPOINT_LOG("router", "DynamicRouter initialized");
}

void DynamicRouter::addRoute(const std::string& pattern, 
                            std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)> handler) {
    
    ENDPOINT_LOG("router", "Registering dynamic route pattern: " + pattern);
    
    RoutePattern route;
    route.pattern = pattern;
    route.handler = handler;
    
    auto compiled = compilePattern(pattern);
    route.regex_pattern = compiled.first;
    route.param_names = compiled.second;
    
    dynamic_routes_.push_back(route);
}

void DynamicRouter::addStaticRoute(const std::string& path,
                                  std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)> handler) {
    
    ENDPOINT_LOG("router", "Registering static route: " + path);
    static_routes_[path] = handler;
}

std::string DynamicRouter::processRequest(const std::string& method, const std::string& path, 
                                         const std::map<std::string, std::string>& query_params, 
                                         const std::string& body) {
    
    ENDPOINT_LOG("router", "Processing request: " + method + " " + path);
    
    // First, check static routes for exact match
    auto static_it = static_routes_.find(path);
    if (static_it != static_routes_.end()) {
        ENDPOINT_LOG("router", "Matched static route: " + path);
        return static_it->second(method, query_params, body);
    }
    
    // Then check dynamic routes
    for (const auto& route : dynamic_routes_) {
        std::smatch matches;
        if (std::regex_match(path, matches, route.regex_pattern)) {
            ENDPOINT_LOG("router", "Matched dynamic route pattern: " + route.pattern);
            
            // Extract path parameters
            std::map<std::string, std::string> combined_params = query_params;
            
            // Add path parameters
            for (size_t i = 1; i < matches.size() && i - 1 < route.param_names.size(); ++i) {
                combined_params[route.param_names[i - 1]] = matches[i].str();
            }
            
            // Log extracted parameters
            if (!combined_params.empty()) {
                std::string params_str = "Extracted parameters: ";
                for (const auto& param : combined_params) {
                    params_str += param.first + "=" + param.second + " ";
                }
                ENDPOINT_LOG("router", params_str);
            }
            
            return route.handler(method, combined_params, body);
        }
    }
    
    ENDPOINT_LOG("router", "No route matched for path: " + path);
    return "{\"error\":\"Route not found\",\"path\":\"" + path + "\",\"status\":404}";
}

RouteMatch DynamicRouter::findMatch(const std::string& path) const {
    RouteMatch result;
    
    // Check static routes first
    if (static_routes_.find(path) != static_routes_.end()) {
        result.matched = true;
        result.matched_pattern = path;
        return result;
    }
    
    // Check dynamic routes
    for (const auto& route : dynamic_routes_) {
        std::smatch matches;
        if (std::regex_match(path, matches, route.regex_pattern)) {
            result.matched = true;
            result.matched_pattern = route.pattern;
            
            // Extract path parameters
            for (size_t i = 1; i < matches.size() && i - 1 < route.param_names.size(); ++i) {
                result.path_params[route.param_names[i - 1]] = matches[i].str();
            }
            
            return result;
        }
    }
    
    return result;
}

std::vector<std::string> DynamicRouter::getRegisteredPatterns() const {
    std::vector<std::string> patterns;
    
    // Add static routes
    for (const auto& route : static_routes_) {
        patterns.push_back(route.first);
    }
    
    // Add dynamic routes
    for (const auto& route : dynamic_routes_) {
        patterns.push_back(route.pattern);
    }
    
    return patterns;
}

std::pair<std::regex, std::vector<std::string>> DynamicRouter::compilePattern(const std::string& pattern) {
    std::string regex_pattern = pattern;
    std::vector<std::string> param_names;
    
    // Replace {param} with regex capture groups
    std::regex param_regex(R"(\{([^}]+)\})");
    std::smatch matches;
    
    size_t offset = 0;
    std::string::const_iterator start = regex_pattern.cbegin();
    
    while (std::regex_search(start, regex_pattern.cend(), matches, param_regex)) {
        // Extract parameter name
        param_names.push_back(matches[1].str());
        
        // Calculate position and replace with regex
        size_t pos = offset + matches.position();
        regex_pattern.replace(pos, matches.length(), "([^/]+)");
        
        // Update offset and iterator
        offset = pos + 7; // Length of "([^/]+)"
        start = regex_pattern.cbegin() + offset;
    }
    
    // Handle wildcard patterns (*)
    std::regex wildcard_regex(R"(\*)");
    regex_pattern = std::regex_replace(regex_pattern, wildcard_regex, "(.*)");
    
    // Add anchors to ensure full path matching
    regex_pattern = "^" + regex_pattern + "$";
    
    ENDPOINT_LOG("router", "Compiled pattern '" + pattern + "' to regex '" + regex_pattern + "'");
    
    try {
        std::regex compiled_regex(regex_pattern);
        return std::make_pair(compiled_regex, param_names);
    } catch (const std::regex_error& e) {
        ENDPOINT_LOG("router", "Regex compilation error for pattern '" + pattern + "': " + e.what());
        // Return a regex that matches nothing
        return std::make_pair(std::regex("^$"), param_names);
    }
}

std::map<std::string, std::string> DynamicRouter::extractPathParams(const std::string& path, const RoutePattern& route) const {
    std::map<std::string, std::string> params;
    std::smatch matches;
    
    if (std::regex_match(path, matches, route.regex_pattern)) {
        for (size_t i = 1; i < matches.size() && i - 1 < route.param_names.size(); ++i) {
            params[route.param_names[i - 1]] = matches[i].str();
        }
    }
    
    return params;
}

std::string DynamicRouter::escapeRegex(const std::string& input) {
    std::string escaped;
    for (char c : input) {
        if (c == '.' || c == '^' || c == '$' || c == '*' || c == '+' || c == '?' || 
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || 
            c == '\\' || c == '|') {
            escaped += '\\';
        }
        escaped += c;
    }
    return escaped;
}

std::vector<std::string> DynamicRouter::splitPath(const std::string& path) {
    std::vector<std::string> segments;
    std::stringstream ss(path);
    std::string segment;
    
    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }
    
    return segments;
}