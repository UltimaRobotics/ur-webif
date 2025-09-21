
# VPN Parser Mechanism

This mechanism provides automatic parsing and configuration detection for various VPN protocols including OpenVPN, IKEv2, and WireGuard.

## Features

- **Automatic Protocol Detection**: Automatically identifies the VPN protocol from configuration content
- **Multi-Format Support**: Supports OpenVPN (.ovpn), IKEv2 (.conf), and WireGuard (.wg, .config) configuration files
- **Field Extraction**: Automatically extracts common configuration fields such as:
  - Profile Name
  - Server Address
  - Protocol
  - Port
  - Username
  - Password (if available)
  - Authentication Method
  - Encryption Settings

## API Endpoints

### Universal Parser
- **POST** `/api/vpn-parser/openvpn-parser`
- **POST** `/api/vpn-parser/ikev2-parser`  
- **POST** `/api/vpn-parser/wireguard-parser`

All endpoints accept:
```json
{
  "config_content": "string - VPN configuration file content"
}
```

And return:
```json
{
  "success": true/false,
  "protocol_detected": "OpenVPN|IKEv2|WireGuard",
  "profile_data": {
    "name": "string",
    "server": "string", 
    "protocol": "string",
    "port": "number",
    "username": "string",
    "password": "string",
    "auth_method": "string",
    "encryption": "string"
  },
  "timestamp": "number",
  "parser_type": "string"
}
```

## Usage

### Frontend Integration
The VPN configuration section automatically:
1. Detects when a file is uploaded
2. Reads the file content
3. Calls the appropriate parser API
4. Auto-fills the form fields with parsed data
5. Shows success/error notifications

### Backend Integration
The parsers are integrated into the VPN router and can be used by:
1. Including the VPN parser manager header
2. Calling `VPNParserManager::parseVPNConfig(configContent)`
3. Processing the returned JSON response

## Supported Configuration Formats

### OpenVPN
- Detects: client, remote, ca, cert, key, tls-auth, cipher directives
- Extracts: server address, port, protocol (TCP/UDP), authentication method

### IKEv2
- Detects: conn, left, right, keyexchange=ikev2, ike=, esp= directives
- Extracts: server address, port, username, connection name

### WireGuard
- Detects: [Interface], [Peer], PrivateKey, PublicKey, Endpoint sections
- Extracts: server address, port, allowed IPs, DNS settings

## Error Handling

The parser provides comprehensive error handling for:
- Empty or invalid configuration content
- Unsupported file formats
- Malformed configuration syntax
- Network errors during API calls

## Security Considerations

- Configuration files are parsed in memory only
- Sensitive data like private keys are not stored
- File content is validated before processing
- All API calls are logged for security auditing
