#include "NetworkUtilityRouter.h"
#include "endpoint_logger.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <mutex>

// Global mutex for thread safety
static std::mutex traceroute_mutex;

NetworkUtilityRouter::NetworkUtilityRouter() {
    ENDPOINT_LOG("network-utility", "NetworkUtilityRouter initializing...");

    try {
        // Initialize with strict timeout protection
        auto start = std::chrono::steady_clock::now();
        const auto maxInitTime = std::chrono::seconds(3); // Strict 3-second limit

        // Initialize minimal fallback state first
        serverList = json::array();
        serversEngine_.reset();
        bandwidthEngine_.reset();
        pingEngine_.reset();
        tracerouteEngine_.reset();
        dnsEngine_.reset();

        // Initialize servers engine with timeout protection
        try {
            auto engineStart = std::chrono::steady_clock::now();
            initializeServersEngineNonBlocking();
            auto engineDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - engineStart);
            ENDPOINT_LOG("network-utility", "Servers engine initialized in " + std::to_string(engineDuration.count()) + "ms");
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Warning: Servers engine initialization failed: " + std::string(e.what()));
            // Continue without servers engine - non-critical for basic functionality
        }

        // Check timeout after each major step
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed >= maxInitTime) {
            ENDPOINT_LOG("network-utility", "Warning: NetworkUtilityRouter initialization timeout, using minimal configuration");
            ENDPOINT_LOG("network-utility", "NetworkUtilityRouter initialized with minimal configuration");
            return;
        }

        // Load server configuration with timeout protection
        try {
            loadServerConfigurationNonBlocking();
            ENDPOINT_LOG("network-utility", "Server configuration loaded successfully");
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Warning: Server configuration loading failed: " + std::string(e.what()));
            // Keep empty server list as fallback
        }

        // Check timeout again
        elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed >= maxInitTime) {
            ENDPOINT_LOG("network-utility", "Warning: NetworkUtilityRouter initialization timeout after config load");
            ENDPOINT_LOG("network-utility", "NetworkUtilityRouter initialized with partial configuration");
            return;
        }

        // Initialize utility engines with timeout protection (async)
        try {
            initializeUtilityEnginesAsync();
            ENDPOINT_LOG("network-utility", "Utility engines initialization started (async)");
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Warning: Utility engines initialization failed: " + std::string(e.what()));
            // Continue - engines will be initialized later if needed
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        ENDPOINT_LOG("network-utility", "NetworkUtilityRouter initialized successfully in " + 
                    std::to_string(duration.count()) + "ms");

    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Fatal error during NetworkUtilityRouter initialization: " + std::string(e.what()));
        // Force minimal fallback state
        serverList = json::array();
        serversEngine_.reset();
        bandwidthEngine_.reset();
        pingEngine_.reset();
        tracerouteEngine_.reset();
        dnsEngine_.reset();

        ENDPOINT_LOG("network-utility", "NetworkUtilityRouter initialized with emergency fallback configuration");
        // Don't re-throw - allow system to continue with minimal functionality
    }
}

NetworkUtilityRouter::~NetworkUtilityRouter() {
    ENDPOINT_LOG("network-utility", "NetworkUtilityRouter destroyed");
}

void NetworkUtilityRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> registerFunc) {
    // Bandwidth test routes
    registerFunc("/api/network-utility/bandwidth/start", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            return handleStartBandwidthTest(requestData);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    registerFunc("/api/network-utility/bandwidth/stop", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleStopBandwidthTest();
    });

    registerFunc("/api/network-utility/bandwidth/status", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "GET") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleGetBandwidthTestStatus();
    });

    // Ping test routes
    registerFunc("/api/network-utility/ping/start", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            return handleStartPingTest(requestData);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    registerFunc("/api/network-utility/ping/stop", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleStopPingTest();
    });

    registerFunc("/api/network-utility/ping/results", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "GET") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleGetPingTestResults();
    });

    // Traceroute routes
    registerFunc("/api/network-utility/traceroute/start", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            return handleStartTraceroute(requestData);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    registerFunc("/api/network-utility/traceroute/stop", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleStopTraceroute();
    });

    registerFunc("/api/network-utility/traceroute/results", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "GET") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleGetTracerouteResults();
    });

    // DNS lookup route
    registerFunc("/api/network-utility/dns/lookup", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            return handlePerformDnsLookup(requestData);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    // Server status routes
    registerFunc("/api/network-utility/servers/status", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "GET") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleGetServerStatus();
    });

    registerFunc("/api/network-utility/servers/test", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            std::string serverId = requestData.value("serverId", "");
            return handleTestServerConnection(serverId);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    // System info routes
    registerFunc("/api/network-utility/system/info", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "GET") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleGetSystemInfo();
    });

    registerFunc("/api/network-utility/system/interfaces", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "GET") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleGetNetworkInterfaces();
    });

    // Server list management routes
    registerFunc("/api/network-utility/servers/list", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "GET") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        return handleGetServerList();
    });

    registerFunc("/api/network-utility/servers/add", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            return handleAddCustomServer(requestData);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    registerFunc("/api/network-utility/servers/remove", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            std::string serverId = requestData.value("serverId", "");
            return handleRemoveCustomServer(serverId);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    registerFunc("/api/network-utility/servers/update", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        if (method != "POST") {
            return json{{"success", false}, {"message", "Method not allowed"}}.dump();
        }
        try {
            json requestData = json::parse(body);
            std::string serverId = requestData.value("serverId", "");
            return handleUpdateCustomServer(serverId, requestData);
        } catch (const std::exception& e) {
            return json{{"success", false}, {"message", "Invalid JSON"}}.dump();
        }
    });

    ENDPOINT_LOG("network-utility", "All network utility routes registered successfully");
}

std::string NetworkUtilityRouter::handleStartBandwidthTest(const json& requestData) {
    ENDPOINT_LOG("network-utility", "Starting bandwidth test with new engine");

    if (!bandwidthEngine_) {
        return json{{"success", false}, {"message", "Bandwidth engine not initialized"}}.dump();
    }

    try {
        // Convert request to BandwidthConfig
        BandwidthUtilityEngine::BandwidthConfig config;
        config.targetServer = requestData.value("targetServer", "iperf3.example.com");
        config.port = requestData.value("port", 5201);
        config.protocol = requestData.value("protocol", "tcp");
        config.duration = requestData.value("duration", 10);
        config.parallelConnections = requestData.value("parallelConnections", 1);
        config.bidirectional = requestData.value("bidirectional", false);
        config.bandwidth = requestData.value("bandwidth", 0);
        config.bufferSize = requestData.value("bufferSize", 0);
        config.interval = requestData.value("interval", 1);

        // Start test with progress callback
        std::string testId = bandwidthEngine_->startBandwidthTest(config, 
            [this](const BandwidthUtilityEngine::RealtimeUpdate& update) {
                // Update active tests for compatibility
                // This allows frontend to get real-time updates
                ENDPOINT_LOG("network-utility", "Bandwidth progress: " + std::to_string(update.progress) + "% - " + update.phase);
            });

        if (testId.empty()) {
            return json{{"success", false}, {"message", "Failed to start bandwidth test"}}.dump();
        }

        // Store in activeTests for compatibility with existing frontend
        TestState& state = activeTests[testId];
        state.isRunning = true;
        state.testType = "bandwidth";
        state.startTime = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        state.configuration = requestData;
        state.progress = 0;

        json response = {
            {"success", true},
            {"message", "Bandwidth test started successfully"},
            {"testId", testId},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };

        return response.dump();

    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Error in bandwidth test: " + std::string(e.what()));
        json response = {
            {"success", false},
            {"message", "Failed to start bandwidth test"},
            {"error", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    }
}

std::string NetworkUtilityRouter::handleStopBandwidthTest() {
    ENDPOINT_LOG("network-utility", "Stopping bandwidth test");

    // Kill any running iperf3 processes
    system("pkill -f iperf3");

    // Mark all bandwidth tests as stopped
    for (auto& test : activeTests) {
        if (test.second.testType == "bandwidth" && test.second.isRunning) {
            test.second.isRunning = false;
            test.second.results = "Test stopped by user";
        }
    }

    json response = {
        {"success", true},
        {"message", "Bandwidth test stopped"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleGetBandwidthTestStatus() {
    // Check if we have an active bandwidth test and use the engine for real-time data
    if (bandwidthEngine_) {
        auto activeEngineTests = bandwidthEngine_->getActiveTestIds();

        if (activeEngineTests.empty()) {
            return json{
                {"success", true},
                {"testData", {
                    {"downloadSpeed", 0.0},
                    {"uploadSpeed", 0.0}, 
                    {"latency", 0.0},
                    {"progress", 0.0},
                    {"isRunning", false},
                    {"phase", "idle"},
                    {"bytesTransferred", 0},
                    {"testDuration", 0}
                }},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            }.dump();
        }

        // Return basic test status since we only have test IDs
        return json{
            {"success", true},
            {"testData", {
                {"downloadSpeed", 0.0},
                {"uploadSpeed", 0.0}, 
                {"latency", 0.0},
                {"progress", 50.0},
                {"isRunning", true},
                {"phase", "running"},
                {"bytesTransferred", 0},
                {"testDuration", 0}
            }},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        }.dump();
    }

    // Fallback: check activeTests map for any bandwidth tests
    json response = {
        {"success", true},
        {"activeTests", json::array()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    for (const auto& test : activeTests) {
        if (test.second.testType == "bandwidth") {
            json testInfo = {
                {"testId", test.first},
                {"isRunning", test.second.isRunning},
                {"startTime", test.second.startTime},
                {"progress", test.second.progress},
                {"configuration", test.second.configuration}
            };

            if (!test.second.results.empty()) {
                testInfo["results"] = test.second.results;
            }

            response["activeTests"].push_back(testInfo);

            // If this is the first bandwidth test, also provide testData format for frontend compatibility
            if (response["activeTests"].size() == 1) {
                response["testData"] = {
                    {"downloadSpeed", 0.0},
                    {"uploadSpeed", 0.0}, 
                    {"latency", 0.0},
                    {"progress", test.second.progress},
                    {"isRunning", test.second.isRunning},
                    {"phase", "initializing"},
                    {"bytesTransferred", 0},
                    {"testDuration", 0}
                };
            }
        }
    }

    return response.dump();
}

std::string NetworkUtilityRouter::handleStartPingTest(const json& requestData) {
    ENDPOINT_LOG("network-utility", "Starting ping test");

    std::string testId = "ping_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    std::string targetHost = requestData.value("targetHost", "8.8.8.8");
    int count = requestData.value("count", 10);
    int packetSize = requestData.value("packetSize", 64);
    double interval = requestData.value("interval", 1.0);
    int timeout = requestData.value("timeout", 5);
    bool continuous = requestData.value("continuous", false);

    // Input validation for security
    if (targetHost.empty() || targetHost.length() > 253) {
        return json{{"success", false}, {"message", "Invalid target host"}}.dump();
    }

    // Validate and sanitize target host to prevent command injection
    std::string safeHost = targetHost;
    safeHost.erase(std::remove_if(safeHost.begin(), safeHost.end(), 
        [](char c) { 
            return c == ';' || c == '|' || c == '&' || c == '`' || c == '$' || 
                   c == '\'' || c == '"' || c == '\n' || c == '\r' || c == ' ' ||
                   c == '<' || c == '>' || c == '(' || c == ')';
        }), safeHost.end());

    if (safeHost.empty() || safeHost != targetHost) {
        return json{{"success", false}, {"message", "Invalid characters in target host"}}.dump();
    }

    // Validate numeric parameters
    if (count < 1 || count > 100) {
        return json{{"success", false}, {"message", "Count must be between 1 and 100"}}.dump();
    }
    if (packetSize < 32 || packetSize > 1472) {
        return json{{"success", false}, {"message", "Packet size must be between 32 and 1472 bytes"}}.dump();
    }
    if (interval < 0.1 || interval > 10.0) {
        return json{{"success", false}, {"message", "Interval must be between 0.1 and 10.0 seconds"}}.dump();
    }
    if (timeout < 1 || timeout > 30) {
        return json{{"success", false}, {"message", "Timeout must be between 1 and 30 seconds"}}.dump();
    }

    // Build ping command with validated parameters
    std::ostringstream cmd;
    cmd << "ping -c " << (continuous ? "0" : std::to_string(count));
    cmd << " -s " << packetSize;
    cmd << " -i " << interval;
    cmd << " -W " << timeout;
    cmd << " " << safeHost;

    // Store test state
    TestState& state = activeTests[testId];
    state.isRunning = true;
    state.testType = "ping";
    state.startTime = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    state.configuration = requestData;
    state.progress = 0;
    state.realTimeResults = json::array();

    // Execute command in background thread with real-time parsing
    std::string cmdStr = cmd.str();
    std::thread([this, testId, cmdStr, count, continuous]() {
        try {
            auto testIt = activeTests.find(testId);
            if (testIt == activeTests.end()) return;

            FILE* pipe = popen((cmdStr + " 2>&1").c_str(), "r");
            if (!pipe) {
                testIt->second.isRunning = false;
                testIt->second.results = "Error: Could not execute ping command";
                return;
            }

            char buffer[1024];
            std::string line;
            int sequenceNum = 0;
            int totalPackets = continuous ? 0 : count;

            // Regex patterns for parsing ping output
            std::regex pingRegex(R"(64 bytes from .+?: icmp_seq=(\d+) ttl=(\d+) time=([0-9.]+) ms)");
            std::regex timeoutRegex(R"(no answer yet for icmp_seq=(\d+))");
            std::smatch match;

            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                testIt = activeTests.find(testId);
                if (testIt == activeTests.end() || !testIt->second.isRunning) {
                    break;
                }

                line = std::string(buffer);
                line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());

                if (line.empty()) continue;

                // Parse successful ping response
                if (std::regex_search(line, match, pingRegex)) {
                    try {
                        int seq = std::stoi(match[1].str());
                        int ttl = std::stoi(match[2].str());
                        double rtt = std::stod(match[3].str());

                        json pingResult = {
                            {"sequence", seq},
                            {"ttl", ttl},
                            {"rtt", rtt},
                            {"status", "success"},
                            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()}
                        };

                        testIt->second.realTimeResults.push_back(pingResult);
                        sequenceNum = std::max(sequenceNum, seq);

                        // Update progress
                        if (totalPackets > 0) {
                            testIt->second.progress = std::min(100, (sequenceNum * 100) / totalPackets);
                        } else {
                            testIt->second.progress = std::min(99, sequenceNum * 10); // Continuous mode
                        }

                    } catch (const std::exception& e) {
                        ENDPOINT_LOG("network-utility", "Error parsing ping response: " + std::string(e.what()));
                    }
                }
                // Parse timeout/no response
                else if (std::regex_search(line, match, timeoutRegex)) {
                    try {
                        int seq = std::stoi(match[1].str());

                        json pingResult = {
                            {"sequence", seq},
                            {"ttl", 0},
                            {"rtt", -1},
                            {"status", "timeout"},
                            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()).count()}
                        };

                        testIt->second.realTimeResults.push_back(pingResult);
                        sequenceNum = std::max(sequenceNum, seq);

                        // Update progress
                        if (totalPackets > 0) {
                            testIt->second.progress = std::min(100, (sequenceNum * 100) / totalPackets);
                        }

                    } catch (const std::exception& e) {
                        ENDPOINT_LOG("network-utility", "Error parsing ping timeout: " + std::string(e.what()));
                    }
                }
            }

            pclose(pipe);

            // Final update
            testIt = activeTests.find(testId);
            if (testIt != activeTests.end()) {
                testIt->second.isRunning = false;
                testIt->second.progress = 100;

                // Store formatted results as backup
                if (!testIt->second.realTimeResults.empty()) {
                    testIt->second.results = testIt->second.realTimeResults.dump(2);
                } else {
                    testIt->second.results = "[]";
                }
            }

        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Fatal error in ping execution: " + std::string(e.what()));
            auto testIt = activeTests.find(testId);
            if (testIt != activeTests.end()) {
                testIt->second.isRunning = false;
                testIt->second.results = "Error: " + std::string(e.what());
            }
        }
    }).detach();

    json response = {
        {"success", true},
        {"message", "Ping test started successfully"},
        {"testId", testId},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleStopPingTest() {
    ENDPOINT_LOG("network-utility", "Stopping ping test");

    system("pkill -f ping");

    for (auto& test : activeTests) {
        if (test.second.testType == "ping" && test.second.isRunning) {
            test.second.isRunning = false;
            test.second.results = "Test stopped by user";
        }
    }

    json response = {
        {"success", true},
        {"message", "Ping test stopped"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleGetPingTestResults() {
    json response = {
        {"success", true},
        {"activeTests", json::array()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    for (const auto& test : activeTests) {
        if (test.second.testType == "ping") {
            json testInfo = {
                {"testId", test.first},
                {"isRunning", test.second.isRunning},
                {"startTime", test.second.startTime},
                {"progress", test.second.progress},
                {"configuration", test.second.configuration}
            };

            // Add real-time results if available
            if (!test.second.realTimeResults.is_null()) {
                testInfo["realTimeResults"] = test.second.realTimeResults;

                // Calculate and add statistics from real-time results
                double totalRtt = 0.0;
                int validPings = 0;
                int failedPings = 0;
                double minRtt = std::numeric_limits<double>::max();
                double maxRtt = 0.0;

                for (const auto& result : test.second.realTimeResults) {
                    if (result.contains("rtt") && result["rtt"].is_number()) {
                        double rtt = result["rtt"];
                        totalRtt += rtt;
                        validPings++;
                        minRtt = std::min(minRtt, rtt);
                        maxRtt = std::max(maxRtt, rtt);
                    } else {
                        failedPings++;
                    }
                }

                if (validPings > 0) {
                    testInfo["averageRtt"] = totalRtt / validPings;
                    testInfo["minRtt"] = minRtt;
                    testInfo["maxRtt"] = maxRtt;
                    testInfo["packetLoss"] = (failedPings * 100.0) / (validPings + failedPings);
                    testInfo["packetsReceived"] = validPings;
                    testInfo["packetsSent"] = validPings + failedPings;
                } else {
                    testInfo["averageRtt"] = 0.0;
                    testInfo["minRtt"] = 0.0;
                    testInfo["maxRtt"] = 0.0;
                    testInfo["packetLoss"] = 100.0;
                    testInfo["packetsReceived"] = 0;
                    testInfo["packetsSent"] = failedPings;
                }
            }

            if (!test.second.results.empty()) {
                testInfo["results"] = test.second.results;
            }

            response["activeTests"].push_back(testInfo);
        }
    }

    return response.dump();
}

std::string NetworkUtilityRouter::handleStartTraceroute(const json& requestData) {
    ENDPOINT_LOG("network-utility", "Starting traceroute");

    try {
        std::string testId = "traceroute_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

        std::string targetHost = requestData.value("targetHost", "8.8.8.8");
        int maxHops = requestData.value("maxHops", 30);
        int timeout = requestData.value("timeout", 5);
        std::string protocol = requestData.value("protocol", "icmp");

        // Input validation
        if (targetHost.empty()) {
            return json{{"success", false}, {"message", "Target host cannot be empty"}}.dump();
        }
        if (maxHops < 1 || maxHops > 64) {
            return json{{"success", false}, {"message", "Max hops must be between 1 and 64"}}.dump();
        }
        if (timeout < 1 || timeout > 30) {
            return json{{"success", false}, {"message", "Timeout must be between 1 and 30 seconds"}}.dump();
        }
        if (protocol != "icmp" && protocol != "udp" && protocol != "tcp") {
            return json{{"success", false}, {"message", "Protocol must be icmp, udp, or tcp"}}.dump();
        }

    // Build traceroute command safely
    std::ostringstream cmd;

    // Use basic traceroute command
    cmd << "timeout " << (timeout * maxHops + 10) << " traceroute";

    // Add safe parameters
    cmd << " -m " << maxHops;
    cmd << " -w " << timeout;
    cmd << " -q 1";  // One probe per hop
    cmd << " -n";    // Numeric output only

    // Validate and escape target host
    std::string safeHost = targetHost;

    // Remove dangerous characters and validate
    safeHost.erase(std::remove_if(safeHost.begin(), safeHost.end(), 
        [](char c) { 
            return c == ';' || c == '|' || c == '&' || c == '`' || c == '$' || 
                   c == '\'' || c == '"' || c == '\n' || c == '\r'; 
        }), safeHost.end());

    // Basic hostname/IP validation
    if (safeHost.empty() || safeHost.length() > 253) {
        return json{{"success", false}, {"message", "Invalid target host"}}.dump();
    }

    // Add validated host
    cmd << " " << safeHost;

    // Store test state
    TestState& state = activeTests[testId];
    state.isRunning = true;
    state.testType = "traceroute";
    state.startTime = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    state.configuration = requestData;
    state.progress = 0;
    state.realTimeResults = json::array();

    // Execute command in background thread with real-time updates
    std::string cmdStr = cmd.str();
    std::thread([this, testId, cmdStr, maxHops]() {
        std::lock_guard<std::mutex> lock(traceroute_mutex);
        executeTracerouteWithProgress(testId, cmdStr, maxHops);
    }).detach();

    json response = {
        {"success", true},
        {"message", "Traceroute started successfully"},
        {"testId", testId},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();

    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Error in traceroute: " + std::string(e.what()));
        json response = {
            {"success", false},
            {"message", "Failed to start traceroute"},
            {"error", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    }
}

std::string NetworkUtilityRouter::handleStopTraceroute() {
    system("pkill -f traceroute");

    for (auto& test : activeTests) {
        if (test.second.testType == "traceroute" && test.second.isRunning) {
            test.second.isRunning = false;
            test.second.results = "Test stopped by user";
        }
    }

    json response = {
        {"success", true},
        {"message", "Traceroute stopped"},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleGetTracerouteResults() {
    json response = {
        {"success", true},
        {"activeTests", json::array()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    for (const auto& test : activeTests) {
        if (test.second.testType == "traceroute") {
            json testInfo = {
                {"testId", test.first},
                {"isRunning", test.second.isRunning},
                {"startTime", test.second.startTime},
                {"progress", test.second.progress},
                {"configuration", test.second.configuration},
                {"currentHop", test.second.currentHop}
            };

            // Add real-time results if available
            if (!test.second.realTimeResults.is_null()) {
                testInfo["realTimeResults"] = test.second.realTimeResults;
            } else {
                testInfo["realTimeResults"] = json::array();
            }

            // Add lastUpdate timestamp if available
            if (!test.second.lastUpdate.empty()) {
                testInfo["lastUpdate"] = test.second.lastUpdate;
            }

            if (!test.second.results.empty()) {
                testInfo["results"] = test.second.results;
            }

            response["activeTests"].push_back(testInfo);
        }
    }

    return response.dump();
}

std::string NetworkUtilityRouter::handlePerformDnsLookup(const json& requestData) {
    ENDPOINT_LOG("network-utility", "Performing DNS lookup");

    std::string domain = requestData.value("domain", "example.com");
    std::string recordType = requestData.value("recordType", "A");
    std::string dnsServer = requestData.value("dnsServer", "");
    int timeout = requestData.value("timeout", 5);
    bool trace = requestData.value("trace", false);
    bool recursive = requestData.value("recursive", true);

    // Input validation for security
    if (domain.empty() || domain.length() > 253) {
        return json{{"success", false}, {"message", "Invalid domain name"}}.dump();
    }

    // Validate and sanitize domain to prevent command injection
    std::string safeDomain = domain;
    safeDomain.erase(std::remove_if(safeDomain.begin(), safeDomain.end(), 
        [](char c) { 
            return c == ';' || c == '|' || c == '&' || c == '`' || c == '$' || 
                   c == '\'' || c == '"' || c == '\n' || c == '\r' || c == ' ' ||
                   c == '<' || c == '>' || c == '(' || c == ')';
        }), safeDomain.end());

    if (safeDomain.empty() || safeDomain != domain) {
        return json{{"success", false}, {"message", "Invalid characters in domain name"}}.dump();
    }

    // Validate record type (whitelist approach)
    std::vector<std::string> validRecordTypes = {"A", "AAAA", "CNAME", "MX", "NS", "PTR", "SOA", "SRV", "TXT"};
    if (std::find(validRecordTypes.begin(), validRecordTypes.end(), recordType) == validRecordTypes.end()) {
        return json{{"success", false}, {"message", "Invalid DNS record type"}}.dump();
    }

    // Validate DNS server if specified
    std::string safeDnsServer = dnsServer;
    if (!dnsServer.empty() && dnsServer != "System Default") {
        safeDnsServer.erase(std::remove_if(safeDnsServer.begin(), safeDnsServer.end(), 
            [](char c) { 
                return c == ';' || c == '|' || c == '&' || c == '`' || c == '$' || 
                       c == '\'' || c == '"' || c == '\n' || c == '\r' || c == ' ' ||
                       c == '<' || c == '>' || c == '(' || c == ')';
            }), safeDnsServer.end());

        if (safeDnsServer.empty() || safeDnsServer != dnsServer || dnsServer.length() > 253) {
            return json{{"success", false}, {"message", "Invalid DNS server"}}.dump();
        }
    }

    // Validate timeout parameter
    if (timeout < 1 || timeout > 60) {
        return json{{"success", false}, {"message", "Timeout must be between 1 and 60 seconds"}}.dump();
    }

    // Build dig command with validated parameters
    std::ostringstream cmd;
    cmd << "dig";

    if (!safeDnsServer.empty() && safeDnsServer != "System Default") {
        cmd << " @" << safeDnsServer;
    }

    cmd << " " << safeDomain << " " << recordType;

    if (trace) {
        cmd << " +trace";
    }

    if (!recursive) {
        cmd << " +norecurse";
    }

    cmd << " +time=" << timeout;
    cmd << " +json";

    auto startTime = std::chrono::steady_clock::now();
    std::string result = executeCommand(cmd.str(), timeout + 5);
    auto endTime = std::chrono::steady_clock::now();

    // Calculate query time in milliseconds
    auto queryTime = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();

    // Parse DNS results
    json records = json::array();
    std::string responseCode = "NOERROR";
    std::string serverUsed = dnsServer.empty() ? "System Default" : dnsServer;

    try {
        // Try to parse JSON output from dig +json
        json digOutput = json::parse(result);

        if (digOutput.contains("Status") && digOutput["Status"].is_number()) {
            int status = digOutput["Status"];
            responseCode = (status == 0) ? "NOERROR" : "ERROR";
        }

        // Extract records from dig JSON output
        if (digOutput.contains("Answer") && digOutput["Answer"].is_array()) {
            for (const auto& answer : digOutput["Answer"]) {
                json record = {
                    {"name", answer.value("name", domain)},
                    {"type", answer.value("type", recordType)},
                    {"ttl", answer.value("TTL", 0)},
                    {"value", answer.value("data", "")},
                    {"class", answer.value("class", "IN")}
                };

                // Add type-specific fields
                if (recordType == "MX" && answer.contains("data")) {
                    std::string data = answer["data"];
                    size_t spacePos = data.find(' ');
                    if (spacePos != std::string::npos) {
                        record["priority"] = std::stoi(data.substr(0, spacePos));
                        record["value"] = data.substr(spacePos + 1);
                    }
                }

                records.push_back(record);
            }
        }
    } catch (const std::exception& e) {
        // Fallback: parse text output if JSON parsing fails
        ENDPOINT_LOG("network-utility", "JSON parsing failed, using text parsing: " + std::string(e.what()));

        std::istringstream iss(result);
        std::string line;
        while (std::getline(iss, line)) {
            // Basic text parsing for common record types
            if (line.find(domain) != std::string::npos && 
                line.find(recordType) != std::string::npos &&
                line.find(";;") != 0) {

                std::istringstream lineStream(line);
                std::string name, ttl, cls, type, value;
                lineStream >> name >> ttl >> cls >> type;
                std::getline(lineStream, value);

                if (!value.empty()) {
                    value = value.substr(1); // Remove leading space

                    json record = {
                        {"name", name},
                        {"type", type}, 
                        {"ttl", std::stoi(ttl)},
                        {"value", value},
                        {"class", cls}
                    };

                    records.push_back(record);
                }
            }
        }
    }

    json response = {
        {"success", true},
        {"message", "DNS lookup completed"},
        {"domain", domain},
        {"recordType", recordType},
        {"dnsServer", dnsServer},
        {"queryTime", queryTime},
        {"query_time_ms", queryTime},
        {"serverUsed", serverUsed},
        {"dns_server", serverUsed},
        {"records", records},
        {"recordsCount", records.size()},
        {"responseCode", responseCode},
        {"response_code", responseCode},
        {"rawResults", result},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleGetServerStatus() {
    try {
        if (!serversEngine_) {
            ENDPOINT_LOG("network-utility", "Servers engine not initialized");
            json response = {
                {"success", false},
                {"message", "Servers engine not initialized"},
                {"servers", json::array()},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            return response.dump();
        }

        // Get servers with error handling
        std::vector<Iperf3ServersEngine::ServerInfo> servers;
        try {
            servers = serversEngine_->getAllServers();
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Failed to get servers from engine: " + std::string(e.what()));
            json response = {
                {"success", false},
                {"message", "Failed to retrieve server list"},
                {"servers", json::array()},
                {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()}
            };
            return response.dump();
        }

        json serverArray = json::array();
        ENDPOINT_LOG("network-utility", "Retrieved " + std::to_string(servers.size()) + " servers for status check");

        // Limit server processing to prevent overload
        size_t maxServers = std::min(servers.size(), static_cast<size_t>(20));

        for (size_t i = 0; i < maxServers; ++i) {
            const auto& server = servers[i];

            try {
                // Convert server info to JSON using the engine
                json serverJson = serversEngine_->serverToJson(server);

                // Skip real-time connectivity testing to prevent request hanging
                // Real-time testing should be done via separate async endpoint
                if (server.status == "untested" || server.last_tested.empty()) {
                    // Set default values for untested servers instead of testing now
                    serverJson["status"] = "untested";
                    serverJson["last_tested"] = "Never";
                    serverJson["ping_ms"] = 0;
                    serverJson["response_time_ms"] = 0;
                    serverJson["packet_loss"] = 0;
                    serverJson["jitter_ms"] = 0;
                    serverJson["load_percent"] = 0;
                }

                serverArray.push_back(serverJson);

            } catch (const std::exception& e) {
                ENDPOINT_LOG("network-utility", "Error processing server " + std::to_string(i) + ": " + std::string(e.what()));

                // Add minimal server info even on error
                json errorServerJson = {
                    {"id", "error_" + std::to_string(i)},
                    {"name", "Error"},
                    {"hostname", "unknown"},
                    {"ip_address", ""},
                    {"port", 0},
                    {"location", ""},
                    {"region", ""},
                    {"country", ""},
                    {"status", "Error"},
                    {"load_percent", 0},
                    {"uptime_percent", 0.0},
                    {"is_custom", false},
                    {"last_tested", "Error"},
                    {"ping_ms", -1},
                    {"response_time_ms", -1},
                    {"packet_loss", 100.0},
                    {"jitter_ms", 0.0}
                };
                serverArray.push_back(errorServerJson);
            }
        }

        json response = {
            {"success", true},
            {"servers", serverArray},
            {"summary", serversEngine_->getServerStatusSummary()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };

        return response.dump();

    } catch (const std::bad_alloc& e) {
        ENDPOINT_LOG("network-utility", "Memory allocation error in handleGetServerStatus: " + std::string(e.what()));
        json response = {
            {"success", false},
            {"message", "Memory allocation error"},
            {"servers", json::array()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Fatal error in handleGetServerStatus: " + std::string(e.what()));
        json response = {
            {"success", false},
            {"message", "Failed to retrieve server status"},
            {"error", e.what()},
            {"servers", json::array()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    }
}

std::string NetworkUtilityRouter::handleTestServerConnection(const std::string& serverId) {
    if (!serversEngine_) {
        return json{{"success", false}, {"message", "Servers engine not initialized"}}.dump();
    }

    auto testResult = serversEngine_->testServerConnectivity(serverId);
    const auto* server = serversEngine_->getServerById(serverId);

    if (!server) {
        json response = {
            {"success", false},
            {"message", "Server not found"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    }

    json response = {
        {"success", testResult.success},
        {"serverId", serverId},
        {"hostname", server->hostname},
        {"port", server->port},
        {"connected", testResult.success},
        {"status", testResult.status},
        {"load_percent", testResult.load_percent},
        {"ping_ms", testResult.ping_ms},
        {"response_time_ms", testResult.response_time_ms},
        {"packet_loss", testResult.packet_loss},
        {"jitter_ms", testResult.jitter_ms},
        {"last_tested", testResult.last_tested},
        {"message", testResult.success ? "Connection successful" : testResult.error_message},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleGetSystemInfo() {
    json response = {
        {"success", true},
        {"systemInfo", {
            {"os", executeCommand("uname -o", 5)},
            {"kernel", executeCommand("uname -r", 5)},
            {"architecture", executeCommand("uname -m", 5)},
            {"iperf3_version", executeCommand("iperf3 --version 2>&1 | head -1", 5)},
            {"hostname", executeCommand("hostname", 5)},
            {"uptime", executeCommand("uptime -p", 5)}
        }},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleGetNetworkInterfaces() {
    std::string ifconfig = executeCommand("ip addr show", 10);

    json response = {
        {"success", true},
        {"interfaces", ifconfig},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::executeCommand(const std::string& command, int timeoutSeconds) {
    std::string result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return "Error: Could not execute command";
    }

    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

    int status = pclose(pipe);
    if (status == -1) {
        return "Error: Command execution failed";
    }

    // Trim whitespace
    result.erase(result.find_last_not_of(" \n\r\t") + 1);

    return result;
}

std::string NetworkUtilityRouter::formatIperfResults(const std::string& output) {
    try {
        json result = json::parse(output);
        return result.dump();
    } catch (const std::exception& e) {
        // Fallback to raw output if JSON parsing fails
        return output;
    }
}

std::string NetworkUtilityRouter::formatPingResults(const std::string& output) {
    json result = {
        {"raw_output", output},
        {"parsed_results", json::array()}
    };

    // Parse ping output for structured data
    std::istringstream iss(output);
    std::string line;
    std::regex pingRegex(R"(64 bytes from ([^:]+): icmp_seq=(\d+) ttl=(\d+) time=([0-9.]+) ms)");
    std::smatch match;

    while (std::getline(iss, line)) {
        if (std::regex_search(line, match, pingRegex)) {
            json pingResult = {
                {"host", match[1].str()},
                {"seq", std::stoi(match[2].str())},
                {"ttl", std::stoi(match[3].str())},
                {"time", std::stod(match[4].str())}
            };
            result["parsed_results"].push_back(pingResult);
        }
    }

    return result.dump();
}

std::string NetworkUtilityRouter::formatTracerouteResults(const std::string& output) {
    json result = {
        {"raw_output", output},
        {"parsed_hops", json::array()}
    };

    // Parse traceroute output for structured data
    std::istringstream iss(output);
    std::string line;
    std::regex hopRegex(R"(^\s*(\d+)\s+([^\s]+)\s+\(([^)]+)\)\s+([0-9.]+)\s*ms)");
    std::smatch match;

    while (std::getline(iss, line)) {
        if (std::regex_search(line, match, hopRegex)) {
            json hop = {
                {"hop", std::stoi(match[1].str())},
                {"hostname", match[2].str()},
                {"ip", match[3].str()},
                {"rtt", std::stod(match[4].str())}
            };
            result["parsed_hops"].push_back(hop);
        }
    }

    return result.dump();
}

std::string NetworkUtilityRouter::formatDnsResults(const std::string& output) {
    json result = {
        {"raw_output", output},
        {"parsed_records", json::array()}
    };

    try {
        json dnsJson = json::parse(output);
        result["json_output"] = dnsJson;
    } catch (const std::exception& e) {
        // Parse text output if JSON parsing fails
        result["parse_error"] = e.what();
    }

    return result.dump();
}

std::string NetworkUtilityRouter::handleGetServerList() {
    if (!serversEngine_) {
        return json{{"success", false}, {"message", "Servers engine not initialized"}}.dump();
    }

    auto servers = serversEngine_->getAllServers();
    json serverArray = serversEngine_->serversToJson();

    json response = {
        {"success", true},
        {"servers", serverArray},
        {"total_servers", servers.size()},
        {"summary", serversEngine_->getServerStatusSummary()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleAddCustomServer(const json& requestData) {
    if (!serversEngine_) {
        return json{{"success", false}, {"message", "Servers engine not initialized"}}.dump();
    }

    try {
        Iperf3ServersEngine::ServerInfo newServer;
        newServer.name = requestData.value("name", "");
        newServer.hostname = requestData.value("hostname", "");
        newServer.ip_address = requestData.value("ip_address", "");
        newServer.port = requestData.value("port", 5201);
        newServer.location = requestData.value("location", "");
        newServer.region = requestData.value("region", "");
        newServer.country = requestData.value("country", "");

        // Add metadata if provided
        if (requestData.contains("metadata") && requestData["metadata"].is_object()) {
            for (auto& item : requestData["metadata"].items()) {
                newServer.metadata[item.key()] = item.value();
            }
        }

        bool success = serversEngine_->addCustomServer(newServer);

        json response = {
            {"success", success},
            {"message", success ? "Server added successfully" : "Failed to add server (duplicate or invalid)"},
            {"server_id", success ? serversEngine_->generateServerId(newServer.hostname) : ""},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };

        return response.dump();
    } catch (const std::exception& e) {
        json response = {
            {"success", false},
            {"message", "Invalid server data"},
            {"error", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    }
}

std::string NetworkUtilityRouter::handleRemoveCustomServer(const std::string& serverId) {
    if (!serversEngine_) {
        return json{{"success", false}, {"message", "Servers engine not initialized"}}.dump();
    }

    bool success = serversEngine_->removeCustomServer(serverId);

    json response = {
        {"success", success},
        {"message", success ? "Server removed successfully" : "Server not found or not removable"},
        {"server_id", serverId},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };

    return response.dump();
}

std::string NetworkUtilityRouter::handleUpdateCustomServer(const std::string& serverId, const json& requestData) {
    if (!serversEngine_) {
        return json{{"success", false}, {"message", "Servers engine not initialized"}}.dump();
    }

    const auto* existingServer = serversEngine_->getServerById(serverId);
    if (!existingServer) {
        json response = {
            {"success", false},
            {"message", "Server not found"},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    }

    try {
        Iperf3ServersEngine::ServerInfo updatedServer = *existingServer;

        if (requestData.contains("name")) updatedServer.name = requestData["name"];
        if (requestData.contains("hostname")) updatedServer.hostname = requestData["hostname"];
        if (requestData.contains("ip_address")) updatedServer.ip_address = requestData["ip_address"];
        if (requestData.contains("port")) updatedServer.port = requestData["port"];
        if (requestData.contains("location")) updatedServer.location = requestData["location"];
        if (requestData.contains("region")) updatedServer.region = requestData["region"];
        if (requestData.contains("country")) updatedServer.country = requestData["country"];

        if (requestData.contains("metadata") && requestData["metadata"].is_object()) {
            updatedServer.metadata.clear();
            for (auto& item : requestData["metadata"].items()) {
                updatedServer.metadata[item.key()] = item.value();
            }
        }

        bool success = serversEngine_->updateServer(serverId, updatedServer);

        json response = {
            {"success", success},
            {"message", success ? "Server updated successfully" : "Failed to update server"},
            {"server_id", serverId},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };

        return response.dump();
    } catch (const std::exception& e) {
        json response = {
            {"success", false},
            {"message", "Invalid update data"},
            {"error", e.what()},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()}
        };
        return response.dump();
    }
}

void NetworkUtilityRouter::initializeServersEngineNonBlocking() {
    try {
        auto start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(1500); // 1.5 second timeout

        // Create engine with minimal initialization
        serversEngine_ = std::make_unique<Iperf3ServersEngine>();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        if (duration > timeout) {
            ENDPOINT_LOG("network-utility", "Warning: Servers engine initialization exceeded timeout: " + 
                        std::to_string(duration.count()) + "ms");
            serversEngine_.reset();
            return;
        }

        ENDPOINT_LOG("network-utility", "Iperf3 servers engine initialized successfully in " + 
                    std::to_string(duration.count()) + "ms");

    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Failed to initialize servers engine: " + std::string(e.what()));
        serversEngine_.reset();
    }
}

void NetworkUtilityRouter::initializeServersEngine() {
    initializeServersEngineNonBlocking();
}

NetworkUtilityRouter::ServerConnectivityResult NetworkUtilityRouter::testServerConnectivity(const std::string& hostname, int port) {
    ServerConnectivityResult result;

    // Initialize all fields with safe defaults
    result.status = "Unknown";
    result.ping_ms = -1;
    result.response_time_ms = -1;
    result.packet_loss = 100.0;
    result.jitter_ms = 0.0;
    result.load_percent = 0;
    result.last_tested = "Never";

    try {
        // Validate input parameters
        if (hostname.empty() || port <= 0 || port > 65535) {
            result.status = "Invalid";
            return result;
        }

        // Get current timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        result.last_tested = oss.str();

        // Perform ping test with timeout protection
        std::string pingResult;
        try {
            pingResult = performPingTest(hostname);
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Ping test failed for " + hostname + ": " + std::string(e.what()));
            result.status = "Offline";
            return result;
        }

        if (pingResult.empty()) {
            result.status = "Offline";
            return result;
        }

        // Parse ping results with better error handling
        std::regex pingRegex(R"(time=([0-9.]+) ms)");
        std::smatch match;
        std::vector<double> ping_times;
        ping_times.reserve(10); // Reserve space to avoid reallocations

        try {
            std::istringstream iss(pingResult);
            std::string line;
            while (std::getline(iss, line) && ping_times.size() < 20) { // Limit iterations
                if (std::regex_search(line, match, pingRegex)) {
                    try {
                        double time_val = std::stod(match[1].str());
                        if (time_val >= 0 && time_val < 10000) { // Sanity check
                            ping_times.push_back(time_val);
                        }
                    } catch (const std::exception& e) {
                        // Skip invalid numeric values
                        continue;
                    }
                }
            }
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Error parsing ping results: " + std::string(e.what()));
            result.status = "Error";
            return result;
        }

        // Guard against insufficient samples
        if (ping_times.empty()) {
            result.status = "Offline";
            return result;
        }

        // Calculate statistics safely
        double sum = 0.0;
        for (double time : ping_times) {
            sum += time;
        }
        result.ping_ms = sum / ping_times.size();

        // Calculate jitter (standard deviation) safely
        if (ping_times.size() == 1) {
            result.jitter_ms = 0.0;
        } else {
            double variance = 0.0;
            for (double time : ping_times) {
                double diff = time - result.ping_ms;
                variance += diff * diff;
            }
            result.jitter_ms = std::sqrt(variance / ping_times.size());
        }

        // Test port connectivity with error handling
        std::string portResult;
        try {
            portResult = performPortConnectivityTest(hostname, port);
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Port test failed for " + hostname + ":" + std::to_string(port) + ": " + std::string(e.what()));
            result.status = "Error";
            return result;
        }

        if (portResult.find("Connection successful") != std::string::npos) {
            result.response_time_ms = result.ping_ms;
            result.packet_loss = 0.0;

            // Calculate simulated load safely
            try {
                result.load_percent = calculateServerLoad(hostname, port);
            } catch (const std::exception& e) {
                result.load_percent = 50; // Default moderate load
            }

            // Determine status based on performance
            if (result.ping_ms < 50 && result.load_percent < 80) {
                result.status = "Online";
            } else if (result.ping_ms < 100 && result.load_percent < 90) {
                result.status = "Degraded";
            } else {
                result.status = "High Load";
            }
        } else {
            result.status = "Port Closed";
            result.response_time_ms = -1;
            result.packet_loss = 100.0;
            result.load_percent = 0;
        }

    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Fatal error in testServerConnectivity: " + std::string(e.what()));
        result.status = "Error";
        result.ping_ms = -1;
        result.response_time_ms = -1;
        result.packet_loss = 100.0;
        result.jitter_ms = 0.0;
        result.load_percent = 0;
    }

    return result;
}

std::string NetworkUtilityRouter::performPingTest(const std::string& hostname) {
    if (hostname.empty()) {
        return "";
    }

    try {
        std::ostringstream cmd;
        cmd << "timeout 8 ping -c 3 -W 2 " << hostname << " 2>/dev/null || echo 'ping_failed'";
        std::string result = executeCommand(cmd.str(), 10);

        // Check if the command actually failed
        if (result.find("ping_failed") != std::string::npos) {
            return "";
        }

        return result;
    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Ping command failed: " + std::string(e.what()));
        return "";
    }
}

std::string NetworkUtilityRouter::performPortConnectivityTest(const std::string& hostname, int port) {
    if (hostname.empty() || port <= 0 || port > 65535) {
        return "Connection failed";
    }

    try {
        std::ostringstream cmd;
        cmd << "timeout 3 nc -z -w2 " << hostname << " " << port << " 2>/dev/null && echo \"Connection successful\" || echo \"Connection failed\"";
        return executeCommand(cmd.str(), 5);
    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Port connectivity test failed: " + std::string(e.what()));
        return "Connection failed";
    }
}

double NetworkUtilityRouter::calculateServerLoad(const std::string& hostname, int port) {
    if (hostname.empty()) {
        return 50.0; // Default moderate load
    }

    try {
        // Simple simulated load calculation to avoid complex network operations
        // Use hostname length and port as factors for deterministic but varied results
        size_t hash = std::hash<std::string>{}(hostname);
        int base_load = 20 + ((hash + port) % 60); // 20-80% range

        // Add some randomness but keep it bounded
        int variation = (hash % 20) - 10; // -10 to +10
        int final_load = base_load + variation;

        // Ensure load is within valid range
        if (final_load < 0) final_load = 0;
        if (final_load > 100) final_load = 100;

        return static_cast<double>(final_load);
    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Load calculation failed: " + std::string(e.what()));
        return 50.0; // Default moderate load
    }
}

void NetworkUtilityRouter::executeTracerouteWithProgress(const std::string& testId, const std::string& command, int maxHops) {
    try {
        // Validate inputs first
        if (testId.empty() || command.empty() || maxHops <= 0) {
            ENDPOINT_LOG("network-utility", "Invalid parameters for traceroute");
            return;
        }

        // Check if test still exists before proceeding
        auto testIt = activeTests.find(testId);
        if (testIt == activeTests.end()) {
            ENDPOINT_LOG("network-utility", "Test ID not found: " + testId);
            return;
        }

        ENDPOINT_LOG("network-utility", "Starting traceroute execution for test: " + testId);

        // Use a safer command execution approach
        std::string safeCommand = command + " 2>&1";
        FILE* pipe = popen(safeCommand.c_str(), "r");
        if (!pipe) {
            testIt->second.isRunning = false;
            testIt->second.results = "Error: Could not execute traceroute command";
            ENDPOINT_LOG("network-utility", "Failed to open pipe for traceroute");
            return;
        }

        // Initialize results array if not already done
        if (testIt->second.realTimeResults.is_null()) {
            testIt->second.realTimeResults = json::array();
        }

        char buffer[2048];  // Larger buffer
        std::string line;
        int processedHops = 0;
        int lineCount = 0;
        const int maxLines = 100; // Prevent infinite loops

        // Simplified regex patterns to avoid complex parsing issues
        std::regex hopRegex(R"(^\s*(\d+)\s+(.+?)\s+([0-9.]+)\s*ms)");
        std::regex timeoutRegex(R"(^\s*(\d+)\s+\*\s*\*\s*\*)");

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr && 
               lineCount < maxLines && 
               processedHops < maxHops) {

            lineCount++;

            // Check if test is still valid and running
            testIt = activeTests.find(testId);
            if (testIt == activeTests.end() || !testIt->second.isRunning) {
                break;
            }

            line = std::string(buffer);

            // Clean the line
            line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
            line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());

            if (line.empty() || line.length() < 3) continue;

            std::smatch match;

            // Try to parse successful hop
            if (std::regex_search(line, match, hopRegex)) {
                try {
                    int hopNum = std::stoi(match[1].str());
                    std::string hostInfo = match[2].str();
                    double rtt = std::stod(match[3].str());

                    // Extract hostname and IP from hostInfo
                    std::string hostname = "*";
                    std::string ip = "*";

                    std::regex hostIpRegex(R"(([^\s\(]+)\s*\(([^\)]+)\))");
                    std::smatch hostMatch;
                    if (std::regex_search(hostInfo, hostMatch, hostIpRegex)) {
                        hostname = hostMatch[1].str();
                        ip = hostMatch[2].str();
                    } else {
                        hostname = hostInfo;
                    }

                    json hopData = {
                        {"hop", hopNum},
                        {"hostname", hostname},
                        {"ip", ip},
                        {"rtt1", rtt},
                        {"rtt2", -1},
                        {"rtt3", -1},
                        {"status", "success"},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };

                    testIt->second.realTimeResults.push_back(hopData);
                    testIt->second.currentHop = hopNum;
                    testIt->second.progress = std::min(100, (hopNum * 100) / maxHops);
                    testIt->second.lastUpdate = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());

                    processedHops++;

                } catch (const std::exception& e) {
                    ENDPOINT_LOG("network-utility", "Error parsing hop: " + std::string(e.what()));
                }
            }
            // Try to parse timeout
            else if (std::regex_search(line, match, timeoutRegex)) {
                try {
                    int hopNum = std::stoi(match[1].str());

                    json hopData = {
                        {"hop", hopNum},
                        {"hostname", "*"},
                        {"ip", "*"},
                        {"rtt1", -1},
                        {"rtt2", -1},
                        {"rtt3", -1},
                        {"status", "timeout"},
                        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count()}
                    };

                    testIt->second.realTimeResults.push_back(hopData);
                    testIt->second.currentHop = hopNum;
                    testIt->second.progress = std::min(100, (hopNum * 100) / maxHops);
                    testIt->second.lastUpdate = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count());

                    processedHops++;

                } catch (const std::exception& e) {
                    ENDPOINT_LOG("network-utility", "Error parsing timeout: " + std::string(e.what()));
                }
            }
        }

        // Close pipe safely
        int pipeResult = pclose(pipe);
        ENDPOINT_LOG("network-utility", "Traceroute completed with exit code: " + std::to_string(pipeResult));

        // Update final state if test still exists
        testIt = activeTests.find(testId);
        if (testIt != activeTests.end()) {
            testIt->second.isRunning = false;
            testIt->second.progress = 100;

            // Safely serialize results
            try {
                if (!testIt->second.realTimeResults.empty()) {
                    testIt->second.results = testIt->second.realTimeResults.dump(2);
                } else {
                    testIt->second.results = "[]";
                }
            } catch (const std::exception& e) {
                ENDPOINT_LOG("network-utility", "Error serializing results: " + std::string(e.what()));
                testIt->second.results = "Error: Failed to serialize results";
            }
        }

    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Fatal error in traceroute execution: " + std::string(e.what()));

        // Clean up safely
        auto testIt = activeTests.find(testId);
        if (testIt != activeTests.end()) {
            testIt->second.isRunning = false;
            testIt->second.results = "Error: " + std::string(e.what());
        }
    }
}

void NetworkUtilityRouter::initializeUtilityEnginesAsync() {
    // Initialize engines asynchronously to prevent blocking
    std::thread([this]() {
        try {
            auto start = std::chrono::steady_clock::now();

            // Initialize bandwidth engine
            try {
                bandwidthEngine_ = std::make_unique<BandwidthUtilityEngine>();
                ENDPOINT_LOG("network-utility", "Bandwidth engine initialized (async)");
            } catch (const std::exception& e) {
                ENDPOINT_LOG("network-utility", "Warning: Bandwidth engine initialization failed: " + std::string(e.what()));
            }

            // Initialize ping engine
            try {
                pingEngine_ = std::make_unique<PingUtilityEngine>();
                ENDPOINT_LOG("network-utility", "Ping engine initialized (async)");
            } catch (const std::exception& e) {
                ENDPOINT_LOG("network-utility", "Warning: Ping engine initialization failed: " + std::string(e.what()));
            }

            // Initialize traceroute engine
            try {
                tracerouteEngine_ = std::make_unique<TracerouteUtilityEngine>();
                ENDPOINT_LOG("network-utility", "Traceroute engine initialized (async)");
            } catch (const std::exception& e) {
                ENDPOINT_LOG("network-utility", "Warning: Traceroute engine initialization failed: " + std::string(e.what()));
            }

            // Initialize DNS engine
            try {
                dnsEngine_ = std::make_unique<DNSLookupUtilityEngine>();
                ENDPOINT_LOG("network-utility", "DNS engine initialized (async)");
            } catch (const std::exception& e) {
                ENDPOINT_LOG("network-utility", "Warning: DNS engine initialization failed: " + std::string(e.what()));
            }

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start);

            ENDPOINT_LOG("network-utility", "Utility engines async initialization completed in " + 
                        std::to_string(duration.count()) + "ms");

        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Error during async utility engines initialization: " + std::string(e.what()));
        }
    }).detach();
}

void NetworkUtilityRouter::initializeUtilityEngines() {
    initializeUtilityEnginesAsync();
}

void NetworkUtilityRouter::loadServerConfigurationNonBlocking() {
    auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(1000); // 1 second timeout

    try {
        serverList = json::array();

        if (!serversEngine_) {
            ENDPOINT_LOG("network-utility", "Servers engine not available, using empty configuration");
            return;
        }

        // Get servers with strict timeout protection
        std::vector<Iperf3ServersEngine::ServerInfo> servers;

        try {
            servers = serversEngine_->getAllServers();
        } catch (const std::exception& e) {
            ENDPOINT_LOG("network-utility", "Warning: Failed to get servers from engine: " + std::string(e.what()));
            return;
        }

        // Process only a limited number of servers to prevent blocking
        size_t maxServers = std::min(servers.size(), static_cast<size_t>(10));

        for (size_t i = 0; i < maxServers; ++i) {
            // Check timeout frequently
            auto current = std::chrono::steady_clock::now();
            if (current - start > timeout) {
                ENDPOINT_LOG("network-utility", "Warning: Server configuration loading timed out after processing " + 
                            std::to_string(i) + " servers");
                break;
            }

            const auto& server = servers[i];

            try {
                json serverJson = {
                    {"id", server.id},
                    {"name", server.name},
                    {"host", server.hostname},
                    {"location", server.location},
                    {"port", server.port},
                    {"status", "untested"}, // Don't test connectivity during init
                    {"load", 0}
                };
                serverList.push_back(serverJson);
            } catch (const std::exception& e) {
                ENDPOINT_LOG("network-utility", "Warning: Failed to process server " + std::to_string(i) + ": " + std::string(e.what()));
                continue;
            }
        }

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        ENDPOINT_LOG("network-utility", "Server configuration loaded with " + 
                    std::to_string(serverList.size()) + " servers in " + 
                    std::to_string(duration.count()) + "ms");

    } catch (const std::exception& e) {
        ENDPOINT_LOG("network-utility", "Error during server configuration loading: " + std::string(e.what()));
        serverList = json::array();
    }
}

void NetworkUtilityRouter::loadServerConfiguration() {
    loadServerConfigurationNonBlocking();
}