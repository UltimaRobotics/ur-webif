LOGIN MECHANISM INTEGRATION GUIDE
==================================

This login mechanism provides secure WebSocket-based authentication for the UR WebIF API.

FEATURES:
---------
- Secure password hashing with salt
- Encrypted credential storage  
- Session management with configurable timeouts
- Rate limiting and IP-based lockouts
- Security event logging
- Thread-safe operations
- Standards-compliant authentication flow

INTEGRATION STEPS:
------------------

1. Include the login mechanism in your CMakeLists.txt:
   ```cmake
   add_subdirectory(mecanisms/login)
   target_link_libraries(your_target login_mechanism)
   ```

2. Initialize the login system in your main server:
   ```cpp
   #include "mecanisms/login/login_handler.hpp"
   
   auto login_manager = std::make_shared<LoginManager>("./config/server.json");
   auto login_handler = std::make_shared<LoginHandler>(login_manager);
   ```

3. Handle WebSocket login messages:
   ```cpp
   void on_message(websocketpp::connection_hdl hdl, message_ptr msg) {
       auto message = nlohmann::json::parse(msg->get_payload());
       
       if (message["type"] == "login_button_click") {
           std::string client_ip = extractClientIP(hdl);
           std::string client_id = getClientId(hdl);
           
           login_handler->handleLoginMessage(message, client_id, client_ip, 
               [this, hdl](const std::string& response) {
                   sendMessage(hdl, response);
               });
       }
   }
   ```

MESSAGE FORMATS:
----------------

Login Request:
```json
{
    "type": "login_button_click",
    "payload": {
        "username": "root",
        "password": "password", 
        "remember_me": false
    },
    "client_id": "client_123",
    "timestamp": 1234567890
}
```

Login Response (Success):
```json
{
    "type": "login_response",
    "success": true,
    "message": "Login successful",
    "payload": {
        "session_token": "abc123...",
        "authenticated": true
    },
    "client_id": "client_123",
    "timestamp": 1234567890
}
```

Login Response (Failure):
```json
{
    "type": "login_response", 
    "success": false,
    "message": "Invalid username or password",
    "payload": {
        "remaining_attempts": 4,
        "authenticated": false
    },
    "client_id": "client_123",
    "timestamp": 1234567890
}
```

SECURITY FEATURES:
------------------

1. Password Hashing: SHA-256 with unique salts
2. Encryption: AES-256-CBC for credential storage
3. Rate Limiting: Configurable failed attempt limits
4. Session Management: Secure token-based sessions
5. Input Validation: Comprehensive input sanitization
6. Timing Attack Protection: Constant-time operations
7. Security Logging: Detailed audit trail

CONFIGURATION:
--------------

Add to server.json:
```json
{
    "credentials_file": "./config/credentials.enc",
    "session_timeout": 3600,
    "max_login_attempts": 5, 
    "lockout_duration": 300
}
```

DEFAULT CREDENTIALS:
-------------------
Username: root
Password: password

The system automatically creates encrypted default credentials on first run.
Change the default password immediately in production environments.

MAINTENANCE:
------------

Run periodic cleanup to remove expired sessions:
```cpp
login_handler->performMaintenance(); // Call every few minutes
```

DEPENDENCIES:
-------------
- OpenSSL (for cryptographic functions)
- nlohmann/json (for JSON processing)
- C++17 standard library

SECURITY NOTES:
---------------
- Change default credentials before deployment
- Use HTTPS/WSS in production
- Implement proper logging and monitoring
- Regular security updates and reviews
- Consider implementing 2FA for enhanced security