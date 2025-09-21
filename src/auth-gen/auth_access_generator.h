
#ifndef AUTH_ACCESS_GENERATOR_H
#define AUTH_ACCESS_GENERATOR_H

#include <string>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Auth Access Generator
 * 
 * Generates encrypted .uacc files containing current login parameters
 * for secure authentication access sharing.
 */
class AuthAccessGenerator {
public:
    struct AuthAccessData {
        std::string username;
        std::string password_hash;
        std::string server_endpoint;
        std::string token_name;
        int64_t created_timestamp;
        int64_t expiry_timestamp;
        std::string access_level;
        std::map<std::string, std::string> metadata;
    };

    struct GenerationResult {
        bool success;
        std::string message;
        std::string file_path;
        std::string file_name;
        std::string download_token;
        size_t file_size;
        std::string content_base64;
    };

    AuthAccessGenerator();
    ~AuthAccessGenerator() = default;

    // Main generation method
    GenerationResult generateAuthAccessFile(
        const std::string& username,
        const std::string& password_hash,
        const std::string& token_name,
        int expiry_days = 30,
        const std::string& access_level = "standard"
    );

    // File management
    bool validateAuthAccessFile(const std::string& file_path);
    AuthAccessData decryptAuthAccessFile(const std::string& file_path);
    bool deleteAuthAccessFile(const std::string& file_path);
    
    // Download management
    std::string createDownloadToken(const std::string& file_path);
    bool validateDownloadToken(const std::string& token);
    std::string getFilePathFromToken(const std::string& token);
    void cleanupExpiredTokens();

    // Configuration
    void setOutputDirectory(const std::string& directory);
    void setEncryptionKey(const std::string& key);
    void setServerEndpoint(const std::string& endpoint);

private:
    std::string output_directory_;
    std::string encryption_key_;
    std::string server_endpoint_;
    std::map<std::string, std::pair<std::string, int64_t>> download_tokens_; // token -> {file_path, expiry}

    // Encryption/Decryption
    std::string encryptData(const std::string& data, const std::string& key);
    std::string decryptData(const std::string& encrypted_data, const std::string& key);
    
    // File operations
    std::string generateFileName(const std::string& username, const std::string& token_name);
    bool writeEncryptedFile(const std::string& file_path, const std::string& content);
    std::string readEncryptedFile(const std::string& file_path);
    
    // Utilities
    std::string generateDownloadToken();
    std::string generateEncryptionKey();
    std::string hashPassword(const std::string& password, const std::string& salt);
    bool ensureDirectoryExists(const std::string& directory);
    std::string toBase64(const std::string& data);
    std::string fromBase64(const std::string& base64_data);
    int64_t getCurrentTimestamp();
};

#endif // AUTH_ACCESS_GENERATOR_H
