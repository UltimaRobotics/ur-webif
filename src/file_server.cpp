#include "file_server.h"
#include "endpoint_logger.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <map>
#include <algorithm>
#include <cstring>
#include <random>
#include <functional>

FileServer::FileServer(const std::string& document_root, const std::string& default_file)
    : document_root_(document_root), default_file_(default_file), 
      cache_enabled_(true), cache_max_age_(300) { // 5 minute default cache
    ENDPOINT_LOG("file_server", "[FILESERVER] Initialized with transaction support and caching");
    ENDPOINT_LOG("file_server", "[FILESERVER] Document root: " + document_root_);
    ENDPOINT_LOG("file_server", "[FILESERVER] Default file: " + default_file_);
}

FileServer::~FileServer() {
    clearCache();
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    active_transactions_.clear();
    ENDPOINT_LOG("file_server", "[FILESERVER] Shutdown complete");
}

enum MHD_Result FileServer::serveFile(struct MHD_Connection* connection, const std::string& requested_path) {
    // MHD requires connection state handling for proper response flow
    return serveFileWithState(connection, requested_path, nullptr);
}

enum MHD_Result FileServer::serveFileWithState(struct MHD_Connection* connection, 
                                              const std::string& requested_path,
                                              void** con_cls) {
    try {
        ENDPOINT_LOG("file_server", "FileServer: Serving request for: " + requested_path);
        std::string sanitized_path = sanitizePath(requested_path);
        ENDPOINT_LOG("file_server", "FileServer: Sanitized path: " + sanitized_path);
        std::string full_path = document_root_ + "/" + sanitized_path;
        ENDPOINT_LOG("file_server", "FileServer: Full path: " + full_path);
        
        // If path is a directory, try to serve the default file
        if (isDirectory(full_path)) {
            ENDPOINT_LOG("file_server", "FileServer: Path is directory, using default file");
            if (full_path.back() != '/') {
                full_path += "/";
            }
            full_path += default_file_;
            ENDPOINT_LOG("file_server", "FileServer: Final path: " + full_path);
        }
        
        // Check if file exists
        if (!fileExists(full_path)) {
            ENDPOINT_LOG("file_server", "FileServer: File does not exist: " + full_path);
            return sendNotFound(connection);
        }
        
        // Read file content with caching support
        ENDPOINT_LOG("file_server", "FileServer: Reading file with cache: " + full_path);
        auto cache_entry = readFileWithCache(full_path);
        if (!cache_entry || cache_entry->content.empty()) {
            ENDPOINT_LOG("file_server", "FileServer: Failed to read file: " + full_path);
            return sendInternalError(connection);
        }
        
        ENDPOINT_LOG("file_server", "FileServer: Successfully read " + std::to_string(cache_entry->content.length()) + " bytes (cached: " + (cache_enabled_ ? "yes" : "no") + ")");
        
        // Use the production-ready sendFileResponse method
        return sendFileResponse(connection, full_path, cache_entry->content);
        
    } catch (const std::exception& e) {
        std::cerr << "Error serving file: " << e.what() << std::endl;
        return sendInternalError(connection);
    }
}

void FileServer::setDocumentRoot(const std::string& document_root) {
    document_root_ = document_root;
}

void FileServer::setDefaultFile(const std::string& default_file) {
    default_file_ = default_file;
}

bool FileServer::fileExists(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_regular_file(path);
}

bool FileServer::isDirectory(const std::string& path) {
    return std::filesystem::exists(path) && std::filesystem::is_directory(path);
}

std::string FileServer::readFile(const std::string& path) {
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }
        
        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
        
    } catch (const std::exception& e) {
        std::cerr << "Error reading file " << path << ": " << e.what() << std::endl;
        return "";
    }
}

std::string FileServer::getMimeType(const std::string& file_path) {
    std::string extension;
    size_t dot_pos = file_path.find_last_of('.');
    if (dot_pos != std::string::npos) {
        extension = file_path.substr(dot_pos + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    }
    
    // Common MIME types
    static const std::map<std::string, std::string> mime_types = {
        {"html", "text/html"},
        {"htm", "text/html"},
        {"css", "text/css"},
        {"js", "application/javascript"},
        {"json", "application/json"},
        {"xml", "application/xml"},
        {"txt", "text/plain"},
        {"png", "image/png"},
        {"jpg", "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"gif", "image/gif"},
        {"svg", "image/svg+xml"},
        {"ico", "image/x-icon"},
        {"pdf", "application/pdf"},
        {"zip", "application/zip"},
        {"tar", "application/x-tar"},
        {"gz", "application/gzip"},
        {"mp3", "audio/mpeg"},
        {"mp4", "video/mp4"},
        {"avi", "video/x-msvideo"},
        {"mov", "video/quicktime"},
        {"woff", "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf", "font/ttf"},
        {"eot", "application/vnd.ms-fontobject"}
    };
    
    auto it = mime_types.find(extension);
    if (it != mime_types.end()) {
        return it->second;
    }
    
    return "application/octet-stream";
}

// ======== NEW CACHE AND TRANSACTION METHODS ========

std::shared_ptr<CacheEntry> FileServer::readFileWithCache(const std::string& path) {
    if (!cache_enabled_) {
        // Cache disabled, read directly
        auto entry = std::make_shared<CacheEntry>();
        entry->content = readFile(path);
        entry->file_size = entry->content.length();
        entry->etag = generateETag(path, entry->content);
        return entry;
    }
    
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    // Check if file is in cache and still valid
    auto cache_it = file_cache_.find(path);
    if (cache_it != file_cache_.end()) {
        auto& entry = cache_it->second;
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - entry->cache_time).count();
        
        if (age < cache_max_age_) {
            ENDPOINT_LOG("file_server", "[FILESERVER-CACHE] Cache hit for: " + path + " (age: " + std::to_string(age) + "s)");
            return entry;
        } else {
            ENDPOINT_LOG("file_server", "[FILESERVER-CACHE] Cache expired for: " + path + " (age: " + std::to_string(age) + "s)");
            file_cache_.erase(cache_it);
        }
    }
    
    // Read file and cache it
    ENDPOINT_LOG("file_server", "[FILESERVER-CACHE] Cache miss, reading: " + path);
    auto entry = std::make_shared<CacheEntry>();
    entry->content = readFile(path);
    entry->file_size = entry->content.length();
    entry->etag = generateETag(path, entry->content);
    entry->cache_time = std::chrono::steady_clock::now();
    
    if (!entry->content.empty()) {
        file_cache_[path] = entry;
        ENDPOINT_LOG("file_server", "[FILESERVER-CACHE] Cached file: " + path + " (" + std::to_string(entry->file_size) + " bytes)");
    }
    
    return entry;
}

std::string FileServer::generateETag(const std::string& path, const std::string& content) {
    // Simple ETag generation based on path and content hash
    std::hash<std::string> hasher;
    auto path_hash = hasher(path);
    auto content_hash = hasher(content);
    
    std::ostringstream oss;
    oss << "\"" << std::hex << path_hash << "-" << content_hash << "\"";
    return oss.str();
}

void FileServer::clearCache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    file_cache_.clear();
    ENDPOINT_LOG("file_server", "[FILESERVER-CACHE] Cache cleared");
}

void FileServer::refreshFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = file_cache_.find(path);
    if (it != file_cache_.end()) {
        file_cache_.erase(it);
        ENDPOINT_LOG("file_server", "[FILESERVER-CACHE] File removed from cache for refresh: " + path);
    }
}

std::string FileServer::createPageTransaction(const std::string& session_token,
                                            const std::string& user_id,
                                            const std::string& source_page,
                                            const std::string& target_page,
                                            bool requires_auth) {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    // Generate transaction ID
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    std::ostringstream oss;
    oss << "txn_" << timestamp << "_" << dis(gen);
    std::string transaction_id = oss.str();
    
    // Create transaction
    auto transaction = std::make_shared<PageTransaction>();
    transaction->session_token = session_token;
    transaction->user_id = user_id;
    transaction->source_page = source_page;
    transaction->target_page = target_page;
    transaction->requires_auth = requires_auth;
    transaction->timestamp = now;
    
    active_transactions_[transaction_id] = transaction;
    
    ENDPOINT_LOG("file_server", "[FILESERVER-TXN] Created page transaction: " + transaction_id + " (" + source_page + " -> " + target_page + ")");
    
    return transaction_id;
}

bool FileServer::validatePageTransaction(const std::string& transaction_id) {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto it = active_transactions_.find(transaction_id);
    if (it == active_transactions_.end()) {
        ENDPOINT_LOG("file_server", "[FILESERVER-TXN] Transaction not found: " + transaction_id);
        return false;
    }
    
    auto& transaction = it->second;
    auto now = std::chrono::steady_clock::now();
    auto age = std::chrono::duration_cast<std::chrono::seconds>(now - transaction->timestamp).count();
    
    // Transactions expire after 30 seconds
    if (age > 30) {
        ENDPOINT_LOG("file_server", "[FILESERVER-TXN] Transaction expired: " + transaction_id + " (age: " + std::to_string(age) + "s)");
        active_transactions_.erase(it);
        return false;
    }
    
    ENDPOINT_LOG("file_server", "[FILESERVER-TXN] Transaction valid: " + transaction_id);
    return true;
}

void FileServer::clearExpiredTransactions() {
    std::lock_guard<std::mutex> lock(transaction_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto it = active_transactions_.begin();
    int cleared = 0;
    
    while (it != active_transactions_.end()) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->timestamp).count();
        if (age > 30) { // 30 second expiry
            it = active_transactions_.erase(it);
            cleared++;
        } else {
            ++it;
        }
    }
    
    if (cleared > 0) {
        ENDPOINT_LOG("file_server", "[FILESERVER-TXN] Cleared " + std::to_string(cleared) + " expired transactions");
    }
}

enum MHD_Result FileServer::servePageWithTransaction(struct MHD_Connection* connection,
                                                   const std::string& requested_path,
                                                   const std::string& session_token,
                                                   const std::string& user_id) {
    try {
        ENDPOINT_LOG("file_server", "[FILESERVER-TXN] Serving page with transaction: " + requested_path);
        
        // Clear expired transactions first
        clearExpiredTransactions();
        
        // Create transaction for this page request
        std::string transaction_id = createPageTransaction(session_token, user_id, 
                                                         "previous", requested_path, !session_token.empty());
        
        // Serve the file normally but with transaction context
        return serveFileWithState(connection, requested_path, nullptr);
        
    } catch (const std::exception& e) {
        std::cerr << "[FILESERVER-TXN] Error serving page with transaction: " << e.what() << std::endl;
        return sendInternalError(connection);
    }
}

std::string FileServer::processPageForTransaction(const std::string& content, 
                                                const std::string& page_path,
                                                const std::shared_ptr<PageTransaction>& transaction) {
    // For now, return content as-is
    // In the future, could inject transaction ID, session data, etc.
    ENDPOINT_LOG("file_server", "[FILESERVER-TXN] Processing page for transaction: " + page_path);
    return content;
}

std::string FileServer::sanitizePath(const std::string& path) {
    std::string sanitized = path;
    
    // Remove leading slashes
    while (!sanitized.empty() && sanitized[0] == '/') {
        sanitized = sanitized.substr(1);
    }
    
    // Prevent directory traversal attacks
    size_t pos = 0;
    while ((pos = sanitized.find("../", pos)) != std::string::npos) {
        sanitized.erase(pos, 3);
    }
    
    // Remove trailing ../
    if (sanitized.length() >= 3 && sanitized.substr(sanitized.length() - 3) == "../") {
        sanitized = sanitized.substr(0, sanitized.length() - 3);
    }
    
    return sanitized;
}

enum MHD_Result FileServer::sendFileResponse(struct MHD_Connection* connection, 
                                const std::string& file_path,
                                const std::string& content) {
    
    std::string mime_type = getMimeType(file_path);
    ENDPOINT_LOG("file_server", "FileServer: Sending file response for " + file_path + " (" + std::to_string(content.length()) + " bytes, " + mime_type + ")");
    
    // Critical: Use safe memory management to prevent core dumps
    struct MHD_Response* response = nullptr;
    
    // Validate content size to prevent memory issues
    if (content.length() > 100 * 1024 * 1024) { // 100MB limit
        std::cerr << "FileServer: File too large for response: " << content.length() << " bytes" << std::endl;
        return sendInternalError(connection);
    }
    
    if (content.empty()) {
        std::cerr << "FileServer: Attempting to serve empty file" << std::endl;
        return sendNotFound(connection);
    }
    
    // Use MHD_RESPMEM_MUST_COPY for safety - avoids memory ownership issues
    try {
        response = MHD_create_response_from_buffer(
            content.length(),
            const_cast<char*>(content.c_str()),
            MHD_RESPMEM_MUST_COPY  // Safe: MHD copies the data
        );
    } catch (const std::exception& e) {
        std::cerr << "FileServer: Exception creating response: " << e.what() << std::endl;
        return sendInternalError(connection);
    }
    
    if (!response) {
        std::cerr << "FileServer: Critical failure creating MHD response" << std::endl;
        return sendInternalError(connection);
    }
    
    // Set production headers
    MHD_add_response_header(response, "Content-Type", mime_type.c_str());
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Cache-Control", "public, max-age=300");
    
    // Critical: Safe response queueing to prevent core dumps
    enum MHD_Result ret = MHD_NO;
    
    try {
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        if (ret != MHD_YES) {
            std::cerr << "FileServer: Failed to queue response, status: " << ret << std::endl;
        } else {
            ENDPOINT_LOG("file_server", "FileServer: Successfully queued response (" + std::to_string(content.length()) + " bytes)");
        }
    } catch (const std::exception& e) {
        std::cerr << "FileServer: Exception during response queueing: " << e.what() << std::endl;
        ret = MHD_NO;
    } catch (...) {
        std::cerr << "FileServer: Unknown exception during response queueing" << std::endl;
        ret = MHD_NO;
    }
    
    // Always destroy response to prevent memory leaks
    try {
        MHD_destroy_response(response);
    } catch (const std::exception& e) {
        std::cerr << "FileServer: Exception destroying response: " << e.what() << std::endl;
    }
    
    ENDPOINT_LOG("file_server", "FileServer: File response result=" + std::to_string(ret));
    return ret;
}

enum MHD_Result FileServer::sendDirectoryListing(struct MHD_Connection* connection, 
                                    const std::string& directory_path) {
    // For security reasons, directory listing is disabled by default
    return sendForbidden(connection);
}

enum MHD_Result FileServer::sendNotFound(struct MHD_Connection* connection) {
    std::string content = R"(
<!DOCTYPE html>
<html>
<head>
    <title>404 Not Found</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        h1 { color: #333; }
        p { color: #666; }
    </style>
</head>
<body>
    <h1>404 - File Not Found</h1>
    <p>The requested resource could not be found on this server.</p>
</body>
</html>)";
    
    struct MHD_Response* response = MHD_create_response_from_buffer(
        content.length(),
        const_cast<char*>(content.c_str()),
        MHD_RESPMEM_MUST_COPY
    );
    
    if (!response) {
        return MHD_NO;
    }
    
    MHD_add_response_header(response, "Content-Type", "text/html");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    
    return ret;
}

enum MHD_Result FileServer::sendForbidden(struct MHD_Connection* connection) {
    std::string content = R"(
<!DOCTYPE html>
<html>
<head>
    <title>403 Forbidden</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        h1 { color: #333; }
        p { color: #666; }
    </style>
</head>
<body>
    <h1>403 - Forbidden</h1>
    <p>You don't have permission to access this resource.</p>
</body>
</html>)";
    
    struct MHD_Response* response = MHD_create_response_from_buffer(
        content.length(),
        const_cast<char*>(content.c_str()),
        MHD_RESPMEM_MUST_COPY
    );
    
    if (!response) {
        return MHD_NO;
    }
    
    MHD_add_response_header(response, "Content-Type", "text/html");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_FORBIDDEN, response);
    MHD_destroy_response(response);
    
    return ret;
}

enum MHD_Result FileServer::sendInternalError(struct MHD_Connection* connection) {
    std::string content = R"(
<!DOCTYPE html>
<html>
<head>
    <title>500 Internal Server Error</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
        h1 { color: #333; }
        p { color: #666; }
    </style>
</head>
<body>
    <h1>500 - Internal Server Error</h1>
    <p>The server encountered an error while processing your request.</p>
</body>
</html>)";
    
    struct MHD_Response* response = MHD_create_response_from_buffer(
        content.length(),
        const_cast<char*>(content.c_str()),
        MHD_RESPMEM_MUST_COPY
    );
    
    if (!response) {
        return MHD_NO;
    }
    
    MHD_add_response_header(response, "Content-Type", "text/html");
    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_INTERNAL_SERVER_ERROR, response);
    MHD_destroy_response(response);
    
    return ret;
}