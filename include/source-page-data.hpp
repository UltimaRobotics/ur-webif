#pragma once

#include "api_request.h"
#include <string>
#include <memory>

/**
 * Data structures for system information
 */
struct SystemInfo {
    // CPU Information
    int cpu_usage_percent = 0;
    int cpu_cores = 0;
    std::string cpu_model = "";
    int cpu_temp_celsius = 0;
    float cpu_base_clock_ghz = 0.0f;
    float cpu_boost_clock_ghz = 0.0f;
    
    // RAM Information
    int ram_usage_percent = 0;
    float ram_used_gb = 0.0f;
    float ram_total_gb = 0.0f;
    float ram_available_gb = 0.0f;
    std::string ram_type = "";
    
    // Swap Information
    int swap_usage_percent = 0;
    float swap_used_mb = 0.0f;
    float swap_total_gb = 0.0f;
    float swap_available_gb = 0.0f;
    std::string swap_priority = "";
};

struct NetworkInfo {
    // Internet Connection
    bool internet_connected = false;
    std::string internet_status = "";
    std::string external_ip = "";
    std::string dns_primary = "";
    std::string dns_secondary = "";
    int latency_ms = 0;
    int download_speed_mbps = 0;
    int upload_speed_mbps = 0;
    
    // Ultima Server Connection
    bool server_connected = false;
    std::string server_status = "";
    std::string server_hostname = "";
    int server_port = 0;
    std::string server_protocol = "";
    int last_ping_ms = 0;
    std::string session_duration = "";
    
    // Connection Type
    std::string connection_type = "";
    std::string interface_name = "";
    std::string mac_address = "";
    std::string local_ip = "";
    std::string gateway_ip = "";
    std::string connection_speed = "";
};

struct CellularInfo {
    // Signal Strength
    bool has_signal = false;
    std::string signal_status = "";
    int signal_bars = 0; // 0-4 bars
    int rssi_dbm = 0;
    int rsrp_dbm = 0;
    int rsrq_db = 0;
    int sinr_db = 0;
    std::string cell_id = "";
    
    // Connection Status
    bool is_connected = false;
    std::string connection_status = "";
    std::string network_name = "";
    std::string technology = "";
    std::string band = "";
    std::string apn = "";
    int data_usage_mb = 0;
};

struct SystemStatusData {
    SystemInfo system;
    NetworkInfo network;
    CellularInfo cellular;
    std::string last_update = "";
    std::string last_login = "";
};

/**
 * System data collector and manager
 */
class SourcePageDataManager {
public:
    SourcePageDataManager();
    ~SourcePageDataManager();
    
    // Data collection methods
    SystemInfo collectSystemInfo();
    NetworkInfo collectNetworkInfo();
    CellularInfo collectCellularInfo();
    
    // Combined data retrieval
    SystemStatusData getAllSystemData();
    
    // API endpoint processor
    ApiResponse processSystemDataRequest(const ApiRequest& request);
    
    // Refresh data periodically
    void refreshData();
    
    // Enable/disable auto-refresh
    void setAutoRefresh(bool enabled, int interval_seconds = 30);
    
private:
    SystemStatusData cached_data_;
    bool auto_refresh_enabled_ = false;
    int refresh_interval_seconds_ = 30;
    std::string last_refresh_time_;
    
    // Helper methods for data collection
    int getCpuUsagePercent();
    float getMemoryUsage();
    bool checkInternetConnectivity();
    std::string getCurrentTimestamp();
    
    // Network interface helpers
    std::string getLocalIpAddress();
    std::string getMacAddress();
    std::string getGatewayIp();
    
    // System command helpers
    std::string executeSystemCommand(const std::string& command);
    int parseIntFromCommand(const std::string& command, const std::string& fallback_pattern = "");
    float parseFloatFromCommand(const std::string& command, const std::string& fallback_pattern = "");
};