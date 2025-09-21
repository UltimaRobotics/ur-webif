
#include "cellular_data_manager.h"
#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <filesystem>

// File path constants - will be updated with configurable data directory
std::string CellularDataManager::STATUS_DATA_FILE = "";
std::string CellularDataManager::SCAN_DATA_FILE = "";
std::string CellularDataManager::CONFIG_DATA_FILE = "";
std::string CellularDataManager::SIM_DATA_FILE = "";

// Initialize file paths with configurable data directory
void CellularDataManager::initializePaths() {
    const std::string dataDir = ConfigManager::getInstance().getDataDirectory();
    STATUS_DATA_FILE = dataDir + "/cellular/status-data.json";
    SCAN_DATA_FILE = dataDir + "/cellular/scan-data.json";
    CONFIG_DATA_FILE = dataDir + "/cellular/config-data.json";
    SIM_DATA_FILE = dataDir + "/cellular/sim-data.json";
}

// Generic file operations
json CellularDataManager::loadJsonFile(const std::string& filePath) {
    try {
        std::ifstream file(filePath);
        if (file.is_open()) {
            json data;
            file >> data;
            file.close();
            std::cout << "[CELLULAR-DATA-MGR] Successfully loaded data from " << filePath << std::endl;
            return data;
        } else {
            std::cout << "[CELLULAR-DATA-MGR] File not found: " << filePath << ", creating default" << std::endl;
            return json::object();
        }
    } catch (const std::exception& e) {
        std::cout << "[CELLULAR-DATA-MGR] Error loading " << filePath << ": " << e.what() << std::endl;
        return json::object();
    }
}

bool CellularDataManager::saveJsonFile(const std::string& filePath, const json& data) {
    try {
        ensureDirectoryExists(filePath);
        std::ofstream file(filePath);
        if (file.is_open()) {
            file << data.dump(2);
            file.close();
            std::cout << "[CELLULAR-DATA-MGR] Data saved to " << filePath << std::endl;
            return true;
        } else {
            std::cout << "[CELLULAR-DATA-MGR] Error: Could not open " << filePath << " for writing" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cout << "[CELLULAR-DATA-MGR] Error saving " << filePath << ": " << e.what() << std::endl;
        return false;
    }
}

void CellularDataManager::ensureDirectoryExists(const std::string& filePath) {
    std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
}

// Status data operations
json CellularDataManager::loadStatusData() {
    auto data = loadJsonFile(STATUS_DATA_FILE);
    if (data.empty()) {
        data = createDefaultStatusData();
        saveStatusData(data);
    }
    return data;
}

bool CellularDataManager::saveStatusData(const json& data) {
    return saveJsonFile(STATUS_DATA_FILE, data);
}

json CellularDataManager::getNetworkInformation() {
    auto data = loadStatusData();
    return data.value("network_information", json::object());
}

json CellularDataManager::getSignalQuality() {
    auto data = loadStatusData();
    return data.value("signal_quality", json::object());
}

json CellularDataManager::getConnectionDetails() {
    auto data = loadStatusData();
    return data.value("connection_details", json::object());
}

bool CellularDataManager::updateNetworkInformation(const json& networkInfo) {
    auto data = loadStatusData();
    data["network_information"] = networkInfo;
    return saveStatusData(data);
}

bool CellularDataManager::updateSignalQuality(const json& signalInfo) {
    auto data = loadStatusData();
    data["signal_quality"] = signalInfo;
    return saveStatusData(data);
}

bool CellularDataManager::updateConnectionDetails(const json& connectionInfo) {
    auto data = loadStatusData();
    data["connection_details"] = connectionInfo;
    return saveStatusData(data);
}

// Scan data operations
json CellularDataManager::loadScanData() {
    auto data = loadJsonFile(SCAN_DATA_FILE);
    if (data.empty()) {
        data = createDefaultScanData();
        saveScanData(data);
    }
    return data;
}

bool CellularDataManager::saveScanData(const json& data) {
    return saveJsonFile(SCAN_DATA_FILE, data);
}

json CellularDataManager::getScanStatus() {
    auto data = loadScanData();
    return data.value("scan_status", json::object());
}

json CellularDataManager::getAvailableNetworks() {
    auto data = loadScanData();
    return data.value("available_networks", json::array());
}

bool CellularDataManager::updateScanStatus(const json& scanInfo) {
    auto data = loadScanData();
    data["scan_status"] = scanInfo;
    return saveScanData(data);
}

bool CellularDataManager::updateAvailableNetworks(const json& networks) {
    auto data = loadScanData();
    data["available_networks"] = networks;
    return saveScanData(data);
}

// Configuration data operations
json CellularDataManager::loadConfigData() {
    auto data = loadJsonFile(CONFIG_DATA_FILE);
    if (data.empty()) {
        data = createDefaultConfigData();
        saveConfigData(data);
    }
    return data;
}

bool CellularDataManager::saveConfigData(const json& data) {
    return saveJsonFile(CONFIG_DATA_FILE, data);
}

json CellularDataManager::getCellularConfiguration() {
    auto data = loadConfigData();
    return data.value("cellular_configuration", json::object());
}

bool CellularDataManager::updateCellularConfiguration(const json& config) {
    auto data = loadConfigData();
    data["cellular_configuration"] = config;
    return saveConfigData(data);
}

// SIM and Device data operations
json CellularDataManager::loadSimData() {
    auto data = loadJsonFile(SIM_DATA_FILE);
    if (data.empty()) {
        data = createDefaultSimData();
        saveSimData(data);
    }
    return data;
}

bool CellularDataManager::saveSimData(const json& data) {
    return saveJsonFile(SIM_DATA_FILE, data);
}

json CellularDataManager::getSimInformation() {
    auto data = loadSimData();
    return data.value("sim_information", json::object());
}

json CellularDataManager::getDeviceInformation() {
    auto data = loadSimData();
    return data.value("device_information", json::object());
}

bool CellularDataManager::updateSimInformation(const json& simInfo) {
    auto data = loadSimData();
    data["sim_information"] = simInfo;
    return saveSimData(data);
}

bool CellularDataManager::updateDeviceInformation(const json& deviceInfo) {
    auto data = loadSimData();
    data["device_information"] = deviceInfo;
    return saveSimData(data);
}

// Combined operations for compatibility
json CellularDataManager::getAllCellularData() {
    json combined;
    
    // Load all data files
    auto statusData = loadStatusData();
    auto scanData = loadScanData();
    auto configData = loadConfigData();
    auto simData = loadSimData();
    
    // Combine into single structure
    combined["network_information"] = statusData.value("network_information", json::object());
    combined["signal_quality"] = statusData.value("signal_quality", json::object());
    combined["connection_details"] = statusData.value("connection_details", json::object());
    combined["scan_status"] = scanData.value("scan_status", json::object());
    combined["available_networks"] = scanData.value("available_networks", json::array());
    combined["cellular_configuration"] = configData.value("cellular_configuration", json::object());
    combined["sim_information"] = simData.value("sim_information", json::object());
    combined["device_information"] = simData.value("device_information", json::object());
    
    return combined;
}

json CellularDataManager::getCurrentStatus() {
    json status;
    
    // Combine status-related data
    auto statusData = loadStatusData();
    auto simData = loadSimData();
    
    // Merge network information
    auto networkInfo = statusData.value("network_information", json::object());
    auto signalQuality = statusData.value("signal_quality", json::object());
    auto connectionDetails = statusData.value("connection_details", json::object());
    auto simInfo = simData.value("sim_information", json::object());
    auto deviceInfo = simData.value("device_information", json::object());
    
    // Create combined status
    status["operator"] = networkInfo.value("operator", "Not Connected");
    status["network_type"] = networkInfo.value("network_type", "N/A");
    status["band"] = networkInfo.value("band", "N/A");
    status["cell_id"] = networkInfo.value("cell_id", "Unknown");
    status["apn"] = networkInfo.value("apn", "Not Set");
    
    status["signal_bars"] = signalQuality.value("signal_bars", 0);
    status["rssi_dbm"] = signalQuality.value("rssi_dbm", 0);
    status["rsrp_dbm"] = signalQuality.value("rsrp_dbm", 0);
    status["rsrq_db"] = signalQuality.value("rsrq_db", 0);
    status["sinr_db"] = signalQuality.value("sinr_db", 0);
    status["signal_status"] = signalQuality.value("signal_status", "No Signal");
    status["has_signal"] = signalQuality.value("has_signal", false);
    status["signal_strength"] = signalQuality.value("signal_strength", 0);
    
    status["status"] = connectionDetails.value("status", "Disconnected");
    status["data_usage_mb"] = connectionDetails.value("data_usage_mb", 0);
    status["connection_time_seconds"] = connectionDetails.value("connection_time_seconds", 0);
    status["is_connected"] = connectionDetails.value("is_connected", false);
    status["connection_status"] = connectionDetails.value("connection_status", "Disconnected");
    
    status["sim_status"] = simInfo.value("sim_status", "Not Available");
    status["imsi"] = simInfo.value("imsi", "Not Available");
    status["iccid"] = simInfo.value("iccid", "Not Available");
    status["sim_type"] = simInfo.value("sim_type", "Unknown");
    status["pin_status"] = simInfo.value("pin_status", "Unknown");
    
    status["imei"] = deviceInfo.value("imei", "Not Available");
    status["device_model"] = deviceInfo.value("device_model", "Unknown");
    status["firmware_version"] = deviceInfo.value("firmware_version", "Unknown");
    status["temperature"] = deviceInfo.value("temperature", "Unknown");
    status["manufacturer"] = deviceInfo.value("manufacturer", "Unknown");
    
    return status;
}

// Default data creators
json CellularDataManager::createDefaultStatusData() {
    return json::parse(R"({
        "network_information": {
            "operator": "Not Connected",
            "network_type": "N/A",
            "band": "N/A",
            "cell_id": "Unknown",
            "apn": "Not Set"
        },
        "signal_quality": {
            "signal_bars": 0,
            "rssi_dbm": 0,
            "rsrp_dbm": 0,
            "rsrq_db": 0,
            "sinr_db": 0,
            "signal_status": "No Signal",
            "has_signal": false,
            "signal_strength": 0
        },
        "connection_details": {
            "status": "Disconnected",
            "data_usage_mb": 0,
            "connection_time_seconds": 0,
            "signal_status": "No Signal",
            "is_connected": false,
            "connection_status": "Disconnected"
        }
    })");
}

json CellularDataManager::createDefaultScanData() {
    return json::parse(R"({
        "scan_status": {
            "last_scan": "Not scanned",
            "scan_in_progress": false,
            "networks_found": 0,
            "scan_completed": false,
            "strongest_signal": 0
        },
        "available_networks": []
    })");
}

json CellularDataManager::createDefaultConfigData() {
    return json::parse(R"({
        "cellular_configuration": {
            "apn": "",
            "username": "",
            "password": "",
            "preferred_network_type": "auto",
            "auto_connect": false,
            "data_roaming": false,
            "data_limit_mb": 0,
            "data_limit_enabled": false,
            "auth_type": "none",
            "network_selection": "automatic",
            "connection_timeout": 60,
            "retry_attempts": 3,
            "pdp_type": "ipv4"
        }
    })");
}

json CellularDataManager::createDefaultSimData() {
    return json::parse(R"({
        "sim_information": {
            "sim_status": "Not Available",
            "imsi": "Not Available",
            "iccid": "Not Available",
            "sim_type": "Unknown",
            "pin_status": "Unknown"
        },
        "device_information": {
            "imei": "Not Available",
            "device_model": "Unknown",
            "firmware_version": "Unknown",
            "temperature": "Unknown",
            "manufacturer": "Unknown"
        }
    })");
}

bool CellularDataManager::validateDataStructure(const json& data, const std::string& type) {
    if (type == "status") {
        return data.contains("network_information") && 
               data.contains("signal_quality") && 
               data.contains("connection_details");
    } else if (type == "scan") {
        return data.contains("scan_status") && 
               data.contains("available_networks");
    } else if (type == "config") {
        return data.contains("cellular_configuration");
    } else if (type == "sim") {
        return data.contains("sim_information") && 
               data.contains("device_information");
    }
    return false;
}
