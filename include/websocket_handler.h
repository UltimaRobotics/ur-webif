#ifndef WEBSOCKET_HANDLER_H
#define WEBSOCKET_HANDLER_H

#include <functional>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <iostream>
#include <set>
#include <queue>
#include <condition_variable>
#include <variant>
#include <chrono>

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/server.hpp>
#include <nlohmann/json.hpp>

// WebSocket server types for TLS and non-TLS
typedef websocketpp::server<websocketpp::config::asio_tls> tls_server;
typedef websocketpp::server<websocketpp::config::asio> server;
typedef server::message_ptr message_ptr;
typedef websocketpp::connection_hdl connection_hdl;

// Forward declarations
struct WebSocketEvent;
struct ConnectionContext;
struct MessageContext;
struct RequestContext;
struct EventHandlerRegistry;

// Event Types for comprehensive callback system
enum class EventType {
    // Connection Events
    CONNECTION_OPENED,
    CONNECTION_CLOSED,
    CONNECTION_FAILED,
    CONNECTION_TIMEOUT,
    CONNECTION_ERROR,
    
    // Message Events  
    MESSAGE_RECEIVED,
    MESSAGE_SENT,
    MESSAGE_FAILED,
    BINARY_MESSAGE_RECEIVED,
    
    // Request Events
    HTTP_UPGRADE_REQUEST,
    AUTHENTICATION_REQUEST,
    AUTHORIZATION_REQUEST,
    VALIDATION_REQUEST,
    
    // Data Events
    DATA_VALIDATION,
    DATA_TRANSFORMATION,
    DATA_PROCESSING,
    DATA_PERSISTENCE,
    
    // System Events
    SERVER_STARTED,
    SERVER_STOPPED,
    HEALTH_CHECK,
    HEARTBEAT,
    PING_RECEIVED,
    PONG_RECEIVED,
    
    // Custom Events
    CUSTOM_EVENT,
    USER_DEFINED_EVENT
};

// Event Priority for processing order
enum class EventPriority {
    CRITICAL = 0,   // Security, authentication
    HIGH = 1,       // Connection management
    NORMAL = 2,     // Regular messages
    LOW = 3         // Background tasks, logging
};

// Event processing result
enum class EventResult {
    SUCCESS,
    FAILED,
    RETRY,
    IGNORE,
    PROPAGATE
};

// Connection information structure
struct ConnectionInfo {
    int connection_id;
    std::string remote_endpoint;
    std::string user_agent;
    std::chrono::steady_clock::time_point connected_at;
    std::chrono::steady_clock::time_point last_activity;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::variant<std::string, int, bool, double>> metadata;
    bool is_authenticated = false;
    std::string session_id;
};

// Message information structure  
struct MessageInfo {
    std::string content;
    std::string message_type;
    std::chrono::steady_clock::time_point timestamp;
    size_t size;
    std::map<std::string, std::string> headers;
    websocketpp::frame::opcode::value opcode;
    bool is_binary = false;
};

// Request information structure
struct RequestInfo {
    std::string path;
    std::string method;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::string body;
    std::string client_ip;
    std::chrono::steady_clock::time_point timestamp;
};

// Event context structures
struct ConnectionContext {
    ConnectionInfo connection;
    connection_hdl handle;
    EventType event_type;
    std::string reason;
    std::map<std::string, std::variant<std::string, int, bool, double>> event_data;
};

struct MessageContext {
    ConnectionInfo connection;
    MessageInfo message;
    EventType event_type;
    std::map<std::string, std::variant<std::string, int, bool, double>> processing_data;
};

struct RequestContext {
    ConnectionInfo connection;
    RequestInfo request;
    EventType event_type;
    std::map<std::string, std::variant<std::string, int, bool, double>> validation_data;
};

struct DataContext {
    ConnectionInfo connection;
    std::string data;
    std::string data_type;
    EventType event_type;
    std::map<std::string, std::variant<std::string, int, bool, double>> processing_metadata;
    std::chrono::steady_clock::time_point processing_start;
};

struct SystemContext {
    EventType event_type;
    std::string component;
    std::map<std::string, std::variant<std::string, int, bool, double>> system_data;
    std::chrono::steady_clock::time_point timestamp;
};

// Generic event structure
struct WebSocketEvent {
    EventType type;
    EventPriority priority;
    std::chrono::steady_clock::time_point timestamp;
    std::variant<ConnectionContext, MessageContext, RequestContext, DataContext, SystemContext> context;
    std::string event_id;
    std::map<std::string, std::variant<std::string, int, bool, double>> metadata;
};

// Callback function type definitions
using ConnectionCallback = std::function<EventResult(const ConnectionContext&)>;
using MessageCallback = std::function<EventResult(const MessageContext&)>;
using RequestCallback = std::function<EventResult(const RequestContext&)>;
using DataCallback = std::function<EventResult(const DataContext&)>;
using SystemCallback = std::function<EventResult(const SystemContext&)>;
using GenericCallback = std::function<EventResult(const WebSocketEvent&)>;

// Event filter for conditional processing
struct EventFilter {
    std::set<EventType> event_types;
    std::set<int> connection_ids;
    std::function<bool(const WebSocketEvent&)> custom_filter;
    EventPriority min_priority = EventPriority::LOW;
    std::chrono::milliseconds max_age{3600000}; // 1 hour default
};

// Callback registration structure
struct CallbackRegistration {
    std::string callback_id;
    EventFilter filter;
    GenericCallback callback;
    bool is_active = true;
    std::chrono::steady_clock::time_point registered_at;
    int execution_count = 0;
    std::chrono::milliseconds total_execution_time{0};
};

class WebSocketHandler {
public:
    WebSocketHandler();
    ~WebSocketHandler();

    // Server lifecycle
    bool start(int port, bool use_tls = false);
    void stop();
    bool isRunning() const { return running_; }

    // ======== CALLBACK REGISTRATION SYSTEM ========
    
    // Register callbacks for specific event types
    std::string registerConnectionCallback(const EventFilter& filter, ConnectionCallback callback);
    std::string registerMessageCallback(const EventFilter& filter, MessageCallback callback);
    std::string registerRequestCallback(const EventFilter& filter, RequestCallback callback);
    std::string registerDataCallback(const EventFilter& filter, DataCallback callback);
    std::string registerSystemCallback(const EventFilter& filter, SystemCallback callback);
    
    // Register generic callback for any event type
    std::string registerCallback(const EventFilter& filter, GenericCallback callback);
    
    // Convenient helper methods for common scenarios
    std::string onConnectionOpened(std::function<EventResult(int connection_id, const ConnectionInfo&)> callback);
    std::string onConnectionClosed(std::function<EventResult(int connection_id, const std::string& reason)> callback);
    std::string onMessageReceived(std::function<EventResult(int connection_id, const std::string& message)> callback);
    std::string onBinaryReceived(std::function<EventResult(int connection_id, const std::vector<uint8_t>& data)> callback);
    std::string onHttpUpgrade(std::function<EventResult(int connection_id, const RequestInfo& request)> callback);
    std::string onAuthentication(std::function<EventResult(int connection_id, const std::string& token)> callback);
    std::string onDataValidation(std::function<EventResult(int connection_id, const std::string& data)> callback);
    std::string onHeartbeat(std::function<EventResult()> callback);
    
    // Callback management
    bool unregisterCallback(const std::string& callback_id);
    void unregisterAllCallbacks();
    std::vector<std::string> getRegisteredCallbacks(EventType event_type = EventType::CUSTOM_EVENT) const;
    void activateCallback(const std::string& callback_id);
    void deactivateCallback(const std::string& callback_id);
    
    // ======== EVENT PROCESSING SYSTEM ========
    
    // Manual event triggering
    void triggerEvent(const WebSocketEvent& event);
    void triggerConnectionEvent(EventType type, int connection_id, const std::string& data = "");
    void triggerMessageEvent(EventType type, int connection_id, const std::string& message);
    void triggerSystemEvent(EventType type, const std::string& component, const std::map<std::string, std::string>& data = {});
    
    // Event processing control
    void setEventProcessingThreads(int thread_count);
    void pauseEventProcessing();
    void resumeEventProcessing();
    bool isEventProcessingActive() const;
    
    // Event queue management
    size_t getEventQueueSize() const;
    void clearEventQueue();
    void setMaxEventQueueSize(size_t max_size);
    
    // ======== MESSAGE HANDLING SYSTEM ========
    
    // Send messages with callback support
    void sendMessage(int connection_id, const std::string& message, 
                    std::function<void(bool success)> callback = nullptr);
    void broadcastMessage(const std::string& message, 
                         std::function<void(std::vector<int> failed_connections)> callback = nullptr);
    void sendToConnections(const std::vector<int>& connection_ids, const std::string& message,
                          std::function<void(std::map<int, bool> results)> callback = nullptr);
    
    // Binary message support
    void sendBinaryMessage(int connection_id, const std::vector<uint8_t>& data,
                          std::function<void(bool success)> callback = nullptr);
    
    // ======== CONNECTION MANAGEMENT ========
    
    // Connection information
    std::vector<int> getActiveConnectionIds() const;
    ConnectionInfo getConnectionInfo(int connection_id) const;
    bool isConnectionAlive(int connection_id) const;
    int getActiveConnectionCount() const;
    
    // Connection control
    void closeConnection(int connection_id, const std::string& reason = "",
                        std::function<void(bool success)> callback = nullptr);
    void setConnectionMetadata(int connection_id, const std::string& key, 
                              const std::variant<std::string, int, bool, double>& value);
    std::variant<std::string, int, bool, double> getConnectionMetadata(int connection_id, const std::string& key) const;
    
    // ======== CONFIGURATION ========
    
    // Debug configuration
    void setDebuggingEnabled(bool enabled);
    void setEventLogging(bool enabled);
    void setPerformanceMetrics(bool enabled);
    
    // Security configuration  
    void setAuthenticationRequired(bool required);
    void setConnectionLimit(int limit);
    void setRateLimit(int messages_per_second, int burst_size = 10);
    
    // Performance tuning
    void setHeartbeatInterval(std::chrono::seconds interval);
    void setConnectionTimeout(std::chrono::seconds timeout);
    void setMessageBufferSize(size_t buffer_size);

    // ======== STATISTICS AND MONITORING ========
    
    struct Statistics {
        std::chrono::steady_clock::time_point server_start_time;
        uint64_t total_connections = 0;
        uint64_t active_connections = 0;
        uint64_t messages_sent = 0;
        uint64_t messages_received = 0;
        uint64_t events_processed = 0;
        uint64_t callbacks_executed = 0;
        std::chrono::milliseconds average_callback_execution_time{0};
        std::map<EventType, uint64_t> event_counts;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    nlohmann::json getStatisticsJson() const;

private:
    // Core server state
    std::atomic<bool> running_;
    std::atomic<int> next_connection_id_;
    bool tls_enabled_;
    
    // WebSocket++ servers
    server ws_server_;
    tls_server tls_ws_server_;
    std::thread server_thread_;
    
    // Connection management
    std::unordered_map<int, connection_hdl> connections_;
    std::map<connection_hdl, int, std::owner_less<connection_hdl>> connection_ids_;
    std::unordered_map<int, ConnectionInfo> connection_info_;
    mutable std::shared_mutex connections_mutex_;
    
    // Event processing system
    std::queue<WebSocketEvent> event_queue_;
    std::vector<std::thread> event_processing_threads_;
    mutable std::mutex event_queue_mutex_;
    std::condition_variable event_queue_condition_;
    std::atomic<bool> event_processing_active_;
    size_t max_event_queue_size_ = 10000;
    int event_processing_thread_count_ = 4;
    
    // Callback registry
    std::unordered_map<std::string, CallbackRegistration> callback_registry_;
    mutable std::shared_mutex callback_registry_mutex_;
    std::atomic<uint64_t> callback_id_counter_;
    
    // Statistics
    mutable Statistics statistics_;
    mutable std::mutex statistics_mutex_;
    
    // Configuration
    bool debug_enabled_ = false;
    bool event_logging_enabled_ = false;
    bool performance_metrics_enabled_ = false;
    bool authentication_required_ = false;
    int connection_limit_ = 1000;
    int rate_limit_messages_per_second_ = 100;
    int rate_limit_burst_size_ = 10;
    std::chrono::seconds heartbeat_interval_{30};
    std::chrono::seconds connection_timeout_{300};
    size_t message_buffer_size_ = 8192;
    
    // WebSocket++ event handlers
    void on_open(connection_hdl hdl);
    void on_close(connection_hdl hdl);
    void on_message(connection_hdl hdl, message_ptr msg);
    void on_http(connection_hdl hdl);
    void on_fail(connection_hdl hdl);
    bool on_validate(connection_hdl hdl);
    bool on_ping(connection_hdl hdl, std::string payload);
    void on_pong(connection_hdl hdl, std::string payload);
    void on_pong_timeout(connection_hdl hdl, std::string payload);
    
    // TLS event handlers
    void on_tls_open(connection_hdl hdl);
    void on_tls_close(connection_hdl hdl);
    void on_tls_message(connection_hdl hdl, tls_server::message_ptr msg);
    void on_tls_http(connection_hdl hdl);
    void on_tls_fail(connection_hdl hdl);
    bool on_tls_validate(connection_hdl hdl);
    bool on_tls_ping(connection_hdl hdl, std::string payload);
    void on_tls_pong(connection_hdl hdl, std::string payload);
    void on_tls_pong_timeout(connection_hdl hdl, std::string payload);
    websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> on_tls_init(connection_hdl hdl);
    
    // Internal event processing
    void processEventQueue();
    void processEvent(const WebSocketEvent& event);
    std::vector<CallbackRegistration*> findMatchingCallbacks(const WebSocketEvent& event);
    bool matchesFilter(const WebSocketEvent& event, const EventFilter& filter) const;
    
    // Utility functions
    std::string generateCallbackId();
    std::string generateEventId();
    ConnectionInfo createConnectionInfo(int connection_id, connection_hdl hdl);
    void updateConnectionActivity(int connection_id);
    void logEvent(const WebSocketEvent& event);
    void updateStatistics(const WebSocketEvent& event, EventResult result, 
                         std::chrono::microseconds execution_time);
    
    // Internal helpers
    void startEventProcessingThreads();
    void stopEventProcessingThreads();
    void cleanupConnection(int connection_id);
};

#endif // WEBSOCKET_HANDLER_H