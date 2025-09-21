
#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <chrono>
#include <memory>
#include <vector>
#include <map>
#include <mutex>

namespace SysupgradeHandler {
    
    enum class UpgradeStatus {
        IDLE,
        PREPARING,
        DOWNLOADING,
        VERIFYING,
        FLASHING,
        REBOOTING,
        COMPLETED,
        FAILED
    };

    struct UpgradeProgress {
        std::string upgrade_id;
        UpgradeStatus status;
        int progress_percentage;
        std::string current_stage;
        std::string stage_description;
        std::vector<std::string> upgrade_log;
        std::chrono::system_clock::time_point started_at;
        std::chrono::system_clock::time_point last_updated;
        std::string error_message;
        
        // Stage information
        std::map<std::string, bool> stages_completed;
        std::string estimated_completion;
    };

    class SysupgradeManager {
    public:
        SysupgradeManager();
        ~SysupgradeManager();
        
        // Main upgrade operations
        std::string startUpgrade(const std::string& firmware_path, bool preserve_config = true);
        bool cancelUpgrade();
        UpgradeProgress getProgress() const;
        
        // Progress callbacks
        void setProgressCallback(std::function<void(const UpgradeProgress&)> callback);
        void setCompletionCallback(std::function<void(bool success, const std::string& message)> callback);
        
        // Static utility functions
        static bool isSysupgradeAvailable();
        static std::string validateFirmwareFile(const std::string& firmware_path);
        static std::vector<std::string> getSupportedFormats();
        
    private:
        // Internal methods
        void upgradeWorker(const std::string& firmware_path, bool preserve_config);
        void updateProgress(UpgradeStatus status, int percentage, const std::string& stage, 
                          const std::string& description);
        void addLogEntry(const std::string& entry);
        void saveProgressToFile();
        void loadProgressFromFile();
        
        // Command execution
        std::string executeSysupgrade(const std::string& firmware_path, bool preserve_config);
        std::string executeCommand(const std::string& command, bool capture_output = true);
        void parseCommandOutput(const std::string& output);
        
        // Progress tracking
        void initializeStages();
        void completeStage(const std::string& stage_id);
        std::string estimateCompletion();
        
        // Member variables
        std::unique_ptr<std::thread> upgrade_thread_;
        std::atomic<bool> is_running_;
        std::atomic<bool> should_cancel_;
        UpgradeProgress current_progress_;
        mutable std::mutex progress_mutex_;
        
        // Callbacks
        std::function<void(const UpgradeProgress&)> progress_callback_;
        std::function<void(bool, const std::string&)> completion_callback_;
        
        // Configuration
        std::string progress_file_path_;
        std::chrono::milliseconds update_interval_;
    };
    
    // Helper functions for progress file management
    std::string progressToJson(const UpgradeProgress& progress);
    UpgradeProgress progressFromJson(const std::string& json_data);
    std::string statusToString(UpgradeStatus status);
    UpgradeStatus statusFromString(const std::string& status_str);
}
