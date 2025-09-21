#include "config_parser.h"
#include <fstream>
#include <iostream>

bool ConfigParser::parseConfig(const std::string& config_file, ServerConfig& config) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            return false;
        }

        nlohmann::json json_config;
        file >> json_config;

        // Parse configuration values
        if (json_config.contains("port")) {
            config.port = json_config["port"];
        }
        
        if (json_config.contains("host")) {
            config.host = json_config["host"];
        }
        
        if (json_config.contains("document_root")) {
            config.document_root = json_config["document_root"];
        }
        
        if (json_config.contains("default_file")) {
            config.default_file = json_config["default_file"];
        }
        
        if (json_config.contains("data_directory")) {
            config.data_directory = json_config["data_directory"];
        }
        
        if (json_config.contains("enable_websocket")) {
            config.enable_websocket = json_config["enable_websocket"];
        }
        
        if (json_config.contains("websocket_port")) {
            config.websocket_port = json_config["websocket_port"];
        }
        
        if (json_config.contains("enable_ssl")) {
            config.enable_ssl = json_config["enable_ssl"];
        }
        
        if (json_config.contains("ssl_cert_file")) {
            config.ssl_cert_file = json_config["ssl_cert_file"];
        }
        
        if (json_config.contains("ssl_key_file")) {
            config.ssl_key_file = json_config["ssl_key_file"];
        }
        
        if (json_config.contains("max_connections")) {
            config.max_connections = json_config["max_connections"];
        }
        
        if (json_config.contains("connection_timeout")) {
            config.connection_timeout = json_config["connection_timeout"];
        }
        
        if (json_config.contains("enable_cors")) {
            config.enable_cors = json_config["enable_cors"];
        }
        
        if (json_config.contains("allowed_origins")) {
            config.allowed_origins = json_config["allowed_origins"].get<std::vector<std::string>>();
        }
        
        // Parse WebSocket debugging configuration
        if (json_config.contains("websocket_debug_enabled")) {
            config.websocket_debug_enabled = json_config["websocket_debug_enabled"];
        }
        
        if (json_config.contains("websocket_debug_connections")) {
            config.websocket_debug_connections = json_config["websocket_debug_connections"];
        }
        
        if (json_config.contains("websocket_debug_messages")) {
            config.websocket_debug_messages = json_config["websocket_debug_messages"];
        }
        
        if (json_config.contains("websocket_debug_handshakes")) {
            config.websocket_debug_handshakes = json_config["websocket_debug_handshakes"];
        }
        
        if (json_config.contains("websocket_debug_errors")) {
            config.websocket_debug_errors = json_config["websocket_debug_errors"];
        }
        
        if (json_config.contains("websocket_debug_frame_details")) {
            config.websocket_debug_frame_details = json_config["websocket_debug_frame_details"];
        }
        
        // Parse endpoint logging configuration
        if (json_config.contains("endpoint_logging")) {
            config.endpoint_logging.clear();
            for (auto& [key, value] : json_config["endpoint_logging"].items()) {
                config.endpoint_logging[key] = value.get<bool>();
            }
        }

        validateConfig(config);
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigParser::saveConfig(const std::string& config_file, const ServerConfig& config) {
    try {
        nlohmann::json json_config;
        
        json_config["port"] = config.port;
        json_config["host"] = config.host;
        json_config["document_root"] = config.document_root;
        json_config["default_file"] = config.default_file;
        json_config["data_directory"] = config.data_directory;
        json_config["enable_websocket"] = config.enable_websocket;
        json_config["websocket_port"] = config.websocket_port;
        json_config["enable_ssl"] = config.enable_ssl;
        json_config["ssl_cert_file"] = config.ssl_cert_file;
        json_config["ssl_key_file"] = config.ssl_key_file;
        json_config["max_connections"] = config.max_connections;
        json_config["connection_timeout"] = config.connection_timeout;
        json_config["enable_cors"] = config.enable_cors;
        json_config["allowed_origins"] = config.allowed_origins;
        
        // Save WebSocket debugging configuration
        json_config["websocket_debug_enabled"] = config.websocket_debug_enabled;
        json_config["websocket_debug_connections"] = config.websocket_debug_connections;
        json_config["websocket_debug_messages"] = config.websocket_debug_messages;
        json_config["websocket_debug_handshakes"] = config.websocket_debug_handshakes;
        json_config["websocket_debug_errors"] = config.websocket_debug_errors;
        json_config["websocket_debug_frame_details"] = config.websocket_debug_frame_details;
        
        // Save endpoint logging configuration
        json_config["endpoint_logging"] = config.endpoint_logging;
        
        std::ofstream file(config_file);
        if (!file.is_open()) {
            return false;
        }
        
        file << json_config.dump(4);
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving config file: " << e.what() << std::endl;
        return false;
    }
}

ServerConfig ConfigParser::getDefaultConfig() {
    ServerConfig config;
    config.port = 8080;
    config.host = "0.0.0.0";
    config.document_root = "./web";
    config.default_file = "index.html";
    config.data_directory = "./data";
    config.enable_websocket = true;
    config.websocket_port = 8081;
    config.enable_ssl = false;
    config.max_connections = 1000;
    config.connection_timeout = 30;
    config.enable_cors = true;
    config.allowed_origins = {"*"};
    
    // Default WebSocket debugging configuration
    config.websocket_debug_enabled = false;
    config.websocket_debug_connections = false;
    config.websocket_debug_messages = false;
    config.websocket_debug_handshakes = false;
    config.websocket_debug_errors = true;
    config.websocket_debug_frame_details = false;
    
    // Default endpoint logging configuration (all enabled by default)
    config.endpoint_logging = {
        {"auth", true},
        {"dashboard", true},
        {"system", true},
        {"network", true},
        {"cellular", true},
        {"utils", true},
        {"wired", true},
        {"http", true},
        {"websocket", true},
        {"file_server", true}
    };
    
    return config;
}

void ConfigParser::validateConfig(ServerConfig& config) {
    // Validate port ranges
    if (config.port < 1 || config.port > 65535) {
        std::cerr << "Warning: Invalid port " << config.port << ", using default 8080" << std::endl;
        config.port = 8080;
    }
    
    if (config.websocket_port < 1 || config.websocket_port > 65535) {
        std::cerr << "Warning: Invalid websocket port " << config.websocket_port << ", using default 8081" << std::endl;
        config.websocket_port = 8081;
    }
    
    // Validate connection limits
    if (config.max_connections < 1) {
        std::cerr << "Warning: Invalid max_connections " << config.max_connections << ", using default 1000" << std::endl;
        config.max_connections = 1000;
    }
    
    if (config.connection_timeout < 1) {
        std::cerr << "Warning: Invalid connection_timeout " << config.connection_timeout << ", using default 30" << std::endl;
        config.connection_timeout = 30;
    }
    
    // Ensure document root is not empty
    if (config.document_root.empty()) {
        config.document_root = "./web";
    }
    
    // Ensure default file is not empty
    if (config.default_file.empty()) {
        config.default_file = "index.html";
    }
    
    // Ensure data directory is not empty
    if (config.data_directory.empty()) {
        config.data_directory = "./data";
    }
}