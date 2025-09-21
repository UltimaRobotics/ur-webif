#include "login_manager.hpp"
#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/evp.h>
#include <openssl/evp.h>

// Static constants
const std::string LoginManager::DEFAULT_USERNAME = "root";
const std::string LoginManager::DEFAULT_PASSWORD = "password";

LoginManager::LoginManager(const std::string& config_path) 
    : session_timeout_seconds_(3600)
    , max_login_attempts_(5)
    , lockout_duration_seconds_(300) {
    
    loadConfiguration(config_path);
    
    if (!loadCredentials()) {
        initializeDefaultCredentials();
    }
}

LoginManager::~LoginManager() {
    saveCredentials();
}

void LoginManager::loadConfiguration(const std::string& config_path) {
    try {
        std::ifstream config_file(config_path);
        if (!config_file.is_open()) {
            throw std::runtime_error("Cannot open configuration file: " + config_path);
        }
        
        nlohmann::json config;
        config_file >> config;
        
        credentials_file_path_ = config.value("credentials_file", "./config/credentials.enc");
        session_timeout_seconds_ = config.value("session_timeout", 3600);
        max_login_attempts_ = config.value("max_login_attempts", 5);
        lockout_duration_seconds_ = config.value("lockout_duration", 300);
        
        // Generate encryption key based on system properties
        encryption_key_ = getEncryptionKey();
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Configuration error: " + std::string(e.what()));
    }
}

bool LoginManager::initializeDefaultCredentials() {
    std::lock_guard<std::mutex> lock(credentials_mutex_);
    
    UserCredentials default_user;
    default_user.username = DEFAULT_USERNAME;
    default_user.salt = generateSalt();
    default_user.password_hash = hashPassword(DEFAULT_PASSWORD, default_user.salt);
    default_user.remember_me_enabled = false;
    default_user.created_at = std::chrono::system_clock::now();
    
    users_[DEFAULT_USERNAME] = default_user;
    
    return saveCredentials();
}

LoginManager::LoginResult LoginManager::authenticate(const std::string& username, 
                                                    const std::string& password, 
                                                    bool remember_me,
                                                    const std::string& client_ip) {
    LoginResult result;
    result.success = false;
    result.remaining_attempts = max_login_attempts_;
    
    // Check if IP is locked out
    if (isIpLocked(client_ip)) {
        result.message = "Account temporarily locked due to multiple failed attempts";
        return result;
    }
    
    std::lock_guard<std::mutex> lock(credentials_mutex_);
    
    // Find user
    auto user_it = users_.find(username);
    if (user_it == users_.end()) {
        recordLoginAttempt(client_ip, false);
        result.message = "Invalid username or password";
        result.remaining_attempts = max_login_attempts_ - login_attempts_[client_ip].attempts;
        return result;
    }
    
    // Verify password
    std::string computed_hash = hashPassword(password, user_it->second.salt);
    if (computed_hash != user_it->second.password_hash) {
        recordLoginAttempt(client_ip, false);
        result.message = "Invalid username or password";
        result.remaining_attempts = max_login_attempts_ - login_attempts_[client_ip].attempts;
        return result;
    }
    
    // Authentication successful
    recordLoginAttempt(client_ip, true);
    
    // Create session
    SessionInfo session;
    session.session_token = generateSessionToken();
    session.username = username;
    session.created_at = std::chrono::steady_clock::now();
    session.remember_me = remember_me;
    
    // Set expiration based on remember_me
    auto duration = remember_me ? std::chrono::hours(24 * 30) : std::chrono::seconds(session_timeout_seconds_);
    session.expires_at = session.created_at + duration;
    
    {
        std::lock_guard<std::mutex> session_lock(sessions_mutex_);
        active_sessions_[session.session_token] = session;
    }
    
    // Update last login time
    user_it->second.last_login = std::chrono::system_clock::now();
    saveCredentials();
    
    result.success = true;
    result.message = "Login successful";
    result.session_token = session.session_token;
    result.remaining_attempts = max_login_attempts_;
    
    return result;
}

bool LoginManager::validateSession(const std::string& session_token) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto session_it = active_sessions_.find(session_token);
    if (session_it == active_sessions_.end()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now > session_it->second.expires_at) {
        active_sessions_.erase(session_it);
        return false;
    }
    
    return true;
}

bool LoginManager::logout(const std::string& session_token) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto session_it = active_sessions_.find(session_token);
    if (session_it != active_sessions_.end()) {
        active_sessions_.erase(session_it);
        return true;
    }
    
    return false;
}

std::string LoginManager::getUsernameFromSession(const std::string& session_token) {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto session_it = active_sessions_.find(session_token);
    if (session_it != active_sessions_.end()) {
        return session_it->second.username;
    }
    
    return "";
}

void LoginManager::cleanupExpiredSessions() {
    std::lock_guard<std::mutex> lock(sessions_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = active_sessions_.begin(); it != active_sessions_.end();) {
        if (now > it->second.expires_at) {
            it = active_sessions_.erase(it);
        } else {
            ++it;
        }
    }
}

bool LoginManager::isIpLocked(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(attempts_mutex_);
    
    auto attempt_it = login_attempts_.find(client_ip);
    if (attempt_it == login_attempts_.end()) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    if (now < attempt_it->second.lockout_until) {
        return true;
    }
    
    return false;
}

void LoginManager::resetLoginAttempts(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(attempts_mutex_);
    login_attempts_.erase(client_ip);
}

void LoginManager::recordLoginAttempt(const std::string& client_ip, bool success) {
    std::lock_guard<std::mutex> lock(attempts_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto& attempt = login_attempts_[client_ip];
    
    if (success) {
        attempt = LoginAttempt{}; // Reset on success
        return;
    }
    
    attempt.attempts++;
    attempt.last_attempt = now;
    
    if (attempt.attempts >= max_login_attempts_) {
        attempt.lockout_until = now + std::chrono::seconds(lockout_duration_seconds_);
    }
}

std::string LoginManager::generateSalt() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < SALT_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    
    return ss.str();
}

std::string LoginManager::hashPassword(const std::string& password, const std::string& salt) {
    std::string input = password + salt;
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.c_str(), input.length());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

std::string LoginManager::generateSessionToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < TOKEN_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    
    return ss.str();
}

bool LoginManager::loadCredentials() {
    std::lock_guard<std::mutex> lock(credentials_mutex_);
    
    try {
        std::ifstream file(credentials_file_path_, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        
        std::string decrypted_data = decryptData(buffer.str(), encryption_key_);
        if (decrypted_data.empty()) {
            return false;
        }
        
        nlohmann::json credentials_json = nlohmann::json::parse(decrypted_data);
        
        users_.clear();
        for (const auto& user_data : credentials_json["users"]) {
            UserCredentials user;
            user.username = user_data["username"];
            user.password_hash = user_data["password_hash"];
            user.salt = user_data["salt"];
            user.remember_me_enabled = user_data.value("remember_me_enabled", false);
            
            // Parse timestamps
            if (user_data.contains("created_at")) {
                auto created_time = std::chrono::system_clock::from_time_t(user_data["created_at"]);
                user.created_at = created_time;
            }
            
            if (user_data.contains("last_login")) {
                auto login_time = std::chrono::system_clock::from_time_t(user_data["last_login"]);
                user.last_login = login_time;
            }
            
            users_[user.username] = user;
        }
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

bool LoginManager::saveCredentials() {
    std::lock_guard<std::mutex> lock(credentials_mutex_);
    
    try {
        // Ensure the directory exists
        std::string dir_path = credentials_file_path_.substr(0, credentials_file_path_.find_last_of('/'));
        if (!dir_path.empty()) {
            system(("mkdir -p " + dir_path).c_str());
        }
        nlohmann::json credentials_json;
        credentials_json["users"] = nlohmann::json::array();
        
        for (const auto& user_pair : users_) {
            const UserCredentials& user = user_pair.second;
            
            nlohmann::json user_json;
            user_json["username"] = user.username;
            user_json["password_hash"] = user.password_hash;
            user_json["salt"] = user.salt;
            user_json["remember_me_enabled"] = user.remember_me_enabled;
            
            auto created_time_t = std::chrono::system_clock::to_time_t(user.created_at);
            auto login_time_t = std::chrono::system_clock::to_time_t(user.last_login);
            
            user_json["created_at"] = created_time_t;
            user_json["last_login"] = login_time_t;
            
            credentials_json["users"].push_back(user_json);
        }
        
        std::string json_string = credentials_json.dump(4);
        std::string encrypted_data = encryptData(json_string, encryption_key_);
        
        std::ofstream file(credentials_file_path_, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }
        
        file << encrypted_data;
        file.close();
        
        return true;
        
    } catch (const std::exception& e) {
        return false;
    }
}

std::string LoginManager::encryptData(const std::string& data, const std::string& key) {
    // Simplified encryption using AES-256-CBC
    // In production, consider using more robust encryption libraries
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    
    unsigned char iv[16];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    unsigned char key_bytes[32];
    memset(key_bytes, 0, sizeof(key_bytes));
    memcpy(key_bytes, key.c_str(), std::min(key.length(), sizeof(key_bytes)));
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key_bytes, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    std::vector<unsigned char> encrypted_data(data.length() + AES_BLOCK_SIZE);
    int len = 0, final_len = 0;
    
    if (EVP_EncryptUpdate(ctx, encrypted_data.data(), &len, 
                         reinterpret_cast<const unsigned char*>(data.c_str()), 
                         data.length()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    if (EVP_EncryptFinal_ex(ctx, encrypted_data.data() + len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    // Prepend IV to encrypted data
    std::string result;
    result.append(reinterpret_cast<const char*>(iv), sizeof(iv));
    result.append(reinterpret_cast<const char*>(encrypted_data.data()), len + final_len);
    
    return result;
}

std::string LoginManager::decryptData(const std::string& encrypted_data, const std::string& key) {
    if (encrypted_data.length() < 16) return "";
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return "";
    
    // Extract IV from beginning of encrypted data
    const unsigned char* iv = reinterpret_cast<const unsigned char*>(encrypted_data.c_str());
    const unsigned char* ciphertext = iv + 16;
    int ciphertext_len = encrypted_data.length() - 16;
    
    unsigned char key_bytes[32];
    memset(key_bytes, 0, sizeof(key_bytes));
    memcpy(key_bytes, key.c_str(), std::min(key.length(), sizeof(key_bytes)));
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key_bytes, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    std::vector<unsigned char> decrypted_data(ciphertext_len + AES_BLOCK_SIZE);
    int len = 0, final_len = 0;
    
    if (EVP_DecryptUpdate(ctx, decrypted_data.data(), &len, ciphertext, ciphertext_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    if (EVP_DecryptFinal_ex(ctx, decrypted_data.data() + len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return "";
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    return std::string(reinterpret_cast<const char*>(decrypted_data.data()), len + final_len);
}

std::string LoginManager::getEncryptionKey() {
    // Generate a consistent key based on system properties
    // In production, consider using a proper key derivation function
    std::string base_key = "UR_WEBIF_API_ENCRYPTION_KEY_2024";
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, base_key.c_str(), base_key.length());
    SHA256_Final(hash, &sha256);
    
    return std::string(reinterpret_cast<const char*>(hash), SHA256_DIGEST_LENGTH);
}