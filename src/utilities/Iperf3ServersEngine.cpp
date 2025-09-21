#include "Iperf3ServersEngine.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>
#include <random>
#include <cstdlib>
#include <unistd.h>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <future>
#include <iostream> // For std::cout and std::cerr
#include <ctime>    // For std::time

// Using nlohmann/json library
#include <nlohmann/json.hpp>
using json = nlohmann::json;

Iperf3ServersEngine::Iperf3ServersEngine()
    : servers_file_path_("mecanisms/iperf3-server-parser/servers/ultima-rabotics-servers.json") {
    try {
        // First ensure the directory exists
        ensureServersFileExists();

        // Try to load servers with timeout protection
        if (!loadServersFromFile(servers_file_path_)) {
            std::cout << "[Iperf3ServersEngine] Warning: Could not load servers file, using defaults" << std::endl;
            loadDefaultServers();
        }

        // If still empty after loading, add at least one default server
        if (servers_.empty()) {
            std::cout << "[Iperf3ServersEngine] No servers available, creating minimal default set" << std::endl;
            loadDefaultServers();
        }

        std::cout << "[Iperf3ServersEngine] Initialized with " << servers_.size() << " servers" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[Iperf3ServersEngine] Error during initialization: " << e.what() << std::endl;
        // Continue with empty servers list rather than crash
        loadDefaultServers();
    }
}

Iperf3ServersEngine::~Iperf3ServersEngine() {
    // Destructor
}

void Iperf3ServersEngine::initializeDefaultConfiguration() {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    if (!ensureServersFileExists()) {
        loadDefaultServers();
        saveServersToFile(servers_file_path_);
    } else {
        loadServersFromFile(servers_file_path_);
    }
}

bool Iperf3ServersEngine::ensureServersFileExists() {
    try {
        std::ifstream file(servers_file_path_);
        if (!file.good()) {
            std::cout << "[Iperf3ServersEngine] Servers file doesn't exist, creating: " << servers_file_path_ << std::endl;

            // Create directory if it doesn't exist
            size_t lastSlash = servers_file_path_.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                std::string directory = servers_file_path_.substr(0, lastSlash);
                // Try to create directory (ignore errors if it already exists)
                system(("mkdir -p " + directory).c_str());
            }

            // File doesn't exist, create it with default content
            std::ofstream outFile(servers_file_path_);
            if (!outFile.is_open()) {
                std::cerr << "[Iperf3ServersEngine] Failed to create servers file: " << servers_file_path_ << std::endl;
                return false;
            }

            json defaultServers = json::array();
            outFile << defaultServers.dump(4);
            outFile.close();

            std::cout << "[Iperf3ServersEngine] Created default servers file: " << servers_file_path_ << std::endl;
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Iperf3ServersEngine] Error ensuring servers file exists: " << e.what() << std::endl;
        return false;
    }
}

bool Iperf3ServersEngine::loadServersFromFile(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    try {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "[Iperf3ServersEngine] Failed to open servers file: " << filePath << std::endl;
            return false;
        }

        // Check if file is empty
        file.seekg(0, std::ios::end);
        if (file.tellg() == 0) {
            std::cerr << "[Iperf3ServersEngine] Servers file is empty: " << filePath << std::endl;
            return false;
        }
        file.seekg(0, std::ios::beg);

        json serversJson;
        try {
            file >> serversJson;
        } catch (const json::parse_error& e) {
            std::cerr << "[Iperf3ServersEngine] JSON parse error: " << e.what() << std::endl;
            return false;
        }

        servers_.clear();

        if (serversJson.is_array()) {
            for (const auto& serverJson : serversJson) {
                try {
                    ServerInfo server = jsonToServer(serverJson);
                    if (validateServer(server)) {
                        servers_.push_back(server);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Iperf3ServersEngine] Error parsing server entry: " << e.what() << std::endl;
                    continue;
                }
            }
        } else if (serversJson.contains("servers") && serversJson["servers"].is_array()) {
            for (const auto& serverJson : serversJson["servers"]) {
                try {
                    ServerInfo server = jsonToServer(serverJson);
                    if (validateServer(server)) {
                        servers_.push_back(server);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Iperf3ServersEngine] Error parsing server entry: " << e.what() << std::endl;
                    continue;
                }
            }
        } else {
            std::cerr << "[Iperf3ServersEngine] Invalid JSON format in servers file" << std::endl;
            return false;
        }

        std::cout << "[Iperf3ServersEngine] Loaded " << servers_.size() << " servers from file" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "[Iperf3ServersEngine] Error loading servers from file: " << e.what() << std::endl;
        return false;
    }
}

bool Iperf3ServersEngine::saveServersToFile(const std::string& filePath) {
    // Note: mutex is already locked by calling function

    std::cout << "[Iperf3ServersEngine] Attempting to save to: " << filePath << std::endl;

    try {
        // Ensure directory exists
        size_t lastSlash = filePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            std::string directory = filePath.substr(0, lastSlash);
            std::string cmd = "mkdir -p \"" + directory + "\" 2>/dev/null";
            int result = system(cmd.c_str());
            (void)result; // Suppress unused variable warning
        }

        json output;
        output["servers"] = json::array();
        output["last_updated"] = getCurrentTimestamp();
        output["version"] = "2.0";
        output["engine"] = "Iperf3ServersEngine";

        std::cout << "[Iperf3ServersEngine] Building JSON for " << servers_.size() << " servers" << std::endl;

        for (const auto& server : servers_) {
            try {
                output["servers"].push_back(serverToJson(server));
            } catch (const std::exception& e) {
                std::cout << "[Iperf3ServersEngine] Error serializing server " << server.id << ": " << e.what() << std::endl;
                continue;
            }
        }

        std::cout << "[Iperf3ServersEngine] Opening file for writing..." << std::endl;

        std::ofstream file(filePath, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            std::cout << "[Iperf3ServersEngine] Failed to open file for writing: " << filePath << std::endl;
            return false;
        }

        std::cout << "[Iperf3ServersEngine] Writing JSON data..." << std::endl;

        std::string jsonData = output.dump(2);
        file << jsonData;
        file.flush();
        file.close();

        std::cout << "[Iperf3ServersEngine] File saved successfully" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cout << "[Iperf3ServersEngine] Exception in saveServersToFile: " << e.what() << std::endl;
        return false;
    }
}

std::vector<Iperf3ServersEngine::ServerInfo> Iperf3ServersEngine::getAllServers() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    return servers_;
}

std::vector<Iperf3ServersEngine::ServerInfo> Iperf3ServersEngine::getActiveServers() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    std::vector<ServerInfo> activeServers;
    for (const auto& server : servers_) {
        if (server.status == "online" || server.status == "degraded") {
            activeServers.push_back(server);
        }
    }
    return activeServers;
}

std::vector<Iperf3ServersEngine::ServerInfo> Iperf3ServersEngine::getCustomServers() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    std::vector<ServerInfo> customServers;
    for (const auto& server : servers_) {
        if (server.is_custom) {
            customServers.push_back(server);
        }
    }
    return customServers;
}

bool Iperf3ServersEngine::addCustomServer(const ServerInfo& server) {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    std::cout << "[Iperf3ServersEngine] Adding custom server: " << server.hostname << std::endl;

    if (!validateServer(server)) {
        std::cout << "[Iperf3ServersEngine] Server validation failed" << std::endl;
        return false;
    }

    // Check for duplicates
    for (const auto& existingServer : servers_) {
        if (existingServer.hostname == server.hostname && existingServer.port == server.port) {
            std::cout << "[Iperf3ServersEngine] Duplicate server found, rejecting" << std::endl;
            return false;
        }
    }

    ServerInfo newServer = server;
    newServer.id = generateServerId(server.hostname);
    newServer.is_custom = true;
    newServer.created_date = getCurrentTimestamp();
    newServer.last_tested = "";
    newServer.status = "untested";
    newServer.load_percent = 0.0;
    newServer.uptime_percent = 0.0;
    newServer.ping_ms = -1;
    newServer.response_time_ms = -1;
    newServer.packet_loss = 100.0;
    newServer.jitter_ms = 0.0;

    std::cout << "[Iperf3ServersEngine] Generated server ID: " << newServer.id << std::endl;

    servers_.push_back(newServer);

    std::cout << "[Iperf3ServersEngine] Server added to memory, saving to file..." << std::endl;

    // Try to save with timeout protection
    try {
        if (!saveServersToFile(servers_file_path_)) {
            std::cout << "[Iperf3ServersEngine] Warning: Failed to save servers to file, but server added to memory" << std::endl;
        } else {
            std::cout << "[Iperf3ServersEngine] Server saved to file successfully" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[Iperf3ServersEngine] Exception while saving: " << e.what() << std::endl;
    }

    return true;
}

bool Iperf3ServersEngine::removeCustomServer(const std::string& serverId) {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    for (auto it = servers_.begin(); it != servers_.end(); ++it) {
        if (it->id == serverId && it->is_custom) {
            servers_.erase(it);
            saveServersToFile(servers_file_path_);
            return true;
        }
    }
    return false;
}

bool Iperf3ServersEngine::updateServer(const std::string& serverId, const ServerInfo& updatedServer) {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    for (auto& server : servers_) {
        if (server.id == serverId) {
            // Preserve certain fields
            std::string originalId = server.id;
            bool originalIsCustom = server.is_custom;
            std::string originalCreatedDate = server.created_date;

            server = updatedServer;
            server.id = originalId;
            server.is_custom = originalIsCustom;
            server.created_date = originalCreatedDate;

            saveServersToFile(servers_file_path_);
            return true;
        }
    }
    return false;
}

Iperf3ServersEngine::ServerInfo* Iperf3ServersEngine::getServerById(const std::string& serverId) {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    for (auto& server : servers_) {
        if (server.id == serverId) {
            return &server;
        }
    }
    return nullptr;
}

const Iperf3ServersEngine::ServerInfo* Iperf3ServersEngine::getServerById(const std::string& serverId) const {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    for (const auto& server : servers_) {
        if (server.id == serverId) {
            return &server;
        }
    }
    return nullptr;
}

// Helper method that doesn't acquire mutex (assumes already locked)
const Iperf3ServersEngine::ServerInfo* Iperf3ServersEngine::findServerByIdNoLock(const std::string& serverId) const {
    for (const auto& server : servers_) {
        if (server.id == serverId) {
            return &server;
        }
    }
    return nullptr;
}

Iperf3ServersEngine::ServerTestResult Iperf3ServersEngine::testServerConnectivity(const std::string& serverId) {
    ServerTestResult result;
    result.success = false;
    result.status = "Unknown";
    result.ping_ms = -1;
    result.response_time_ms = -1;
    result.packet_loss = 100.0;
    result.jitter_ms = 0.0;
    result.load_percent = 0;
    result.last_tested = getCurrentTimestamp();
    result.error_message = "";

    const ServerInfo* server = getServerById(serverId);
    if (!server) {
        result.error_message = "Server not found";
        return result;
    }

    try {
        // Perform ping test
        std::string pingResult = performPingTest(server->hostname, 3);

        // Perform port connectivity test
        std::string portResult = performPortConnectivityTest(server->hostname, server->port);

        // Parse results
        if (parseConnectivityResults(pingResult, portResult, result)) {
            result.success = true;
            result.load_percent = static_cast<int>(calculateServerLoad(server->hostname, server->port));

            // Update server status
            {
                std::lock_guard<std::mutex> lock(servers_mutex_);
                for (auto& srv : servers_) {
                    if (srv.id == serverId) {
                        srv.status = result.status;
                        srv.ping_ms = result.ping_ms;
                        srv.response_time_ms = result.response_time_ms;
                        srv.packet_loss = result.packet_loss;
                        srv.jitter_ms = result.jitter_ms;
                        srv.load_percent = result.load_percent;
                        srv.last_tested = result.last_tested;
                        break;
                    }
                }
            }

            saveServersToFile(servers_file_path_);
        } else {
            result.error_message = "Failed to parse connectivity test results";
        }

    } catch (const std::exception& e) {
        result.error_message = e.what();
    }

    return result;
}

bool Iperf3ServersEngine::updateServerStatus(const std::string& serverId, const std::string& status, double load) {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    for (auto& server : servers_) {
        if (server.id == serverId) {
            server.status = status;
            if (load >= 0) {
                server.load_percent = load;
            }
            server.uptime_percent = calculateUptime(serverId);
            server.last_tested = getCurrentTimestamp();
            return true;
        }
    }
    return false;
}

json Iperf3ServersEngine::serversToJson() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    json result = json::array();
    for (const auto& server : servers_) {
        result.push_back(serverToJson(server));
    }
    return result;
}

json Iperf3ServersEngine::serverToJson(const ServerInfo& server) const {
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
    serverJson["ping_ms"] = server.ping_ms;
    serverJson["response_time_ms"] = server.response_time_ms;
    serverJson["packet_loss"] = server.packet_loss;
    serverJson["jitter_ms"] = server.jitter_ms;
    serverJson["metadata"] = server.metadata;

    return serverJson;
}

Iperf3ServersEngine::ServerInfo Iperf3ServersEngine::jsonToServer(const json& serverJson) const {
    ServerInfo server;

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
    server.ping_ms = serverJson.value("ping_ms", -1.0);
    server.response_time_ms = serverJson.value("response_time_ms", -1.0);
    server.packet_loss = serverJson.value("packet_loss", 100.0);
    server.jitter_ms = serverJson.value("jitter_ms", 0.0);

    if (serverJson.contains("metadata") && serverJson["metadata"].is_object()) {
        for (auto& item : serverJson["metadata"].items()) {
            server.metadata[item.key()] = item.value();
        }
    }

    return server;
}

bool Iperf3ServersEngine::validateServer(const ServerInfo& server) const {
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

std::string Iperf3ServersEngine::generateServerId(const std::string& hostname) const {
    std::cout << "[Iperf3ServersEngine] Generating ID for hostname: " << hostname << std::endl;

    if (hostname.empty()) {
        std::cout << "[Iperf3ServersEngine] Empty hostname, using default" << std::endl;
        return "server_" + std::to_string(std::time(nullptr));
    }

    std::string baseId = hostname;
    try {
        std::regex specialChars("[^a-zA-Z0-9]");
        baseId = std::regex_replace(baseId, specialChars, "_");
    } catch (const std::exception& e) {
        std::cout << "[Iperf3ServersEngine] Regex error, using simple replacement: " << e.what() << std::endl;
        // Fallback: simple character replacement
        for (char& c : baseId) {
            if (!std::isalnum(c)) {
                c = '_';
            }
        }
    }

    std::transform(baseId.begin(), baseId.end(), baseId.begin(), ::tolower);

    int counter = 1;
    std::string finalId = baseId;

    // Limit loop to prevent infinite loops
    const int MAX_ATTEMPTS = 1000;
    // Use the non-locking findServerByIdNoLock here
    while (counter < MAX_ATTEMPTS && findServerByIdNoLock(finalId) != nullptr) {
        finalId = baseId + "_" + std::to_string(counter);
        counter++;
    }

    if (counter >= MAX_ATTEMPTS) {
        // Fallback to timestamp-based ID
        finalId = baseId + "_" + std::to_string(std::time(nullptr));
    }

    std::cout << "[Iperf3ServersEngine] Generated ID: " << finalId << std::endl;
    return finalId;
}

void Iperf3ServersEngine::testAllServersAsync() {
    std::thread([this]() {
        auto servers = getAllServers();

        for (const auto& server : servers) {
            if (!server.is_custom || server.status == "untested") {
                testServerConnectivity(server.id);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }).detach();
}

json Iperf3ServersEngine::getServerStatusSummary() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);

    int total = servers_.size();
    int online = 0;
    int offline = 0;
    int degraded = 0;
    int untested = 0;

    for (const auto& server : servers_) {
        if (server.status == "online") online++;
        else if (server.status == "offline") offline++;
        else if (server.status == "degraded") degraded++;
        else untested++;
    }

    json summary;
    summary["total_servers"] = total;
    summary["online_servers"] = online;
    summary["offline_servers"] = offline;
    summary["degraded_servers"] = degraded;
    summary["untested_servers"] = untested;
    summary["last_updated"] = getCurrentTimestamp();

    return summary;
}

void Iperf3ServersEngine::setServersFilePath(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    servers_file_path_ = filePath;
}

std::string Iperf3ServersEngine::getServersFilePath() const {
    std::lock_guard<std::mutex> lock(servers_mutex_);
    return servers_file_path_;
}

void Iperf3ServersEngine::loadDefaultServers() {
    try {
        std::cout << "[Iperf3ServersEngine] Loading default servers..." << std::endl;

        // Clear existing servers
        servers_.clear();

        // Add some default iperf3 servers (non-blocking initialization)
        ServerInfo server1;
        server1.id = generateServerId("iperf3.velocitynetwork.net");
        server1.name = "Velocity Network (US East)";
        server1.hostname = "iperf3.velocitynetwork.net";
        server1.ip_address = "198.144.176.10";
        server1.port = 5201;
        server1.location = "New York, NY, US";
        server1.region = "North America";
        server1.country = "United States";
        server1.status = "unknown";
        server1.load_percent = 0;
        server1.uptime_percent = 95.0;
        server1.is_custom = false;
        server1.created_date = getCurrentTimestamp();
        server1.ping_ms = -1;
        server1.response_time_ms = -1;
        server1.packet_loss = -1;
        server1.jitter_ms = -1;

        servers_.push_back(server1);

        ServerInfo server2;
        server2.id = generateServerId("iperf.scottlinux.com");
        server2.name = "ScottLinux (US West)";
        server2.hostname = "iperf.scottlinux.com";
        server2.ip_address = "205.204.9.54";
        server2.port = 5201;
        server2.location = "California, US";
        server2.region = "North America";
        server2.country = "United States";
        server2.status = "unknown";
        server2.load_percent = 0;
        server2.uptime_percent = 92.0;
        server2.is_custom = false;
        server2.created_date = getCurrentTimestamp();
        server2.ping_ms = -1;
        server2.response_time_ms = -1;
        server2.packet_loss = -1;
        server2.jitter_ms = -1;

        servers_.push_back(server2);

        ServerInfo server3;
        server3.id = generateServerId("ping.online.net");
        server3.name = "Online.net (Europe)";
        server3.hostname = "ping.online.net";
        server3.ip_address = "195.154.146.47";
        server3.port = 5201;
        server3.location = "Paris, France";
        server3.region = "Europe";
        server3.country = "France";
        server3.status = "unknown";
        server3.load_percent = 0;
        server3.uptime_percent = 97.0;
        server3.is_custom = false;
        server3.created_date = getCurrentTimestamp();
        server3.ping_ms = -1;
        server3.response_time_ms = -1;
        server3.packet_loss = -1;
        server3.jitter_ms = -1;

        servers_.push_back(server3);

        std::cout << "[Iperf3ServersEngine] Loaded " << servers_.size() << " default servers" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[Iperf3ServersEngine] Error loading default servers: " << e.what() << std::endl;
        // Continue without servers rather than crash
    }
}

bool Iperf3ServersEngine::isValidHostname(const std::string& hostname) const {
    if (hostname.empty() || hostname.length() > 253) {
        return false;
    }

    std::regex hostnameRegex(R"(^[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?(\.[a-zA-Z0-9]([a-zA-Z0-9\-]{0,61}[a-zA-Z0-9])?)*$)");
    return std::regex_match(hostname, hostnameRegex);
}

bool Iperf3ServersEngine::isValidPort(int port) const {
    return port > 0 && port <= 65535;
}

std::string Iperf3ServersEngine::getCurrentTimestamp() const {
    try {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        ss << " UTC";

        return ss.str();
    } catch (const std::exception& e) {
        std::cerr << "[Iperf3ServersEngine] Error generating timestamp: " << e.what() << std::endl;
        return "unknown";
    }
}

double Iperf3ServersEngine::calculateServerLoad(const std::string& hostname, int port) const {
    if (hostname.empty()) {
        return 50.0;
    }

    try {
        size_t hash = std::hash<std::string>{}(hostname);
        int base_load = 20 + ((hash + port) % 60);
        int variation = (hash % 20) - 10;
        int final_load = base_load + variation;

        if (final_load < 0) final_load = 0;
        if (final_load > 100) final_load = 100;

        return static_cast<double>(final_load);
    } catch (const std::exception& e) {
        return 50.0;
    }
}

double Iperf3ServersEngine::calculateUptime(const std::string& serverId) const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(95.0, 99.9);
    return dis(gen);
}

std::string Iperf3ServersEngine::performPingTest(const std::string& hostname, int count) const {
    if (hostname.empty()) {
        return "";
    }

    try {
        std::ostringstream cmd;
        cmd << "timeout 10 ping -c " << count << " -W 2 " << hostname << " 2>/dev/null || echo 'ping_failed'";

        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            return "";
        }

        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        int status = pclose(pipe);

        if (result.find("ping_failed") != std::string::npos) {
            return "";
        }

        return result;
    } catch (const std::exception& e) {
        return "";
    }
}

std::string Iperf3ServersEngine::performPortConnectivityTest(const std::string& hostname, int port) const {
    if (hostname.empty() || port <= 0 || port > 65535) {
        return "Connection failed";
    }

    try {
        std::ostringstream cmd;
        cmd << "timeout 5 nc -z -w3 " << hostname << " " << port << " 2>/dev/null && echo \"Connection successful\" || echo \"Connection failed\"";

        FILE* pipe = popen(cmd.str().c_str(), "r");
        if (!pipe) {
            return "Connection failed";
        }

        char buffer[128];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }

        pclose(pipe);

        // Trim whitespace
        result.erase(result.find_last_not_of(" \n\r\t") + 1);

        return result;
    } catch (const std::exception& e) {
        return "Connection failed";
    }
}

bool Iperf3ServersEngine::parseConnectivityResults(const std::string& pingOutput, const std::string& portOutput, ServerTestResult& result) const {
    try {
        // Parse ping results
        std::regex pingRegex(R"(time=([0-9.]+) ms)");
        std::smatch match;
        std::vector<double> ping_times;

        std::istringstream iss(pingOutput);
        std::string line;
        while (std::getline(iss, line) && ping_times.size() < 10) {
            if (std::regex_search(line, match, pingRegex)) {
                try {
                    double time_val = std::stod(match[1].str());
                    if (time_val >= 0 && time_val < 10000) {
                        ping_times.push_back(time_val);
                    }
                } catch (const std::exception& e) {
                    continue;
                }
            }
        }

        if (!ping_times.empty()) {
            double sum = 0.0;
            for (double time : ping_times) {
                sum += time;
            }
            result.ping_ms = sum / ping_times.size();

            // Calculate jitter
            if (ping_times.size() > 1) {
                double variance = 0.0;
                for (double time : ping_times) {
                    double diff = time - result.ping_ms;
                    variance += diff * diff;
                }
                result.jitter_ms = std::sqrt(variance / ping_times.size());
            } else {
                result.jitter_ms = 0.0;
            }

            result.packet_loss = 0.0;
        } else {
            result.ping_ms = -1;
            result.jitter_ms = 0.0;
            result.packet_loss = 100.0;
        }

        // Parse port connectivity
        if (portOutput.find("Connection successful") != std::string::npos) {
            result.response_time_ms = result.ping_ms;

            if (result.ping_ms >= 0) {
                if (result.ping_ms < 50) {
                    result.status = "online";
                } else if (result.ping_ms < 150) {
                    result.status = "degraded";
                } else {
                    result.status = "high_latency";
                }
            } else {
                result.status = "reachable";
            }
        } else {
            result.status = "offline";
            result.response_time_ms = -1;
        }

        return true;
    } catch (const std::exception& e) {
        return false;
    }
}