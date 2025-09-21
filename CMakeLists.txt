cmake_minimum_required(VERSION 3.16)
project(ur-webif-api VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use system packages for most dependencies, FetchContent only for nlohmann_json
include(FetchContent)

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
# Find ZLIB using pkg-config
pkg_check_modules(ZLIB REQUIRED zlib)

# Find libmicrohttpd using pkg-config
pkg_check_modules(MICROHTTPD REQUIRED libmicrohttpd)

# Use locally downloaded ASIO library
set(ASIO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/asio/asio/include)
if(NOT EXISTS "${ASIO_INCLUDE_DIR}/asio.hpp")
    message(FATAL_ERROR "ASIO headers not found at ${ASIO_INCLUDE_DIR}")
endif()

# Use locally downloaded websocketpp headers
set(WEBSOCKETPP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/websocketpp)
if(NOT EXISTS "${WEBSOCKETPP_INCLUDE_DIR}/websocketpp/config/asio_no_tls.hpp")
    message(FATAL_ERROR "websocketpp headers not found at ${WEBSOCKETPP_INCLUDE_DIR}")
endif()

# Use locally downloaded nlohmann_json header
set(NLOHMANN_JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
if(NOT EXISTS "${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
    message(FATAL_ERROR "nlohmann_json header not found at ${NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
endif()

message(STATUS "Using local ASIO headers at: ${ASIO_INCLUDE_DIR}")
message(STATUS "Using local websocketpp headers at: ${WEBSOCKETPP_INCLUDE_DIR}")
message(STATUS "Using local nlohmann_json header at: ${NLOHMANN_JSON_INCLUDE_DIR}")

# Include directories
include_directories(
    src/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    src
    src/network-ops
    mecanisms/login
    ${ASIO_INCLUDE_DIR}
)

# Additional include directory for build
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Include local header directories
include_directories(${WEBSOCKETPP_INCLUDE_DIR})
include_directories(${NLOHMANN_JSON_INCLUDE_DIR})
include_directories(${ASIO_INCLUDE_DIR})

# Add mechanism subdirectories
add_subdirectory(mecanisms/login)
add_subdirectory(mecanisms/vpn-parser)
# Iperf3 Server Parser Library - DEPRECATED, using Iperf3ServersEngine instead
# add_subdirectory(mecanisms/iperf3-server-parser)

# Add all source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.c"
)

# Add backup-restore sources specifically
file(GLOB BACKUP_RESTORE_SOURCES
    "src/backup-restore/*.cpp"
)

# Source files
set(SOURCES
    src/main.cpp
    src/web_server.cpp
    src/http_handler.cpp
    src/dynamic_router.cpp
    src/websocket_handler.cpp
    src/file_server.cpp
    src/api_endpoints.cpp
    src/auth_router.cpp
    src/config_parser.cpp
    src/config_manager.cpp
    src/credential_manager.cpp
    src/password_manager.cpp
    src/route_processors.cpp
    src/dashboard_router.cpp
    src/dashboard_globals.cpp
    src/http_event_handler.cpp
    src/endpoint_logger.cpp
    src/source-page-data.cpp
    src/utils_router.cpp
    src/wired_router.cpp
    src/cellular_data_manager.cpp
    src/vpn_data_manager.cpp
    src/license_data_structure.cpp
    src/routers/BackupRouter.cpp
    src/routers/FirmwareRouter.cpp
    src/routers/LicenseRouter.cpp
    src/firmware-update/tftp_client.cpp
    src/firmware-update/manual_upload_handler.cpp
    src/sysupgrade-mecanism/sysupgrade_handler.cpp
    src/multipart_parser.cpp
    src/routers/VpnRouter.cpp
    src/routers/WirelessRouter.cpp
    src/routers/NetworkPriorityRouter.cpp
    src/routers/NetworkUtilityRouter.cpp
    src/network_priority_data_manager.cpp
    src/routers/AdvancedNetworkRouter.cpp
    src/auth-gen/auth_access_generator.cpp
    src/auth-gen/auth_access_router.cpp
    src/network-ops/vlan-handler.cpp
    src/network-ops/bridge-handler.cpp
    src/network-ops/nat-handler.cpp
    src/network-ops/firewall-handler.cpp
    src/network-ops/static-routes-handler.cpp
    src/utilities/BandwidthUtilityEngine.cpp
    src/utilities/PingUtilityEngine.cpp
    src/utilities/TracerouteUtilityEngine.cpp
    src/utilities/DNSLookupUtilityEngine.cpp
    src/utilities/Iperf3ServersEngine.cpp
)

# Create executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${LOGIN_SOURCES}
    ${VPN_PARSER_SOURCES}
    ${BACKUP_RESTORE_SOURCES}
)

# Link libraries to main executable
target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    ${CMAKE_THREAD_LIBS_INIT}
    microhttpd
    OpenSSL::SSL
    OpenSSL::Crypto
    ${ZLIB_LIBRARIES}
    vpn_parser_mechanism
)

# Add include directories for libmicrohttpd, websocketpp, and ZLIB
target_include_directories(${PROJECT_NAME} PRIVATE
    ${MICROHTTPD_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

# Header-only libraries don't need dependencies

# Add compile flags for libmicrohttpd, websocketpp, and ZLIB  
target_compile_options(${PROJECT_NAME} PRIVATE ${MICROHTTPD_CFLAGS_OTHER} ${ZLIB_CFLAGS_OTHER})
target_compile_definitions(${PROJECT_NAME} PRIVATE
    ASIO_STANDALONE
    _WEBSOCKETPP_CPP11_STL_
)

# No additional compile definitions needed

# Install target
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Install web files
install(DIRECTORY web/
    DESTINATION share/${PROJECT_NAME}/web
)

# Install config files
install(DIRECTORY config/
    DESTINATION share/${PROJECT_NAME}/config
)

# Add utility test executables
add_executable(test_bandwidth_utility 
    src/utilities/test_bandwidth_utility.cpp
    src/utilities/BandwidthUtilityEngine.cpp
    src/endpoint_logger.cpp
)

add_executable(test_ping_utility 
    src/utilities/test_ping_utility.cpp
    src/utilities/PingUtilityEngine.cpp
    src/endpoint_logger.cpp
)

add_executable(test_traceroute_utility 
    src/utilities/test_traceroute_utility.cpp
    src/utilities/TracerouteUtilityEngine.cpp
    src/endpoint_logger.cpp
)

add_executable(test_dns_lookup_utility 
    src/utilities/test_dns_lookup_utility.cpp
    src/utilities/DNSLookupUtilityEngine.cpp
    src/endpoint_logger.cpp
)

add_executable(test_iperf3_servers_engine 
    src/utilities/test_iperf3_servers_engine.cpp
    src/utilities/Iperf3ServersEngine.cpp
    src/endpoint_logger.cpp
)

# Link libraries for test executables
target_link_libraries(test_bandwidth_utility ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_ping_utility ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_traceroute_utility ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_dns_lookup_utility ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_iperf3_servers_engine ${CMAKE_THREAD_LIBS_INIT})

# Set include directories for test executables
target_include_directories(test_bandwidth_utility PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(test_ping_utility PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(test_traceroute_utility PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(test_dns_lookup_utility PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(test_iperf3_servers_engine PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/third_party)