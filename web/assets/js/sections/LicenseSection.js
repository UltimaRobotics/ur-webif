
class LicenseSection extends BaseSectionManager {
    constructor() {
        super('LICENSE');
        this.autoRefreshInterval = 60000; // 60 seconds
        this.init();
    }

    init() {
        this.log('Initializing License Section...');
        this.startAutoRefresh(this.autoRefreshInterval);
        this.refreshData();
    }

    async refreshData() {
        if (this.isRefreshing) {
            return;
        }

        this.isRefreshing = true;
        this.log('Auto-refreshing license data...');

        try {
            await this.fetchLicenseData();
        } catch (error) {
            this.error('Failed to refresh license data:', error);
        } finally {
            this.isRefreshing = false;
        }
    }

    async fetchLicenseData() {
        this.log('Fetching license data...');
        this.log('Making GET request to /license/status');

        try {
            const data = await this.httpManager.get('/license/status');
            this.log('Response received:', data);
            
            if (data && data.success) {
                this.log('License data received:', data.license);
                this.setState({ 
                    license: data.license,
                    timestamp: data.timestamp
                });
                this.updateLicenseDisplay(data.license);
            } else {
                this.error('Invalid license response');
            }
        } catch (error) {
            this.error('License data fetch failed:', error);
        }
    }

    updateLicenseDisplay(licenseData) {
        // Update license info
        this.updateElement('license-status', licenseData.status || 'Unknown');
        this.updateElement('license-type', licenseData.type || 'Unknown');
        this.updateElement('license-holder', licenseData.license_holder || 'Unknown');
        this.updateElement('license-organization', licenseData.organization || 'Unknown');
        this.updateElement('license-serial', licenseData.serial_number || 'Unknown');
        
        // Update dates
        this.updateElement('license-issued-date', this.formatTimestamp(licenseData.issued_date || 0));
        this.updateElement('license-expiry-date', this.formatTimestamp(licenseData.expiry_date || 0));
        this.updateElement('license-last-validated', this.formatTimestamp(licenseData.last_validated || 0));
        
        // Update features
        if (licenseData.features && Array.isArray(licenseData.features)) {
            const featuresContainer = document.getElementById('license-features');
            if (featuresContainer) {
                featuresContainer.innerHTML = licenseData.features.map(feature => 
                    `<span class="feature-tag">${feature}</span>`
                ).join('');
            }
        }
        
        // Update usage limits
        this.updateElement('license-devices-limit', licenseData.device_limit || 'Unlimited');
        this.updateElement('license-devices-current', licenseData.devices_registered || 0);
        this.updateElement('license-bandwidth-limit', licenseData.bandwidth_limit_mbps ? 
            `${licenseData.bandwidth_limit_mbps} Mbps` : 'Unlimited');
        
        // Update status indicators
        const isValid = licenseData.is_valid || false;
        const isExpired = licenseData.is_expired || false;
        
        this.updateStatusIndicator('license-validity-indicator', isValid && !isExpired);
        
        // Update warnings
        this.updateLicenseWarnings(licenseData);
    }

    updateStatusIndicator(id, isActive) {
        const element = document.getElementById(id);
        if (element) {
            element.className = isActive ? 'status-valid' : 'status-invalid';
        }
    }

    updateLicenseWarnings(licenseData) {
        const warningsContainer = document.getElementById('license-warnings');
        if (!warningsContainer) return;

        const warnings = [];
        
        if (licenseData.is_expired) {
            warnings.push('License has expired');
        } else if (licenseData.expires_soon) {
            warnings.push('License expires soon');
        }
        
        if (licenseData.device_limit && licenseData.devices_registered >= licenseData.device_limit) {
            warnings.push('Device limit reached');
        }
        
        if (warnings.length > 0) {
            warningsContainer.innerHTML = warnings.map(warning => 
                `<div class="warning-item">${warning}</div>`
            ).join('');
            warningsContainer.style.display = 'block';
        } else {
            warningsContainer.style.display = 'none';
        }
    }

    onStateChange(changes) {
        if (changes.license) {
            this.log('License state changed:', changes.license);
        }
        if (changes.timestamp) {
            this.log('State changed:', { timestamp: changes.timestamp });
        }
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    if (window.location.pathname.includes('license-section.html')) {
        window.licenseSection = new LicenseSection();
    }
});
