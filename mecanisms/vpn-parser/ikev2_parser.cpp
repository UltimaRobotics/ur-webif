
#include "ikev2_parser.hpp"
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>

bool IKEv2Parser::isIKEv2Config(const std::string& configContent) {
    std::cout << "[IKEV2-PARSER] Checking for IKEv2 configuration..." << std::endl;
    
    std::string lowerContent = configContent;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
    
    // Check for strongSwan/Libreswan configuration structure
    bool hasConfigSetup = lowerContent.find("config setup") != std::string::npos;
    bool hasConnSection = lowerContent.find("conn ") != std::string::npos;
    bool hasIKEv2Exchange = lowerContent.find("keyexchange=ikev2") != std::string::npos;
    
    // Essential IKEv2 patterns
    bool hasLeftRight = (lowerContent.find("left=") != std::string::npos || 
                        lowerContent.find("right=") != std::string::npos);
    
    // IKEv2 specific configuration options
    std::vector<std::string> ikev2Keywords = {
        "keyexchange=ikev2", "ike=", "esp=", "authby=", "leftauth=", "rightauth=",
        "leftid=", "rightid=", "leftsourceip=", "rightsubnet=", "auto=add", "auto=start",
        "rekey=", "reauth=", "closeaction=", "fragmentation=", "forceencaps=",
        "charondebug=", "leftcert=", "rightcert=", "type=tunnel"
    };
    
    int ikev2Matches = 0;
    for (const auto& keyword : ikev2Keywords) {
        if (lowerContent.find(keyword) != std::string::npos) {
            ikev2Matches++;
            std::cout << "[IKEV2-PARSER] Found IKEv2 keyword: " << keyword << std::endl;
        }
    }
    
    // Exclude other protocols
    bool hasOpenVPNPatterns = (lowerContent.find("client") != std::string::npos &&
                              lowerContent.find("remote ") != std::string::npos) ||
                             lowerContent.find("<ca>") != std::string::npos ||
                             lowerContent.find("dev tun") != std::string::npos;
    
    bool hasWireGuardPatterns = (lowerContent.find("[interface]") != std::string::npos &&
                                lowerContent.find("[peer]") != std::string::npos) ||
                               lowerContent.find("privatekey") != std::string::npos;
    
    if (hasOpenVPNPatterns || hasWireGuardPatterns) {
        std::cout << "[IKEV2-PARSER] Detected other protocol patterns, not IKEv2" << std::endl;
        return false;
    }
    
    // Valid if we have conn section with IKEv2 exchange or sufficient IKEv2 keywords
    bool isValid = hasConnSection && 
                   (hasIKEv2Exchange || ikev2Matches >= 4 || 
                    (hasConfigSetup && ikev2Matches >= 2));
    
    std::cout << "[IKEV2-PARSER] Validation result: " << (isValid ? "VALID" : "INVALID")
              << " (config setup: " << hasConfigSetup << ", conn: " << hasConnSection 
              << ", ikev2 exchange: " << hasIKEv2Exchange << ", ikev2 matches: " << ikev2Matches << ")" << std::endl;
    
    return isValid;
}

json IKEv2Parser::parseConfig(const std::string& configContent) {
    std::cout << "[IKEV2-PARSER] Starting IKEv2 configuration parsing..." << std::endl;
    
    json result;
    
    // Extract connection name from conn section
    std::string profileName = extractProfileName(configContent);
    
    // Extract server information
    std::string serverAddress = extractServer(configContent);
    int port = extractPort(configContent);
    
    if (serverAddress.empty()) {
        result["success"] = false;
        result["error"] = "Missing or invalid server address in IKEv2 configuration";
        return result;
    }
    
    if (profileName.empty()) {
        profileName = "IKEv2 (" + serverAddress + ")";
    }
    
    // Extract identity information
    std::string leftId = extractValue(configContent, "leftid");
    std::string rightId = extractValue(configContent, "rightid");
    std::string username = leftId; // leftid is typically the client username
    
    // Determine authentication method
    std::string authMethod = "PSK"; // Default for this example
    std::string leftAuth = extractValue(configContent, "leftauth");
    std::string rightAuth = extractValue(configContent, "rightauth");
    std::string authBy = extractValue(configContent, "authby");
    
    if (leftAuth == "psk" || rightAuth == "psk" || authBy == "secret") {
        authMethod = "PSK (Pre-Shared Key)";
    } else if (leftAuth.find("cert") != std::string::npos || rightAuth.find("cert") != std::string::npos) {
        authMethod = "Certificate";
    } else if (leftAuth == "eap" || rightAuth == "eap") {
        authMethod = "EAP";
    }
    
    // Extract IKE and ESP parameters
    std::string ikeParams = extractValue(configContent, "ike");
    std::string espParams = extractValue(configContent, "esp");
    
    // Extract connection settings
    std::string connectionType = extractValue(configContent, "type");
    std::string autoSetting = extractValue(configContent, "auto");
    std::string leftSourceIp = extractValue(configContent, "leftsourceip");
    std::string rightSubnet = extractValue(configContent, "rightsubnet");
    
    // Extract advanced settings
    bool fragmentation = extractValue(configContent, "fragmentation") == "yes";
    bool forceEncaps = extractValue(configContent, "forceencaps") == "yes";
    bool rekey = extractValue(configContent, "rekey") == "yes";
    bool reauth = extractValue(configContent, "reauth") == "yes";
    std::string closeAction = extractValue(configContent, "closeaction");
    
    // Determine encryption from IKE parameters
    std::string encryption = "AES-256";
    if (!ikeParams.empty()) {
        if (ikeParams.find("aes128") != std::string::npos) {
            encryption = "AES-128";
        } else if (ikeParams.find("aes256") != std::string::npos) {
            encryption = "AES-256";
        } else if (ikeParams.find("3des") != std::string::npos) {
            encryption = "3DES";
        }
    }
    
    // Extract debug settings if present
    std::string charonDebug = "";
    std::regex debugRegex("charondebug\\s*=\\s*\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(configContent, match, debugRegex)) {
        charonDebug = match[1].str();
    }
    
    // Extract IKEv2-specific data
    json ikev2Data = {
        {"ike_parameters", ikeParams.empty() ? "aes256-sha256-modp2048" : ikeParams},
        {"esp_parameters", espParams.empty() ? "aes256-sha256" : espParams},
        {"connection_type", connectionType.empty() ? "tunnel" : connectionType},
        {"auto_setting", autoSetting.empty() ? "add" : autoSetting},
        {"left_id", leftId},
        {"right_id", rightId},
        {"left_source_ip", leftSourceIp},
        {"right_subnet", rightSubnet},
        {"fragmentation", fragmentation},
        {"force_encaps", forceEncaps},
        {"rekey", rekey},
        {"reauth", reauth},
        {"close_action", closeAction},
        {"charon_debug", charonDebug},
        {"has_certificates", authMethod.find("Certificate") != std::string::npos},
        {"supports_mobility", reauth && rekey}
    };
    
    result["success"] = true;
    result["protocol_detected"] = "IKEv2";
    result["parser_used"] = "IKEv2";
    result["profile_data"] = {
        {"name", profileName},
        {"server", serverAddress},
        {"protocol", "IKEv2/IPSec"},
        {"port", port},
        {"username", username},
        {"password", ""}, // Password not stored in config files for security
        {"auth_method", authMethod},
        {"encryption", encryption},
        {"compression", false}, // IKEv2 typically doesn't use compression at VPN level
        {"ikev2_specific", ikev2Data}
    };
    
    std::cout << "[IKEV2-PARSER] Successfully parsed IKEv2 configuration for server: " << serverAddress << std::endl;
    return result;
}

std::string IKEv2Parser::extractValue(const std::string& content, const std::string& key) {
    std::regex valueRegex(key + R"(\s*=\s*([^\s\n]+))");
    std::smatch match;
    
    if (std::regex_search(content, match, valueRegex)) {
        return match[1].str();
    }
    
    return "";
}

std::string IKEv2Parser::extractServer(const std::string& content) {
    // Try different server identification patterns
    std::string server = extractValue(content, "right");
    if (server.empty()) {
        server = extractValue(content, "rightid");
    }
    if (server.empty()) {
        server = extractValue(content, "server");
    }
    
    // Clean up server address (remove quotes, etc.)
    if (!server.empty() && server.front() == '"' && server.back() == '"') {
        server = server.substr(1, server.length() - 2);
    }
    
    return server;
}

int IKEv2Parser::extractPort(const std::string& content) {
    std::string portStr = extractValue(content, "port");
    if (!portStr.empty()) {
        try {
            return std::stoi(portStr);
        } catch (...) {
            // Fall through to default
        }
    }
    
    // Check for custom ports in server specification
    std::regex portRegex(R"(:(\d+))");
    std::smatch match;
    if (std::regex_search(content, match, portRegex)) {
        try {
            return std::stoi(match[1].str());
        } catch (...) {
            // Fall through to default
        }
    }
    
    return 500; // Default IKEv2 port
}

std::string IKEv2Parser::extractUsername(const std::string& content) {
    // Try different username identification patterns
    std::string username = extractValue(content, "leftid");
    if (username.empty()) {
        username = extractValue(content, "username");
    }
    if (username.empty()) {
        username = extractValue(content, "xauth_username");
    }
    
    // Clean up username (remove quotes, etc.)
    if (!username.empty() && username.front() == '"' && username.back() == '"') {
        username = username.substr(1, username.length() - 2);
    }
    
    return username;
}

std::string IKEv2Parser::extractProfileName(const std::string& content) {
    // Look for connection name
    std::regex connRegex(R"(conn\s+([^\s\n]+))");
    std::smatch match;
    
    if (std::regex_search(content, match, connRegex)) {
        std::string connName = match[1].str();
        if (connName != "%default") {
            return connName;
        }
    }
    
    // Look for comments with profile names
    std::regex commentRegex(R"(#\s*([^#\n]+))");
    if (std::regex_search(content, match, commentRegex)) {
        std::string comment = match[1].str();
        // Clean up the comment
        comment.erase(0, comment.find_first_not_of(" \t"));
        comment.erase(comment.find_last_not_of(" \t") + 1);
        if (!comment.empty() && comment.size() < 50) { // Reasonable profile name length
            return comment;
        }
    }
    
    return "";
}
