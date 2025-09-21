
#ifndef BANDWIDTH_UTILITY_ENGINE_H
#define BANDWIDTH_UTILITY_ENGINE_H

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include "../third_party/nlohmann/json.hpp"

using json = nlohmann::json;

class BandwidthUtilityEngine {
public:
    struct BandwidthConfig {
        std::string targetServer = "iperf3.example.com";
        int port = 5201;
        std::string protocol = "tcp"; // tcp or udp
        int duration = 10;
        int parallelConnections = 1;
        bool bidirectional = false;
        int bandwidth = 0; // 0 = unlimited, otherwise in Mbps
        int bufferSize = 0; // 0 = default
        int interval = 1; // reporting interval in seconds
    };
    
    struct BandwidthResult {
        bool success = false;
        std::string error;
        json fullResults;
        
        // Summary stats
        double downloadMbps = 0.0;
        double uploadMbps = 0.0;
        double totalDataMB = 0.0;
        double avgLatency = 0.0;
        double jitter = 0.0;
        double packetLoss = 0.0;
        
        // Test metadata
        std::string startTime;
        std::string endTime;
        int actualDuration = 0;
        std::string serverInfo;
    };
    
    struct RealtimeUpdate {
        double currentMbps = 0.0;
        double totalDataMB = 0.0;
        int elapsedSeconds = 0;
        int progress = 0; // 0-100%
        std::string phase; // "connecting", "testing", "complete", "error"
        json intervalData;
    };
    
    using ProgressCallback = std::function<void(const RealtimeUpdate&)>;
    
    BandwidthUtilityEngine();
    ~BandwidthUtilityEngine();
    
    // Test management
    std::string startBandwidthTest(const BandwidthConfig& config, ProgressCallback callback = nullptr);
    bool stopBandwidthTest(const std::string& testId);
    bool isTestRunning(const std::string& testId) const;
    
    // Results retrieval
    BandwidthResult getTestResult(const std::string& testId) const;
    std::vector<std::string> getActiveTestIds() const;
    
    // Utility functions
    static bool validateConfig(const BandwidthConfig& config, std::string& error);
    static json configToJson(const BandwidthConfig& config);
    static BandwidthConfig configFromJson(const json& j);

private:
    struct TestSession {
        std::string testId;
        BandwidthConfig config;
        std::atomic<bool> isRunning{false};
        std::atomic<bool> shouldStop{false};
        std::unique_ptr<std::thread> workerThread;
        BandwidthResult result;
        ProgressCallback progressCallback;
        std::chrono::steady_clock::time_point startTime;
        mutable std::mutex resultMutex;
    };
    
    mutable std::mutex sessionsMutex_;
    std::map<std::string, std::unique_ptr<TestSession>> activeSessions_;
    
    // Worker thread functions
    void runBandwidthTest(TestSession* session);
    std::string buildIperfCommand(const BandwidthConfig& config) const;
    bool parseIperfOutput(const std::string& output, BandwidthResult& result) const;
    void parseRealtimeOutput(const std::string& line, RealtimeUpdate& update) const;
    
    // Process management
    int executeCommandWithCallback(const std::string& command, 
                                 std::function<void(const std::string&)> outputCallback,
                                 const std::atomic<bool>& shouldStop,
                                 int timeoutSeconds = 300);
    
    // Validation and sanitization
    bool isValidHostname(const std::string& hostname) const;
    std::string sanitizeHostname(const std::string& hostname) const;
    std::string generateTestId() const;
};

#endif // BANDWIDTH_UTILITY_ENGINE_H
