#include "../include/websocket_handler.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <random>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <thread>
#include <functional>
#include <asio.hpp>

WebSocketHandler::WebSocketHandler() 
    : running_(false), 
      next_connection_id_(1), 
      tls_enabled_(false),
      event_processing_active_(false),
      callback_id_counter_(1) {

    try {
        // Initialize statistics
        statistics_.server_start_time = std::chrono::steady_clock::now();
        
        // Configure non-TLS WebSocket server
        ws_server_.clear_access_channels(websocketpp::log::alevel::all);
        ws_server_.set_access_channels(websocketpp::log::alevel::connect);
        ws_server_.set_access_channels(websocketpp::log::alevel::disconnect);
        ws_server_.set_access_channels(websocketpp::log::alevel::app);

        ws_server_.clear_error_channels(websocketpp::log::elevel::all);
        ws_server_.set_error_channels(websocketpp::log::elevel::rerror);
        ws_server_.set_error_channels(websocketpp::log::elevel::fatal);

        ws_server_.init_asio();
        ws_server_.set_reuse_addr(true);

        // Set WebSocket++ event handlers
        ws_server_.set_open_handler(std::bind(&WebSocketHandler::on_open, this, std::placeholders::_1));
        ws_server_.set_close_handler(std::bind(&WebSocketHandler::on_close, this, std::placeholders::_1));
        ws_server_.set_message_handler(std::bind(&WebSocketHandler::on_message, this, std::placeholders::_1, std::placeholders::_2));
        ws_server_.set_http_handler(std::bind(&WebSocketHandler::on_http, this, std::placeholders::_1));
        ws_server_.set_fail_handler(std::bind(&WebSocketHandler::on_fail, this, std::placeholders::_1));
        ws_server_.set_validate_handler(std::bind(&WebSocketHandler::on_validate, this, std::placeholders::_1));
        ws_server_.set_ping_handler(std::bind(&WebSocketHandler::on_ping, this, std::placeholders::_1, std::placeholders::_2));
        ws_server_.set_pong_handler(std::bind(&WebSocketHandler::on_pong, this, std::placeholders::_1, std::placeholders::_2));
        ws_server_.set_pong_timeout_handler(std::bind(&WebSocketHandler::on_pong_timeout, this, std::placeholders::_1, std::placeholders::_2));

        // Configure TLS WebSocket server
        tls_ws_server_.clear_access_channels(websocketpp::log::alevel::all);
        tls_ws_server_.set_access_channels(websocketpp::log::alevel::connect);
        tls_ws_server_.set_access_channels(websocketpp::log::alevel::disconnect);
        tls_ws_server_.set_access_channels(websocketpp::log::alevel::app);

        tls_ws_server_.clear_error_channels(websocketpp::log::elevel::all);
        tls_ws_server_.set_error_channels(websocketpp::log::elevel::rerror);
        tls_ws_server_.set_error_channels(websocketpp::log::elevel::fatal);

        tls_ws_server_.init_asio();
        tls_ws_server_.set_reuse_addr(true);

        // Set TLS event handlers
        tls_ws_server_.set_open_handler(std::bind(&WebSocketHandler::on_tls_open, this, std::placeholders::_1));
        tls_ws_server_.set_close_handler(std::bind(&WebSocketHandler::on_tls_close, this, std::placeholders::_1));
        tls_ws_server_.set_message_handler(std::bind(&WebSocketHandler::on_tls_message, this, std::placeholders::_1, std::placeholders::_2));
        tls_ws_server_.set_http_handler(std::bind(&WebSocketHandler::on_tls_http, this, std::placeholders::_1));
        tls_ws_server_.set_fail_handler(std::bind(&WebSocketHandler::on_tls_fail, this, std::placeholders::_1));
        tls_ws_server_.set_validate_handler(std::bind(&WebSocketHandler::on_tls_validate, this, std::placeholders::_1));
        tls_ws_server_.set_tls_init_handler(std::bind(&WebSocketHandler::on_tls_init, this, std::placeholders::_1));
        tls_ws_server_.set_ping_handler(std::bind(&WebSocketHandler::on_tls_ping, this, std::placeholders::_1, std::placeholders::_2));
        tls_ws_server_.set_pong_handler(std::bind(&WebSocketHandler::on_tls_pong, this, std::placeholders::_1, std::placeholders::_2));
        tls_ws_server_.set_pong_timeout_handler(std::bind(&WebSocketHandler::on_tls_pong_timeout, this, std::placeholders::_1, std::placeholders::_2));

        std::cout << "WebSocketHandler initialized with callback-based event system" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "WebSocketHandler initialization failed: " << e.what() << std::endl;
        throw;
    }
}

WebSocketHandler::~WebSocketHandler() {
    stop();
}

// ======== SERVER LIFECYCLE ========

bool WebSocketHandler::start(int port, bool use_tls) {
    if (running_.load()) {
        std::cerr << "WebSocket server is already running" << std::endl;
        return false;
    }

    tls_enabled_ = use_tls;
    
    try {
        std::cout << "Starting WebSocket server on port " << port 
                  << " (TLS: " << (use_tls ? "enabled" : "disabled") << ")" << std::endl;

        websocketpp::lib::error_code ec;

        if (use_tls) {
            tls_ws_server_.listen(asio::ip::tcp::endpoint(
                asio::ip::address::from_string("0.0.0.0"), port), ec);
            if (ec) {
                std::cerr << "TLS WebSocket listen failed: " << ec.message() << std::endl;
                return false;
            }

            tls_ws_server_.start_accept(ec);
            if (ec) {
                std::cerr << "TLS WebSocket start_accept failed: " << ec.message() << std::endl;
                return false;
            }
        } else {
            ws_server_.listen(asio::ip::tcp::endpoint(
                asio::ip::address::from_string("0.0.0.0"), port), ec);
            if (ec) {
                std::cerr << "WebSocket listen failed: " << ec.message() << std::endl;
                return false;
            }

            ws_server_.start_accept(ec);
            if (ec) {
                std::cerr << "WebSocket start_accept failed: " << ec.message() << std::endl;
                return false;
            }
        }

        running_ = true;
        event_processing_active_ = true;

        // Start event processing threads
        startEventProcessingThreads();

        // Start the WebSocket server thread
        server_thread_ = std::thread([this]() {
            try {
                std::cout << "WebSocket server thread started" << std::endl;
                if (tls_enabled_) {
                    tls_ws_server_.run();
                } else {
                    ws_server_.run();
                }
                std::cout << "WebSocket server thread finished" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "WebSocket server thread exception: " << e.what() << std::endl;
            }
        });

        // Trigger server started event
        triggerSystemEvent(EventType::SERVER_STARTED, "WebSocketHandler", {
            {"port", std::to_string(port)},
            {"tls_enabled", use_tls ? "true" : "false"}
        });

        std::cout << "WebSocket server started successfully on port " << port << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "WebSocket server start exception: " << e.what() << std::endl;
        return false;
    }
}

void WebSocketHandler::stop() {
    if (!running_.load()) {
        return;
    }

    std::cout << "Stopping WebSocket server..." << std::endl;
    
    running_ = false;
    event_processing_active_ = false;

    // Stop the WebSocket servers
    try {
        if (tls_enabled_) {
            tls_ws_server_.stop();
        } else {
            ws_server_.stop();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error stopping WebSocket server: " << e.what() << std::endl;
    }

    // Stop event processing threads
    stopEventProcessingThreads();

    // Wait for server thread to finish
    if (server_thread_.joinable()) {
        server_thread_.join();
    }

    // Clear connections
    {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_.clear();
        connection_ids_.clear();
        connection_info_.clear();
    }

    // Trigger server stopped event
    triggerSystemEvent(EventType::SERVER_STOPPED, "WebSocketHandler", {});

    std::cout << "WebSocket server stopped" << std::endl;
}

// isRunning() is defined inline in header file

// ======== CALLBACK REGISTRATION SYSTEM ========

std::string WebSocketHandler::registerCallback(const EventFilter& filter, GenericCallback callback) {
    std::string callback_id = generateCallbackId();
    
    CallbackRegistration registration;
    registration.callback_id = callback_id;
    registration.filter = filter;
    registration.callback = callback;
    registration.registered_at = std::chrono::steady_clock::now();
    
    {
        std::unique_lock<std::shared_mutex> lock(callback_registry_mutex_);
        callback_registry_[callback_id] = registration;
    }
    
    if (debug_enabled_) {
        std::cout << "Registered callback: " << callback_id << std::endl;
    }
    
    return callback_id;
}

std::string WebSocketHandler::registerConnectionCallback(const EventFilter& filter, ConnectionCallback callback) {
    return registerCallback(filter, [callback](const WebSocketEvent& event) -> EventResult {
        if (std::holds_alternative<ConnectionContext>(event.context)) {
            return callback(std::get<ConnectionContext>(event.context));
        }
        return EventResult::IGNORE;
    });
}

std::string WebSocketHandler::registerMessageCallback(const EventFilter& filter, MessageCallback callback) {
    return registerCallback(filter, [callback](const WebSocketEvent& event) -> EventResult {
        if (std::holds_alternative<MessageContext>(event.context)) {
            return callback(std::get<MessageContext>(event.context));
        }
        return EventResult::IGNORE;
    });
}

std::string WebSocketHandler::registerRequestCallback(const EventFilter& filter, RequestCallback callback) {
    return registerCallback(filter, [callback](const WebSocketEvent& event) -> EventResult {
        if (std::holds_alternative<RequestContext>(event.context)) {
            return callback(std::get<RequestContext>(event.context));
        }
        return EventResult::IGNORE;
    });
}

std::string WebSocketHandler::registerDataCallback(const EventFilter& filter, DataCallback callback) {
    return registerCallback(filter, [callback](const WebSocketEvent& event) -> EventResult {
        if (std::holds_alternative<DataContext>(event.context)) {
            return callback(std::get<DataContext>(event.context));
        }
        return EventResult::IGNORE;
    });
}

std::string WebSocketHandler::registerSystemCallback(const EventFilter& filter, SystemCallback callback) {
    return registerCallback(filter, [callback](const WebSocketEvent& event) -> EventResult {
        if (std::holds_alternative<SystemContext>(event.context)) {
            return callback(std::get<SystemContext>(event.context));
        }
        return EventResult::IGNORE;
    });
}

// Convenient helper methods
std::string WebSocketHandler::onConnectionOpened(std::function<EventResult(int connection_id, const ConnectionInfo&)> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::CONNECTION_OPENED);
    
    return registerConnectionCallback(filter, [callback](const ConnectionContext& ctx) -> EventResult {
        return callback(ctx.connection.connection_id, ctx.connection);
    });
}

std::string WebSocketHandler::onConnectionClosed(std::function<EventResult(int connection_id, const std::string& reason)> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::CONNECTION_CLOSED);
    
    return registerConnectionCallback(filter, [callback](const ConnectionContext& ctx) -> EventResult {
        return callback(ctx.connection.connection_id, ctx.reason);
    });
}

std::string WebSocketHandler::onMessageReceived(std::function<EventResult(int connection_id, const std::string& message)> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::MESSAGE_RECEIVED);
    
    return registerMessageCallback(filter, [callback](const MessageContext& ctx) -> EventResult {
        return callback(ctx.connection.connection_id, ctx.message.content);
    });
}

std::string WebSocketHandler::onBinaryReceived(std::function<EventResult(int connection_id, const std::vector<uint8_t>& data)> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::BINARY_MESSAGE_RECEIVED);
    
    return registerMessageCallback(filter, [callback](const MessageContext& ctx) -> EventResult {
        std::vector<uint8_t> data(ctx.message.content.begin(), ctx.message.content.end());
        return callback(ctx.connection.connection_id, data);
    });
}

std::string WebSocketHandler::onHttpUpgrade(std::function<EventResult(int connection_id, const RequestInfo& request)> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::HTTP_UPGRADE_REQUEST);
    
    return registerRequestCallback(filter, [callback](const RequestContext& ctx) -> EventResult {
        return callback(ctx.connection.connection_id, ctx.request);
    });
}

std::string WebSocketHandler::onAuthentication(std::function<EventResult(int connection_id, const std::string& token)> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::AUTHENTICATION_REQUEST);
    
    return registerRequestCallback(filter, [callback](const RequestContext& ctx) -> EventResult {
        auto it = ctx.request.headers.find("Authorization");
        std::string token = (it != ctx.request.headers.end()) ? it->second : "";
        return callback(ctx.connection.connection_id, token);
    });
}

std::string WebSocketHandler::onDataValidation(std::function<EventResult(int connection_id, const std::string& data)> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::DATA_VALIDATION);
    
    return registerDataCallback(filter, [callback](const DataContext& ctx) -> EventResult {
        return callback(ctx.connection.connection_id, ctx.data);
    });
}

std::string WebSocketHandler::onHeartbeat(std::function<EventResult()> callback) {
    EventFilter filter;
    filter.event_types.insert(EventType::HEARTBEAT);
    
    return registerSystemCallback(filter, [callback](const SystemContext& ctx) -> EventResult {
        return callback();
    });
}

bool WebSocketHandler::unregisterCallback(const std::string& callback_id) {
    std::unique_lock<std::shared_mutex> lock(callback_registry_mutex_);
    auto it = callback_registry_.find(callback_id);
    if (it != callback_registry_.end()) {
        callback_registry_.erase(it);
        if (debug_enabled_) {
            std::cout << "Unregistered callback: " << callback_id << std::endl;
        }
        return true;
    }
    return false;
}

void WebSocketHandler::unregisterAllCallbacks() {
    std::unique_lock<std::shared_mutex> lock(callback_registry_mutex_);
    callback_registry_.clear();
    if (debug_enabled_) {
        std::cout << "Unregistered all callbacks" << std::endl;
    }
}

std::vector<std::string> WebSocketHandler::getRegisteredCallbacks(EventType event_type) const {
    std::shared_lock<std::shared_mutex> lock(callback_registry_mutex_);
    std::vector<std::string> result;
    
    for (const auto& [callback_id, registration] : callback_registry_) {
        if (event_type == EventType::CUSTOM_EVENT || 
            registration.filter.event_types.find(event_type) != registration.filter.event_types.end()) {
            result.push_back(callback_id);
        }
    }
    
    return result;
}

void WebSocketHandler::activateCallback(const std::string& callback_id) {
    std::unique_lock<std::shared_mutex> lock(callback_registry_mutex_);
    auto it = callback_registry_.find(callback_id);
    if (it != callback_registry_.end()) {
        it->second.is_active = true;
    }
}

void WebSocketHandler::deactivateCallback(const std::string& callback_id) {
    std::unique_lock<std::shared_mutex> lock(callback_registry_mutex_);
    auto it = callback_registry_.find(callback_id);
    if (it != callback_registry_.end()) {
        it->second.is_active = false;
    }
}

// ======== EVENT PROCESSING SYSTEM ========

void WebSocketHandler::triggerEvent(const WebSocketEvent& event) {
    if (!event_processing_active_.load()) {
        return;
    }

    std::unique_lock<std::mutex> lock(event_queue_mutex_);
    
    if (event_queue_.size() >= max_event_queue_size_) {
        std::cerr << "Event queue full, dropping oldest event" << std::endl;
        event_queue_.pop();
    }
    
    event_queue_.push(event);
    lock.unlock();
    
    event_queue_condition_.notify_one();
    
    if (event_logging_enabled_) {
        logEvent(event);
    }
}

void WebSocketHandler::triggerConnectionEvent(EventType type, int connection_id, const std::string& data) {
    WebSocketEvent event;
    event.type = type;
    event.priority = EventPriority::HIGH;
    event.timestamp = std::chrono::steady_clock::now();
    event.event_id = generateEventId();
    
    ConnectionContext ctx;
    {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connection_info_.find(connection_id);
        if (it != connection_info_.end()) {
            ctx.connection = it->second;
        } else {
            ctx.connection.connection_id = connection_id;
        }
        
        auto handle_it = connections_.find(connection_id);
        if (handle_it != connections_.end()) {
            ctx.handle = handle_it->second;
        }
    }
    
    ctx.event_type = type;
    ctx.reason = data;
    event.context = ctx;
    
    triggerEvent(event);
}

void WebSocketHandler::triggerMessageEvent(EventType type, int connection_id, const std::string& message) {
    WebSocketEvent event;
    event.type = type;
    event.priority = EventPriority::NORMAL;
    event.timestamp = std::chrono::steady_clock::now();
    event.event_id = generateEventId();
    
    MessageContext ctx;
    {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connection_info_.find(connection_id);
        if (it != connection_info_.end()) {
            ctx.connection = it->second;
        } else {
            ctx.connection.connection_id = connection_id;
        }
    }
    
    ctx.message.content = message;
    ctx.message.timestamp = std::chrono::steady_clock::now();
    ctx.message.size = message.size();
    ctx.event_type = type;
    event.context = ctx;
    
    triggerEvent(event);
}

void WebSocketHandler::triggerSystemEvent(EventType type, const std::string& component, const std::map<std::string, std::string>& data) {
    WebSocketEvent event;
    event.type = type;
    event.priority = EventPriority::LOW;
    event.timestamp = std::chrono::steady_clock::now();
    event.event_id = generateEventId();
    
    SystemContext ctx;
    ctx.event_type = type;
    ctx.component = component;
    ctx.timestamp = std::chrono::steady_clock::now();
    
    for (const auto& [key, value] : data) {
        ctx.system_data[key] = value;
    }
    
    event.context = ctx;
    triggerEvent(event);
}

void WebSocketHandler::setEventProcessingThreads(int thread_count) {
    if (thread_count < 1 || thread_count > 32) {
        std::cerr << "Invalid thread count: " << thread_count << std::endl;
        return;
    }
    
    event_processing_thread_count_ = thread_count;
    
    if (event_processing_active_.load()) {
        stopEventProcessingThreads();
        startEventProcessingThreads();
    }
}

void WebSocketHandler::pauseEventProcessing() {
    event_processing_active_ = false;
    std::cout << "Event processing paused" << std::endl;
}

void WebSocketHandler::resumeEventProcessing() {
    event_processing_active_ = true;
    event_queue_condition_.notify_all();
    std::cout << "Event processing resumed" << std::endl;
}

bool WebSocketHandler::isEventProcessingActive() const {
    return event_processing_active_.load();
}

size_t WebSocketHandler::getEventQueueSize() const {
    std::lock_guard<std::mutex> lock(event_queue_mutex_);
    return event_queue_.size();
}

void WebSocketHandler::clearEventQueue() {
    std::lock_guard<std::mutex> lock(event_queue_mutex_);
    std::queue<WebSocketEvent> empty_queue;
    event_queue_.swap(empty_queue);
    std::cout << "Event queue cleared" << std::endl;
}

void WebSocketHandler::setMaxEventQueueSize(size_t max_size) {
    max_event_queue_size_ = max_size;
}

// ======== MESSAGE HANDLING SYSTEM ========

void WebSocketHandler::sendMessage(int connection_id, const std::string& message, std::function<void(bool success)> callback) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it == connections_.end()) {
            if (callback) callback(false);
            return;
        }

        websocketpp::lib::error_code ec;
        
        if (tls_enabled_) {
            tls_ws_server_.send(it->second, message, websocketpp::frame::opcode::text, ec);
        } else {
            ws_server_.send(it->second, message, websocketpp::frame::opcode::text, ec);
        }

        bool success = !ec;
        
        if (success) {
            std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
            statistics_.messages_sent++;
            updateConnectionActivity(connection_id);
            triggerMessageEvent(EventType::MESSAGE_SENT, connection_id, message);
        } else {
            std::cerr << "Failed to send message to connection " << connection_id 
                      << ": " << ec.message() << std::endl;
            triggerMessageEvent(EventType::MESSAGE_FAILED, connection_id, message);
        }

        if (callback) callback(success);
        
    } catch (const std::exception& e) {
        std::cerr << "Exception sending message: " << e.what() << std::endl;
        if (callback) callback(false);
    }
}

void WebSocketHandler::broadcastMessage(const std::string& message, std::function<void(std::vector<int> failed_connections)> callback) {
    std::vector<int> failed_connections;
    std::vector<int> connection_ids;
    
    {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        for (const auto& [conn_id, handle] : connections_) {
            connection_ids.push_back(conn_id);
        }
    }
    
    for (int connection_id : connection_ids) {
        sendMessage(connection_id, message, [&failed_connections, connection_id](bool success) {
            if (!success) {
                failed_connections.push_back(connection_id);
            }
        });
    }
    
    if (callback) callback(failed_connections);
}

void WebSocketHandler::sendToConnections(const std::vector<int>& connection_ids, const std::string& message,
                                        std::function<void(std::map<int, bool> results)> callback) {
    std::map<int, bool> results;
    
    for (int connection_id : connection_ids) {
        sendMessage(connection_id, message, [&results, connection_id](bool success) {
            results[connection_id] = success;
        });
    }
    
    if (callback) callback(results);
}

void WebSocketHandler::sendBinaryMessage(int connection_id, const std::vector<uint8_t>& data,
                                        std::function<void(bool success)> callback) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it == connections_.end()) {
            if (callback) callback(false);
            return;
        }

        std::string binary_string(data.begin(), data.end());
        websocketpp::lib::error_code ec;
        
        if (tls_enabled_) {
            tls_ws_server_.send(it->second, binary_string, websocketpp::frame::opcode::binary, ec);
        } else {
            ws_server_.send(it->second, binary_string, websocketpp::frame::opcode::binary, ec);
        }

        bool success = !ec;
        
        if (success) {
            std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
            statistics_.messages_sent++;
            updateConnectionActivity(connection_id);
        } else {
            std::cerr << "Failed to send binary message to connection " << connection_id 
                      << ": " << ec.message() << std::endl;
        }

        if (callback) callback(success);
        
    } catch (const std::exception& e) {
        std::cerr << "Exception sending binary message: " << e.what() << std::endl;
        if (callback) callback(false);
    }
}

// ======== CONNECTION MANAGEMENT ========

std::vector<int> WebSocketHandler::getActiveConnectionIds() const {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    std::vector<int> ids;
    for (const auto& [conn_id, handle] : connections_) {
        ids.push_back(conn_id);
    }
    return ids;
}

ConnectionInfo WebSocketHandler::getConnectionInfo(int connection_id) const {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    auto it = connection_info_.find(connection_id);
    if (it != connection_info_.end()) {
        return it->second;
    }
    return ConnectionInfo{}; // Return empty info if not found
}

bool WebSocketHandler::isConnectionAlive(int connection_id) const {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    return connections_.find(connection_id) != connections_.end();
}

int WebSocketHandler::getActiveConnectionCount() const {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    return static_cast<int>(connections_.size());
}

void WebSocketHandler::closeConnection(int connection_id, const std::string& reason,
                                      std::function<void(bool success)> callback) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        auto it = connections_.find(connection_id);
        if (it == connections_.end()) {
            if (callback) callback(false);
            return;
        }

        websocketpp::lib::error_code ec;
        
        if (tls_enabled_) {
            tls_ws_server_.close(it->second, websocketpp::close::status::going_away, reason, ec);
        } else {
            ws_server_.close(it->second, websocketpp::close::status::going_away, reason, ec);
        }

        bool success = !ec;
        if (callback) callback(success);
        
    } catch (const std::exception& e) {
        std::cerr << "Exception closing connection: " << e.what() << std::endl;
        if (callback) callback(false);
    }
}

void WebSocketHandler::setConnectionMetadata(int connection_id, const std::string& key, 
                                            const std::variant<std::string, int, bool, double>& value) {
    std::unique_lock<std::shared_mutex> lock(connections_mutex_);
    auto it = connection_info_.find(connection_id);
    if (it != connection_info_.end()) {
        it->second.metadata[key] = value;
    }
}

std::variant<std::string, int, bool, double> WebSocketHandler::getConnectionMetadata(int connection_id, const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(connections_mutex_);
    auto it = connection_info_.find(connection_id);
    if (it != connection_info_.end()) {
        auto meta_it = it->second.metadata.find(key);
        if (meta_it != it->second.metadata.end()) {
            return meta_it->second;
        }
    }
    return std::string(""); // Default value
}

// ======== CONFIGURATION ========

void WebSocketHandler::setDebuggingEnabled(bool enabled) {
    debug_enabled_ = enabled;
    std::cout << "Debug mode " << (enabled ? "enabled" : "disabled") << std::endl;
}

void WebSocketHandler::setEventLogging(bool enabled) {
    event_logging_enabled_ = enabled;
    std::cout << "Event logging " << (enabled ? "enabled" : "disabled") << std::endl;
}

void WebSocketHandler::setPerformanceMetrics(bool enabled) {
    performance_metrics_enabled_ = enabled;
    std::cout << "Performance metrics " << (enabled ? "enabled" : "disabled") << std::endl;
}

void WebSocketHandler::setAuthenticationRequired(bool required) {
    authentication_required_ = required;
    std::cout << "Authentication " << (required ? "required" : "optional") << std::endl;
}

void WebSocketHandler::setConnectionLimit(int limit) {
    connection_limit_ = limit;
    std::cout << "Connection limit set to " << limit << std::endl;
}

void WebSocketHandler::setRateLimit(int messages_per_second, int burst_size) {
    rate_limit_messages_per_second_ = messages_per_second;
    rate_limit_burst_size_ = burst_size;
    std::cout << "Rate limit set to " << messages_per_second << " msg/s, burst: " << burst_size << std::endl;
}

void WebSocketHandler::setHeartbeatInterval(std::chrono::seconds interval) {
    heartbeat_interval_ = interval;
    std::cout << "Heartbeat interval set to " << interval.count() << " seconds" << std::endl;
}

void WebSocketHandler::setConnectionTimeout(std::chrono::seconds timeout) {
    connection_timeout_ = timeout;
    std::cout << "Connection timeout set to " << timeout.count() << " seconds" << std::endl;
}

void WebSocketHandler::setMessageBufferSize(size_t buffer_size) {
    message_buffer_size_ = buffer_size;
    std::cout << "Message buffer size set to " << buffer_size << " bytes" << std::endl;
}

// ======== STATISTICS AND MONITORING ========

WebSocketHandler::Statistics WebSocketHandler::getStatistics() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return statistics_;
}

void WebSocketHandler::resetStatistics() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    statistics_ = Statistics();
    statistics_.server_start_time = std::chrono::steady_clock::now();
    std::cout << "Statistics reset" << std::endl;
}

nlohmann::json WebSocketHandler::getStatisticsJson() const {
    auto stats = getStatistics();
    
    nlohmann::json json_stats;
    json_stats["server_start_time"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        stats.server_start_time.time_since_epoch()).count();
    json_stats["total_connections"] = stats.total_connections;
    json_stats["active_connections"] = stats.active_connections;
    json_stats["messages_sent"] = stats.messages_sent;
    json_stats["messages_received"] = stats.messages_received;
    json_stats["events_processed"] = stats.events_processed;
    json_stats["callbacks_executed"] = stats.callbacks_executed;
    json_stats["average_callback_execution_time_ms"] = stats.average_callback_execution_time.count();
    
    nlohmann::json event_counts;
    for (const auto& [event_type, count] : stats.event_counts) {
        event_counts[std::to_string(static_cast<int>(event_type))] = count;
    }
    json_stats["event_counts"] = event_counts;
    
    return json_stats;
}

// ======== WebSocket++ EVENT HANDLERS ========

void WebSocketHandler::on_open(connection_hdl hdl) {
    try {
        int connection_id = next_connection_id_.fetch_add(1);
        
        {
            std::unique_lock<std::shared_mutex> lock(connections_mutex_);
            connections_[connection_id] = hdl;
            connection_ids_[hdl] = connection_id;
            connection_info_[connection_id] = createConnectionInfo(connection_id, hdl);
            
            std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
            statistics_.total_connections++;
            statistics_.active_connections++;
        }
        
        updateConnectionActivity(connection_id);
        triggerConnectionEvent(EventType::CONNECTION_OPENED, connection_id, "");
        
        if (debug_enabled_) {
            std::cout << "Connection opened: " << connection_id << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_open: " << e.what() << std::endl;
    }
}

void WebSocketHandler::on_close(connection_hdl hdl) {
    try {
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            
            // Get close information
            std::string close_reason;
            try {
                server::connection_ptr con = ws_server_.get_con_from_hdl(hdl);
                if (con) {
                    close_reason = con->get_remote_close_reason();
                }
            } catch (...) {
                try {
                    tls_server::connection_ptr tls_con = tls_ws_server_.get_con_from_hdl(hdl);
                    if (tls_con) {
                        close_reason = tls_con->get_remote_close_reason();
                    }
                } catch (...) {
                    close_reason = "Unknown reason";
                }
            }
            
            connections_.erase(connection_id);
            connection_ids_.erase(it);
            connection_info_.erase(connection_id);
            
            {
                std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
                if (statistics_.active_connections > 0) {
                    statistics_.active_connections--;
                }
            }
            
            lock.unlock();
            
            triggerConnectionEvent(EventType::CONNECTION_CLOSED, connection_id, close_reason);
            
            if (debug_enabled_) {
                std::cout << "Connection closed: " << connection_id 
                          << " Reason: " << close_reason << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_close: " << e.what() << std::endl;
    }
}

void WebSocketHandler::on_message(connection_hdl hdl, message_ptr msg) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            lock.unlock();
            
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {
                std::string payload = msg->get_payload();
                updateConnectionActivity(connection_id);
                
                {
                    std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
                    statistics_.messages_received++;
                }
                
                triggerMessageEvent(EventType::MESSAGE_RECEIVED, connection_id, payload);
                
                if (debug_enabled_) {
                    std::cout << "Message received from connection " << connection_id 
                              << ": " << payload.substr(0, 100) << std::endl;
                }
            } else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                std::string payload = msg->get_payload();
                updateConnectionActivity(connection_id);
                
                {
                    std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
                    statistics_.messages_received++;
                }
                
                triggerMessageEvent(EventType::BINARY_MESSAGE_RECEIVED, connection_id, payload);
                
                if (debug_enabled_) {
                    std::cout << "Binary message received from connection " << connection_id 
                              << " (" << payload.size() << " bytes)" << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_message: " << e.what() << std::endl;
    }
}

void WebSocketHandler::on_http(connection_hdl hdl) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            lock.unlock();
            
            // Create request info for HTTP upgrade
            RequestInfo request_info;
            request_info.path = "/"; // Default path
            request_info.method = "GET";
            request_info.timestamp = std::chrono::steady_clock::now();
            
            // Try to get request details
            try {
                server::connection_ptr con = ws_server_.get_con_from_hdl(hdl);
                if (con) {
                    request_info.path = con->get_uri()->get_resource();
                    // Add more request details as needed
                }
            } catch (...) {
                // Try TLS connection if regular connection fails
                try {
                    tls_server::connection_ptr tls_con = tls_ws_server_.get_con_from_hdl(hdl);
                    if (tls_con) {
                        request_info.path = tls_con->get_uri()->get_resource();
                    }
                } catch (...) {
                    // Keep default values
                }
            }
            
            WebSocketEvent event;
            event.type = EventType::HTTP_UPGRADE_REQUEST;
            event.priority = EventPriority::HIGH;
            event.timestamp = std::chrono::steady_clock::now();
            event.event_id = generateEventId();
            
            RequestContext ctx;
            {
                std::shared_lock<std::shared_mutex> lock2(connections_mutex_);
                auto info_it = connection_info_.find(connection_id);
                if (info_it != connection_info_.end()) {
                    ctx.connection = info_it->second;
                }
            }
            ctx.request = request_info;
            ctx.event_type = EventType::HTTP_UPGRADE_REQUEST;
            event.context = ctx;
            
            triggerEvent(event);
            
            if (debug_enabled_) {
                std::cout << "HTTP upgrade request from connection " << connection_id << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_http: " << e.what() << std::endl;
    }
}

void WebSocketHandler::on_fail(connection_hdl hdl) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            lock.unlock();
            
            triggerConnectionEvent(EventType::CONNECTION_FAILED, connection_id, "Connection failed");
            
            if (debug_enabled_) {
                std::cout << "Connection failed: " << connection_id << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_fail: " << e.what() << std::endl;
    }
}

bool WebSocketHandler::on_validate(connection_hdl hdl) {
    try {
        // Default validation - can be overridden by callbacks
        if (authentication_required_) {
            // Trigger authentication event and check result
            // For now, return true - actual validation would be done in callbacks
            return true;
        }
        
        // Check connection limit
        if (getActiveConnectionCount() >= connection_limit_) {
            return false;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_validate: " << e.what() << std::endl;
        return false;
    }
}

bool WebSocketHandler::on_ping(connection_hdl hdl, std::string payload) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            lock.unlock();
            
            updateConnectionActivity(connection_id);
            triggerSystemEvent(EventType::PING_RECEIVED, "WebSocketHandler", {
                {"connection_id", std::to_string(connection_id)},
                {"payload", payload}
            });
            
            if (debug_enabled_) {
                std::cout << "Ping received from connection " << connection_id << std::endl;
            }
        }
        
        return true; // Allow ping
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_ping: " << e.what() << std::endl;
        return false;
    }
}

void WebSocketHandler::on_pong(connection_hdl hdl, std::string payload) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            lock.unlock();
            
            updateConnectionActivity(connection_id);
            triggerSystemEvent(EventType::PONG_RECEIVED, "WebSocketHandler", {
                {"connection_id", std::to_string(connection_id)},
                {"payload", payload}
            });
            
            if (debug_enabled_) {
                std::cout << "Pong received from connection " << connection_id << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_pong: " << e.what() << std::endl;
    }
}

void WebSocketHandler::on_pong_timeout(connection_hdl hdl, std::string payload) {
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            lock.unlock();
            
            triggerConnectionEvent(EventType::CONNECTION_TIMEOUT, connection_id, "Pong timeout");
            
            if (debug_enabled_) {
                std::cout << "Pong timeout from connection " << connection_id << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_pong_timeout: " << e.what() << std::endl;
    }
}

// TLS Event handlers (similar implementations)
void WebSocketHandler::on_tls_open(connection_hdl hdl) { on_open(hdl); }
void WebSocketHandler::on_tls_close(connection_hdl hdl) { on_close(hdl); }
void WebSocketHandler::on_tls_message(connection_hdl hdl, tls_server::message_ptr msg) {
    // Handle TLS message directly without conversion
    try {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        
        auto it = connection_ids_.find(hdl);
        if (it != connection_ids_.end()) {
            int connection_id = it->second;
            lock.unlock();
            
            if (msg->get_opcode() == websocketpp::frame::opcode::text) {
                std::string payload = msg->get_payload();
                updateConnectionActivity(connection_id);
                
                {
                    std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
                    statistics_.messages_received++;
                }
                
                triggerMessageEvent(EventType::MESSAGE_RECEIVED, connection_id, payload);
                
                if (debug_enabled_) {
                    std::cout << "TLS Message received from connection " << connection_id 
                              << ": " << payload.substr(0, 100) << std::endl;
                }
            } else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
                std::string payload = msg->get_payload();
                updateConnectionActivity(connection_id);
                
                {
                    std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
                    statistics_.messages_received++;
                }
                
                triggerMessageEvent(EventType::BINARY_MESSAGE_RECEIVED, connection_id, payload);
                
                if (debug_enabled_) {
                    std::cout << "TLS Binary message received from connection " << connection_id 
                              << " (" << payload.size() << " bytes)" << std::endl;
                }
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in on_tls_message: " << e.what() << std::endl;
    }
}
void WebSocketHandler::on_tls_http(connection_hdl hdl) { on_http(hdl); }
void WebSocketHandler::on_tls_fail(connection_hdl hdl) { on_fail(hdl); }
bool WebSocketHandler::on_tls_validate(connection_hdl hdl) { return on_validate(hdl); }
bool WebSocketHandler::on_tls_ping(connection_hdl hdl, std::string payload) { return on_ping(hdl, payload); }
void WebSocketHandler::on_tls_pong(connection_hdl hdl, std::string payload) { on_pong(hdl, payload); }
void WebSocketHandler::on_tls_pong_timeout(connection_hdl hdl, std::string payload) { on_pong_timeout(hdl, payload); }

websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> 
WebSocketHandler::on_tls_init(connection_hdl hdl) {
    // Create a basic SSL context - this should be configured properly for production
    auto ctx = websocketpp::lib::make_shared<websocketpp::lib::asio::ssl::context>(websocketpp::lib::asio::ssl::context::tlsv12);
    
    try {
        ctx->set_options(websocketpp::lib::asio::ssl::context::default_workarounds |
                        websocketpp::lib::asio::ssl::context::no_sslv2 |
                        websocketpp::lib::asio::ssl::context::no_sslv3 |
                        websocketpp::lib::asio::ssl::context::single_dh_use);
    } catch (std::exception& e) {
        std::cerr << "TLS initialization error: " << e.what() << std::endl;
    }
    
    return ctx;
}

// ======== INTERNAL EVENT PROCESSING ========

void WebSocketHandler::processEventQueue() {
    while (event_processing_active_.load()) {
        std::unique_lock<std::mutex> lock(event_queue_mutex_);
        
        event_queue_condition_.wait(lock, [this] { 
            return !event_queue_.empty() || !event_processing_active_.load(); 
        });
        
        if (!event_processing_active_.load()) {
            break;
        }
        
        if (!event_queue_.empty()) {
            WebSocketEvent event = event_queue_.front();
            event_queue_.pop();
            lock.unlock();
            
            processEvent(event);
        }
    }
}

void WebSocketHandler::processEvent(const WebSocketEvent& event) {
    try {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        auto matching_callbacks = findMatchingCallbacks(event);
        
        for (CallbackRegistration* callback_reg : matching_callbacks) {
            if (!callback_reg->is_active) {
                continue;
            }
            
            try {
                auto callback_start = std::chrono::high_resolution_clock::now();
                EventResult result = callback_reg->callback(event);
                auto callback_end = std::chrono::high_resolution_clock::now();
                
                auto execution_time = std::chrono::duration_cast<std::chrono::microseconds>(
                    callback_end - callback_start);
                
                callback_reg->execution_count++;
                callback_reg->total_execution_time += std::chrono::duration_cast<std::chrono::milliseconds>(execution_time);
                
                if (performance_metrics_enabled_) {
                    updateStatistics(event, result, execution_time);
                }
                
                if (debug_enabled_) {
                    std::cout << "Callback " << callback_reg->callback_id 
                              << " executed with result: " << static_cast<int>(result)
                              << " in " << execution_time.count() << "μs" << std::endl;
                }
                
                // Handle result
                if (result == EventResult::FAILED && debug_enabled_) {
                    std::cerr << "Callback " << callback_reg->callback_id << " failed" << std::endl;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Exception in callback " << callback_reg->callback_id 
                          << ": " << e.what() << std::endl;
            }
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        {
            std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
            statistics_.events_processed++;
            statistics_.callbacks_executed += matching_callbacks.size();
        }
        
        if (debug_enabled_ && total_time.count() > 1000) { // Log slow events (>1ms)
            std::cout << "Event processing took " << total_time.count() << "μs for " 
                      << matching_callbacks.size() << " callbacks" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception processing event: " << e.what() << std::endl;
    }
}

std::vector<CallbackRegistration*> WebSocketHandler::findMatchingCallbacks(const WebSocketEvent& event) {
    std::vector<CallbackRegistration*> matching_callbacks;
    
    std::shared_lock<std::shared_mutex> lock(callback_registry_mutex_);
    
    for (auto& [callback_id, registration] : callback_registry_) {
        if (matchesFilter(event, registration.filter)) {
            matching_callbacks.push_back(&registration);
        }
    }
    
    // Sort by priority (higher priority first)
    std::sort(matching_callbacks.begin(), matching_callbacks.end(),
        [&event](const CallbackRegistration* a, const CallbackRegistration* b) {
            return static_cast<int>(event.priority) < static_cast<int>(event.priority);
        });
    
    return matching_callbacks;
}

bool WebSocketHandler::matchesFilter(const WebSocketEvent& event, const EventFilter& filter) const {
    // Check event type
    if (!filter.event_types.empty() && 
        filter.event_types.find(event.type) == filter.event_types.end()) {
        return false;
    }
    
    // Check priority
    if (event.priority > filter.min_priority) {
        return false;
    }
    
    // Check age
    auto age = std::chrono::steady_clock::now() - event.timestamp;
    if (age > filter.max_age) {
        return false;
    }
    
    // Check connection IDs
    if (!filter.connection_ids.empty()) {
        int connection_id = -1;
        
        if (std::holds_alternative<ConnectionContext>(event.context)) {
            connection_id = std::get<ConnectionContext>(event.context).connection.connection_id;
        } else if (std::holds_alternative<MessageContext>(event.context)) {
            connection_id = std::get<MessageContext>(event.context).connection.connection_id;
        } else if (std::holds_alternative<RequestContext>(event.context)) {
            connection_id = std::get<RequestContext>(event.context).connection.connection_id;
        } else if (std::holds_alternative<DataContext>(event.context)) {
            connection_id = std::get<DataContext>(event.context).connection.connection_id;
        }
        
        if (connection_id != -1 && 
            filter.connection_ids.find(connection_id) == filter.connection_ids.end()) {
            return false;
        }
    }
    
    // Check custom filter
    if (filter.custom_filter && !filter.custom_filter(event)) {
        return false;
    }
    
    return true;
}

// ======== UTILITY FUNCTIONS ========

std::string WebSocketHandler::generateCallbackId() {
    return "callback_" + std::to_string(callback_id_counter_.fetch_add(1));
}

std::string WebSocketHandler::generateEventId() {
    static std::atomic<uint64_t> event_counter{1};
    return "event_" + std::to_string(event_counter.fetch_add(1));
}

ConnectionInfo WebSocketHandler::createConnectionInfo(int connection_id, connection_hdl hdl) {
    ConnectionInfo info;
    info.connection_id = connection_id;
    info.connected_at = std::chrono::steady_clock::now();
    info.last_activity = info.connected_at;
    
    try {
        if (tls_enabled_) {
            tls_server::connection_ptr con = tls_ws_server_.get_con_from_hdl(hdl);
            if (con) {
                info.remote_endpoint = con->get_remote_endpoint();
                // Get more connection details
                auto req = con->get_request();
                if (req.get_header("User-Agent") != "") {
                    info.user_agent = req.get_header("User-Agent");
                }
            }
        } else {
            server::connection_ptr con = ws_server_.get_con_from_hdl(hdl);
            if (con) {
                info.remote_endpoint = con->get_remote_endpoint();
                // Get more connection details
                auto req = con->get_request();
                if (req.get_header("User-Agent") != "") {
                    info.user_agent = req.get_header("User-Agent");
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting connection info: " << e.what() << std::endl;
    }
    
    return info;
}

void WebSocketHandler::updateConnectionActivity(int connection_id) {
    std::unique_lock<std::shared_mutex> lock(connections_mutex_);
    auto it = connection_info_.find(connection_id);
    if (it != connection_info_.end()) {
        it->second.last_activity = std::chrono::steady_clock::now();
    }
}

void WebSocketHandler::logEvent(const WebSocketEvent& event) {
    if (!event_logging_enabled_) {
        return;
    }
    
    std::cout << "[EVENT] ID: " << event.event_id 
              << " Type: " << static_cast<int>(event.type)
              << " Priority: " << static_cast<int>(event.priority);
    
    if (std::holds_alternative<ConnectionContext>(event.context)) {
        auto ctx = std::get<ConnectionContext>(event.context);
        std::cout << " Connection: " << ctx.connection.connection_id;
    } else if (std::holds_alternative<MessageContext>(event.context)) {
        auto ctx = std::get<MessageContext>(event.context);
        std::cout << " Connection: " << ctx.connection.connection_id 
                  << " Message size: " << ctx.message.size;
    }
    
    std::cout << std::endl;
}

void WebSocketHandler::updateStatistics(const WebSocketEvent& event, EventResult result, 
                                       std::chrono::microseconds execution_time) {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    
    statistics_.event_counts[event.type]++;
    
    // Update average execution time
    uint64_t total_callbacks = statistics_.callbacks_executed;
    if (total_callbacks > 0) {
        auto current_avg_ms = statistics_.average_callback_execution_time.count();
        auto new_execution_ms = std::chrono::duration_cast<std::chrono::milliseconds>(execution_time).count();
        statistics_.average_callback_execution_time = std::chrono::milliseconds(
            (current_avg_ms * (total_callbacks - 1) + new_execution_ms) / total_callbacks
        );
    }
}

void WebSocketHandler::startEventProcessingThreads() {
    event_processing_threads_.clear();
    
    for (int i = 0; i < event_processing_thread_count_; ++i) {
        event_processing_threads_.emplace_back(&WebSocketHandler::processEventQueue, this);
    }
    
    std::cout << "Started " << event_processing_thread_count_ << " event processing threads" << std::endl;
}

void WebSocketHandler::stopEventProcessingThreads() {
    event_processing_active_ = false;
    event_queue_condition_.notify_all();
    
    for (auto& thread : event_processing_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    event_processing_threads_.clear();
    std::cout << "Stopped event processing threads" << std::endl;
}

void WebSocketHandler::cleanupConnection(int connection_id) {
    std::unique_lock<std::shared_mutex> lock(connections_mutex_);
    connections_.erase(connection_id);
    connection_info_.erase(connection_id);
    
    // Remove from connection_ids_ map
    for (auto it = connection_ids_.begin(); it != connection_ids_.end(); ++it) {
        if (it->second == connection_id) {
            connection_ids_.erase(it);
            break;
        }
    }
}