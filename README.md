# UR WebIF API

A generic web server API built with C++ using libmicrohttpd and WebSocket++ libraries. This server provides a modern web interface with real-time WebSocket communication and REST API endpoints.

## Features

- **HTTP Server**: Based on libmicrohttpd for high-performance HTTP handling
- **WebSocket Support**: Real-time bidirectional communication using WebSocket++
- **JSON Configuration**: Flexible server configuration via JSON files
- **Static File Serving**: Efficient serving of HTML, CSS, JS, and other assets
- **REST API**: Built-in API endpoints for common operations
- **CORS Support**: Cross-origin resource sharing enabled
- **Modern Web UI**: Responsive login interface with TailwindCSS

## Architecture

### Core Components

1. **WebServer**: Main server class that orchestrates all components
2. **ConfigParser**: Handles JSON configuration file parsing
3. **HttpHandler**: Processes HTTP requests and API routes
4. **WebSocketHandler**: Manages WebSocket connections and messaging
5. **FileServer**: Serves static files with proper MIME type detection

### Dependencies

- **libmicrohttpd**: HTTP server library
- **WebSocket++**: WebSocket implementation
- **nlohmann/json**: JSON parsing and generation
- **C++17**: Modern C++ standard

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler (GCC 8+, Clang 7+, MSVC 2019+)
- Git (for fetching dependencies)

### Build Steps

```bash
cd ur-webif-api
mkdir build
cd build
cmake ..
make -j$(nproc)
```

The build system automatically downloads and builds all dependencies using CMake's FetchContent.

## Configuration

Server configuration is managed through JSON files. Default configuration:

```json
{
    "port": 8080,
    "host": "0.0.0.0",
    "document_root": "./web",
    "default_file": "index.html",
    "enable_websocket": true,
    "websocket_port": 8081,
    "enable_ssl": false,
    "max_connections": 1000,
    "connection_timeout": 30,
    "enable_cors": true,
    "allowed_origins": ["*"]
}
```

## Usage

### Basic Usage

```bash
./ur-webif-api
```

### Command Line Options

```bash
./ur-webif-api [options]

Options:
  -c, --config <file>        Configuration file path (default: config/server.json)
  -p, --port <port>          Server port (default: 8080)
  -d, --document-root <path> Document root directory (default: ./web)
  -h, --help                 Show help message
```

### Running with Custom Configuration

```bash
./ur-webif-api -c my_config.json -p 9000
```

## API Endpoints

### Authentication

- **POST /api/login**: User authentication
  ```json
  {
    "username": "user",
    "password": "password"
  }
  ```

- **GET /api/status**: Server status information

### WebSocket API

Connect to `ws://localhost:8081` for real-time communication.

#### Message Types

1. **Ping/Pong**:
   ```json
   {
     "type": "ping",
     "timestamp": 1234567890
   }
   ```

2. **Echo**:
   ```json
   {
     "type": "echo",
     "data": "message to echo"
   }
   ```

3. **Broadcast**:
   ```json
   {
     "type": "broadcast",
     "data": "message to broadcast to all clients"
   }
   ```

## Web Interface

The included web interface provides:

- Modern login form with username/password authentication
- File upload support for authentication documents
- Real-time WebSocket status indicator
- Responsive design with TailwindCSS
- Interactive notifications and feedback

### Features

- Password visibility toggle
- Drag & drop file upload
- Form validation
- Real-time connection status
- Cross-browser compatibility

## Extending the API

### Adding New Routes

```cpp
server.addRouteHandler("/api/users", [](const std::string& method, 
                                       const std::map<std::string, std::string>& params,
                                       const std::string& body) -> std::string {
    if (method == "GET") {
        nlohmann::json response;
        response["users"] = {"alice", "bob", "charlie"};
        return response.dump();
    }
    return R"({"error": "Method not allowed"})";
});
```

### WebSocket Message Handling

```cpp
server.setWebSocketMessageHandler([](int connection_id, const std::string& message) {
    // Handle incoming WebSocket messages
    auto json_msg = nlohmann::json::parse(message);
    // Process and respond...
});
```

## Security Considerations

- Input validation on all API endpoints
- File upload restrictions (type and size limits)
- CORS configuration for cross-origin requests
- SSL/TLS support (configurable)
- Connection limits and timeouts

## Performance

- Multi-threaded request handling
- Efficient static file serving with caching headers
- Asynchronous WebSocket connections
- Configurable connection limits

## Troubleshooting

### Common Issues

1. **Port already in use**: Change the port in configuration or command line
2. **Permission denied**: Ensure the user has rights to bind to the specified port
3. **WebSocket connection fails**: Check firewall settings for the WebSocket port

### Debugging

Enable verbose logging by modifying the WebSocket log levels in the source code:

```cpp
server_.set_access_channels(websocketpp::log::alevel::all);
```

## License

This project is open source. Please refer to the license file for details.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## Changelog

### Version 1.0.0
- Initial release
- HTTP server with libmicrohttpd
- WebSocket support
- JSON configuration
- Modern web interface
- File serving capabilities