
#pragma once

#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "credential_manager.h"

using json = nlohmann::json;

/**
 * Password Manager
 * 
 * Handles password change operations with validation and security.
 */
class PasswordManager {
public:
    struct PasswordChangeResult {
        bool success;
        std::string message;
        std::string error_code;
    };

    PasswordManager(std::shared_ptr<CredentialManager> credential_manager);
    ~PasswordManager() = default;

    // Main password change method
    PasswordChangeResult changePassword(
        const std::string& username,
        const std::string& current_password,
        const std::string& new_password,
        const std::string& confirm_password,
        const std::string& session_token = ""
    );

    // Password validation
    bool validatePasswordStrength(const std::string& password, std::string& error_message);
    bool validateCurrentPassword(const std::string& username, const std::string& current_password);

private:
    std::shared_ptr<CredentialManager> credential_manager_;

    // Password strength validation helpers
    bool hasMinimumLength(const std::string& password);
    bool hasUppercase(const std::string& password);
    bool hasLowercase(const std::string& password);
    bool hasDigit(const std::string& password);
    bool hasSpecialChar(const std::string& password);
    bool isNotCommonPassword(const std::string& password);
};
