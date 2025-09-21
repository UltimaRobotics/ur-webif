#ifndef CREDENTIAL_MANAGER_H
#define CREDENTIAL_MANAGER_H

#include <string>
#include <map>
#include <nlohmann/json.hpp>

/**
 * Credential Manager
 * 
 * Handles encrypted credential storage and authentication.
 * Auto-creates credential files with default credentials when missing.
 */
class CredentialManager {
public:
    struct LoginResult {
        bool success;
        std::string message;
        std::string username;
        std::string session_token;
        std::map<std::string, std::string> user_data;
        int attempts_remaining;
        int total_attempts;
        bool is_banned;
        int lockout_remaining_seconds;
    };

    CredentialManager(const std::string& config_path = "./config/login.json");
    ~CredentialManager() = default;

    // Main authentication method
    LoginResult authenticate(const std::string& username, const std::string& password);

    // Login attempt tracking methods
    void recordFailedAttempt(const std::string& username, const std::string& client_id = "");
    void clearFailedAttempts(const std::string& username);
    int getRemainingAttempts(const std::string& username);
    bool isUserBanned(const std::string& username);
    int getLockoutRemainingSeconds(const std::string& username);

    // Session management
    std::string generateSessionToken(const std::string& username);
    bool validateSession(const std::string& session_token);
    void invalidateSession(const std::string& session_token);

    // Password management
    bool updatePassword(const std::string& username, const std::string& new_password);

    // Credential management
    bool ensureCredentialsFileExists();
    bool createDefaultCredentials();
    bool loadCredentials();
    bool saveCredentialsToFile();

private:
    struct FailedAttempt {
        int count;
        std::chrono::system_clock::time_point last_attempt;
        std::chrono::system_clock::time_point lockout_until;
    };

    std::string config_path_;
    std::string credentials_file_path_;
    nlohmann::json config_;
    std::map<std::string, std::string> credentials_;
    std::map<std::string, std::string> active_sessions_;
    std::map<std::string, FailedAttempt> failed_attempts_;

    // Encryption/Decryption methods
    std::string encrypt(const std::string& data, const std::string& key);
    std::string decrypt(const std::string& encrypted_data, const std::string& key);
    std::string generateEncryptionKey();
    std::string hashPassword(const std::string& password, const std::string& salt);
    std::string generateSalt();

    // File operations
    bool fileExists(const std::string& path);
    std::string readFile(const std::string& path);
    bool writeFile(const std::string& path, const std::string& content);

    // Configuration
    void loadConfig();
    void setDefaultConfig();
};

#endif // CREDENTIAL_MANAGER_H