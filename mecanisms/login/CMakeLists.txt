# Login Mechanism CMake Configuration
cmake_minimum_required(VERSION 3.10)

# Find required packages (OpenSSL only, nlohmann_json comes from parent)
find_package(OpenSSL REQUIRED)

# Create login mechanism library
add_library(login_mechanism STATIC
    login_manager.cpp
    login_handler.cpp
    security_utils.cpp
)

# Set include directories
target_include_directories(login_mechanism PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

# Link libraries (nlohmann_json is header-only, no linking needed)
target_link_libraries(login_mechanism
    ${OPENSSL_LIBRARIES}
)

# Set C++ standard
set_property(TARGET login_mechanism PROPERTY CXX_STANDARD 17)
set_property(TARGET login_mechanism PROPERTY CXX_STANDARD_REQUIRED ON)

# Add compiler warnings
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(login_mechanism PRIVATE -Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(login_mechanism PRIVATE /W4)
endif()

# Add security-related compile definitions
target_compile_definitions(login_mechanism PRIVATE
    OPENSSL_API_COMPAT=0x10100000L
)

# Create directories for logs if they don't exist
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)