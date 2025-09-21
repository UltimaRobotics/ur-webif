
#ifndef PING_UTILITY_ENGINE_H
#define PING_UTILITY_ENGINE_H

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

class PingUtilityEngine {
public:
    struct PingConfig {
        std::string targetHost = "8.8.8.8";
        int count = 10;
        int packetSize = 64;
        double interval = 1.0;
        int timeout = 5;
        bool continuous = false;
        bool ipv6 = false;
        bool dontFragment = false;
        int ttl = 64;
    };
    
    struct PingPacket {
        int sequence = 0;
        std::string fromHost;
        int ttl = 0;
        double rtt = 0.0;
        int packetSize = 0;
        std::string timestamp;
        bool success = true;
        std::string error;
    };
    
    struct PingResult {
        bool success = false;
        std::string error;
        
        // Configuration
        std::string targetHost;
        std::string resolvedIp;
        int totalPackets = 0;
        int packetsSent = 0;
        int packetsReceived = 0;
        
        // Statistics
        double packetLossPercent = 100.0;
        double minRtt = 0.0;
        double maxRtt = 0.0;
        double avgRtt = 0.0;
        double stdDevRtt = 0.0;
        double jitter = 0.0;
        
        // Individual packets
        std::vector<PingPacket> packets;
        
        // Test metadata
        std::string startTime;
        std::string endTime;
        double totalDuration = 0.0;
        json rawOutput;
    };
    
    struct RealtimeUpdate {
        int packetsSent = 0;
        int packetsReceived = 0;
        double currentRtt = 0.0;
        double avgRtt = 0.0;
        double packetLossPercent = 100.0;
        int progress = 0; // 0-100%
        std::string phase; // "starting", "pinging", "complete", "error"
        PingPacket latestPacket;
    };
    
    using ProgressCallback = std::function<void(const RealtimeUpdate&)>;
    
    PingUtilityEngine();
    ~PingUtilityEngine();
    
    // Test management
    std::string startPingTest(const PingConfig& config, ProgressCallback callback = nullptr);
    bool stopPingTest(const std::string& testId);
    bool isTestRunning(const std::string& testId) const;
    
    // Results retrieval
    PingResult getTestResult(const std::string& testId) const;
    std::vector<std::string> getActiveTestIds() const;
    
    // Utility functions
    static bool validateConfig(const PingConfig& config, std::string& error);
    static json configToJson(const PingConfig& config);
    static PingConfig configFromJson(const json& j);

private:
    struct TestSession {
        std::string testId;
        PingConfig config;
        std::atomic<bool> isRunning{false};
        std::atomic<bool> shouldStop{false};
        std::unique_ptr<std::thread> workerThread;
        PingResult result;
        ProgressCallback progressCallback;
        std::chrono::steady_clock::time_point startTime;
        mutable std::mutex resultMutex;
    };
    
    mutable std::mutex sessionsMutex_;
    std::map<std::string, std::unique_ptr<TestSession>> activeSessions_;
    
    // Worker thread functions
    void runPingTest(TestSession* session);
    std::string buildPingCommand(const PingConfig& config) const;
    bool parsePingOutput(const std::string& output, PingResult& result) const;
    PingPacket parsePingLine(const std::string& line) const;
    void calculateStatistics(PingResult& result) const;
    
    // Process management
    int executeCommandWithRealtimeOutput(const std::string& command,
                                        std::function<void(const std::string&)> lineCallback,
                                        const std::atomic<bool>& shouldStop,
                                        int timeoutSeconds = 300);
    
    // Validation and utilities
    bool isValidHost(const std::string& host) const;
    std::string sanitizeHost(const std::string& host) const;
    std::string generateTestId() const;
    std::string resolveHostname(const std::string& hostname) const;
};

#endif // PING_UTILITY_ENGINE_H
