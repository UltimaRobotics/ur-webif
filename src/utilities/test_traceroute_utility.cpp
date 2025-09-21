
#include "TracerouteUtilityEngine.hpp"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>
#include <thread>

void printTracerouteResult(const TracerouteUtilityEngine::TracerouteResult& result) {
    std::cout << "\n=== Traceroute Test Result ===" << std::endl;
    std::cout << "Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "Target: " << result.targetHost << " (" << result.resolvedIp << ")" << std::endl;
    
    if (!result.success) {
        std::cout << "Error: " << result.error << std::endl;
    } else {
        std::cout << "Reached Target: " << (result.reachedTarget ? "YES" : "NO") << std::endl;
        std::cout << "Total Hops: " << result.totalHops << std::endl;
        std::cout << "Total Time: " << result.totalTime << " seconds" << std::endl;
        std::cout << "Average Hop Time: " << result.avgHopTime << " ms" << std::endl;
        std::cout << "Timeouts: " << result.timeouts << std::endl;
        
        std::cout << "\nRoute trace:" << std::endl;
        for (const auto& hop : result.hops) {
            std::cout << "  Hop " << hop.hopNumber << ": ";
            
            if (hop.timeout) {
                std::cout << "* * * (timeout)" << std::endl;
            } else {
                std::cout << hop.hostname;
                if (hop.ip != hop.hostname) {
                    std::cout << " (" << hop.ip << ")";
                }
                
                if (!hop.rtts.empty()) {
                    std::cout << "  ";
                    for (size_t i = 0; i < hop.rtts.size(); i++) {
                        std::cout << hop.rtts[i] << " ms";
                        if (i < hop.rtts.size() - 1) std::cout << "  ";
                    }
                }
                std::cout << std::endl;
            }
        }
    }
}

void testTracerouteEngine() {
    std::cout << "Testing TracerouteUtilityEngine..." << std::endl;
    
    TracerouteUtilityEngine engine;
    
    // Test 1: Configuration validation
    std::cout << "\n--- Test 1: Configuration Validation ---" << std::endl;
    TracerouteUtilityEngine::TracerouteConfig invalidConfig;
    invalidConfig.targetHost = ""; // Invalid empty host
    std::string error;
    bool isValid = TracerouteUtilityEngine::validateConfig(invalidConfig, error);
    std::cout << "Invalid config validation: " << (isValid ? "FAIL" : "PASS") << std::endl;
    std::cout << "Error message: " << error << std::endl;
    
    // Test 2: Valid configuration
    TracerouteUtilityEngine::TracerouteConfig validConfig;
    validConfig.targetHost = "8.8.8.8";
    validConfig.maxHops = 15;
    validConfig.timeout = 3;
    validConfig.protocol = "icmp";
    validConfig.queries = 3;
    
    isValid = TracerouteUtilityEngine::validateConfig(validConfig, error);
    std::cout << "Valid config validation: " << (isValid ? "PASS" : "FAIL") << std::endl;
    
    // Test 3: JSON serialization
    std::cout << "\n--- Test 3: JSON Serialization ---" << std::endl;
    json configJson = TracerouteUtilityEngine::configToJson(validConfig);
    std::cout << "Config to JSON: " << configJson.dump(2) << std::endl;
    
    auto configFromJson = TracerouteUtilityEngine::configFromJson(configJson);
    std::cout << "JSON to Config validation: " << 
        (configFromJson.targetHost == validConfig.targetHost ? "PASS" : "FAIL") << std::endl;
    
    // Test 4: Basic traceroute test
    std::cout << "\n--- Test 4: Basic Traceroute Test ---" << std::endl;
    
    auto progressCallback = [](const TracerouteUtilityEngine::RealtimeUpdate& update) {
        std::cout << "Progress: " << update.progress << "% - Phase: " << update.phase 
                  << " - Current Hop: " << update.currentHop << std::endl;
        
        if (update.latestHop.hopNumber > 0 && !update.latestHop.timeout) {
            std::cout << "  Hop " << update.latestHop.hopNumber << ": " 
                      << update.latestHop.hostname << " (" << update.latestHop.ip << ")" << std::endl;
        }
        
        if (update.reachedTarget) {
            std::cout << "  Target reached!" << std::endl;
        }
    };
    
    std::string testId = engine.startTraceroute(validConfig, progressCallback);
    if (testId.empty()) {
        std::cout << "FAIL: Could not start traceroute test" << std::endl;
        return;
    }
    
    std::cout << "Started traceroute test with ID: " << testId << std::endl;
    
    // Monitor test progress
    while (engine.isTestRunning(testId)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Get final result
    auto result = engine.getTestResult(testId);
    printTracerouteResult(result);
    
    // Test 5: UDP traceroute test
    std::cout << "\n--- Test 5: UDP Traceroute Test ---" << std::endl;
    
    TracerouteUtilityEngine::TracerouteConfig udpConfig = validConfig;
    udpConfig.protocol = "udp";
    udpConfig.port = 33434;
    udpConfig.maxHops = 10;
    
    std::string udpTestId = engine.startTraceroute(udpConfig);
    if (!udpTestId.empty()) {
        while (engine.isTestRunning(udpTestId)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        auto udpResult = engine.getTestResult(udpTestId);
        printTracerouteResult(udpResult);
    }
    
    // Test 6: Hostname traceroute test
    std::cout << "\n--- Test 6: Hostname Traceroute Test ---" << std::endl;
    
    TracerouteUtilityEngine::TracerouteConfig hostnameConfig = validConfig;
    hostnameConfig.targetHost = "google.com";
    hostnameConfig.maxHops = 12;
    hostnameConfig.resolve = true;
    
    std::string hostnameTestId = engine.startTraceroute(hostnameConfig);
    if (!hostnameTestId.empty()) {
        while (engine.isTestRunning(hostnameTestId)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        auto hostnameResult = engine.getTestResult(hostnameTestId);
        printTracerouteResult(hostnameResult);
    }
    
    // Test 7: IPv6 traceroute test
    std::cout << "\n--- Test 7: IPv6 Traceroute Test ---" << std::endl;
    
    TracerouteUtilityEngine::TracerouteConfig ipv6Config = validConfig;
    ipv6Config.targetHost = "2001:4860:4860::8888"; // Google IPv6 DNS
    ipv6Config.ipv6 = true;
    ipv6Config.maxHops = 10;
    
    std::string ipv6TestId = engine.startTraceroute(ipv6Config);
    if (!ipv6TestId.empty()) {
        while (engine.isTestRunning(ipv6TestId)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        auto ipv6Result = engine.getTestResult(ipv6TestId);
        printTracerouteResult(ipv6Result);
    }
    
    // Test 8: No hostname resolution test
    std::cout << "\n--- Test 8: No Resolution Traceroute Test ---" << std::endl;
    
    TracerouteUtilityEngine::TracerouteConfig noResolveConfig = validConfig;
    noResolveConfig.resolve = false;
    noResolveConfig.maxHops = 8;
    
    std::string noResolveTestId = engine.startTraceroute(noResolveConfig);
    if (!noResolveTestId.empty()) {
        while (engine.isTestRunning(noResolveTestId)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        auto noResolveResult = engine.getTestResult(noResolveTestId);
        printTracerouteResult(noResolveResult);
    }
    
    // Test 9: Stop test functionality
    std::cout << "\n--- Test 9: Stop Test ---" << std::endl;
    
    TracerouteUtilityEngine::TracerouteConfig longConfig = validConfig;
    longConfig.maxHops = 30; // Long test
    longConfig.timeout = 5;
    
    std::string longTestId = engine.startTraceroute(longConfig);
    if (!longTestId.empty()) {
        std::cout << "Started long traceroute test, will stop after 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        bool stopped = engine.stopTraceroute(longTestId);
        std::cout << "Stop test result: " << (stopped ? "PASS" : "FAIL") << std::endl;
        
        auto stoppedResult = engine.getTestResult(longTestId);
        std::cout << "Stopped test hops completed: " << stoppedResult.hops.size() << std::endl;
    }
    
    // Test 10: Multiple active tests
    std::cout << "\n--- Test 10: Multiple Active Tests ---" << std::endl;
    
    TracerouteUtilityEngine::TracerouteConfig config1 = validConfig;
    config1.maxHops = 8;
    std::string test1 = engine.startTraceroute(config1);
    
    TracerouteUtilityEngine::TracerouteConfig config2 = validConfig;
    config2.targetHost = "1.1.1.1"; // Cloudflare DNS
    config2.maxHops = 8;
    std::string test2 = engine.startTraceroute(config2);
    
    if (!test1.empty() && !test2.empty()) {
        auto activeTests = engine.getActiveTestIds();
        std::cout << "Active tests count: " << activeTests.size() << std::endl;
        
        // Wait for both to complete
        while (engine.isTestRunning(test1) || engine.isTestRunning(test2)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        auto result1 = engine.getTestResult(test1);
        auto result2 = engine.getTestResult(test2);
        
        std::cout << "Test 1 (8.8.8.8) success: " << result1.success 
                  << ", hops: " << result1.hops.size() << std::endl;
        std::cout << "Test 2 (1.1.1.1) success: " << result2.success 
                  << ", hops: " << result2.hops.size() << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "TracerouteUtilityEngine Test Suite" << std::endl;
    std::cout << "===================================" << std::endl;
    
    try {
        testTracerouteEngine();
        std::cout << "\nAll tests completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
