
class HttpRequestManager {
    constructor(apiUrl, sessionToken = null) {
        this.apiUrl = apiUrl || (window.app?.apiUrl || `${window.location.protocol}//${window.location.host}/api`);
        this.sessionToken = sessionToken || (window.app?.sessionToken || localStorage.getItem('session_token'));
        this.timeout = 10000;
        this.retryAttempts = 3;
        
        console.log('[HTTP] HttpRequestManager initialized with API URL:', this.apiUrl);
    }

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
            timeout: options.timeout || this.timeout,
            ...options
        };

        if (requestOptions.body && typeof requestOptions.body === 'object') {
            requestOptions.body = JSON.stringify(requestOptions.body);
        }

        console.log(`[HTTP] Making ${requestOptions.method} request to ${endpoint}`);

        try {
            const controller = new AbortController();
            const timeoutId = setTimeout(() => controller.abort(), requestOptions.timeout);

            const response = await fetch(url, {
                ...requestOptions,
                signal: controller.signal
            });

            clearTimeout(timeoutId);

            let data;
            try {
                data = await response.json();
            } catch (e) {
                data = { message: 'Invalid JSON response' };
            }

            console.log(`[HTTP] Response received:`, data);

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${data.message || response.statusText}`);
            }
            
            return data;
        } catch (error) {
            console.error(`[HTTP] Request failed for ${endpoint}:`, error);
            
            if (error.name === 'AbortError') {
                throw new Error('Request timed out');
            }
            
            throw error;
        }
    }

    async get(endpoint, headers = {}) {
        return this.makeRequest(endpoint, { method: 'GET', headers });
    }

    async post(endpoint, data, headers = {}) {
        return this.makeRequest(endpoint, { method: 'POST', body: data, headers });
    }

    async put(endpoint, data, headers = {}) {
        return this.makeRequest(endpoint, { method: 'PUT', body: data, headers });
    }

    async delete(endpoint, headers = {}) {
        return this.makeRequest(endpoint, { method: 'DELETE', headers });
    }

    updateSessionToken(token) {
        this.sessionToken = token;
        console.log('[HTTP] Session token updated');
    }
}

// Make it globally available
window.HttpRequestManager = HttpRequestManager;
