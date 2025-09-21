
#include "openvpn_parser.hpp"
#include <sstream>
#include <regex>
#include <algorithm>
#include <iostream>

bool OpenVPNParser::isOpenVPNConfig(const std::string& configContent) {
    std::cout << "[OPENVPN-PARSER] Checking for OpenVPN configuration..." << std::endl;
    
    std::string lowerContent = configContent;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(), ::tolower);
    
    // Essential OpenVPN directives - must have remote and client/server mode
    bool hasRemote = lowerContent.find("remote ") != std::string::npos;
    bool hasClient = lowerContent.find("client") != std::string::npos;
    bool hasServer = lowerContent.find("server ") != std::string::npos;
    
    if (!hasRemote || (!hasClient && !hasServer)) {
        std::cout << "[OPENVPN-PARSER] Missing essential OpenVPN directives (remote: " << hasRemote 
                  << ", client: " << hasClient << ", server: " << hasServer << ")" << std::endl;
        return false;
    }
    
    // OpenVPN-specific directives that confirm it's OpenVPN
    std::vector<std::string> openvpnSpecific = {
        "dev tun", "dev tap", "proto udp", "proto tcp", "cipher ", "auth ",
        "tls-client", "tls-server", "tls-auth", "tls-crypt", "ca ", "cert ", "key ",
        "persist-key", "persist-tun", "resolv-retry", "nobind", "verb ", 
        "explicit-exit-notify", "remote-cert-tls", "auth-nocache", "setenv opt"
    };
    
    int specificMatches = 0;
    for (const auto& directive : openvpnSpecific) {
        if (lowerContent.find(directive) != std::string::npos) {
            specificMatches++;
            std::cout << "[OPENVPN-PARSER] Found OpenVPN directive: " << directive << std::endl;
        }
    }
    
    // Check for OpenVPN certificate/key blocks
    bool hasEmbeddedCerts = (configContent.find("<ca>") != std::string::npos ||
                            configContent.find("<cert>") != std::string::npos ||
                            configContent.find("<key>") != std::string::npos ||
                            configContent.find("<tls-crypt>") != std::string::npos ||
                            configContent.find("<tls-auth>") != std::string::npos);
    
    // Exclude other protocols
    bool hasWireGuardSections = (lowerContent.find("[interface]") != std::string::npos &&
                                lowerContent.find("[peer]") != std::string::npos);
    bool hasIKEv2Keywords = (lowerContent.find("keyexchange=ikev2") != std::string::npos ||
                            (lowerContent.find("conn ") != std::string::npos && 
                             lowerContent.find("config setup") != std::string::npos));
    
    if (hasWireGuardSections || hasIKEv2Keywords) {
        std::cout << "[OPENVPN-PARSER] Detected other protocol patterns, not OpenVPN" << std::endl;
        return false;
    }
    
    // Valid if we have specific directives or embedded certificates
    bool isValid = (specificMatches >= 3 || hasEmbeddedCerts);
    
    std::cout << "[OPENVPN-PARSER] Validation result: " << (isValid ? "VALID" : "INVALID")
              << " (specific matches: " << specificMatches << ", embedded certs: " << hasEmbeddedCerts << ")" << std::endl;
    
    return isValid;
}

json OpenVPNParser::parseConfig(const std::string& configContent) {
    std::cout << "[OPENVPN-PARSER] Starting OpenVPN configuration parsing..." << std::endl;
    
    json result;
    auto configMap = parseConfigLines(configContent);
    
    // Validate essential fields are present
    if (configMap.find("remote") == configMap.end()) {
        result["success"] = false;
        result["error"] = "Missing required 'remote' directive in OpenVPN configuration";
        return result;
    }
    
    // Extract server information
    std::string serverAddress = extractRemoteServer(configMap["remote"]);
    int port = extractRemotePort(configMap["remote"]);
    
    if (serverAddress.empty()) {
        result["success"] = false;
        result["error"] = "Invalid or missing server address in 'remote' directive";
        return result;
    }
    
    // Extract profile name - try to get from filename or generate from server
    std::string profileName = "OpenVPN (" + serverAddress + ")";
    
    // Extract authentication information
    std::string username = "";
    std::string authMethod = "Certificate"; // Default for most configs
    
    if (configMap.find("auth-user-pass") != configMap.end()) {
        authMethod = "Username/Password";
        // Extract username if specified in auth-user-pass file
        std::string authUserPass = configMap["auth-user-pass"];
        if (!authUserPass.empty() && authUserPass != "auth-user-pass") {
            // If it points to a file, we can't extract username from config
            username = "";
        }
    }
    
    // Determine protocol details
    std::string protocol = "OpenVPN";
    if (configMap.find("proto") != configMap.end()) {
        std::string proto = configMap["proto"];
        if (proto.find("udp") != std::string::npos) {
            protocol = "OpenVPN (UDP)";
        } else if (proto.find("tcp") != std::string::npos) {
            protocol = "OpenVPN (TCP)";
        }
    }
    
    // Extract encryption details
    std::string cipher = "AES-256-CBC"; // Default
    if (configMap.find("cipher") != configMap.end()) {
        cipher = configMap["cipher"];
    }
    
    std::string auth = "SHA1"; // Default
    if (configMap.find("auth") != configMap.end()) {
        auth = configMap["auth"];
    }
    
    // Extract device type
    std::string deviceType = "tun"; // Default
    if (configMap.find("dev") != configMap.end()) {
        std::string dev = configMap["dev"];
        if (dev.find("tap") != std::string::npos) {
            deviceType = "tap";
        }
    }
    
    // Check for compression
    bool compression = (configMap.find("comp-lzo") != configMap.end() || 
                       configMap.find("compress") != configMap.end());
    
    // Check for TLS features
    bool hasTLSAuth = configMap.find("tls-auth") != configMap.end();
    bool hasTLSCrypt = configContent.find("<tls-crypt>") != std::string::npos;
    bool hasTLSClient = configMap.find("tls-client") != configMap.end();
    bool hasRemoteCertTls = configMap.find("remote-cert-tls") != configMap.end();
    
    // Check for embedded certificates
    bool hasCACert = configContent.find("<ca>") != std::string::npos;
    bool hasClientCert = configContent.find("<cert>") != std::string::npos;
    bool hasPrivateKey = configContent.find("<key>") != std::string::npos;
    
    // Extract additional directives
    bool hasVerifyX509 = configMap.find("verify-x509-name") != configMap.end();
    bool hasPersistKey = configMap.find("persist-key") != configMap.end();
    bool hasPersistTun = configMap.find("persist-tun") != configMap.end();
    bool hasNoBind = configMap.find("nobind") != configMap.end();
    
    // Get TLS version info
    std::string tlsVersionMin = "";
    if (configMap.find("tls-version-min") != configMap.end()) {
        tlsVersionMin = configMap["tls-version-min"];
    }
    
    std::string tlsCipher = "";
    if (configMap.find("tls-cipher") != configMap.end()) {
        tlsCipher = configMap["tls-cipher"];
    }
    
    // Extract additional OpenVPN-specific data
    json openvpnData = {
        {"device_type", deviceType},
        {"tls_auth", hasTLSAuth},
        {"tls_crypt", hasTLSCrypt},
        {"tls_client", hasTLSClient},
        {"remote_cert_tls", hasRemoteCertTls},
        {"auth_algorithm", auth},
        {"has_ca_cert", hasCACert},
        {"has_client_cert", hasClientCert},
        {"has_private_key", hasPrivateKey},
        {"verify_x509_name", hasVerifyX509},
        {"persist_key", hasPersistKey},
        {"persist_tun", hasPersistTun},
        {"nobind", hasNoBind},
        {"tls_version_min", tlsVersionMin},
        {"tls_cipher", tlsCipher},
        {"has_embedded_certs", hasCACert || hasClientCert || hasPrivateKey}
    };
    
    result["success"] = true;
    result["protocol_detected"] = "OpenVPN";
    result["parser_used"] = "OpenVPN";
    result["profile_data"] = {
        {"name", profileName},
        {"server", serverAddress},
        {"protocol", protocol},
        {"port", port},
        {"username", username},
        {"password", ""}, // Password not stored in config files for security
        {"auth_method", authMethod},
        {"encryption", cipher},
        {"compression", compression},
        {"openvpn_specific", openvpnData}
    };
    
    std::cout << "[OPENVPN-PARSER] Successfully parsed OpenVPN configuration for server: " << serverAddress << std::endl;
    return result;
}

std::map<std::string, std::string> OpenVPNParser::parseConfigLines(const std::string& configContent) {
    std::map<std::string, std::string> configMap;
    std::istringstream iss(configContent);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Skip empty lines and comments (unless they're special comments)
        if (line.empty() || (line[0] == '#' && line.find("Profile") == std::string::npos)) {
            continue;
        }
        
        // Handle special comments
        if (line[0] == '#' && line.find("Profile") != std::string::npos) {
            configMap["# Profile"] = line.substr(line.find(":") + 1);
            continue;
        }
        
        // Split line into directive and value
        size_t spacePos = line.find(' ');
        if (spacePos != std::string::npos) {
            std::string directive = line.substr(0, spacePos);
            std::string value = line.substr(spacePos + 1);
            configMap[directive] = value;
        } else {
            configMap[line] = "";
        }
    }
    
    return configMap;
}

std::string OpenVPNParser::extractRemoteServer(const std::string& remoteLine) {
    std::istringstream iss(remoteLine);
    std::string server;
    iss >> server; // First part after "remote" is the server
    return server;
}

int OpenVPNParser::extractRemotePort(const std::string& remoteLine) {
    std::istringstream iss(remoteLine);
    std::string server, portStr;
    iss >> server >> portStr; // Second part is the port
    
    if (!portStr.empty()) {
        try {
            return std::stoi(portStr);
        } catch (...) {
            return 1194; // Default port
        }
    }
    return 1194;
}

std::string OpenVPNParser::extractUsername(const std::string& configContent) {
    // Look for username in auth-user-pass file or inline
    std::regex usernameRegex(R"(auth-user-pass\s+(\S+))");
    std::smatch match;
    
    if (std::regex_search(configContent, match, usernameRegex)) {
        // Username might be in a separate file or inline
        return ""; // Usually not stored in config file for security
    }
    
    return "";
}

std::string OpenVPNParser::determineProtocol(const std::string& configContent) {
    if (configContent.find("proto udp") != std::string::npos) {
        return "OpenVPN (UDP)";
    } else if (configContent.find("proto tcp") != std::string::npos) {
        return "OpenVPN (TCP)";
    }
    return "OpenVPN";
}
