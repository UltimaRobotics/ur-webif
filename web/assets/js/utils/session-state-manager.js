class SessionStateManager {
    constructor() {
        this.sessionToken = null;
        this.userInfo = null;
        this.isAuthenticated = false;
        this.username = null; // Added for username

        console.log('[SESSION] SessionStateManager initialized');
        this.init();
    }

    init() {
        this.loadSession();
        this.setupSessionChecking();
    }

    loadSession() {
        const token = localStorage.getItem('session_token');
        const userInfo = localStorage.getItem('user_info');
        const username = localStorage.getItem('username'); // Load username

        if (token) {
            this.sessionToken = token;
            this.isAuthenticated = true;

            if (userInfo) {
                try {
                    this.userInfo = JSON.parse(userInfo);
                } catch (e) {
                    console.warn('[SESSION] Failed to parse user info');
                }
            }

            if (username) {
                this.username = username; // Set username
            }

            console.log('[SESSION] Session loaded from storage');
        }
    }

    saveSession(token, userInfo = null, username = null) { // Added username parameter
        this.sessionToken = token;
        this.userInfo = userInfo;
        this.username = username; // Save username
        this.isAuthenticated = true;

        localStorage.setItem('session_token', token);
        if (userInfo) {
            localStorage.setItem('user_info', JSON.stringify(userInfo));
        }
        if (username) {
            localStorage.setItem('username', username); // Save username to local storage
        }

        console.log('[SESSION] Session saved to storage');
    }

    clearSession() {
        this.sessionToken = null;
        this.userInfo = null;
        this.username = null; // Clear username
        this.isAuthenticated = false;

        localStorage.removeItem('session_token');
        localStorage.removeItem('user_info');
        localStorage.removeItem('jwt_token');
        localStorage.removeItem('username'); // Remove username from local storage

        console.log('[SESSION] Session cleared');
    }

    async checkAndRedirect() {
        const currentPath = window.location.pathname;
        const isLoginPage = currentPath.includes('index.html') || currentPath === '/' || currentPath === '';
        const isDashboardPage = currentPath.includes('source.html') || currentPath.includes('components/');

        if (isLoginPage && this.isAuthenticated) {
            // User is on login page but authenticated - redirect to dashboard
            console.log('[SESSION] Authenticated user on login page, redirecting to dashboard');
            this.redirectToDashboard();
            return true;
        } else if (isDashboardPage && !this.isAuthenticated) {
            // User is on dashboard but not authenticated - redirect to login
            console.log('[SESSION] Unauthenticated user on dashboard, redirecting to login');
            this.redirectToLogin();
            return true;
        }

        return false;
    }

    redirectToDashboard() {
        // Prevent infinite redirect loops
        if (!window.location.pathname.includes('source.html')) {
            console.log('[SESSION] Redirecting to dashboard');
            window.location.href = '/components/source.html';
        }
    }

    redirectToLogin() {
        this.clearSession();

        // Prevent infinite redirect loops
        if (!window.location.pathname.includes('index.html') && 
            window.location.pathname !== '/' && 
            window.location.pathname !== '') {
            console.log('[SESSION] Redirecting to login');
            window.location.href = '/index.html';
        }
    }

    logout() {
        console.log('[SESSION] Logging out user');
        this.clearSession();
        this.redirectToLogin();
    }

    setupSessionChecking() {
        // Check session validity periodically
        setInterval(() => {
            if (this.isAuthenticated && this.sessionToken) {
                this.validateSession();
            }
        }, 300000); // Check every 5 minutes
    }

    async validateSession() {
        if (!this.sessionToken || !window.app?.apiUrl) {
            return false;
        }

        try {
            const response = await fetch(`${window.app.apiUrl}/validate-session`, {
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
                    return true;
                }
            }

            console.log('[SESSION] Session validation failed');
            this.logout();
            return false;
        } catch (error) {
            console.error('[SESSION] Session validation error:', error);
            return false;
        }
    }

    getSessionToken() {
        return this.sessionToken;
    }

    getUsername() {
        return this.username || localStorage.getItem('username') || 'admin';
    }
}

// Initialize session manager
let sessionStateManager;

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        sessionStateManager = new SessionStateManager();
        window.sessionStateManager = sessionStateManager;
    });
} else {
    sessionStateManager = new SessionStateManager();
    window.sessionStateManager = sessionStateManager;
}