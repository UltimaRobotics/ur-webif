
#include "DNSLookupUtilityEngine.hpp"
#include "endpoint_logger.h"
#include <sstream>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <iomanip>
#include <algorithm>

const std::vector<std::string> DNSLookupUtilityEngine::supportedRecordTypes_ = {
    "A", "AAAA", "CNAME", "MX", "NS", "TXT", "SOA", "PTR", "SRV", "CAA", "DNSKEY", "DS", "RRSIG", "NSEC", "ANY"
};

DNSLookupUtilityEngine::DNSLookupUtilityEngine() {
    ENDPOINT_LOG("dns-engine", "DNSLookupUtilityEngine initialized");
}

DNSLookupUtilityEngine::~DNSLookupUtilityEngine() {
    ENDPOINT_LOG("dns-engine", "DNSLookupUtilityEngine destroyed");
}

DNSLookupUtilityEngine::DNSResult DNSLookupUtilityEngine::performDNSLookup(const DNSConfig& config) {
    std::lock_guard<std::mutex> lock(engineMutex_);
    
    std::string validationError;
    if (!validateConfig(config, validationError)) {
        ENDPOINT_LOG("dns-engine", "Invalid config: " + validationError);
        DNSResult result;
        result.success = false;
        result.error = validationError;
        return result;
    }
    
    ENDPOINT_LOG("dns-engine", "Performing DNS lookup for " + config.domain + " (" + config.recordType + ")");
    
    // Try dig first (preferred), fall back to nslookup
    DNSResult result = performDigLookup(config);
    if (!result.success && result.error.find("dig") != std::string::npos) {
        ENDPOINT_LOG("dns-engine", "dig failed, trying nslookup");
        result = performNslookupLookup(config);
    }
    
    result.timestamp = std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    return result;
}

DNSLookupUtilityEngine::DNSResult DNSLookupUtilityEngine::performDigLookup(const DNSConfig& config) {
    DNSResult result;
    result.domain = config.domain;
    result.recordType = config.recordType;
    result.dnsServer = config.dnsServer.empty() ? "System Default" : config.dnsServer;
    
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        // Try JSON output first
        std::string jsonCommand = buildDigCommand(config) + " +json";
        std::string jsonOutput = executeCommand(jsonCommand, config.timeout + 5);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.queryTime = duration.count() / 1000.0; // Convert to milliseconds
        
        // Try parsing JSON output
        if (!jsonOutput.empty() && jsonOutput.find("{") != std::string::npos) {
            if (parseDigJSON(jsonOutput, result)) {
                result.success = true;
                return result;
            }
        }
        
        // Fall back to standard dig output
        std::string command = buildDigCommand(config);
        std::string output = executeCommand(command, config.timeout + 5);
        
        if (output.empty()) {
            result.error = "No output from dig command";
            return result;
        }
        
        result.rawOutput = output;
        
        if (parseDigOutput(output, result)) {
            result.success = true;
        } else {
            result.error = "Failed to parse dig output";
        }
        
    } catch (const std::exception& e) {
        result.error = "Exception during dig lookup: " + std::string(e.what());
        ENDPOINT_LOG("dns-engine", result.error);
    }
    
    return result;
}

DNSLookupUtilityEngine::DNSResult DNSLookupUtilityEngine::performNslookupLookup(const DNSConfig& config) {
    DNSResult result;
    result.domain = config.domain;
    result.recordType = config.recordType;
    result.dnsServer = config.dnsServer.empty() ? "System Default" : config.dnsServer;
    
    try {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        std::string command = buildNslookupCommand(config);
        std::string output = executeCommand(command, config.timeout + 5);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        result.queryTime = duration.count() / 1000.0; // Convert to milliseconds
        
        if (output.empty()) {
            result.error = "No output from nslookup command";
            return result;
        }
        
        result.rawOutput = output;
        
        if (parseNslookupOutput(output, result)) {
            result.success = true;
        } else {
            result.error = "Failed to parse nslookup output";
        }
        
    } catch (const std::exception& e) {
        result.error = "Exception during nslookup: " + std::string(e.what());
        ENDPOINT_LOG("dns-engine", result.error);
    }
    
    return result;
}

std::string DNSLookupUtilityEngine::buildDigCommand(const DNSConfig& config) const {
    std::ostringstream cmd;
    
    cmd << "dig";
    
    if (!config.dnsServer.empty() && config.dnsServer != "System Default") {
        cmd << " @" << sanitizeDNSServer(config.dnsServer);
    }
    
    cmd << " " << sanitizeDomain(config.domain);
    cmd << " " << config.recordType;
    
    cmd << " +time=" << config.timeout;
    
    if (config.trace) {
        cmd << " +trace";
    }
    
    if (!config.recursive) {
        cmd << " +norecurse";
    }
    
    if (config.showStats) {
        cmd << " +stats";
    }
    
    if (config.ipv6) {
        cmd << " -6";
    }
    
    cmd << " 2>&1";
    
    return cmd.str();
}

std::string DNSLookupUtilityEngine::buildNslookupCommand(const DNSConfig& config) const {
    std::ostringstream cmd;
    
    cmd << "nslookup";
    cmd << " -type=" << config.recordType;
    cmd << " -timeout=" << config.timeout;
    
    if (!config.recursive) {
        cmd << " -norecurse";
    }
    
    cmd << " " << sanitizeDomain(config.domain);
    
    if (!config.dnsServer.empty() && config.dnsServer != "System Default") {
        cmd << " " << sanitizeDNSServer(config.dnsServer);
    }
    
    cmd << " 2>&1";
    
    return cmd.str();
}

bool DNSLookupUtilityEngine::parseDigJSON(const std::string& jsonOutput, DNSResult& result) const {
    try {
        // Find the JSON part in the output
        size_t jsonStart = jsonOutput.find('{');
        if (jsonStart == std::string::npos) {
            return false;
        }
        
        std::string jsonPart = jsonOutput.substr(jsonStart);
        json digJson = json::parse(jsonPart);
        
        result.rawResponse = digJson;
        
        // Extract query information
        if (digJson.contains("question") && digJson["question"].is_array() && !digJson["question"].empty()) {
            auto question = digJson["question"][0];
            if (question.contains("name")) {
                result.domain = question["name"];
            }
        }
        
        // Extract answer records
        if (digJson.contains("answer") && digJson["answer"].is_array()) {
            for (const auto& record : digJson["answer"]) {
                DNSRecord dnsRecord;
                
                if (record.contains("name")) dnsRecord.name = record["name"];
                if (record.contains("type")) dnsRecord.type = record["type"];
                if (record.contains("data")) dnsRecord.value = record["data"];
                if (record.contains("TTL")) dnsRecord.ttl = record["TTL"];
                
                result.records.push_back(dnsRecord);
            }
        }
        
        // Extract statistics
        if (digJson.contains("stats")) {
            auto stats = digJson["stats"];
            if (stats.contains("queryTime")) {
                result.queryTime = stats["queryTime"];
            }
            if (stats.contains("server")) {
                result.resolvedServer = stats["server"];
            }
        }
        
        // Extract flags
        if (digJson.contains("status")) {
            auto status = digJson["status"];
            if (status.contains("flags")) {
                auto flags = status["flags"];
                if (flags.contains("AA")) result.authoritative = flags["AA"];
                if (flags.contains("RA")) result.recursionAvailable = flags["RA"];
                if (flags.contains("TC")) result.truncated = flags["TC"];
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        ENDPOINT_LOG("dns-engine", "JSON parse error: " + std::string(e.what()));
        return false;
    }
}

bool DNSLookupUtilityEngine::parseDigOutput(const std::string& output, DNSResult& result) const {
    std::istringstream iss(output);
    std::string line;
    bool inAnswerSection = false;
    bool inAuthoritySection = false;
    bool inAdditionalSection = false;
    
    while (std::getline(iss, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        // Section headers
        if (line.find("ANSWER SECTION:") != std::string::npos) {
            inAnswerSection = true;
            inAuthoritySection = false;
            inAdditionalSection = false;
            continue;
        } else if (line.find("AUTHORITY SECTION:") != std::string::npos) {
            inAnswerSection = false;
            inAuthoritySection = true;
            inAdditionalSection = false;
            continue;
        } else if (line.find("ADDITIONAL SECTION:") != std::string::npos) {
            inAnswerSection = false;
            inAuthoritySection = false;
            inAdditionalSection = true;
            continue;
        }
        
        // Parse query time
        if (line.find("Query time:") != std::string::npos) {
            std::regex queryTimeRegex(R"(Query time: (\d+) msec)");
            std::smatch match;
            if (std::regex_search(line, match, queryTimeRegex)) {
                result.queryTime = std::stod(match[1].str());
            }
        }
        
        // Parse server info
        if (line.find("SERVER:") != std::string::npos) {
            std::regex serverRegex(R"(SERVER: ([^#]+))");
            std::smatch match;
            if (std::regex_search(line, match, serverRegex)) {
                result.resolvedServer = match[1].str();
            }
        }
        
        // Parse flags
        if (line.find("flags:") != std::string::npos) {
            if (line.find(" aa ") != std::string::npos) result.authoritative = true;
            if (line.find(" ra ") != std::string::npos) result.recursionAvailable = true;
            if (line.find(" tc ") != std::string::npos) result.truncated = true;
        }
        
        // Parse records
        if (inAnswerSection && line[0] != ';' && !line.empty()) {
            DNSRecord record = parseDigRecord(line);
            if (!record.name.empty()) {
                result.records.push_back(record);
            }
        } else if (inAuthoritySection && line[0] != ';' && !line.empty()) {
            DNSRecord record = parseDigRecord(line);
            if (!record.name.empty()) {
                result.authorityRecords.push_back(record);
            }
        } else if (inAdditionalSection && line[0] != ';' && !line.empty()) {
            DNSRecord record = parseDigRecord(line);
            if (!record.name.empty()) {
                result.additionalRecords.push_back(record);
            }
        }
    }
    
    return !result.records.empty() || !result.authorityRecords.empty();
}

DNSLookupUtilityEngine::DNSRecord DNSLookupUtilityEngine::parseDigRecord(const std::string& line) const {
    DNSRecord record;
    
    // Standard format: name TTL class type data
    std::regex recordRegex(R"(^([^\s]+)\s+(\d+)\s+(IN|CH|HS)\s+([A-Z]+)\s+(.+)$)");
    std::smatch match;
    
    if (std::regex_search(line, match, recordRegex)) {
        record.name = match[1].str();
        record.ttl = std::stoi(match[2].str());
        record.recordClass = match[3].str();
        record.type = match[4].str();
        record.value = match[5].str();
        
        // Handle MX records (extract priority)
        if (record.type == "MX") {
            std::regex mxRegex(R"(^(\d+)\s+(.+)$)");
            std::smatch mxMatch;
            if (std::regex_search(record.value, mxMatch, mxRegex)) {
                record.priority = std::stoi(mxMatch[1].str());
                record.value = mxMatch[2].str();
            }
        }
    }
    
    return record;
}

bool DNSLookupUtilityEngine::parseNslookupOutput(const std::string& output, DNSResult& result) const {
    std::istringstream iss(output);
    std::string line;
    bool inAnswerSection = false;
    
    while (std::getline(iss, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        // Check for authoritative answer
        if (line.find("Authoritative answers can be found from:") != std::string::npos) {
            inAnswerSection = false;
            continue;
        }
        
        // Check for non-authoritative answer
        if (line.find("Non-authoritative answer:") != std::string::npos) {
            inAnswerSection = true;
            continue;
        }
        
        // Parse server info
        if (line.find("Server:") != std::string::npos) {
            std::regex serverRegex(R"(Server:\s+([^\s]+))");
            std::smatch match;
            if (std::regex_search(line, match, serverRegex)) {
                result.resolvedServer = match[1].str();
            }
        }
        
        // Parse records in answer section
        if (inAnswerSection && line.find(result.domain) != std::string::npos) {
            DNSRecord record = parseNslookupRecord(line, result.recordType);
            if (!record.value.empty()) {
                result.records.push_back(record);
            }
        }
        
        // Direct record formats
        if (line.find("Address:") != std::string::npos) {
            std::regex addrRegex(R"(Address:\s+([^\s]+))");
            std::smatch match;
            if (std::regex_search(line, match, addrRegex)) {
                DNSRecord record;
                record.name = result.domain;
                record.type = "A";
                record.value = match[1].str();
                result.records.push_back(record);
            }
        }
    }
    
    return !result.records.empty();
}

DNSLookupUtilityEngine::DNSRecord DNSLookupUtilityEngine::parseNslookupRecord(const std::string& line, const std::string& recordType) const {
    DNSRecord record;
    record.type = recordType;
    
    // Different patterns for different record types
    if (recordType == "A" || recordType == "AAAA") {
        std::regex ipRegex(R"(Address:\s+([^\s]+))");
        std::smatch match;
        if (std::regex_search(line, match, ipRegex)) {
            record.value = match[1].str();
        }
    } else if (recordType == "MX") {
        std::regex mxRegex(R"(mail exchanger = (\d+)\s+([^\s]+))");
        std::smatch match;
        if (std::regex_search(line, match, mxRegex)) {
            record.priority = std::stoi(match[1].str());
            record.value = match[2].str();
        }
    } else if (recordType == "CNAME") {
        std::regex cnameRegex(R"(canonical name = ([^\s]+))");
        std::smatch match;
        if (std::regex_search(line, match, cnameRegex)) {
            record.value = match[1].str();
        }
    } else if (recordType == "TXT") {
        std::regex txtRegex(R"(text = \"([^\"]*)\")");
        std::smatch match;
        if (std::regex_search(line, match, txtRegex)) {
            record.value = match[1].str();
        }
    }
    
    return record;
}

std::string DNSLookupUtilityEngine::executeCommand(const std::string& command, int timeoutSeconds) const {
    std::string result;
    
    std::string timeoutCommand = "timeout " + std::to_string(timeoutSeconds) + " " + command;
    FILE* pipe = popen(timeoutCommand.c_str(), "r");
    
    if (!pipe) {
        ENDPOINT_LOG("dns-engine", "Failed to execute command: " + command);
        return "";
    }
    
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    if (status == -1 || (WIFEXITED(status) && WEXITSTATUS(status) != 0)) {
        ENDPOINT_LOG("dns-engine", "Command failed: " + command + " (exit code: " + std::to_string(WEXITSTATUS(status)) + ")");
    }
    
    return result;
}

bool DNSLookupUtilityEngine::validateConfig(const DNSConfig& config, std::string& error) {
    if (config.domain.empty()) {
        error = "Domain cannot be empty";
        return false;
    }
    
    // Create a temporary instance to use the validation methods
    DNSLookupUtilityEngine temp;
    
    if (!temp.isValidDomain(config.domain)) {
        error = "Invalid domain format";
        return false;
    }
    
    if (!temp.isValidRecordType(config.recordType)) {
        error = "Unsupported record type: " + config.recordType;
        return false;
    }
    
    if (!config.dnsServer.empty() && !temp.isValidDNSServer(config.dnsServer)) {
        error = "Invalid DNS server format";
        return false;
    }
    
    if (config.timeout < 1 || config.timeout > 60) {
        error = "Timeout must be between 1 and 60 seconds";
        return false;
    }
    
    return true;
}

bool DNSLookupUtilityEngine::isValidDomain(const std::string& domain) const {
    if (domain.empty() || domain.length() > 253) {
        return false;
    }
    
    // Basic validation - no dangerous characters
    for (char c : domain) {
        if (c == ';' || c == '|' || c == '&' || c == '`' || 
            c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r') {
            return false;
        }
    }
    
    return true;
}

bool DNSLookupUtilityEngine::isValidRecordType(const std::string& recordType) const {
    return std::find(supportedRecordTypes_.begin(), supportedRecordTypes_.end(), recordType) != supportedRecordTypes_.end();
}

bool DNSLookupUtilityEngine::isValidDNSServer(const std::string& server) const {
    if (server.empty() || server.length() > 253) {
        return false;
    }
    
    // Basic validation - no dangerous characters
    for (char c : server) {
        if (c == ';' || c == '|' || c == '&' || c == '`' || 
            c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r') {
            return false;
        }
    }
    
    return true;
}

std::string DNSLookupUtilityEngine::sanitizeDomain(const std::string& domain) const {
    std::string sanitized = domain;
    
    // Remove dangerous characters
    sanitized.erase(std::remove_if(sanitized.begin(), sanitized.end(),
        [](char c) {
            return c == ';' || c == '|' || c == '&' || c == '`' || 
                   c == '$' || c == '\'' || c == '"' || c == '\n' || c == '\r';
        }), sanitized.end());
    
    return sanitized;
}

std::string DNSLookupUtilityEngine::sanitizeDNSServer(const std::string& server) const {
    return sanitizeDomain(server); // Same sanitization rules
}

json DNSLookupUtilityEngine::configToJson(const DNSConfig& config) {
    return json{
        {"domain", config.domain},
        {"recordType", config.recordType},
        {"dnsServer", config.dnsServer},
        {"timeout", config.timeout},
        {"trace", config.trace},
        {"recursive", config.recursive},
        {"showStats", config.showStats},
        {"ipv6", config.ipv6}
    };
}

DNSLookupUtilityEngine::DNSConfig DNSLookupUtilityEngine::configFromJson(const json& j) {
    DNSConfig config;
    
    if (j.contains("domain")) config.domain = j["domain"];
    if (j.contains("recordType")) config.recordType = j["recordType"];
    if (j.contains("dnsServer")) config.dnsServer = j["dnsServer"];
    if (j.contains("timeout")) config.timeout = j["timeout"];
    if (j.contains("trace")) config.trace = j["trace"];
    if (j.contains("recursive")) config.recursive = j["recursive"];
    if (j.contains("showStats")) config.showStats = j["showStats"];
    if (j.contains("ipv6")) config.ipv6 = j["ipv6"];
    
    return config;
}

std::vector<std::string> DNSLookupUtilityEngine::getSupportedRecordTypes() {
    return supportedRecordTypes_;
}
