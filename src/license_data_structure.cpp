#include "license_data_structure.h"
#include <sstream>
#include <algorithm>

// LicenseDisplayData Implementation

LicenseDisplayData::LicenseDisplayData(const json& j) {
    fromJson(j);
}

std::string LicenseDisplayData::getStatusColor() const {
    if (isExpired()) return "red";
    if (remaining_days_ <= 30) return "yellow";
    if (isActive()) return "green";
    return "gray";
}

std::string LicenseDisplayData::getFormattedExpiryDate() const {
    return LicenseHelpers::formatDate(expiry_date_);
}

std::string LicenseDisplayData::getFormattedRemainingDays() const {
    return LicenseHelpers::formatDaysRemaining(remaining_days_);
}

json LicenseDisplayData::toJson() const {
    json j;
    j["license_type"] = license_type_;
    j["license_id"] = license_id_;
    j["activation_status"] = activation_status_;
    j["start_date"] = start_date_;
    j["expiry_date"] = expiry_date_;
    j["remaining_days"] = remaining_days_;
    j["health_score"] = health_score_;
    j["enabled_features"] = enabled_features_;
    return j;
}

void LicenseDisplayData::fromJson(const json& j) {
    if (j.contains("license_type")) license_type_ = j["license_type"];
    if (j.contains("license_id")) license_id_ = j["license_id"];
    if (j.contains("activation_status")) activation_status_ = j["activation_status"];
    if (j.contains("start_date")) start_date_ = j["start_date"];
    if (j.contains("expiry_date")) expiry_date_ = j["expiry_date"];
    if (j.contains("remaining_days")) remaining_days_ = j["remaining_days"];
    if (j.contains("health_score")) health_score_ = j["health_score"];
    if (j.contains("enabled_features")) enabled_features_ = j["enabled_features"];
}

json LicenseDisplayData::toDisplayJson() const {
    json j;
    j["license_type"] = license_type_;
    j["license_id"] = LicenseHelpers::maskLicenseId(license_id_);
    j["status"] = activation_status_;
    j["expiry_date"] = getFormattedExpiryDate();
    j["start_date"] = getFormattedExpiryDate();
    j["remaining_days"] = remaining_days_;
    j["remaining_days_formatted"] = getFormattedRemainingDays();
    j["health_score"] = health_score_;
    j["status_color"] = getStatusColor();
    j["enabled_features"] = enabled_features_;
    j["feature_count"] = enabled_features_.size();
    j["is_active"] = isActive();
    j["is_expired"] = isExpired();
    return j;
}

LicenseDisplayData LicenseDisplayData::createMockData() {
    LicenseDisplayData data;
    data.setLicenseType("Enterprise Subscription");
    data.setLicenseId("ENTR-2024-PROD-A7B9");
    data.setActivationStatus("Active");
    data.setStartDate("2025-03-15");
    data.setExpiryDate("2026-03-15");
    data.setRemainingDays(247);
    data.setHealthScore(98);
    data.setEnabledFeatures(LicenseHelpers::getDefaultFeatures());
    return data;
}

LicenseDisplayData LicenseDisplayData::createDefault() {
    LicenseDisplayData data;
    data.setLicenseType("Unknown");
    data.setLicenseId("XXXX-XXXX-XXXX-XXXX");
    data.setActivationStatus("Unknown");
    data.setStartDate("Unknown");
    data.setExpiryDate("Unknown");
    data.setRemainingDays(0);
    data.setHealthScore(0);
    data.setEnabledFeatures({});
    return data;
}

// Global JSON serialization functions
void to_json(json& j, const LicenseDisplayData& license) {
    j = license.toJson();
}

void from_json(const json& j, LicenseDisplayData& license) {
    license.fromJson(j);
}

// Helper functions implementation
namespace LicenseHelpers {

    std::string maskLicenseId(const std::string& license_id) {
        if (license_id.length() < 8) {
            return "••••-••••-••••-XXXX";
        }

        std::string masked = "••••-••••-••••-" + license_id.substr(license_id.length() - 4);
        return masked;
    }

    std::string formatDate(const std::string& iso_date) {
        if (iso_date.empty()) return "Unknown";

        // Convert "2025-03-15" to "March 15, 2025"
        if (iso_date.length() >= 10) {
            std::string year = iso_date.substr(0, 4);
            std::string month = iso_date.substr(5, 2);
            std::string day = iso_date.substr(8, 2);

            std::vector<std::string> months = {
                "", "January", "February", "March", "April", "May", "June",
                "July", "August", "September", "October", "November", "December"
            };

            int month_num = std::stoi(month);
            if (month_num >= 1 && month_num <= 12) {
                return months[month_num] + " " + std::to_string(std::stoi(day)) + ", " + year;
            }
        }

        return iso_date;
    }

    std::string formatDaysRemaining(int days) {
        if (days <= 0) return "Expired";
        if (days == 1) return "1 day left";
        return std::to_string(days) + " days left";
    }

    std::vector<std::string> getDefaultFeatures() {
        return {
            "5G Modem Support",
            "Advanced VPN",
            "Enterprise Firewall",
            "Mesh Networking",
            "Load Balancing",
            "Traffic Analytics",
            "API Access",
            "Priority Support"
        };
    }
}