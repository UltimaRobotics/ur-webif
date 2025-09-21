
#include "password_manager.h"
#include "endpoint_logger.h"
#include <iostream>
#include <algorithm>
#include <regex>
#include <set>

PasswordManager::PasswordManager(std::shared_ptr<CredentialManager> credential_manager)
    : credential_manager_(credential_manager) {
    ENDPOINT_LOG_INFO("password", "PasswordManager initialized");
}

PasswordManager::PasswordChangeResult PasswordManager::changePassword(
    const std::string& username,
    const std::string& current_password,
    const std::string& new_password,
    const std::string& confirm_password,
    const std::string& session_token) {

    PasswordChangeResult result;
    result.success = false;

    ENDPOINT_LOG_INFO("password", "Processing password change request for user: " + username);

    // Validate input parameters
    if (username.empty() || current_password.empty() || new_password.empty() || confirm_password.empty()) {
        result.message = "All fields are required";
        result.error_code = "MISSING_FIELDS";
        ENDPOINT_LOG_ERROR("password", "Password change failed: Missing required fields");
        return result;
    }

    // Check if new password matches confirmation
    if (new_password != confirm_password) {
        result.message = "New passwords do not match";
        result.error_code = "PASSWORD_MISMATCH";
        ENDPOINT_LOG_ERROR("password", "Password change failed: Password confirmation mismatch");
        return result;
    }

    // Validate current password
    if (!validateCurrentPassword(username, current_password)) {
        result.message = "Current password is incorrect";
        result.error_code = "INVALID_CURRENT_PASSWORD";
        ENDPOINT_LOG_ERROR("password", "Password change failed: Invalid current password for user: " + username);
        return result;
    }

    // Validate new password strength
    std::string strength_error;
    if (!validatePasswordStrength(new_password, strength_error)) {
        result.message = strength_error;
        result.error_code = "WEAK_PASSWORD";
        ENDPOINT_LOG_ERROR("password", "Password change failed: " + strength_error);
        return result;
    }

    // Check if new password is same as current password
    if (new_password == current_password) {
        result.message = "New password must be different from current password";
        result.error_code = "SAME_PASSWORD";
        ENDPOINT_LOG_ERROR("password", "Password change failed: New password same as current");
        return result;
    }

    // Perform password update using credential manager
    try {
        // Create a temporary authentication to verify current credentials
        auto auth_result = credential_manager_->authenticate(username, current_password);
        
        if (!auth_result.success) {
            result.message = "Authentication failed";
            result.error_code = "AUTH_FAILED";
            ENDPOINT_LOG_ERROR("password", "Password change failed: Authentication failed for user: " + username);
            return result;
        }

        // Update password in credential manager
        bool update_success = credential_manager_->updatePassword(username, new_password);
        
        if (update_success) {
            result.success = true;
            result.message = "Password changed successfully";
            result.error_code = "";
        } else {
            result.message = "Failed to update password";
            result.error_code = "UPDATE_FAILED";
            ENDPOINT_LOG_ERROR("password", "Failed to update password in credential manager");
            return result;
        }

        ENDPOINT_LOG_INFO("password", "Password changed successfully for user: " + username);

        // Invalidate current session if session token is provided
        if (!session_token.empty()) {
            credential_manager_->invalidateSession(session_token);
            ENDPOINT_LOG_INFO("password", "Session invalidated after password change");
        }

    } catch (const std::exception& e) {
        result.message = "Internal error during password change";
        result.error_code = "INTERNAL_ERROR";
        ENDPOINT_LOG_ERROR("password", "Password change error: " + std::string(e.what()));
    }

    return result;
}

bool PasswordManager::validatePasswordStrength(const std::string& password, std::string& error_message) {
    if (!hasMinimumLength(password)) {
        error_message = "Password must be at least 8 characters long";
        return false;
    }

    if (!hasUppercase(password)) {
        error_message = "Password must contain at least one uppercase letter";
        return false;
    }

    if (!hasLowercase(password)) {
        error_message = "Password must contain at least one lowercase letter";
        return false;
    }

    if (!hasDigit(password)) {
        error_message = "Password must contain at least one digit";
        return false;
    }

    if (!hasSpecialChar(password)) {
        error_message = "Password must contain at least one special character (!@#$%^&*)";
        return false;
    }

    if (!isNotCommonPassword(password)) {
        error_message = "Password is too common, please choose a more secure password";
        return false;
    }

    return true;
}

bool PasswordManager::validateCurrentPassword(const std::string& username, const std::string& current_password) {
    try {
        auto auth_result = credential_manager_->authenticate(username, current_password);
        return auth_result.success;
    } catch (const std::exception& e) {
        ENDPOINT_LOG_ERROR("password", "Error validating current password: " + std::string(e.what()));
        return false;
    }
}

bool PasswordManager::hasMinimumLength(const std::string& password) {
    return password.length() >= 8;
}

bool PasswordManager::hasUppercase(const std::string& password) {
    return std::any_of(password.begin(), password.end(), [](char c) {
        return std::isupper(c);
    });
}

bool PasswordManager::hasLowercase(const std::string& password) {
    return std::any_of(password.begin(), password.end(), [](char c) {
        return std::islower(c);
    });
}

bool PasswordManager::hasDigit(const std::string& password) {
    return std::any_of(password.begin(), password.end(), [](char c) {
        return std::isdigit(c);
    });
}

bool PasswordManager::hasSpecialChar(const std::string& password) {
    std::string special_chars = "!@#$%^&*()_+-=[]{}|;:,.<>?";
    return std::any_of(password.begin(), password.end(), [&special_chars](char c) {
        return special_chars.find(c) != std::string::npos;
    });
}

bool PasswordManager::isNotCommonPassword(const std::string& password) {
    // List of common passwords to reject
    std::set<std::string> common_passwords = {
        "password", "123456", "12345678", "qwerty", "abc123",
        "password123", "admin", "letmein", "welcome", "monkey",
        "1234567890", "dragon", "master", "login", "password1"
    };

    std::string lower_password = password;
    std::transform(lower_password.begin(), lower_password.end(), lower_password.begin(), ::tolower);

    return common_passwords.find(lower_password) == common_passwords.end();
}
