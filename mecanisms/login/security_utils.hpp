#ifndef SECURITY_UTILS_HPP
#define SECURITY_UTILS_HPP

#include <string>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <mutex>

class SecurityUtils {
public:
    // IP address validation and filtering
    static bool isValidIPAddress(const std::string& ip);
    static bool isPrivateIPAddress(const std::string& ip);
    static std::string extractClientIP(const std::string& forwarded_header,
                                     const std::string& remote_addr);

    // Rate limiting
    struct RateLimit {
        int requests_per_window;
        std::chrono::seconds window_duration;
        std::chrono::steady_clock::time_point window_start;
        int current_requests;
    };

    static bool checkRateLimit(const std::string& identifier,
                              int max_requests,
                              std::chrono::seconds window);
    static void resetRateLimit(const std::string& identifier);

    // Input validation
    static bool isValidUsername(const std::string& username);
    static bool isValidSessionToken(const std::string& token);
    static std::string sanitizeString(const std::string& input, size_t max_length = 256);

    // Timing attack protection
    static void constantTimeDelay(std::chrono::milliseconds min_delay = std::chrono::milliseconds(100));

    // Password strength validation
    enum class PasswordStrength {
        WEAK,
        MEDIUM,
        STRONG,
        VERY_STRONG
    };

    static PasswordStrength evaluatePasswordStrength(const std::string& password);
    static bool isPasswordSecure(const std::string& password, PasswordStrength min_strength = PasswordStrength::MEDIUM);

    // Secure random generation
    static std::string generateSecureRandom(size_t length);
    static std::vector<uint8_t> generateSecureRandomBytes(size_t length);

    // Logging and audit
    static void logSecurityEvent(const std::string& event_type,
                                const std::string& client_ip,
                                const std::string& username = "",
                                const std::string& details = "");

private:
    static std::unordered_map<std::string, RateLimit> rate_limits_;
    static std::mutex rate_limit_mutex_;
};

#endif // SECURITY_UTILS_HPP