
#include "PingUtilityEngine.hpp"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>
#include <thread>

void printPingResult(const PingUtilityEngine::PingResult& result) {
    std::cout << "\n=== Ping Test Result ===" << std::endl;
    std::cout << "Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "Target: " << result.targetHost << " (" << result.resolvedIp << ")" << std::endl;
    
    if (!result.success) {
        std::cout << "Error: " << result.error << std::endl;
    } else {
        std::cout << "Packets Sent: " << result.packetsSent << std::endl;
        std::cout << "Packets Received: " << result.packetsReceived << std::endl;
        std::cout << "Packet Loss: " << result.packetLossPercent << "%" << std::endl;
        std::cout << "Min/Avg/Max RTT: " << result.minRtt << "/" << result.avgRtt 
                  << "/" << result.maxRtt << " ms" << std::endl;
        std::cout << "Jitter: " << result.jitter << " ms" << std::endl;
        std::cout << "Total Duration: " << result.totalDuration << " seconds" << std::endl;
        
        std::cout << "Individual packets:" << std::endl;
        for (size_t i = 0; i < std::min(result.packets.size(), size_t(5)); i++) {
            const auto& packet = result.packets[i];
            if (packet.success) {
                std::cout << "  Seq " << packet.sequence << ": " << packet.rtt << " ms" << std::endl;
            } else {
                std::cout << "  Seq " << packet.sequence << ": " << packet.error << std::endl;
            }
        }
        if (result.packets.size() > 5) {
            std::cout << "  ... and " << (result.packets.size() - 5) << " more packets" << std::endl;
        }
    }
}

void testPingEngine() {
    std::cout << "Testing PingUtilityEngine..." << std::endl;
    
    PingUtilityEngine engine;
    
    // Test 1: Configuration validation
    std::cout << "\n--- Test 1: Configuration Validation ---" << std::endl;
    PingUtilityEngine::PingConfig invalidConfig;
    invalidConfig.targetHost = ""; // Invalid empty host
    std::string error;
    bool isValid = PingUtilityEngine::validateConfig(invalidConfig, error);
    std::cout << "Invalid config validation: " << (isValid ? "FAIL" : "PASS") << std::endl;
    std::cout << "Error message: " << error << std::endl;
    
    // Test 2: Valid configuration
    PingUtilityEngine::PingConfig validConfig;
    validConfig.targetHost = "8.8.8.8";
    validConfig.count = 5;
    validConfig.packetSize = 64;
    validConfig.interval = 1.0;
    validConfig.timeout = 3;
    
    isValid = PingUtilityEngine::validateConfig(validConfig, error);
    std::cout << "Valid config validation: " << (isValid ? "PASS" : "FAIL") << std::endl;
    
    // Test 3: JSON serialization
    std::cout << "\n--- Test 3: JSON Serialization ---" << std::endl;
    json configJson = PingUtilityEngine::configToJson(validConfig);
    std::cout << "Config to JSON: " << configJson.dump(2) << std::endl;
    
    auto configFromJson = PingUtilityEngine::configFromJson(configJson);
    std::cout << "JSON to Config validation: " << 
        (configFromJson.targetHost == validConfig.targetHost ? "PASS" : "FAIL") << std::endl;
    
    // Test 4: Basic ping test
    std::cout << "\n--- Test 4: Basic Ping Test ---" << std::endl;
    
    auto progressCallback = [](const PingUtilityEngine::RealtimeUpdate& update) {
        std::cout << "Progress: " << update.progress << "% - Phase: " << update.phase << std::endl;
        if (update.latestPacket.success && update.latestPacket.sequence > 0) {
            std::cout << "  Packet " << update.latestPacket.sequence << ": " 
                      << update.latestPacket.rtt << " ms" << std::endl;
        }
    };
    
    std::string testId = engine.startPingTest(validConfig, progressCallback);
    if (testId.empty()) {
        std::cout << "FAIL: Could not start ping test" << std::endl;
        return;
    }
    
    std::cout << "Started ping test with ID: " << testId << std::endl;
    
    // Monitor test progress
    while (engine.isTestRunning(testId)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    
    // Get final result
    auto result = engine.getTestResult(testId);
    printPingResult(result);
    
    // Test 5: IPv6 test
    std::cout << "\n--- Test 5: IPv6 Ping Test ---" << std::endl;
    
    PingUtilityEngine::PingConfig ipv6Config = validConfig;
    ipv6Config.targetHost = "2001:4860:4860::8888"; // Google IPv6 DNS
    ipv6Config.ipv6 = true;
    ipv6Config.count = 3;
    
    std::string ipv6TestId = engine.startPingTest(ipv6Config);
    if (!ipv6TestId.empty()) {
        while (engine.isTestRunning(ipv6TestId)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        auto ipv6Result = engine.getTestResult(ipv6TestId);
        printPingResult(ipv6Result);
    }
    
    // Test 6: Hostname resolution test
    std::cout << "\n--- Test 6: Hostname Ping Test ---" << std::endl;
    
    PingUtilityEngine::PingConfig hostnameConfig = validConfig;
    hostnameConfig.targetHost = "google.com";
    hostnameConfig.count = 3;
    
    std::string hostnameTestId = engine.startPingTest(hostnameConfig);
    if (!hostnameTestId.empty()) {
        while (engine.isTestRunning(hostnameTestId)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        auto hostnameResult = engine.getTestResult(hostnameTestId);
        printPingResult(hostnameResult);
    }
    
    // Test 7: Large packet size test
    std::cout << "\n--- Test 7: Large Packet Size Test ---" << std::endl;
    
    PingUtilityEngine::PingConfig largeConfig = validConfig;
    largeConfig.packetSize = 1400;
    largeConfig.count = 3;
    largeConfig.dontFragment = true;
    
    std::string largeTestId = engine.startPingTest(largeConfig);
    if (!largeTestId.empty()) {
        while (engine.isTestRunning(largeTestId)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        auto largeResult = engine.getTestResult(largeTestId);
        printPingResult(largeResult);
    }
    
    // Test 8: Stop test functionality
    std::cout << "\n--- Test 8: Stop Test ---" << std::endl;
    
    PingUtilityEngine::PingConfig longConfig = validConfig;
    longConfig.count = 50; // Long test
    longConfig.interval = 1.0;
    
    std::string longTestId = engine.startPingTest(longConfig);
    if (!longTestId.empty()) {
        std::cout << "Started long ping test, will stop after 3 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        bool stopped = engine.stopPingTest(longTestId);
        std::cout << "Stop test result: " << (stopped ? "PASS" : "FAIL") << std::endl;
        
        auto stoppedResult = engine.getTestResult(longTestId);
        std::cout << "Stopped test packets received: " << stoppedResult.packetsReceived << std::endl;
    }
    
    // Test 9: Multiple active tests
    std::cout << "\n--- Test 9: Multiple Active Tests ---" << std::endl;
    
    std::string test1 = engine.startPingTest(validConfig);
    PingUtilityEngine::PingConfig config2 = validConfig;
    config2.targetHost = "1.1.1.1"; // Cloudflare DNS
    std::string test2 = engine.startPingTest(config2);
    
    if (!test1.empty() && !test2.empty()) {
        auto activeTests = engine.getActiveTestIds();
        std::cout << "Active tests count: " << activeTests.size() << std::endl;
        
        // Wait for both to complete
        while (engine.isTestRunning(test1) || engine.isTestRunning(test2)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        
        auto result1 = engine.getTestResult(test1);
        auto result2 = engine.getTestResult(test2);
        
        std::cout << "Test 1 (8.8.8.8) success: " << result1.success << std::endl;
        std::cout << "Test 2 (1.1.1.1) success: " << result2.success << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "PingUtilityEngine Test Suite" << std::endl;
    std::cout << "============================" << std::endl;
    
    try {
        testPingEngine();
        std::cout << "\nAll tests completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
