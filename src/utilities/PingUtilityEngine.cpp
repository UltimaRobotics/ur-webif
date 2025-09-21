
#include "PingUtilityEngine.hpp"
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
#include <cmath>

PingUtilityEngine::PingUtilityEngine() {
    ENDPOINT_LOG("ping-engine", "PingUtilityEngine initialized");
}

PingUtilityEngine::~PingUtilityEngine() {
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
    ENDPOINT_LOG("ping-engine", "PingUtilityEngine destroyed");
}

std::string PingUtilityEngine::startPingTest(const PingConfig& config, ProgressCallback callback) {
    std::string validationError;
    if (!validateConfig(config, validationError)) {
        ENDPOINT_LOG("ping-engine", "Invalid config: " + validationError);
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
    session->result.targetHost = config.targetHost;
    
    // Start worker thread
    session->isRunning.store(true);
    session->workerThread = std::make_unique<std::thread>(&PingUtilityEngine::runPingTest, this, session.get());
    
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        activeSessions_[testId] = std::move(session);
    }
    
    ENDPOINT_LOG("ping-engine", "Started ping test: " + testId);
    return testId;
}

bool PingUtilityEngine::stopPingTest(const std::string& testId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it == activeSessions_.end()) {
        return false;
    }
    
    auto& session = it->second;
    if (session && session->isRunning.load()) {
        session->shouldStop.store(true);
        
        // Kill any ping processes
        std::string killCmd = "pkill -f 'ping.*" + session->config.targetHost + "'";
        system(killCmd.c_str());
        
        if (session->workerThread && session->workerThread->joinable()) {
            session->workerThread->join();
        }
        
        ENDPOINT_LOG("ping-engine", "Stopped ping test: " + testId);
        return true;
    }
    
    return false;
}

bool PingUtilityEngine::isTestRunning(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it != activeSessions_.end() && it->second) {
        return it->second->isRunning.load();
    }
    return false;
}

PingUtilityEngine::PingResult PingUtilityEngine::getTestResult(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it != activeSessions_.end() && it->second) {
        std::lock_guard<std::mutex> resultLock(it->second->resultMutex);
        return it->second->result;
    }
    
    return PingResult{}; // Return empty result
}

std::vector<std::string> PingUtilityEngine::getActiveTestIds() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    std::vector<std::string> activeIds;
    for (const auto& [testId, session] : activeSessions_) {
        if (session && session->isRunning.load()) {
            activeIds.push_back(testId);
        }
    }
    return activeIds;
}

void PingUtilityEngine::runPingTest(TestSession* session) {
    if (!session) return;
    
    try {
        RealtimeUpdate update;
        update.phase = "starting";
        
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
        // Resolve hostname first
        std::string resolvedIp = resolveHostname(session->config.targetHost);
        {
            std::lock_guard<std::mutex> lock(session->resultMutex);
            session->result.resolvedIp = resolvedIp;
        }
        
        std::string command = buildPingCommand(session->config);
        ENDPOINT_LOG("ping-engine", "Executing: " + command);
        
        update.phase = "pinging";
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
        std::vector<std::string> outputLines;
        int exitCode = executeCommandWithRealtimeOutput(command,
            [session, &update, &outputLines](const std::string& line) {
                if (session->shouldStop.load()) return;
                
                outputLines.push_back(line);
                
                // Parse individual ping response
                PingPacket packet = PingUtilityEngine().parsePingLine(line);
                if (packet.success) {
                    std::lock_guard<std::mutex> lock(session->resultMutex);
                    session->result.packets.push_back(packet);
                    session->result.packetsReceived++;
                    
                    update.latestPacket = packet;
                    update.packetsReceived = session->result.packetsReceived;
                    update.currentRtt = packet.rtt;
                    
                    // Calculate running average
                    double totalRtt = 0.0;
                    for (const auto& p : session->result.packets) {
                        totalRtt += p.rtt;
                    }
                    update.avgRtt = totalRtt / session->result.packets.size();
                    
                    if (session->result.totalPackets > 0) {
                        update.packetLossPercent = 100.0 * (1.0 - (double)session->result.packetsReceived / session->result.totalPackets);
                        update.progress = (100 * session->result.packetsReceived) / session->result.totalPackets;
                    }
                    
                    if (session->progressCallback) {
                        session->progressCallback(update);
                    }
                }
                
                // Track packets sent
                if (line.find("PING") != std::string::npos && line.find("bytes of data") != std::string::npos) {
                    std::lock_guard<std::mutex> lock(session->resultMutex);
                    session->result.totalPackets = session->config.count;
                    update.packetsSent = 1;
                } else if (packet.sequence > 0) {
                    std::lock_guard<std::mutex> lock(session->resultMutex);
                    session->result.packetsSent = std::max(session->result.packetsSent, packet.sequence);
                    update.packetsSent = session->result.packetsSent;
                }
            },
            session->shouldStop,
            session->config.timeout * session->config.count + 30
        );
        
        // Process final results
        std::string fullOutput;
        for (const auto& line : outputLines) {
            fullOutput += line + "\n";
        }
        
        {
            std::lock_guard<std::mutex> lock(session->resultMutex);
            session->result.endTime = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - session->startTime);
            session->result.totalDuration = elapsed.count() / 1000.0;
            
            session->result.rawOutput = json{{"output", fullOutput}};
            
            // Calculate final statistics
            calculateStatistics(session->result);
            
            if (session->shouldStop.load()) {
                session->result.success = false;
                session->result.error = "Test stopped by user";
                update.phase = "stopped";
            } else if (session->result.packetsReceived > 0) {
                session->result.success = true;
                update.phase = "complete";
                update.progress = 100;
            } else {
                session->result.success = false;
                session->result.error = "No packets received";
                update.phase = "error";
            }
        }
        
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
    } catch (const std::exception& e) {
        ENDPOINT_LOG("ping-engine", "Exception in ping test: " + std::string(e.what()));
        
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

std::string PingUtilityEngine::buildPingCommand(const PingConfig& config) const {
    std::ostringstream cmd;
    
    cmd << "ping";
    
    if (config.ipv6) {
        cmd << "6";
    }
    
    cmd << " -c " << (config.continuous ? "0" : std::to_string(config.count));
    cmd << " -s " << config.packetSize;
    cmd << " -i " << std::fixed << std::setprecision(1) << config.interval;
    cmd << " -W " << config.timeout;
    cmd << " -t " << config.ttl;
    
    if (config.dontFragment) {
        cmd << " -M do";
    }
    
    cmd << " " << sanitizeHost(config.targetHost);
    cmd << " 2>&1";
    
    return cmd.str();
}

PingUtilityEngine::PingPacket PingUtilityEngine::parsePingLine(const std::string& line) const {
    PingPacket packet;
    
    // Standard ping response pattern
    std::regex pingRegex(R"((\d+) bytes from ([^:]+): icmp_seq=(\d+) ttl=(\d+) time=([0-9.]+) ms)");
    std::smatch match;
    
    if (std::regex_search(line, match, pingRegex)) {
        packet.success = true;
        packet.packetSize = std::stoi(match[1].str());
        packet.fromHost = match[2].str();
        packet.sequence = std::stoi(match[3].str());
        packet.ttl = std::stoi(match[4].str());
        packet.rtt = std::stod(match[5].str());
        
        auto now = std::chrono::system_clock::now();
        packet.timestamp = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
    } else {
        // Check for error patterns
        if (line.find("Destination Host Unreachable") != std::string::npos) {
            packet.success = false;
            packet.error = "Destination Host Unreachable";
        } else if (line.find("Request timeout") != std::string::npos || line.find("no answer") != std::string::npos) {
            packet.success = false;
            packet.error = "Request timeout";
        } else if (line.find("Network is unreachable") != std::string::npos) {
            packet.success = false;
            packet.error = "Network is unreachable";
        }
        
        // Try to extract sequence number from error lines
        std::regex seqRegex(R"(icmp_seq=(\d+))");
        if (std::regex_search(line, match, seqRegex)) {
            packet.sequence = std::stoi(match[1].str());
        }
    }
    
    return packet;
}

void PingUtilityEngine::calculateStatistics(PingResult& result) const {
    if (result.packets.empty()) {
        return;
    }
    
    // Basic counts
    result.packetsReceived = 0;
    result.packetsSent = result.packets.size(); // This might need adjustment based on actual counting
    
    std::vector<double> rtts;
    for (const auto& packet : result.packets) {
        if (packet.success) {
            result.packetsReceived++;
            rtts.push_back(packet.rtt);
        }
    }
    
    // Packet loss
    if (result.totalPackets > 0) {
        result.packetLossPercent = 100.0 * (1.0 - (double)result.packetsReceived / result.totalPackets);
    } else if (result.packetsSent > 0) {
        result.packetLossPercent = 100.0 * (1.0 - (double)result.packetsReceived / result.packetsSent);
    }
    
    // RTT statistics
    if (!rtts.empty()) {
        result.minRtt = *std::min_element(rtts.begin(), rtts.end());
        result.maxRtt = *std::max_element(rtts.begin(), rtts.end());
        
        double sum = 0.0;
        for (double rtt : rtts) {
            sum += rtt;
        }
        result.avgRtt = sum / rtts.size();
        
        // Standard deviation
        double variance = 0.0;
        for (double rtt : rtts) {
            double diff = rtt - result.avgRtt;
            variance += diff * diff;
        }
        result.stdDevRtt = std::sqrt(variance / rtts.size());
        
        // Jitter (approximate)
        if (rtts.size() > 1) {
            double jitterSum = 0.0;
            for (size_t i = 1; i < rtts.size(); i++) {
                jitterSum += std::abs(rtts[i] - rtts[i-1]);
            }
            result.jitter = jitterSum / (rtts.size() - 1);
        }
    }
}

int PingUtilityEngine::executeCommandWithRealtimeOutput(const std::string& command,
                                                       std::function<void(const std::string&)> lineCallback,
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
        
        std::string line(buffer);
        // Remove trailing newline
        if (!line.empty() && line.back() == '\n') {
            line.pop_back();
        }
        
        if (lineCallback && !line.empty()) {
            lineCallback(line);
        }
    }
    
    return pclose(pipe);
}

bool PingUtilityEngine::validateConfig(const PingConfig& config, std::string& error) {
    if (config.targetHost.empty()) {
        error = "Target host cannot be empty";
        return false;
    }
    
    if (config.count < 1 || config.count > 1000) {
        error = "Count must be between 1 and 1000";
        return false;
    }
    
    if (config.packetSize < 8 || config.packetSize > 65507) {
        error = "Packet size must be between 8 and 65507 bytes";
        return false;
    }
    
    if (config.interval < 0.1 || config.interval > 60.0) {
        error = "Interval must be between 0.1 and 60 seconds";
        return false;
    }
    
    if (config.timeout < 1 || config.timeout > 60) {
        error = "Timeout must be between 1 and 60 seconds";
        return false;
    }
    
    if (config.ttl < 1 || config.ttl > 255) {
        error = "TTL must be between 1 and 255";
        return false;
    }
    
    return true;
}

bool PingUtilityEngine::isValidHost(const std::string& host) const {
    if (host.empty() || host.length() > 253) {
        return false;
    }
    
    // Basic validation - no dangerous characters
    for (char c : host) {
        if (c == ';' || c == '|' || c == '&' || c == '`' || 
            c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r') {
            return false;
        }
    }
    
    return true;
}

std::string PingUtilityEngine::sanitizeHost(const std::string& host) const {
    std::string sanitized = host;
    
    // Remove dangerous characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
        [](char c) {
            return c == ';' || c == '|' || c == '&' || c == '`' || 
                   c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r';
        }), sanitized.end());
    
    return sanitized;
}

std::string PingUtilityEngine::generateTestId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    return "ping_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

std::string PingUtilityEngine::resolveHostname(const std::string& hostname) const {
    std::string command = "getent hosts " + sanitizeHost(hostname) + " | head -1 | awk '{print $1}'";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return hostname; // Fallback to original
    }
    
    char buffer[256];
    std::string result;
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result = std::string(buffer);
        // Remove trailing newline
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
    }
    
    pclose(pipe);
    return result.empty() ? hostname : result;
}

json PingUtilityEngine::configToJson(const PingConfig& config) {
    return json{
        {"targetHost", config.targetHost},
        {"count", config.count},
        {"packetSize", config.packetSize},
        {"interval", config.interval},
        {"timeout", config.timeout},
        {"continuous", config.continuous},
        {"ipv6", config.ipv6},
        {"dontFragment", config.dontFragment},
        {"ttl", config.ttl}
    };
}

PingUtilityEngine::PingConfig PingUtilityEngine::configFromJson(const json& j) {
    PingConfig config;
    
    if (j.contains("targetHost")) config.targetHost = j["targetHost"];
    if (j.contains("count")) config.count = j["count"];
    if (j.contains("packetSize")) config.packetSize = j["packetSize"];
    if (j.contains("interval")) config.interval = j["interval"];
    if (j.contains("timeout")) config.timeout = j["timeout"];
    if (j.contains("continuous")) config.continuous = j["continuous"];
    if (j.contains("ipv6")) config.ipv6 = j["ipv6"];
    if (j.contains("dontFragment")) config.dontFragment = j["dontFragment"];
    if (j.contains("ttl")) config.ttl = j["ttl"];
    
    return config;
}
