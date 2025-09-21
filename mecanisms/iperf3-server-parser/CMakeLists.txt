
cmake_minimum_required(VERSION 3.16)

# Create a static library for the iperf3 server parser
add_library(iperf3_server_parser STATIC
    iperf3_server_parser.cpp
    iperf3_server_parser.h
)

# Set include directories
target_include_directories(iperf3_server_parser PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/nlohmann
)

# Set compiler flags
target_compile_features(iperf3_server_parser PRIVATE cxx_std_17)
target_compile_options(iperf3_server_parser PRIVATE -Wall -Wextra)

# Install headers
install(FILES iperf3_server_parser.h
    DESTINATION include/iperf3-server-parser
)

# Install library
install(TARGETS iperf3_server_parser
    ARCHIVE DESTINATION lib
)
