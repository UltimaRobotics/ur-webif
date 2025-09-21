#include "auth_access_generator.h"
#include "endpoint_logger.h"
#include "config_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <random>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/buffer.h>

AuthAccessGenerator::AuthAccessGenerator() 
    : server_endpoint_("http://localhost:8080") {
    // Set output directory using configurable data directory
    output_directory_ = ConfigManager::getInstance().getDataPath("auth-gen");

    // Generate default encryption key
    encryption_key_ = generateEncryptionKey();

    // Ensure output directory exists
    ensureDirectoryExists(output_directory_);

    ENDPOINT_LOG_INFO("auth_gen", "AuthAccessGenerator initialized");
}

AuthAccessGenerator::GenerationResult AuthAccessGenerator::generateAuthAccessFile(
    const std::string& username,
    const std::string& password_hash,
    const std::string& token_name,
    int expiry_days,
    const std::string& access_level) {

    GenerationResult result;
    result.success = false;

    try {
        ENDPOINT_LOG_INFO("auth_gen", "Generating auth access file for user: " + username);

        // Validate input parameters
        if (username.empty() || password_hash.empty() || token_name.empty()) {
            result.message = "Missing required parameters";
            ENDPOINT_LOG_ERROR("auth_gen", result.message);
            return result;
        }

        if (expiry_days <= 0 || expiry_days > 365) {
            result.message = "Invalid expiry days (must be 1-365)";
            ENDPOINT_LOG_ERROR("auth_gen", result.message);
            return result;
        }

        // Create auth access data
        AuthAccessData auth_data;
        auth_data.username = username;
        auth_data.password_hash = password_hash;
        auth_data.server_endpoint = server_endpoint_;
        auth_data.token_name = token_name;
        auth_data.created_timestamp = getCurrentTimestamp();
        auth_data.expiry_timestamp = auth_data.created_timestamp + (static_cast<int64_t>(expiry_days) * 24 * 60 * 60 * 1000);
        auth_data.access_level = access_level;

        // Add metadata
        auth_data.metadata["generator_version"] = "2.0";
        auth_data.metadata["platform"] = "ur-webif-api";
        auth_data.metadata["creation_method"] = "web_interface";
        auth_data.metadata["encryption_method"] = "AES-256-CBC";

        // Convert to JSON
        json auth_json;
        auth_json["username"] = auth_data.username;
        auth_json["password_hash"] = auth_data.password_hash;
        auth_json["server_endpoint"] = auth_data.server_endpoint;
        auth_json["token_name"] = auth_data.token_name;
        auth_json["created_timestamp"] = auth_data.created_timestamp;
        auth_json["expiry_timestamp"] = auth_data.expiry_timestamp;
        auth_json["access_level"] = auth_data.access_level;
        auth_json["metadata"] = auth_data.metadata;

        // Generate file name and path
        std::string file_name = generateFileName(username, token_name);
        std::string file_path = output_directory_ + "/" + file_name;

        ENDPOINT_LOG_INFO("auth_gen", "Generating file: " + file_path);

        // Encrypt and write file
        std::string json_string = auth_json.dump(2); // Pretty print JSON
        ENDPOINT_LOG_INFO("auth_gen", "JSON content created, length: " + std::to_string(json_string.length()));
        
        std::string encrypted_content = encryptData(json_string, encryption_key_);
        
        if (encrypted_content.empty()) {
            result.message = "Failed to encrypt data";
            ENDPOINT_LOG_ERROR("auth_gen", result.message);
            return result;
        }

        ENDPOINT_LOG_INFO("auth_gen", "Content encrypted, length: " + std::to_string(encrypted_content.length()));

        if (writeEncryptedFile(file_path, encrypted_content)) {
            // Verify file was written correctly
            if (!std::filesystem::exists(file_path)) {
                result.message = "File was not created";
                ENDPOINT_LOG_ERROR("auth_gen", result.message);
                return result;
            }

            auto file_size = std::filesystem::file_size(file_path);
            if (file_size == 0) {
                result.message = "Generated file is empty";
                ENDPOINT_LOG_ERROR("auth_gen", result.message);
                return result;
            }

            // Read file for base64 encoding
            std::ifstream file(file_path, std::ios::binary);
            if (file.is_open()) {
                std::string file_content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                file.close();

                result.success = true;
                result.message = "Auth access file generated successfully";
                result.file_path = file_path;
                result.file_name = file_name;
                result.file_size = file_content.length();
                result.content_base64 = toBase64(file_content);
                result.download_token = createDownloadToken(file_path);

                ENDPOINT_LOG_INFO("auth_gen", "Auth access file created successfully: " + file_path);
                ENDPOINT_LOG_INFO("auth_gen", "File size: " + std::to_string(result.file_size) + " bytes");
            } else {
                result.message = "Failed to read generated file";
                ENDPOINT_LOG_ERROR("auth_gen", result.message);
            }
        } else {
            result.message = "Failed to write encrypted file";
            ENDPOINT_LOG_ERROR("auth_gen", result.message);
        }

    } catch (const std::exception& e) {
        result.message = "Error generating auth access file: " + std::string(e.what());
        ENDPOINT_LOG_ERROR("auth_gen", result.message);
    }

    return result;
}

bool AuthAccessGenerator::validateAuthAccessFile(const std::string& file_path) {
    try {
        std::string encrypted_content = readEncryptedFile(file_path);
        if (encrypted_content.empty()) {
            return false;
        }

        std::string decrypted_content = decryptData(encrypted_content, encryption_key_);
        if (decrypted_content.empty()) {
            return false;
        }

        json auth_json = json::parse(decrypted_content);

        // Validate required fields
        if (!auth_json.contains("username") || !auth_json.contains("password_hash") ||
            !auth_json.contains("created_timestamp") || !auth_json.contains("expiry_timestamp")) {
            return false;
        }

        // Check if expired
        int64_t current_time = getCurrentTimestamp();
        int64_t expiry_time = auth_json["expiry_timestamp"];

        return current_time < expiry_time;

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Error validating auth access file: " + std::string(e.what()));
        return false;
    }
}

AuthAccessGenerator::AuthAccessData AuthAccessGenerator::decryptAuthAccessFile(const std::string& file_path) {
    AuthAccessData auth_data;

    try {
        std::string encrypted_content = readEncryptedFile(file_path);
        std::string decrypted_content = decryptData(encrypted_content, encryption_key_);

        json auth_json = json::parse(decrypted_content);

        auth_data.username = auth_json.value("username", "");
        auth_data.password_hash = auth_json.value("password_hash", "");
        auth_data.server_endpoint = auth_json.value("server_endpoint", "");
        auth_data.token_name = auth_json.value("token_name", "");
        auth_data.created_timestamp = auth_json.value("created_timestamp", 0);
        auth_data.expiry_timestamp = auth_json.value("expiry_timestamp", 0);
        auth_data.access_level = auth_json.value("access_level", "standard");

        if (auth_json.contains("metadata")) {
            for (const auto& [key, value] : auth_json["metadata"].items()) {
                auth_data.metadata[key] = value;
            }
        }

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Error decrypting auth access file: " + std::string(e.what()));
    }

    return auth_data;
}

std::string AuthAccessGenerator::createDownloadToken(const std::string& file_path) {
    std::string token = generateDownloadToken();
    int64_t expiry = getCurrentTimestamp() + (10 * 60 * 1000); // 10 minutes

    download_tokens_[token] = {file_path, expiry};

    ENDPOINT_LOG_INFO("auth_gen", "Created download token for file: " + file_path);
    return token;
}

bool AuthAccessGenerator::validateDownloadToken(const std::string& token) {
    auto it = download_tokens_.find(token);
    if (it == download_tokens_.end()) {
        return false;
    }

    int64_t current_time = getCurrentTimestamp();
    if (current_time > it->second.second) {
        download_tokens_.erase(it);
        return false;
    }

    return true;
}

std::string AuthAccessGenerator::getFilePathFromToken(const std::string& token) {
    auto it = download_tokens_.find(token);
    if (it != download_tokens_.end()) {
        return it->second.first;
    }
    return "";
}

void AuthAccessGenerator::cleanupExpiredTokens() {
    int64_t current_time = getCurrentTimestamp();

    for (auto it = download_tokens_.begin(); it != download_tokens_.end();) {
        if (current_time > it->second.second) {
            it = download_tokens_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string AuthAccessGenerator::encryptData(const std::string& data, const std::string& key) {
    try {
        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";

        // Generate IV
        unsigned char iv[16];
        if (RAND_bytes(iv, sizeof(iv)) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }

        // Initialize encryption
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                              reinterpret_cast<const unsigned char*>(key.c_str()), iv) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }

        // Encrypt data
        int len;
        int ciphertext_len;
        std::vector<unsigned char> ciphertext(data.length() + AES_BLOCK_SIZE);

        if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, 
                             reinterpret_cast<const unsigned char*>(data.c_str()), data.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        ciphertext_len = len;

        if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        ciphertext_len += len;

        EVP_CIPHER_CTX_free(ctx);

        // Prepend IV to ciphertext
        std::string result(reinterpret_cast<char*>(iv), 16);
        result.append(reinterpret_cast<char*>(ciphertext.data()), ciphertext_len);

        return toBase64(result);

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Encryption error: " + std::string(e.what()));
        return "";
    }
}

std::string AuthAccessGenerator::decryptData(const std::string& encrypted_data, const std::string& key) {
    try {
        std::string binary_data = fromBase64(encrypted_data);
        if (binary_data.length() < 16) return "";

        // Extract IV
        unsigned char iv[16];
        std::copy(binary_data.begin(), binary_data.begin() + 16, iv);

        // Extract ciphertext
        std::string ciphertext = binary_data.substr(16);

        EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";

        // Initialize decryption
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                              reinterpret_cast<const unsigned char*>(key.c_str()), iv) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }

        // Decrypt data
        int len;
        int plaintext_len;
        std::vector<unsigned char> plaintext(ciphertext.length() + AES_BLOCK_SIZE);

        if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, 
                             reinterpret_cast<const unsigned char*>(ciphertext.c_str()), ciphertext.length()) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        plaintext_len = len;

        if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
            EVP_CIPHER_CTX_free(ctx);
            return "";
        }
        plaintext_len += len;

        EVP_CIPHER_CTX_free(ctx);

        return std::string(reinterpret_cast<char*>(plaintext.data()), plaintext_len);

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Decryption error: " + std::string(e.what()));
        return "";
    }
}

std::string AuthAccessGenerator::generateFileName(const std::string& username, const std::string& token_name) {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    std::string sanitized_username = username;
    std::string sanitized_token = token_name;

    // Replace invalid characters
    std::replace_if(sanitized_username.begin(), sanitized_username.end(), 
                   [](char c) { return !std::isalnum(c); }, '_');
    std::replace_if(sanitized_token.begin(), sanitized_token.end(), 
                   [](char c) { return !std::isalnum(c); }, '_');

    std::ostringstream oss;
    oss << sanitized_username << "_" << sanitized_token << "_" << timestamp << ".uacc";
    return oss.str();
}

bool AuthAccessGenerator::writeEncryptedFile(const std::string& file_path, const std::string& content) {
    try {
        std::ofstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        file.write(content.c_str(), content.length());
        file.close();

        return file.good();

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Error writing file: " + std::string(e.what()));
        return false;
    }
}

std::string AuthAccessGenerator::readEncryptedFile(const std::string& file_path) {
    try {
        std::ifstream file(file_path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        file.close();

        return content;

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Error reading file: " + std::string(e.what()));
        return "";
    }
}

std::string AuthAccessGenerator::generateDownloadToken() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);

    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string token;

    for (int i = 0; i < 32; ++i) {
        token += chars[dis(gen)];
    }

    return token;
}

std::string AuthAccessGenerator::generateEncryptionKey() {
    // Generate a proper 32-byte encryption key
    std::string key = "ur-webif-auth-key-2025-32-bytes!!";
    if (key.length() < 32) {
        key.resize(32, '0'); // Pad with zeros if needed
    } else if (key.length() > 32) {
        key = key.substr(0, 32); // Truncate if too long
    }
    return key;
}

bool AuthAccessGenerator::ensureDirectoryExists(const std::string& directory) {
    try {
        return std::filesystem::create_directories(directory);
    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Error creating directory: " + std::string(e.what()));
        return false;
    }
}

std::string AuthAccessGenerator::toBase64(const std::string& data) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data.c_str(), data.length());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

std::string AuthAccessGenerator::fromBase64(const std::string& base64_data) {
    BIO *bio, *b64;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_mem_buf(base64_data.c_str(), -1);
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    std::vector<char> buffer(base64_data.length());
    int length = BIO_read(bio, buffer.data(), base64_data.length());
    BIO_free_all(bio);

    return std::string(buffer.data(), length);
}

int64_t AuthAccessGenerator::getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void AuthAccessGenerator::setOutputDirectory(const std::string& directory) {
    output_directory_ = directory;
    ensureDirectoryExists(directory);
}

void AuthAccessGenerator::setEncryptionKey(const std::string& key) {
    encryption_key_ = key;
}

void AuthAccessGenerator::setServerEndpoint(const std::string& endpoint) {
    server_endpoint_ = endpoint;
}

bool AuthAccessGenerator::deleteAuthAccessFile(const std::string& file_path) {
    try {
        return std::filesystem::remove(file_path);
    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("auth_gen", "Error deleting file: " + std::string(e.what()));
        return false;
    }
}