#include "FirmwareRouter.h"
#include "config_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include "nlohmann/json.hpp"
#include "../firmware-update/tftp_client.h"
#include "../firmware-update/manual_upload_handler.h"
#include "../sysupgrade-mecanism/sysupgrade_handler.h"
#include "../multipart_parser.h"
#include "endpoint_logger.h"
#include <filesystem>

using json = nlohmann::json;

FirmwareRouter::FirmwareRouter() :
    tftp_client(std::make_unique<FirmwareUpdate::TftpClient>()),
    manual_upload_handler(std::make_unique<FirmwareUpdate::ManualUploadHandler>()),
    sysupgrade_manager(std::make_unique<SysupgradeHandler::SysupgradeManager>()) {

    // Set up sysupgrade callbacks for real-time progress updates
    sysupgrade_manager->setProgressCallback([this](const SysupgradeHandler::UpgradeProgress& progress) {
        // Progress callback will automatically update the JSON file
        ENDPOINT_LOG_INFO("firmware", "Sysupgrade progress updated: " + std::to_string(progress.progress_percentage) + "%");
    });

    sysupgrade_manager->setCompletionCallback([this](bool success, const std::string& message) {
        ENDPOINT_LOG_INFO("firmware", "Sysupgrade completed - Success: " + std::string(success ? "true" : "false") + ", Message: " + message);
    });

    std::cout << "[FIRMWARE-ROUTER] FirmwareRouter initialized with sysupgrade support" << std::endl;
}

FirmwareRouter::~FirmwareRouter() {
    if (tftp_client) {
        tftp_client->cancel_operation();
    }
    std::cout << "[FIRMWARE-ROUTER] FirmwareRouter destroyed" << std::endl;
}

void FirmwareRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler) {
    std::cout << "FirmwareRouter: Registering firmware routes..." << std::endl;

    // Firmware status endpoint
    addRouteHandler("/api/firmware/status", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleFirmwareStatus(req);
    });

    // Firmware upgrade readiness endpoint
    addRouteHandler("/api/firmware/readiness", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleUpgradeReadiness(req);
    });

    // Firmware upgrade action endpoint
    addRouteHandler("/api/firmware/upgrade", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleUpgradeAction(req);
    });

    // Firmware upgrade progress endpoint
    addRouteHandler("/api/firmware/progress", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleUpgradeProgress(req);
    });

    // TFTP test connection endpoint
    addRouteHandler("/api/firmware/tftp/test", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleTftpTest(req);
    });

    // TFTP download endpoint
    addRouteHandler("/api/firmware/tftp/download", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleTftpDownload(req);
    });

    // TFTP download progress endpoint
    addRouteHandler("/api/firmware/tftp/progress", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleTftpDownloadProgress(req);
    });

    // Manual upload endpoints
    addRouteHandler("/api/firmware/manual/upload", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleManualUpload(req);
    });

    addRouteHandler("/api/firmware/manual/list", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleManualUploadList(req);
    });

    addRouteHandler("/api/firmware/manual/info", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleManualUploadInfo(req);
    });

    // Firmware selection endpoint
    addRouteHandler("/api/firmware/available", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleAvailableFirmware(req);
    });

    // Firmware selection update endpoint
    addRouteHandler("/api/firmware/select", [this](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) {
        ApiRequest req;
        req.method = method;
        req.params = params;
        req.body = body;
        return this->handleFirmwareSelection(req);
    });

    std::cout << "FirmwareRouter: All firmware routes registered successfully" << std::endl;
}

std::string FirmwareRouter::handleFirmwareStatus(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing firmware status request, method: " << request.method << std::endl;

    try {
        if (request.method == "GET") {
            std::cout << "[FIRMWARE-STATUS] GET request received" << std::endl;

            // Load current firmware data
            std::string current_file = ConfigManager::getInstance().getDataPath("firmware/current-firmware.json");

            std::string firmwareData = loadJsonFile(current_file);
            if (firmwareData.empty()) {
                return R"({"success":false,"error":"Failed to load firmware data","timestamp":)" + generateTimestamp() + "}";
            }

            json response = json::parse(firmwareData);
            response["success"] = true;
            response["timestamp"] = std::stoull(generateTimestamp());

            std::cout << "[FIRMWARE-STATUS] Current firmware data loaded successfully" << std::endl;
            return response.dump();
        }

        return R"({"success":false,"error":"Method not allowed","timestamp":)" + generateTimestamp() + "}";

    } catch (const std::exception& e) {
        std::cout << "[FIRMWARE-STATUS] Error: " << e.what() << std::endl;
        return R"({"success":false,"error":"Failed to process firmware status","error_code":500,"timestamp":)" + generateTimestamp() + "}";
    }
}

std::string FirmwareRouter::handleUpgradeReadiness(const ApiRequest& request) {
    std::cout << "[FIRMWARE-READINESS] Processing upgrade readiness request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        // Load upgrade readiness data
        std::string readiness_file = ConfigManager::getInstance().getDataPath("firmware/upgrade-readiness.json");

        std::ifstream readiness_file_stream(readiness_file);
        if (readiness_file_stream.is_open()) {
            nlohmann::json readiness_data;
            readiness_file_stream >> readiness_data;
            readiness_file_stream.close();

            json_response.merge_patch(readiness_data);
            std::cout << "[FIRMWARE-ROUTER] Readiness data loaded successfully" << std::endl;
        } else {
            json_response["success"] = false;
            json_response["error"] = "Readiness data file not found";
            std::cout << "[FIRMWARE-ROUTER] Error: Readiness data file not found" << std::endl;
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error loading readiness data: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error loading readiness data: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] Readiness request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::handleUpgradeAction(const ApiRequest& request) {
    ENDPOINT_LOG_INFO("firmware", "Processing firmware upgrade action request");

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "POST") {
            nlohmann::json request_data = nlohmann::json::parse(request.body);
            std::string action = request_data.value("action", "");

            if (action == "start_upgrade") {
                // Get firmware path from request or from selected firmware
                std::string firmware_path = request_data.value("firmware_path", "");
                bool preserve_config = request_data.value("preserve_config", true);

                // If no path provided, try to determine from readiness data
                if (firmware_path.empty()) {
                    firmware_path = determineFirmwarePath();
                }

                if (firmware_path.empty()) {
                    json_response["success"] = false;
                    json_response["error"] = "No firmware file selected for upgrade";
                    return json_response.dump();
                }

                ENDPOINT_LOG_INFO("firmware", "Starting sysupgrade with firmware: " + firmware_path);

                // Start the actual sysupgrade process
                std::string error = sysupgrade_manager->startUpgrade(firmware_path, preserve_config);

                if (!error.empty()) {
                    json_response["success"] = false;
                    json_response["error"] = error;
                    ENDPOINT_LOG_ERROR("firmware", "Failed to start upgrade: " + error);
                } else {
                    auto progress = sysupgrade_manager->getProgress();
                    json_response["message"] = "Firmware upgrade initiated successfully";
                    json_response["upgrade_id"] = progress.upgrade_id;
                    json_response["status"] = "initiated";
                    ENDPOINT_LOG_INFO("firmware", "Upgrade initiated successfully: " + progress.upgrade_id);
                }
            } else if (action == "cancel_upgrade") {
                bool cancelled = sysupgrade_manager->cancelUpgrade();
                if (cancelled) {
                    json_response["message"] = "Upgrade cancellation requested";
                    json_response["status"] = "cancelling";
                } else {
                    json_response["success"] = false;
                    json_response["error"] = "No active upgrade to cancel";
                }
            } else {
                json_response["success"] = false;
                json_response["error"] = "Unknown action: " + action;
            }
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only POST method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error processing upgrade action: " + std::string(e.what());
        ENDPOINT_LOG_ERROR("firmware", "Error processing upgrade action: " + std::string(e.what()));
    }

    ENDPOINT_LOG_INFO("firmware", "Upgrade action request completed");
    return json_response.dump();
}

std::string FirmwareRouter::handleUpgradeProgress(const ApiRequest& request) {
    ENDPOINT_LOG_INFO("firmware", "Processing firmware progress request");

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        // Get current progress from sysupgrade manager
        auto progress = sysupgrade_manager->getProgress();

        // Convert progress to JSON format expected by frontend
        std::string progress_json = SysupgradeHandler::progressToJson(progress);
        nlohmann::json progress_data = nlohmann::json::parse(progress_json);

        // Merge with response
        json_response.merge_patch(progress_data);

        ENDPOINT_LOG_INFO("firmware", "Progress data retrieved from sysupgrade manager");

        // Also try to load from file as fallback (in case manager was restarted)
        if (progress.status == SysupgradeHandler::UpgradeStatus::IDLE) {
            // Load upgrade progress data
            std::string progress_file = ConfigManager::getInstance().getDataPath("firmware/upgrade-progress.json");

            std::ifstream progress_file_stream(progress_file);
            if (progress_file_stream.is_open()) {
                nlohmann::json file_data;
                progress_file_stream >> file_data;
                progress_file_stream.close();

                // Only use file data if it's more recent than manager data
                if (!file_data.empty()) {
                    json_response.merge_patch(file_data);
                    ENDPOINT_LOG_INFO("firmware", "Progress data supplemented from file");
                }
            }
        }

    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error loading progress data: " + std::string(e.what());
        ENDPOINT_LOG_ERROR("firmware", "Error loading progress data: " + std::string(e.what()));
    }

    ENDPOINT_LOG_INFO("firmware", "Progress request completed");
    return json_response.dump();
}

std::string FirmwareRouter::loadJsonFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cout << "[FIRMWARE-ROUTER] Error: Could not open file " << filename << std::endl;
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        std::cout << "[FIRMWARE-ROUTER] Successfully loaded " << filename << std::endl;
        return buffer.str();

    } catch (const std::exception& e) {
        std::cout << "[FIRMWARE-ROUTER] Error loading file " << filename << ": " << e.what() << std::endl;
        return "";
    }
}

std::string FirmwareRouter::generateTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return std::to_string(timestamp);
}

std::string FirmwareRouter::handleTftpTest(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing TFTP test request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "POST") {
            nlohmann::json request_data = nlohmann::json::parse(request.body);
            std::string server_ip = request_data.value("server_ip", "");
            uint16_t port = static_cast<uint16_t>(request_data.value("port", 69));

            if (server_ip.empty()) {
                json_response["success"] = false;
                json_response["error"] = "Server IP address is required";
                return json_response.dump();
            }

            // Validate IP address
            if (!FirmwareUpdate::is_valid_ip_address(server_ip)) {
                json_response["success"] = false;
                json_response["error"] = "Invalid IP address format";
                return json_response.dump();
            }

            std::string result = performTftpConnectionTest(server_ip, port);

            if (result.empty()) {
                json_response["message"] = "TFTP connection test successful";
                json_response["status"] = "connected";
                json_response["server_responsive"] = true;
            } else {
                json_response["success"] = false;
                json_response["error"] = result;
                json_response["status"] = "connection_failed";
                json_response["server_responsive"] = false;
            }
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only POST method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error processing TFTP test: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error processing TFTP test: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] TFTP test request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::handleTftpDownload(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing TFTP download request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "POST") {
            nlohmann::json request_data = nlohmann::json::parse(request.body);
            std::string server_ip = request_data.value("server_ip", "");
            uint16_t port = static_cast<uint16_t>(request_data.value("port", 69));
            std::string filename = request_data.value("filename", "");

            if (server_ip.empty() || filename.empty()) {
                json_response["success"] = false;
                json_response["error"] = "Server IP and filename are required";
                return json_response.dump();
            }

            // Validate inputs
            if (!FirmwareUpdate::is_valid_ip_address(server_ip)) {
                json_response["success"] = false;
                json_response["error"] = "Invalid IP address format";
                return json_response.dump();
            }

            if (!FirmwareUpdate::is_valid_tftp_filename(filename)) {
                json_response["success"] = false;
                json_response["error"] = "Invalid filename";
                return json_response.dump();
            }

            // Start TFTP download in a separate thread
            std::thread download_thread([this, server_ip, port, filename]() {
                std::string result = performTftpDownload(server_ip, port, filename);
                if (!result.empty()) {
                    std::cout << "[FIRMWARE-ROUTER] TFTP download error: " << result << std::endl;
                }
            });
            download_thread.detach();

            json_response["message"] = "TFTP download initiated";
            json_response["status"] = "downloading";
            json_response["download_id"] = "tftp_" + generateTimestamp();
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only POST method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error processing TFTP download: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error processing TFTP download: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] TFTP download request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::handleTftpDownloadProgress(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing TFTP download progress request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "GET") {
            // Load progress data from file
            std::string download_progress_file = ConfigManager::getInstance().getDataPath("firmware/download-progress.json");

            std::ifstream progress_file(download_progress_file);
            if (progress_file.is_open()) {
                nlohmann::json progress_data;
                progress_file >> progress_data;
                progress_file.close();

                json_response.merge_patch(progress_data);
                std::cout << "[FIRMWARE-ROUTER] TFTP download progress loaded successfully" << std::endl;
            } else {
                json_response["success"] = false;
                json_response["error"] = "Download progress data file not found";
                std::cout << "[FIRMWARE-ROUTER] Error: Download progress data file not found" << std::endl;
            }
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only GET method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error loading download progress: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error loading download progress: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] TFTP download progress request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::performTftpConnectionTest(const std::string& server_ip, uint16_t port) {
    std::cout << "[FIRMWARE-ROUTER] Testing TFTP connection to " << server_ip << ":" << port << std::endl;

    try {
        tftp_client->set_server_address(server_ip);
        tftp_client->set_server_port(port);
        tftp_client->set_timeout(std::chrono::milliseconds(5000));
        tftp_client->set_retries(2);

        if (tftp_client->test_connection()) {
            std::cout << "[FIRMWARE-ROUTER] TFTP connection test successful" << std::endl;
            return ""; // Success
        } else {
            std::string error = tftp_client->get_last_error_message();
            std::cout << "[FIRMWARE-ROUTER] TFTP connection test failed: " << error << std::endl;
            return error;
        }
    } catch (const std::exception& e) {
        std::string error = "TFTP connection test exception: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] " << error << std::endl;
        return error;
    }
}

std::string FirmwareRouter::performTftpDownload(const std::string& server_ip, uint16_t port, const std::string& filename) {
    std::cout << "[FIRMWARE-ROUTER] Starting TFTP download: " << filename << " from " << server_ip << ":" << port << std::endl;

    std::string download_id = "tftp_" + generateTimestamp();
    auto start_time = std::chrono::steady_clock::now();

    // Initialize download progress
    initializeDownloadProgress(download_id, filename, server_ip, port);

    try {
        tftp_client->set_server_address(server_ip);
        tftp_client->set_server_port(port);
        tftp_client->set_timeout(std::chrono::milliseconds(15000)); // Increased timeout
        tftp_client->set_retries(5); // Increased retries

        // Set progress callback
        tftp_client->set_progress_callback([this, download_id, start_time](const FirmwareUpdate::TftpProgress& progress) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();

            double speed_kbps = 0.0;
            if (elapsed > 0) {
                speed_kbps = (progress.bytes_transferred / 1024.0) / elapsed;
            }

            int eta_seconds = 0;
            if (progress.total_bytes > 0 && speed_kbps > 0) {
                double remaining_kb = (progress.total_bytes - progress.bytes_transferred) / 1024.0;
                eta_seconds = static_cast<int>(remaining_kb / speed_kbps);
            }

            updateDownloadProgress(download_id, "downloading", progress.percentage,
                                 progress.bytes_transferred, progress.total_bytes,
                                 speed_kbps, elapsed, eta_seconds, "");
        });

        // Set completion callback
        tftp_client->set_completion_callback([this, download_id](FirmwareUpdate::TftpError error, const std::string& message) {
            if (error == FirmwareUpdate::TftpError::NONE) {
                updateDownloadProgress(download_id, "completed", 100, 0, 0, 0, 0, 0, "");
                std::cout << "[FIRMWARE-ROUTER] TFTP download completed: " << message << std::endl;
            } else {
                updateDownloadProgress(download_id, "failed", -1, 0, 0, 0, 0, 0, message);
                std::cout << "[FIRMWARE-ROUTER] TFTP download failed: " << message << std::endl;
            }
        });

        std::vector<uint8_t> firmware_data;
        if (tftp_client->download_file(filename, firmware_data)) {
            // Save the firmware file to downloads directory
            std::string downloads_dir = ConfigManager::getInstance().getDataPath("firmware/downloads");
            std::string firmware_path = downloads_dir + "/" + filename;

            // Ensure the directory exists
            if (!std::filesystem::exists(downloads_dir)) {
                std::filesystem::create_directories(downloads_dir);
            }

            std::ofstream firmware_file(firmware_path, std::ios::binary);
            if (firmware_file) {
                firmware_file.write(reinterpret_cast<const char*>(firmware_data.data()), firmware_data.size());
                firmware_file.close();
                std::cout << "[FIRMWARE-ROUTER] Firmware saved to " << firmware_path << std::endl;
            }

            return ""; // Success
        } else {
            std::string error = tftp_client->get_last_error_message();
            updateDownloadProgress(download_id, "failed", -1, 0, 0, 0, 0, 0, error);
            std::cout << "[FIRMWARE-ROUTER] TFTP download failed: " << error << std::endl;
            return error;
        }
    } catch (const std::exception& e) {
        std::string error = "TFTP download exception: " + std::string(e.what());
        updateDownloadProgress(download_id, "failed", -1, 0, 0, 0, 0, 0, error);
        std::cout << "[FIRMWARE-ROUTER] " << error << std::endl;
        return error;
    }
}

void FirmwareRouter::initializeDownloadProgress(const std::string& download_id, const std::string& filename,
                                                        const std::string& server_ip, uint16_t port) {
    try {
        nlohmann::json progress_data;
        progress_data["download_id"] = download_id;
        progress_data["status"] = "initializing";
        progress_data["filename"] = filename;
        progress_data["server_ip"] = server_ip;
        progress_data["server_port"] = port;
        progress_data["progress_percentage"] = 0;
        progress_data["bytes_downloaded"] = 0;
        progress_data["total_bytes"] = 0;
        progress_data["download_speed_kbps"] = 0;
        progress_data["elapsed_time_seconds"] = 0;
        progress_data["estimated_remaining_seconds"] = 0;
        progress_data["error_message"] = "";
        progress_data["started_at"] = generateTimestamp();
        progress_data["last_updated"] = generateTimestamp();
        progress_data["completed_at"] = "";

        std::string download_progress_file = ConfigManager::getInstance().getDataPath("firmware/download-progress.json");
        std::ofstream out_file(download_progress_file);
        if (out_file.is_open()) {
            out_file << progress_data.dump(2);
            out_file.close();
        }

        std::cout << "[FIRMWARE-ROUTER] Download progress initialized: " << download_id << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[FIRMWARE-ROUTER] Error initializing download progress: " << e.what() << std::endl;
    }
}

void FirmwareRouter::updateDownloadProgress(const std::string& download_id, const std::string& status,
                                          double percentage, size_t bytes_downloaded, size_t total_bytes,
                                          double speed_kbps, int elapsed_seconds, int eta_seconds,
                                          const std::string& error_message) {
    try {
        nlohmann::json progress_data;

        // Try to load existing progress
        std::string download_progress_file = ConfigManager::getInstance().getDataPath("firmware/download-progress.json");
        std::ifstream progress_file(download_progress_file);
        if (progress_file.is_open()) {
            progress_file >> progress_data;
            progress_file.close();
        }

        // Update progress
        progress_data["download_id"] = download_id;
        progress_data["status"] = status;
        progress_data["progress_percentage"] = static_cast<int>(percentage);
        progress_data["bytes_downloaded"] = bytes_downloaded;
        progress_data["total_bytes"] = total_bytes;
        progress_data["download_speed_kbps"] = speed_kbps;
        progress_data["elapsed_time_seconds"] = elapsed_seconds;
        progress_data["estimated_remaining_seconds"] = eta_seconds;
        progress_data["error_message"] = error_message;
        progress_data["last_updated"] = generateTimestamp();

        if (status == "completed" || status == "failed") {
            progress_data["completed_at"] = generateTimestamp();
        }

        // Save updated progress
        std::ofstream out_file(download_progress_file);
        if (out_file.is_open()) {
            out_file << progress_data.dump(2);
            out_file.close();
        }

        std::cout << "[FIRMWARE-ROUTER] Download Progress: " << status << " (" << percentage << "%) - "
                  << bytes_downloaded << "/" << total_bytes << " bytes" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "[FIRMWARE-ROUTER] Error updating download progress: " << e.what() << std::endl;
    }
}

std::string FirmwareRouter::handleManualUpload(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing manual upload request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "POST") {
            // Check if this is multipart/form-data by looking at the body content
            std::string request_body = request.body;

            // Check for multipart boundary in the body
            if (request_body.find("------WebKit") != std::string::npos ||
                request_body.find("Content-Disposition: form-data") != std::string::npos) {

                std::cout << "[FIRMWARE-ROUTER] Detected multipart form data upload" << std::endl;

                // Extract boundary from body (first line typically contains boundary)
                size_t boundary_start = request_body.find("------");
                if (boundary_start == std::string::npos) {
                    json_response["success"] = false;
                    json_response["error"] = "Invalid multipart data: no boundary found";
                    return json_response.dump();
                }

                size_t boundary_end = request_body.find('\n', boundary_start);
                if (boundary_end == std::string::npos) boundary_end = request_body.find('\r', boundary_start);
                std::string boundary = request_body.substr(boundary_start, boundary_end - boundary_start);

                std::cout << "[FIRMWARE-ROUTER] Using boundary: " << boundary << std::endl;

                // Parse multipart form data manually
                size_t file_data_start = request_body.find("\r\n\r\n");
                if (file_data_start == std::string::npos) {
                    file_data_start = request_body.find("\n\n");
                }

                if (file_data_start == std::string::npos) {
                    json_response["success"] = false;
                    json_response["error"] = "Invalid multipart data: no file content found";
                    return json_response.dump();
                }

                file_data_start += 4; // Skip past \r\n\r\n or \n\n

                // Find end of file data
                size_t file_data_end = request_body.find(boundary, file_data_start);
                if (file_data_end == std::string::npos) {
                    json_response["success"] = false;
                    json_response["error"] = "Invalid multipart data: no end boundary found";
                    return json_response.dump();
                }

                // Extract file data
                std::vector<uint8_t> file_data;
                std::string file_content = request_body.substr(file_data_start, file_data_end - file_data_start);

                // Remove trailing newlines/carriage returns
                while (!file_content.empty() && (file_content.back() == '\n' || file_content.back() == '\r')) {
                    file_content.pop_back();
                }

                // Convert to vector
                file_data.assign(file_content.begin(), file_content.end());

                std::cout << "[FIRMWARE-ROUTER] Extracted file data: " << file_data.size() << " bytes" << std::endl;

                if (file_data.empty()) {
                    json_response["success"] = false;
                    json_response["error"] = "No file data found in upload";
                    return json_response.dump();
                }

                // Extract filename from Content-Disposition header
                std::string filename = "firmware.bin"; // Default filename
                size_t filename_pos = request_body.find("filename=\"");
                if (filename_pos != std::string::npos) {
                    filename_pos += 10; // Skip 'filename="'
                    size_t filename_end = request_body.find("\"", filename_pos);
                    if (filename_end != std::string::npos) {
                        filename = request_body.substr(filename_pos, filename_end - filename_pos);
                    }
                }

                std::cout << "[FIRMWARE-ROUTER] Processing file upload: " << filename
                          << " (" << file_data.size() << " bytes)" << std::endl;

                // Upload the firmware file
                std::string upload_id = manual_upload_handler->uploadFirmwareFile(
                    file_data,
                    filename,
                    "application/octet-stream",
                    false); // Default to no signature verification

                if (!upload_id.empty()) {
                    json_response["message"] = "File uploaded successfully";
                    json_response["upload_id"] = upload_id;
                    json_response["status"] = "completed";
                    json_response["filename"] = filename;
                    json_response["file_size"] = file_data.size();
                    json_response["file_type"] = "application/octet-stream";
                    std::cout << "[FIRMWARE-ROUTER] Manual upload successful: " << upload_id << std::endl;
                } else {
                    json_response["success"] = false;
                    json_response["error"] = "Failed to upload file - validation or storage error";
                    std::cout << "[FIRMWARE-ROUTER] Manual upload failed" << std::endl;
                }
            } else {
                // Try to parse as JSON for testing purposes
                try {
                    nlohmann::json request_data = nlohmann::json::parse(request.body);
                    json_response["success"] = false;
                    json_response["error"] = "Multipart/form-data required for file uploads";
                } catch (...) {
                    json_response["success"] = false;
                    json_response["error"] = "Invalid request format. Expected multipart/form-data for file uploads.";
                }
            }
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only POST method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error processing manual upload: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error processing manual upload: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] Manual upload request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::handleManualUploadList(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing manual upload list request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "GET") {
            auto uploads = manual_upload_handler->listUploads();

            nlohmann::json uploads_json = nlohmann::json::array();

            for (const auto& upload : uploads) {
                nlohmann::json upload_json;
                upload_json["upload_id"] = upload->upload_id;
                upload_json["filename"] = upload->filename;
                upload_json["file_size"] = upload->file_size;
                upload_json["file_type"] = upload->file_type;
                upload_json["checksum_md5"] = upload->checksum_md5;
                upload_json["checksum_sha256"] = upload->checksum_sha256;
                upload_json["upload_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    upload->upload_timestamp.time_since_epoch()).count();
                upload_json["status"] = upload->status;
                upload_json["verify_signature"] = upload->verify_signature;

                uploads_json.push_back(upload_json);
            }

            json_response["uploads"] = uploads_json;
            json_response["count"] = uploads.size();
            std::cout << "[FIRMWARE-ROUTER] Listed " << uploads.size() << " uploads" << std::endl;
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only GET method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error listing uploads: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error listing uploads: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] Manual upload list request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::handleManualUploadInfo(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing manual upload info request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "GET") {
            auto it = request.params.find("upload_id");
            if (it == request.params.end()) {
                json_response["success"] = false;
                json_response["error"] = "Upload ID parameter required";
                return json_response.dump();
            }

            std::string upload_id = it->second;
            auto upload_info = manual_upload_handler->getUploadInfo(upload_id);

            if (upload_info) {
                json_response["upload_info"] = {
                    {"upload_id", upload_info->upload_id},
                    {"filename", upload_info->filename},
                    {"file_size", upload_info->file_size},
                    {"file_type", upload_info->file_type},
                    {"checksum_md5", upload_info->checksum_md5},
                    {"checksum_sha256", upload_info->checksum_sha256},
                    {"upload_timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        upload_info->upload_timestamp.time_since_epoch()).count()},
                    {"status", upload_info->status},
                    {"verify_signature", upload_info->verify_signature},
                    {"error_message", upload_info->error_message}
                };
                std::cout << "[FIRMWARE-ROUTER] Upload info retrieved for: " << upload_id << std::endl;
            } else {
                json_response["success"] = false;
                json_response["error"] = "Upload not found";
                std::cout << "[FIRMWARE-ROUTER] Upload not found: " << upload_id << std::endl;
            }
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only GET method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error retrieving upload info: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error retrieving upload info: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] Manual upload info request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::handleAvailableFirmware(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing available firmware request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "GET") {
            nlohmann::json firmware_sources;

            // Manual uploads
            auto manual_uploads = manual_upload_handler->listUploads();
            nlohmann::json manual_list = nlohmann::json::array();

            for (const auto& upload : manual_uploads) {
                nlohmann::json manual_item;
                manual_item["id"] = upload->upload_id;
                manual_item["name"] = upload->filename;
                manual_item["size"] = upload->file_size;
                manual_item["upload_date"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    upload->upload_timestamp.time_since_epoch()).count();
                manual_item["checksum"] = upload->checksum_sha256.substr(0, 16) + "...";
                manual_item["status"] = upload->status;
                manual_list.push_back(manual_item);
            }

            // TFTP downloads (check downloads directory)
            nlohmann::json tftp_list = nlohmann::json::array();
            std::string downloads_dir = ConfigManager::getInstance().getDataPath("firmware/downloads");
            try {
                if (std::filesystem::exists(downloads_dir)) {
                    for (const auto& entry : std::filesystem::directory_iterator(downloads_dir)) {
                        if (entry.is_regular_file()) {
                            nlohmann::json tftp_item;
                            tftp_item["id"] = "tftp_" + entry.path().stem().string();
                            tftp_item["name"] = entry.path().filename().string();
                            tftp_item["size"] = std::filesystem::file_size(entry.path());
                            auto file_time = std::filesystem::last_write_time(entry.path());
                            auto system_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                                file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                            tftp_item["download_date"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                                system_time.time_since_epoch()).count();
                            tftp_item["status"] = "completed";
                            tftp_list.push_back(tftp_item);
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "[FIRMWARE-ROUTER] Error reading TFTP downloads: " << e.what() << std::endl;
            }

            // Online updates (simulated)
            nlohmann::json online_list = nlohmann::json::array();
            nlohmann::json online_item;
            online_item["id"] = "online_v2_5_0";
            online_item["name"] = "Firmware v2.5.0";
            online_item["version"] = "v2.5.0";
            online_item["release_date"] = "Feb 10, 2025";
            online_item["status"] = "available";
            online_item["size"] = 2097152; // 2MB
            online_list.push_back(online_item);

            firmware_sources["manual"] = manual_list;
            firmware_sources["tftp"] = tftp_list;
            firmware_sources["online"] = online_list;

            json_response["firmware_sources"] = firmware_sources;
            std::cout << "[FIRMWARE-ROUTER] Available firmware loaded successfully" << std::endl;
        } else {
            json_response["success"] = false;
            json_response["error"] = "Only GET method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error loading available firmware: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error loading available firmware: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] Available firmware request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::handleFirmwareSelection(const ApiRequest& request) {
    std::cout << "[FIRMWARE-ROUTER] Processing firmware selection request, method: " << request.method << std::endl;

    nlohmann::json json_response;
    json_response["success"] = true;
    json_response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    try {
        if (request.method == "POST") {
            nlohmann::json request_data = nlohmann::json::parse(request.body);
            std::string source = request_data.value("source", "");
            std::string firmware_id = request_data.value("firmware_id", "");

            if (source.empty() || firmware_id.empty()) {
                json_response["success"] = false;
                json_response["error"] = "Source and firmware_id are required";
                return json_response.dump();
            }

            // Save selection to readiness file
            nlohmann::json selection_data;
            selection_data["selected_source"] = source;
            selection_data["selected_firmware_id"] = firmware_id;
            selection_data["selection_timestamp"] = generateTimestamp();

            // Load existing readiness data
            std::string readiness_file = ConfigManager::getInstance().getDataPath("firmware/upgrade-readiness.json");

            std::ifstream readiness_file_stream(readiness_file);
            nlohmann::json readiness_data;
            if (readiness_file_stream.is_open()) {
                readiness_file_stream >> readiness_data;
                readiness_file_stream.close();
            }

            // Update with selection
            readiness_data["firmware_selection"] = selection_data;
            readiness_data["readiness_status"] = "Ready";
            readiness_data["status_description"] = "Firmware selected and ready for upgrade";
            readiness_data["status_color"] = "green";

            // Save updated readiness data
            std::ofstream out_file(readiness_file);
            if (out_file.is_open()) {
                out_file << readiness_data.dump(2);
                out_file.close();

                json_response["message"] = "Firmware selection updated successfully";
                json_response["selected_source"] = source;
                json_response["selected_firmware_id"] = firmware_id;
            } else {
                json_response["success"] = false;
                json_response["error"] = "Failed to save firmware selection";
            }

        } else {
            json_response["success"] = false;
            json_response["error"] = "Only POST method supported";
        }
    } catch (const std::exception& e) {
        json_response["success"] = false;
        json_response["error"] = "Error processing firmware selection: " + std::string(e.what());
        std::cout << "[FIRMWARE-ROUTER] Error processing firmware selection: " << e.what() << std::endl;
    }

    std::cout << "[FIRMWARE-ROUTER] Firmware selection request completed" << std::endl;
    return json_response.dump();
}

std::string FirmwareRouter::determineFirmwarePath() {
    try {
        // Load readiness data to find selected firmware
        std::string readiness_file = ConfigManager::getInstance().getDataPath("firmware/upgrade-readiness.json");

        std::ifstream readiness_file_stream(readiness_file);
        if (!readiness_file_stream.is_open()) {
            ENDPOINT_LOG_ERROR("firmware", "Cannot open upgrade-readiness.json");
            return "";
        }

        nlohmann::json readiness_data;
        readiness_file_stream >> readiness_data;
        readiness_file_stream.close();

        if (!readiness_data.contains("firmware_selection")) {
            ENDPOINT_LOG_ERROR("firmware", "No firmware selection found in readiness data");
            return "";
        }

        auto selection = readiness_data["firmware_selection"];
        std::string source = selection.value("selected_source", "");
        std::string firmware_id = selection.value("selected_firmware_id", "");

        if (source.empty() || firmware_id.empty()) {
            ENDPOINT_LOG_ERROR("firmware", "Invalid firmware selection data");
            return "";
        }

        std::string firmware_path;

        if (source == "manual") {
            // Manual upload - find by upload ID
            auto uploads = manual_upload_handler->listUploads();
            for (const auto& upload : uploads) {
                if (upload->upload_id == firmware_id) {
                    // Construct the path for manually uploaded firmware
                    std::string manual_upload_dir = ConfigManager::getInstance().getDataPath("firmware/manual-upload");
                    firmware_path = manual_upload_dir + "/" + upload->upload_id + "/" + upload->filename;
                    break;
                }
            }
        } else if (source == "tftp") {
            // TFTP download - construct path
            std::string filename = firmware_id.substr(5); // Remove "tftp_" prefix
            std::string downloads_dir = ConfigManager::getInstance().getDataPath("firmware/downloads");
            firmware_path = downloads_dir + "/" + filename;
        } else if (source == "online") {
            // Online update - would need to download first
            ENDPOINT_LOG_ERROR("firmware", "Online firmware updates not yet implemented");
            return "";
        }

        // Verify file exists
        if (!std::filesystem::exists(firmware_path)) {
            ENDPOINT_LOG_ERROR("firmware", "Selected firmware file does not exist: " + firmware_path);
            return "";
        }

        ENDPOINT_LOG_INFO("firmware", "Determined firmware path: " + firmware_path);
        return firmware_path;

    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("firmware", "Error determining firmware path: " + std::string(e.what()));
        return "";
    }
}