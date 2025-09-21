
#ifndef TRACEROUTE_UTILITY_ENGINE_H
#define TRACEROUTE_UTILITY_ENGINE_H

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

class TracerouteUtilityEngine {
public:
    struct TracerouteConfig {
        std::string targetHost = "8.8.8.8";
        int maxHops = 30;
        int timeout = 5;
        std::string protocol = "icmp"; // icmp, udp, tcp
        int port = 33434;
        int queries = 3;
        bool ipv6 = false;
        bool dontFragment = false;
        bool resolve = true;
    };
    
    struct TracerouteHop {
        int hopNumber = 0;
        std::string hostname = "*";
        std::string ip = "*";
        std::vector<double> rtts;
        std::vector<std::string> errors;
        bool timeout = false;
        bool complete = false;
        std::string asn;
        std::string location;
    };
    
    struct TracerouteResult {
        bool success = false;
        std::string error;
        
        // Configuration
        std::string targetHost;
        std::string resolvedIp;
        int maxHops = 30;
        
        // Results
        std::vector<TracerouteHop> hops;
        bool reachedTarget = false;
        int totalHops = 0;
        
        // Statistics
        double totalTime = 0.0;
        double avgHopTime = 0.0;
        int timeouts = 0;
        
        // Test metadata
        std::string startTime;
        std::string endTime;
        json rawOutput;
    };
    
    struct RealtimeUpdate {
        int currentHop = 0;
        int totalHops = 0;
        TracerouteHop latestHop;
        int progress = 0; // 0-100%
        std::string phase; // "starting", "tracing", "complete", "error"
        bool reachedTarget = false;
    };
    
    using ProgressCallback = std::function<void(const RealtimeUpdate&)>;
    
    TracerouteUtilityEngine();
    ~TracerouteUtilityEngine();
    
    // Test management
    std::string startTraceroute(const TracerouteConfig& config, ProgressCallback callback = nullptr);
    bool stopTraceroute(const std::string& testId);
    bool isTestRunning(const std::string& testId) const;
    
    // Results retrieval
    TracerouteResult getTestResult(const std::string& testId) const;
    std::vector<std::string> getActiveTestIds() const;
    
    // Utility functions
    static bool validateConfig(const TracerouteConfig& config, std::string& error);
    static json configToJson(const TracerouteConfig& config);
    static TracerouteConfig configFromJson(const json& j);

private:
    struct TestSession {
        std::string testId;
        TracerouteConfig config;
        std::atomic<bool> isRunning{false};
        std::atomic<bool> shouldStop{false};
        std::unique_ptr<std::thread> workerThread;
        TracerouteResult result;
        ProgressCallback progressCallback;
        std::chrono::steady_clock::time_point startTime;
        mutable std::mutex resultMutex;
    };
    
    mutable std::mutex sessionsMutex_;
    std::map<std::string, std::unique_ptr<TestSession>> activeSessions_;
    
    // Worker thread functions
    void runTraceroute(TestSession* session);
    std::string buildTracerouteCommand(const TracerouteConfig& config) const;
    TracerouteHop parseTracerouteLine(const std::string& line) const;
    void processTracerouteOutput(const std::vector<std::string>& lines, TracerouteResult& result) const;
    
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
    std::string lookupHostname(const std::string& ip) const;
};

#endif // TRACEROUTE_UTILITY_ENGINE_H
