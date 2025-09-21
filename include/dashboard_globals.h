#pragma once

#include "source-page-data.hpp"
#include <memory>
#include <mutex>
#include <atomic>
#include <map>
#include <string>
#include <chrono>

/**
 * Global Dashboard Data Management System
 * 
 * This system maintains global variables for dashboard data that can be
 * accessed across the entire backend. It provides thread-safe access
 * and automatic updates to ensure frontend always receives current data.
 */

namespace DashboardGlobals {

    // Thread-safe global data structures
    extern std::mutex g_data_mutex;
    extern SystemStatusData g_system_data;
    extern std::map<std::string, std::string> g_custom_metrics;
    extern std::atomic<bool> g_data_initialized;
    extern std::atomic<bool> g_auto_update_enabled;
    extern std::chrono::steady_clock::time_point g_last_update;
    
    // Update intervals (in seconds)
    extern std::atomic<int> g_system_update_interval;
    extern std::atomic<int> g_network_update_interval;
    extern std::atomic<int> g_cellular_update_interval;
    
    // Data freshness tracking
    extern std::atomic<int> g_system_data_age_seconds;
    extern std::atomic<int> g_network_data_age_seconds;
    extern std::atomic<int> g_cellular_data_age_seconds;
    
    // Performance metrics
    extern std::atomic<long> g_total_requests;
    extern std::atomic<long> g_successful_requests;
    extern std::atomic<long> g_failed_requests;
    extern std::atomic<double> g_average_response_time_ms;
    
    // Connection counters
    extern std::atomic<int> g_active_connections;
    extern std::atomic<long> g_total_connections_served;
    
    /**
     * Initialize the global dashboard data system
     * @param data_manager Pointer to the system data manager
     * @return true if initialization successful
     */
    bool initialize(std::shared_ptr<SourcePageDataManager> data_manager);
    
    /**
     * Shutdown the global dashboard data system
     */
    void shutdown();
    
    /**
     * Update all dashboard data from the system
     * @param force_update Force update even if data is still fresh
     * @return true if update successful
     */
    bool updateAllData(bool force_update = false);
    
    /**
     * Update specific component data
     */
    bool updateSystemData(bool force_update = false);
    bool updateNetworkData(bool force_update = false);
    bool updateCellularData(bool force_update = false);
    
    /**
     * Get current dashboard data (thread-safe)
     * @return Copy of current system data
     */
    SystemStatusData getCurrentData();
    
    /**
     * Get specific component data (thread-safe)
     */
    SystemInfo getCurrentSystemInfo();
    NetworkInfo getCurrentNetworkInfo();
    CellularInfo getCurrentCellularInfo();
    
    /**
     * Set custom metric value
     * @param key Metric name
     * @param value Metric value
     */
    void setCustomMetric(const std::string& key, const std::string& value);
    
    /**
     * Get custom metric value
     * @param key Metric name
     * @return Metric value or empty string if not found
     */
    std::string getCustomMetric(const std::string& key);
    
    /**
     * Get all custom metrics (thread-safe)
     * @return Copy of all custom metrics
     */
    std::map<std::string, std::string> getAllCustomMetrics();
    
    /**
     * Enable/disable automatic data updates
     * @param enabled Enable auto updates
     * @param system_interval_sec System data update interval in seconds
     * @param network_interval_sec Network data update interval in seconds
     * @param cellular_interval_sec Cellular data update interval in seconds
     */
    void setAutoUpdate(bool enabled, 
                      int system_interval_sec = 30,
                      int network_interval_sec = 60,
                      int cellular_interval_sec = 120);
    
    /**
     * Check if data is fresh based on component-specific intervals
     * @param component Component name ("system", "network", "cellular")
     * @return true if data is still fresh
     */
    bool isDataFresh(const std::string& component);
    
    /**
     * Get data age in seconds
     * @param component Component name ("system", "network", "cellular")
     * @return Age in seconds, -1 if invalid component
     */
    int getDataAge(const std::string& component);
    
    /**
     * Record request statistics
     * @param response_time_ms Response time in milliseconds
     * @param success Whether request was successful
     */
    void recordRequest(double response_time_ms, bool success);
    
    /**
     * Update connection counters
     * @param delta Change in active connections (+1 for new, -1 for closed)
     */
    void updateConnectionCount(int delta);
    
    /**
     * Get performance statistics as JSON
     * @return JSON object with performance metrics
     */
    std::string getPerformanceStats();
    
    /**
     * Get comprehensive dashboard response for API
     * @param include_custom_metrics Include custom metrics in response
     * @param include_performance Include performance stats in response
     * @return JSON string with all dashboard data
     */
    std::string getDashboardResponse(bool include_custom_metrics = true, 
                                   bool include_performance = true);
    
    /**
     * Force refresh of all data and return updated response
     * @return JSON string with refreshed dashboard data
     */
    std::string getRefreshedDashboardResponse();
    
    // Internal helper functions (not for external use)
    namespace Internal {
        void updateDataAge();
        std::string timestampToString(const std::chrono::steady_clock::time_point& tp);
        bool isValidComponent(const std::string& component);
        std::shared_ptr<SourcePageDataManager> getDataManager();
        void setDataManager(std::shared_ptr<SourcePageDataManager> manager);
    }
}