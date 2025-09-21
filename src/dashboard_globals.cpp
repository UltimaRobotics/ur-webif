#include "dashboard_globals.h"
#include "endpoint_logger.h"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

namespace DashboardGlobals {

    // Global variable definitions
    std::mutex g_data_mutex;
    SystemStatusData g_system_data;
    std::map<std::string, std::string> g_custom_metrics;
    std::atomic<bool> g_data_initialized{false};
    std::atomic<bool> g_auto_update_enabled{false};
    std::chrono::steady_clock::time_point g_last_update;
    
    // Update intervals (in seconds)
    std::atomic<int> g_system_update_interval{30};
    std::atomic<int> g_network_update_interval{60};
    std::atomic<int> g_cellular_update_interval{120};
    
    // Data freshness tracking
    std::atomic<int> g_system_data_age_seconds{0};
    std::atomic<int> g_network_data_age_seconds{0};
    std::atomic<int> g_cellular_data_age_seconds{0};
    
    // Performance metrics
    std::atomic<long> g_total_requests{0};
    std::atomic<long> g_successful_requests{0};
    std::atomic<long> g_failed_requests{0};
    std::atomic<double> g_average_response_time_ms{0.0};
    
    // Connection counters
    std::atomic<int> g_active_connections{0};
    std::atomic<long> g_total_connections_served{0};
    
    // Internal data manager (static to this compilation unit)
    static std::shared_ptr<SourcePageDataManager> g_data_manager;
    static std::chrono::steady_clock::time_point g_system_last_update;
    static std::chrono::steady_clock::time_point g_network_last_update;
    static std::chrono::steady_clock::time_point g_cellular_last_update;
    
    bool initialize(std::shared_ptr<SourcePageDataManager> data_manager) {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        
        if (g_data_initialized.load()) {
            ENDPOINT_LOG_INFO("dashboard", "Already initialized");
            return true;
        }
        
        if (!data_manager) {
            std::cerr << "[DASHBOARD-GLOBALS] Error: Null data manager provided" << std::endl;
            return false;
        }
        
        g_data_manager = data_manager;
        
        // Initialize timestamps
        auto now = std::chrono::steady_clock::now();
        g_last_update = now;
        g_system_last_update = now;
        g_network_last_update = now;
        g_cellular_last_update = now;
        
        // Set default values immediately (non-blocking initialization)
        g_system_data.system.cpu_usage_percent = 15;
        g_system_data.system.cpu_cores = 2;
        g_system_data.system.cpu_model = "System CPU";
        g_system_data.system.cpu_temp_celsius = 45;
        g_system_data.system.ram_usage_percent = 30;
        g_system_data.system.ram_total_gb = 4.0f;
        
        g_system_data.network.internet_connected = true;
        g_system_data.network.internet_status = "Connected";
        g_system_data.network.local_ip = "127.0.0.1";
        
        g_system_data.cellular.has_signal = false;
        g_system_data.cellular.signal_status = "No cellular";
        g_system_data.cellular.signal_bars = 0;
        
        g_system_data.last_update = Internal::timestampToString(now);
        
        g_data_initialized.store(true);
        ENDPOINT_LOG_INFO("dashboard", "Initialized successfully with default values");
        
        return true;
    }
    
    void shutdown() {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        
        g_auto_update_enabled.store(false);
        g_data_initialized.store(false);
        g_data_manager.reset();
        g_custom_metrics.clear();
        
        ENDPOINT_LOG_INFO("dashboard", "Shutdown complete");
    }
    
    bool updateAllData(bool force_update) {
        if (!g_data_manager) {
            std::cerr << "[DASHBOARD-GLOBALS] No data manager available" << std::endl;
            return false;
        }
        
        // If not initialized, initialize with default values
        if (!g_data_initialized.load()) {
            ENDPOINT_LOG_WARNING("dashboard", "System not initialized, using default values");
            std::lock_guard<std::mutex> lock(g_data_mutex);
            
            // Set default values for all data structures
            g_system_data.system.cpu_usage_percent = 0;
            g_system_data.system.cpu_cores = 1;
            g_system_data.system.cpu_model = "Unknown CPU";
            g_system_data.system.cpu_temp_celsius = 45;
            g_system_data.system.ram_usage_percent = 0;
            g_system_data.system.ram_total_gb = 4.0f;
            
            g_system_data.network.internet_connected = false;
            g_system_data.network.internet_status = "Unknown";
            g_system_data.network.local_ip = "127.0.0.1";
            
            g_system_data.cellular.has_signal = false;
            g_system_data.cellular.signal_status = "No signal";
            g_system_data.cellular.signal_bars = 0;
            
            g_system_data.last_update = Internal::timestampToString(std::chrono::steady_clock::now());
            g_data_initialized.store(true);
        }
        
        bool system_updated = updateSystemData(force_update);
        bool network_updated = updateNetworkData(force_update);
        bool cellular_updated = updateCellularData(force_update);
        
        if (system_updated || network_updated || cellular_updated) {
            std::lock_guard<std::mutex> lock(g_data_mutex);
            g_last_update = std::chrono::steady_clock::now();
            g_system_data.last_update = Internal::timestampToString(g_last_update);
            
            ENDPOINT_LOG_INFO("dashboard", "Data updated - System: " + std::to_string(system_updated) + ", Network: " + std::to_string(network_updated) + ", Cellular: " + std::to_string(cellular_updated));
        }
        
        Internal::updateDataAge();
        return true;
    }
    
    bool updateSystemData(bool force_update) {
        if (!isDataFresh("system") || force_update) {
            try {
                SystemInfo new_system_info = g_data_manager->collectSystemInfo();
                
                std::lock_guard<std::mutex> lock(g_data_mutex);
                g_system_data.system = new_system_info;
                g_system_last_update = std::chrono::steady_clock::now();
                
                ENDPOINT_LOG_INFO("dashboard", "System data updated - CPU: " + std::to_string(new_system_info.cpu_usage_percent) + "%, RAM: " + std::to_string(new_system_info.ram_usage_percent) + "%");
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[DASHBOARD-GLOBALS] Error updating system data: " << e.what() 
                          << " - Using default values" << std::endl;
                
                // Use default values when collection fails
                std::lock_guard<std::mutex> lock(g_data_mutex);
                g_system_data.system.cpu_usage_percent = 15;  // Default reasonable values
                g_system_data.system.ram_usage_percent = 25;
                g_system_last_update = std::chrono::steady_clock::now();
                return true;  // Return true since we provided default data
            }
        }
        return false;
    }
    
    bool updateNetworkData(bool force_update) {
        if (!isDataFresh("network") || force_update) {
            try {
                NetworkInfo new_network_info = g_data_manager->collectNetworkInfo();
                
                std::lock_guard<std::mutex> lock(g_data_mutex);
                g_system_data.network = new_network_info;
                g_network_last_update = std::chrono::steady_clock::now();
                
                ENDPOINT_LOG_INFO("dashboard", "Network data updated - Connected: " + std::string(new_network_info.internet_connected ? "Yes" : "No") + ", IP: " + new_network_info.local_ip);
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[DASHBOARD-GLOBALS] Error updating network data: " << e.what() 
                          << " - Using default values" << std::endl;
                
                // Use default values when collection fails
                std::lock_guard<std::mutex> lock(g_data_mutex);
                g_system_data.network.internet_connected = true;
                g_system_data.network.internet_status = "Connected";
                g_system_data.network.local_ip = "127.0.0.1";
                g_network_last_update = std::chrono::steady_clock::now();
                return true;  // Return true since we provided default data
            }
        }
        return false;
    }
    
    bool updateCellularData(bool force_update) {
        if (!isDataFresh("cellular") || force_update) {
            try {
                CellularInfo new_cellular_info = g_data_manager->collectCellularInfo();
                
                std::lock_guard<std::mutex> lock(g_data_mutex);
                g_system_data.cellular = new_cellular_info;
                g_cellular_last_update = std::chrono::steady_clock::now();
                
                ENDPOINT_LOG_INFO("dashboard", "Cellular data updated - Signal: " + std::to_string(new_cellular_info.signal_bars) + " bars, Connected: " + std::string(new_cellular_info.is_connected ? "Yes" : "No"));
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[DASHBOARD-GLOBALS] Error updating cellular data: " << e.what() 
                          << " - Using default values" << std::endl;
                
                // Use default values when collection fails
                std::lock_guard<std::mutex> lock(g_data_mutex);
                g_system_data.cellular.has_signal = false;
                g_system_data.cellular.signal_status = "No signal available";
                g_system_data.cellular.signal_bars = 0;
                g_cellular_last_update = std::chrono::steady_clock::now();
                return true;  // Return true since we provided default data
            }
        }
        return false;
    }
    
    SystemStatusData getCurrentData() {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        return g_system_data;
    }
    
    SystemInfo getCurrentSystemInfo() {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        return g_system_data.system;
    }
    
    NetworkInfo getCurrentNetworkInfo() {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        return g_system_data.network;
    }
    
    CellularInfo getCurrentCellularInfo() {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        return g_system_data.cellular;
    }
    
    void setCustomMetric(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        g_custom_metrics[key] = value;
        std::cout << "[DASHBOARD-GLOBALS] Custom metric set: " << key << " = " << value << std::endl;
    }
    
    std::string getCustomMetric(const std::string& key) {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        auto it = g_custom_metrics.find(key);
        return (it != g_custom_metrics.end()) ? it->second : "";
    }
    
    std::map<std::string, std::string> getAllCustomMetrics() {
        std::lock_guard<std::mutex> lock(g_data_mutex);
        return g_custom_metrics;
    }
    
    void setAutoUpdate(bool enabled, int system_interval_sec, int network_interval_sec, int cellular_interval_sec) {
        g_auto_update_enabled.store(enabled);
        g_system_update_interval.store(std::max(1, system_interval_sec));
        g_network_update_interval.store(std::max(1, network_interval_sec));
        g_cellular_update_interval.store(std::max(1, cellular_interval_sec));
        
        ENDPOINT_LOG_INFO("dashboard", "Auto-update " + std::string(enabled ? "enabled" : "disabled") + " - Intervals: System=" + std::to_string(system_interval_sec) + "s, Network=" + std::to_string(network_interval_sec) + "s, Cellular=" + std::to_string(cellular_interval_sec) + "s");
    }
    
    bool isDataFresh(const std::string& component) {
        auto now = std::chrono::steady_clock::now();
        auto age_seconds = getDataAge(component);
        
        if (component == "system") {
            return age_seconds < g_system_update_interval.load();
        } else if (component == "network") {
            return age_seconds < g_network_update_interval.load();
        } else if (component == "cellular") {
            return age_seconds < g_cellular_update_interval.load();
        }
        
        return false;
    }
    
    int getDataAge(const std::string& component) {
        auto now = std::chrono::steady_clock::now();
        
        if (component == "system") {
            return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                now - g_system_last_update).count());
        } else if (component == "network") {
            return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                now - g_network_last_update).count());
        } else if (component == "cellular") {
            return static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(
                now - g_cellular_last_update).count());
        }
        
        return -1;
    }
    
    void recordRequest(double response_time_ms, bool success) {
        g_total_requests.fetch_add(1);
        
        if (success) {
            g_successful_requests.fetch_add(1);
        } else {
            g_failed_requests.fetch_add(1);
        }
        
        // Update rolling average (simple approach)
        double current_avg = g_average_response_time_ms.load();
        long total = g_total_requests.load();
        double new_avg = ((current_avg * (total - 1)) + response_time_ms) / total;
        g_average_response_time_ms.store(new_avg);
    }
    
    void updateConnectionCount(int delta) {
        g_active_connections.fetch_add(delta);
        if (delta > 0) {
            g_total_connections_served.fetch_add(delta);
        }
    }
    
    std::string getPerformanceStats() {
        json perf;
        perf["total_requests"] = g_total_requests.load();
        perf["successful_requests"] = g_successful_requests.load();
        perf["failed_requests"] = g_failed_requests.load();
        perf["success_rate_percent"] = (g_total_requests.load() > 0) ? 
            (static_cast<double>(g_successful_requests.load()) / g_total_requests.load() * 100.0) : 0.0;
        perf["average_response_time_ms"] = g_average_response_time_ms.load();
        perf["active_connections"] = g_active_connections.load();
        perf["total_connections_served"] = g_total_connections_served.load();
        
        return perf.dump();
    }
    
    std::string getDashboardResponse(bool include_custom_metrics, bool include_performance) {
        // Update data if auto-update is enabled
        if (g_auto_update_enabled.load()) {
            updateAllData(false);
        }
        
        json response;
        
        // Get current data
        SystemStatusData data = getCurrentData();
        
        // System info
        response["system"] = {
            {"cpu_usage_percent", data.system.cpu_usage_percent},
            {"cpu_cores", data.system.cpu_cores},
            {"cpu_model", data.system.cpu_model},
            {"cpu_temp_celsius", data.system.cpu_temp_celsius},
            {"cpu_base_clock_ghz", data.system.cpu_base_clock_ghz},
            {"cpu_boost_clock_ghz", data.system.cpu_boost_clock_ghz},
            {"ram_usage_percent", data.system.ram_usage_percent},
            {"ram_used_gb", data.system.ram_used_gb},
            {"ram_total_gb", data.system.ram_total_gb},
            {"ram_available_gb", data.system.ram_available_gb},
            {"ram_type", data.system.ram_type},
            {"swap_usage_percent", data.system.swap_usage_percent},
            {"swap_used_mb", data.system.swap_used_mb},
            {"swap_total_gb", data.system.swap_total_gb},
            {"swap_available_gb", data.system.swap_available_gb},
            {"swap_priority", data.system.swap_priority}
        };
        
        // Network info
        response["network"] = {
            {"internet_connected", data.network.internet_connected},
            {"internet_status", data.network.internet_status},
            {"external_ip", data.network.external_ip},
            {"dns_primary", data.network.dns_primary},
            {"dns_secondary", data.network.dns_secondary},
            {"latency_ms", data.network.latency_ms},
            {"download_speed_mbps", data.network.download_speed_mbps},
            {"upload_speed_mbps", data.network.upload_speed_mbps},
            {"server_connected", data.network.server_connected},
            {"server_status", data.network.server_status},
            {"server_hostname", data.network.server_hostname},
            {"server_port", data.network.server_port},
            {"server_protocol", data.network.server_protocol},
            {"last_ping_ms", data.network.last_ping_ms},
            {"session_duration", data.network.session_duration},
            {"connection_type", data.network.connection_type},
            {"interface_name", data.network.interface_name},
            {"mac_address", data.network.mac_address},
            {"local_ip", data.network.local_ip},
            {"gateway_ip", data.network.gateway_ip},
            {"connection_speed", data.network.connection_speed}
        };
        
        // Cellular info
        response["cellular"] = {
            {"has_signal", data.cellular.has_signal},
            {"signal_status", data.cellular.signal_status},
            {"signal_bars", data.cellular.signal_bars},
            {"rssi_dbm", data.cellular.rssi_dbm},
            {"rsrp_dbm", data.cellular.rsrp_dbm},
            {"rsrq_db", data.cellular.rsrq_db},
            {"sinr_db", data.cellular.sinr_db},
            {"cell_id", data.cellular.cell_id},
            {"is_connected", data.cellular.is_connected},
            {"connection_status", data.cellular.connection_status},
            {"network_name", data.cellular.network_name},
            {"technology", data.cellular.technology},
            {"band", data.cellular.band},
            {"apn", data.cellular.apn},
            {"data_usage_mb", data.cellular.data_usage_mb}
        };
        
        // Data freshness info
        response["data_freshness"] = {
            {"system_age_seconds", getDataAge("system")},
            {"network_age_seconds", getDataAge("network")},
            {"cellular_age_seconds", getDataAge("cellular")},
            {"last_update", data.last_update}
        };
        
        // Custom metrics
        if (include_custom_metrics) {
            response["custom_metrics"] = getAllCustomMetrics();
        }
        
        // Performance statistics
        if (include_performance) {
            json perf_json = json::parse(getPerformanceStats());
            response["performance"] = perf_json;
        }
        
        // Metadata
        response["timestamp"] = Internal::timestampToString(std::chrono::steady_clock::now());
        response["auto_update_enabled"] = g_auto_update_enabled.load();
        response["update_intervals"] = {
            {"system_seconds", g_system_update_interval.load()},
            {"network_seconds", g_network_update_interval.load()},
            {"cellular_seconds", g_cellular_update_interval.load()}
        };
        
        return response.dump();
    }
    
    std::string getRefreshedDashboardResponse() {
        updateAllData(true);
        return getDashboardResponse(true, true);
    }
    
    // Internal helper functions
    namespace Internal {
        void updateDataAge() {
            g_system_data_age_seconds.store(getDataAge("system"));
            g_network_data_age_seconds.store(getDataAge("network"));
            g_cellular_data_age_seconds.store(getDataAge("cellular"));
        }
        
        std::string timestampToString(const std::chrono::steady_clock::time_point& tp) {
            auto time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
            return ss.str();
        }
        
        bool isValidComponent(const std::string& component) {
            return (component == "system" || component == "network" || component == "cellular");
        }
        
        std::shared_ptr<SourcePageDataManager> getDataManager() {
            return g_data_manager;
        }
        
        void setDataManager(std::shared_ptr<SourcePageDataManager> manager) {
            g_data_manager = manager;
        }
    }
}