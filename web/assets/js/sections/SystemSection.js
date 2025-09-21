
class SystemSection extends BaseSectionManager {
    constructor() {
        super('SYSTEM');
        this.autoRefreshInterval = 30000; // 30 seconds
        this.systemData = null;
        this.init();
    }

    init() {
        this.log('Initializing System Section...');
        this.startAutoRefresh(this.autoRefreshInterval);
        this.refreshData();
    }

    async refreshData() {
        if (this.isRefreshing) {
            return;
        }

        this.isRefreshing = true;
        this.log('Auto-refreshing system dashboard...');

        try {
            await this.fetchDashboardData();
        } catch (error) {
            this.error('Failed to refresh system data:', error);
        } finally {
            this.isRefreshing = false;
        }
    }

    async fetchDashboardData() {
        this.log('Fetching dashboard data...');
        this.log('Making POST request to /dashboard-data');
        this.log('Request data:', {});

        try {
            const data = await this.httpManager.post('/dashboard-data', {});
            this.log('Response received:', Object.keys(data));
            
            if (data && data.success) {
                this.log('Dashboard data received:', Object.keys(data));
                this.systemData = data;
                this.setState({ 
                    system: data.system,
                    network: data.network,
                    cellular: data.cellular,
                    performance: data.performance,
                    custom_metrics: data.custom_metrics,
                    data_freshness: data.data_freshness
                });
                this.updateSystemDisplay(data);
            } else {
                this.error('Invalid dashboard response');
            }
        } catch (error) {
            this.error('Dashboard data fetch failed:', error);
        }
    }

    updateSystemDisplay(data) {
        // Update system info
        if (data.system) {
            this.updateElement('system-hostname', data.system.hostname || 'Unknown');
            this.updateElement('system-uptime', this.formatUptime(data.system.uptime_seconds || 0));
            this.updateElement('system-load', data.system.load_average_1m || '0.00');
            this.updateElement('system-temperature', `${data.system.cpu_temperature_celsius || 0}°C`);
        }

        // Update performance metrics
        if (data.performance) {
            this.updateElement('cpu-usage', `${data.performance.cpu_usage_percent || 0}%`);
            this.updateElement('memory-usage', `${data.performance.memory_usage_percent || 0}%`);
            this.updateElement('disk-usage', `${data.performance.disk_usage_percent || 0}%`);
            this.updateElement('memory-available', this.formatBytes(data.performance.memory_available_bytes || 0));
            this.updateElement('disk-available', this.formatBytes(data.performance.disk_available_bytes || 0));
        }

        // Update network info
        if (data.network) {
            this.updateElement('network-interface', data.network.interface_name || 'Unknown');
            this.updateElement('network-ip', data.network.local_ip || 'Unknown');
            this.updateElement('network-external-ip', data.network.external_ip || 'Unknown');
            this.updateElement('network-gateway', data.network.gateway_ip || 'Unknown');
            this.updateElement('network-dns-primary', data.network.dns_primary || 'Unknown');
            this.updateElement('network-connection-type', data.network.connection_type || 'Unknown');
            this.updateElement('network-speed', data.network.connection_speed || 'Unknown');
        }

        // Update cellular info (integrated here instead of separate section)
        if (data.cellular) {
            this.updateElement('cellular-status', data.cellular.connection_status || 'Unknown');
            this.updateElement('cellular-network', data.cellular.network_name || 'Unknown');
            this.updateElement('cellular-signal', `${data.cellular.signal_bars || 0}/5`);
            this.updateElement('cellular-technology', data.cellular.technology || 'Unknown');
            this.updateElement('cellular-data-usage', this.formatBytes(data.cellular.data_usage_mb * 1024 * 1024 || 0));
        }
    }

    onStateChange(changes) {
        // Log specific changes
        if (changes.system) {
            this.log('System state changed:', changes.system);
        }
        if (changes.network) {
            this.log('Network state changed:', changes.network);
        }
        if (changes.cellular) {
            this.log('Cellular state changed:', changes.cellular);
        }
        if (changes.data_freshness) {
            this.log('State changed:', { data_freshness: changes.data_freshness });
        }
    }

    getSystemData() {
        return this.systemData;
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    if (window.location.pathname.includes('source.html') || 
        window.location.pathname.includes('landing-section.html')) {
        window.systemSection = new SystemSection();
    }
});
