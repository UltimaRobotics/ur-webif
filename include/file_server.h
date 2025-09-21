#ifndef FILE_SERVER_H
#define FILE_SERVER_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <memory>
#include <microhttpd.h>

// Page transaction state for hot swap and session handling
struct PageTransaction {
    std::string session_token;
    std::string user_id;
    std::string source_page;
    std::string target_page;
    std::chrono::steady_clock::time_point timestamp;
    bool requires_auth;
    
    PageTransaction() : requires_auth(false) {
        timestamp = std::chrono::steady_clock::now();
    }
};

// File cache entry for hot swap capability
struct CacheEntry {
    std::string content;
    std::string etag;
    std::chrono::steady_clock::time_point last_modified;
    std::chrono::steady_clock::time_point cache_time;
    size_t file_size;
    
    CacheEntry() {
        auto now = std::chrono::steady_clock::now();
        last_modified = now;
        cache_time = now;
        file_size = 0;
    }
};

class FileServer {
public:
    FileServer(const std::string& document_root, const std::string& default_file = "index.html");
    ~FileServer();

    enum MHD_Result serveFile(struct MHD_Connection* connection, const std::string& requested_path);
    enum MHD_Result serveFileWithState(struct MHD_Connection* connection, 
                                      const std::string& requested_path,
                                      void** con_cls);
    
    // Enhanced transaction-aware page serving
    enum MHD_Result servePageWithTransaction(struct MHD_Connection* connection,
                                           const std::string& requested_path,
                                           const std::string& session_token = "",
                                           const std::string& user_id = "");
    
    void setDocumentRoot(const std::string& document_root);
    void setDefaultFile(const std::string& default_file);
    
    // Hot swap and cache management
    void enableCaching(bool enable) { cache_enabled_ = enable; }
    void setCacheMaxAge(int seconds) { cache_max_age_ = seconds; }
    void clearCache();
    void refreshFile(const std::string& path);
    
    // Transaction management
    std::string createPageTransaction(const std::string& session_token,
                                    const std::string& user_id,
                                    const std::string& source_page,
                                    const std::string& target_page,
                                    bool requires_auth = true);
    bool validatePageTransaction(const std::string& transaction_id);
    void clearExpiredTransactions();
    
    std::string getDocumentRoot() const { return document_root_; }
    std::string getDefaultFile() const { return default_file_; }

private:
    std::string document_root_;
    std::string default_file_;
    
    // Cache management
    bool cache_enabled_;
    int cache_max_age_;
    std::unordered_map<std::string, std::shared_ptr<CacheEntry>> file_cache_;
    mutable std::mutex cache_mutex_;
    
    // Transaction management
    std::unordered_map<std::string, std::shared_ptr<PageTransaction>> active_transactions_;
    mutable std::mutex transaction_mutex_;
    
    // File operations
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    std::string readFile(const std::string& path);
    std::shared_ptr<CacheEntry> readFileWithCache(const std::string& path);
    std::string getMimeType(const std::string& file_path);
    std::string sanitizePath(const std::string& path);
    std::string generateETag(const std::string& path, const std::string& content);
    
    // Enhanced page processing for transactions
    std::string processPageForTransaction(const std::string& content, 
                                        const std::string& page_path,
                                        const std::shared_ptr<PageTransaction>& transaction);
    
    // Response helpers
    enum MHD_Result sendFileResponse(struct MHD_Connection* connection, 
                        const std::string& file_path,
                        const std::string& content);
    
    enum MHD_Result sendDirectoryListing(struct MHD_Connection* connection, 
                           const std::string& directory_path);
    
    enum MHD_Result sendNotFound(struct MHD_Connection* connection);
    enum MHD_Result sendForbidden(struct MHD_Connection* connection);
    enum MHD_Result sendInternalError(struct MHD_Connection* connection);
};

#endif // FILE_SERVER_H