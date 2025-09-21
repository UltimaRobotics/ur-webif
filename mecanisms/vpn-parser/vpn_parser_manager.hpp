
#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "openvpn_parser.hpp"
#include "ikev2_parser.hpp"
#include "wireguard_parser.hpp"

using json = nlohmann::json;

class VPNParserManager {
public:
    static json parseVPNConfig(const std::string& configContent);
    static json detectProtocol(const std::string& configContent);
    
private:
    enum class VPNProtocol {
        UNKNOWN,
        OPENVPN,
        IKEV2,
        WIREGUARD
    };
    
    static VPNProtocol identifyProtocol(const std::string& configContent);
    static std::string protocolToString(VPNProtocol protocol);
};
