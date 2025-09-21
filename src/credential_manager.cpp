#include "credential_manager.h"
#include "endpoint_logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <iomanip>

using json = nlohmann::json;

CredentialManager::CredentialManager(const std::string& config_path) 
    : config_path_(config_path) {
    
    std::cout << "[CREDENTIAL-MANAGER] Initializing credential manager..." << std::endl;
    
    // Load configuration
    loadConfig();
    
    // Ensure credentials file exists with default credentials
    if (!ensureCredentialsFileExists()) {
        std::cout << "[CREDENTIAL-MANAGER] Warning: Could not ensure credentials file exists" << std::endl;
    }
    
    // Load existing credentials
    if (!loadCredentials()) {
        std::cout << "[CREDENTIAL-MANAGER] Warning: Could not load credentials" << std::endl;
    }
    
    std::cout << "[CREDENTIAL-MANAGER] Credential manager initialized successfully" << std::endl;
}

CredentialManager::LoginResult CredentialManager::authenticate(
    const std::string& username, const std::string& password) {
    
    std::cout << "[CREDENTIAL-MANAGER] Authenticating user: " << username << std::endl;
    
    LoginResult result;
    result.success = false;
    result.username = username;
    result.is_banned = false;
    result.lockout_remaining_seconds = 0;
    
    // Get max attempts from config
    int max_attempts = config_.value("max_login_attempts", 5);
    result.total_attempts = max_attempts;
    result.attempts_remaining = getRemainingAttempts(username);
    
    // Check if user is currently banned
    if (isUserBanned(username)) {
        result.is_banned = true;
        result.lockout_remaining_seconds = getLockoutRemainingSeconds(username);
        result.message = "Account temporarily locked due to too many failed attempts. Try again later.";
        std::cout << "[CREDENTIAL-MANAGER] Authentication blocked: User is banned" << std::endl;
        return result;
    }
    
    // Check if user exists in credentials
    auto user_it = credentials_.find(username);
    if (user_it == credentials_.end()) {
        result.message = "Username not found";
        recordFailedAttempt(username);
        result.attempts_remaining = getRemainingAttempts(username);
        std::cout << "[CREDENTIAL-MANAGER] Authentication failed: Username not found" << std::endl;
        return result;
    }
    
    // Verify password (simple hash comparison for now)
    std::string stored_hash = user_it->second;
    std::string provided_hash = hashPassword(password, username); // Use username as salt for simplicity
    
    if (stored_hash == provided_hash) {
        // Successful login - clear failed attempts
        clearFailedAttempts(username);
        
        result.success = true;
        result.message = "Authentication successful";
        result.session_token = generateSessionToken(username);
        result.user_data["role"] = (username == "admin") ? "admin" : "user";
        result.user_data["login_time"] = std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        result.attempts_remaining = max_attempts;
        
        // Store active session
        active_sessions_[result.session_token] = username;
        
        std::cout << "[CREDENTIAL-MANAGER] Authentication successful for user: " << username << std::endl;
    } else {
        // Failed login - record attempt
        recordFailedAttempt(username);
        result.attempts_remaining = getRemainingAttempts(username);
        result.message = "Invalid password";
        
        if (result.attempts_remaining <= 0) {
            result.is_banned = true;
            result.lockout_remaining_seconds = getLockoutRemainingSeconds(username);
            result.message = "Account temporarily locked due to too many failed attempts";
        }
        
        std::cout << "[CREDENTIAL-MANAGER] Authentication failed: Invalid password. Attempts remaining: " 
                  << result.attempts_remaining << std::endl;
    }
    
    return result;
}

bool CredentialManager::validateSession(const std::string& session_token) {
    return active_sessions_.find(session_token) != active_sessions_.end();
}

std::string CredentialManager::generateSessionToken(const std::string& username) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    std::ostringstream oss;
    oss << "session_" << timestamp << "_" << username << "_" << dis(gen);
    return oss.str();
}

void CredentialManager::invalidateSession(const std::string& session_token) {
    active_sessions_.erase(session_token);
}

bool CredentialManager::updatePassword(const std::string& username, const std::string& new_password) {
    ENDPOINT_LOG_INFO("credential", "Updating password for user: " + username);
    
    try {
        auto user_it = credentials_.find(username);
        if (user_it == credentials_.end()) {
            ENDPOINT_LOG_ERROR("credential", "User not found for password update: " + username);
            return false;
        }
        
        // Hash new password with username as salt
        std::string new_hash = hashPassword(new_password, username);
        
        // Update the password hash
        credentials_[username] = new_hash;
        
        // Save updated credentials back to file
        if (saveCredentialsToFile()) {
            ENDPOINT_LOG_INFO("credential", "Password updated successfully for user: " + username);
            return true;
        } else {
            ENDPOINT_LOG_ERROR("credential", "Failed to save updated credentials to file");
            return false;
        }
        
    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("credential", "Error updating password: " + std::string(e.what()));
        return false;
    }
}

bool CredentialManager::saveCredentialsToFile() {
    try {
        // Create credentials JSON structure
        json credentials_data;
        credentials_data["credentials"] = json::object();
        
        for (const auto& [username, password_hash] : credentials_) {
            credentials_data["credentials"][username] = password_hash;
        }
        
        credentials_data["metadata"] = {
            {"last_updated", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()},
            {"version", "1.0"},
            {"encryption", "simple_hash"}
        };
        
        // Encrypt the credentials data
        std::string encryption_key = generateEncryptionKey();
        std::string encrypted_data = encrypt(credentials_data.dump(), encryption_key);
        
        // Create the encrypted credentials file structure
        json credential_file;
        credential_file["encrypted"] = true;
        credential_file["data"] = encrypted_data;
        credential_file["key_hint"] = "ur-webif-default-key";
        credential_file["created"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // Write to file
        if (writeFile(credentials_file_path_, credential_file.dump(4))) {
            std::cout << "[CREDENTIAL-MANAGER] Credentials file updated successfully" << std::endl;
            return true;
        }
        
        std::cout << "[CREDENTIAL-MANAGER] Failed to write updated credentials file" << std::endl;
        return false;
        
    } catch (const std::exception& e) {
        std::cout << "[CREDENTIAL-MANAGER] Error saving credentials: " << e.what() << std::endl;
        return false;
    }
}

bool CredentialManager::ensureCredentialsFileExists() {
    if (!fileExists(credentials_file_path_)) {
        std::cout << "[CREDENTIAL-MANAGER] Credentials file not found, creating default credentials..." << std::endl;
        return createDefaultCredentials();
    } else {
        // Check if file is empty or corrupted
        std::string content = readFile(credentials_file_path_);
        if (content.empty() || content == "\n") {
            std::cout << "[CREDENTIAL-MANAGER] Credentials file is empty, creating default credentials..." << std::endl;
            return createDefaultCredentials();
        }
    }
    return true;
}

bool CredentialManager::createDefaultCredentials() {
    std::cout << "[CREDENTIAL-MANAGER] Creating default credentials file with admin/admin..." << std::endl;
    
    // Create default credentials with admin/admin
    json default_creds;
    std::string admin_password_hash = hashPassword("admin", "admin");
    
    default_creds["credentials"] = {
        {"admin", admin_password_hash}
    };
    
    default_creds["metadata"] = {
        {"created", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()},
        {"version", "1.0"},
        {"encryption", "simple_hash"}
    };
    
    // Encrypt the credentials data
    std::string encryption_key = generateEncryptionKey();
    std::string encrypted_data = encrypt(default_creds.dump(), encryption_key);
    
    // Create the encrypted credentials file structure
    json credential_file;
    credential_file["encrypted"] = true;
    credential_file["data"] = encrypted_data;
    credential_file["key_hint"] = "ur-webif-default-key";
    credential_file["created"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // Write to file
    if (writeFile(credentials_file_path_, credential_file.dump(4))) {
        std::cout << "[CREDENTIAL-MANAGER] Default credentials file created successfully" << std::endl;
        return true;
    }
    
    std::cout << "[CREDENTIAL-MANAGER] Failed to create default credentials file" << std::endl;
    return false;
}

bool CredentialManager::loadCredentials() {
    if (!fileExists(credentials_file_path_)) {
        return false;
    }
    
    try {
        std::string file_content = readFile(credentials_file_path_);
        json credential_file = json::parse(file_content);
        
        if (credential_file["encrypted"] == true) {
            // Decrypt the credentials
            std::string encrypted_data = credential_file["data"];
            std::string encryption_key = generateEncryptionKey(); // Use same key generation
            std::string decrypted_data = decrypt(encrypted_data, encryption_key);
            
            json credentials_json = json::parse(decrypted_data);
            
            // Load credentials into memory
            if (credentials_json.contains("credentials")) {
                for (const auto& [username, password_hash] : credentials_json["credentials"].items()) {
                    credentials_[username] = password_hash;
                }
                
                std::cout << "[CREDENTIAL-MANAGER] Loaded " << credentials_.size() 
                         << " user credentials" << std::endl;
                return true;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "[CREDENTIAL-MANAGER] Error loading credentials: " << e.what() << std::endl;
    }
    
    return false;
}

void CredentialManager::loadConfig() {
    try {
        if (fileExists(config_path_)) {
            std::string config_content = readFile(config_path_);
            config_ = json::parse(config_content);
            credentials_file_path_ = config_.value("credentials_file", "./config/credentials.enc");
        } else {
            setDefaultConfig();
        }
    } catch (const std::exception& e) {
        std::cout << "[CREDENTIAL-MANAGER] Error loading config: " << e.what() << std::endl;
        setDefaultConfig();
    }
}

void CredentialManager::setDefaultConfig() {
    config_ = {
        {"credentials_file", "./config/credentials.enc"},
        {"session_timeout", 3600},
        {"max_login_attempts", 5},
        {"lockout_duration", 300},
        {"encryption_enabled", true}
    };
    credentials_file_path_ = "./config/credentials.enc";
}

// Simple encryption implementation (for demo purposes)
std::string CredentialManager::encrypt(const std::string& data, const std::string& key) {
    std::string result = data;
    for (size_t i = 0; i < result.length(); ++i) {
        result[i] ^= key[i % key.length()];
    }
    
    // Convert to hex for storage
    std::ostringstream hex_stream;
    for (unsigned char c : result) {
        hex_stream << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    }
    return hex_stream.str();
}

std::string CredentialManager::decrypt(const std::string& encrypted_data, const std::string& key) {
    // Convert from hex
    std::string binary_data;
    for (size_t i = 0; i < encrypted_data.length(); i += 2) {
        std::string byte_str = encrypted_data.substr(i, 2);
        unsigned char byte = static_cast<unsigned char>(std::stoi(byte_str, nullptr, 16));
        binary_data.push_back(byte);
    }
    
    // Decrypt
    std::string result = binary_data;
    for (size_t i = 0; i < result.length(); ++i) {
        result[i] ^= key[i % key.length()];
    }
    return result;
}

std::string CredentialManager::generateEncryptionKey() {
    return "ur-webif-default-encryption-key-2025";
}

std::string CredentialManager::hashPassword(const std::string& password, const std::string& salt) {
    // Simple hash implementation (use proper hashing in production)
    std::string combined = salt + password + "ur-webif-salt";
    std::hash<std::string> hasher;
    return std::to_string(hasher(combined));
}

std::string CredentialManager::generateSalt() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::ostringstream salt;
    for (int i = 0; i < 16; ++i) {
        salt << std::hex << dis(gen);
    }
    return salt.str();
}

bool CredentialManager::fileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string CredentialManager::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    return content.str();
}

bool CredentialManager::writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    
    file << content;
    return file.good();
}

// Login attempt tracking methods
void CredentialManager::recordFailedAttempt(const std::string& username, const std::string& client_ip) {
    auto now = std::chrono::system_clock::now();
    
    if (failed_attempts_.find(username) == failed_attempts_.end()) {
        failed_attempts_[username] = {0, now, now};
    }
    
    failed_attempts_[username].count++;
    failed_attempts_[username].last_attempt = now;
    
    // Check if user should be locked out
    int max_attempts = config_.value("max_login_attempts", 5);
    if (failed_attempts_[username].count >= max_attempts) {
        int lockout_duration = config_.value("lockout_duration", 300); // 5 minutes default
        failed_attempts_[username].lockout_until = now + std::chrono::seconds(lockout_duration);
        
        std::cout << "[CREDENTIAL-MANAGER] User " << username << " locked out for " 
                  << lockout_duration << " seconds after " << failed_attempts_[username].count 
                  << " failed attempts" << std::endl;
    }
    
    std::cout << "[CREDENTIAL-MANAGER] Failed attempt recorded for " << username 
              << ". Total attempts: " << failed_attempts_[username].count << std::endl;
}

void CredentialManager::clearFailedAttempts(const std::string& username) {
    auto it = failed_attempts_.find(username);
    if (it != failed_attempts_.end()) {
        std::cout << "[CREDENTIAL-MANAGER] Clearing failed attempts for " << username << std::endl;
        failed_attempts_.erase(it);
    }
}

bool CredentialManager::isUserBanned(const std::string& username) {
    auto it = failed_attempts_.find(username);
    if (it == failed_attempts_.end()) {
        return false;
    }
    
    auto now = std::chrono::system_clock::now();
    bool is_banned = now < it->second.lockout_until;
    
    // If lockout period has expired, clear the failed attempts
    if (!is_banned && it->second.count >= config_.value("max_login_attempts", 5)) {
        clearFailedAttempts(username);
    }
    
    return is_banned;
}

int CredentialManager::getRemainingAttempts(const std::string& username) {
    auto it = failed_attempts_.find(username);
    if (it == failed_attempts_.end()) {
        return config_.value("max_login_attempts", 5);
    }
    
    int max_attempts = config_.value("max_login_attempts", 5);
    return std::max(0, max_attempts - it->second.count);
}

int CredentialManager::getLockoutRemainingSeconds(const std::string& username) {
    auto it = failed_attempts_.find(username);
    if (it == failed_attempts_.end()) {
        return 0;
    }
    
    auto now = std::chrono::system_clock::now();
    if (now >= it->second.lockout_until) {
        return 0;
    }
    
    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
        it->second.lockout_until - now);
    return static_cast<int>(remaining.count());
}