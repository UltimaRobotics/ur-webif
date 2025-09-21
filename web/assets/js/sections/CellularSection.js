
class CellularSection {
    constructor() {
        this.httpManager = new HttpRequestManager();
        this.autoRefreshInterval = 30000; // 30 seconds
        this.isRefreshing = false;
        this.init();
    }

    init() {
        console.log('[CELLULAR] Initializing Cellular Section...');
        this.attachEventHandlers();
        this.refreshData();
        this.startAutoRefresh();
    }

    attachEventHandlers() {
        // Connect button
        const connectBtn = document.getElementById('cellular-connect-btn');
        if (connectBtn) {
            connectBtn.addEventListener('click', () => this.connectCellular());
        }

        // Disconnect button
        const disconnectBtn = document.getElementById('cellular-disconnect-btn');
        if (disconnectBtn) {
            disconnectBtn.addEventListener('click', () => this.disconnectCellular());
        }

        // Scan networks button
        const scanBtn = document.getElementById('cellular-scan-btn');
        if (scanBtn) {
            scanBtn.addEventListener('click', () => this.scanNetworks());
        }

        // Refresh status button
        const refreshBtn = document.getElementById('cellular-refresh-status-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.refreshData());
        }

        // Configuration form
        const configForm = document.getElementById('cellular-config-form');
        if (configForm) {
            configForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.saveConfiguration();
            });
        }
    }

    startAutoRefresh() {
        setInterval(() => {
            this.refreshData();
        }, this.autoRefreshInterval);
    }

    async refreshData() {
        if (this.isRefreshing) {
            return;
        }

        this.isRefreshing = true;
        console.log('[CELLULAR] Auto-refreshing cellular status...');

        try {
            await this.fetchCellularStatus();
        } catch (error) {
            console.error('[CELLULAR] Failed to refresh cellular data:', error);
        } finally {
            this.isRefreshing = false;
        }
    }

    async fetchCellularStatus() {
        try {
            console.log('[CELLULAR] Making request to /dashboard-data');
            const data = await this.httpManager.post('/dashboard-data', {});
            
            if (data && data.cellular) {
                console.log('[CELLULAR] Received cellular data:', data.cellular);
                this.updateCellularDisplay(data.cellular);
                return data.cellular;
            } else {
                console.warn('[CELLULAR] No cellular data in response');
            }
        } catch (error) {
            console.error('[CELLULAR] Fetch error:', error);
            this.showError('Failed to fetch cellular status: ' + error.message);
        }
    }

    updateCellularDisplay(data) {
        console.log('[CELLULAR] Updating display with data:', data);
        
        // Update connection info
        this.updateElement('cellular-connection-status', data.connection_status || 'Disconnected');
        this.updateElement('cellular-detailed-status', data.connection_status || 'Disconnected');
        this.updateElement('cellular-network-name', data.network_name || 'Not Connected');
        this.updateElement('cellular-technology', data.technology || 'N/A');
        this.updateElement('cellular-band', data.band || 'N/A');
        this.updateElement('cellular-cell-id', data.cell_id || 'Unknown');
        this.updateElement('cellular-apn-display', data.apn || 'Not Set');
        
        // Update signal info
        this.updateElement('cellular-signal-strength', `${data.signal_bars || 0}/5 bars`);
        this.updateElement('cellular-signal-status', data.signal_status || 'No Signal');
        this.updateElement('cellular-rssi', `${data.rssi_dbm || 0} dBm`);
        this.updateElement('cellular-rsrp', `${data.rsrp_dbm || 0} dBm`);
        this.updateElement('cellular-rsrq', `${data.rsrq_db || 0} dB`);
        this.updateElement('cellular-sinr', `${data.sinr_db || 0} dB`);
        
        // Update connection details
        this.updateElement('cellular-data-usage', `${data.data_usage_mb || 0} MB`);
        
        // Update connection indicator
        const isConnected = data.is_connected || false;
        const connectionIndicator = document.getElementById('cellular-connection-indicator');
        if (connectionIndicator) {
            connectionIndicator.className = `w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`;
        }
        
        // Update signal bars
        this.updateSignalBars(data.signal_bars || 0);
        
        // Update button states
        this.updateButtonStates(isConnected);
        
        // Update last updated time
        this.updateElement('cellular-last-updated', new Date().toLocaleTimeString());
    }

    updateElement(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
        }
    }
    
    updateSignalBars(signalBars) {
        const barsContainer = document.getElementById('cellular-signal-bars');
        if (barsContainer) {
            const bars = barsContainer.querySelectorAll('.signal-bar');
            bars.forEach((bar, index) => {
                if (index < signalBars) {
                    bar.className = 'w-2 h-6 bg-green-500 rounded-sm signal-bar';
                } else {
                    bar.className = 'w-2 h-6 bg-neutral-300 rounded-sm signal-bar';
                }
            });
        }
    }
    
    updateButtonStates(isConnected) {
        const connectBtn = document.getElementById('cellular-connect-btn');
        const disconnectBtn = document.getElementById('cellular-disconnect-btn');
        
        if (connectBtn) {
            connectBtn.disabled = isConnected;
        }
        if (disconnectBtn) {
            disconnectBtn.disabled = !isConnected;
        }
    }
    
    async connectCellular() {
        try {
            console.log('[CELLULAR] Connecting...');
            this.showStatus('Connecting cellular...');
            
            const result = await this.httpManager.post('/cellular/connect', {
                action: 'connect'
            });
            
            if (result.success) {
                this.showSuccess('Cellular connection initiated');
                setTimeout(() => this.refreshData(), 2000);
            } else {
                this.showError('Failed to connect: ' + (result.message || 'Unknown error'));
            }
        } catch (error) {
            console.error('[CELLULAR] Connect error:', error);
            this.showError('Connection failed: ' + error.message);
        }
    }
    
    async disconnectCellular() {
        try {
            console.log('[CELLULAR] Disconnecting...');
            this.showStatus('Disconnecting cellular...');
            
            const result = await this.httpManager.post('/cellular/disconnect', {
                action: 'disconnect'
            });
            
            if (result.success) {
                this.showSuccess('Cellular disconnected');
                setTimeout(() => this.refreshData(), 1000);
            } else {
                this.showError('Failed to disconnect: ' + (result.message || 'Unknown error'));
            }
        } catch (error) {
            console.error('[CELLULAR] Disconnect error:', error);
            this.showError('Disconnect failed: ' + error.message);
        }
    }
    
    async scanNetworks() {
        try {
            console.log('[CELLULAR] Scanning networks...');
            this.showStatus('Scanning for networks...');
            
            const scanBtn = document.getElementById('cellular-scan-btn');
            if (scanBtn) {
                scanBtn.disabled = true;
                scanBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Scanning...';
            }
            
            const result = await this.httpManager.post('/cellular/scan', {
                action: 'scan_networks'
            });
            
            if (result.success) {
                this.showSuccess('Network scan completed');
                if (result.networks) {
                    this.updateAvailableNetworks(result.networks);
                }
            } else {
                this.showError('Scan failed: ' + (result.message || 'Unknown error'));
            }
        } catch (error) {
            console.error('[CELLULAR] Scan error:', error);
            this.showError('Network scan failed: ' + error.message);
        } finally {
            const scanBtn = document.getElementById('cellular-scan-btn');
            if (scanBtn) {
                scanBtn.disabled = false;
                scanBtn.innerHTML = '<i class="fa-solid fa-search mr-2"></i>Scan Networks';
            }
        }
    }
    
    async saveConfiguration() {
        try {
            console.log('[CELLULAR] Saving configuration...');
            this.showStatus('Saving configuration...');
            
            const config = {
                apn: document.getElementById('cellular-apn')?.value || '',
                username: document.getElementById('cellular-username')?.value || '',
                password: document.getElementById('cellular-password')?.value || '',
                auth_type: document.getElementById('auth-type')?.value || 'none',
                network_type: document.getElementById('cellular-network-type')?.value || 'auto',
                auto_connect: document.getElementById('cellular-auto-connect')?.checked || false,
                data_roaming: document.getElementById('cellular-data-roaming')?.checked || false,
                data_limit_enabled: document.getElementById('cellular-data-limit-enabled')?.checked || false,
                data_limit_mb: parseInt(document.getElementById('cellular-data-limit')?.value) || 0
            };
            
            const result = await this.httpManager.post('/cellular/configure', {
                action: 'save_config',
                config: config
            });
            
            if (result.success) {
                this.showSuccess('Configuration saved successfully');
            } else {
                this.showError('Failed to save: ' + (result.message || 'Unknown error'));
            }
        } catch (error) {
            console.error('[CELLULAR] Config save error:', error);
            this.showError('Save failed: ' + error.message);
        }
    }

    updateAvailableNetworks(networks) {
        const container = document.getElementById('cellular-available-networks');
        if (!container || !networks) return;
        
        if (networks.length === 0) {
            container.innerHTML = '<div class="text-xs text-neutral-500 p-3">No networks found</div>';
            return;
        }
        
        const networksHtml = networks.map(network => `
            <div class="text-xs p-2 border border-neutral-200 rounded mb-1">
                <div class="font-medium">${network.operator_name || 'Unknown'}</div>
                <div class="text-neutral-600">${network.technology || 'N/A'} • Signal: ${network.signal_strength || 0} dBm</div>
            </div>
        `).join('');
        
        container.innerHTML = networksHtml;
    }
    
    showStatus(message) {
        console.log('[CELLULAR] Status:', message);
        // You can add a status indicator here if needed
    }
    
    showSuccess(message) {
        console.log('[CELLULAR] Success:', message);
        const successDiv = document.getElementById('cellular-success-message');
        if (successDiv) {
            successDiv.style.display = 'block';
            document.getElementById('cellular-success-text').textContent = message;
            setTimeout(() => {
                successDiv.style.display = 'none';
            }, 5000);
        }
    }
    
    showError(message) {
        console.error('[CELLULAR] Error:', message);
        const errorDiv = document.getElementById('cellular-error-message');
        if (errorDiv) {
            errorDiv.style.display = 'block';
            document.getElementById('cellular-error-text').textContent = message;
            setTimeout(() => {
                errorDiv.style.display = 'none';
            }, 8000);
        }
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    console.log('[CELLULAR] DOM Content Loaded, initializing...');
    try {
        window.cellularSection = new CellularSection();
        console.log('[CELLULAR] Section initialized successfully');
    } catch (error) {
        console.error('[CELLULAR] Failed to initialize section:', error);
    }
});
