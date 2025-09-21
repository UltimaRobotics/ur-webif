// Session Integration for WebApp
// This script extends the WebApp class with session management

// Add session event listeners to WebApp prototype
WebApp.prototype.setupSessionEventListeners = function() {
    // Listen for session state changes
    window.addEventListener('sessionStateChange', (event) => {
        console.log('[APP] Session state changed:', event.detail);
        
        if (event.detail.state === 'logged_in') {
            // User logged in successfully
            this.showNotification('Session active', 'success');
        } else if (event.detail.state === 'session_expired') {
            // Session expired
            this.showNotification('Session expired, please log in again', 'warning');
        }
    });

    // Listen for storage changes (session changes from other tabs)
    window.addEventListener('storage', (e) => {
        if (e.key === 'ur_webif_login_state' && e.newValue === 'logged_out') {
            this.showNotification('You have been logged out from another tab', 'warning');
        }
    });
};

// Initialize session state management on page load
document.addEventListener('DOMContentLoaded', async function() {
    console.log('[SESSION-INTEGRATION] Setting up session management');
    
    // Check for existing login session on page load
    if (window.sessionStateManager) {
        // Longer delay to ensure all scripts are loaded and session is properly saved
        setTimeout(async () => {
            try {
                const shouldRedirect = await window.sessionStateManager.checkAndRedirect();
                if (shouldRedirect) {
                    console.log('[SESSION-INTEGRATION] Redirecting based on session state');
                }
            } catch (error) {
                console.error('[SESSION-INTEGRATION] Error during session check:', error);
            }
        }, 500); // Increased delay for better reliability
    }
});

// Add logout functionality
WebApp.prototype.logout = function() {
    console.log('[APP] Logging out user');
    if (window.sessionStateManager) {
        window.sessionStateManager.logout();
    } else {
        // Fallback logout
        localStorage.clear();
        window.location.reload();
    }
};