#ifndef INTEGRATION_EXAMPLE_HPP
#define INTEGRATION_EXAMPLE_HPP

#include "login_handler.hpp"
#include "security_utils.hpp"
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

/**
 * Example integration class showing how to integrate the login mechanism
 * with a WebSocket server. This demonstrates the proper way to handle
 * authentication messages in the main server application.
 */
class WebSocketServerWithLogin {
public:
    typedef websocketpp::server<websocketpp::config::asio> server;
    typedef server::message_ptr message_ptr;
    
    WebSocketServerWithLogin();
    ~WebSocketServerWithLogin();
    
    // Server lifecycle
    bool initialize(const std::string& config_path);
    void run(uint16_t port);
    void stop();
    
    // Message handling
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
    void on_open(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    
private:
    server ws_server_;
    std::shared_ptr<LoginManager> login_manager_;
    std::shared_ptr<LoginHandler> login_handler_;
    
    // Connection tracking
    struct ConnectionInfo {
        std::string client_id;
        std::string client_ip;
        std::string session_token;
        std::chrono::steady_clock::time_point connected_at;
        bool authenticated;
    };
    
    std::unordered_map<websocketpp::connection_hdl, ConnectionInfo, 
                      std::owner_less<websocketpp::connection_hdl>> connections_;
    std::mutex connections_mutex_;
    
    // Helper methods
    std::string extractClientIP(websocketpp::connection_hdl hdl);
    std::string generateClientId();
    void sendMessage(websocketpp::connection_hdl hdl, const std::string& message);
    void broadcastToAuthenticated(const std::string& message);
    
    // Message processors
    void processLoginMessage(websocketpp::connection_hdl hdl, const nlohmann::json& message);
    void processLogoutMessage(websocketpp::connection_hdl hdl, const nlohmann::json& message);
    void processAuthenticatedMessage(websocketpp::connection_hdl hdl, const nlohmann::json& message);
    
    // Security and maintenance
    void performPeriodicMaintenance();
    bool isConnectionAuthenticated(websocketpp::connection_hdl hdl);
    
    // Configuration
    std::string config_path_;
    std::thread maintenance_thread_;
    bool running_;
};

#endif // INTEGRATION_EXAMPLE_HPP