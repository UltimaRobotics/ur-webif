
#include "iperf3_server_parser.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>
#include <random>
#include <cstdlib>
#include <unistd.h>

Iperf3ServerParser::Iperf3ServerParser() {
    serversFilePath = "./mecanisms/iperf3-server-parser/servers/ultima-rabotics-servers.json";
    loadDefaultServers();
}

Iperf3ServerParser::~Iperf3ServerParser() {
    // Destructor
}

bool Iperf3ServerParser::loadServersFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // File doesn't exist, create with default servers
        loadDefaultServers();
        return saveServersToFile(filePath);
    }
    
    try {
        json serverData;
        file >> serverData;
        
        servers.clear();
        
        if (serverData.contains("servers") && serverData["servers"].is_array()) {
            for (const auto& serverJson : serverData["servers"]) {
                Iperf3Server server = jsonToServer(serverJson);
                if (validateServer(server)) {
                    servers.push_back(server);
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        // If parsing fails, load default servers
        loadDefaultServers();
        return false;
    }
}

bool Iperf3ServerParser::saveServersToFile(const std::string& filePath) {
    try {
        json output;
        output["servers"] = json::array();
        output["last_updated"] = getCurrentTimestamp();
        output["version"] = "1.0";
        
        for (const auto& server : servers) {
            output["servers"].push_back(serverToJson(server));
        }
        
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return false;
        }
        
        file << output.dump(2);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<Iperf3Server> Iperf3ServerParser::getAllServers() const {
    return servers;
}

std::vector<Iperf3Server> Iperf3ServerParser::getActiveServers() const {
    std::vector<Iperf3Server> activeServers;
    for (const auto& server : servers) {
        if (server.status == "online" || server.status == "degraded") {
            activeServers.push_back(server);
        }
    }
    return activeServers;
}

std::vector<Iperf3Server> Iperf3ServerParser::getCustomServers() const {
    std::vector<Iperf3Server> customServers;
    for (const auto& server : servers) {
        if (server.is_custom) {
            customServers.push_back(server);
        }
    }
    return customServers;
}

bool Iperf3ServerParser::addCustomServer(const Iperf3Server& server) {
    if (!validateServer(server)) {
        return false;
    }
    
    // Check if server with same hostname:port already exists
    for (const auto& existingServer : servers) {
        if (existingServer.hostname == server.hostname && existingServer.port == server.port) {
            return false; // Duplicate server
        }
    }
    
    Iperf3Server newServer = server;
    newServer.id = generateServerId(server.hostname);
    newServer.is_custom = true;
    newServer.created_date = getCurrentTimestamp();
    newServer.last_tested = "";
    newServer.status = "untested";
    newServer.load_percent = 0.0;
    newServer.uptime_percent = 0.0;
    
    servers.push_back(newServer);
    saveServersToFile(serversFilePath);
    
    return true;
}

bool Iperf3ServerParser::removeCustomServer(const std::string& serverId) {
    for (auto it = servers.begin(); it != servers.end(); ++it) {
        if (it->id == serverId && it->is_custom) {
            servers.erase(it);
            saveServersToFile(serversFilePath);
            return true;
        }
    }
    return false;
}

bool Iperf3ServerParser::updateServer(const std::string& serverId, const Iperf3Server& updatedServer) {
    for (auto& server : servers) {
        if (server.id == serverId) {
            // Preserve certain fields
            std::string originalId = server.id;
            bool originalIsCustom = server.is_custom;
            std::string originalCreatedDate = server.created_date;
            
            server = updatedServer;
            server.id = originalId;
            server.is_custom = originalIsCustom;
            server.created_date = originalCreatedDate;
            
            saveServersToFile(serversFilePath);
            return true;
        }
    }
    return false;
}

Iperf3Server* Iperf3ServerParser::getServerById(const std::string& serverId) {
    for (auto& server : servers) {
        if (server.id == serverId) {
            return &server;
        }
    }
    return nullptr;
}

const Iperf3Server* Iperf3ServerParser::getServerById(const std::string& serverId) const {
    for (const auto& server : servers) {
        if (server.id == serverId) {
            return &server;
        }
    }
    return nullptr;
}

bool Iperf3ServerParser::testServerConnection(const std::string& serverId) {
    Iperf3Server* server = getServerById(serverId);
    if (!server) {
        return false;
    }
    
    // Test connection using netcat
    std::ostringstream cmd;
    cmd << "timeout 10 nc -z " << server->hostname << " " << server->port << " 2>&1";
    
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        updateServerStatus(serverId, "error");
        return false;
    }
    
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    bool connected = (status == 0);
    
    if (connected) {
        updateServerStatus(serverId, "online", calculateServerLoad(serverId));
    } else {
        updateServerStatus(serverId, "offline", 0.0);
    }
    
    server->last_tested = getCurrentTimestamp();
    saveServersToFile(serversFilePath);
    
    return connected;
}

void Iperf3ServerParser::updateServerStatus(const std::string& serverId, const std::string& status, double load) {
    Iperf3Server* server = getServerById(serverId);
    if (server) {
        server->status = status;
        if (load >= 0) {
            server->load_percent = load;
        }
        server->uptime_percent = calculateUptime(serverId);
    }
}

json Iperf3ServerParser::serversToJson() const {
    json result = json::array();
    for (const auto& server : servers) {
        result.push_back(serverToJson(server));
    }
    return result;
}

json Iperf3ServerParser::serverToJson(const Iperf3Server& server) const {
    json serverJson;
    serverJson["id"] = server.id;
    serverJson["name"] = server.name;
    serverJson["hostname"] = server.hostname;
    serverJson["ip_address"] = server.ip_address;
    serverJson["port"] = server.port;
    serverJson["location"] = server.location;
    serverJson["region"] = server.region;
    serverJson["country"] = server.country;
    serverJson["status"] = server.status;
    serverJson["load_percent"] = server.load_percent;
    serverJson["uptime_percent"] = server.uptime_percent;
    serverJson["is_custom"] = server.is_custom;
    serverJson["created_date"] = server.created_date;
    serverJson["last_tested"] = server.last_tested;
    serverJson["metadata"] = server.metadata;
    
    return serverJson;
}

Iperf3Server Iperf3ServerParser::jsonToServer(const json& serverJson) const {
    Iperf3Server server;
    
    server.id = serverJson.value("id", "");
    server.name = serverJson.value("name", "");
    server.hostname = serverJson.value("hostname", "");
    server.ip_address = serverJson.value("ip_address", "");
    server.port = serverJson.value("port", 5201);
    server.location = serverJson.value("location", "");
    server.region = serverJson.value("region", "");
    server.country = serverJson.value("country", "");
    server.status = serverJson.value("status", "untested");
    server.load_percent = serverJson.value("load_percent", 0.0);
    server.uptime_percent = serverJson.value("uptime_percent", 0.0);
    server.is_custom = serverJson.value("is_custom", false);
    server.created_date = serverJson.value("created_date", "");
    server.last_tested = serverJson.value("last_tested", "");
    
    if (serverJson.contains("metadata") && serverJson["metadata"].is_object()) {
        for (auto& item : serverJson["metadata"].items()) {
            server.metadata[item.key()] = item.value();
        }
    }
    
    return server;
}

bool Iperf3ServerParser::validateServer(const Iperf3Server& server) const {
    if (server.hostname.empty() || server.name.empty()) {
        return false;
    }
    
    if (!isValidHostname(server.hostname)) {
        return false;
    }
    
    if (!isValidPort(server.port)) {
        return false;
    }
    
    return true;
}

std::string Iperf3ServerParser::generateServerId(const std::string& hostname) const {
    std::string baseId = hostname;
    // Replace dots and special characters with underscores
    std::regex specialChars("[^a-zA-Z0-9]");
    baseId = std::regex_replace(baseId, specialChars, "_");
    
    // Make lowercase
    std::transform(baseId.begin(), baseId.end(), baseId.begin(), ::tolower);
    
    // Check for uniqueness
    int counter = 1;
    std::string finalId = baseId;
    
    while (getServerById(finalId) != nullptr) {
        finalId = baseId + "_" + std::to_string(counter);
        counter++;
    }
    
    return finalId;
}

void Iperf3ServerParser::loadDefaultServers() {
    servers.clear();
    
    // Default iperf3 servers
    Iperf3Server server1;
    server1.id = "iperf_bouygues_fr";
    server1.name = "Bouygues Telecom";
    server1.hostname = "iperf3.bouyguestelecom.fr";
    server1.ip_address = "89.84.4.200";
    server1.port = 5201;
    server1.location = "Paris, France";
    server1.region = "Europe";
    server1.country = "France";
    server1.status = "online";
    server1.load_percent = 25.0;
    server1.uptime_percent = 99.5;
    server1.is_custom = false;
    server1.created_date = getCurrentTimestamp();
    server1.last_tested = "";
    servers.push_back(server1);
    
    Iperf3Server server2;
    server2.id = "iperf_proof_ovh_net";
    server2.name = "OVH Network";
    server2.hostname = "proof.ovh.net";
    server2.ip_address = "213.186.33.87";
    server2.port = 5201;
    server2.location = "Roubaix, France";
    server2.region = "Europe";
    server2.country = "France";
    server2.status = "online";
    server2.load_percent = 45.0;
    server2.uptime_percent = 98.8;
    server2.is_custom = false;
    server2.created_date = getCurrentTimestamp();
    server2.last_tested = "";
    servers.push_back(server2);
    
    Iperf3Server server3;
    server3.id = "iperf_scottlinux_com";
    server3.name = "ScottLinux";
    server3.hostname = "iperf.scottlinux.com";
    server3.ip_address = "198.199.68.71";
    server3.port = 5201;
    server3.location = "New York, USA";
    server3.region = "North America";
    server3.country = "USA";
    server3.status = "online";
    server3.load_percent = 35.0;
    server3.uptime_percent = 97.2;
    server3.is_custom = false;
    server3.created_date = getCurrentTimestamp();
    server3.last_tested = "";
    servers.push_back(server3);
    
    Iperf3Server server4;
    server4.id = "iperf_ping_online_net";
    server4.name = "Online.net";
    server4.hostname = "ping.online.net";
    server4.ip_address = "195.154.173.55";
    server4.port = 5201;
    server4.location = "Paris, France";
    server4.region = "Europe";
    server4.country = "France";
    server4.status = "degraded";
    server4.load_percent = 78.0;
    server4.uptime_percent = 95.1;
    server4.is_custom = false;
    server4.created_date = getCurrentTimestamp();
    server4.last_tested = "";
    servers.push_back(server4);
}

bool Iperf3ServerParser::isValidHostname(const std::string& hostname) const {
    if (hostname.empty() || hostname.length() > 253) {
        return false;
    }
    
    std::regex hostnameRegex(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*$)");
    return std::regex_match(hostname, hostnameRegex);
}

bool Iperf3ServerParser::isValidPort(int port) const {
    return port > 0 && port <= 65535;
}

std::string Iperf3ServerParser::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto tm = *std::gmtime(&time_t);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

double Iperf3ServerParser::calculateServerLoad(const std::string& serverId) const {
    // Simulate server load calculation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(10.0, 90.0);
    return dis(gen);
}

double Iperf3ServerParser::calculateUptime(const std::string& serverId) const {
    // Simulate uptime calculation
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(95.0, 99.9);
    return dis(gen);
}
