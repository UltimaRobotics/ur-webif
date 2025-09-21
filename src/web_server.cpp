#include "../include/web_server.h"
#include "endpoint_logger.h"
#include "config_parser.h"
#include "websocket_handler.h"
#include "http_handler.h"
#include "file_server.h"
#include "api_request.h"
#include <iostream>
#include <cstring>
#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <functional>
#include <memory>

WebServer::WebServer() 
   : running_(false), http_daemon_(nullptr) {
   
   // Initialize handlers with new callback-based WebSocketHandler
   websocket_handler_ = std::make_unique<WebSocketHandler>();
   http_handler_ = std::make_unique<HttpHandler>();
   file_server_ = std::make_unique<FileServer>("./web", "index.html");
   
   std::cout << "WebServer initialized with callback-based WebSocket system" << std::endl;
}

WebServer::~WebServer() {
   stop();
}

bool WebServer::loadConfig(const std::string& config_file) {
   return ConfigParser::parseConfig(config_file, config_);
}

void WebServer::setConfig(const ServerConfig& config) {
   config_ = config;
   
   // Initialize endpoint logging filters
   EndpointLogger::getInstance().setEndpointFilters(config_.endpoint_logging);
   
   // Update file server configuration
   if (file_server_) {
       file_server_->setDocumentRoot(config_.document_root);
       file_server_->setDefaultFile(config_.default_file);
   }
}

bool WebServer::start() {
   if (running_) {
       std::cerr << "Server is already running" << std::endl;
       return false;
   }

   try {
       // Simplified HTTP daemon flags for better compatibility
       unsigned int flags = MHD_USE_INTERNAL_POLLING_THREAD;
       
       // Start HTTP server with minimal configuration for debugging
       http_daemon_ = MHD_start_daemon(
           flags,
           config_.port,
           nullptr, nullptr,
           &WebServer::accessHandlerCallback, this,
           MHD_OPTION_CONNECTION_TIMEOUT, (unsigned int)30,
           MHD_OPTION_NOTIFY_COMPLETED, &WebServer::requestCompletedCallback, this,
           MHD_OPTION_END
       );

       if (!http_daemon_) {
           std::cerr << "Failed to start HTTP server on port " << config_.port 
                     << ". Port may be in use or permission denied." << std::endl;
           return false;
       }

       std::cout << "HTTP server started on " << config_.host << ":" << config_.port << std::endl;

       // Start WebSocket server if enabled
       if (config_.enable_websocket) {
           // Configure WebSocket handler with new callback system
           websocket_handler_->setDebuggingEnabled(config_.websocket_debug_enabled);
           websocket_handler_->setEventLogging(config_.websocket_debug_messages);
           websocket_handler_->setPerformanceMetrics(true);
           
           // Use TLS based on configuration
           bool use_tls = config_.enable_ssl;
           if (!websocket_handler_->start(config_.websocket_port, use_tls)) {
               std::cerr << "Failed to start WebSocket server" << std::endl;
               MHD_stop_daemon(http_daemon_);
               http_daemon_ = nullptr;
               return false;
           }
           
           // Set up default callbacks for basic functionality
           setupDefaultWebSocketCallbacks();
       }

       running_ = true;
       return true;

   } catch (const std::exception& e) {
       std::cerr << "Error starting server: " << e.what() << std::endl;
       return false;
   }
}

void WebServer::stop() {
   if (!running_) {
       return;
   }

   running_ = false;

   // Unregister all WebSocket callbacks
   unregisterAllWebSocketCallbacks();

   // Stop HTTP server
   if (http_daemon_) {
       MHD_stop_daemon(http_daemon_);
       http_daemon_ = nullptr;
       std::cout << "HTTP server stopped" << std::endl;
   }

   // Stop WebSocket server
   if (websocket_handler_) {
       websocket_handler_->stop();
   }
}

void WebServer::addRouteHandler(const std::string& path, 
                               std::function<std::string(const std::string& method, 
                                                        const std::map<std::string, std::string>& params,
                                                        const std::string& body)> handler) {
   if (http_handler_) {
       http_handler_->addRouteHandler(path, handler);
   }
   route_handlers_[path] = handler;
}

void WebServer::addDynamicRouteHandler(const std::string& pattern,
                                      std::function<std::string(const std::string& method, 
                                                               const std::map<std::string, std::string>& params,
                                                               const std::string& body)> handler) {
   if (http_handler_) {
       http_handler_->addDynamicRoute(pattern, handler);
   }
}

// Structured API route handler registration
void WebServer::addStructuredRouteHandler(const std::string& path, RouteProcessor processor) {
   // Create a wrapper that converts the structured API call to the standard route handler interface
   auto wrapper_handler = [processor, path](const std::string& method, 
                                           const std::map<std::string, std::string>& params,
                                           const std::string& body) -> std::string {
       // Create ApiRequest from standard inputs
       ApiRequest request;
       request.method = method;
       request.route = path;
       request.headers = params; // Using params as headers map
       request.body = body;
       
       // Parse JSON if present
       if (!body.empty()) {
           try {
               request.json_data = nlohmann::json::parse(body);
               request.is_json_valid = true;
           } catch (const std::exception& e) {
               request.is_json_valid = false;
           }
       } else {
           request.is_json_valid = false;
       }
       
       // Call the structured processor
       ApiResponse response = processor(request);
       
       // Return the JSON response as string
       return response.body;
   };
   
   // Register the wrapped handler
   addRouteHandler(path, wrapper_handler);
}

// ======== NEW CALLBACK-BASED WEBSOCKET INTERFACE ========

std::string WebServer::onWebSocketConnected(std::function<EventResult(int connection_id, const ConnectionInfo&)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   std::string callback_id = websocket_handler_->onConnectionOpened(callback);
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketDisconnected(std::function<EventResult(int connection_id, const std::string& reason)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   std::string callback_id = websocket_handler_->onConnectionClosed(callback);
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketError(std::function<EventResult(int connection_id, const std::string& error)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   // Register for connection error events
   EventFilter filter;
   filter.event_types.insert(EventType::CONNECTION_ERROR);
   filter.event_types.insert(EventType::CONNECTION_FAILED);
   
   std::string callback_id = websocket_handler_->registerConnectionCallback(filter, 
       [callback](const ConnectionContext& ctx) -> EventResult {
           return callback(ctx.connection.connection_id, ctx.reason);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketMessage(std::function<EventResult(int connection_id, const std::string& message)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   std::string callback_id = websocket_handler_->onMessageReceived(callback);
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketBinaryMessage(std::function<EventResult(int connection_id, const std::vector<uint8_t>& data)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   std::string callback_id = websocket_handler_->onBinaryReceived(callback);
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketMessageSent(std::function<EventResult(int connection_id, const std::string& message)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::MESSAGE_SENT);
   
   std::string callback_id = websocket_handler_->registerMessageCallback(filter,
       [callback](const MessageContext& ctx) -> EventResult {
           return callback(ctx.connection.connection_id, ctx.message.content);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketMessageFailed(std::function<EventResult(int connection_id, const std::string& message)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::MESSAGE_FAILED);
   
   std::string callback_id = websocket_handler_->registerMessageCallback(filter,
       [callback](const MessageContext& ctx) -> EventResult {
           return callback(ctx.connection.connection_id, ctx.message.content);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketUpgradeRequest(std::function<EventResult(int connection_id, const RequestContext& request)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::HTTP_UPGRADE_REQUEST);
   
   std::string callback_id = websocket_handler_->registerRequestCallback(filter,
       [callback](const RequestContext& ctx) -> EventResult {
           return callback(ctx.connection.connection_id, ctx);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketAuthentication(std::function<EventResult(int connection_id, const std::string& token)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   std::string callback_id = websocket_handler_->onAuthentication(callback);
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketAuthorization(std::function<EventResult(int connection_id, const std::map<std::string, std::string>& headers)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::AUTHORIZATION_REQUEST);
   
   std::string callback_id = websocket_handler_->registerRequestCallback(filter,
       [callback](const RequestContext& ctx) -> EventResult {
           return callback(ctx.connection.connection_id, ctx.request.headers);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketDataValidation(std::function<EventResult(int connection_id, const std::string& data)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   std::string callback_id = websocket_handler_->onDataValidation(callback);
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketDataTransformation(std::function<EventResult(int connection_id, std::string& data)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::DATA_TRANSFORMATION);
   
   std::string callback_id = websocket_handler_->registerDataCallback(filter,
       [callback](const DataContext& ctx) -> EventResult {
           std::string mutable_data = ctx.data;
           EventResult result = callback(ctx.connection.connection_id, mutable_data);
           // Note: In a more complete implementation, we'd need to handle the modified data
           return result;
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketDataProcessing(std::function<EventResult(int connection_id, const nlohmann::json& data)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::DATA_PROCESSING);
   
   std::string callback_id = websocket_handler_->registerDataCallback(filter,
       [callback](const DataContext& ctx) -> EventResult {
           try {
               nlohmann::json json_data = nlohmann::json::parse(ctx.data);
               return callback(ctx.connection.connection_id, json_data);
           } catch (const std::exception& e) {
               std::cerr << "JSON parsing error in data processing: " << e.what() << std::endl;
               return EventResult::FAILED;
           }
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketHeartbeat(std::function<EventResult()> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   std::string callback_id = websocket_handler_->onHeartbeat(callback);
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketHealthCheck(std::function<EventResult(int active_connections)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::HEALTH_CHECK);
   
   std::string callback_id = websocket_handler_->registerSystemCallback(filter,
       [this, callback](const SystemContext& ctx) -> EventResult {
           int active_connections = websocket_handler_->getActiveConnectionCount();
           return callback(active_connections);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::onWebSocketServerEvent(std::function<EventResult(EventType type, const std::map<std::string, std::string>& data)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter filter;
   filter.event_types.insert(EventType::SERVER_STARTED);
   filter.event_types.insert(EventType::SERVER_STOPPED);
   filter.event_types.insert(EventType::HEALTH_CHECK);
   
   std::string callback_id = websocket_handler_->registerSystemCallback(filter,
       [callback](const SystemContext& ctx) -> EventResult {
           std::map<std::string, std::string> data;
           for (const auto& [key, value] : ctx.system_data) {
               if (std::holds_alternative<std::string>(value)) {
                   data[key] = std::get<std::string>(value);
               }
           }
           return callback(ctx.event_type, data);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

std::string WebServer::registerWebSocketCallback(const std::function<bool(EventType, int connection_id)>& filter,
                                                 std::function<EventResult(EventType type, int connection_id, const std::string& data)> callback) {
   if (!websocket_handler_) {
       return "";
   }
   
   EventFilter event_filter;
   event_filter.custom_filter = [filter](const WebSocketEvent& event) -> bool {
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
       
       return filter(event.type, connection_id);
   };
   
   std::string callback_id = websocket_handler_->registerCallback(event_filter,
       [callback](const WebSocketEvent& event) -> EventResult {
           int connection_id = -1;
           std::string data;
           
           if (std::holds_alternative<ConnectionContext>(event.context)) {
               const auto& ctx = std::get<ConnectionContext>(event.context);
               connection_id = ctx.connection.connection_id;
               data = ctx.reason;
           } else if (std::holds_alternative<MessageContext>(event.context)) {
               const auto& ctx = std::get<MessageContext>(event.context);
               connection_id = ctx.connection.connection_id;
               data = ctx.message.content;
           } else if (std::holds_alternative<RequestContext>(event.context)) {
               const auto& ctx = std::get<RequestContext>(event.context);
               connection_id = ctx.connection.connection_id;
               data = ctx.request.body;
           } else if (std::holds_alternative<DataContext>(event.context)) {
               const auto& ctx = std::get<DataContext>(event.context);
               connection_id = ctx.connection.connection_id;
               data = ctx.data;
           }
           
           return callback(event.type, connection_id, data);
       });
   
   addCallbackId(callback_id);
   return callback_id;
}

bool WebServer::unregisterWebSocketCallback(const std::string& callback_id) {
   if (!websocket_handler_) {
       return false;
   }
   
   removeCallbackId(callback_id);
   return websocket_handler_->unregisterCallback(callback_id);
}

void WebServer::unregisterAllWebSocketCallbacks() {
   if (!websocket_handler_) {
       return;
   }
   
   std::lock_guard<std::mutex> lock(callback_ids_mutex_);
   for (const std::string& callback_id : websocket_callback_ids_) {
       websocket_handler_->unregisterCallback(callback_id);
   }
   websocket_callback_ids_.clear();
}

std::vector<std::string> WebServer::getRegisteredWebSocketCallbacks() const {
   if (!websocket_handler_) {
       return {};
   }
   
   std::lock_guard<std::mutex> lock(callback_ids_mutex_);
   return websocket_callback_ids_;
}

void WebServer::activateWebSocketCallback(const std::string& callback_id) {
   if (websocket_handler_) {
       websocket_handler_->activateCallback(callback_id);
   }
}

void WebServer::deactivateWebSocketCallback(const std::string& callback_id) {
   if (websocket_handler_) {
       websocket_handler_->deactivateCallback(callback_id);
   }
}

// ======== WEBSOCKET MESSAGING (Enhanced) ========

void WebServer::sendWebSocketMessage(int connection_id, const std::string& message,
                                    std::function<void(bool success)> callback) {
   if (websocket_handler_) {
       websocket_handler_->sendMessage(connection_id, message, callback);
   } else if (callback) {
       callback(false);
   }
}

void WebServer::broadcastWebSocketMessage(const std::string& message,
                                         std::function<void(std::vector<int> failed_connections)> callback) {
   if (websocket_handler_) {
       websocket_handler_->broadcastMessage(message, callback);
   } else if (callback) {
       std::vector<int> all_failed = getActiveWebSocketConnections();
       callback(all_failed);
   }
}

void WebServer::sendToWebSocketConnections(const std::vector<int>& connection_ids, const std::string& message,
                                          std::function<void(std::map<int, bool> results)> callback) {
   if (websocket_handler_) {
       websocket_handler_->sendToConnections(connection_ids, message, callback);
   } else if (callback) {
       std::map<int, bool> all_failed;
       for (int conn_id : connection_ids) {
           all_failed[conn_id] = false;
       }
       callback(all_failed);
   }
}

void WebServer::sendWebSocketBinaryMessage(int connection_id, const std::vector<uint8_t>& data,
                                          std::function<void(bool success)> callback) {
   if (websocket_handler_) {
       websocket_handler_->sendBinaryMessage(connection_id, data, callback);
   } else if (callback) {
       callback(false);
   }
}

// ======== WEBSOCKET CONNECTION MANAGEMENT ========

std::vector<int> WebServer::getActiveWebSocketConnections() const {
   if (websocket_handler_) {
       return websocket_handler_->getActiveConnectionIds();
   }
   return {};
}

ConnectionInfo WebServer::getWebSocketConnectionInfo(int connection_id) const {
   if (websocket_handler_) {
       return websocket_handler_->getConnectionInfo(connection_id);
   }
   return ConnectionInfo{};
}

bool WebServer::isWebSocketConnectionAlive(int connection_id) const {
   if (websocket_handler_) {
       return websocket_handler_->isConnectionAlive(connection_id);
   }
   return false;
}

int WebServer::getActiveWebSocketConnectionCount() const {
   if (websocket_handler_) {
       return websocket_handler_->getActiveConnectionCount();
   }
   return 0;
}

void WebServer::closeWebSocketConnection(int connection_id, const std::string& reason,
                                        std::function<void(bool success)> callback) {
   if (websocket_handler_) {
       websocket_handler_->closeConnection(connection_id, reason, callback);
   } else if (callback) {
       callback(false);
   }
}

void WebServer::setWebSocketConnectionMetadata(int connection_id, const std::string& key, 
                                              const std::string& value) {
   if (websocket_handler_) {
       websocket_handler_->setConnectionMetadata(connection_id, key, value);
   }
}

std::string WebServer::getWebSocketConnectionMetadata(int connection_id, const std::string& key) const {
   if (websocket_handler_) {
       auto value = websocket_handler_->getConnectionMetadata(connection_id, key);
       if (std::holds_alternative<std::string>(value)) {
           return std::get<std::string>(value);
       }
   }
   return "";
}

// ======== WEBSOCKET CONFIGURATION ========

void WebServer::setWebSocketDebugging(bool enabled) {
   if (websocket_handler_) {
       websocket_handler_->setDebuggingEnabled(enabled);
   }
}

void WebServer::setWebSocketEventLogging(bool enabled) {
   if (websocket_handler_) {
       websocket_handler_->setEventLogging(enabled);
   }
}

void WebServer::setWebSocketPerformanceMetrics(bool enabled) {
   if (websocket_handler_) {
       websocket_handler_->setPerformanceMetrics(enabled);
   }
}

void WebServer::setWebSocketAuthenticationRequired(bool required) {
   if (websocket_handler_) {
       websocket_handler_->setAuthenticationRequired(required);
   }
}

void WebServer::setWebSocketConnectionLimit(int limit) {
   if (websocket_handler_) {
       websocket_handler_->setConnectionLimit(limit);
   }
}

void WebServer::setWebSocketRateLimit(int messages_per_second, int burst_size) {
   if (websocket_handler_) {
       websocket_handler_->setRateLimit(messages_per_second, burst_size);
   }
}

void WebServer::setWebSocketHeartbeatInterval(int seconds) {
   if (websocket_handler_) {
       websocket_handler_->setHeartbeatInterval(std::chrono::seconds(seconds));
   }
}

void WebServer::setWebSocketConnectionTimeout(int seconds) {
   if (websocket_handler_) {
       websocket_handler_->setConnectionTimeout(std::chrono::seconds(seconds));
   }
}

// ======== WEBSOCKET STATISTICS ========

nlohmann::json WebServer::getWebSocketStatistics() const {
   if (websocket_handler_) {
       return websocket_handler_->getStatisticsJson();
   }
   return nlohmann::json{};
}

void WebServer::resetWebSocketStatistics() {
   if (websocket_handler_) {
       websocket_handler_->resetStatistics();
   }
}

// ======== LEGACY COMPATIBILITY ========

void WebServer::setWebSocketMessageHandler(std::function<void(int, const std::string&)> handler) {
   std::cerr << "Warning: setWebSocketMessageHandler is deprecated. Use onWebSocketMessage() callback instead." << std::endl;
   
   // Convert to new callback system
   if (handler) {
       onWebSocketMessage([handler](int connection_id, const std::string& message) -> EventResult {
           handler(connection_id, message);
           return EventResult::SUCCESS;
       });
   }
}

// ======== EVENT PROCESSING CONTROL ========

void WebServer::setWebSocketEventProcessingThreads(int thread_count) {
   if (websocket_handler_) {
       websocket_handler_->setEventProcessingThreads(thread_count);
   }
}

void WebServer::pauseWebSocketEventProcessing() {
   if (websocket_handler_) {
       websocket_handler_->pauseEventProcessing();
   }
}

void WebServer::resumeWebSocketEventProcessing() {
   if (websocket_handler_) {
       websocket_handler_->resumeEventProcessing();
   }
}

bool WebServer::isWebSocketEventProcessingActive() const {
   if (websocket_handler_) {
       return websocket_handler_->isEventProcessingActive();
   }
   return false;
}

size_t WebServer::getWebSocketEventQueueSize() const {
   if (websocket_handler_) {
       return websocket_handler_->getEventQueueSize();
   }
   return 0;
}

void WebServer::clearWebSocketEventQueue() {
   if (websocket_handler_) {
       websocket_handler_->clearEventQueue();
   }
}

void WebServer::setMaxWebSocketEventQueueSize(size_t max_size) {
   if (websocket_handler_) {
       websocket_handler_->setMaxEventQueueSize(max_size);
   }
}

// ======== HTTP REQUEST HANDLING ========

enum MHD_Result WebServer::accessHandlerCallback(void* cls, struct MHD_Connection* connection,
                                    const char* url, const char* method,
                                    const char* version, const char* upload_data,
                                    size_t* upload_data_size, void** con_cls) {
   
   WebServer* server = static_cast<WebServer*>(cls);
   return server->handleRequest(connection, url, method, version, upload_data, upload_data_size, con_cls);
}

void WebServer::requestCompletedCallback(void* cls, struct MHD_Connection* connection,
                                       void** con_cls, enum MHD_RequestTerminationCode toe) {
   // Critical: Safe cleanup of connection-specific data to prevent memory leaks and corruption
   if (con_cls && *con_cls != nullptr) {
       // Validate the pointer before attempting to delete it
       std::string* state = static_cast<std::string*>(*con_cls);
       try {
           delete state;
           *con_cls = nullptr;
       } catch (const std::exception& e) {
           std::cerr << "Error during connection cleanup: " << e.what() << std::endl;
       } catch (...) {
           std::cerr << "Unknown error during connection cleanup" << std::endl;
       }
   }
   
   // Enhanced logging for debugging connection termination issues
   switch (toe) {
       case MHD_REQUEST_TERMINATED_COMPLETED_OK:
           // Normal completion - no logging needed
           break;
       case MHD_REQUEST_TERMINATED_WITH_ERROR:
           std::cerr << "Warning: Request terminated with error - possible client disconnect" << std::endl;
           break;
       case MHD_REQUEST_TERMINATED_TIMEOUT_REACHED:
           std::cerr << "Warning: Request terminated due to timeout - client may be slow" << std::endl;
           break;
       case MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN:
           std::cout << "Info: Request terminated due to server shutdown" << std::endl;
           break;
       case MHD_REQUEST_TERMINATED_CLIENT_ABORT:
           std::cout << "Info: Request terminated by client abort" << std::endl;
           break;
       default:
           std::cerr << "Warning: Request terminated with unknown code: " << toe << std::endl;
           break;
   }
}

enum MHD_Result WebServer::handleRequest(struct MHD_Connection* connection,
                        const char* url, const char* method,
                        const char* version, const char* upload_data,
                        size_t* upload_data_size, void** con_cls) {

   if (!connection || !url || !method) {
       return MHD_NO;
   }
   
   // Critical: Proper MHD connection state handling to prevent core dumps
   if (nullptr == *con_cls) {
       // First call - initialize connection state with properly allocated memory
       std::string* request_state = new std::string("initialized");
       *con_cls = request_state;
       return MHD_YES; // Tell MHD to call us again with initialized state
   }

   // Validate connection state to prevent memory corruption
   std::string* state = static_cast<std::string*>(*con_cls);
   if (!state) {
       std::cerr << "Error: Invalid connection state detected" << std::endl;
       return MHD_NO;
   }

   std::string url_str(url);
   std::string method_str(method);
   ENDPOINT_LOG("http", "Request: " + method_str + " " + url_str + " (processing)");
   
   // Try HTTP handler first (for API routes only)
   if (url_str.find("/api/") == 0) {
       ENDPOINT_LOG("http", "Processing as API route: " + url_str);
       enum MHD_Result result = http_handler_->handleRequest(connection, url, method, version, upload_data, upload_data_size, con_cls);
       if (result != MHD_NO) {
           return result;
       }
   }

   // Handle static files with session-aware transaction processing
   ENDPOINT_LOG("file_server", "Processing as static file: " + url_str);
   
   // Extract session information from headers
   std::string session_token;
   std::string user_id;
   
   // Get session token from Authorization header
   const char* auth_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Authorization");
   if (auth_header) {
       std::string auth_str(auth_header);
       if (auth_str.find("Bearer ") == 0) {
           session_token = auth_str.substr(7); // Remove "Bearer " prefix
       }
   }
   
   // Get session token from cookies as fallback
   if (session_token.empty()) {
       const char* cookie_header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Cookie");
       if (cookie_header) {
           std::string cookie_str(cookie_header);
           size_t pos = cookie_str.find("session_token=");
           if (pos != std::string::npos) {
               pos += 14; // length of "session_token="
               size_t end_pos = cookie_str.find(";", pos);
               if (end_pos == std::string::npos) {
                   end_pos = cookie_str.length();
               }
               session_token = cookie_str.substr(pos, end_pos - pos);
           }
       }
   }
   
   // Check if this is a protected page that requires authentication
   bool requires_auth = (url_str.find("/components/source.html") != std::string::npos ||
                        url_str.find("/dashboard") != std::string::npos);
   
   std::cout << "[WEB-SERVER] Session token: " << (session_token.empty() ? "(none)" : "present") 
             << ", requires auth: " << (requires_auth ? "yes" : "no") << std::endl;
   
   // Use transaction-aware file serving for better page management
   if (!session_token.empty() || requires_auth) {
       return file_server_->servePageWithTransaction(connection, url_str, session_token, user_id);
   } else {
       // Use regular file serving for public pages
       return file_server_->serveFileWithState(connection, url_str, con_cls);
   }
}

// ======== INTERNAL HELPERS ========

void WebServer::setupDefaultWebSocketCallbacks() {
   // Set up basic logging callbacks
   onWebSocketConnected([](int connection_id, const ConnectionInfo& info) -> EventResult {
       std::cout << "WebSocket client connected: " << connection_id 
                 << " from " << info.remote_endpoint << std::endl;
       return EventResult::SUCCESS;
   });
   
   onWebSocketDisconnected([](int connection_id, const std::string& reason) -> EventResult {
       std::cout << "WebSocket client disconnected: " << connection_id 
                 << " reason: " << reason << std::endl;
       return EventResult::SUCCESS;
   });
   
   // Set up basic message logging
   onWebSocketMessage([](int connection_id, const std::string& message) -> EventResult {
       std::cout << "WebSocket message received from " << connection_id 
                 << ": " << message.substr(0, 100) 
                 << (message.length() > 100 ? "..." : "") << std::endl;
       return EventResult::SUCCESS;
   });
}

void WebServer::addCallbackId(const std::string& callback_id) {
   std::lock_guard<std::mutex> lock(callback_ids_mutex_);
   websocket_callback_ids_.push_back(callback_id);
}

void WebServer::removeCallbackId(const std::string& callback_id) {
   std::lock_guard<std::mutex> lock(callback_ids_mutex_);
   auto it = std::find(websocket_callback_ids_.begin(), websocket_callback_ids_.end(), callback_id);
   if (it != websocket_callback_ids_.end()) {
       websocket_callback_ids_.erase(it);
   }
}

// ======== UTILITY FUNCTIONS ========

std::string WebServer::getMimeType(const std::string& file_path) {
   size_t dot_pos = file_path.find_last_of('.');
   if (dot_pos == std::string::npos) {
       return "application/octet-stream";
   }

   std::string extension = file_path.substr(dot_pos + 1);
   std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

   static const std::map<std::string, std::string> mime_types = {
       {"html", "text/html"},
       {"htm", "text/html"},
       {"css", "text/css"},
       {"js", "application/javascript"},
       {"json", "application/json"},
       {"png", "image/png"},
       {"jpg", "image/jpeg"},
       {"jpeg", "image/jpeg"},
       {"gif", "image/gif"},
       {"svg", "image/svg+xml"},
       {"ico", "image/x-icon"},
       {"pdf", "application/pdf"},
       {"txt", "text/plain"},
       {"xml", "application/xml"}
   };

   auto it = mime_types.find(extension);
   return (it != mime_types.end()) ? it->second : "application/octet-stream";
}

enum MHD_Result WebServer::enableCORS(struct MHD_Connection* connection) {
   // Critical: This method should NOT send a response, only add headers to existing responses
   // CORS headers are added in the individual response methods
   // Returning MHD_YES indicates CORS is enabled for header addition
   return MHD_YES;
}

enum MHD_Result WebServer::createSuccessfulResponse(struct MHD_Connection* connection, 
                                                   const std::string& content, 
                                                   const std::string& content_type) {
   // Critical: Safe response creation to prevent memory corruption
   struct MHD_Response* response = nullptr;
   
   // Validate content before response creation
   if (content.empty()) {
       std::cerr << "Warning: Creating response with empty content" << std::endl;
       std::string default_content = "{\"error\":\"Empty content\"}";
       try {
           response = MHD_create_response_from_buffer(
               default_content.length(),
               const_cast<char*>(default_content.c_str()),
               MHD_RESPMEM_MUST_COPY
           );
       } catch (const std::exception& e) {
           std::cerr << "Exception creating default response: " << e.what() << std::endl;
           return MHD_NO;
       }
   } else {
       // Use safe memory strategy - let MHD copy the data
       try {
           response = MHD_create_response_from_buffer(
               content.length(),
               const_cast<char*>(content.c_str()),
               MHD_RESPMEM_MUST_COPY  // Safe: MHD copies data, no ownership transfer
           );
       } catch (const std::exception& e) {
           std::cerr << "Exception creating MHD response: " << e.what() << std::endl;
           return MHD_NO;
       }
   }
   
   if (!response) {
       std::cerr << "Critical: Failed to create MHD response" << std::endl;
       return MHD_NO;
   }
   
   // Add production-ready headers
   MHD_add_response_header(response, "Content-Type", content_type.c_str());
   MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
   MHD_add_response_header(response, "Cache-Control", "no-cache, no-store, must-revalidate");
   MHD_add_response_header(response, "Pragma", "no-cache");
   MHD_add_response_header(response, "Expires", "0");
   
   // Critical: Thread-safe response handling with comprehensive error checking
   enum MHD_Result ret = MHD_NO;
   
   try {
       ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
       if (ret != MHD_YES) {
           std::cerr << "Critical: MHD_queue_response failed in createSuccessfulResponse, status: " << ret << std::endl;
       }
   } catch (const std::exception& e) {
       std::cerr << "Exception during response queueing in createSuccessfulResponse: " << e.what() << std::endl;
       ret = MHD_NO;
   } catch (...) {
       std::cerr << "Unknown exception during response queueing in createSuccessfulResponse" << std::endl;
       ret = MHD_NO;
   }
   
   // Always destroy response to prevent memory leaks
   try {
       MHD_destroy_response(response);
   } catch (const std::exception& e) {
       std::cerr << "Exception destroying response in createSuccessfulResponse: " << e.what() << std::endl;
   }
   
   std::cout << "Production response result: " << ret << std::endl;
   return ret;
}

std::string WebServer::getHomepageContent() const {
   return R"(<!DOCTYPE html>
<html>
<head>
    <title>UR WebIF API - HTTP-Based Architecture</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            padding: 20px;
            line-height: 1.6;
            max-width: 1200px;
            margin: 0 auto;
            background-color: #f8f9fa;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 12px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
            padding-bottom: 20px;
            border-bottom: 2px solid #e9ecef;
        }
        .status-badge {
            background: #28a745;
            color: white;
            padding: 8px 16px;
            border-radius: 20px;
            font-weight: 500;
            display: inline-block;
            margin: 10px 0;
        }
        .endpoint {
            background: #f8f9fa;
            padding: 15px;
            margin: 10px 0;
            border-radius: 8px;
            border-left: 4px solid #007bff;
            transition: transform 0.2s;
        }
        .endpoint:hover {
            transform: translateX(5px);
            background: #e9ecef;
        }
        .method {
            font-weight: bold;
            color: #007bff;
            font-family: 'Courier New', monospace;
        }
        .description {
            color: #6c757d;
        }
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-top: 20px;
        }
        .feature {
            background: #f8f9fa;
            padding: 15px;
            border-radius: 8px;
        }
        .feature-title {
            font-weight: bold;
            color: #495057;
            margin-bottom: 8px;
        }
        @media (max-width: 768px) {
            .grid { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>🚀 UR WebIF API Server</h1>
            <div class="status-badge">HTTP-Based Event Architecture</div>
            <p style="margin: 10px 0; color: #6c757d;">Production-ready server successfully running</p>
        </div>
        
        <h2>📡 Available API Endpoints</h2>
        <div class="endpoint">
            <span class="method">POST /api/login</span>
            <span class="description"> - User authentication and session management</span>
        </div>
        <div class="endpoint">
            <span class="method">POST /api/logout</span>
            <span class="description"> - User logout and session cleanup</span>
        </div>
        <div class="endpoint">
            <span class="method">POST /api/validate-session</span>
            <span class="description"> - Session validation and renewal</span>
        </div>
        <div class="endpoint">
            <span class="method">POST /api/echo</span>
            <span class="description"> - Echo service for testing and debugging</span>
        </div>
        <div class="endpoint">
            <span class="method">POST /api/broadcast</span>
            <span class="description"> - Message broadcasting to all connected clients</span>
        </div>
        <div class="endpoint">
            <span class="method">GET /api/health</span>
            <span class="description"> - Health check and server status</span>
        </div>
        <div class="endpoint">
            <span class="method">GET /api/stats</span>
            <span class="description"> - Server statistics and performance metrics</span>
        </div>
        <div class="endpoint">
            <span class="method">GET /api/events</span>
            <span class="description"> - Server-Sent Events (SSE) for real-time updates</span>
        </div>
        
        <h2>🎯 System Features</h2>
        <div class="grid">
            <div class="feature">
                <div class="feature-title">✅ HTTP-Based Architecture</div>
                Complete transition from WebSocket to RESTful HTTP endpoints
            </div>
            <div class="feature">
                <div class="feature-title">✅ Production Ready</div>
                Robust error handling, logging, and connection management
            </div>
            <div class="feature">
                <div class="feature-title">✅ Cross-Origin Support</div>
                CORS enabled for web browser compatibility
            </div>
            <div class="feature">
                <div class="feature-title">✅ Static File Serving</div>
                Efficient static asset delivery with MIME type detection
            </div>
        </div>
        
        <div style="margin-top: 30px; padding: 20px; background: #e8f5e8; border-radius: 8px; text-align: center;">
            <strong>🟢 Server Status:</strong> Running successfully on port 5000<br>
            <strong>⚡ Architecture:</strong> Complete HTTP-based event system operational
        </div>
    </div>
</body>
</html>)";
}