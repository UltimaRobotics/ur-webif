
/**
 * Advanced Network Section Manager
 * Redesigned with cellular-inspired HTTP connection architecture
 * Handles VLAN, NAT, Firewall, Static Routes, and Bridge configurations
 */
class AdvancedNetworkSection {
    constructor() {
        console.log('[ADVANCED-NETWORK] Initializing AdvancedNetworkSection...');
        
        // Configuration
        this.sectionName = 'advanced-network';
        this.config = {
            autoRefreshInterval: 30000,
            enableAutoRefresh: true,
            retryAttempts: 3,
            retryDelay: 1000
        };

        // API endpoints
        this.endpoints = {
            vlans: '/api/advanced-network/vlans',
            natRules: '/api/advanced-network/nat-rules',
            firewallRules: '/api/advanced-network/firewall-rules',
            staticRoutes: '/api/advanced-network/static-routes',
            bridges: '/api/advanced-network/bridges'
        };

        // State management
        this.state = {
            vlans: [],
            natRules: [],
            firewallRules: [],
            staticRoutes: [],
            bridges: [],
            loading: false,
            lastUpdated: null
        };

        // Request state management
        this.requestState = {
            pendingRequests: new Map(),
            retryAttempts: new Map()
        };

        // Auto-refresh timer
        this.autoRefreshTimer = null;

        this.init();
    }

    /**
     * Initialize the section
     */
    async init() {
        try {
            console.log('[ADVANCED-NETWORK] Initializing section...');

            if (document.readyState === 'loading') {
                document.addEventListener('DOMContentLoaded', () => this.setupEventHandlers());
            } else {
                this.setupEventHandlers();
            }

            await this.loadAllData();
            this.startAutoRefresh();

            console.log('[ADVANCED-NETWORK] Section initialized successfully');
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to initialize:', error);
            this.showErrorMessage('Failed to initialize Advanced Network section');
        }
    }

    /**
     * Setup event handlers
     */
    setupEventHandlers() {
        this.setupAddButtons();
        this.setupTableActions();
        this.setupPopupHandlers();
        this.setupSectionToggles();
    }

    setupAddButtons() {
        // Use event delegation to handle dynamically loaded buttons
        document.addEventListener('click', (e) => {
            if (e.target.matches('#add-vlan-btn, .add-vlan-btn')) {
                e.preventDefault();
                console.log('[ADVANCED-NETWORK] Add VLAN button clicked');
                this.showVlanPopup();
            }
            
            if (e.target.matches('#add-nat-rule-btn, .add-nat-rule-btn')) {
                e.preventDefault();
                console.log('[ADVANCED-NETWORK] Add NAT rule button clicked');
                this.showNatRulePopup();
            }
            
            if (e.target.matches('#add-firewall-rule-btn, .add-firewall-rule-btn')) {
                e.preventDefault();
                console.log('[ADVANCED-NETWORK] Add firewall rule button clicked');
                this.showFirewallRulePopup();
            }
            
            if (e.target.matches('#add-static-route-btn, .add-static-route-btn')) {
                e.preventDefault();
                console.log('[ADVANCED-NETWORK] Add static route button clicked');
                this.showStaticRoutePopup();
            }
            
            if (e.target.matches('#create-bridge-btn, .create-bridge-btn')) {
                e.preventDefault();
                console.log('[ADVANCED-NETWORK] Create bridge button clicked');
                this.showBridgePopup();
            }
        });

        // Direct binding for immediate elements
        setTimeout(() => this.bindDirectButtons(), 500);
    }

    bindDirectButtons() {
        const buttons = [
            { id: 'add-vlan-btn', handler: () => this.showVlanPopup() },
            { id: 'add-nat-rule-btn', handler: () => this.showNatRulePopup() },
            { id: 'add-firewall-rule-btn', handler: () => this.showFirewallRulePopup() },
            { id: 'add-static-route-btn', handler: () => this.showStaticRoutePopup() },
            { id: 'create-bridge-btn', handler: () => this.showBridgePopup() }
        ];

        buttons.forEach(({ id, handler }) => {
            const btn = document.getElementById(id);
            if (btn && !btn.hasAttribute('data-bound')) {
                btn.setAttribute('data-bound', 'true');
                btn.addEventListener('click', (e) => {
                    e.preventDefault();
                    handler();
                });
            }
        });
    }

    setupTableActions() {
        document.addEventListener('click', async (e) => {
            const target = e.target;
            
            if (target.classList.contains('fa-edit')) {
                const row = target.closest('tr');
                const section = target.closest('[id$="-section"]');
                await this.handleEdit(row, section);
            }
            
            if (target.classList.contains('fa-trash')) {
                const row = target.closest('tr');
                const section = target.closest('[id$="-section"]');
                await this.handleDelete(row, section);
            }
            
            if (target.classList.contains('fa-power-off')) {
                const row = target.closest('tr');
                const section = target.closest('[id$="-section"]');
                await this.handleToggle(row, section);
            }
        });
    }

    setupPopupHandlers() {
        document.addEventListener('click', (e) => {
            if (e.target.classList.contains('popup-close') || 
                e.target.classList.contains('popup-cancel') ||
                e.target.closest('.popup-close') ||
                e.target.closest('.popup-cancel')) {
                this.closeAllPopups();
            }
        });

        document.addEventListener('click', async (e) => {
            if (e.target.classList.contains('popup-submit') || 
                e.target.closest('.popup-submit')) {
                await this.handlePopupSubmit(e.target);
            }
        });

        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') {
                this.closeAllPopups();
            }
        });
    }

    setupSectionToggles() {
        const toggleButtons = document.querySelectorAll('[id$="-section"] .fa-chevron-down');
        toggleButtons.forEach(button => {
            button.addEventListener('click', (e) => {
                const section = e.target.closest('[id$="-section"]');
                const content = section.querySelector('.p-6');
                const chevron = e.target;
                
                if (content.style.display === 'none') {
                    content.style.display = 'block';
                    chevron.style.transform = 'rotate(0deg)';
                } else {
                    content.style.display = 'none';
                    chevron.style.transform = 'rotate(-90deg)';
                }
            });
        });
    }

    /**
     * Auto-refresh functionality
     */
    startAutoRefresh() {
        if (this.config.enableAutoRefresh && !this.autoRefreshTimer) {
            this.autoRefreshTimer = setInterval(() => {
                this.performAutoRefresh();
            }, this.config.autoRefreshInterval);
            console.log('[ADVANCED-NETWORK] Auto-refresh started');
        }
    }

    stopAutoRefresh() {
        if (this.autoRefreshTimer) {
            clearInterval(this.autoRefreshTimer);
            this.autoRefreshTimer = null;
            console.log('[ADVANCED-NETWORK] Auto-refresh stopped');
        }
    }

    async performAutoRefresh() {
        console.log('[ADVANCED-NETWORK] Auto-refreshing data...');
        await this.loadAllData();
    }

    /**
     * Data loading methods
     */
    async loadAllData() {
        if (this.state.loading) return;

        this.state.loading = true;
        console.log('[ADVANCED-NETWORK] Loading all data...');

        try {
            const loadPromises = [
                this.loadVlans(),
                this.loadNatRules(),
                this.loadFirewallRules(),
                this.loadStaticRoutes(),
                this.loadBridges()
            ];

            await Promise.allSettled(loadPromises);
            this.state.lastUpdated = new Date();
            this.showSuccessMessage('Advanced network data loaded successfully');
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to load data:', error);
            this.showErrorMessage('Failed to load advanced network data');
        } finally {
            this.state.loading = false;
        }
    }

    async loadVlans() {
        try {
            const response = await this.makeRequest('GET', this.endpoints.vlans);
            if (response && response.success) {
                this.state.vlans = response.vlans || [];
                this.renderVlans();
                console.log('[ADVANCED-NETWORK] VLANs loaded:', this.state.vlans.length);
            } else {
                throw new Error(response?.message || 'Failed to load VLANs');
            }
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to load VLANs:', error);
            this.state.vlans = [];
            this.renderVlans();
        }
    }

    async loadNatRules() {
        try {
            const response = await this.makeRequest('GET', this.endpoints.natRules);
            if (response && response.success) {
                this.state.natRules = response.nat_rules || [];
                this.renderNatRules();
                console.log('[ADVANCED-NETWORK] NAT rules loaded:', this.state.natRules.length);
            } else {
                throw new Error(response?.message || 'Failed to load NAT rules');
            }
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to load NAT rules:', error);
            this.state.natRules = [];
            this.renderNatRules();
        }
    }

    async loadFirewallRules() {
        try {
            const response = await this.makeRequest('GET', this.endpoints.firewallRules);
            if (response && response.success) {
                this.state.firewallRules = response.firewall_rules || [];
                this.renderFirewallRules();
                console.log('[ADVANCED-NETWORK] Firewall rules loaded:', this.state.firewallRules.length);
            } else {
                throw new Error(response?.message || 'Failed to load firewall rules');
            }
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to load firewall rules:', error);
            this.state.firewallRules = [];
            this.renderFirewallRules();
        }
    }

    async loadStaticRoutes() {
        try {
            const response = await this.makeRequest('GET', this.endpoints.staticRoutes);
            if (response && response.success) {
                this.state.staticRoutes = response.static_routes || [];
                this.renderStaticRoutes();
                console.log('[ADVANCED-NETWORK] Static routes loaded:', this.state.staticRoutes.length);
            } else {
                throw new Error(response?.message || 'Failed to load static routes');
            }
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to load static routes:', error);
            this.state.staticRoutes = [];
            this.renderStaticRoutes();
        }
    }

    async loadBridges() {
        try {
            const response = await this.makeRequest('GET', this.endpoints.bridges);
            if (response && response.success) {
                this.state.bridges = response.bridges || [];
                this.renderBridges();
                console.log('[ADVANCED-NETWORK] Bridges loaded:', this.state.bridges.length);
            } else {
                throw new Error(response?.message || 'Failed to load bridges');
            }
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to load bridges:', error);
            this.state.bridges = [];
            this.renderBridges();
        }
    }

    /**
     * Enhanced HTTP request method with retry logic
     */
    async makeRequest(method, endpoint, data = null, options = {}) {
        const requestId = `${method}-${endpoint}-${Date.now()}`;
        const maxRetries = options.maxRetries || this.config.retryAttempts;
        
        // Check if there's already a pending request for this endpoint
        if (this.requestState.pendingRequests.has(endpoint)) {
            console.log('[ADVANCED-NETWORK] Request already pending for:', endpoint);
            return this.requestState.pendingRequests.get(endpoint);
        }

        const requestPromise = this.executeRequestWithRetry(method, endpoint, data, maxRetries, requestId);
        this.requestState.pendingRequests.set(endpoint, requestPromise);

        try {
            const result = await requestPromise;
            return result;
        } finally {
            this.requestState.pendingRequests.delete(endpoint);
            this.requestState.retryAttempts.delete(requestId);
        }
    }

    async executeRequestWithRetry(method, endpoint, data, maxRetries, requestId) {
        let lastError;

        for (let attempt = 1; attempt <= maxRetries; attempt++) {
            try {
                console.log(`[ADVANCED-NETWORK] Making ${method} request to: ${endpoint} (attempt ${attempt}/${maxRetries})`);

                const options = {
                    method: method,
                    headers: {
                        'Content-Type': 'application/json',
                    }
                };

                if (data && (method === 'POST' || method === 'PUT')) {
                    const processedData = this.processFormData(data);
                    options.body = JSON.stringify(processedData);
                    console.log(`[ADVANCED-NETWORK] Request body:`, processedData);
                }

                const response = await fetch(endpoint, options);
                console.log(`[ADVANCED-NETWORK] Response status: ${response.status}`);

                if (!response.ok) {
                    const errorText = await response.text();
                    throw new Error(`HTTP ${response.status}: ${errorText}`);
                }

                const responseData = await response.json();
                console.log(`[ADVANCED-NETWORK] Response data:`, responseData);

                // Reset retry counter on success
                this.requestState.retryAttempts.delete(requestId);
                return responseData;

            } catch (error) {
                lastError = error;
                console.error(`[ADVANCED-NETWORK] Request failed (attempt ${attempt}/${maxRetries}):`, error);

                if (attempt < maxRetries) {
                    const delay = this.config.retryDelay * attempt;
                    console.log(`[ADVANCED-NETWORK] Retrying in ${delay}ms...`);
                    await this.sleep(delay);
                } else {
                    console.error(`[ADVANCED-NETWORK] All retry attempts failed for ${endpoint}`);
                }
            }
        }

        // All retries failed
        this.showErrorMessage(`Request failed after ${maxRetries} attempts: ${lastError.message}`);
        throw lastError;
    }

    /**
     * Process form data before sending
     */
    processFormData(data) {
        const processed = { ...data };
        
        // Convert string arrays to actual arrays
        if (processed.assigned_ports && typeof processed.assigned_ports === 'string') {
            processed.assigned_ports = processed.assigned_ports.split(',').map(port => port.trim()).filter(port => port);
        }

        // Process bridge member interfaces
        if (processed.custom_interfaces && typeof processed.custom_interfaces === 'string') {
            processed.member_interfaces = processed.custom_interfaces.split(',').map(iface => iface.trim()).filter(iface => iface);
            delete processed.custom_interfaces;
        } else {
            // Collect checked interfaces for bridge
            const interfaces = [];
            Object.keys(processed).forEach(key => {
                if (key.startsWith('interface_') && processed[key]) {
                    interfaces.push(key.replace('interface_', ''));
                    delete processed[key];
                }
            });
            if (interfaces.length > 0) {
                processed.member_interfaces = interfaces;
            }
        }

        // Convert boolean strings to actual booleans
        ['enabled', 'logging', 'persistent', 'stp_enabled', 'auto_start', 'multicast_snooping'].forEach(field => {
            if (processed[field] !== undefined) {
                processed[field] = processed[field] === 'on' || processed[field] === true || processed[field] === 'true';
            }
        });

        // Convert numeric strings to numbers
        ['vlan_id', 'priority', 'metric', 'hello_time', 'max_age', 'forward_delay', 'aging_time', 'hash_max', 'table_id'].forEach(field => {
            if (processed[field] && processed[field] !== '') {
                const num = parseInt(processed[field], 10);
                if (!isNaN(num)) {
                    processed[field] = num;
                }
            }
        });

        return processed;
    }

    /**
     * Security and Validation Utility Methods
     */
    sleep(ms) {
        return new Promise(resolve => setTimeout(resolve, ms));
    }

    log(message) {
        console.log(`[ADVANCED-NETWORK] ${message}`);
    }

    logError(message, error) {
        console.error(`[ADVANCED-NETWORK] ${message}`, error);
    }

    /**
     * Sanitize text content to prevent XSS attacks
     */
    sanitizeText(text) {
        if (typeof text !== 'string') return String(text || '');
        return text
            .replace(/&/g, '&amp;')
            .replace(/</g, '&lt;')
            .replace(/>/g, '&gt;')
            .replace(/"/g, '&quot;')
            .replace(/'/g, '&#x27;')
            .replace(/\//g, '&#x2F;');
    }

    /**
     * Validate IP address format
     */
    validateIP(ip) {
        const ipRegex = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;
        return ipRegex.test(ip);
    }

    /**
     * Validate CIDR notation
     */
    validateCIDR(cidr) {
        const cidrRegex = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\/(3[0-2]|[12]?[0-9])$/;
        return cidrRegex.test(cidr) || this.validateIP(cidr);
    }

    /**
     * Validate port number or range
     */
    validatePort(port) {
        if (!port || port.trim() === '') return true; // Allow empty for "any"
        
        // Check for port range (e.g., "80-90")
        if (port.includes('-')) {
            const [start, end] = port.split('-').map(p => parseInt(p.trim()));
            return start >= 1 && start <= 65535 && end >= 1 && end <= 65535 && start <= end;
        }
        
        // Check for comma-separated ports (e.g., "80,443,8080")
        if (port.includes(',')) {
            return port.split(',').every(p => {
                const portNum = parseInt(p.trim());
                return portNum >= 1 && portNum <= 65535;
            });
        }
        
        // Single port
        const portNum = parseInt(port);
        return portNum >= 1 && portNum <= 65535;
    }

    /**
     * Validate network interface name
     */
    validateInterface(iface) {
        const ifaceRegex = /^[a-zA-Z][a-zA-Z0-9._-]{0,14}$/;
        return ifaceRegex.test(iface);
    }

    /**
     * Create safe DOM element with text content
     */
    createElement(tag, className = '', textContent = '') {
        const element = document.createElement(tag);
        if (className) element.className = className;
        if (textContent) element.textContent = this.sanitizeText(textContent);
        return element;
    }

    /**
     * Create safe span element with styling
     */
    createSpan(className, textContent) {
        return this.createElement('span', className, textContent);
    }

    /**
     * UI feedback methods
     */
    showSuccessMessage(message) {
        this.showNotification(message, 'success');
    }

    showErrorMessage(message) {
        this.showNotification(message, 'error');
    }

    showNotification(message, type = 'info') {
        const notification = document.createElement('div');
        notification.className = `fixed top-4 right-4 px-4 py-2 rounded-lg text-white z-50 ${
            type === 'success' ? 'bg-green-500' : 
            type === 'error' ? 'bg-red-500' : 'bg-blue-500'
        } shadow-lg transform transition-all duration-300 translate-x-full`;
        notification.textContent = message;
        
        document.body.appendChild(notification);
        
        // Animate in
        setTimeout(() => {
            notification.classList.remove('translate-x-full');
        }, 100);
        
        // Animate out and remove
        setTimeout(() => {
            notification.classList.add('translate-x-full');
            setTimeout(() => {
                if (notification.parentNode) {
                    notification.remove();
                }
            }, 300);
        }, type === 'error' ? 5000 : 3000);
    }

    /**
     * Render methods (keeping existing implementation)
     */
    renderVlans() {
        const tbody = document.querySelector('#vlan-section tbody');
        if (!tbody) return;
        
        // Clear existing content safely
        while (tbody.firstChild) {
            tbody.removeChild(tbody.firstChild);
        }
        
        this.state.vlans.forEach(vlan => {
            const row = document.createElement('tr');
            row.className = 'border-b border-neutral-100 hover:bg-neutral-50';
            row.dataset.id = vlan.id;
            
            // Create VLAN ID cell
            const vlanIdCell = this.createElement('td', 'py-3 px-4 text-sm', vlan.vlan_id);
            row.appendChild(vlanIdCell);
            
            // Create name cell
            const nameCell = this.createElement('td', 'py-3 px-4', vlan.name);
            row.appendChild(nameCell);
            
            // Create ports cell with safe content
            const portsCell = document.createElement('td');
            portsCell.className = 'py-3 px-4';
            const portsContainer = document.createElement('div');
            portsContainer.className = 'flex flex-wrap gap-1';
            
            if (Array.isArray(vlan.assigned_ports)) {
                vlan.assigned_ports.forEach(port => {
                    const portSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', port);
                    portsContainer.appendChild(portSpan);
                });
            }
            portsCell.appendChild(portsContainer);
            row.appendChild(portsCell);
            
            // Create tagged cell
            const taggedCell = document.createElement('td');
            taggedCell.className = 'py-3 px-4';
            const taggedSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', vlan.tagged);
            taggedCell.appendChild(taggedSpan);
            row.appendChild(taggedCell);
            
            // Create status cell
            const statusCell = document.createElement('td');
            statusCell.className = 'py-3 px-4';
            const statusSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', vlan.status);
            statusCell.appendChild(statusSpan);
            row.appendChild(statusCell);
            
            // Create actions cell
            const actionsCell = document.createElement('td');
            actionsCell.className = 'py-3 px-4';
            const actionsContainer = document.createElement('div');
            actionsContainer.className = 'flex items-center space-x-2';
            
            // Edit button
            const editBtn = document.createElement('button');
            editBtn.className = 'text-neutral-600 hover:text-neutral-800';
            editBtn.innerHTML = '<i class="fa-solid fa-edit"></i>';
            actionsContainer.appendChild(editBtn);
            
            // Delete button
            const deleteBtn = document.createElement('button');
            deleteBtn.className = 'text-neutral-600 hover:text-neutral-800';
            deleteBtn.innerHTML = '<i class="fa-solid fa-trash"></i>';
            actionsContainer.appendChild(deleteBtn);
            
            // Toggle button
            const toggleBtn = document.createElement('button');
            toggleBtn.className = 'text-neutral-600 hover:text-neutral-800';
            toggleBtn.innerHTML = '<i class="fa-solid fa-power-off"></i>';
            actionsContainer.appendChild(toggleBtn);
            
            actionsCell.appendChild(actionsContainer);
            row.appendChild(actionsCell);
            
            tbody.appendChild(row);
        });
    }

    renderNatRules() {
        const tbody = document.querySelector('#nat-section tbody');
        if (!tbody) return;
        
        // Clear existing content safely
        while (tbody.firstChild) {
            tbody.removeChild(tbody.firstChild);
        }
        
        this.state.natRules.forEach(rule => {
            const row = document.createElement('tr');
            row.className = 'border-b border-neutral-100 hover:bg-neutral-50';
            row.dataset.id = rule.id;
            
            // Rule ID cell
            const ruleIdCell = this.createElement('td', 'py-3 px-4 text-sm', rule.rule_id);
            row.appendChild(ruleIdCell);
            
            // Type cell
            const typeCell = document.createElement('td');
            typeCell.className = 'py-3 px-4';
            const typeSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', rule.type);
            typeCell.appendChild(typeSpan);
            row.appendChild(typeCell);
            
            // Source cell
            const sourceCell = this.createElement('td', 'py-3 px-4 text-sm', rule.source);
            row.appendChild(sourceCell);
            
            // Destination cell
            const destCell = this.createElement('td', 'py-3 px-4 text-sm', rule.destination);
            row.appendChild(destCell);
            
            // Protocol cell
            const protocolCell = document.createElement('td');
            protocolCell.className = 'py-3 px-4';
            const protocolSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', rule.protocol);
            protocolCell.appendChild(protocolSpan);
            row.appendChild(protocolCell);
            
            // Destination ports cell
            const portsCell = this.createElement('td', 'py-3 px-4 text-sm', rule.destination_ports || 'Any');
            row.appendChild(portsCell);
            
            // Actions cell
            const actionsCell = document.createElement('td');
            actionsCell.className = 'py-3 px-4';
            const actionsContainer = document.createElement('div');
            actionsContainer.className = 'flex items-center space-x-2';
            
            // Edit button
            const editBtn = document.createElement('button');
            editBtn.className = 'text-neutral-600 hover:text-neutral-800';
            editBtn.innerHTML = '<i class="fa-solid fa-edit"></i>';
            actionsContainer.appendChild(editBtn);
            
            // Delete button
            const deleteBtn = document.createElement('button');
            deleteBtn.className = 'text-neutral-600 hover:text-neutral-800';
            deleteBtn.innerHTML = '<i class="fa-solid fa-trash"></i>';
            actionsContainer.appendChild(deleteBtn);
            
            // Toggle button
            const toggleBtn = document.createElement('button');
            toggleBtn.className = 'text-neutral-600 hover:text-neutral-800';
            toggleBtn.innerHTML = '<i class="fa-solid fa-power-off"></i>';
            actionsContainer.appendChild(toggleBtn);
            
            actionsCell.appendChild(actionsContainer);
            row.appendChild(actionsCell);
            
            tbody.appendChild(row);
        });
    }

    renderFirewallRules() {
        const tbody = document.querySelector('#firewall-section tbody');
        if (!tbody) return;
        
        // Clear existing content safely
        while (tbody.firstChild) {
            tbody.removeChild(tbody.firstChild);
        }
        
        this.state.firewallRules.forEach(rule => {
            const row = document.createElement('tr');
            row.className = 'border-b border-neutral-100 hover:bg-neutral-50';
            row.dataset.id = rule.id;
            
            // Rule ID cell
            const ruleIdCell = this.createElement('td', 'py-3 px-4 text-sm', rule.rule_id);
            row.appendChild(ruleIdCell);
            
            // Direction cell
            const directionCell = document.createElement('td');
            directionCell.className = 'py-3 px-4';
            const directionSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', rule.direction);
            directionCell.appendChild(directionSpan);
            row.appendChild(directionCell);
            
            // Source cell
            const sourceCell = this.createElement('td', 'py-3 px-4 text-sm', rule.source);
            row.appendChild(sourceCell);
            
            // Destination cell
            const destCell = this.createElement('td', 'py-3 px-4 text-sm', rule.destination);
            row.appendChild(destCell);
            
            // Protocol cell
            const protocolCell = document.createElement('td');
            protocolCell.className = 'py-3 px-4';
            const protocolSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', rule.protocol);
            protocolCell.appendChild(protocolSpan);
            row.appendChild(protocolCell);
            
            // Destination ports cell
            const portsCell = this.createElement('td', 'py-3 px-4 text-sm', rule.destination_ports || 'Any');
            row.appendChild(portsCell);
            
            // Action cell
            const actionCell = document.createElement('td');
            actionCell.className = 'py-3 px-4';
            const actionSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', rule.action);
            actionCell.appendChild(actionSpan);
            row.appendChild(actionCell);
            
            // Logging cell
            const loggingCell = document.createElement('td');
            loggingCell.className = 'py-3 px-4';
            const loggingIcon = document.createElement('i');
            loggingIcon.className = `fa-solid ${rule.logging ? 'fa-check' : 'fa-times'} text-neutral-600`;
            loggingCell.appendChild(loggingIcon);
            row.appendChild(loggingCell);
            
            // Actions cell
            const actionsCell = document.createElement('td');
            actionsCell.className = 'py-3 px-4';
            const actionsContainer = document.createElement('div');
            actionsContainer.className = 'flex items-center space-x-2';
            
            // Edit button
            const editBtn = document.createElement('button');
            editBtn.className = 'text-neutral-600 hover:text-neutral-800';
            editBtn.innerHTML = '<i class="fa-solid fa-edit"></i>';
            actionsContainer.appendChild(editBtn);
            
            // Delete button
            const deleteBtn = document.createElement('button');
            deleteBtn.className = 'text-neutral-600 hover:text-neutral-800';
            deleteBtn.innerHTML = '<i class="fa-solid fa-trash"></i>';
            actionsContainer.appendChild(deleteBtn);
            
            // Move button
            const moveBtn = document.createElement('button');
            moveBtn.className = 'text-neutral-600 hover:text-neutral-800';
            moveBtn.innerHTML = '<i class="fa-solid fa-arrows-up-down"></i>';
            actionsContainer.appendChild(moveBtn);
            
            actionsCell.appendChild(actionsContainer);
            row.appendChild(actionsCell);
            
            tbody.appendChild(row);
        });
    }

    renderStaticRoutes() {
        const tbody = document.querySelector('#routing-section tbody');
        if (!tbody) return;
        
        // Clear existing content safely
        while (tbody.firstChild) {
            tbody.removeChild(tbody.firstChild);
        }
        
        this.state.staticRoutes.forEach(route => {
            const row = document.createElement('tr');
            row.className = 'border-b border-neutral-100 hover:bg-neutral-50';
            row.dataset.id = route.id;
            
            // Destination network cell
            const networkCell = this.createElement('td', 'py-3 px-4 text-sm', route.destination_network);
            row.appendChild(networkCell);
            
            // Subnet mask cell
            const maskCell = this.createElement('td', 'py-3 px-4 text-sm', route.subnet_mask);
            row.appendChild(maskCell);
            
            // Gateway cell
            const gatewayCell = this.createElement('td', 'py-3 px-4 text-sm', route.gateway);
            row.appendChild(gatewayCell);
            
            // Interface cell
            const interfaceCell = document.createElement('td');
            interfaceCell.className = 'py-3 px-4';
            const interfaceSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', route.interface);
            interfaceCell.appendChild(interfaceSpan);
            row.appendChild(interfaceCell);
            
            // Metric cell
            const metricCell = this.createElement('td', 'py-3 px-4', route.metric);
            row.appendChild(metricCell);
            
            // Status cell
            const statusCell = document.createElement('td');
            statusCell.className = 'py-3 px-4';
            const statusSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', route.status);
            statusCell.appendChild(statusSpan);
            row.appendChild(statusCell);
            
            // Actions cell
            const actionsCell = document.createElement('td');
            actionsCell.className = 'py-3 px-4';
            const actionsContainer = document.createElement('div');
            actionsContainer.className = 'flex items-center space-x-2';
            
            // Edit button
            const editBtn = document.createElement('button');
            editBtn.className = 'text-neutral-600 hover:text-neutral-800';
            editBtn.innerHTML = '<i class="fa-solid fa-edit"></i>';
            actionsContainer.appendChild(editBtn);
            
            // Delete button
            const deleteBtn = document.createElement('button');
            deleteBtn.className = 'text-neutral-600 hover:text-neutral-800';
            deleteBtn.innerHTML = '<i class="fa-solid fa-trash"></i>';
            actionsContainer.appendChild(deleteBtn);
            
            // Toggle button
            const toggleBtn = document.createElement('button');
            toggleBtn.className = 'text-neutral-600 hover:text-neutral-800';
            toggleBtn.innerHTML = '<i class="fa-solid fa-power-off"></i>';
            actionsContainer.appendChild(toggleBtn);
            
            actionsCell.appendChild(actionsContainer);
            row.appendChild(actionsCell);
            
            tbody.appendChild(row);
        });
    }

    renderBridges() {
        const tbody = document.querySelector('#bridge-section tbody');
        if (!tbody) return;
        
        // Clear existing content safely
        while (tbody.firstChild) {
            tbody.removeChild(tbody.firstChild);
        }
        
        this.state.bridges.forEach(bridge => {
            const row = document.createElement('tr');
            row.className = 'border-b border-neutral-100 hover:bg-neutral-50';
            row.dataset.id = bridge.id;
            
            // Bridge name cell
            const nameCell = this.createElement('td', 'py-3 px-4 text-sm', bridge.name);
            row.appendChild(nameCell);
            
            // IP address cell
            const ipCell = this.createElement('td', 'py-3 px-4 text-sm', bridge.ip_address || 'N/A');
            row.appendChild(ipCell);
            
            // Member interfaces cell
            const interfacesCell = document.createElement('td');
            interfacesCell.className = 'py-3 px-4';
            const interfacesContainer = document.createElement('div');
            interfacesContainer.className = 'flex flex-wrap gap-1';
            
            if (Array.isArray(bridge.member_interfaces) && bridge.member_interfaces.length > 0) {
                bridge.member_interfaces.forEach(iface => {
                    const ifaceSpan = this.createSpan('bg-neutral-100 text-neutral-800 text-xs px-2 py-1 rounded', iface);
                    interfacesContainer.appendChild(ifaceSpan);
                });
            } else {
                const noIfaceSpan = this.createSpan('text-neutral-500 text-xs', 'No interfaces');
                interfacesContainer.appendChild(noIfaceSpan);
            }
            interfacesCell.appendChild(interfacesContainer);
            row.appendChild(interfacesCell);
            
            // STP enabled cell
            const stpCell = document.createElement('td');
            stpCell.className = 'py-3 px-4';
            const stpEnabled = bridge.stp_enabled;
            const stpColor = stpEnabled ? 'green' : 'red';
            const stpSpan = this.createSpan(
                `bg-${stpColor}-100 text-${stpColor}-800 text-xs px-2 py-1 rounded`,
                stpEnabled ? 'Enabled' : 'Disabled'
            );
            stpCell.appendChild(stpSpan);
            row.appendChild(stpCell);
            
            // Priority cell
            const priorityCell = this.createElement('td', 'py-3 px-4', bridge.priority || 'Default');
            row.appendChild(priorityCell);
            
            // Status cell
            const statusCell = document.createElement('td');
            statusCell.className = 'py-3 px-4';
            const status = bridge.status || 'Unknown';
            let statusColor = 'yellow';
            if (status === 'Up') statusColor = 'green';
            else if (status === 'Down') statusColor = 'red';
            
            const statusSpan = this.createSpan(
                `bg-${statusColor}-100 text-${statusColor}-800 text-xs px-2 py-1 rounded`,
                status
            );
            statusCell.appendChild(statusSpan);
            row.appendChild(statusCell);
            
            // Actions cell
            const actionsCell = document.createElement('td');
            actionsCell.className = 'py-3 px-4';
            const actionsContainer = document.createElement('div');
            actionsContainer.className = 'flex items-center space-x-2';
            
            // Edit button
            const editBtn = document.createElement('button');
            editBtn.className = 'text-neutral-600 hover:text-neutral-800';
            editBtn.innerHTML = '<i class="fa-solid fa-edit"></i>';
            actionsContainer.appendChild(editBtn);
            
            // Delete button
            const deleteBtn = document.createElement('button');
            deleteBtn.className = 'text-neutral-600 hover:text-neutral-800';
            deleteBtn.innerHTML = '<i class="fa-solid fa-trash"></i>';
            actionsContainer.appendChild(deleteBtn);
            
            // Toggle button
            const toggleBtn = document.createElement('button');
            toggleBtn.className = 'text-neutral-600 hover:text-neutral-800';
            toggleBtn.innerHTML = '<i class="fa-solid fa-power-off"></i>';
            actionsContainer.appendChild(toggleBtn);
            
            actionsCell.appendChild(actionsContainer);
            row.appendChild(actionsCell);
            
            tbody.appendChild(row);
        });
    }

    /**
     * Action handlers
     */
    async handleEdit(row, section) {
        const id = row.dataset.id;
        const sectionId = section.id;
        
        switch (sectionId) {
            case 'vlan-section':
                const vlan = this.state.vlans.find(v => v.id === id);
                if (vlan) this.showVlanPopup(vlan);
                break;
            case 'nat-section':
                const natRule = this.state.natRules.find(r => r.id === id);
                if (natRule) this.showNatRulePopup(natRule);
                break;
            case 'firewall-section':
                const fwRule = this.state.firewallRules.find(r => r.id === id);
                if (fwRule) this.showFirewallRulePopup(fwRule);
                break;
            case 'routing-section':
                const route = this.state.staticRoutes.find(r => r.id === id);
                if (route) this.showStaticRoutePopup(route);
                break;
            case 'bridge-section':
                const bridge = this.state.bridges.find(b => b.id === id);
                if (bridge) this.showBridgePopup(bridge);
                break;
        }
    }

    async handleDelete(row, section) {
        const id = row.dataset.id;
        const sectionId = section.id;
        
        if (!confirm('Are you sure you want to delete this item?')) {
            return;
        }

        try {
            let endpoint;
            let refreshMethod;

            switch (sectionId) {
                case 'vlan-section':
                    endpoint = `${this.endpoints.vlans}/${id}`;
                    refreshMethod = () => this.loadVlans();
                    break;
                case 'nat-section':
                    endpoint = `${this.endpoints.natRules}/${id}`;
                    refreshMethod = () => this.loadNatRules();
                    break;
                case 'firewall-section':
                    endpoint = `${this.endpoints.firewallRules}/${id}`;
                    refreshMethod = () => this.loadFirewallRules();
                    break;
                case 'routing-section':
                    endpoint = `${this.endpoints.staticRoutes}/${id}`;
                    refreshMethod = () => this.loadStaticRoutes();
                    break;
                case 'bridge-section':
                    endpoint = `${this.endpoints.bridges}/${id}`;
                    refreshMethod = () => this.loadBridges();
                    break;
            }

            const response = await this.makeRequest('DELETE', endpoint);
            
            if (response && response.success) {
                await refreshMethod();
                this.showSuccessMessage('Item deleted successfully');
            } else {
                throw new Error(response?.message || 'Delete failed');
            }
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to delete item:', error);
            this.showErrorMessage('Failed to delete item');
        }
    }

    async handleToggle(row, section) {
        const id = row.dataset.id;
        const sectionId = section.id;
        
        try {
            let currentItem, endpoint, refreshMethod;
            
            switch (sectionId) {
                case 'vlan-section':
                    currentItem = this.state.vlans.find(v => v.id === id);
                    endpoint = `${this.endpoints.vlans}/${id}`;
                    refreshMethod = () => this.loadVlans();
                    break;
                // Add similar cases for other sections...
            }

            if (currentItem) {
                const response = await this.makeRequest('PUT', endpoint, {
                    enabled: !currentItem.enabled
                });
                
                if (response && response.success) {
                    await refreshMethod();
                    this.showSuccessMessage('Status updated successfully');
                }
            }
        } catch (error) {
            console.error('[ADVANCED-NETWORK] Failed to toggle status:', error);
            this.showErrorMessage('Failed to update status');
        }
    }

    async handlePopupSubmit(target) {
        const popup = target.closest('.advanced-network-popup');
        const popupType = popup.dataset.popupType;
        const form = popup.querySelector('form');
        
        // Validate form first
        const validationErrors = this.validateForm(form, popupType);
        if (validationErrors.length > 0) {
            this.showValidationErrors(validationErrors);
            return;
        }

        const formData = new FormData(form);
        const data = Object.fromEntries(formData.entries());
        
        // Show loading state
        const submitBtn = target.classList.contains('popup-submit') ? target : target.closest('.popup-submit');
        const originalText = submitBtn.innerHTML;
        submitBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Processing...';
        submitBtn.disabled = true;

        try {
            let endpoint, refreshMethod;
            const isEdit = popup.dataset.editId;
            const method = isEdit ? 'PUT' : 'POST';
            
            switch (popupType) {
                case 'vlan':
                    endpoint = isEdit ? `${this.endpoints.vlans}/${isEdit}` : this.endpoints.vlans;
                    refreshMethod = () => this.loadVlans();
                    break;
                case 'nat':
                    endpoint = isEdit ? `${this.endpoints.natRules}/${isEdit}` : this.endpoints.natRules;
                    refreshMethod = () => this.loadNatRules();
                    break;
                case 'firewall':
                    endpoint = isEdit ? `${this.endpoints.firewallRules}/${isEdit}` : this.endpoints.firewallRules;
                    refreshMethod = () => this.loadFirewallRules();
                    break;
                case 'route':
                    endpoint = isEdit ? `${this.endpoints.staticRoutes}/${isEdit}` : this.endpoints.staticRoutes;
                    refreshMethod = () => this.loadStaticRoutes();
                    break;
                case 'bridge':
                    endpoint = isEdit ? `${this.endpoints.bridges}/${isEdit}` : this.endpoints.bridges;
                    refreshMethod = () => this.loadBridges();
                    break;
            }

            const response = await this.makeRequest(method, endpoint, data);
            
            if (response && response.success) {
                this.closeAllPopups();
                await refreshMethod();
                this.showSuccessMessage(`${popupType.charAt(0).toUpperCase() + popupType.slice(1)} ${isEdit ? 'updated' : 'created'} successfully`);
            } else {
                throw new Error(response?.message || 'Operation failed');
            }
        } catch (error) {
            console.error(`[ADVANCED-NETWORK] Failed to ${isEdit ? 'update' : 'create'} ${popupType}:`, error);
            this.showErrorMessage(`Failed to ${isEdit ? 'update' : 'create'} ${popupType}: ${error.message}`);
        } finally {
            // Restore button state
            submitBtn.innerHTML = originalText;
            submitBtn.disabled = false;
        }
    }

    /**
     * Popup methods (keeping existing implementation but simplified calls)
     */
    showVlanPopup(vlan = null) {
        console.log('[ADVANCED-NETWORK] Showing VLAN popup', vlan);
        this.closeAllPopups();
        const popup = this.createVlanPopup(vlan);
        document.body.appendChild(popup);
        popup.style.display = 'flex';
        popup.style.zIndex = '9999';
        setTimeout(() => {
            const firstInput = popup.querySelector('input, select, textarea');
            if (firstInput) firstInput.focus();
        }, 100);
    }

    showNatRulePopup(rule = null) {
        console.log('[ADVANCED-NETWORK] Showing NAT rule popup', rule);
        this.closeAllPopups();
        const popup = this.createNatRulePopup(rule);
        document.body.appendChild(popup);
        popup.style.display = 'flex';
        popup.style.zIndex = '9999';
        setTimeout(() => {
            const firstInput = popup.querySelector('input, select, textarea');
            if (firstInput) firstInput.focus();
        }, 100);
    }

    showFirewallRulePopup(rule = null) {
        console.log('[ADVANCED-NETWORK] Showing firewall rule popup', rule);
        this.closeAllPopups();
        const popup = this.createFirewallRulePopup(rule);
        document.body.appendChild(popup);
        popup.style.display = 'flex';
        popup.style.zIndex = '9999';
        setTimeout(() => {
            const firstInput = popup.querySelector('input, select, textarea');
            if (firstInput) firstInput.focus();
        }, 100);
    }

    showStaticRoutePopup(route = null) {
        console.log('[ADVANCED-NETWORK] Showing static route popup', route);
        this.closeAllPopups();
        const popup = this.createStaticRoutePopup(route);
        document.body.appendChild(popup);
        popup.style.display = 'flex';
        popup.style.zIndex = '9999';
        setTimeout(() => {
            const firstInput = popup.querySelector('input, select, textarea');
            if (firstInput) firstInput.focus();
        }, 100);
    }

    showBridgePopup(bridge = null) {
        console.log('[ADVANCED-NETWORK] Showing bridge popup', bridge);
        this.closeAllPopups();
        const popup = this.createBridgePopup(bridge);
        document.body.appendChild(popup);
        popup.style.display = 'flex';
        popup.style.zIndex = '9999';
        setTimeout(() => {
            const firstInput = popup.querySelector('input, select, textarea');
            if (firstInput) firstInput.focus();
        }, 100);
    }

    closeAllPopups() {
        const popups = document.querySelectorAll('.advanced-network-popup');
        popups.forEach(popup => {
            popup.remove();
        });
    }

    /**
     * Enhanced validation methods with robust checking
     */
    validateForm(form, popupType) {
        const errors = [];
        const formData = new FormData(form);
        const data = Object.fromEntries(formData.entries());
        
        switch (popupType) {
            case 'vlan':
                // VLAN ID validation
                const vlanId = parseInt(data.vlan_id);
                if (!data.vlan_id || isNaN(vlanId) || vlanId < 1 || vlanId > 4094) {
                    errors.push('VLAN ID must be between 1 and 4094');
                }
                
                // Name validation
                if (!data.name || data.name.trim() === '') {
                    errors.push('VLAN name is required');
                } else if (data.name.length > 32) {
                    errors.push('VLAN name must be 32 characters or less');
                }
                
                // Ports validation
                if (data.assigned_ports && data.assigned_ports.trim()) {
                    const ports = data.assigned_ports.split(',').map(p => p.trim());
                    const invalidPorts = ports.filter(p => !/^[a-zA-Z0-9._-]+$/.test(p));
                    if (invalidPorts.length > 0) {
                        errors.push(`Invalid port names: ${invalidPorts.join(', ')}`);
                    }
                }
                break;
                
            case 'nat':
                // Rule ID validation
                if (!data.rule_id || data.rule_id.trim() === '') {
                    errors.push('Rule ID is required');
                } else if (!/^[a-zA-Z0-9_-]+$/.test(data.rule_id)) {
                    errors.push('Rule ID can only contain letters, numbers, underscores, and hyphens');
                }
                
                // NAT type validation
                if (!data.type || !['SNAT', 'DNAT', 'Masquerade'].includes(data.type)) {
                    errors.push('Valid NAT type is required (SNAT, DNAT, or Masquerade)');
                }
                
                // Address validation
                if (!data.source || !this.validateCIDR(data.source.trim())) {
                    errors.push('Valid source address/CIDR is required');
                }
                if (!data.destination || !this.validateCIDR(data.destination.trim())) {
                    errors.push('Valid destination address/CIDR is required');
                }
                
                // Port validation
                if (data.source_ports && !this.validatePort(data.source_ports)) {
                    errors.push('Invalid source port format');
                }
                if (data.destination_ports && !this.validatePort(data.destination_ports)) {
                    errors.push('Invalid destination port format');
                }
                
                // Interface validation
                if (data.input_interface && !this.validateInterface(data.input_interface)) {
                    errors.push('Invalid input interface name');
                }
                if (data.output_interface && !this.validateInterface(data.output_interface)) {
                    errors.push('Invalid output interface name');
                }
                
                // Translation address validation for SNAT/DNAT
                if (['SNAT', 'DNAT'].includes(data.type) && data.translation_address) {
                    if (!this.validateIP(data.translation_address)) {
                        errors.push('Valid translation IP address is required for SNAT/DNAT');
                    }
                }
                break;
                
            case 'firewall':
                // Rule ID validation
                if (!data.rule_id || data.rule_id.trim() === '') {
                    errors.push('Rule ID is required');
                } else if (!/^[a-zA-Z0-9_-]+$/.test(data.rule_id)) {
                    errors.push('Rule ID can only contain letters, numbers, underscores, and hyphens');
                }
                
                // Name validation
                if (!data.name || data.name.trim() === '') {
                    errors.push('Rule name is required');
                } else if (data.name.length > 64) {
                    errors.push('Rule name must be 64 characters or less');
                }
                
                // Direction and action validation
                if (!data.direction || !['INPUT', 'OUTPUT', 'FORWARD'].includes(data.direction)) {
                    errors.push('Valid direction is required (INPUT, OUTPUT, or FORWARD)');
                }
                if (!data.action || !['ACCEPT', 'DROP', 'REJECT'].includes(data.action)) {
                    errors.push('Valid action is required (ACCEPT, DROP, or REJECT)');
                }
                
                // Address validation
                if (data.source && data.source.trim() && !this.validateCIDR(data.source.trim())) {
                    errors.push('Invalid source address/CIDR format');
                }
                if (data.destination && data.destination.trim() && !this.validateCIDR(data.destination.trim())) {
                    errors.push('Invalid destination address/CIDR format');
                }
                
                // Port validation
                if (data.source_ports && !this.validatePort(data.source_ports)) {
                    errors.push('Invalid source port format');
                }
                if (data.destination_ports && !this.validatePort(data.destination_ports)) {
                    errors.push('Invalid destination port format');
                }
                break;
                
            case 'route':
                // Name validation
                if (!data.name || data.name.trim() === '') {
                    errors.push('Route name is required');
                } else if (data.name.length > 32) {
                    errors.push('Route name must be 32 characters or less');
                }
                
                // Network validation
                if (!data.destination_network || !this.validateCIDR(data.destination_network.trim())) {
                    errors.push('Valid destination network/CIDR is required');
                }
                
                // Gateway validation
                if (!data.gateway || !this.validateIP(data.gateway.trim())) {
                    errors.push('Valid gateway IP address is required');
                }
                
                // Interface validation
                if (!data.interface || !this.validateInterface(data.interface)) {
                    errors.push('Valid interface name is required');
                }
                
                // Metric validation
                if (data.metric) {
                    const metric = parseInt(data.metric);
                    if (isNaN(metric) || metric < 0 || metric > 65535) {
                        errors.push('Metric must be between 0 and 65535');
                    }
                }
                break;
                
            case 'bridge':
                // Name validation
                if (!data.name || data.name.trim() === '') {
                    errors.push('Bridge name is required');
                } else if (!/^[a-zA-Z][a-zA-Z0-9._-]{0,14}$/.test(data.name)) {
                    errors.push('Bridge name must start with a letter, be max 15 characters, and contain only letters, numbers, dots, underscores, and hyphens');
                }
                
                // IP address validation (if provided)
                if (data.ip_address && data.ip_address.trim() && !this.validateCIDR(data.ip_address.trim())) {
                    errors.push('Invalid IP address/CIDR format');
                }
                
                // STP settings validation
                if (data.hello_time) {
                    const helloTime = parseInt(data.hello_time);
                    if (isNaN(helloTime) || helloTime < 1 || helloTime > 10) {
                        errors.push('Hello time must be between 1 and 10 seconds');
                    }
                }
                
                if (data.max_age) {
                    const maxAge = parseInt(data.max_age);
                    if (isNaN(maxAge) || maxAge < 6 || maxAge > 40) {
                        errors.push('Max age must be between 6 and 40 seconds');
                    }
                }
                
                if (data.forward_delay) {
                    const forwardDelay = parseInt(data.forward_delay);
                    if (isNaN(forwardDelay) || forwardDelay < 4 || forwardDelay > 30) {
                        errors.push('Forward delay must be between 4 and 30 seconds');
                    }
                }
                break;
        }
        
        return errors;
    }

    showValidationErrors(errors) {
        const errorContainer = document.createElement('div');
        errorContainer.className = 'fixed top-4 right-4 bg-red-100 border border-red-400 text-red-700 px-4 py-3 rounded z-50 max-w-md';
        
        const title = document.createElement('div');
        title.className = 'font-medium mb-2';
        title.textContent = 'Validation Errors:';
        errorContainer.appendChild(title);
        
        const errorList = document.createElement('ul');
        errorList.className = 'text-sm list-disc list-inside';
        
        errors.forEach(error => {
            const listItem = document.createElement('li');
            listItem.textContent = error;
            errorList.appendChild(listItem);
        });
        
        errorContainer.appendChild(errorList);
        document.body.appendChild(errorContainer);
        
        setTimeout(() => {
            if (errorContainer.parentNode) {
                errorContainer.remove();
            }
        }, 5000);
    }

    /**
     * Enhanced popup creation methods for all network configuration sections
     */

    createVlanPopup(vlan = null) {
        const isEdit = vlan !== null;
        const title = isEdit ? 'Edit VLAN' : 'Add New VLAN';
        
        const popup = document.createElement('div');
        popup.className = 'advanced-network-popup fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50';
        popup.innerHTML = `
            <div class="popup-content bg-white rounded-xl shadow-xl max-w-md w-full mx-4 max-h-[90vh] overflow-y-auto">
                <div class="popup-header border-b border-neutral-200 p-6">
                    <div class="flex items-center justify-between">
                        <h3 class="text-lg font-semibold text-neutral-900">${title}</h3>
                        <button class="popup-close w-8 h-8 rounded-full bg-neutral-100 hover:bg-neutral-200 flex items-center justify-center">
                            <i class="fa-solid fa-times text-neutral-500"></i>
                        </button>
                    </div>
                </div>
                <form class="popup-form p-6 space-y-4" data-popup-type="vlan">
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">VLAN ID</label>
                        <input type="number" name="vlan_id" min="1" max="4094" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-purple-500 focus:border-transparent" value="${vlan?.vlan_id || ''}" placeholder="e.g., 100" required>
                        <p class="text-xs text-neutral-500 mt-1">Valid range: 1-4094</p>
                    </div>
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">VLAN Name</label>
                        <input type="text" name="name" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-purple-500 focus:border-transparent" value="${vlan?.name || ''}" placeholder="e.g., Management" required>
                    </div>
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">Assigned Ports</label>
                        <input type="text" name="assigned_ports" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-purple-500 focus:border-transparent" value="${vlan?.assigned_ports ? vlan.assigned_ports.join(', ') : ''}" placeholder="e.g., Port 1, Port 2, Port 3">
                        <p class="text-xs text-neutral-500 mt-1">Comma-separated list of ports</p>
                    </div>
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">Tagged Mode</label>
                        <select name="tagged" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-purple-500 focus:border-transparent">
                            <option value="tagged" ${vlan?.tagged === 'tagged' ? 'selected' : ''}>Tagged</option>
                            <option value="untagged" ${vlan?.tagged === 'untagged' ? 'selected' : ''}>Untagged</option>
                        </select>
                    </div>
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">Priority</label>
                        <input type="number" name="priority" min="0" max="7" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-purple-500 focus:border-transparent" value="${vlan?.priority || '0'}" placeholder="0-7">
                    </div>
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">Description</label>
                        <textarea name="description" rows="3" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-purple-500 focus:border-transparent" placeholder="Optional description">${vlan?.description || ''}</textarea>
                    </div>
                    <div class="form-group">
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" name="enabled" class="rounded border-neutral-300 text-purple-600 focus:ring-purple-500" ${vlan?.enabled !== false ? 'checked' : ''}>
                            <span class="text-sm font-medium text-neutral-700">Enable VLAN</span>
                        </label>
                    </div>
                    ${isEdit ? `<input type="hidden" name="id" value="${vlan.id}">` : ''}
                    <div class="popup-actions flex items-center justify-end space-x-3 pt-4 border-t border-neutral-200">
                        <button type="button" class="popup-cancel px-4 py-2 border border-neutral-300 rounded-lg text-neutral-700 hover:bg-neutral-50">Cancel</button>
                        <button type="submit" class="popup-submit px-6 py-2 bg-purple-600 hover:bg-purple-700 text-white rounded-lg">${isEdit ? 'Update' : 'Create'} VLAN</button>
                    </div>
                </form>
            </div>
        `;
        return popup;
    }

    createNatRulePopup(rule = null) {
        const isEdit = rule !== null;
        const title = isEdit ? 'Edit NAT Rule' : 'Add New NAT Rule';
        
        const popup = document.createElement('div');
        popup.className = 'advanced-network-popup fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50';
        popup.innerHTML = `
            <div class="popup-content bg-white rounded-xl shadow-xl max-w-lg w-full mx-4 max-h-[90vh] overflow-y-auto">
                <div class="popup-header border-b border-neutral-200 p-6">
                    <div class="flex items-center justify-between">
                        <h3 class="text-lg font-semibold text-neutral-900">${title}</h3>
                        <button class="popup-close w-8 h-8 rounded-full bg-neutral-100 hover:bg-neutral-200 flex items-center justify-center">
                            <i class="fa-solid fa-times text-neutral-500"></i>
                        </button>
                    </div>
                </div>
                <form class="popup-form p-6 space-y-4" data-popup-type="nat">
                    <div class="grid grid-cols-2 gap-4">
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Rule ID</label>
                            <input type="text" name="rule_id" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.rule_id || ''}" placeholder="e.g., NAT001" required>
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">NAT Type</label>
                            <select name="type" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" required>
                                <option value="SNAT" ${rule?.type === 'SNAT' ? 'selected' : ''}>SNAT (Source NAT)</option>
                                <option value="DNAT" ${rule?.type === 'DNAT' ? 'selected' : ''}>DNAT (Destination NAT)</option>
                                <option value="Masquerade" ${rule?.type === 'Masquerade' ? 'selected' : ''}>Masquerade</option>
                            </select>
                        </div>
                    </div>
                    <div class="grid grid-cols-2 gap-4">
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Source Address</label>
                            <input type="text" name="source" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.source || ''}" placeholder="e.g., 192.168.1.0/24" required>
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Destination Address</label>
                            <input type="text" name="destination" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.destination || ''}" placeholder="e.g., 0.0.0.0/0" required>
                        </div>
                    </div>
                    <div class="grid grid-cols-3 gap-4">
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Protocol</label>
                            <select name="protocol" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent">
                                <option value="TCP" ${rule?.protocol === 'TCP' ? 'selected' : ''}>TCP</option>
                                <option value="UDP" ${rule?.protocol === 'UDP' ? 'selected' : ''}>UDP</option>
                                <option value="ICMP" ${rule?.protocol === 'ICMP' ? 'selected' : ''}>ICMP</option>
                                <option value="Any" ${rule?.protocol === 'Any' ? 'selected' : ''}>Any</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Source Ports</label>
                            <input type="text" name="source_ports" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.source_ports || ''}" placeholder="e.g., 80,443">
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Dest Ports</label>
                            <input type="text" name="destination_ports" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.destination_ports || ''}" placeholder="e.g., 22">
                        </div>
                    </div>
                    <div class="grid grid-cols-2 gap-4">
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Input Interface</label>
                            <input type="text" name="input_interface" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.input_interface || ''}" placeholder="e.g., eth0">
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Output Interface</label>
                            <input type="text" name="output_interface" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.output_interface || ''}" placeholder="e.g., eth1">
                        </div>
                    </div>
                    <div class="grid grid-cols-2 gap-4">
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Translation Address</label>
                            <input type="text" name="translation_address" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.translation_address || ''}" placeholder="e.g., 10.0.0.100">
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Translation Port</label>
                            <input type="text" name="translation_port" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" value="${rule?.translation_port || ''}" placeholder="e.g., 2222">
                        </div>
                    </div>
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">Description</label>
                        <textarea name="description" rows="2" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-green-500 focus:border-transparent" placeholder="Optional description">${rule?.description || ''}</textarea>
                    </div>
                    <div class="form-group">
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" name="enabled" class="rounded border-neutral-300 text-green-600 focus:ring-green-500" ${rule?.enabled !== false ? 'checked' : ''}>
                            <span class="text-sm font-medium text-neutral-700">Enable Rule</span>
                        </label>
                    </div>
                    ${isEdit ? `<input type="hidden" name="id" value="${rule.id}">` : ''}
                    <div class="popup-actions flex items-center justify-end space-x-3 pt-4 border-t border-neutral-200">
                        <button type="button" class="popup-cancel px-4 py-2 border border-neutral-300 rounded-lg text-neutral-700 hover:bg-neutral-50">Cancel</button>
                        <button type="submit" class="popup-submit px-6 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg">${isEdit ? 'Update' : 'Create'} Rule</button>
                    </div>
                </form>
            </div>
        `;
        return popup;
    }
    
    createFirewallRulePopup(rule = null) {
        const isEdit = rule !== null;
        const title = isEdit ? 'Edit Firewall Rule' : 'Add New Firewall Rule';
        
        const popup = document.createElement('div');
        popup.className = 'advanced-network-popup fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50';
        popup.innerHTML = `
            <div class="popup-content bg-white rounded-xl shadow-xl max-w-lg w-full mx-4 max-h-[90vh] overflow-y-auto">
                <div class="popup-header border-b border-neutral-200 p-6">
                    <div class="flex items-center justify-between">
                        <h3 class="text-lg font-semibold text-neutral-900">${title}</h3>
                        <button class="popup-close w-8 h-8 rounded-full bg-neutral-100 hover:bg-neutral-200 flex items-center justify-center">
                            <i class="fa-solid fa-times text-neutral-500"></i>
                        </button>
                    </div>
                </div>
                <form class="popup-form p-6 space-y-4" data-popup-type="firewall">
                    <div class="grid grid-cols-2 gap-4">
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Rule ID</label>
                            <input type="text" name="rule_id" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-red-500 focus:border-transparent" value="${rule?.rule_id || ''}" placeholder="e.g., FW001" required>
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Rule Name</label>
                            <input type="text" name="name" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-red-500 focus:border-transparent" value="${rule?.name || ''}" placeholder="e.g., Allow SSH" required>
                        </div>
                    </div>
                    <div class="grid grid-cols-2 gap-4">
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Direction</label>
                            <select name="direction" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-red-500 focus:border-transparent" required>
                                <option value="INPUT" ${rule?.direction === 'INPUT' ? 'selected' : ''}>INPUT</option>
                                <option value="OUTPUT" ${rule?.direction === 'OUTPUT' ? 'selected' : ''}>OUTPUT</option>
                                <option value="FORWARD" ${rule?.direction === 'FORWARD' ? 'selected' : ''}>FORWARD</option>
                            </select>
                        </div>
                        <div class="form-group">
                            <label class="block text-sm font-medium text-neutral-700 mb-2">Action</label>
                            <select name="action" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-red-500 focus:border-transparent" required>
                                <option value="ACCEPT" ${rule?.action === 'ACCEPT' ? 'selected' : ''}>ACCEPT</option>
                                <option value="DROP" ${rule?.action === 'DROP' ? 'selected' : ''}>DROP</option>
                                <option value="REJECT" ${rule?.action === 'REJECT' ? 'selected' : ''}>REJECT</option>
                            </select>
                        </div>
                    </div>
                    <div class="form-group">
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" name="enabled" class="rounded border-neutral-300 text-red-600 focus:ring-red-500" ${rule?.enabled !== false ? 'checked' : ''}>
                            <span class="text-sm font-medium text-neutral-700">Enable Rule</span>
                        </label>
                    </div>
                    ${isEdit ? `<input type="hidden" name="id" value="${rule.id}">` : ''}
                    <div class="popup-actions flex items-center justify-end space-x-3 pt-4 border-t border-neutral-200">
                        <button type="button" class="popup-cancel px-4 py-2 border border-neutral-300 rounded-lg text-neutral-700 hover:bg-neutral-50">Cancel</button>
                        <button type="submit" class="popup-submit px-6 py-2 bg-red-600 hover:bg-red-700 text-white rounded-lg">${isEdit ? 'Update' : 'Create'} Rule</button>
                    </div>
                </form>
            </div>
        `;
        return popup;
    }

    createStaticRoutePopup(route = null) {
        const isEdit = route !== null;
        const title = isEdit ? 'Edit Static Route' : 'Add New Static Route';
        
        const popup = document.createElement('div');
        popup.className = 'advanced-network-popup fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50';
        popup.innerHTML = `
            <div class="popup-content bg-white rounded-xl shadow-xl max-w-md w-full mx-4 max-h-[90vh] overflow-y-auto">
                <div class="popup-header border-b border-neutral-200 p-6">
                    <div class="flex items-center justify-between">
                        <h3 class="text-lg font-semibold text-neutral-900">${title}</h3>
                        <button class="popup-close w-8 h-8 rounded-full bg-neutral-100 hover:bg-neutral-200 flex items-center justify-center">
                            <i class="fa-solid fa-times text-neutral-500"></i>
                        </button>
                    </div>
                </div>
                <form class="popup-form p-6 space-y-4" data-popup-type="route">
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">Route Name</label>
                        <input type="text" name="name" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent" value="${route?.name || ''}" placeholder="e.g., Default Route" required>
                    </div>
                    <div class="form-group">
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" name="enabled" class="rounded border-neutral-300 text-blue-600 focus:ring-blue-500" ${route?.enabled !== false ? 'checked' : ''}>
                            <span class="text-sm font-medium text-neutral-700">Enable Route</span>
                        </label>
                    </div>
                    ${isEdit ? `<input type="hidden" name="id" value="${route.id}">` : ''}
                    <div class="popup-actions flex items-center justify-end space-x-3 pt-4 border-t border-neutral-200">
                        <button type="button" class="popup-cancel px-4 py-2 border border-neutral-300 rounded-lg text-neutral-700 hover:bg-neutral-50">Cancel</button>
                        <button type="submit" class="popup-submit px-6 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg">${isEdit ? 'Update' : 'Create'} Route</button>
                    </div>
                </form>
            </div>
        `;
        return popup;
    }

    createBridgePopup(bridge = null) {
        const isEdit = bridge !== null;
        const title = isEdit ? 'Edit Bridge' : 'Create New Bridge';
        
        const popup = document.createElement('div');
        popup.className = 'advanced-network-popup fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50';
        popup.innerHTML = `
            <div class="popup-content bg-white rounded-xl shadow-xl max-w-md w-full mx-4 max-h-[90vh] overflow-y-auto">
                <div class="popup-header border-b border-neutral-200 p-6">
                    <div class="flex items-center justify-between">
                        <h3 class="text-lg font-semibold text-neutral-900">${title}</h3>
                        <button class="popup-close w-8 h-8 rounded-full bg-neutral-100 hover:bg-neutral-200 flex items-center justify-center">
                            <i class="fa-solid fa-times text-neutral-500"></i>
                        </button>
                    </div>
                </div>
                <form class="popup-form p-6 space-y-4" data-popup-type="bridge">
                    <div class="form-group">
                        <label class="block text-sm font-medium text-neutral-700 mb-2">Bridge Name</label>
                        <input type="text" name="name" class="w-full px-3 py-2 border border-neutral-300 rounded-lg focus:ring-2 focus:ring-orange-500 focus:border-transparent" value="${bridge?.name || ''}" placeholder="e.g., br0" required>
                    </div>
                    <div class="form-group">
                        <label class="flex items-center space-x-2">
                            <input type="checkbox" name="enabled" class="rounded border-neutral-300 text-orange-600 focus:ring-orange-500" ${bridge?.enabled !== false ? 'checked' : ''}>
                            <span class="text-sm font-medium text-neutral-700">Enable Bridge</span>
                        </label>
                    </div>
                    ${isEdit ? `<input type="hidden" name="id" value="${bridge.id}">` : ''}
                    <div class="popup-actions flex items-center justify-end space-x-3 pt-4 border-t border-neutral-200">
                        <button type="button" class="popup-cancel px-4 py-2 border border-neutral-300 rounded-lg text-neutral-700 hover:bg-neutral-50">Cancel</button>
                        <button type="submit" class="popup-submit px-6 py-2 bg-orange-600 hover:bg-orange-700 text-white rounded-lg">${isEdit ? 'Update' : 'Create'} Bridge</button>
                    </div>
                </form>
            </div>
        `;
        return popup;
    }

    /**
     * Cleanup method
     */
    cleanup() {
        this.stopAutoRefresh();
        this.closeAllPopups();
        
        // Clear pending requests
        this.requestState.pendingRequests.clear();
        this.requestState.retryAttempts.clear();
        
        console.log('[ADVANCED-NETWORK] Section cleanup complete');
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    console.log('[ADVANCED-NETWORK] DOM Content Loaded, initializing...');
    try {
        window.advancedNetworkSection = new AdvancedNetworkSection();
        console.log('[ADVANCED-NETWORK] Section initialized successfully');
    } catch (error) {
        console.error('[ADVANCED-NETWORK] Failed to initialize section:', error);
    }
});
