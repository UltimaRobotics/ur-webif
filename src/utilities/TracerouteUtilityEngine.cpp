
#include "TracerouteUtilityEngine.hpp"
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

TracerouteUtilityEngine::TracerouteUtilityEngine() {
    ENDPOINT_LOG("traceroute-engine", "TracerouteUtilityEngine initialized");
}

TracerouteUtilityEngine::~TracerouteUtilityEngine() {
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
    ENDPOINT_LOG("traceroute-engine", "TracerouteUtilityEngine destroyed");
}

std::string TracerouteUtilityEngine::startTraceroute(const TracerouteConfig& config, ProgressCallback callback) {
    std::string validationError;
    if (!validateConfig(config, validationError)) {
        ENDPOINT_LOG("traceroute-engine", "Invalid config: " + validationError);
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
    session->result.maxHops = config.maxHops;
    
    // Start worker thread
    session->isRunning.store(true);
    session->workerThread = std::make_unique<std::thread>(&TracerouteUtilityEngine::runTraceroute, this, session.get());
    
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        activeSessions_[testId] = std::move(session);
    }
    
    ENDPOINT_LOG("traceroute-engine", "Started traceroute: " + testId);
    return testId;
}

bool TracerouteUtilityEngine::stopTraceroute(const std::string& testId) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it == activeSessions_.end()) {
        return false;
    }
    
    auto& session = it->second;
    if (session && session->isRunning.load()) {
        session->shouldStop.store(true);
        
        // Kill any traceroute processes
        std::string killCmd = "pkill -f 'traceroute.*" + session->config.targetHost + "'";
        system(killCmd.c_str());
        
        if (session->workerThread && session->workerThread->joinable()) {
            session->workerThread->join();
        }
        
        ENDPOINT_LOG("traceroute-engine", "Stopped traceroute: " + testId);
        return true;
    }
    
    return false;
}

bool TracerouteUtilityEngine::isTestRunning(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it != activeSessions_.end() && it->second) {
        return it->second->isRunning.load();
    }
    return false;
}

TracerouteUtilityEngine::TracerouteResult TracerouteUtilityEngine::getTestResult(const std::string& testId) const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = activeSessions_.find(testId);
    if (it != activeSessions_.end() && it->second) {
        std::lock_guard<std::mutex> resultLock(it->second->resultMutex);
        return it->second->result;
    }
    
    return TracerouteResult{}; // Return empty result
}

std::vector<std::string> TracerouteUtilityEngine::getActiveTestIds() const {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    std::vector<std::string> activeIds;
    for (const auto& [testId, session] : activeSessions_) {
        if (session && session->isRunning.load()) {
            activeIds.push_back(testId);
        }
    }
    return activeIds;
}

void TracerouteUtilityEngine::runTraceroute(TestSession* session) {
    if (!session) return;
    
    try {
        RealtimeUpdate update;
        update.phase = "starting";
        update.totalHops = session->config.maxHops;
        
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
        // Resolve hostname first
        std::string resolvedIp = resolveHostname(session->config.targetHost);
        {
            std::lock_guard<std::mutex> lock(session->resultMutex);
            session->result.resolvedIp = resolvedIp;
        }
        
        std::string command = buildTracerouteCommand(session->config);
        ENDPOINT_LOG("traceroute-engine", "Executing: " + command);
        
        update.phase = "tracing";
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
        std::vector<std::string> outputLines;
        int exitCode = executeCommandWithRealtimeOutput(command,
            [session, &update, &outputLines](const std::string& line) {
                if (session->shouldStop.load()) return;
                
                outputLines.push_back(line);
                
                // Parse individual traceroute hop
                TracerouteHop hop = TracerouteUtilityEngine().parseTracerouteLine(line);
                if (hop.hopNumber > 0) {
                    std::lock_guard<std::mutex> lock(session->resultMutex);
                    
                    // Find existing hop or add new one
                    auto it = std::find_if(session->result.hops.begin(), session->result.hops.end(),
                        [&hop](const TracerouteHop& h) { return h.hopNumber == hop.hopNumber; });
                    
                    if (it != session->result.hops.end()) {
                        // Merge data into existing hop
                        if (!hop.rtts.empty()) {
                            it->rtts.insert(it->rtts.end(), hop.rtts.begin(), hop.rtts.end());
                        }
                        if (hop.hostname != "*" && it->hostname == "*") {
                            it->hostname = hop.hostname;
                        }
                        if (hop.ip != "*" && it->ip == "*") {
                            it->ip = hop.ip;
                        }
                        it->timeout = hop.timeout;
                        it->complete = hop.complete;
                    } else {
                        // Add new hop
                        session->result.hops.push_back(hop);
                        std::sort(session->result.hops.begin(), session->result.hops.end(),
                            [](const TracerouteHop& a, const TracerouteHop& b) {
                                return a.hopNumber < b.hopNumber;
                            });
                    }
                    
                    update.currentHop = hop.hopNumber;
                    update.latestHop = hop;
                    update.progress = (100 * hop.hopNumber) / session->config.maxHops;
                    
                    // Check if we reached the target
                    if (hop.ip == session->result.resolvedIp || hop.hostname == session->config.targetHost) {
                        update.reachedTarget = true;
                        session->result.reachedTarget = true;
                        session->result.totalHops = hop.hopNumber;
                    }
                    
                    if (session->progressCallback) {
                        session->progressCallback(update);
                    }
                }
            },
            session->shouldStop,
            session->config.timeout * session->config.maxHops + 60
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
            session->result.totalTime = elapsed.count() / 1000.0;
            
            session->result.rawOutput = json{{"output", fullOutput}};
            
            // Calculate statistics
            if (!session->result.hops.empty()) {
                session->result.totalHops = session->result.hops.back().hopNumber;
                
                double totalHopTime = 0.0;
                int hopCount = 0;
                int timeouts = 0;
                
                for (const auto& hop : session->result.hops) {
                    if (!hop.rtts.empty()) {
                        for (double rtt : hop.rtts) {
                            totalHopTime += rtt;
                            hopCount++;
                        }
                    }
                    if (hop.timeout) {
                        timeouts++;
                    }
                }
                
                if (hopCount > 0) {
                    session->result.avgHopTime = totalHopTime / hopCount;
                }
                session->result.timeouts = timeouts;
            }
            
            if (session->shouldStop.load()) {
                session->result.success = false;
                session->result.error = "Test stopped by user";
                update.phase = "stopped";
            } else if (!session->result.hops.empty()) {
                session->result.success = true;
                update.phase = "complete";
                update.progress = 100;
            } else {
                session->result.success = false;
                session->result.error = "No hops received";
                update.phase = "error";
            }
        }
        
        if (session->progressCallback) {
            session->progressCallback(update);
        }
        
    } catch (const std::exception& e) {
        ENDPOINT_LOG("traceroute-engine", "Exception in traceroute: " + std::string(e.what()));
        
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

std::string TracerouteUtilityEngine::buildTracerouteCommand(const TracerouteConfig& config) const {
    std::ostringstream cmd;
    
    if (config.ipv6) {
        cmd << "traceroute6";
    } else {
        cmd << "traceroute";
    }
    
    cmd << " -m " << config.maxHops;
    cmd << " -w " << config.timeout;
    cmd << " -q " << config.queries;
    
    if (config.protocol == "udp") {
        cmd << " -U";
        if (config.port != 33434) {
            cmd << " -p " << config.port;
        }
    } else if (config.protocol == "tcp") {
        cmd << " -T -p " << config.port;
    }
    
    if (!config.resolve) {
        cmd << " -n";
    }
    
    if (config.dontFragment) {
        cmd << " -F";
    }
    
    cmd << " " << sanitizeHost(config.targetHost);
    cmd << " 2>&1";
    
    return cmd.str();
}

TracerouteUtilityEngine::TracerouteHop TracerouteUtilityEngine::parseTracerouteLine(const std::string& line) const {
    TracerouteHop hop;
    
    // Skip header lines
    if (line.find("traceroute") == 0 || line.empty()) {
        return hop;
    }
    
    // Standard traceroute line pattern: " 1  hostname (ip)  rtt1 ms  rtt2 ms  rtt3 ms"
    std::regex hopRegex(R"(^\s*(\d+)\s+([^\s]+)\s+\(([^)]+)\)\s+([\d.]+)\s*ms)");
    std::smatch match;
    
    if (std::regex_search(line, match, hopRegex)) {
        hop.hopNumber = std::stoi(match[1].str());
        hop.hostname = match[2].str();
        hop.ip = match[3].str();
        
        // Parse all RTT values in the line
        std::regex rttRegex(R"(([\d.]+)\s*ms)");
        std::sregex_iterator rttIt(line.begin(), line.end(), rttRegex);
        std::sregex_iterator rttEnd;
        
        for (; rttIt != rttEnd; ++rttIt) {
            double rtt = std::stod((*rttIt)[1].str());
            hop.rtts.push_back(rtt);
        }
        
        hop.complete = true;
        return hop;
    }
    
    // Pattern for numeric IP only: " 1  192.168.1.1  rtt1 ms  rtt2 ms  rtt3 ms"
    std::regex ipOnlyRegex(R"(^\s*(\d+)\s+([0-9.]+)\s+([\d.]+)\s*ms)");
    if (std::regex_search(line, match, ipOnlyRegex)) {
        hop.hopNumber = std::stoi(match[1].str());
        hop.ip = match[2].str();
        hop.hostname = match[2].str(); // Use IP as hostname
        
        // Parse RTT values
        std::regex rttRegex(R"(([\d.]+)\s*ms)");
        std::sregex_iterator rttIt(line.begin(), line.end(), rttRegex);
        std::sregex_iterator rttEnd;
        
        for (; rttIt != rttEnd; ++rttIt) {
            double rtt = std::stod((*rttIt)[1].str());
            hop.rtts.push_back(rtt);
        }
        
        hop.complete = true;
        return hop;
    }
    
    // Pattern for timeouts: " 1  * * *"
    std::regex timeoutRegex(R"(^\s*(\d+)\s+\*.*\*)");
    if (std::regex_search(line, match, timeoutRegex)) {
        hop.hopNumber = std::stoi(match[1].str());
        hop.timeout = true;
        hop.complete = true;
        return hop;
    }
    
    // Pattern for mixed responses: " 1  hostname (ip) rtt ms  *  rtt ms"
    std::regex mixedRegex(R"(^\s*(\d+)\s+([^\s*]+))");
    if (std::regex_search(line, match, mixedRegex)) {
        hop.hopNumber = std::stoi(match[1].str());
        
        std::string hostPart = match[2].str();
        std::regex hostIpRegex(R"(([^\s(]+)\s*\(([^)]+)\))");
        std::smatch hostMatch;
        
        if (std::regex_search(hostPart, hostMatch, hostIpRegex)) {
            hop.hostname = hostMatch[1].str();
            hop.ip = hostMatch[2].str();
        } else {
            hop.hostname = hostPart;
            hop.ip = hostPart;
        }
        
        // Parse RTT values
        std::regex rttRegex(R"(([\d.]+)\s*ms)");
        std::sregex_iterator rttIt(line.begin(), line.end(), rttRegex);
        std::sregex_iterator rttEnd;
        
        for (; rttIt != rttEnd; ++rttIt) {
            double rtt = std::stod((*rttIt)[1].str());
            hop.rtts.push_back(rtt);
        }
        
        // Check for timeouts in the line
        if (line.find("*") != std::string::npos) {
            hop.timeout = true;
        }
        
        hop.complete = true;
        return hop;
    }
    
    return hop; // Return empty hop if no pattern matches
}

int TracerouteUtilityEngine::executeCommandWithRealtimeOutput(const std::string& command,
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

bool TracerouteUtilityEngine::validateConfig(const TracerouteConfig& config, std::string& error) {
    if (config.targetHost.empty()) {
        error = "Target host cannot be empty";
        return false;
    }
    
    if (config.maxHops < 1 || config.maxHops > 64) {
        error = "Max hops must be between 1 and 64";
        return false;
    }
    
    if (config.timeout < 1 || config.timeout > 60) {
        error = "Timeout must be between 1 and 60 seconds";
        return false;
    }
    
    if (config.queries < 1 || config.queries > 10) {
        error = "Queries must be between 1 and 10";
        return false;
    }
    
    if (config.protocol != "icmp" && config.protocol != "udp" && config.protocol != "tcp") {
        error = "Protocol must be icmp, udp, or tcp";
        return false;
    }
    
    if (config.port < 1 || config.port > 65535) {
        error = "Port must be between 1 and 65535";
        return false;
    }
    
    return true;
}

bool TracerouteUtilityEngine::isValidHost(const std::string& host) const {
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

std::string TracerouteUtilityEngine::sanitizeHost(const std::string& host) const {
    std::string sanitized = host;
    
    // Remove dangerous characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
        [](char c) {
            return c == ';' || c == '|' || c == '&' || c == '`' || 
                   c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r';
        }), sanitized.end());
    
    return sanitized;
}

std::string TracerouteUtilityEngine::generateTestId() const {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    return "traceroute_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
}

std::string TracerouteUtilityEngine::resolveHostname(const std::string& hostname) const {
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

std::string TracerouteUtilityEngine::lookupHostname(const std::string& ip) const {
    std::string command = "getent hosts " + sanitizeHost(ip) + " | head -1 | awk '{print $2}'";
    
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return ip; // Fallback to IP
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
    return result.empty() ? ip : result;
}

json TracerouteUtilityEngine::configToJson(const TracerouteConfig& config) {
    return json{
        {"targetHost", config.targetHost},
        {"maxHops", config.maxHops},
        {"timeout", config.timeout},
        {"protocol", config.protocol},
        {"port", config.port},
        {"queries", config.queries},
        {"ipv6", config.ipv6},
        {"dontFragment", config.dontFragment},
        {"resolve", config.resolve}
    };
}

TracerouteUtilityEngine::TracerouteConfig TracerouteUtilityEngine::configFromJson(const json& j) {
    TracerouteConfig config;
    
    if (j.contains("targetHost")) config.targetHost = j["targetHost"];
    if (j.contains("maxHops")) config.maxHops = j["maxHops"];
    if (j.contains("timeout")) config.timeout = j["timeout"];
    if (j.contains("protocol")) config.protocol = j["protocol"];
    if (j.contains("port")) config.port = j["port"];
    if (j.contains("queries")) config.queries = j["queries"];
    if (j.contains("ipv6")) config.ipv6 = j["ipv6"];
    if (j.contains("dontFragment")) config.dontFragment = j["dontFragment"];
    if (j.contains("resolve")) config.resolve = j["resolve"];
    
    return config;
}
