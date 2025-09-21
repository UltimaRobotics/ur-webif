
# HTTP Client Architecture Documentation

## Overview

This document describes the HTTP client architecture used in the UR WebIF API frontend, specifically how it's implemented in the Firmware Upgrade section. This serves as a reference guide for implementing similar functionality in other sections.

## Architecture Components

### 1. Backend Router Structure

Each section has a dedicated router class that handles API endpoints:

```cpp
class FirmwareRouter {
public:
    FirmwareRouter();
    ~FirmwareRouter();
    
    // Register all routes for this section
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler);
    
    // Individual endpoint handlers
    std::string handleFirmwareStatus(const ApiRequest& request);
    std::string handleUpgradeReadiness(const ApiRequest& request);
    std::string handleUpgradeAction(const ApiRequest& request);
    std::string handleUpgradeProgress(const ApiRequest& request);
    
private:
    std::string loadJsonFile(const std::string& filename);
    std::string generateTimestamp();
};
```

#### Key Router Characteristics:
- **Modular Design**: Each functional area has its own router
- **Consistent Interface**: All routers follow the same pattern
- **Route Registration**: Routes are registered through a lambda function
- **JSON Response Format**: All responses are in JSON format
- **Error Handling**: Proper error responses with status codes

### 2. API Endpoint Structure

Each router exposes multiple endpoints following RESTful conventions:

```cpp
void FirmwareRouter::registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler) {
    // Status endpoint - GET request
    addRouteHandler("/api/firmware/status", [this](const std::string& method, ...) {
        // Handler implementation
    });
    
    // Action endpoint - POST request  
    addRouteHandler("/api/firmware/upgrade", [this](const std::string& method, ...) {
        // Handler implementation
    });
}
```

#### Endpoint Naming Convention:
- `/api/{section}/{action}` format
- Clear, descriptive action names
- HTTP method appropriate to action (GET for data, POST for actions)

### 3. Data Storage Structure

Each section maintains its data in JSON files under the `data/` directory:

```
data/
├── firmware/
│   ├── current-firmware.json      # Current firmware information
│   ├── upgrade-readiness.json     # Pre-upgrade checks
│   └── upgrade-progress.json      # Upgrade progress tracking
```

#### Data File Characteristics:
- **Structured JSON**: Well-defined schemas
- **Timestamp Tracking**: All responses include timestamps
- **Status Information**: Clear status indicators
- **Error Handling**: Graceful degradation when files are missing

### 4. Frontend HTTP Client Implementation

The frontend uses a consistent pattern for HTTP requests:

```javascript
// Load firmware data from backend
async function loadFirmwareData() {
    console.log('[FIRMWARE] Loading firmware data...');
    try {
        const response = await fetch('/api/firmware/status');
        const data = await response.json();
        
        if (data.success) {
            console.log('[FIRMWARE] Firmware data loaded successfully:', data);
            updateFirmwareDisplay(data);
        } else {
            console.error('[FIRMWARE] Failed to load firmware data:', data.error);
            showFirmwareError('Failed to load firmware information');
        }
    } catch (error) {
        console.error('[FIRMWARE] Error loading firmware data:', error);
        showFirmwareError('Network error loading firmware data');
    }
}
```

#### Frontend Client Characteristics:
- **Async/Await Pattern**: Modern JavaScript async handling
- **Comprehensive Logging**: Detailed console logging for debugging
- **Error Handling**: Multiple levels of error handling
- **User Feedback**: Visual feedback for success/error states
- **Data Validation**: Check response structure before using

### 5. Request/Response Flow

#### Typical GET Request Flow:
1. Frontend initiates fetch request
2. Backend router receives request
3. Router loads data from JSON file
4. Router formats response with success/error status
5. Frontend processes response and updates UI

#### Typical POST Request Flow:
1. Frontend collects form data
2. Frontend sends POST request with JSON body
3. Backend validates request data
4. Backend processes action and updates data files
5. Backend returns success/error response
6. Frontend updates UI based on response

### 6. Error Handling Patterns

#### Backend Error Handling:
```cpp
std::string FirmwareRouter::handleFirmwareStatus(const ApiRequest& request) {
    try {
        if (request.method == "GET") {
            std::string firmwareData = loadJsonFile("data/firmware/current-firmware.json");
            if (firmwareData.empty()) {
                return R"({"success":false,"error":"Failed to load firmware data","timestamp":)" + generateTimestamp() + "}";
            }
            
            json response = json::parse(firmwareData);
            response["success"] = true;
            response["timestamp"] = std::stoull(generateTimestamp());
            
            return response.dump();
        }
        
        return R"({"success":false,"error":"Method not allowed","timestamp":)" + generateTimestamp() + "}";
        
    } catch (const std::exception& e) {
        return R"({"success":false,"error":"Failed to process firmware status","error_code":500,"timestamp":)" + generateTimestamp() + "}";
    }
}
```

#### Frontend Error Handling:
```javascript
async function loadFirmwareData() {
    try {
        const response = await fetch('/api/firmware/status');
        const data = await response.json();
        
        if (data.success) {
            // Success path
            updateFirmwareDisplay(data);
        } else {
            // API-level error
            showFirmwareError('Failed to load firmware information');
        }
    } catch (error) {
        // Network or parsing error
        showFirmwareError('Network error loading firmware data');
    }
}
```

### 7. Integration with Main Server

Routes are registered in `main.cpp` using a consistent pattern:

```cpp
// Register Firmware router
FirmwareRouter firmwareRouter;
server.addRouteHandler("/api/firmware/status", [&firmwareRouter](const std::string& method, const std::map<std::string, std::string>& params, const std::string& body) -> std::string {
    ApiRequest request;
    request.method = method;
    request.body = body;
    return firmwareRouter.handleFirmwareStatus(request);
});
```

## Implementation Best Practices

### 1. Router Implementation
- Create dedicated router class for each functional area
- Implement consistent error handling patterns
- Use JSON for all API responses
- Include timestamps in all responses
- Log all requests and responses for debugging

### 2. Data Management
- Store section data in dedicated subdirectory under `data/`
- Use descriptive JSON file names
- Implement graceful handling of missing files
- Maintain data structure consistency

### 3. Frontend Implementation
- Use async/await for all HTTP requests
- Implement comprehensive error handling
- Provide user feedback for all operations
- Log requests and responses for debugging
- Validate response structure before processing

### 4. URL Structure
- Follow `/api/{section}/{action}` pattern
- Use HTTP methods appropriately (GET for data, POST for actions)
- Keep endpoint names descriptive and consistent

### 5. Response Format
All API responses should follow this structure:
```json
{
    "success": true|false,
    "timestamp": 1234567890,
    "data": {...},
    "error": "Error message if success=false",
    "error_code": 500
}
```

## Example Implementation Template

### Backend Router Template:
```cpp
#include "routers/SectionRouter.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include "nlohmann/json.hpp"

class SectionRouter {
public:
    void registerRoutes(std::function<void(const std::string&, RouteHandler)> addRouteHandler) {
        addRouteHandler("/api/section/data", [this](const std::string& method, ...) {
            ApiRequest req;
            req.method = method;
            return this->handleSectionData(req);
        });
    }

private:
    std::string handleSectionData(const ApiRequest& request) {
        try {
            if (request.method == "GET") {
                std::string data = loadJsonFile("data/section/section-data.json");
                if (data.empty()) {
                    return R"({"success":false,"error":"Failed to load data","timestamp":)" + generateTimestamp() + "}";
                }
                
                json response = json::parse(data);
                response["success"] = true;
                response["timestamp"] = std::stoull(generateTimestamp());
                
                return response.dump();
            }
        } catch (const std::exception& e) {
            return R"({"success":false,"error":"Internal server error","timestamp":)" + generateTimestamp() + "}";
        }
    }
    
    std::string loadJsonFile(const std::string& filename) {
        // Implementation similar to FirmwareRouter
    }
    
    std::string generateTimestamp() {
        // Implementation similar to FirmwareRouter
    }
};
```

### Frontend Template:
```javascript
class SectionManager {
    constructor() {
        this.apiEndpoint = '/api/section/data';
        this.init();
    }
    
    async init() {
        await this.loadSectionData();
        this.setupEventHandlers();
    }
    
    async loadSectionData() {
        try {
            console.log('[SECTION] Loading section data...');
            const response = await fetch(this.apiEndpoint);
            const data = await response.json();
            
            if (data.success) {
                console.log('[SECTION] Data loaded successfully:', data);
                this.updateDisplay(data);
            } else {
                console.error('[SECTION] Failed to load data:', data.error);
                this.showError('Failed to load section data');
            }
        } catch (error) {
            console.error('[SECTION] Network error:', error);
            this.showError('Network error loading data');
        }
    }
    
    updateDisplay(data) {
        // Update UI with loaded data
    }
    
    showError(message) {
        // Show error message to user
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    new SectionManager();
});
```

## Testing and Debugging

### 1. Backend Testing
- Test each endpoint with different HTTP methods
- Verify error handling with invalid requests
- Check JSON file loading and parsing
- Validate response format consistency

### 2. Frontend Testing
- Test network error scenarios
- Verify error message display
- Check console logging output
- Test with malformed server responses

### 3. Integration Testing
- Test complete request/response flow
- Verify data persistence
- Check error propagation
- Test concurrent request handling

This architecture provides a robust, scalable foundation for HTTP client functionality that can be adapted to any section of the application while maintaining consistency and reliability.
