// Main Application Manager
class WebApp {
    constructor() {
        this.apiUrl = null;
        this.sessionToken = null;
        this.currentUser = null;
        this.isInitialized = false;

        console.log('[APP] Initializing WebApp...');
        this.init();
    }

    async init() {
        try {
            await this.loadConfig();
            this.checkAuthentication();
            this.setupGlobalErrorHandling();
            this.initNavigation(); // Initialize navigation handlers
            this.initSectionManagers(); // Initialize section managers
            this.isInitialized = true;
            console.log('[APP] WebApp initialized successfully');
        } catch (error) {
            console.error('[APP] Failed to initialize:', error);
        }
    }

    async loadConfig() {
        try {
            const response = await fetch('/config.json');
            const config = await response.json();
            this.apiUrl = config.api_base_url || `${window.location.protocol}//${window.location.host}/api`;
            console.log('[APP] API URL loaded:', this.apiUrl);
        } catch (error) {
            console.warn('[APP] Config load failed, using default:', error);
            this.apiUrl = `${window.location.protocol}//${window.location.host}/api`;
        }
    }

    checkAuthentication() {
        // Only redirect if we're on a page that requires authentication
        if (window.location.pathname.includes('source.html') || 
            window.location.pathname.includes('components/')) {

            const token = localStorage.getItem('session_token');
            if (!token) {
                console.log('[APP] No session token found, redirecting to login');
                this.redirectToLogin();
                return;
            }

            this.sessionToken = token;
            this.validateSession();
        }
    }

    async validateSession() {
        if (!this.sessionToken) {
            this.redirectToLogin();
            return;
        }

        try {
            const response = await fetch(`${this.apiUrl}/validate-session`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Authorization': `Bearer ${this.sessionToken}`
                },
                body: JSON.stringify({ session_token: this.sessionToken })
            });

            if (response.ok) {
                const result = await response.json();
                if (result.success) {
                    console.log('[APP] Session validated successfully');
                    this.currentUser = result.user;
                    return;
                }
            }

            console.log('[APP] Session validation failed, redirecting to login');
            this.redirectToLogin();
        } catch (error) {
            console.error('[APP] Session validation error:', error);
            this.redirectToLogin();
        }
    }

    redirectToLogin() {
        localStorage.removeItem('session_token');
        localStorage.removeItem('jwt_token');

        // Prevent infinite redirect loops
        if (!window.location.pathname.includes('index.html') && 
            window.location.pathname !== '/' && 
            window.location.pathname !== '') {
            console.log('[APP] Redirecting to login page');
            window.location.href = '/index.html';
        }
    }

    setupGlobalErrorHandling() {
        window.addEventListener('error', (event) => {
            console.error('[APP] Global error:', event.error);
        });

        window.addEventListener('unhandledrejection', (event) => {
            console.error('[APP] Unhandled promise rejection:', event.reason);
        });
    }

    logout() {
        console.log('[APP] Logging out user');
        localStorage.removeItem('session_token');
        localStorage.removeItem('jwt_token');
        localStorage.clear();
        this.redirectToLogin();
    }

    // Provide missing functionality that was in popup-manager.js and component-globals.js
    showNotification(message, type = 'info') {
        console.log(`[APP] Notification (${type}): ${message}`);

        // Create notification element if it doesn't exist
        let notification = document.getElementById('app-notification');
        if (!notification) {
            notification = document.createElement('div');
            notification.id = 'app-notification';
            notification.style.cssText = `
                position: fixed;
                top: 20px;
                right: 20px;
                padding: 12px 20px;
                border-radius: 6px;
                color: white;
                z-index: 1000;
                opacity: 0;
                transition: opacity 0.3s ease;
                max-width: 300px;
            `;
            document.body.appendChild(notification);
        }

        // Set notification style based on type
        const styles = {
            success: 'background-color: #10b981;',
            error: 'background-color: #ef4444;',
            warning: 'background-color: #f59e0b;',
            info: 'background-color: #3b82f6;'
        };

        notification.style.cssText += styles[type] || styles.info;
        notification.textContent = message;
        notification.style.opacity = '1';

        // Auto-hide after 4 seconds
        setTimeout(() => {
            notification.style.opacity = '0';
        }, 4000);
    }

    // HTTP request wrapper
    async makeRequest(endpoint, options = {}) {
        const url = endpoint.startsWith('http') ? endpoint : `${this.apiUrl}${endpoint}`;

        const defaultHeaders = {
            'Content-Type': 'application/json',
            'Accept': 'application/json'
        };

        if (this.sessionToken) {
            defaultHeaders['Authorization'] = `Bearer ${this.sessionToken}`;
        }

        const requestOptions = {
            method: options.method || 'GET',
            headers: { ...defaultHeaders, ...options.headers },
            ...options
        };

        if (requestOptions.body && typeof requestOptions.body === 'object') {
            requestOptions.body = JSON.stringify(requestOptions.body);
        }

        try {
            const response = await fetch(url, requestOptions);
            const data = await response.json();

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${data.message || response.statusText}`);
            }

            return data;
        } catch (error) {
            console.error(`[APP] Request failed for ${endpoint}:`, error);
            throw error;
        }
    }

    // Utility functions
    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    formatUptime(seconds) {
        const days = Math.floor(seconds / 86400);
        const hours = Math.floor((seconds % 86400) / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);

        if (days > 0) return `${days}d ${hours}h ${minutes}m`;
        if (hours > 0) return `${hours}h ${minutes}m`;
        return `${minutes}m`;
    }

    formatTimestamp(timestamp) {
        return new Date(timestamp * 1000).toLocaleString();
    }

    // Section loading and management
    loadSection(sectionId) {
        console.log(`[APP] Loading section: ${sectionId}`);

        // Hide all sections
        document.querySelectorAll('.section').forEach(section => {
            section.style.display = 'none';
        });

        // Show the target section
        const targetSection = document.getElementById(sectionId);
        if (targetSection) {
            targetSection.style.display = 'block';

            // Initialize the section manager if it exists
            if (this[`${sectionId.replace('-section', '')}Section`]) {
                this[`${sectionId.replace('-section', '')}Section`].initialize();
            }
        } else {
            console.error(`[APP] Section not found: ${sectionId}`);
        }
    }

    initNavigation() {
        // Add event listeners for navigation
        document.getElementById('nav-system-dashboard').addEventListener('click', () => {
            console.log('[APP] System Dashboard clicked');
            this.loadSection('landing-section');
        });

        document.getElementById('nav-cellular-config').addEventListener('click', () => {
            console.log('[APP] Cellular Config clicked');
            this.loadSection('cellular-config-section');
        });

        // Add backup and restore navigation
        document.getElementById('nav-backup-restore').addEventListener('click', () => {
            console.log('[APP] Backup and Restore clicked');
            this.loadSection('backup-restore-section');
        });
    }

    initSectionManagers() {
        // Initialize section managers
        this.systemSection = new SystemSection();
        this.cellularSection = new CellularSection();
        this.licenseSection = new LicenseSection();
        this.vpnSection = new VpnSection();
        this.wirelessSection = new WirelessSection();
        this.backupSection = new BackupSection();
    }
}

// Initialize the app
let app;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        app = new WebApp();
        window.app = app;
    });
} else {
    app = new WebApp();
    window.app = app;
}

// Export for use by other modules
window.WebApp = WebApp;

// Dummy section classes for demonstration purposes.
// In a real application, these would be in separate files and imported.
class SystemSection {
    initialize() { console.log('[SystemSection] Initialized'); }
    // ... other methods
}

class CellularSection {
    initialize() { console.log('[CellularSection] Initialized'); }
    // ... other methods
}

class LicenseSection {
    initialize() { console.log('[LicenseSection] Initialized'); }
    // ... other methods
}

class VpnSection {
    initialize() { console.log('[VpnSection] Initialized'); }
    // ... other methods
}

class WirelessSection {
    initialize() { console.log('[WirelessSection] Initialized'); }
    // ... other methods
}

// Note: BackupSection is now defined in sections/BackupSection.js
// This section handles compatibility with the new BackupSection implementation

class LegacyBackupSection {
    constructor() {
        console.log('[LegacyBackupSection] Constructed for compatibility');
    }

    initialize() {
        console.log('[LegacyBackupSection] Delegating to new BackupSection...');
        
        // Check if the new BackupSection is available
        if (window.BackupSection) {
            window.backupSectionInstance = new window.BackupSection();
        } else {
            console.warn('[LegacyBackupSection] New BackupSection not found, using legacy implementation');
            this.setupEventListeners();
            this.loadBackupData();
        }
    }

    setupEventListeners() {
        const backupButton = document.getElementById('trigger-backup');
        if (backupButton) {
            backupButton.addEventListener('click', () => this.triggerBackup());
        }

        const restoreButton = document.getElementById('trigger-restore');
        if (restoreButton) {
            restoreButton.addEventListener('click', () => this.triggerRestore());
        }

        // Event listener for file input for restore
        const restoreFileInput = document.getElementById('restore-file-input');
        if (restoreFileInput) {
            restoreFileInput.addEventListener('change', (event) => this.handleRestoreFileSelect(event));
        }
    }

    async loadBackupData() {
        console.log('[LegacyBackupSection] Loading backup data...');
        try {
            const data = await app.makeRequest('/api/backup/list');
            this.renderStatus(data);
        } catch (error) {
            if (app && app.showNotification) {
                app.showNotification('Failed to load backup status.', 'error');
            }
            console.error('[LegacyBackupSection] Error loading backup data:', error);
        }
    }

    renderStatus(data) {
        console.log('[BackupSection] Rendering status:', data);
        const statusDiv = document.getElementById('backup-status');
        if (statusDiv) {
            statusDiv.innerHTML = `
                <p>Last Backup: ${data.lastBackup ? app.formatTimestamp(data.lastBackup) : 'Never'}</p>
                <p>Backup Size: ${data.backupSize ? app.formatBytes(data.backupSize) : 'N/A'}</p>
                <p>Status: ${data.status || 'Unknown'}</p>
            `;
        }
    }

    async triggerBackup() {
        console.log('[BackupSection] Triggering backup...');
        app.showNotification('Backup process initiated. Please wait.', 'info');
        try {
            const result = await app.makeRequest('/api/backup/start', { method: 'POST' });
            app.showNotification('Backup started successfully!', 'success');
            this.loadBackupData(); // Refresh status
        } catch (error) {
            app.showNotification(`Backup failed: ${error.message}`, 'error');
            console.error('[BackupSection] Error triggering backup:', error);
        }
    }

    async triggerRestore() {
        console.log('[BackupSection] Triggering restore...');
        const restoreFileInput = document.getElementById('restore-file-input');
        if (!restoreFileInput || restoreFileInput.files.length === 0) {
            app.showNotification('Please select a backup file to restore.', 'warning');
            return;
        }

        const file = restoreFileInput.files[0];
        const formData = new FormData();
        formData.append('backupFile', file);

        app.showNotification('Restore process initiated. This may take a while...', 'info');
        try {
            const result = await app.makeRequest('/api/backup/restore', {
                method: 'POST',
                body: formData,
                headers: {
                    // Content-Type is automatically set by fetch for FormData
                    // 'Content-Type': 'multipart/form-data'
                }
            });
            app.showNotification('Restore process completed successfully!', 'success');
            this.loadBackupData(); // Refresh status
            restoreFileInput.value = ''; // Clear the file input
        } catch (error) {
            app.showNotification(`Restore failed: ${error.message}`, 'error');
            console.error('[BackupSection] Error triggering restore:', error);
        }
    }

    handleRestoreFileSelect(event) {
        const files = event.target.files;
        if (files.length > 0) {
            const fileName = files[0].name;
            console.log(`[BackupSection] Selected file for restore: ${fileName}`);
            // Optionally, display the selected file name to the user
            const selectedFileDisplay = document.getElementById('selected-restore-file');
            if (selectedFileDisplay) {
                selectedFileDisplay.textContent = `Selected: ${fileName}`;
            }
        }
    }
}