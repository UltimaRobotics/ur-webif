
#include "DNSLookupUtilityEngine.hpp"
#include "endpoint_logger.h"
#include <iostream>
#include <chrono>
#include <thread>

void printDNSResult(const DNSLookupUtilityEngine::DNSResult& result) {
    std::cout << "\n=== DNS Lookup Result ===" << std::endl;
    std::cout << "Success: " << (result.success ? "YES" : "NO") << std::endl;
    std::cout << "Domain: " << result.domain << std::endl;
    std::cout << "Record Type: " << result.recordType << std::endl;
    std::cout << "DNS Server: " << result.dnsServer << std::endl;
    
    if (!result.success) {
        std::cout << "Error: " << result.error << std::endl;
    } else {
        std::cout << "Query Time: " << result.queryTime << " ms" << std::endl;
        std::cout << "Resolved Server: " << result.resolvedServer << std::endl;
        std::cout << "Authoritative: " << (result.authoritative ? "YES" : "NO") << std::endl;
        std::cout << "Recursion Available: " << (result.recursionAvailable ? "YES" : "NO") << std::endl;
        
        std::cout << "Answer Records (" << result.records.size() << "):" << std::endl;
        for (const auto& record : result.records) {
            std::cout << "  " << record.name << " " << record.ttl << " " 
                      << record.recordClass << " " << record.type << " " << record.value;
            if (record.priority > 0) {
                std::cout << " (priority: " << record.priority << ")";
            }
            std::cout << std::endl;
        }
        
        if (!result.authorityRecords.empty()) {
            std::cout << "Authority Records (" << result.authorityRecords.size() << "):" << std::endl;
            for (size_t i = 0; i < std::min(result.authorityRecords.size(), size_t(3)); i++) {
                const auto& record = result.authorityRecords[i];
                std::cout << "  " << record.name << " " << record.ttl << " " 
                          << record.recordClass << " " << record.type << " " << record.value << std::endl;
            }
        }
        
        if (!result.additionalRecords.empty()) {
            std::cout << "Additional Records (" << result.additionalRecords.size() << "):" << std::endl;
            for (size_t i = 0; i < std::min(result.additionalRecords.size(), size_t(3)); i++) {
                const auto& record = result.additionalRecords[i];
                std::cout << "  " << record.name << " " << record.ttl << " " 
                          << record.recordClass << " " << record.type << " " << record.value << std::endl;
            }
        }
    }
}

void testDNSLookupEngine() {
    std::cout << "Testing DNSLookupUtilityEngine..." << std::endl;
    
    DNSLookupUtilityEngine engine;
    
    // Test 1: Configuration validation
    std::cout << "\n--- Test 1: Configuration Validation ---" << std::endl;
    DNSLookupUtilityEngine::DNSConfig invalidConfig;
    invalidConfig.domain = ""; // Invalid empty domain
    std::string error;
    bool isValid = DNSLookupUtilityEngine::validateConfig(invalidConfig, error);
    std::cout << "Invalid config validation: " << (isValid ? "FAIL" : "PASS") << std::endl;
    std::cout << "Error message: " << error << std::endl;
    
    // Test 2: Valid configuration
    DNSLookupUtilityEngine::DNSConfig validConfig;
    validConfig.domain = "google.com";
    validConfig.recordType = "A";
    validConfig.timeout = 5;
    
    isValid = DNSLookupUtilityEngine::validateConfig(validConfig, error);
    std::cout << "Valid config validation: " << (isValid ? "PASS" : "FAIL") << std::endl;
    
    // Test 3: Supported record types
    std::cout << "\n--- Test 3: Supported Record Types ---" << std::endl;
    auto supportedTypes = DNSLookupUtilityEngine::getSupportedRecordTypes();
    std::cout << "Supported record types: ";
    for (size_t i = 0; i < supportedTypes.size(); i++) {
        std::cout << supportedTypes[i];
        if (i < supportedTypes.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    
    // Test 4: JSON serialization
    std::cout << "\n--- Test 4: JSON Serialization ---" << std::endl;
    json configJson = DNSLookupUtilityEngine::configToJson(validConfig);
    std::cout << "Config to JSON: " << configJson.dump(2) << std::endl;
    
    auto configFromJson = DNSLookupUtilityEngine::configFromJson(configJson);
    std::cout << "JSON to Config validation: " << 
        (configFromJson.domain == validConfig.domain ? "PASS" : "FAIL") << std::endl;
    
    // Test 5: A record lookup
    std::cout << "\n--- Test 5: A Record Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig aConfig = validConfig;
    aConfig.domain = "google.com";
    aConfig.recordType = "A";
    
    auto aResult = engine.performDNSLookup(aConfig);
    printDNSResult(aResult);
    
    // Test 6: AAAA record lookup
    std::cout << "\n--- Test 6: AAAA Record Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig aaaaConfig = validConfig;
    aaaaConfig.domain = "google.com";
    aaaaConfig.recordType = "AAAA";
    
    auto aaaaResult = engine.performDNSLookup(aaaaConfig);
    printDNSResult(aaaaResult);
    
    // Test 7: MX record lookup
    std::cout << "\n--- Test 7: MX Record Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig mxConfig = validConfig;
    mxConfig.domain = "google.com";
    mxConfig.recordType = "MX";
    
    auto mxResult = engine.performDNSLookup(mxConfig);
    printDNSResult(mxResult);
    
    // Test 8: NS record lookup
    std::cout << "\n--- Test 8: NS Record Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig nsConfig = validConfig;
    nsConfig.domain = "google.com";
    nsConfig.recordType = "NS";
    
    auto nsResult = engine.performDNSLookup(nsConfig);
    printDNSResult(nsResult);
    
    // Test 9: TXT record lookup
    std::cout << "\n--- Test 9: TXT Record Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig txtConfig = validConfig;
    txtConfig.domain = "google.com";
    txtConfig.recordType = "TXT";
    
    auto txtResult = engine.performDNSLookup(txtConfig);
    printDNSResult(txtResult);
    
    // Test 10: CNAME record lookup
    std::cout << "\n--- Test 10: CNAME Record Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig cnameConfig = validConfig;
    cnameConfig.domain = "www.github.com";
    cnameConfig.recordType = "CNAME";
    
    auto cnameResult = engine.performDNSLookup(cnameConfig);
    printDNSResult(cnameResult);
    
    // Test 11: SOA record lookup
    std::cout << "\n--- Test 11: SOA Record Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig soaConfig = validConfig;
    soaConfig.domain = "google.com";
    soaConfig.recordType = "SOA";
    
    auto soaResult = engine.performDNSLookup(soaConfig);
    printDNSResult(soaResult);
    
    // Test 12: Custom DNS server lookup
    std::cout << "\n--- Test 12: Custom DNS Server Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig customDnsConfig = validConfig;
    customDnsConfig.domain = "google.com";
    customDnsConfig.recordType = "A";
    customDnsConfig.dnsServer = "8.8.8.8";
    
    auto customDnsResult = engine.performDNSLookup(customDnsConfig);
    printDNSResult(customDnsResult);
    
    // Test 13: Cloudflare DNS server lookup
    std::cout << "\n--- Test 13: Cloudflare DNS Server Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig cloudflareConfig = validConfig;
    cloudflareConfig.domain = "example.com";
    cloudflareConfig.recordType = "A";
    cloudflareConfig.dnsServer = "1.1.1.1";
    
    auto cloudflareResult = engine.performDNSLookup(cloudflareConfig);
    printDNSResult(cloudflareResult);
    
    // Test 14: IPv6 DNS lookup
    std::cout << "\n--- Test 14: IPv6 DNS Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig ipv6Config = validConfig;
    ipv6Config.domain = "google.com";
    ipv6Config.recordType = "A";
    ipv6Config.ipv6 = true;
    
    auto ipv6Result = engine.performDNSLookup(ipv6Config);
    printDNSResult(ipv6Result);
    
    // Test 15: Non-existent domain lookup
    std::cout << "\n--- Test 15: Non-existent Domain Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig nxdomainConfig = validConfig;
    nxdomainConfig.domain = "this-domain-should-not-exist-12345.com";
    nxdomainConfig.recordType = "A";
    
    auto nxdomainResult = engine.performDNSLookup(nxdomainConfig);
    printDNSResult(nxdomainResult);
    
    // Test 16: Trace lookup
    std::cout << "\n--- Test 16: Trace Lookup ---" << std::endl;
    
    DNSLookupUtilityEngine::DNSConfig traceConfig = validConfig;
    traceConfig.domain = "google.com";
    traceConfig.recordType = "A";
    traceConfig.trace = true;
    
    auto traceResult = engine.performDNSLookup(traceConfig);
    printDNSResult(traceResult);
    
    // Test 17: Performance test
    std::cout << "\n--- Test 17: Performance Test ---" << std::endl;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    std::vector<std::string> domains = {"google.com", "github.com", "stackoverflow.com", "cloudflare.com"};
    std::vector<std::string> recordTypes = {"A", "AAAA", "MX", "NS"};
    
    int successCount = 0;
    int totalTests = domains.size() * recordTypes.size();
    
    for (const auto& domain : domains) {
        for (const auto& recordType : recordTypes) {
            DNSLookupUtilityEngine::DNSConfig perfConfig = validConfig;
            perfConfig.domain = domain;
            perfConfig.recordType = recordType;
            perfConfig.timeout = 3;
            
            auto perfResult = engine.performDNSLookup(perfConfig);
            if (perfResult.success) {
                successCount++;
            }
            
            std::cout << "  " << domain << " " << recordType << ": " 
                      << (perfResult.success ? "OK" : "FAIL") << std::endl;
        }
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "Performance test completed: " << successCount << "/" << totalTests 
              << " successful in " << duration.count() << " ms" << std::endl;
    std::cout << "Average time per lookup: " << (duration.count() / totalTests) << " ms" << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "DNSLookupUtilityEngine Test Suite" << std::endl;
    std::cout << "==================================" << std::endl;
    
    try {
        testDNSLookupEngine();
        std::cout << "\nAll tests completed successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
