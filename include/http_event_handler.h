#ifndef HTTP_EVENT_HANDLER_H
#define HTTP_EVENT_HANDLER_H

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include "credential_manager.h"

/**
 * HTTP Event Handler
 * 
 * Processes HTTP events from frontend components and routes them to appropriate
 * backend mechanisms based on event type and source identification.
 */
class HttpEventHandler {
public:
    HttpEventHandler();
    ~HttpEventHandler();

    // Event processing types
    enum class EventResult {
        SUCCESS,
        ERROR,
        UNAUTHORIZED,
        AUTHENTICATION_FAILED,
        VALIDATION_ERROR,
        NOT_FOUND
    };

    // Event source identification
    struct EventSource {
        std::string component;    // e.g., "login-form", "file-upload", "user-session"
        std::string action;       // e.g., "submit", "click", "validate"
        std::string element_id;   // e.g., "login-button", "username-input"
        std::string user_agent;
        std::string client_id;
        std::string session_token;
    };

    // Event data structure
    struct EventData {
        std::string type;         // e.g., "login_attempt", "file_upload", "session_validate"
        nlohmann::json payload;   // Event-specific data
        EventSource source;       // Source identification
        int64_t timestamp;        // Event timestamp
    };

    // Event processing result
    struct EventResponse {
        EventResult result;
        nlohmann::json data;      // Response data for frontend
        std::string message;      // User-friendly message
        std::string session_token; // Updated session token if applicable
        int http_status;          // HTTP status code
    };

    // Main event processing method
    EventResponse processEvent(const std::string& raw_request, 
                              const std::map<std::string, std::string>& headers);

    // Event handler registration
    void registerEventHandler(const std::string& event_type,
                             std::function<EventResponse(const EventData&)> handler);
    
    // Get credential manager for external use
    std::shared_ptr<CredentialManager> getCredentialManager() const;

private:
    std::map<std::string, std::function<EventResponse(const EventData&)>> event_handlers_;
    std::unique_ptr<CredentialManager> credential_manager_;

    // Helper methods
    EventData parseEventRequest(const std::string& raw_request, 
                               const std::map<std::string, std::string>& headers);
    EventSource extractEventSource(const nlohmann::json& request_json,
                                  const std::map<std::string, std::string>& headers);
    std::string generateClientId();
    bool validateEventData(const EventData& event);
    nlohmann::json createStandardResponse(const EventResponse& response);

    // Built-in event handlers
    EventResponse handleLoginEvent(const EventData& event);
    EventResponse handleLogoutEvent(const EventData& event);
    EventResponse handleSessionValidationEvent(const EventData& event);
    EventResponse handleFileUploadEvent(const EventData& event);
    EventResponse handleFileAuthenticationEvent(const EventData& event);
    EventResponse handleEchoEvent(const EventData& event);
    EventResponse handleHealthCheckEvent(const EventData& event);

    // Helper methods for file authentication
    nlohmann::json validateAuthAccessFile(const std::string& file_path);
    std::string generateSessionToken(const std::string& username);
    std::string base64_decode(const std::string& encoded_string);
    double calculateFileEntropy(const std::string& data);
};

#endif // HTTP_EVENT_HANDLER_H