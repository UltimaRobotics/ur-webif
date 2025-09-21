#include "security_utils.hpp"
#include <regex>
#include <random>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <unordered_map>

// Static member definitions
std::unordered_map<std::string, SecurityUtils::RateLimit> SecurityUtils::rate_limits_;
std::mutex SecurityUtils::rate_limit_mutex_;

bool SecurityUtils::isValidIPAddress(const std::string& ip) {
    // IPv4 validation
    std::regex ipv4_regex(
        R"(^((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );

    if (std::regex_match(ip, ipv4_regex)) {
        return true;
    }

    // IPv6 validation (simplified)
    std::regex ipv6_regex(
        R"(^([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::1$|^::$)"
    );

    return std::regex_match(ip, ipv6_regex);
}

bool SecurityUtils::isPrivateIPAddress(const std::string& ip) {
    // Check for private IPv4 ranges
    std::regex private_ranges[] = {
        std::regex(R"(^10\.)"),              // 10.0.0.0/8
        std::regex(R"(^192\.168\.)"),        // 192.168.0.0/16
        std::regex(R"(^172\.(1[6-9]|2[0-9]|3[01])\.)"), // 172.16.0.0/12
        std::regex(R"(^127\.)"),             // 127.0.0.0/8 (loopback)
        std::regex(R"(^169\.254\.)"),        // 169.254.0.0/16 (link-local)
    };

    for (const auto& regex : private_ranges) {
        if (std::regex_search(ip, regex)) {
            return true;
        }
    }

    return false;
}

std::string SecurityUtils::extractClientIP(const std::string& forwarded_header, 
                                          const std::string& remote_addr) {
    // Priority order: X-Forwarded-For, X-Real-IP, Remote-Addr
    if (!forwarded_header.empty()) {
        // X-Forwarded-For can contain multiple IPs, take the first one
        size_t comma_pos = forwarded_header.find(',');
        std::string first_ip = (comma_pos != std::string::npos) 
                              ? forwarded_header.substr(0, comma_pos)
                              : forwarded_header;

        // Trim whitespace
        first_ip.erase(0, first_ip.find_first_not_of(" \t"));
        first_ip.erase(first_ip.find_last_not_of(" \t") + 1);

        if (isValidIPAddress(first_ip)) {
            return first_ip;
        }
    }

    if (isValidIPAddress(remote_addr)) {
        return remote_addr;
    }

    return "unknown";
}

bool SecurityUtils::checkRateLimit(const std::string& identifier, 
                                  int max_requests, 
                                  std::chrono::seconds window) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);

    auto now = std::chrono::steady_clock::now();
    auto& limit = rate_limits_[identifier];

    // Check if window has expired
    if (now > limit.window_start + window) {
        limit.window_start = now;
        limit.current_requests = 0;
    }

    // Check if under limit
    if (limit.current_requests >= max_requests) {
        return false; // Rate limit exceeded
    }

    limit.current_requests++;
    return true;
}

void SecurityUtils::resetRateLimit(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    rate_limits_.erase(identifier);
}

bool SecurityUtils::isValidUsername(const std::string& username) {
    if (username.empty() || username.length() > 50) {
        return false;
    }

    // Allow alphanumeric, underscore, hyphen, and dot
    std::regex username_regex("^[a-zA-Z0-9._-]+$");
    return std::regex_match(username, username_regex);
}

bool SecurityUtils::isValidSessionToken(const std::string& token) {
    if (token.empty() || token.length() != 128) { // Expecting 64-byte hex string
        return false;
    }

    // Check if token is valid hexadecimal
    std::regex hex_regex("^[0-9a-fA-F]+$");
    return std::regex_match(token, hex_regex);
}

std::string SecurityUtils::sanitizeString(const std::string& input, size_t max_length) {
    std::string sanitized;
    sanitized.reserve(std::min(input.length(), max_length));

    for (char c : input) {
        // Allow printable ASCII characters except control characters
        if (c >= 32 && c <= 126) {
            sanitized.push_back(c);
        }

        if (sanitized.length() >= max_length) {
            break;
        }
    }

    return sanitized;
}

void SecurityUtils::constantTimeDelay(std::chrono::milliseconds delay) {
    // Perform some lightweight computation to consume time
    volatile int dummy = 0;
    auto end = std::chrono::steady_clock::now() + delay;
    while (std::chrono::steady_clock::now() < end) {
        dummy++;
    }
}

SecurityUtils::PasswordStrength SecurityUtils::evaluatePasswordStrength(const std::string& password) {
    int score = 0;

    // Length check
    if (password.length() >= 8) score++;
    if (password.length() >= 12) score++;
    if (password.length() >= 16) score++;

    // Character variety
    bool has_lower = std::any_of(password.begin(), password.end(), [](char c) { return std::islower(c); });
    bool has_upper = std::any_of(password.begin(), password.end(), [](char c) { return std::isupper(c); });
    bool has_digit = std::any_of(password.begin(), password.end(), [](char c) { return std::isdigit(c); });
    bool has_special = std::any_of(password.begin(), password.end(), [](char c) { 
        return !std::isalnum(c) && c != ' ';
    });

    if (has_lower) score++;
    if (has_upper) score++;
    if (has_digit) score++;
    if (has_special) score++;

    // Check for common patterns
    std::regex weak_patterns[] = {
        std::regex("(.)\\1{2,}"),           // Repeated characters
        std::regex("(012|123|234|345|456|567|678|789|890)"), // Sequential numbers
        std::regex("(abc|bcd|cde|def|efg|fgh|ghi|hij|ijk)"), // Sequential letters
    };

    bool has_weak_pattern = false;
    for (const auto& pattern : weak_patterns) {
        if (std::regex_search(password, pattern)) {
            has_weak_pattern = true;
            break;
        }
    }

    if (has_weak_pattern) {
        score -= 2;
    }

    // Determine strength
    if (score >= 6) return PasswordStrength::VERY_STRONG;
    if (score >= 4) return PasswordStrength::STRONG;
    if (score >= 2) return PasswordStrength::MEDIUM;
    return PasswordStrength::WEAK;
}

bool SecurityUtils::isPasswordSecure(const std::string& password, PasswordStrength min_strength) {
    return evaluatePasswordStrength(password) >= min_strength;
}

std::string SecurityUtils::generateSecureRandom(size_t length) {
    auto bytes = generateSecureRandomBytes(length);

    std::stringstream ss;
    for (uint8_t byte : bytes) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }

    return ss.str();
}

std::vector<uint8_t> SecurityUtils::generateSecureRandomBytes(size_t length) {
    std::vector<uint8_t> bytes(length);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(0, 255);

    for (size_t i = 0; i < length; ++i) {
        bytes[i] = static_cast<uint8_t>(dis(gen));
    }

    return bytes;
}

void SecurityUtils::logSecurityEvent(const std::string& event_type, 
                                    const std::string& client_ip,
                                    const std::string& username,
                                    const std::string& details) {
    // Log to console (in production, use proper logging framework)
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::cout << "[SECURITY] " 
              << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << " | " << event_type
              << " | IP: " << client_ip;

    if (!username.empty()) {
        std::cout << " | User: " << username;
    }

    if (!details.empty()) {
        std::cout << " | " << details;
    }

    std::cout << std::endl;

    // In production, also log to file
    std::ofstream log_file("./logs/security.log", std::ios::app);
    if (log_file.is_open()) {
        log_file << "[SECURITY] " 
                 << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                 << " | " << event_type
                 << " | IP: " << client_ip;

        if (!username.empty()) {
            log_file << " | User: " << username;
        }

        if (!details.empty()) {
            log_file << " | " << details;
        }

        log_file << std::endl;
        log_file.close();
    }
}