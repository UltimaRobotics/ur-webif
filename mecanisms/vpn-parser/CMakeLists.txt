
# VPN Parser Mechanism
add_library(vpn_parser_mechanism STATIC
    openvpn_parser.cpp
    ikev2_parser.cpp
    wireguard_parser.cpp
    vpn_parser_manager.cpp
)

target_include_directories(vpn_parser_mechanism PUBLIC .)
target_link_libraries(vpn_parser_mechanism ${CMAKE_REQUIRED_LIBRARIES})
