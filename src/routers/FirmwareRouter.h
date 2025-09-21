
#pragma once

#include <string>
#include <functional>
#include <map>
#include <memory>
#include "api_request.h"

// Forward declarations
namespace FirmwareUpdate {
    class TftpClient;
    class ManualUploadHandler;
}

namespace SysupgradeHandler {
    class SysupgradeManager;
}

class FirmwareRouter {
public:
    using RouteHandler = std::function<std::string(const std::string&, const std::map<std::string, std::string>&, const std::string&)>;
    
    FirmwareRouter();
    ~FirmwareRouter();
    
    // Register all firmware routes
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler);
    
    // Firmware status endpoint
    std::string handleFirmwareStatus(const ApiRequest& request);
    
    // Firmware upgrade progress endpoint
    std::string handleUpgradeProgress(const ApiRequest& request);
    
    // Firmware upgrade readiness endpoint
    std::string handleUpgradeReadiness(const ApiRequest& request);
    
    // Firmware upgrade action endpoint
    std::string handleUpgradeAction(const ApiRequest& request);
    
    // TFTP operations
    std::string handleTftpTest(const ApiRequest& request);
    std::string handleTftpDownload(const ApiRequest& request);
    std::string handleTftpDownloadProgress(const ApiRequest& request);
    
    // Manual upload operations
    std::string handleManualUpload(const ApiRequest& request);
    std::string handleManualUploadList(const ApiRequest& request);
    std::string handleManualUploadInfo(const ApiRequest& request);
    
    // Firmware selection operations
    std::string handleAvailableFirmware(const ApiRequest& request);
    std::string handleFirmwareSelection(const ApiRequest& request);
    
    // Helper methods for sysupgrade integration
    std::string determineFirmwarePath();
    
private:
    std::string loadJsonFile(const std::string& filename);
    std::string generateTimestamp();
    
    // TFTP helper methods
    std::string performTftpConnectionTest(const std::string& server_ip, uint16_t port);
    std::string performTftpDownload(const std::string& server_ip, uint16_t port, const std::string& filename);
    void initializeDownloadProgress(const std::string& download_id, const std::string& filename, 
                                   const std::string& server_ip, uint16_t port);
    void updateDownloadProgress(const std::string& download_id, const std::string& status, 
                               double percentage, size_t bytes_downloaded, size_t total_bytes,
                               double speed_kbps, int elapsed_seconds, int eta_seconds, 
                               const std::string& error_message);
    
    // TFTP client instance
    std::unique_ptr<FirmwareUpdate::TftpClient> tftp_client;
    
    // Manual upload handler
    std::unique_ptr<FirmwareUpdate::ManualUploadHandler> manual_upload_handler;
    
    // Sysupgrade manager
    std::unique_ptr<SysupgradeHandler::SysupgradeManager> sysupgrade_manager;
};
