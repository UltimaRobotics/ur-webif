
# UR WebIF API - Network Management System

A comprehensive C++ network management API server designed for embedded systems and network infrastructure. This production-ready system provides a complete web interface for managing network configurations, backup operations, firmware updates, VPN connections, and advanced networking features.

## 🌟 Key Features

### Network Management
- **Advanced Network Configuration**: VLANs, bridges, static routes, NAT rules, and firewall management
- **Network Priority System**: Interface prioritization and routing rule management
- **Wireless Management**: Wi-Fi configuration, access point setup, and network scanning
- **Cellular Integration**: Cellular modem configuration and monitoring
- **Wired Networking**: Ethernet interface configuration and management

### System Operations
- **Backup & Restore**: Complete system backup with selective restore capabilities
- **Firmware Management**: TFTP-based and manual firmware upgrade system
- **License Management**: Enterprise license validation and feature control
- **Authentication System**: File-based authentication with security mechanisms

### Advanced Features
- **VPN Support**: OpenVPN, WireGuard, and IKEv2 configuration management
- **Network Utilities**: Integrated ping, traceroute, DNS lookup, and bandwidth testing
- **Real-time Monitoring**: System performance and network status monitoring
- **RESTful API**: Comprehensive HTTP-based API with JSON responses

## 🏗️ Architecture

### Core Components

1. **WebServer**: High-performance HTTP server using libmicrohttpd
2. **Router System**: Modular routing architecture with specialized handlers:
   - `AdvancedNetworkRouter`: VLAN, bridge, NAT, and firewall management
   - `BackupRouter`: System backup and restore operations
   - `FirmwareRouter`: Firmware update and management
   - `LicenseRouter`: License validation and feature management
   - `VpnRouter`: VPN configuration and management
   - `WirelessRouter`: Wi-Fi and wireless network management
3. **Data Managers**: Specialized data handling for cellular, network priority, and VPN
4. **Utility Engines**: Network diagnostic and testing tools
5. **Authentication System**: Secure file-based authentication with session management

### Technology Stack

- **Backend**: C++17 with modern standard library features
- **HTTP Server**: libmicrohttpd for high-performance request handling
- **JSON Processing**: nlohmann/json for configuration and API responses
- **Network Libraries**: ASIO for asynchronous networking operations
- **Frontend**: Modern HTML5/CSS3 with vanilla JavaScript
- **UI Framework**: TailwindCSS for responsive design
- **Security**: OpenSSL for cryptographic operations

## 🚀 Quick Start

### Prerequisites

- C++17 compatible compiler (GCC 8+, Clang 7+)
- CMake 3.16 or higher
- Git for dependency management

### Building

```bash
cd ur-webif-api
mkdir build
cd build
cmake ..
make -j$(nproc)
```

### Running

```bash
# From the ur-webif-api directory
./build/ur-webif-api -c config/server.json
```

The server will start on port 5000 by default and serve the web interface.

## 📋 Configuration

The system uses JSON-based configuration files:

### Server Configuration (`config/server.json`)
```json
{
    "port": 5000,
    "host": "0.0.0.0",
    "document_root": "./web",
    "data_directory": "./data",
    "enable_cors": true,
    "backup_configuration": {
        "storage_directory": "./data/backup-storage",
        "max_backup_count": 10,
        "backup_types": ["full", "partial", "credentials_only"]
    }
}
```

### Data Organization
- `data/network-priority/`: Network interface and routing configurations
- `data/backup/`: System backup files and metadata
- `data/firmware/`: Firmware images and upgrade progress
- `data/vpn/`: VPN profiles and routing rules
- `data/cellular/`: Cellular modem configuration and status
- `data/wireless/`: Wi-Fi networks and access point settings

## 🔌 API Endpoints

### Authentication
- `POST /api/login` - User authentication
- `POST /api/file-auth` - File-based authentication

### Network Management
- `GET/POST /api/advanced-network/vlans` - VLAN configuration
- `GET/POST /api/advanced-network/bridges` - Network bridge management
- `GET/POST /api/advanced-network/firewall-rules` - Firewall configuration
- `GET/POST /api/advanced-network/nat-rules` - NAT rule management
- `GET/POST /api/advanced-network/static-routes` - Static routing

### System Operations
- `GET/POST /api/backup/*` - Backup and restore operations
- `GET/POST /api/firmware/*` - Firmware management
- `GET/POST /api/license/*` - License management
- `POST /api/dashboard-data` - System status and monitoring

### Network Utilities
- `POST /api/utils/ping` - Ping utility
- `POST /api/utils/traceroute` - Traceroute utility
- `POST /api/utils/dns-lookup` - DNS resolution
- `POST /api/utils/bandwidth` - Bandwidth testing

## 🛠️ Development

### Project Structure
```
ur-webif-api/
├── src/                    # Source code
│   ├── routers/           # API route handlers
│   ├── network-ops/       # Network operation handlers
│   ├── backup-restore/    # Backup system implementation
│   ├── firmware-update/   # Firmware management
│   └── utilities/         # Network diagnostic tools
├── web/                   # Frontend web interface
├── config/                # Configuration files
├── data/                  # Runtime data storage
└── tests/                 # Test suites
```

### Adding New Features

1. **Router Implementation**: Create new router in `src/routers/`
2. **Data Management**: Add data manager in `src/` for complex operations
3. **Frontend Integration**: Add corresponding JavaScript section in `web/assets/js/sections/`
4. **Configuration**: Update `config/server.json` for new settings

### Testing

The project includes comprehensive test suites:
- Network operations testing with pytest
- Authentication mechanism validation
- Firmware upgrade testing
- Advanced network feature testing

```bash
# Run network tests
python advance-network-tests/run_advanced_network_tests.py

# Run authentication tests
python auth-mec-tests/test_auth_file_verification.py
```

## 🔒 Security Features

- **File-based Authentication**: Secure token-based authentication system
- **Session Management**: Configurable session timeouts and security policies
- **Input Validation**: Comprehensive input sanitization and validation
- **CORS Support**: Configurable cross-origin resource sharing
- **Encrypted Backups**: Optional encryption for sensitive backup data

## 🌐 Web Interface

The modern web interface provides:
- **Dashboard**: Real-time system monitoring and status
- **Network Configuration**: Visual network topology and configuration
- **Backup Management**: Intuitive backup creation and restoration
- **Firmware Updates**: Progress tracking and validation
- **VPN Management**: Visual VPN profile configuration
- **Mobile Responsive**: Optimized for tablets and mobile devices

## 📊 Monitoring & Diagnostics

- **Real-time Metrics**: CPU, memory, and network utilization
- **Network Diagnostics**: Integrated ping, traceroute, and bandwidth testing
- **Connection Monitoring**: Active connection tracking and performance metrics
- **System Health**: Automated health checks and status reporting
- **Error Logging**: Comprehensive logging with configurable verbosity

## 🚀 Deployment

### Production Deployment on Replit

The project is optimized for deployment on Replit's infrastructure:

1. **Automatic Builds**: CMake-based build system with dependency management
2. **Port Configuration**: Configured for Replit's port forwarding (5000 → 80/443)
3. **Data Persistence**: JSON-based data storage for reliability
4. **Environment Integration**: Seamless integration with Replit's environment

### Performance Optimization

- **Asynchronous Operations**: Non-blocking I/O for network operations
- **Efficient Data Structures**: Optimized JSON processing and caching
- **Resource Management**: Configurable connection limits and timeouts
- **Static File Serving**: Efficient serving with proper MIME types

## 📈 Scalability

The system is designed for:
- **High Concurrency**: Support for 1000+ simultaneous connections
- **Modular Architecture**: Easy addition of new features and modules
- **Configuration Flexibility**: Extensive configuration options
- **Platform Independence**: Cross-platform compatibility

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Implement your changes with tests
4. Submit a pull request with detailed description

## 📄 License

This project is open source. Please refer to the license file for details.

## 🔗 Related Projects

- **QMI Scanner**: Cellular modem detection and configuration
- **Connection Manager**: Network connection prioritization
- **Network Priority Tests**: Automated testing framework

---

**Built with ❤️ for modern network infrastructure management**
