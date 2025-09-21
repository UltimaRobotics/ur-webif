
class BaseSectionManager {
    constructor(sectionName) {
        this.sectionName = sectionName;
        this.httpManager = new HttpRequestManager();
        this.isRefreshing = false;
        this.refreshInterval = null;
        this.currentState = {};
        this.previousState = {};
        
        console.log(`[${this.sectionName.toUpperCase()}] Section manager initialized`);
    }

    log(message, data = '') {
        console.log(`[${this.sectionName.toUpperCase()}] ${message}`, data);
    }

    error(message, error = '') {
        console.error(`[${this.sectionName.toUpperCase()}] ${message}`, error);
    }

    showNotification(message, type = 'info') {
        if (window.app && window.app.showNotification) {
            window.app.showNotification(message, type);
        } else {
            console.log(`[${this.sectionName.toUpperCase()}] ${type.toUpperCase()}: ${message}`);
        }
    }

    updateElement(id, content) {
        const element = document.getElementById(id);
        if (element) {
            if (typeof content === 'object') {
                element.textContent = JSON.stringify(content);
            } else {
                element.textContent = content;
            }
        }
    }

    updateElementHTML(id, html) {
        const element = document.getElementById(id);
        if (element) {
            element.innerHTML = html;
        }
    }

    addClass(id, className) {
        const element = document.getElementById(id);
        if (element) {
            element.classList.add(className);
        }
    }

    removeClass(id, className) {
        const element = document.getElementById(id);
        if (element) {
            element.classList.remove(className);
        }
    }

    toggleClass(id, className) {
        const element = document.getElementById(id);
        if (element) {
            element.classList.toggle(className);
        }
    }

    setVisibility(id, visible) {
        const element = document.getElementById(id);
        if (element) {
            element.style.display = visible ? 'block' : 'none';
        }
    }

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

    startAutoRefresh(interval = 30000) {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
        }

        this.refreshInterval = setInterval(() => {
            if (!this.isRefreshing) {
                this.refreshData();
            }
        }, interval);

        this.log(`Auto-refresh started with ${interval}ms interval`);
    }

    stopAutoRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
            this.log('Auto-refresh stopped');
        }
    }

    async refreshData() {
        // Override in subclasses
        this.log('Base refresh method called - should be overridden');
    }

    setState(newState) {
        this.previousState = { ...this.currentState };
        this.currentState = { ...this.currentState, ...newState };
        
        // Find what changed
        const changes = {};
        for (const key in newState) {
            if (JSON.stringify(this.previousState[key]) !== JSON.stringify(newState[key])) {
                changes[key] = {
                    old: this.previousState[key],
                    new: newState[key]
                };
            }
        }

        if (Object.keys(changes).length > 0) {
            this.log('State changed:', changes);
            this.onStateChange(changes);
        }
    }

    onStateChange(changes) {
        // Override in subclasses
    }

    destroy() {
        this.stopAutoRefresh();
        this.log('Section manager destroyed');
    }
}

// Make it globally available
window.BaseSectionManager = BaseSectionManager;
