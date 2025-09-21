#ifndef ENDPOINT_LOGGER_H
#define ENDPOINT_LOGGER_H

#include <string>
#include <map>
#include <iostream>
#include <mutex>

/**
 * Centralized endpoint logging system that filters log messages based on 
 * endpoint group configuration from the JSON config file
 */
class EndpointLogger {
public:
    static EndpointLogger& getInstance();
    
    // Configure logging filters from ServerConfig
    void setEndpointFilters(const std::map<std::string, bool>& filters);
    
    // Check if logging is enabled for a specific endpoint group
    bool isLoggingEnabled(const std::string& endpoint_group) const;
    
    // Log methods for different endpoint groups
    void log(const std::string& endpoint_group, const std::string& message);
    void logInfo(const std::string& endpoint_group, const std::string& message);
    void logWarning(const std::string& endpoint_group, const std::string& message);
    void logError(const std::string& endpoint_group, const std::string& message);
    
    // Convenience macros will be provided for easy usage
    
private:
    EndpointLogger() = default;
    ~EndpointLogger() = default;
    EndpointLogger(const EndpointLogger&) = delete;
    EndpointLogger& operator=(const EndpointLogger&) = delete;
    
    std::map<std::string, bool> endpoint_filters_;
    mutable std::mutex mutex_;
};

// Convenience macros for easier usage
#define ENDPOINT_LOG(group, message) \
    EndpointLogger::getInstance().log(group, message)

#define ENDPOINT_LOG_INFO(group, message) \
    EndpointLogger::getInstance().logInfo(group, message)

#define ENDPOINT_LOG_WARNING(group, message) \
    EndpointLogger::getInstance().logWarning(group, message)

#define ENDPOINT_LOG_ERROR(group, message) \
    EndpointLogger::getInstance().logError(group, message)

// Check if logging is enabled for a group
#define IS_ENDPOINT_LOGGING_ENABLED(group) \
    EndpointLogger::getInstance().isLoggingEnabled(group)

#endif // ENDPOINT_LOGGER_H