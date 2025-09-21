// Configuration Manager for UR WebIF API
class ConfigManager {
    constructor() {
        this.config = null;
        this.environment = this.detectEnvironment();
    }

    async loadConfig() {
        try {
            // Add cache busting to ensure fresh config
            const cacheBuster = Date.now();
            const response = await fetch(`config.json?v=${cacheBuster}`);
            if (!response.ok) {
                throw new Error(`Failed to load config: ${response.status}`);
            }
            this.config = await response.json();
            console.log('Configuration loaded successfully');
            console.log('Loaded config:', this.config);
            return this.config;
        } catch (error) {
            console.warn('Failed to load config.json, using defaults:', error);
            return this.getDefaultConfig();
        }
    }

    detectEnvironment() {
        const hostname = window.location.hostname;
        const port = window.location.port;
        const protocol = window.location.protocol;

        if (hostname.includes('replit.dev') || hostname.includes('replit.co')) {
            return 'replit';
        } else if (port === '5000' || hostname === 'localhost' || hostname.startsWith('192.168.') || hostname.startsWith('10.') || hostname.startsWith('127.0.0.1')) {
            return 'embedded';
        } else {
            return 'local';
        }
    }

    getDefaultConfig() {
        return {
            connection: {
                api_endpoint: '/api',
                websocket: {
                    enabled: false,
                    url: 'ws://127.0.0.1:8081/',
                    connection_timeout: 3000,
                    retry_interval: 5000,
                    ping_interval: 30000,
                    max_retries: 3
                }
            },
            ui: {
                show_connection_status: true,
                auto_reconnect: true,
                debug_mode: false
            },
            environment: {
                auto_detect: true,
                override: null
            }
        };
    }

    getHttpApiUrl() {
        if (!this.config) {
            console.warn('Config not loaded, using default');
            return this.generateHttpApiUrl();
        }

        // Return the HTTP API base URL - use relative path to avoid CORS issues
        const apiEndpoint = this.config.connection.api_endpoint || '/api';
        return apiEndpoint; // Use relative URL for same-origin requests
    }

    getWebSocketUrl() {
        if (!this.config) {
            console.warn('Config not loaded, using default');
            return this.generateWebSocketUrl();
        }

        // If URL is set to a dynamic placeholder or default localhost, generate the correct URL
        const configUrl = this.config.connection.websocket.url;
        if (!configUrl || configUrl === 'auto' || configUrl.includes('127.0.0.1') || configUrl.includes('localhost')) {
            return this.generateWebSocketUrl();
        }

        return configUrl;
    }

    getApiEndpoint() {
        return this.config?.connection?.api_endpoint || '/api';
    }

    getConnectionTimeout() {
        return this.config?.connection?.websocket?.connection_timeout || 3000;
    }

    getRetryInterval() {
        return this.config?.connection?.websocket?.retry_interval || 5000;
    }

    getPingInterval() {
        return this.config?.connection?.websocket?.ping_interval || 30000;
    }

    getMaxRetries() {
        return this.config?.connection?.websocket?.max_retries || 3;
    }

    isWebSocketEnabled() {
        return this.config?.connection?.websocket?.enabled !== false;
    }

    isAutoReconnectEnabled() {
        return this.config?.ui?.auto_reconnect !== false;
    }

    showConnectionStatus() {
        return this.config?.ui?.show_connection_status !== false;
    }

    isDebugMode() {
        return this.config?.ui?.debug_mode === true;
    }

    generateHttpApiUrl() {
        // Use relative URL to avoid CORS and certificate issues
        const apiUrl = '/api';
        
        console.log('Generated HTTP API URL:', apiUrl);
        return apiUrl;
    }

    generateWebSocketUrl() {
        const hostname = window.location.hostname;
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        
        // Handle different environments
        let wsUrl;
        if (hostname.includes('replit.dev') || hostname.includes('replit.co') || hostname.includes('replit.app')) {
            // Replit environment - use same hostname with WebSocket port
            wsUrl = `${protocol}//${hostname}:8081/`;
        } else if (hostname === 'localhost' || hostname.startsWith('127.') || hostname.startsWith('192.168.') || hostname.startsWith('10.')) {
            // Local development - use same hostname with WebSocket port
            wsUrl = `${protocol}//${hostname}:8081/`;
        } else {
            // Default case - use current hostname with WebSocket port
            wsUrl = `${protocol}//${hostname}:8081/`;
        }
        
        console.log('Generated WebSocket URL:', wsUrl);
        console.log('Current hostname:', hostname);
        console.log('Current protocol:', window.location.protocol);
        return wsUrl;
    }

    log(...args) {
        if (this.isDebugMode()) {
            console.log('[CONFIG]', ...args);
        }
    }
}

// Global config manager instance
window.configManager = new ConfigManager();