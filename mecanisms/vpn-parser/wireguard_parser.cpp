#include "wireguard_parser.hpp"
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>

bool WireGuardParser::isWireGuardConfig(const std::string& configContent) {
    std::cout << "[WIREGUARD-PARSER] Checking for WireGuard configuration..." << std::endl;
    
    // Essential WireGuard sections - must be present (case sensitive)
    bool hasInterface = configContent.find("[Interface]") != std::string::npos;
    bool hasPeer = configContent.find("[Peer]") != std::string::npos;
    
    if (!hasInterface || !hasPeer) {
        std::cout << "[WIREGUARD-PARSER] Missing required sections (Interface: " 
                  << hasInterface << ", Peer: " << hasPeer << ")" << std::endl;
        return false;
    }
    
    // Essential WireGuard keys (case sensitive)
    bool hasPrivateKey = configContent.find("PrivateKey") != std::string::npos;
    bool hasPublicKey = configContent.find("PublicKey") != std::string::npos;
    bool hasEndpoint = configContent.find("Endpoint") != std::string::npos;
    bool hasAllowedIPs = configContent.find("AllowedIPs") != std::string::npos;
    
    // WireGuard-specific configuration options
    std::vector<std::string> wireguardKeys = {
        "PrivateKey", "PublicKey", "Endpoint", "AllowedIPs", 
        "PersistentKeepalive", "ListenPort", "DNS", "Address", "MTU"
    };
    
    int keyMatches = 0;
    for (const auto& key : wireguardKeys) {
        if (configContent.find(key) != std::string::npos) {
            keyMatches++;
            std::cout << "[WIREGUARD-PARSER] Found WireGuard key: " << key << std::endl;
        }
    }
    
    // Check for comments that might indicate WireGuard
    bool hasWireGuardComments = (configContent.find("# Client settings") != std::string::npos ||
                                configContent.find("# Server settings") != std::string::npos ||
                                configContent.find("# WireGuard") != std::string::npos);
    
    // Exclude other protocols - be more specific
    std::string lowerContent = configContent;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
    
    bool hasOpenVPNPatterns = (lowerContent.find("client") != std::string::npos &&
                              lowerContent.find("remote ") != std::string::npos) ||
                             configContent.find("<ca>") != std::string::npos ||
                             lowerContent.find("dev tun") != std::string::npos;
    
    bool hasIKEv2Patterns = (lowerContent.find("conn ") != std::string::npos &&
                            lowerContent.find("keyexchange=ikev2") != std::string::npos) ||
                           lowerContent.find("config setup") != std::string::npos;
    
    if (hasOpenVPNPatterns || hasIKEv2Patterns) {
        std::cout << "[WIREGUARD-PARSER] Detected other protocol patterns, not WireGuard" << std::endl;
        return false;
    }
    
    // Must have the essential structure and keys
    bool isValid = hasInterface && hasPeer && hasPrivateKey && hasEndpoint && keyMatches >= 4;
    
    std::cout << "[WIREGUARD-PARSER] Validation result: " << (isValid ? "VALID" : "INVALID")
              << " (Interface: " << hasInterface << ", Peer: " << hasPeer
              << ", PrivateKey: " << hasPrivateKey << ", PublicKey: " << hasPublicKey 
              << ", Endpoint: " << hasEndpoint << ", AllowedIPs: " << hasAllowedIPs 
              << ", key matches: " << keyMatches << ")" << std::endl;
    
    return isValid;
}

json WireGuardParser::parseConfig(const std::string& configContent) {
    std::cout << "[WIREGUARD-PARSER] Starting WireGuard configuration parsing..." << std::endl;
    
    json result;

    // Get Interface and Peer sections
    std::string interfaceSection = getInterfaceSection(configContent);
    std::string peerSection = getPeerSection(configContent);
    
    if (interfaceSection.empty() || peerSection.empty()) {
        result["success"] = false;
        result["error"] = "Missing required [Interface] or [Peer] sections in WireGuard configuration";
        return result;
    }

    // Extract server information from Endpoint
    std::string endpoint = extractValue(peerSection, "Endpoint");
    if (endpoint.empty()) {
        result["success"] = false;
        result["error"] = "Missing required 'Endpoint' in [Peer] section";
        return result;
    }
    
    std::string serverAddress = extractServer(endpoint);
    int port = extractPort(endpoint);
    
    if (serverAddress.empty()) {
        result["success"] = false;
        result["error"] = "Invalid endpoint format in WireGuard configuration";
        return result;
    }

    // Generate profile name from comments or server
    std::string profileName = generateProfileName(configContent);
    if (profileName.empty()) {
        profileName = "WireGuard (" + serverAddress + ")";
    }

    // Extract Interface configuration
    std::string privateKey = extractValue(interfaceSection, "PrivateKey");
    std::string address = extractValue(interfaceSection, "Address");
    std::string dns = extractValue(interfaceSection, "DNS");
    std::string listenPort = extractValue(interfaceSection, "ListenPort");
    std::string mtu = extractValue(interfaceSection, "MTU");
    
    // Extract Peer configuration
    std::string publicKey = extractValue(peerSection, "PublicKey");
    std::string allowedIPs = extractValue(peerSection, "AllowedIPs");
    std::string presharedKey = extractValue(peerSection, "PresharedKey");
    std::string persistentKeepalive = extractValue(peerSection, "PersistentKeepalive");
    
    // Validate essential keys
    if (privateKey.empty() || privateKey.find("CLIENT_PRIVATE_KEY_HERE") != std::string::npos) {
        result["success"] = false;
        result["error"] = "Missing or placeholder 'PrivateKey' in [Interface] section";
        return result;
    }
    
    if (publicKey.empty() || publicKey.find("SERVER_PUBLIC_KEY_HERE") != std::string::npos) {
        result["success"] = false;
        result["error"] = "Missing or placeholder 'PublicKey' in [Peer] section";
        return result;
    }
    
    // Validate keys are not placeholders (common in example configs)
    bool hasValidKeys = privateKey != "CLIENT_PRIVATE_KEY_HERE" && 
                        publicKey != "SERVER_PUBLIC_KEY_HERE";
    
    if (!hasValidKeys) {
        std::cout << "[WIREGUARD-PARSER] Warning: Configuration contains placeholder keys" << std::endl;
        // Still allow parsing but mark as needing key configuration
    }
    
    // Extract WireGuard-specific data
    json wireguardData = {
        {"private_key_present", !privateKey.empty() && hasValidKeys},
        {"public_key_present", !publicKey.empty() && hasValidKeys},
        {"preshared_key_present", !presharedKey.empty()},
        {"interface_address", address.empty() ? "10.0.0.2/24" : address},
        {"listen_port", listenPort.empty() ? "51820" : listenPort},
        {"allowed_ips", allowedIPs.empty() ? "0.0.0.0/0, ::/0" : allowedIPs},
        {"persistent_keepalive", persistentKeepalive.empty() ? "25" : persistentKeepalive},
        {"dns_servers", dns.empty() ? "1.1.1.1, 8.8.8.8" : dns},
        {"mtu", mtu.empty() ? "1420" : mtu},
        {"has_placeholder_keys", !hasValidKeys},
        {"tunnel_all_traffic", allowedIPs.find("0.0.0.0/0") != std::string::npos}
    };

    // Determine authentication method based on keys
    std::string authMethod = "Public/Private Key";
    if (!presharedKey.empty()) {
        authMethod = "Public/Private Key + Pre-shared Key";
    }

    result["success"] = true;
    result["protocol_detected"] = "WireGuard";
    result["parser_used"] = "WireGuard";
    result["profile_data"] = {
        {"name", profileName},
        {"server", serverAddress},
        {"protocol", "WireGuard"},
        {"port", port},
        {"username", ""}, // WireGuard doesn't use traditional username/password
        {"password", ""}, // WireGuard uses key-based authentication
        {"auth_method", authMethod},
        {"encryption", "ChaCha20Poly1305"},
        {"compression", false}, // WireGuard doesn't use compression
        {"wireguard_specific", wireguardData}
    };

    std::cout << "[WIREGUARD-PARSER] Successfully parsed WireGuard configuration for server: " << serverAddress << std::endl;
    if (!hasValidKeys) {
        std::cout << "[WIREGUARD-PARSER] Note: Configuration contains placeholder keys that need to be replaced" << std::endl;
    }
    
    return result;
}

std::string WireGuardParser::extractValue(const std::string& section, const std::string& key) {
    std::regex valueRegex(key + R"(\s*=\s*([^\n\r]+))");
    std::smatch match;

    if (std::regex_search(section, match, valueRegex)) {
        std::string value = match[1].str();
        // Trim whitespace
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        return value;
    }

    return "";
}

std::string WireGuardParser::getInterfaceSection(const std::string& configContent) {
    std::regex interfaceRegex(R"(\[Interface\]([\s\S]*?)(?=\[|$))");
    std::smatch match;

    if (std::regex_search(configContent, match, interfaceRegex)) {
        return match[1].str();
    }

    return "";
}

std::string WireGuardParser::getPeerSection(const std::string& configContent) {
    std::regex peerRegex(R"(\[Peer\]([\s\S]*?)(?=\[|$))");
    std::smatch match;

    if (std::regex_search(configContent, match, peerRegex)) {
        return match[1].str();
    }

    return "";
}

std::string WireGuardParser::extractServer(const std::string& endpoint) {
    if (endpoint.empty()) return "";

    // Handle IPv6 endpoints [::1]:51820
    if (endpoint[0] == '[') {
        size_t closeBracket = endpoint.find(']');
        if (closeBracket != std::string::npos) {
            return endpoint.substr(1, closeBracket - 1);
        }
    }

    // Handle IPv4 endpoints server.com:51820 or 1.2.3.4:51820
    size_t colonPos = endpoint.find(':');
    if (colonPos != std::string::npos) {
        return endpoint.substr(0, colonPos);
    }

    return endpoint;
}

int WireGuardParser::extractPort(const std::string& endpoint) {
    if (endpoint.empty()) return 51820; // Default WireGuard port

    // Handle IPv6 endpoints [::1]:51820
    if (endpoint[0] == '[') {
        size_t closeBracket = endpoint.find(']');
        if (closeBracket != std::string::npos && closeBracket + 1 < endpoint.size() && endpoint[closeBracket + 1] == ':') {
            std::string portStr = endpoint.substr(closeBracket + 2);
            try {
                return std::stoi(portStr);
            } catch (...) {
                return 51820;
            }
        }
    }

    // Handle IPv4 endpoints
    size_t colonPos = endpoint.rfind(':');
    if (colonPos != std::string::npos) {
        std::string portStr = endpoint.substr(colonPos + 1);
        try {
            return std::stoi(portStr);
        } catch (...) {
            return 51820;
        }
    }

    return 51820;
}

std::string WireGuardParser::generateProfileName(const std::string& content) {
    // Look for comments that might indicate the profile name
    std::regex commentRegex(R"(#\s*([^#\n]+))");
    std::smatch match;

    if (std::regex_search(content, match, commentRegex)) {
        std::string comment = match[1].str();
        comment.erase(0, comment.find_first_not_of(" \t"));
        comment.erase(comment.find_last_not_of(" \t") + 1);
        if (!comment.empty() && comment.size() < 50) {
            return comment;
        }
    }

    // Try to extract server name from endpoint
    std::string peerSection = getPeerSection(content);
    std::string endpoint = extractValue(peerSection, "Endpoint");
    std::string server = extractServer(endpoint);

    if (!server.empty()) {
        return "WireGuard - " + server;
    }

    return "WireGuard Profile";
}