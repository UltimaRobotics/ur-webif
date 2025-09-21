
class WirelessSection extends BaseSectionManager {
    constructor() {
        super('WIRELESS');
        this.autoRefreshInterval = 30000; // 30 seconds
        this.init();
    }

    init() {
        this.log('Initializing Wireless Section...');
        this.startAutoRefresh(this.autoRefreshInterval);
        this.refreshData();
    }

    async refreshData() {
        if (this.isRefreshing) {
            return;
        }

        this.isRefreshing = true;
        this.log('Auto-refreshing wireless status...');

        try {
            await this.fetchWirelessStatus();
        } catch (error) {
            this.error('Failed to refresh wireless data:', error);
        } finally {
            this.isRefreshing = false;
        }
    }

    async fetchWirelessStatus() {
        this.log('Fetching wireless status...');
        this.log('Making GET request to /wireless/status');

        try {
            const data = await this.httpManager.get('/wireless/status');
            this.log('Response received:', data);
            
            if (data && data.success) {
                this.log('Wireless status received:', data);
                this.setState({ 
                    timestamp: data.timestamp,
                    ...data
                });
                this.updateWirelessDisplay(data);
            } else {
                this.error('Invalid wireless status response');
            }
        } catch (error) {
            this.error('Wireless status fetch failed:', error);
        }
    }

    updateWirelessDisplay(data) {
        // Update connection info
        this.updateElement('wireless-connection-status', data.connection_status || 'Unknown');
        this.updateElement('wireless-current-ssid', data.current_ssid || 'Not Connected');
        this.updateElement('wireless-active-mode', data.active_mode || 'Unknown');
        this.updateElement('wireless-interface-name', data.interface_name || 'Unknown');
        this.updateElement('wireless-signal-strength', `${data.signal_strength || 0} dBm`);
        
        // Update status indicators
        const isConnected = data.connection_status === 'connected';
        const radioEnabled = data.wifi_radio_enabled || false;
        const driverLoaded = data.driver_loaded || false;
        const hardwareAvailable = data.hardware_available || false;
        const apActive = data.ap_active || false;
        
        this.updateStatusIndicator('wireless-connection-indicator', isConnected);
        this.updateStatusIndicator('wireless-radio-indicator', radioEnabled);
        this.updateStatusIndicator('wireless-driver-indicator', driverLoaded);
        this.updateStatusIndicator('wireless-hardware-indicator', hardwareAvailable);
        this.updateStatusIndicator('wireless-ap-indicator', apActive);
        
        // Update toggles
        this.updateToggle('wireless-radio-enabled', radioEnabled);
        this.updateToggle('wireless-ap-active', apActive);
        
        // Update scan status
        const scanInProgress = data.scan_in_progress || false;
        this.updateScanStatus(scanInProgress);
        
        // Update signal strength bar
        this.updateSignalStrengthBar(data.signal_strength || 0);
    }

    updateStatusIndicator(id, isActive) {
        const element = document.getElementById(id);
        if (element) {
            element.className = isActive ? 'status-active' : 'status-inactive';
        }
    }

    updateScanStatus(inProgress) {
        const scanButton = document.getElementById('wireless-scan-btn');
        if (scanButton) {
            scanButton.disabled = inProgress;
            scanButton.textContent = inProgress ? 'Scanning...' : 'Scan Networks';
        }
    }

    updateSignalStrengthBar(strength) {
        const signalBar = document.getElementById('wireless-signal-bar');
        if (signalBar) {
            const percentage = Math.max(0, Math.min(100, (strength + 100) * 2)); // Convert dBm to percentage
            signalBar.style.width = `${percentage}%`;
            
            // Color coding for signal strength
            if (percentage > 80) {
                signalBar.className = 'signal-bar signal-excellent';
            } else if (percentage > 60) {
                signalBar.className = 'signal-bar signal-good';
            } else if (percentage > 40) {
                signalBar.className = 'signal-bar signal-fair';
            } else {
                signalBar.className = 'signal-bar signal-poor';
            }
        }
    }

    onStateChange(changes) {
        if (changes.timestamp) {
            this.log('State changed:', { timestamp: changes.timestamp });
        }
        // Add other specific change handlers as needed
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    if (window.location.pathname.includes('wireless-config-section.html')) {
        window.wirelessSection = new WirelessSection();
    }
});
