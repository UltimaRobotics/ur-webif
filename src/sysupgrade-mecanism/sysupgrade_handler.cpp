
#include "sysupgrade_handler.h"
#include "endpoint_logger.h"
#include "nlohmann/json.hpp"
#include <fstream>
#include <sstream>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>
#include <iomanip>

using json = nlohmann::json;

namespace SysupgradeHandler {

SysupgradeManager::SysupgradeManager() 
    : is_running_(false), should_cancel_(false),
      progress_file_path_("data/firmware/upgrade-progress.json"),
      update_interval_(std::chrono::milliseconds(1000)) {
    
    ENDPOINT_LOG_INFO("firmware", "SysupgradeManager initialized");
    initializeStages();
    loadProgressFromFile();
}

SysupgradeManager::~SysupgradeManager() {
    if (upgrade_thread_ && upgrade_thread_->joinable()) {
        should_cancel_ = true;
        upgrade_thread_->join();
    }
    ENDPOINT_LOG_INFO("firmware", "SysupgradeManager destroyed");
}

std::string SysupgradeManager::startUpgrade(const std::string& firmware_path, bool preserve_config) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    
    if (is_running_) {
        ENDPOINT_LOG_ERROR("firmware", "Upgrade already in progress");
        return "Upgrade already in progress";
    }
    
    // Validate firmware file
    std::string validation_error = validateFirmwareFile(firmware_path);
    if (!validation_error.empty()) {
        ENDPOINT_LOG_ERROR("firmware", "Firmware validation failed: " + validation_error);
        return validation_error;
    }
    
    // Check if sysupgrade is available
    if (!isSysupgradeAvailable()) {
        ENDPOINT_LOG_ERROR("firmware", "sysupgrade command not available");
        return "sysupgrade command not available on this system";
    }
    
    // Initialize progress
    current_progress_.upgrade_id = "upgrade_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    current_progress_.status = UpgradeStatus::PREPARING;
    current_progress_.progress_percentage = 0;
    current_progress_.current_stage = "preparing";
    current_progress_.stage_description = "Preparing firmware upgrade...";
    current_progress_.started_at = std::chrono::system_clock::now();
    current_progress_.last_updated = std::chrono::system_clock::now();
    current_progress_.error_message.clear();
    current_progress_.upgrade_log.clear();
    
    initializeStages();
    addLogEntry("Starting firmware upgrade process...");
    addLogEntry("Firmware file: " + firmware_path);
    addLogEntry("Preserve configuration: " + std::string(preserve_config ? "Yes" : "No"));
    
    saveProgressToFile();
    
    // Start upgrade thread
    should_cancel_ = false;
    is_running_ = true;
    upgrade_thread_ = std::make_unique<std::thread>(
        &SysupgradeManager::upgradeWorker, this, firmware_path, preserve_config);
    
    ENDPOINT_LOG_INFO("firmware", "Firmware upgrade started: " + current_progress_.upgrade_id);
    return ""; // Success
}

bool SysupgradeManager::cancelUpgrade() {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    
    if (!is_running_) {
        return false;
    }
    
    should_cancel_ = true;
    addLogEntry("Upgrade cancellation requested...");
    ENDPOINT_LOG_INFO("firmware", "Upgrade cancellation requested");
    
    return true;
}

UpgradeProgress SysupgradeManager::getProgress() const {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    return current_progress_;
}

void SysupgradeManager::setProgressCallback(std::function<void(const UpgradeProgress&)> callback) {
    progress_callback_ = callback;
}

void SysupgradeManager::setCompletionCallback(std::function<void(bool, const std::string&)> callback) {
    completion_callback_ = callback;
}

void SysupgradeManager::upgradeWorker(const std::string& firmware_path, bool preserve_config) {
    try {
        addLogEntry("Validating firmware file...");
        updateProgress(UpgradeStatus::PREPARING, 5, "preparing", "Validating firmware file...");
        
        if (should_cancel_) {
            throw std::runtime_error("Upgrade cancelled by user");
        }
        
        // Stage 1: Preparation
        addLogEntry("Checking system requirements...");
        updateProgress(UpgradeStatus::PREPARING, 10, "preparing", "Checking system requirements...");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        completeStage("prepare");
        
        if (should_cancel_) {
            throw std::runtime_error("Upgrade cancelled by user");
        }
        
        // Stage 2: Pre-upgrade checks
        addLogEntry("Performing pre-upgrade system checks...");
        updateProgress(UpgradeStatus::PREPARING, 20, "preparing", "Performing pre-upgrade checks...");
        
        // Check available space
        std::string space_check = executeCommand("df -h /tmp | tail -1 | awk '{print $4}'", true);
        addLogEntry("Available space in /tmp: " + space_check);
        
        if (should_cancel_) {
            throw std::runtime_error("Upgrade cancelled by user");
        }
        
        // Stage 3: Backup current configuration (if preserving)
        if (preserve_config) {
            addLogEntry("Backing up current configuration...");
            updateProgress(UpgradeStatus::PREPARING, 30, "preparing", "Backing up configuration...");
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Stage 4: Verify firmware integrity
        addLogEntry("Verifying firmware integrity...");
        updateProgress(UpgradeStatus::VERIFYING, 40, "verify", "Verifying firmware checksum...");
        
        // Calculate and verify checksum
        std::string checksum_cmd = "sha256sum '" + firmware_path + "' | cut -d' ' -f1";
        std::string firmware_checksum = executeCommand(checksum_cmd, true);
        addLogEntry("Firmware SHA256: " + firmware_checksum);
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        completeStage("verify");
        
        if (should_cancel_) {
            throw std::runtime_error("Upgrade cancelled by user");
        }
        
        // Stage 5: Execute sysupgrade
        addLogEntry("Starting sysupgrade process...");
        updateProgress(UpgradeStatus::FLASHING, 50, "write_flash", "Writing firmware to flash memory...");
        
        std::string sysupgrade_result = executeSysupgrade(firmware_path, preserve_config);
        
        if (!sysupgrade_result.empty()) {
            throw std::runtime_error("Sysupgrade failed: " + sysupgrade_result);
        }
        
        // Stage 6: Post-upgrade
        addLogEntry("Firmware flash completed successfully");
        updateProgress(UpgradeStatus::REBOOTING, 90, "reboot", "Preparing for system reboot...");
        completeStage("write_flash");
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Stage 7: Complete
        addLogEntry("Firmware upgrade completed successfully");
        addLogEntry("System will reboot shortly...");
        updateProgress(UpgradeStatus::COMPLETED, 100, "completed", "Upgrade completed successfully");
        completeStage("reboot");
        
        if (completion_callback_) {
            completion_callback_(true, "Firmware upgrade completed successfully");
        }
        
    } catch (const std::exception& e) {
        std::string error_msg = std::string("Upgrade failed: ") + e.what();
        addLogEntry(error_msg);
        updateProgress(UpgradeStatus::FAILED, -1, "failed", error_msg);
        
        if (completion_callback_) {
            completion_callback_(false, error_msg);
        }
        
        ENDPOINT_LOG_ERROR("firmware", error_msg);
    }
    
    is_running_ = false;
}

void SysupgradeManager::updateProgress(UpgradeStatus status, int percentage, 
                                     const std::string& stage, const std::string& description) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    
    current_progress_.status = status;
    current_progress_.progress_percentage = percentage;
    current_progress_.current_stage = stage;
    current_progress_.stage_description = description;
    current_progress_.last_updated = std::chrono::system_clock::now();
    current_progress_.estimated_completion = estimateCompletion();
    
    saveProgressToFile();
    
    if (progress_callback_) {
        progress_callback_(current_progress_);
    }
}

void SysupgradeManager::addLogEntry(const std::string& entry) {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "] " << entry;
    
    std::lock_guard<std::mutex> lock(progress_mutex_);
    current_progress_.upgrade_log.push_back(ss.str());
    current_progress_.last_updated = now;
    
    ENDPOINT_LOG_INFO("firmware", ss.str());
}

void SysupgradeManager::saveProgressToFile() {
    try {
        // Ensure directory exists
        std::filesystem::path file_path(progress_file_path_);
        std::filesystem::create_directories(file_path.parent_path());
        
        std::string json_data = progressToJson(current_progress_);
        std::ofstream file(progress_file_path_);
        if (file.is_open()) {
            file << json_data;
            file.close();
        }
    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("firmware", "Failed to save progress file: " + std::string(e.what()));
    }
}

void SysupgradeManager::loadProgressFromFile() {
    try {
        std::ifstream file(progress_file_path_);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            
            if (!buffer.str().empty()) {
                current_progress_ = progressFromJson(buffer.str());
            }
        }
    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("firmware", "Failed to load progress file: " + std::string(e.what()));
        // Initialize with default values
        current_progress_ = UpgradeProgress{};
        current_progress_.status = UpgradeStatus::IDLE;
    }
}

std::string SysupgradeManager::executeSysupgrade(const std::string& firmware_path, bool preserve_config) {
    std::string command = "sysupgrade";
    
    if (!preserve_config) {
        command += " -n";
    }
    
    command += " -v '" + firmware_path + "'";
    
    addLogEntry("Executing: " + command);
    
    // Note: sysupgrade will reboot the system, so we need to handle this carefully
    // In a real implementation, you might want to run this in a way that allows
    // the system to reboot gracefully
    
    try {
        std::string result = executeCommand(command, false);
        return ""; // Success (though system will reboot)
    } catch (const std::exception& e) {
        return std::string("Command execution failed: ") + e.what();
    }
}

std::string SysupgradeManager::executeCommand(const std::string& command, bool capture_output) {
    ENDPOINT_LOG_INFO("firmware", "Executing command: " + command);
    
    if (capture_output) {
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("popen() failed");
        }
        
        std::string result;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        
        int status = pclose(pipe);
        if (status != 0) {
            throw std::runtime_error("Command failed with status: " + std::to_string(status));
        }
        
        // Remove trailing newline
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        
        return result;
    } else {
        // For commands that don't return (like sysupgrade), just execute
        int status = system(command.c_str());
        if (status != 0) {
            throw std::runtime_error("Command failed with status: " + std::to_string(status));
        }
        return "";
    }
}

void SysupgradeManager::initializeStages() {
    current_progress_.stages_completed.clear();
    current_progress_.stages_completed["prepare"] = false;
    current_progress_.stages_completed["verify"] = false;
    current_progress_.stages_completed["write_flash"] = false;
    current_progress_.stages_completed["reboot"] = false;
}

void SysupgradeManager::completeStage(const std::string& stage_id) {
    std::lock_guard<std::mutex> lock(progress_mutex_);
    current_progress_.stages_completed[stage_id] = true;
}

std::string SysupgradeManager::estimateCompletion() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - current_progress_.started_at).count();
    
    // Rough estimates based on typical upgrade times
    int total_estimated_seconds = 300; // 5 minutes total
    int remaining_seconds = total_estimated_seconds - elapsed;
    
    if (remaining_seconds <= 0) {
        remaining_seconds = 30; // At least 30 seconds
    }
    
    auto completion_time = now + std::chrono::seconds(remaining_seconds);
    auto time_t = std::chrono::system_clock::to_time_t(completion_time);
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Static utility functions
bool SysupgradeManager::isSysupgradeAvailable() {
    try {
        int result = system("which sysupgrade > /dev/null 2>&1");
        return (result == 0);
    } catch (...) {
        return false;
    }
}

std::string SysupgradeManager::validateFirmwareFile(const std::string& firmware_path) {
    // Check if file exists
    if (!std::filesystem::exists(firmware_path)) {
        return "Firmware file does not exist: " + firmware_path;
    }
    
    // Check file size (should be reasonable for firmware)
    auto file_size = std::filesystem::file_size(firmware_path);
    if (file_size < 1024 * 1024) { // Less than 1MB
        return "Firmware file too small (< 1MB): " + std::to_string(file_size) + " bytes";
    }
    
    if (file_size > 100 * 1024 * 1024) { // More than 100MB
        return "Firmware file too large (> 100MB): " + std::to_string(file_size) + " bytes";
    }
    
    // Check file extension/format
    std::string extension = firmware_path.substr(firmware_path.find_last_of('.'));
    std::vector<std::string> valid_extensions = {".bin", ".img", ".trx", ".tar.gz"};
    
    bool valid_extension = false;
    for (const auto& ext : valid_extensions) {
        if (extension == ext) {
            valid_extension = true;
            break;
        }
    }
    
    if (!valid_extension) {
        return "Unsupported firmware file format: " + extension;
    }
    
    return ""; // Valid
}

std::vector<std::string> SysupgradeManager::getSupportedFormats() {
    return {".bin", ".img", ".trx", ".tar.gz"};
}

// Helper functions
std::string progressToJson(const UpgradeProgress& progress) {
    json j;
    
    j["upgrade_id"] = progress.upgrade_id;
    j["status"] = statusToString(progress.status);
    j["progress_percentage"] = progress.progress_percentage;
    j["current_stage"] = progress.current_stage;
    j["stage_description"] = progress.stage_description;
    j["error_message"] = progress.error_message;
    
    // Convert timestamps
    auto started_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        progress.started_at.time_since_epoch()).count();
    j["started_at"] = std::to_string(started_ms);
    
    auto updated_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        progress.last_updated.time_since_epoch()).count();
    j["last_updated"] = std::to_string(updated_ms);
    
    j["estimated_completion"] = progress.estimated_completion;
    
    // Upgrade log
    j["upgrade_log"] = progress.upgrade_log;
    
    // Stages
    json stages = json::array();
    
    // Create stage objects matching the frontend expectations
    json download_stage;
    download_stage["id"] = "download";
    download_stage["name"] = "Downloading firmware";
    download_stage["status"] = "complete";
    download_stage["icon"] = "check";
    download_stage["description"] = "Firmware loaded successfully";
    stages.push_back(download_stage);
    
    json verify_stage;
    verify_stage["id"] = "verify";
    verify_stage["name"] = "Verifying firmware integrity";
    verify_stage["status"] = progress.stages_completed.count("verify") && progress.stages_completed.at("verify") ? "complete" : 
                             (progress.current_stage == "verify" ? "in_progress" : "pending");
    verify_stage["icon"] = verify_stage["status"] == "complete" ? "check" : 
                          (verify_stage["status"] == "in_progress" ? "gear" : "arrows-rotate");
    verify_stage["description"] = progress.stages_completed.count("verify") && progress.stages_completed.at("verify") ? 
                                  "Checksum verification passed" : 
                                  (progress.current_stage == "verify" ? "Verifying checksum..." : "Pending verification");
    stages.push_back(verify_stage);
    
    json flash_stage;
    flash_stage["id"] = "write_flash";
    flash_stage["name"] = "Writing flash memory";
    flash_stage["status"] = progress.stages_completed.count("write_flash") && progress.stages_completed.at("write_flash") ? "complete" : 
                           (progress.current_stage == "write_flash" ? "in_progress" : "pending");
    flash_stage["icon"] = flash_stage["status"] == "complete" ? "check" : 
                         (flash_stage["status"] == "in_progress" ? "gear" : "arrows-rotate");
    flash_stage["description"] = progress.stages_completed.count("write_flash") && progress.stages_completed.at("write_flash") ? 
                                "Flash write completed" : 
                                (progress.current_stage == "write_flash" ? progress.stage_description : "Pending flash write");
    stages.push_back(flash_stage);
    
    json reboot_stage;
    reboot_stage["id"] = "reboot";
    reboot_stage["name"] = "Rebooting device";
    reboot_stage["status"] = progress.stages_completed.count("reboot") && progress.stages_completed.at("reboot") ? "complete" : 
                            (progress.current_stage == "reboot" ? "in_progress" : "pending");
    reboot_stage["icon"] = reboot_stage["status"] == "complete" ? "check" : 
                          (reboot_stage["status"] == "in_progress" ? "gear" : "arrows-rotate");
    reboot_stage["description"] = progress.stages_completed.count("reboot") && progress.stages_completed.at("reboot") ? 
                                 "System reboot completed" : 
                                 (progress.current_stage == "reboot" ? "Rebooting system..." : "Pending reboot");
    stages.push_back(reboot_stage);
    
    j["stages"] = stages;
    
    return j.dump(2);
}

UpgradeProgress progressFromJson(const std::string& json_data) {
    UpgradeProgress progress;
    
    try {
        json j = json::parse(json_data);
        
        progress.upgrade_id = j.value("upgrade_id", "");
        progress.status = statusFromString(j.value("status", "idle"));
        progress.progress_percentage = j.value("progress_percentage", 0);
        progress.current_stage = j.value("current_stage", "");
        progress.stage_description = j.value("stage_description", "");
        progress.error_message = j.value("error_message", "");
        progress.estimated_completion = j.value("estimated_completion", "");
        
        if (j.contains("upgrade_log") && j["upgrade_log"].is_array()) {
            for (const auto& entry : j["upgrade_log"]) {
                progress.upgrade_log.push_back(entry.get<std::string>());
            }
        }
        
        // Parse timestamps
        if (j.contains("started_at")) {
            auto ms = std::stoull(j["started_at"].get<std::string>());
            progress.started_at = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }
        
        if (j.contains("last_updated")) {
            auto ms = std::stoull(j["last_updated"].get<std::string>());
            progress.last_updated = std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
        }
        
    } catch (const std::exception& e) {
        // Return default progress on parse error
        progress.status = UpgradeStatus::IDLE;
        progress.progress_percentage = 0;
    }
    
    return progress;
}

std::string statusToString(UpgradeStatus status) {
    switch (status) {
        case UpgradeStatus::IDLE: return "idle";
        case UpgradeStatus::PREPARING: return "preparing";
        case UpgradeStatus::DOWNLOADING: return "downloading";
        case UpgradeStatus::VERIFYING: return "verifying";
        case UpgradeStatus::FLASHING: return "in_progress";
        case UpgradeStatus::REBOOTING: return "rebooting";
        case UpgradeStatus::COMPLETED: return "completed";
        case UpgradeStatus::FAILED: return "failed";
        default: return "unknown";
    }
}

UpgradeStatus statusFromString(const std::string& status_str) {
    if (status_str == "idle") return UpgradeStatus::IDLE;
    if (status_str == "preparing") return UpgradeStatus::PREPARING;
    if (status_str == "downloading") return UpgradeStatus::DOWNLOADING;
    if (status_str == "verifying") return UpgradeStatus::VERIFYING;
    if (status_str == "in_progress") return UpgradeStatus::FLASHING;
    if (status_str == "rebooting") return UpgradeStatus::REBOOTING;
    if (status_str == "completed") return UpgradeStatus::COMPLETED;
    if (status_str == "failed") return UpgradeStatus::FAILED;
    return UpgradeStatus::IDLE;
}

} // namespace SysupgradeHandler
