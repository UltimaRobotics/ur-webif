#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

/**
 * Minimalistic license data structure for UI display only
 */
class LicenseDisplayData {
private:
    // Basic license information
    std::string license_type_;
    std::string license_id_;
    std::string activation_status_;

    // Validity information
    std::string start_date_;
    std::string expiry_date_;
    int remaining_days_;

    // Health and features
    int health_score_;
    std::vector<std::string> enabled_features_;

public:
    // Constructors
    LicenseDisplayData() = default;
    LicenseDisplayData(const json& j);

    // Getters
    const std::string& getLicenseType() const { return license_type_; }
    const std::string& getLicenseId() const { return license_id_; }
    const std::string& getActivationStatus() const { return activation_status_; }
    const std::string& getStartDate() const { return start_date_; }
    const std::string& getExpiryDate() const { return expiry_date_; }
    int getRemainingDays() const { return remaining_days_; }
    int getHealthScore() const { return health_score_; }
    const std::vector<std::string>& getEnabledFeatures() const { return enabled_features_; }

    // Setters
    void setLicenseType(const std::string& type) { license_type_ = type; }
    void setLicenseId(const std::string& id) { license_id_ = id; }
    void setActivationStatus(const std::string& status) { activation_status_ = status; }
    void setStartDate(const std::string& date) { start_date_ = date; }
    void setExpiryDate(const std::string& date) { expiry_date_ = date; }
    void setRemainingDays(int days) { remaining_days_ = days; }
    void setHealthScore(int score) { health_score_ = score; }
    void setEnabledFeatures(const std::vector<std::string>& features) { enabled_features_ = features; }

    // Status methods
    bool isActive() const { return activation_status_ == "Active"; }
    bool isExpired() const { return remaining_days_ <= 0; }
    std::string getStatusColor() const;
    std::string getFormattedExpiryDate() const;
    std::string getFormattedRemainingDays() const;

    // JSON serialization
    json toJson() const;
    void fromJson(const json& j);
    json toDisplayJson() const;

    // Static factory methods
    static LicenseDisplayData createMockData();
    static LicenseDisplayData createDefault();
};

// JSON serialization functions
void to_json(json& j, const LicenseDisplayData& license);
void from_json(const json& j, LicenseDisplayData& license);

// Helper functions
namespace LicenseHelpers {
    std::string maskLicenseId(const std::string& license_id);
    std::string formatDate(const std::string& iso_date);
    std::string formatDaysRemaining(int days);
    std::vector<std::string> getDefaultFeatures();
}