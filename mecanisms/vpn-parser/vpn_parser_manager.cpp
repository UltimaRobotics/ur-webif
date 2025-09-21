
#include "vpn_parser_manager.hpp"
#include <iostream>

json VPNParserManager::parseVPNConfig(const std::string& configContent) {
    json result;
    
    if (configContent.empty()) {
        result["success"] = false;
        result["error"] = "Empty configuration content";
        result["error_type"] = "EMPTY_CONTENT";
        return result;
    }
    
    try {
        VPNProtocol protocol = identifyProtocol(configContent);
        
        if (protocol == VPNProtocol::UNKNOWN) {
            result["success"] = false;
            result["error"] = "Unable to identify VPN protocol from configuration content";
            result["error_type"] = "PROTOCOL_IDENTIFICATION_FAILED";
            result["protocol_detected"] = "Unknown";
            return result;
        }
        
        json parseResult;
        switch (protocol) {
            case VPNProtocol::OPENVPN:
                parseResult = OpenVPNParser::parseConfig(configContent);
                break;
                
            case VPNProtocol::IKEV2:
                parseResult = IKEv2Parser::parseConfig(configContent);
                break;
                
            case VPNProtocol::WIREGUARD:
                parseResult = WireGuardParser::parseConfig(configContent);
                break;
                
            default:
                result["success"] = false;
                result["error"] = "Unsupported VPN protocol";
                result["error_type"] = "UNSUPPORTED_PROTOCOL";
                return result;
        }
        
        // Validate parsing results - no fallbacks allowed
        if (!parseResult["success"].get<bool>()) {
            result["success"] = false;
            result["error"] = "Failed to parse " + protocolToString(protocol) + " configuration: " + parseResult.value("error", "Unknown parsing error");
            result["error_type"] = "PARSING_FAILED";
            result["protocol_detected"] = protocolToString(protocol);
            return result;
        }
        
        // Ensure required fields are present
        if (!parseResult.contains("profile_data") || parseResult["profile_data"].empty()) {
            result["success"] = false;
            result["error"] = "Parser failed to extract profile data from " + protocolToString(protocol) + " configuration";
            result["error_type"] = "MISSING_PROFILE_DATA";
            result["protocol_detected"] = protocolToString(protocol);
            return result;
        }
        
        return parseResult;
        
    } catch (const std::exception& e) {
        result["success"] = false;
        result["error"] = "Configuration parsing exception: " + std::string(e.what());
        result["error_type"] = "PARSING_EXCEPTION";
        return result;
    }
}

json VPNParserManager::detectProtocol(const std::string& configContent) {
    json result;
    VPNProtocol protocol = identifyProtocol(configContent);
    
    result["success"] = protocol != VPNProtocol::UNKNOWN;
    result["protocol"] = protocolToString(protocol);
    result["confidence"] = protocol != VPNProtocol::UNKNOWN ? "high" : "none";
    
    return result;
}

VPNParserManager::VPNProtocol VPNParserManager::identifyProtocol(const std::string& configContent) {
    std::cout << "[VPN-PARSER-MANAGER] Starting protocol identification..." << std::endl;
    
    // Count how many protocols match to avoid ambiguity
    int matchCount = 0;
    VPNProtocol detectedProtocol = VPNProtocol::UNKNOWN;
    
    // WireGuard check - most distinctive format with strict validation
    if (WireGuardParser::isWireGuardConfig(configContent)) {
        std::cout << "[VPN-PARSER-MANAGER] WireGuard pattern detected" << std::endl;
        matchCount++;
        detectedProtocol = VPNProtocol::WIREGUARD;
    }
    
    // IKEv2 check with strict validation
    if (IKEv2Parser::isIKEv2Config(configContent)) {
        std::cout << "[VPN-PARSER-MANAGER] IKEv2 pattern detected" << std::endl;
        matchCount++;
        if (detectedProtocol == VPNProtocol::UNKNOWN) {
            detectedProtocol = VPNProtocol::IKEV2;
        }
    }
    
    // OpenVPN check with strict validation
    if (OpenVPNParser::isOpenVPNConfig(configContent)) {
        std::cout << "[VPN-PARSER-MANAGER] OpenVPN pattern detected" << std::endl;
        matchCount++;
        if (detectedProtocol == VPNProtocol::UNKNOWN) {
            detectedProtocol = VPNProtocol::OPENVPN;
        }
    }
    
    // Reject ambiguous configurations
    if (matchCount > 1) {
        std::cout << "[VPN-PARSER-MANAGER] Multiple protocols detected (" << matchCount << "), rejecting ambiguous configuration" << std::endl;
        return VPNProtocol::UNKNOWN;
    }
    
    if (matchCount == 0) {
        std::cout << "[VPN-PARSER-MANAGER] No known VPN protocol patterns detected" << std::endl;
        return VPNProtocol::UNKNOWN;
    }
    
    std::cout << "[VPN-PARSER-MANAGER] Protocol identified: " << protocolToString(detectedProtocol) << std::endl;
    return detectedProtocol;
}

std::string VPNParserManager::protocolToString(VPNProtocol protocol) {
    switch (protocol) {
        case VPNProtocol::OPENVPN:
            return "OpenVPN";
        case VPNProtocol::IKEV2:
            return "IKEv2";
        case VPNProtocol::WIREGUARD:
            return "WireGuard";
        case VPNProtocol::UNKNOWN:
        default:
            return "Unknown";
    }
}
