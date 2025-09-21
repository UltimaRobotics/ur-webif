#ifndef LOGIN_MANAGER_HPP
#define LOGIN_MANAGER_HPP

#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>

struct LoginAttempt {
    int attempts;
    std::chrono::steady_clock::time_point last_attempt;
    std::chrono::steady_clock::time_point lockout_until;
};

struct UserCredentials {
    std::string username;
    std::string password_hash;
    std::string salt;
    bool remember_me_enabled;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
};

struct SessionInfo {
    std::string session_token;
    std::string username;
    std::chrono::steady_clock::time_point created_at;
    std::chrono::steady_clock::time_point expires_at;
    bool remember_me;
};

class LoginManager {
public:
    LoginManager(const std::string& config_path);
    ~LoginManager();

    // Core login functionality
    struct LoginResult {
        bool success;
        std::string message;
        std::string session_token;
        int remaining_attempts;
    };

    LoginResult authenticate(const std::string& username, 
                           const std::string& password, 
                           bool remember_me,
                           const std::string& client_ip);

    bool validateSession(const std::string& session_token);
    bool logout(const std::string& session_token);
    
    // Session management
    std::string getUsernameFromSession(const std::string& session_token);
    void cleanupExpiredSessions();
    
    // Security features
    bool isIpLocked(const std::string& client_ip);
    void resetLoginAttempts(const std::string& client_ip);
    
    // Configuration
    void loadConfiguration(const std::string& config_path);
    bool initializeDefaultCredentials();

private:
    // Encryption and hashing
    std::string generateSalt();
    std::string hashPassword(const std::string& password, const std::string& salt);
    std::string generateSessionToken();
    
    // File operations
    bool loadCredentials();
    bool saveCredentials();
    std::string encryptData(const std::string& data, const std::string& key);
    std::string decryptData(const std::string& encrypted_data, const std::string& key);
    
    // Security utilities
    std::string getEncryptionKey();
    void recordLoginAttempt(const std::string& client_ip, bool success);
    
    // Member variables
    std::string credentials_file_path_;
    std::string encryption_key_;
    std::unordered_map<std::string, UserCredentials> users_;
    std::unordered_map<std::string, SessionInfo> active_sessions_;
    std::unordered_map<std::string, LoginAttempt> login_attempts_;
    
    // Configuration
    int session_timeout_seconds_;
    int max_login_attempts_;
    int lockout_duration_seconds_;
    
    // Thread safety
    std::mutex credentials_mutex_;
    std::mutex sessions_mutex_;
    std::mutex attempts_mutex_;
    
    // Constants
    static const int SALT_LENGTH = 32;
    static const int TOKEN_LENGTH = 64;
    static const std::string DEFAULT_USERNAME;
    static const std::string DEFAULT_PASSWORD;
};

#endif // LOGIN_MANAGER_HPP