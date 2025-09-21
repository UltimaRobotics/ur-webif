
#include "BandwidthUtilityEngine.hpp"
#include "endpoint_logger.h"
#include <sstream>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <iomanip>
#include <algorithm>
#include <random>

BandwidthUtilityEngine::BandwidthUtilityEngine() {
    ENDPOINT_LOG("bandwidth-engine", "BandwidthUtilityEngine initialized");
}

BandwidthUtilityEngine::~BandwidthUtilityEngine() {
    // Stop all active tests
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    for (auto& [testId, session] : activeSessions_) {
        if (session && session->isRunning.load()) {
            session->shouldStop.store(true);
            if (session->workerThread && session->workerThread->joinable()) {
                session->workerThread->join();
            }
        }
    }
    activeSessions_.clear();
    ENDPOINT_LOG("bandwidth-engine", "BandwidthUtilityEngine destroyed");
}

std::string BandwidthUtilityEngine::startBandwidthTest(const BandwidthConfig& config, ProgressCallback callback) {
    std::string validationError;
    if (!validateConfig(config, validationError)) {
        ENDPOINT_LOG("bandwidth-engine", "Invalid config: " + validationError);
        return "";
    }
    
    std::string testId = generateTestId();
    
    auto session = std::make_unique<TestSession>();
    session->testId = testId;
    session->config = config;
    session->progressCallback = callback;
    session->startTime = std::chrono::steady_clock::now();
    session->result.startTime = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    // Start worker thread
    session->isRunning.store(true);
    session->workerThread = std::make_unique<std::thread>(&BandwidthUtilityEngine::runBandwidthTest, this, session.get());
    
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        activeSessions_[testId] = std::move(session);
    }
    
    ENDPOINT_LOG("bandwidth-engine", "Started bandwidth test: " + testId);
    return testId;
}

bool BandwidthUtilityEngine::stopBandwidthTest(const std::string& testId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it == activeSessions_.end()) {
        return false;
    }
    
    auto& session = it->second;
    if (session && session->isRunning.load()) {
        session->shouldStop.store(true);
        
        // Kill any iperf3 processes
        std::string killCmd = "pkill -f 'iperf3.*" + session->config.targetServer + "'";
        system(killCmd.c_str());
        
        if (session->workerThread && session->workerThread->joinable()) {
            session->workerThread->join();
        }
        
        ENDPOINT_LOG("bandwidth-engine", "Stopped bandwidth test: " + testId);
        return true;
    }
    
    return false;
}

bool BandwidthUtilityEngine::isTestRunning(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it != activeSessions_.end() && it->second) {
        return it->second->isRunning.load();
    }
    return false;
}

BandwidthUtilityEngine::BandwidthResult BandwidthUtilityEngine::getTestResult(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it != activeSessions_.end() && it->second) {
        std::lock_guard<std::mutex> resultLock(it->second->resultMutex);
        return it->second->result;
    }
    
    return BandwidthResult{}; // Return empty result
}

std::vector<std::string> BandwidthUtilityEngine::getActiveTestIds() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    std::vector<std::string> activeIds;
    for (const auto& [testId, session] : activeSessions_) {
        if (session && session->isRunning.load()) {
            activeIds.push_back(testId);
        }
    }
    return activeIds;
}

void BandwidthUtilityEngine::runBandwidthTest(TestSession* session) {
    if (!session) return;
    
    try {
        RealtimeUpdate update;
        update.phase = "connecting";
        
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
        std::string command = buildIperfCommand(session->config);
        ENDPOINT_LOG("bandwidth-engine", "Executing: " + command);
        
        update.phase = "testing";
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
        std::string output;
        int exitCode = executeCommandWithCallback(command, 
            [session, &update](const std::string& line) {
                if (session->shouldStop.load()) return;
                
                // Parse real-time output
                BandwidthUtilityEngine* engine = nullptr; // This is a bit hacky, but works for the callback
                if (line.find("Mbits/sec") != std::string::npos || line.find("Gbits/sec") != std::string::npos) {
                    // Parse bandwidth line
                    std::regex bandwidthRegex(R"(([0-9.]+)\s+(Mbits|Gbits)/sec)");
                    std::smatch match;
                    if (std::regex_search(line, match, bandwidthRegex)) {
                        double speed = std::stod(match[1].str());
                        if (match[2].str() == "Gbits") {
                            speed *= 1000; // Convert to Mbps
                        }
                        update.currentMbps = speed;
                        
                        if (session->progressCallback) {
                            session->progressCallback(update);
                        }
                    }
                }
            },
            session->shouldStop,
            session->config.duration + 30
        );
        
        if (session->shouldStop.load()) {
            std::lock_guard<std::mutex> lock(session->resultMutex);
            session->result.success = false;
            session->result.error = "Test stopped by user";
            update.phase = "stopped";
        } else if (exitCode == 0) {
            // Parse final results from iperf3 JSON output
            std::string jsonCommand = command + " --json";
            std::string jsonOutput;
            
            FILE* pipe = popen(jsonCommand.c_str(), "r");
            if (pipe) {
                char buffer[1024];
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    jsonOutput += buffer;
                }
                pclose(pipe);
            }
            
            std::lock_guard<std::mutex> lock(session->resultMutex);
            session->result.success = parseIperfOutput(jsonOutput, session->result);
            session->result.endTime = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            update.phase = "complete";
            update.progress = 100;
        } else {
            std::lock_guard<std::mutex> lock(session->resultMutex);
            session->result.success = false;
            session->result.error = "iperf3 command failed with exit code " + std::to_string(exitCode);
            update.phase = "error";
        }
        
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
    } catch (const std::exception& e) {
        ENDPOINT_LOG("bandwidth-engine", "Exception in bandwidth test: " + std::string(e.what()));
        
        std::lock_guard<std::mutex> lock(session->resultMutex);
        session->result.success = false;
        session->result.error = "Exception: " + std::string(e.what());
        
        RealtimeUpdate update;
        update.phase = "error";
        if (session->progressCallback) {
            session->progressCallback(update);
        }
    }
    
    session->isRunning.store(false);
}

std::string BandwidthUtilityEngine::buildIperfCommand(const BandwidthConfig& config) const {
    std::ostringstream cmd;
    
    cmd << "timeout " << (config.duration + 30) << " iperf3 -c " << sanitizeHostname(config.targetServer);
    cmd << " -p " << config.port;
    cmd << " -t " << config.duration;
    cmd << " -P " << config.parallelConnections;
    cmd << " -i " << config.interval;
    
    if (config.protocol == "udp") {
        cmd << " -u";
        if (config.bandwidth > 0) {
            cmd << " -b " << config.bandwidth << "M";
        }
    }
    
    if (config.bidirectional) {
        cmd << " --bidir";
    }
    
    if (config.bufferSize > 0) {
        cmd << " -l " << config.bufferSize;
    }
    
    // Add error handling
    cmd << " 2>&1";
    
    return cmd.str();
}

bool BandwidthUtilityEngine::parseIperfOutput(const std::string& output, BandwidthResult& result) const {
    try {
        if (output.empty()) {
            result.error = "No output from iperf3";
            return false;
        }
        
        // Try to parse as JSON first
        size_t jsonStart = output.find('{');
        if (jsonStart != std::string::npos) {
            std::string jsonPart = output.substr(jsonStart);
            
            try {
                json iperfJson = json::parse(jsonPart);
                result.fullResults = iperfJson;
                
                // Extract summary information
                if (iperfJson.contains("end") && iperfJson["end"].contains("sum_received")) {
                    auto sumReceived = iperfJson["end"]["sum_received"];
                    if (sumReceived.contains("bits_per_second")) {
                        result.downloadMbps = sumReceived["bits_per_second"].get<double>() / 1000000.0;
                    }
                }
                
                if (iperfJson.contains("end") && iperfJson["end"].contains("sum_sent")) {
                    auto sumSent = iperfJson["end"]["sum_sent"];
                    if (sumSent.contains("bits_per_second")) {
                        result.uploadMbps = sumSent["bits_per_second"].get<double>() / 1000000.0;
                    }
                    if (sumSent.contains("bytes")) {
                        result.totalDataMB = sumSent["bytes"].get<double>() / 1000000.0;
                    }
                }
                
                // Extract server info
                if (iperfJson.contains("start") && iperfJson["start"].contains("connected")) {
                    auto connected = iperfJson["start"]["connected"];
                    if (connected.is_array() && !connected.empty()) {
                        auto conn = connected[0];
                        if (conn.contains("remote_host")) {
                            result.serverInfo = conn["remote_host"].get<std::string>();
                        }
                    }
                }
                
                return true;
                
            } catch (const std::exception& e) {
                ENDPOINT_LOG("bandwidth-engine", "JSON parse error: " + std::string(e.what()));
            }
        }
        
        // Fallback to text parsing
        std::regex bandwidthRegex(R"(([0-9.]+)\s+(Mbits|Gbits)/sec.*receiver)");
        std::smatch match;
        
        if (std::regex_search(output, match, bandwidthRegex)) {
            double speed = std::stod(match[1].str());
            if (match[2].str() == "Gbits") {
                speed *= 1000;
            }
            result.downloadMbps = speed;
            
            // Store raw output in fullResults
            result.fullResults = json{
                {"raw_output", output},
                {"parsed_download_mbps", speed}
            };
            
            return true;
        }
        
        result.error = "Could not parse iperf3 output";
        result.fullResults = json{{"raw_output", output}};
        return false;
        
    } catch (const std::exception& e) {
        result.error = "Parse exception: " + std::string(e.what());
        return false;
    }
}

int BandwidthUtilityEngine::executeCommandWithCallback(const std::string& command,
                                                     std::function<void(const std::string&)> outputCallback,
                                                     const std::atomic<bool>& shouldStop,
                                                     int timeoutSeconds) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return -1;
    }
    
    char buffer[1024];
    auto startTime = std::chrono::steady_clock::now();
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (shouldStop.load()) {
            break;
        }
        
        // Check timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > timeoutSeconds) {
            break;
        }
        
        if (outputCallback) {
            outputCallback(std::string(buffer));
        }
    }
    
    return pclose(pipe);
}

bool BandwidthUtilityEngine::validateConfig(const BandwidthConfig& config, std::string& error) {
    if (config.targetServer.empty()) {
        error = "Target server cannot be empty";
        return false;
    }
    
    if (config.port <= 0 || config.port > 65535) {
        error = "Invalid port number";
        return false;
    }
    
    if (config.duration < 1 || config.duration > 300) {
        error = "Duration must be between 1 and 300 seconds";
        return false;
    }
    
    if (config.parallelConnections < 1 || config.parallelConnections > 20) {
        error = "Parallel connections must be between 1 and 20";
        return false;
    }
    
    if (config.protocol != "tcp" && config.protocol != "udp") {
        error = "Protocol must be tcp or udp";
        return false;
    }
    
    return true;
}

bool BandwidthUtilityEngine::isValidHostname(const std::string& hostname) const {
    if (hostname.empty() || hostname.length() > 253) {
        return false;
    }
    
    // Basic validation - no dangerous characters
    for (char c : hostname) {
        if (c == ';' || c == '|' || c == '&' || c == '`' || 
            c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r') {
            return false;
        }
    }
    
    return true;
}

std::string BandwidthUtilityEngine::sanitizeHostname(const std::string& hostname) const {
    std::string sanitized = hostname;
    
    // Remove dangerous characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
        [](char c) {
            return c == ';' || c == '|' || c == '&' || c == '`' || 
                   c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r';
        }), sanitized.end());
    
    return sanitized;
}

std::string BandwidthUtilityEngine::generateTestId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    return "bandwidth_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

json BandwidthUtilityEngine::configToJson(const BandwidthConfig& config) {
    return json{
        {"targetServer", config.targetServer},
        {"port", config.port},
        {"protocol", config.protocol},
        {"duration", config.duration},
        {"parallelConnections", config.parallelConnections},
        {"bidirectional", config.bidirectional},
        {"bandwidth", config.bandwidth},
        {"bufferSize", config.bufferSize},
        {"interval", config.interval}
    };
}

BandwidthUtilityEngine::BandwidthConfig BandwidthUtilityEngine::configFromJson(const json& j) {
    BandwidthConfig config;
    
    if (j.contains("targetServer")) config.targetServer = j["targetServer"];
    if (j.contains("port")) config.port = j["port"];
    if (j.contains("protocol")) config.protocol = j["protocol"];
    if (j.contains("duration")) config.duration = j["duration"];
    if (j.contains("parallelConnections")) config.parallelConnections = j["parallelConnections"];
    if (j.contains("bidirectional")) config.bidirectional = j["bidirectional"];
    if (j.contains("bandwidth")) config.bandwidth = j["bandwidth"];
    if (j.contains("bufferSize")) config.bufferSize = j["bufferSize"];
    if (j.contains("interval")) config.interval = j["interval"];
    
    return config;
}
