
#ifndef IPERF3_SERVER_PARSER_H
#define IPERF3_SERVER_PARSER_H

#include <string>
#include <vector>
#include <map>
#include "../../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

struct Iperf3Server {
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
};

class Iperf3ServerParser {
public:
    Iperf3ServerParser();
    ~Iperf3ServerParser();
    
    // Server management
    bool loadServersFromFile(const std::string& filePath);
    bool saveServersToFile(const std::string& filePath);
    std::vector<Iperf3Server> getAllServers() const;
    std::vector<Iperf3Server> getActiveServers() const;
    std::vector<Iperf3Server> getCustomServers() const;
    
    // Server operations
    bool addCustomServer(const Iperf3Server& server);
    bool removeCustomServer(const std::string& serverId);
    bool updateServer(const std::string& serverId, const Iperf3Server& updatedServer);
    Iperf3Server* getServerById(const std::string& serverId);
    const Iperf3Server* getServerById(const std::string& serverId) const;
    
    // Server testing
    bool testServerConnection(const std::string& serverId);
    void updateServerStatus(const std::string& serverId, const std::string& status, double load = -1);
    
    // JSON conversion
    json serversToJson() const;
    json serverToJson(const Iperf3Server& server) const;
    Iperf3Server jsonToServer(const json& serverJson) const;
    
    // Validation
    bool validateServer(const Iperf3Server& server) const;
    std::string generateServerId(const std::string& hostname) const;
    
private:
    std::vector<Iperf3Server> servers;
    std::string serversFilePath;
    
    // Helper methods
    void loadDefaultServers();
    bool isValidHostname(const std::string& hostname) const;
    bool isValidPort(int port) const;
    std::string getCurrentTimestamp() const;
    double calculateServerLoad(const std::string& serverId) const;
    double calculateUptime(const std::string& serverId) const;
};

#endif // IPERF3_SERVER_PARSER_H
