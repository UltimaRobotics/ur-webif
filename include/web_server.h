#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <memory>
#include <string>
#include <functional>
#include <map>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <microhttpd.h>

class WebSocketHandler;
class HttpHandler;
class FileServer;
class ApiRequest;
class ApiResponse;

// Forward declarations for callback types
struct ConnectionInfo;
struct MessageContext;
struct RequestContext;
struct DataContext;
enum class EventResult;
enum class EventType;

struct ServerConfig {
    int port = 8080;
    std::string host = "0.0.0.0";
    std::string document_root = "./web";
    std::string default_file = "index.html";
    std::string data_directory;
    bool enable_websocket = true;
    int websocket_port = 8081;
    bool enable_ssl = false;
    std::string ssl_cert_file;
    std::string ssl_key_file;
    int max_connections = 1000;
    int connection_timeout = 30;
    bool enable_cors = true;
    std::vector<std::string> allowed_origins;

    // WebSocket debugging configuration
    bool websocket_debug_enabled = false;
    bool websocket_debug_connections = false;
    bool websocket_debug_messages = false;
    bool websocket_debug_handshakes = false;
    bool websocket_debug_errors = true;
    bool websocket_debug_frame_details = false;

    // Endpoint logging configuration
    std::map<std::string, bool> endpoint_logging = {
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
};

class WebServer {
public:
    WebServer();
    ~WebServer();

    // Configuration
    bool loadConfig(const std::string& config_file);
    void setConfig(const ServerConfig& config);
    const ServerConfig& getConfig() const { return config_; }

    // Server lifecycle
    bool start();
    void stop();
    bool isRunning() const { return running_; }

    // HTTP handlers
    void addRouteHandler(const std::string& path, 
                        std::function<std::string(const std::string& method, 
                                                  const std::map<std::string, std::string>& params,
                                                  const std::string& body)> handler);

    // Dynamic route handlers (pattern-based routing)
    void addDynamicRouteHandler(const std::string& pattern,
                               std::function<std::string(const std::string& method, 
                                                        const std::map<std::string, std::string>& params,
                                                        const std::string& body)> handler);

    // Structured API route handlers
    using RouteProcessor = std::function<ApiResponse(const ApiRequest&)>;
    void addStructuredRouteHandler(const std::string& path, RouteProcessor processor);

    // ======== NEW CALLBACK-BASED WEBSOCKET INTERFACE ========

    // Connection event callbacks
    std::string onWebSocketConnected(std::function<EventResult(int connection_id, const ConnectionInfo&)> callback);
    std::string onWebSocketDisconnected(std::function<EventResult(int connection_id, const std::string& reason)> callback);
    std::string onWebSocketError(std::function<EventResult(int connection_id, const std::string& error)> callback);

    // Message event callbacks
    std::string onWebSocketMessage(std::function<EventResult(int connection_id, const std::string& message)> callback);
    std::string onWebSocketBinaryMessage(std::function<EventResult(int connection_id, const std::vector<uint8_t>& data)> callback);
    std::string onWebSocketMessageSent(std::function<EventResult(int connection_id, const std::string& message)> callback);
    std::string onWebSocketMessageFailed(std::function<EventResult(int connection_id, const std::string& message)> callback);

    // Request event callbacks  
    std::string onWebSocketUpgradeRequest(std::function<EventResult(int connection_id, const RequestContext& request)> callback);
    std::string onWebSocketAuthentication(std::function<EventResult(int connection_id, const std::string& token)> callback);
    std::string onWebSocketAuthorization(std::function<EventResult(int connection_id, const std::map<std::string, std::string>& headers)> callback);

    // Data processing callbacks
    std::string onWebSocketDataValidation(std::function<EventResult(int connection_id, const std::string& data)> callback);
    std::string onWebSocketDataTransformation(std::function<EventResult(int connection_id, std::string& data)> callback);
    std::string onWebSocketDataProcessing(std::function<EventResult(int connection_id, const nlohmann::json& data)> callback);

    // System event callbacks
    std::string onWebSocketHeartbeat(std::function<EventResult()> callback);
    std::string onWebSocketHealthCheck(std::function<EventResult(int active_connections)> callback);
    std::string onWebSocketServerEvent(std::function<EventResult(EventType type, const std::map<std::string, std::string>& data)> callback);

    // Advanced callback registration
    std::string registerWebSocketCallback(const std::function<bool(EventType, int connection_id)>& filter,
                                         std::function<EventResult(EventType type, int connection_id, const std::string& data)> callback);

    // Callback management
    bool unregisterWebSocketCallback(const std::string& callback_id);
    void unregisterAllWebSocketCallbacks();
    std::vector<std::string> getRegisteredWebSocketCallbacks() const;
    void activateWebSocketCallback(const std::string& callback_id);
    void deactivateWebSocketCallback(const std::string& callback_id);

    // ======== WEBSOCKET MESSAGING (Enhanced) ========

    // Send messages with callbacks
    void sendWebSocketMessage(int connection_id, const std::string& message,
                             std::function<void(bool success)> callback = nullptr);
    void broadcastWebSocketMessage(const std::string& message,
                                  std::function<void(std::vector<int> failed_connections)> callback = nullptr);
    void sendToWebSocketConnections(const std::vector<int>& connection_ids, const std::string& message,
                                   std::function<void(std::map<int, bool> results)> callback = nullptr);

    // Binary message support
    void sendWebSocketBinaryMessage(int connection_id, const std::vector<uint8_t>& data,
                                   std::function<void(bool success)> callback = nullptr);

    // ======== WEBSOCKET CONNECTION MANAGEMENT ========

    std::vector<int> getActiveWebSocketConnections() const;
    ConnectionInfo getWebSocketConnectionInfo(int connection_id) const;
    bool isWebSocketConnectionAlive(int connection_id) const;
    int getActiveWebSocketConnectionCount() const;

    void closeWebSocketConnection(int connection_id, const std::string& reason = "",
                                 std::function<void(bool success)> callback = nullptr);

    // Connection metadata
    void setWebSocketConnectionMetadata(int connection_id, const std::string& key, 
                                       const std::string& value);
    std::string getWebSocketConnectionMetadata(int connection_id, const std::string& key) const;

    // ======== WEBSOCKET CONFIGURATION ========

    void setWebSocketDebugging(bool enabled);
    void setWebSocketEventLogging(bool enabled);
    void setWebSocketPerformanceMetrics(bool enabled);
    void setWebSocketAuthenticationRequired(bool required);
    void setWebSocketConnectionLimit(int limit);
    void setWebSocketRateLimit(int messages_per_second, int burst_size = 10);
    void setWebSocketHeartbeatInterval(int seconds);
    void setWebSocketConnectionTimeout(int seconds);

    // ======== WEBSOCKET STATISTICS ========

    nlohmann::json getWebSocketStatistics() const;
    void resetWebSocketStatistics();

    // ======== LEGACY COMPATIBILITY ========

    // Deprecated - use callback-based methods instead
    [[deprecated("Use onWebSocketMessage() callback instead")]]
    void setWebSocketMessageHandler(std::function<void(int, const std::string&)> handler);

    // ======== EVENT PROCESSING CONTROL ========

    void setWebSocketEventProcessingThreads(int thread_count);
    void pauseWebSocketEventProcessing();
    void resumeWebSocketEventProcessing();
    bool isWebSocketEventProcessingActive() const;

    size_t getWebSocketEventQueueSize() const;
    void clearWebSocketEventQueue();
    void setMaxWebSocketEventQueueSize(size_t max_size);

private:
    ServerConfig config_;
    std::atomic<bool> running_;

    // HTTP server
    struct MHD_Daemon* http_daemon_;

    // WebSocket server with new callback system
    std::unique_ptr<WebSocketHandler> websocket_handler_;

    // Handlers
    std::unique_ptr<HttpHandler> http_handler_;
    std::unique_ptr<FileServer> file_server_;

    // Route handlers (for HTTP)
    std::map<std::string, std::function<std::string(const std::string&, 
                                                   const std::map<std::string, std::string>&,
                                                   const std::string&)>> route_handlers_;

    // WebSocket callback tracking
    std::vector<std::string> websocket_callback_ids_;
    mutable std::mutex callback_ids_mutex_;

    // Static callbacks for MHD
    static enum MHD_Result accessHandlerCallback(void* cls, struct MHD_Connection* connection,
                                   const char* url, const char* method,
                                   const char* version, const char* upload_data,
                                   size_t* upload_data_size, void** con_cls);

    static void requestCompletedCallback(void* cls, struct MHD_Connection* connection,
                                       void** con_cls, enum MHD_RequestTerminationCode toe);

    // Request handling
    enum MHD_Result handleRequest(struct MHD_Connection* connection,
                     const char* url, const char* method,
                     const char* version, const char* upload_data,
                     size_t* upload_data_size, void** con_cls);

    // Internal helpers
    void setupDefaultWebSocketCallbacks();
    void addCallbackId(const std::string& callback_id);
    void removeCallbackId(const std::string& callback_id);

    // HTTP response helpers
    enum MHD_Result createSuccessfulResponse(struct MHD_Connection* connection, 
                                           const std::string& content, 
                                           const std::string& content_type);
    std::string getHomepageContent() const;

    // Utility functions
    std::string getMimeType(const std::string& file_path);
    enum MHD_Result enableCORS(struct MHD_Connection* connection);
};

#endif // WEB_SERVER_H