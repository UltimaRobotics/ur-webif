
#ifndef DNS_LOOKUP_UTILITY_ENGINE_H
#define DNS_LOOKUP_UTILITY_ENGINE_H

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

class DNSLookupUtilityEngine {
public:
    struct DNSConfig {
        std::string domain = "example.com";
        std::string recordType = "A";
        std::string dnsServer = ""; // Empty for system default
        int timeout = 5;
        bool trace = false;
        bool recursive = true;
        bool showStats = true;
        bool ipv6 = false;
    };
    
    struct DNSRecord {
        std::string name;
        std::string type;
        std::string value;
        int ttl = 0;
        std::string recordClass = "IN";
        int priority = 0; // For MX records
        std::string additional; // Additional data
    };
    
    struct DNSResult {
        bool success = false;
        std::string error;
        
        // Query info
        std::string domain;
        std::string recordType;
        std::string dnsServer;
        std::string resolvedServer;
        
        // Results
        std::vector<DNSRecord> records;
        
        // Statistics
        double queryTime = 0.0;
        int responseSize = 0;
        bool authoritative = false;
        bool recursionAvailable = false;
        bool truncated = false;
        
        // Additional sections
        std::vector<DNSRecord> authorityRecords;
        std::vector<DNSRecord> additionalRecords;
        
        // Raw response
        json rawResponse;
        std::string rawOutput;
        
        // Test metadata
        std::string timestamp;
    };
    
    DNSLookupUtilityEngine();
    ~DNSLookupUtilityEngine();
    
    // Synchronous lookup (most DNS queries are quick)
    DNSResult performDNSLookup(const DNSConfig& config);
    
    // Utility functions
    static bool validateConfig(const DNSConfig& config, std::string& error);
    static json configToJson(const DNSConfig& config);
    static DNSConfig configFromJson(const json& j);
    static std::vector<std::string> getSupportedRecordTypes();

private:
    mutable std::mutex engineMutex_;
    
    // Core functionality
    DNSResult performDigLookup(const DNSConfig& config);
    DNSResult performNslookupLookup(const DNSConfig& config);
    std::string buildDigCommand(const DNSConfig& config) const;
    std::string buildNslookupCommand(const DNSConfig& config) const;
    
    // Parsing functions
    bool parseDigOutput(const std::string& output, DNSResult& result) const;
    bool parseDigJSON(const std::string& jsonOutput, DNSResult& result) const;
    bool parseNslookupOutput(const std::string& output, DNSResult& result) const;
    DNSRecord parseDigRecord(const std::string& line) const;
    DNSRecord parseNslookupRecord(const std::string& line, const std::string& recordType) const;
    
    // Validation and utilities
    bool isValidDomain(const std::string& domain) const;
    bool isValidRecordType(const std::string& recordType) const;
    bool isValidDNSServer(const std::string& server) const;
    std::string sanitizeDomain(const std::string& domain) const;
    std::string sanitizeDNSServer(const std::string& server) const;
    
    // Command execution
    std::string executeCommand(const std::string& command, int timeoutSeconds = 30) const;
    
    // Record type mapping
    static const std::vector<std::string> supportedRecordTypes_;
};

#endif // DNS_LOOKUP_UTILITY_ENGINE_H
