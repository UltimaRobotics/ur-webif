
#include "Iperf3ServersEngine.hpp"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>
#include <thread>

void printServerInfo(const Iperf3ServersEngine::ServerInfo& server) {
    std::cout << "  ID: " << server.id << std::endl;
    std::cout << "  Name: " << server.name << std::endl;
    std::cout << "  Hostname: " << server.hostname << std::endl;
    std::cout << "  IP: " << server.ip_address << std::endl;
    std::cout << "  Port: " << server.port << std::endl;
    std::cout << "  Location: " << server.location << std::endl;
    std::cout << "  Status: " << server.status << std::endl;
    std::cout << "  Load: " << server.load_percent << "%" << std::endl;
    std::cout << "  Uptime: " << server.uptime_percent << "%" << std::endl;
    std::cout << "  Custom: " << (server.is_custom ? "YES" : "NO") << std::endl;
    std::cout << "  Ping: " << server.ping_ms << " ms" << std::endl;
    std::cout << "  Last Tested: " << server.last_tested << std::endl;
}

void printTestResult(const Iperf3ServersEngine::ServerTestResult& result) {
    std::cout << "  Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "  Status: " << result.status << std::endl;
    std::cout << "  Ping: " << result.ping_ms << " ms" << std::endl;
    std::cout << "  Response Time: " << result.response_time_ms << " ms" << std::endl;
    std::cout << "  Packet Loss: " << result.packet_loss << "%" << std::endl;
    std::cout << "  Jitter: " << result.jitter_ms << " ms" << std::endl;
    std::cout << "  Load: " << result.load_percent << "%" << std::endl;
    if (!result.error_message.empty()) {
        std::cout << "  Error: " << result.error_message << std::endl;
    }
}

void testIperf3ServersEngine() {
    std::cout << "Testing Iperf3ServersEngine..." << std::endl;
    
    Iperf3ServersEngine engine;
    
    // Test 1: Load servers from file
    std::cout << "\n--- Test 1: Load Servers from File ---" << std::endl;
    
    auto servers = engine.getAllServers();
    std::cout << "Total servers loaded: " << servers.size() << std::endl;
    
    if (!servers.empty()) {
        std::cout << "First server details:" << std::endl;
        printServerInfo(servers[0]);
    }
    
    // Test 2: Get active servers
    std::cout << "\n--- Test 2: Get Active Servers ---" << std::endl;
    
    auto activeServers = engine.getActiveServers();
    std::cout << "Active servers: " << activeServers.size() << std::endl;
    
    // Test 3: Get custom servers
    std::cout << "\n--- Test 3: Get Custom Servers ---" << std::endl;
    
    auto customServers = engine.getCustomServers();
    std::cout << "Custom servers: " << customServers.size() << std::endl;
    
    // Test 4: Add custom server
    std::cout << "\n--- Test 4: Add Custom Server ---" << std::endl;
    
    Iperf3ServersEngine::ServerInfo newServer;
    newServer.name = "Test Server";
    newServer.hostname = "iperf.example.com";
    newServer.ip_address = "192.168.1.100";
    newServer.port = 5201;
    newServer.location = "Test Location";
    newServer.region = "Test Region";
    newServer.country = "Test Country";
    
    bool added = engine.addCustomServer(newServer);
    std::cout << "Add custom server result: " << (added ? "SUCCESS" : "FAIL") << std::endl;
    
    if (added) {
        auto updatedServers = engine.getAllServers();
        std::cout << "Total servers after adding: " << updatedServers.size() << std::endl;
        
        // Find the added server
        for (const auto& server : updatedServers) {
            if (server.hostname == newServer.hostname) {
                std::cout << "Added server found with ID: " << server.id << std::endl;
                newServer.id = server.id; // Store for later tests
                break;
            }
        }
    }
    
    // Test 5: Try to add duplicate server
    std::cout << "\n--- Test 5: Try to Add Duplicate Server ---" << std::endl;
    
    bool duplicateAdded = engine.addCustomServer(newServer);
    std::cout << "Add duplicate server result: " << (duplicateAdded ? "FAIL (should not allow)" : "SUCCESS (correctly rejected)") << std::endl;
    
    // Test 6: Server validation
    std::cout << "\n--- Test 6: Server Validation ---" << std::endl;
    
    Iperf3ServersEngine::ServerInfo invalidServer;
    invalidServer.name = ""; // Invalid empty name
    invalidServer.hostname = "test.com";
    
    bool isValid = engine.validateServer(invalidServer);
    std::cout << "Invalid server validation: " << (isValid ? "FAIL" : "PASS") << std::endl;
    
    bool validServerCheck = engine.validateServer(newServer);
    std::cout << "Valid server validation: " << (validServerCheck ? "PASS" : "FAIL") << std::endl;
    
    // Test 7: Server lookup by ID
    std::cout << "\n--- Test 7: Server Lookup by ID ---" << std::endl;
    
    if (!newServer.id.empty()) {
        const auto* foundServer = engine.getServerById(newServer.id);
        std::cout << "Server lookup by ID: " << (foundServer ? "SUCCESS" : "FAIL") << std::endl;
        
        if (foundServer) {
            std::cout << "Found server: " << foundServer->name << std::endl;
        }
    }
    
    // Test 8: Test server connectivity
    std::cout << "\n--- Test 8: Test Server Connectivity ---" << std::endl;
    
    if (!servers.empty()) {
        const auto& testServer = servers[0];
        std::cout << "Testing connectivity for: " << testServer.name << std::endl;
        
        auto testResult = engine.testServerConnectivity(testServer.id);
        printTestResult(testResult);
    }
    
    // Test 9: Update server status
    std::cout << "\n--- Test 9: Update Server Status ---" << std::endl;
    
    if (!servers.empty()) {
        const auto& server = servers[0];
        bool updated = engine.updateServerStatus(server.id, "online", 25.5);
        std::cout << "Update server status result: " << (updated ? "SUCCESS" : "FAIL") << std::endl;
        
        // Verify the update
        const auto* updatedServer = engine.getServerById(server.id);
        if (updatedServer) {
            std::cout << "Updated status: " << updatedServer->status << std::endl;
            std::cout << "Updated load: " << updatedServer->load_percent << "%" << std::endl;
        }
    }
    
    // Test 10: JSON serialization
    std::cout << "\n--- Test 10: JSON Serialization ---" << std::endl;
    
    json serversJson = engine.serversToJson();
    std::cout << "Servers serialized to JSON successfully" << std::endl;
    std::cout << "JSON size: " << serversJson.size() << " servers" << std::endl;
    
    if (!servers.empty()) {
        json singleServerJson = engine.serverToJson(servers[0]);
        std::cout << "Single server JSON:" << std::endl;
        std::cout << singleServerJson.dump(2) << std::endl;
        
        // Test deserialization
        auto deserializedServer = engine.jsonToServer(singleServerJson);
        bool serializationCheck = (deserializedServer.id == servers[0].id);
        std::cout << "JSON serialization round-trip: " << (serializationCheck ? "PASS" : "FAIL") << std::endl;
    }
    
    // Test 11: Get server status summary
    std::cout << "\n--- Test 11: Get Server Status Summary ---" << std::endl;
    
    json statusSummary = engine.getServerStatusSummary();
    std::cout << "Status summary:" << std::endl;
    std::cout << statusSummary.dump(2) << std::endl;
    
    // Test 12: Update custom server
    std::cout << "\n--- Test 12: Update Custom Server ---" << std::endl;
    
    if (!newServer.id.empty()) {
        Iperf3ServersEngine::ServerInfo updatedInfo = newServer;
        updatedInfo.name = "Updated Test Server";
        updatedInfo.location = "Updated Location";
        
        bool updateResult = engine.updateServer(newServer.id, updatedInfo);
        std::cout << "Update custom server result: " << (updateResult ? "SUCCESS" : "FAIL") << std::endl;
        
        if (updateResult) {
            const auto* updated = engine.getServerById(newServer.id);
            if (updated) {
                std::cout << "Updated server name: " << updated->name << std::endl;
                std::cout << "Updated server location: " << updated->location << std::endl;
            }
        }
    }
    
    // Test 13: Test multiple servers asynchronously
    std::cout << "\n--- Test 13: Test Multiple Servers Asynchronously ---" << std::endl;
    
    std::cout << "Starting async tests for all servers..." << std::endl;
    engine.testAllServersAsync();
    
    // Wait a bit for some tests to complete
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "Checking server statuses after async tests..." << std::endl;
    auto currentServers = engine.getAllServers();
    int testedCount = 0;
    for (const auto& server : currentServers) {
        if (!server.last_tested.empty() && server.last_tested != "") {
            testedCount++;
        }
    }
    std::cout << "Servers with test results: " << testedCount << "/" << currentServers.size() << std::endl;
    
    // Test 14: Save servers to file
    std::cout << "\n--- Test 14: Save Servers to File ---" << std::endl;
    
    std::string testFilePath = "test_servers.json";
    bool saved = engine.saveServersToFile(testFilePath);
    std::cout << "Save servers to file result: " << (saved ? "SUCCESS" : "FAIL") << std::endl;
    
    // Test 15: Load servers from test file
    std::cout << "\n--- Test 15: Load Servers from Test File ---" << std::endl;
    
    Iperf3ServersEngine testEngine;
    bool loaded = testEngine.loadServersFromFile(testFilePath);
    std::cout << "Load servers from test file result: " << (loaded ? "SUCCESS" : "FAIL") << std::endl;
    
    if (loaded) {
        auto loadedServers = testEngine.getAllServers();
        std::cout << "Loaded servers count: " << loadedServers.size() << std::endl;
    }
    
    // Test 16: Remove custom server
    std::cout << "\n--- Test 16: Remove Custom Server ---" << std::endl;
    
    if (!newServer.id.empty()) {
        bool removed = engine.removeCustomServer(newServer.id);
        std::cout << "Remove custom server result: " << (removed ? "SUCCESS" : "FAIL") << std::endl;
        
        if (removed) {
            auto finalServers = engine.getAllServers();
            std::cout << "Total servers after removal: " << finalServers.size() << std::endl;
            
            // Verify server was removed
            const auto* removedServer = engine.getServerById(newServer.id);
            std::cout << "Verify removal: " << (removedServer == nullptr ? "SUCCESS" : "FAIL") << std::endl;
        }
    }
    
    // Test 17: Edge cases
    std::cout << "\n--- Test 17: Edge Cases ---" << std::endl;
    
    // Test with non-existent server ID
    auto noResult = engine.testServerConnectivity("non-existent-id");
    std::cout << "Non-existent server test: " << (noResult.success ? "FAIL" : "PASS (correctly failed)") << std::endl;
    
    // Test with invalid server data
    Iperf3ServersEngine::ServerInfo invalidServer2;
    invalidServer2.hostname = ""; // Invalid
    bool invalidAdded = engine.addCustomServer(invalidServer2);
    std::cout << "Invalid server add: " << (invalidAdded ? "FAIL" : "PASS (correctly rejected)") << std::endl;
    
    // Clean up test file
    std::remove(testFilePath.c_str());
}

int main(int argc, char* argv[]) {
    std::cout << "Iperf3ServersEngine Test Suite" << std::endl;
    std::cout << "===============================" << std::endl;
    
    try {
        testIperf3ServersEngine();
        std::cout << "\nAll tests completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
