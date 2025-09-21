
#include "BandwidthUtilityEngine.hpp"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>
#include <thread>

void printBandwidthResult(const BandwidthUtilityEngine::BandwidthResult& result) {
    std::cout << "\n=== Bandwidth Test Result ===" << std::endl;
    std::cout << "Success: " << (result.success ? "YES" : "NO") << std::endl;
    if (!result.success) {
        std::cout << "Error: " << result.error << std::endl;
    } else {
        std::cout << "Download Speed: " << result.downloadMbps << " Mbps" << std::endl;
        std::cout << "Upload Speed: " << result.uploadMbps << " Mbps" << std::endl;
        std::cout << "Total Data: " << result.totalDataMB << " MB" << std::endl;
        std::cout << "Duration: " << result.actualDuration << " seconds" << std::endl;
        std::cout << "Server Info: " << result.serverInfo << std::endl;
    }
}

void testBandwidthEngine() {
    std::cout << "Testing BandwidthUtilityEngine..." << std::endl;
    
    BandwidthUtilityEngine engine;
    
    // Test 1: Configuration validation
    std::cout << "\n--- Test 1: Configuration Validation ---" << std::endl;
    BandwidthUtilityEngine::BandwidthConfig invalidConfig;
    invalidConfig.targetServer = ""; // Invalid empty server
    std::string error;
    bool isValid = BandwidthUtilityEngine::validateConfig(invalidConfig, error);
    std::cout << "Invalid config validation: " << (isValid ? "FAIL" : "PASS") << std::endl;
    std::cout << "Error message: " << error << std::endl;
    
    // Test 2: Valid configuration
    BandwidthUtilityEngine::BandwidthConfig validConfig;
    validConfig.targetServer = "iperf3.bouyguestelecom.fr";
    validConfig.port = 5201;
    validConfig.protocol = "tcp";
    validConfig.duration = 5;
    validConfig.parallelConnections = 1;
    
    isValid = BandwidthUtilityEngine::validateConfig(validConfig, error);
    std::cout << "Valid config validation: " << (isValid ? "PASS" : "FAIL") << std::endl;
    
    // Test 3: JSON serialization
    std::cout << "\n--- Test 3: JSON Serialization ---" << std::endl;
    json configJson = BandwidthUtilityEngine::configToJson(validConfig);
    std::cout << "Config to JSON: " << configJson.dump(2) << std::endl;
    
    auto configFromJson = BandwidthUtilityEngine::configFromJson(configJson);
    std::cout << "JSON to Config validation: " << 
        (configFromJson.targetServer == validConfig.targetServer ? "PASS" : "FAIL") << std::endl;
    
    // Test 4: Bandwidth test with progress callback
    std::cout << "\n--- Test 4: Bandwidth Test with Callback ---" << std::endl;
    
    auto progressCallback = [](const BandwidthUtilityEngine::RealtimeUpdate& update) {
        std::cout << "Progress: " << update.progress << "% - Phase: " << update.phase 
                  << " - Current Speed: " << update.currentMbps << " Mbps" << std::endl;
    };
    
    std::string testId = engine.startBandwidthTest(validConfig, progressCallback);
    if (testId.empty()) {
        std::cout << "FAIL: Could not start bandwidth test" << std::endl;
        return;
    }
    
    std::cout << "Started bandwidth test with ID: " << testId << std::endl;
    
    // Monitor test progress
    while (engine.isTestRunning(testId)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Get final result
    auto result = engine.getTestResult(testId);
    printBandwidthResult(result);
    
    // Test 5: Multiple simultaneous tests
    std::cout << "\n--- Test 5: Multiple Tests ---" << std::endl;
    
    BandwidthUtilityEngine::BandwidthConfig config2 = validConfig;
    config2.duration = 3;
    config2.protocol = "udp";
    
    std::string testId2 = engine.startBandwidthTest(config2);
    if (!testId2.empty()) {
        std::cout << "Started second test with ID: " << testId2 << std::endl;
        
        auto activeTests = engine.getActiveTestIds();
        std::cout << "Active tests count: " << activeTests.size() << std::endl;
        
        // Wait for completion
        while (engine.isTestRunning(testId2)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        auto result2 = engine.getTestResult(testId2);
        printBandwidthResult(result2);
    }
    
    // Test 6: Stop test functionality
    std::cout << "\n--- Test 6: Stop Test ---" << std::endl;
    
    BandwidthUtilityEngine::BandwidthConfig longConfig = validConfig;
    longConfig.duration = 30; // Long test
    
    std::string longTestId = engine.startBandwidthTest(longConfig);
    if (!longTestId.empty()) {
        std::cout << "Started long test, will stop after 2 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        bool stopped = engine.stopBandwidthTest(longTestId);
        std::cout << "Stop test result: " << (stopped ? "PASS" : "FAIL") << std::endl;
        
        auto stoppedResult = engine.getTestResult(longTestId);
        std::cout << "Stopped test success: " << (stoppedResult.success ? "NO (Expected)" : "YES") << std::endl;
        std::cout << "Stop reason: " << stoppedResult.error << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "BandwidthUtilityEngine Test Suite" << std::endl;
    std::cout << "=================================" << std::endl;
    
    try {
        testBandwidthEngine();
        std::cout << "\nAll tests completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
