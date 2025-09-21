
#ifndef IPERF3_SERVERS_ENGINE_HPP
#define IPERF3_SERVERS_ENGINE_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

class Iperf3ServersEngine {
public:
    struct ServerInfo {
        std::string id;
        std::string name;
        std::string hostname;
        std::string ip_address;
        int port;
        std::string location;
        std::string region;
        std::string country;
        std::string status;
        double load_percent;
        double uptime_percent;
        bool is_custom;
        std::string created_date;
        std::string last_tested;
        std::map<std::string, std::string> metadata;
        
        // Additional connectivity metrics
        double ping_ms;
        double response_time_ms;
        double packet_loss;
        double jitter_ms;
    };

    struct ServerTestResult {
        bool success;
        std::string status;
        double ping_ms;
        double response_time_ms;
        double packet_loss;
        double jitter_ms;
        int load_percent;
        std::string last_tested;
        std::string error_message;
    };

    Iperf3ServersEngine();
    ~Iperf3ServersEngine();

    // Server management
    bool loadServersFromFile(const std::string& filePath);
    bool saveServersToFile(const std::string& filePath);
    std::vector<ServerInfo> getAllServers() const;
    std::vector<ServerInfo> getActiveServers() const;
    std::vector<ServerInfo> getCustomServers() const;
    
    // Server operations
    bool addCustomServer(const ServerInfo& server);
    bool removeCustomServer(const std::string& serverId);
    bool updateServer(const std::string& serverId, const ServerInfo& updatedServer);
    ServerInfo* getServerById(const std::string& serverId);
    const ServerInfo* getServerById(const std::string& serverId) const;
    
    // Server testing and connectivity
    ServerTestResult testServerConnectivity(const std::string& serverId);
    bool updateServerStatus(const std::string& serverId, const std::string& status, double load = -1);
    
    // JSON conversion
    json serversToJson() const;
    json serverToJson(const ServerInfo& server) const;
    ServerInfo jsonToServer(const json& serverJson) const;
    
    // Validation and utilities
    bool validateServer(const ServerInfo& server) const;
    std::string generateServerId(const std::string& hostname) const;
    
    // Bulk operations
    void testAllServersAsync();
    json getServerStatusSummary() const;
    
    // Configuration
    void setServersFilePath(const std::string& filePath);
    std::string getServersFilePath() const;
    
    // Error handling and timeouts
    static const int DEFAULT_TIMEOUT_MS = 5000;

private:
    mutable std::mutex servers_mutex_;
    std::vector<ServerInfo> servers_;
    std::string servers_file_path_;
    
    // Helper methods
    void loadDefaultServers();
    bool isValidHostname(const std::string& hostname) const;
    bool isValidPort(int port) const;
    std::string getCurrentTimestamp() const;
    double calculateServerLoad(const std::string& hostname, int port) const;
    double calculateUptime(const std::string& serverId) const;
    
    // Network testing utilities
    std::string performPingTest(const std::string& hostname, int count = 3) const;
    std::string performPortConnectivityTest(const std::string& hostname, int port) const;
    bool parseConnectivityResults(const std::string& pingOutput, const std::string& portOutput, ServerTestResult& result) const;
    
    // Internal state management
    void initializeDefaultConfiguration();
    bool ensureServersFileExists();
    
    // Helper method that doesn't acquire mutex (assumes already locked)
    const ServerInfo* findServerByIdNoLock(const std::string& serverId) const;
};

#endif // IPERF3_SERVERS_ENGINE_HPP
