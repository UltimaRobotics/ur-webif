#include "endpoint_logger.h"
#include <chrono>
#include <iomanip>
#include <sstream>

EndpointLogger& EndpointLogger::getInstance() {
    static EndpointLogger instance;
    return instance;
}

void EndpointLogger::setEndpointFilters(const std::map<std::string, bool>& filters) {
    std::lock_guard<std::mutex> lock(mutex_);
    endpoint_filters_ = filters;
}

bool EndpointLogger::isLoggingEnabled(const std::string& endpoint_group) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = endpoint_filters_.find(endpoint_group);
    if (it != endpoint_filters_.end()) {
        return it->second;
    }
    // Default to true if group not found in config
    return true;
}

void EndpointLogger::log(const std::string& endpoint_group, const std::string& message) {
    if (!isLoggingEnabled(endpoint_group)) {
        return;
    }
    
    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream timestamp;
    timestamp << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    timestamp << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    std::cout << "[" << timestamp.str() << "] [" << endpoint_group << "] " << message << std::endl;
}

void EndpointLogger::logInfo(const std::string& endpoint_group, const std::string& message) {
    log(endpoint_group, "[INFO] " + message);
}

void EndpointLogger::logWarning(const std::string& endpoint_group, const std::string& message) {
    log(endpoint_group, "[WARNING] " + message);
}

void EndpointLogger::logError(const std::string& endpoint_group, const std::string& message) {
    log(endpoint_group, "[ERROR] " + message);
}