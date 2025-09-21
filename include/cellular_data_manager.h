
#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Cellular Data Manager
 * Utility class for managing cellular data from separate JSON files
 * Handles status, scan, configuration, and SIM/device data
 */
class CellularDataManager {
public:
    // Initialize file paths with configurable data directory
    static void initializePaths();

    // Status data operations (Network Info, Signal Quality, Connection Details)
    static json loadStatusData();
    static bool saveStatusData(const json& data);
    static json getNetworkInformation();
    static json getSignalQuality();
    static json getConnectionDetails();
    static bool updateNetworkInformation(const json& networkInfo);
    static bool updateSignalQuality(const json& signalInfo);
    static bool updateConnectionDetails(const json& connectionInfo);

    // Scan data operations (Scan Status)
    static json loadScanData();
    static bool saveScanData(const json& data);
    static json getScanStatus();
    static json getAvailableNetworks();
    static bool updateScanStatus(const json& scanInfo);
    static bool updateAvailableNetworks(const json& networks);

    // Configuration data operations
    static json loadConfigData();
    static bool saveConfigData(const json& data);
    static json getCellularConfiguration();
    static bool updateCellularConfiguration(const json& config);

    // SIM and Device data operations
    static json loadSimData();
    static bool saveSimData(const json& data);
    static json getSimInformation();
    static json getDeviceInformation();
    static bool updateSimInformation(const json& simInfo);
    static bool updateDeviceInformation(const json& deviceInfo);

    // Combined operations for compatibility
    static json getAllCellularData();
    static json getCurrentStatus();

private:
    // File path constants - initialized with configurable data directory
    static std::string STATUS_DATA_FILE;
    static std::string SCAN_DATA_FILE;
    static std::string CONFIG_DATA_FILE;
    static std::string SIM_DATA_FILE;

    static json loadJsonFile(const std::string& filePath);
    static bool saveJsonFile(const std::string& filePath, const json& data);
    static json createDefaultStatusData();
    static json createDefaultScanData();
    static json createDefaultConfigData();
    static json createDefaultSimData();
    static bool validateDataStructure(const json& data, const std::string& type);
    static void ensureDirectoryExists(const std::string& filePath);
};
