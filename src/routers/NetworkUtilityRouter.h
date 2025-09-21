#ifndef NETWORK_UTILITY_ROUTER_H
#define NETWORK_UTILITY_ROUTER_H

#include <string>
#include <map>
#include <functional>
#include <memory>
#include "api_request.h"
#include "../third_party/nlohmann/json.hpp"
#include "../utilities/BandwidthUtilityEngine.hpp"
#include "../utilities/PingUtilityEngine.hpp"
#include "../utilities/TracerouteUtilityEngine.hpp"
#include "../utilities/DNSLookupUtilityEngine.hpp"
#include "../utilities/Iperf3ServersEngine.hpp"

using json = nlohmann::json;

class NetworkUtilityRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;

    NetworkUtilityRouter();
    ~NetworkUtilityRouter();

    void registerRoutes(std::function<void(const std::string&, RouteHandler)> registerFunc);
    std::string handleRequest(const std::string& method, const std::string& route, const std::string& body);

private:
    // Test execution handlers
    std::string handleStartBandwidthTest(const json& requestData);
    std::string handleStopBandwidthTest();
    std::string handleGetBandwidthTestStatus();

    std::string handleStartPingTest(const json& requestData);
    std::string handleStopPingTest();
    std::string handleGetPingTestResults();

    std::string handleStartTraceroute(const json& requestData);
    std::string handleStopTraceroute();
    std::string handleGetTracerouteResults();

    std::string handlePerformDnsLookup(const json& requestData);

    // Server status handlers
    std::string handleGetServerStatus();
    std::string handleTestServerConnection(const std::string& serverId);
    std::string handleGetServerList();
    std::string handleAddCustomServer(const json& requestData);
    std::string handleRemoveCustomServer(const std::string& serverId);
    std::string handleUpdateCustomServer(const std::string& serverId, const json& requestData);

    // System diagnostics
    std::string handleGetSystemInfo();
    std::string handleGetNetworkInterfaces();

    // Utility functions
    std::string executeCommand(const std::string& command, int timeoutSeconds = 30);
    std::string formatIperfResults(const std::string& output);
    std::string formatPingResults(const std::string& output);
    std::string formatTracerouteResults(const std::string& output);
    std::string formatDnsResults(const std::string& output);

    // State management
    struct TestState {
        bool isRunning = false;
        std::string testType;
        std::string startTime;
        json configuration;
        std::string results;
        int progress = 0;
        json realTimeResults;
        int currentHop = 0;
        std::string lastUpdate;
    };

    struct ServerConnectivityResult {
        std::string status;
        int load_percent;
        std::string last_tested;
        double ping_ms;
        double response_time_ms;
        double packet_loss;
        double jitter_ms;
    };

    std::map<std::string, TestState> activeTests;

    // Server connectivity testing
    ServerConnectivityResult testServerConnectivity(const std::string& hostname, int port);
    std::string performPingTest(const std::string& hostname);
    std::string performPortConnectivityTest(const std::string& hostname, int port);
    double calculateServerLoad(const std::string& hostname, int port);

    // Configuration
    json serverList;
    std::unique_ptr<Iperf3ServersEngine> serversEngine_;
    void loadServerConfiguration();
    void initializeServersEngine();
    void initializeServersEngineNonBlocking();
    void loadServerConfigurationNonBlocking();
    void initializeUtilityEngines();
    void initializeUtilityEnginesAsync();
    void executeTracerouteWithProgress(const std::string& testId, const std::string& command, int maxHops);

    // Utility engines
    std::unique_ptr<BandwidthUtilityEngine> bandwidthEngine_;
    std::unique_ptr<PingUtilityEngine> pingEngine_;
    std::unique_ptr<TracerouteUtilityEngine> tracerouteEngine_;
    std::unique_ptr<DNSLookupUtilityEngine> dnsEngine_;
};

#endif // NETWORK_UTILITY_ROUTER_H