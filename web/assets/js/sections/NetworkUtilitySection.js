/**
 * NetworkUtilitySection.js - Complete Network Utility Management
 * Handles bandwidth tests, ping tests, traceroute, DNS lookup, and server status monitoring
 */

class NetworkUtilitySection {
    constructor() {
        console.log('[NETWORK-UTILITY] Initializing NetworkUtilitySection...');
        this.logPrefix = '[NETWORK-UTILITY]';

        // API endpoints
        this.endpoints = {
            bandwidth: {
                start: '/api/network-utility/bandwidth/start',
                stop: '/api/network-utility/bandwidth/stop',
                status: '/api/network-utility/bandwidth/status'
            },
            ping: {
                start: '/api/network-utility/ping/start',
                stop: '/api/network-utility/ping/stop',
                results: '/api/network-utility/ping/results'
            },
            traceroute: {
                start: '/api/network-utility/traceroute/start',
                stop: '/api/network-utility/traceroute/stop',
                results: '/api/network-utility/traceroute/results'
            },
            dns: {
                lookup: '/api/network-utility/dns/lookup'
            },
            servers: {
                status: '/api/network-utility/servers/status',
                list: '/api/network-utility/servers/list',
                test: '/api/network-utility/servers/test',
                add: '/api/network-utility/servers/add',
                remove: '/api/network-utility/servers/remove'
            },
            system: {
                info: '/api/network-utility/system/info',
                interfaces: '/api/network-utility/system/interfaces'
            }
        };

        // Test state management
        this.activeTests = new Map();
        this.updateIntervals = new Map();
        this.serverStatusInterval = null;
        this.tracerouteStartTime = null;

        this.init();
    }

    log(message, data = null) {
        console.log(`${this.logPrefix} ${message}`, data || '');
    }

    logError(message, error) {
        console.error(`${this.logPrefix} [ERROR] ${message}`, error);
    }

    async init() {
        try {
            this.log('Starting NetworkUtilitySection initialization...');

            this.setupEventHandlers();
            await this.loadServerList();
            this.startServerStatusMonitoring();

            this.log('NetworkUtilitySection initialized successfully');
        } catch (error) {
            this.logError('Failed to initialize NetworkUtilitySection:', error);
        }
    }

    setupEventHandlers() {
        this.log('Setting up event handlers...');

        // Bandwidth test handlers
        this.setupBandwidthTestHandlers();

        // Ping test handlers  
        this.setupPingTestHandlers();

        // Traceroute handlers
        this.setupTracerouteHandlers();

        // DNS lookup handlers
        this.setupDNSLookupHandlers();

        // Server management handlers
        this.setupServerManagementHandlers();

        this.log('Event handlers setup complete');
    }

    setupTracerouteHandlers() {
        const startBtn = document.getElementById('start-traceroute-btn');
        const stopBtn = document.getElementById('stop-traceroute-btn');
        const resetBtn = document.getElementById('reset-traceroute-btn');

        if (startBtn) startBtn.addEventListener('click', () => this.startTraceroute());
        if (stopBtn) stopBtn.addEventListener('click', () => this.stopTraceroute());
        if (resetBtn) resetBtn.addEventListener('click', () => this.resetTraceroute());
    }

    async startTraceroute() {
        try {
            const config = this.getTracerouteConfig();

            if (!config.targetHost) {
                this.showError('Please enter a target host');
                return;
            }

            this.log('Starting traceroute test:', config);

            const response = await this.makeApiRequest('POST', this.endpoints.traceroute.start, config);

            if (response.success) {
                const testId = response.testId;
                this.activeTests.set('traceroute', testId);

                this.updateTracerouteUI('running');
                this.startTracerouteMonitoring(testId);

                this.log('Traceroute started successfully:', testId);
            } else {
                throw new Error(response.message || 'Failed to start traceroute');
            }
        } catch (error) {
            this.logError('Error starting traceroute:', error);
            this.showError('Failed to start traceroute: ' + error.message);
        }
    }

    async stopTraceroute() {
        try {
            const response = await this.makeApiRequest('POST', this.endpoints.traceroute.stop);

            if (response.success) {
                this.stopTracerouteMonitoring();
                this.updateTracerouteUI('stopped');
                this.log('Traceroute stopped successfully');
            }
        } catch (error) {
            this.logError('Error stopping traceroute:', error);
        }
    }

    resetTraceroute() {
        this.stopTracerouteMonitoring();
        this.clearTracerouteResults();
        this.updateTracerouteUI('ready');
        this.activeTests.delete('traceroute');
        this.tracerouteStartTime = null;
        this.log('Traceroute reset');
    }

    startTracerouteMonitoring(testId) {
        this.stopTracerouteMonitoring(); // Clear any existing interval

        const interval = setInterval(async () => {
            await this.updateTracerouteProgress(testId);
        }, 1000);

        this.updateIntervals.set('traceroute', interval);
    }

    stopTracerouteMonitoring() {
        const interval = this.updateIntervals.get('traceroute');
        if (interval) {
            clearInterval(interval);
            this.updateIntervals.delete('traceroute');
        }
    }

    async updateTracerouteProgress(testId) {
        try {
            const response = await this.makeApiRequest('GET', this.endpoints.traceroute.results);

            if (response.success && response.activeTests) {
                const test = response.activeTests.find(t => t.testId === testId);

                if (test) {
                    this.updateTracerouteDisplay(test);

                    if (!test.isRunning) {
                        this.stopTracerouteMonitoring();
                        this.updateTracerouteUI('complete');
                        this.log('Traceroute completed');
                    }
                }
            }
        } catch (error) {
            this.logError('Error updating traceroute progress:', error);
        }
    }

    updateTracerouteDisplay(test) {
        // Update progress
        const progress = test.progress || 0;
        const maxHops = parseInt(document.getElementById('traceroute-max-hops')?.value || 30);

        this.updateElement('traceroute-current-hop', `${test.currentHop || 0} of ${maxHops}`);
        this.updateElement('traceroute-progress-text', `${progress}% Complete`);

        const progressBar = document.getElementById('traceroute-progress-bar');
        if (progressBar) {
            progressBar.style.width = `${progress}%`;
        }

        // Update elapsed time
        this.updateTracerouteElapsedTime();

        // Update results
        if (test.realTimeResults) {
            this.updateTracerouteResults(test.realTimeResults);
        }
    }

    updateTracerouteResults(results) {
        const tbody = document.getElementById('traceroute-results-tbody');
        if (!tbody) return;

        // Clear loading message
        const noDataRow = document.getElementById('traceroute-no-data');
        if (noDataRow) {
            noDataRow.remove();
        }

        // Process results
        if (Array.isArray(results)) {
            results.forEach(hop => {
                this.addOrUpdateTracerouteHop(hop);
            });
        }

        // Update total hops
        this.updateElement('traceroute-total-hops', results.length.toString());
    }

    addOrUpdateTracerouteHop(hop) {
        const tbody = document.getElementById('traceroute-results-tbody');
        if (!tbody) return;

        let row = tbody.querySelector(`tr[data-hop="${hop.hop}"]`);

        if (!row) {
            row = document.createElement('tr');
            row.className = 'hover:bg-neutral-50';
            row.dataset.hop = hop.hop;
            tbody.appendChild(row);
        }

        const statusIcon = this.getTracerouteStatusIcon(hop.status);
        const formatRTT = (rtt) => rtt && rtt > 0 ? `${rtt.toFixed(3)} ms` : '*';

        row.innerHTML = `
            <td class="py-3 px-4 text-neutral-800">${hop.hop}</td>
            <td class="py-3 px-4 text-neutral-600">${hop.ip || '*'}</td>
            <td class="py-3 px-4 text-neutral-600">${hop.hostname || '*'}</td>
            <td class="py-3 px-4 text-neutral-600 text-right">${formatRTT(hop.rtt1)}</td>
            <td class="py-3 px-4 text-neutral-600 text-right">${formatRTT(hop.rtt2)}</td>
            <td class="py-3 px-4 text-neutral-600 text-right">${formatRTT(hop.rtt3)}</td>
            <td class="py-3 px-4 text-center">${statusIcon}</td>
        `;
    }

    getTracerouteStatusIcon(status) {
        switch (status) {
            case 'success':
                return '<i class="fa-solid fa-check text-green-500"></i>';
            case 'timeout':
                return '<i class="fa-solid fa-clock text-yellow-500"></i>';
            case 'running':
                return '<i class="fa-solid fa-spinner fa-spin text-blue-500"></i>';
            default:
                return '<i class="fa-solid fa-question text-gray-500"></i>';
        }
    }

    updateTracerouteUI(state) {
        const startBtn = document.getElementById('start-traceroute-btn');
        const stopBtn = document.getElementById('stop-traceroute-btn');

        switch (state) {
            case 'ready':
                if (startBtn) startBtn.disabled = false;
                if (stopBtn) stopBtn.disabled = true;
                this.updateTracerouteStatus('Ready', 'gray');
                break;
            case 'running':
                if (startBtn) startBtn.disabled = true;
                if (stopBtn) stopBtn.disabled = false;
                this.updateTracerouteStatus('Running', 'blue');
                this.tracerouteStartTime = Date.now();
                break;
            case 'complete':
                if (startBtn) startBtn.disabled = false;
                if (stopBtn) stopBtn.disabled = true;
                this.updateTracerouteStatus('Complete', 'green');
                break;
            case 'stopped':
                if (startBtn) startBtn.disabled = false;
                if (stopBtn) stopBtn.disabled = true;
                this.updateTracerouteStatus('Stopped', 'yellow');
                break;
        }
    }

    updateTracerouteStatus(text, color) {
        const statusElement = document.getElementById('traceroute-status');
        if (!statusElement) return;

        const colorClass = this.getStatusColorClass(color);
        statusElement.innerHTML = `<div class="w-2 h-2 ${colorClass} rounded-full mr-2"></div>${text}`;
    }

    getStatusColorClass(color) {
        const classes = {
            gray: 'bg-gray-400',
            blue: 'bg-blue-500 animate-pulse',
            green: 'bg-green-500',
            yellow: 'bg-yellow-500',
            red: 'bg-red-500'
        };
        return classes[color] || 'bg-gray-400';
    }

    updateTracerouteElapsedTime() {
        if (!this.tracerouteStartTime) return;

        const elapsed = Math.floor((Date.now() - this.tracerouteStartTime) / 1000);
        const minutes = Math.floor(elapsed / 60);
        const seconds = elapsed % 60;

        this.updateElement('traceroute-elapsed-time', 
            `${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`);
    }

    clearTracerouteResults() {
        const tbody = document.getElementById('traceroute-results-tbody');
        if (tbody) {
            tbody.innerHTML = '<tr id="traceroute-no-data"><td colspan="7" class="py-8 text-center text-neutral-500">Click "Start Traceroute" to begin network path analysis</td></tr>';
        }

        // Reset counters
        this.updateElement('traceroute-current-hop', '0 of 30');
        this.updateElement('traceroute-elapsed-time', '00:00');
        this.updateElement('traceroute-total-hops', '0');
        this.updateElement('traceroute-progress-text', '0% Complete');

        const progressBar = document.getElementById('traceroute-progress-bar');
        if (progressBar) {
            progressBar.style.width = '0%';
        }
    }

    getTracerouteConfig() {
        return {
            targetHost: document.getElementById('traceroute-target-host')?.value.trim() || '',
            maxHops: parseInt(document.getElementById('traceroute-max-hops')?.value) || 30,
            timeout: parseInt(document.getElementById('traceroute-timeout')?.value) || 5,
            protocol: document.getElementById('traceroute-protocol')?.value || 'icmp'
        };
    }

    // Bandwidth test handlers
    setupBandwidthTestHandlers() {
        const startBtn = document.getElementById('start-bandwidth-test');
        const stopBtn = document.getElementById('stop-bandwidth-test');
        const saveBandwidthBtn = document.getElementById('save-bandwidth-results');

        if (startBtn) startBtn.addEventListener('click', () => this.startBandwidthTest());
        if (stopBtn) stopBtn.addEventListener('click', () => this.stopBandwidthTest());
        if (saveBandwidthBtn) saveBandwidthBtn.addEventListener('click', () => this.saveBandwidthResults());

        // Load servers when bandwidth test popup is opened
        const bandwidthTestBtn = document.getElementById('bandwidth-test-btn');
        if (bandwidthTestBtn) {
            bandwidthTestBtn.addEventListener('click', () => {
                this.loadBandwidthServers();
            });
        }
    }

    async startBandwidthTest() {
        try {
            const config = this.getBandwidthConfig();

            if (!config.targetServer) {
                this.showError('Please select a target server');
                return;
            }

            this.log('Starting bandwidth test:', config);

            const response = await this.makeApiRequest('POST', this.endpoints.bandwidth.start, config);

            if (response.success) {
                const testId = response.testId || response.test_id;
                this.activeTests.set('bandwidth', testId);

                this.updateBandwidthUI('running');
                this.startBandwidthMonitoring(testId);
                this.showBandwidthResults();

                this.log('Bandwidth test started successfully:', testId);
            } else {
                throw new Error(response.message || 'Failed to start bandwidth test');
            }
        } catch (error) {
            this.logError('Error starting bandwidth test:', error);
            this.showError('Failed to start bandwidth test: ' + error.message);
            this.updateBandwidthUI('ready');
        }
    }

    async stopBandwidthTest() {
        try {
            const testId = this.activeTests.get('bandwidth');
            const response = await this.makeApiRequest('POST', this.endpoints.bandwidth.stop, { testId });

            if (response.success) {
                this.stopBandwidthMonitoring();
                this.updateBandwidthUI('stopped');
                this.log('Bandwidth test stopped successfully');
            }
        } catch (error) {
            this.logError('Error stopping bandwidth test:', error);
            this.showError('Failed to stop bandwidth test');
        }
    }

    startBandwidthMonitoring(testId) {
        this.stopBandwidthMonitoring();

        const interval = setInterval(async () => {
            await this.updateBandwidthProgress(testId);
        }, 1000);

        this.updateIntervals.set('bandwidth', interval);
    }

    stopBandwidthMonitoring() {
        const interval = this.updateIntervals.get('bandwidth');
        if (interval) {
            clearInterval(interval);
            this.updateIntervals.delete('bandwidth');
        }
    }

    async updateBandwidthProgress(testId) {
        try {
            const response = await this.makeApiRequest('GET', `${this.endpoints.bandwidth.status}?testId=${testId}`);

            if (response.success && response.testData) {
                const data = response.testData;

                // Update speed displays
                this.updateElement('bandwidth-download', `${(data.downloadSpeed || 0).toFixed(2)} Mbps`);
                this.updateElement('bandwidth-upload', `${(data.uploadSpeed || 0).toFixed(2)} Mbps`);
                this.updateElement('bandwidth-latency', `${(data.latency || 0).toFixed(1)} ms`);

                // Update progress
                const progress = Math.round(data.progress || 0);
                this.updateElement('bandwidth-progress-text', `${progress}% Complete`);
                
                const progressBar = document.getElementById('bandwidth-progress-bar');
                if (progressBar) {
                    progressBar.style.width = `${progress}%`;
                }

                if (!data.isRunning) {
                    this.stopBandwidthMonitoring();
                    this.updateBandwidthUI('complete');
                }
            }
        } catch (error) {
            this.logError('Error updating bandwidth progress:', error);
        }
    }

    updateBandwidthUI(state) {
        const startBtn = document.getElementById('start-bandwidth-test');
        const stopBtn = document.getElementById('stop-bandwidth-test');

        switch (state) {
            case 'ready':
                if (startBtn) {
                    startBtn.disabled = false;
                    startBtn.innerHTML = '<i class="fa-solid fa-play mr-2"></i>Start Test';
                }
                if (stopBtn) stopBtn.disabled = true;
                break;
            case 'running':
                if (startBtn) {
                    startBtn.disabled = true;
                    startBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Testing...';
                }
                if (stopBtn) stopBtn.disabled = false;
                break;
            case 'complete':
            case 'stopped':
                if (startBtn) {
                    startBtn.disabled = false;
                    startBtn.innerHTML = '<i class="fa-solid fa-play mr-2"></i>Start Test';
                }
                if (stopBtn) stopBtn.disabled = true;
                break;
        }
    }

    showBandwidthResults() {
        const resultsSection = document.getElementById('bandwidth-results-section');
        if (resultsSection) {
            resultsSection.classList.remove('hidden');
        }
    }

    getBandwidthConfig() {
        return {
            targetServer: document.getElementById('bandwidth-server')?.value || '',
            duration: parseInt(document.getElementById('bandwidth-duration')?.value) || 10,
            protocol: document.querySelector('input[name="bandwidth-protocol"]:checked')?.value || 'tcp',
            parallelConnections: parseInt(document.getElementById('bandwidth-parallel')?.value) || 1,
            bidirectional: document.getElementById('bandwidth-bidirectional')?.checked || false,
            reverseMode: document.getElementById('bandwidth-reverse')?.checked || false
        };
    }

    async loadBandwidthServers() {
        try {
            // Use existing server data if available
            if (this.serverStatusData && this.serverStatusData.length > 0) {
                this.populateBandwidthServerDropdown(this.serverStatusData);
                return;
            }

            // Otherwise load fresh data
            await this.loadServerList();
            this.populateBandwidthServerDropdown(this.serverStatusData || []);
        } catch (error) {
            this.logError('Failed to load bandwidth servers:', error);
            this.populateBandwidthServerDropdown([]);
        }
    }

    populateBandwidthServerDropdown(servers) {
        const select = document.getElementById('bandwidth-server');
        if (!select) return;

        select.innerHTML = '<option value="">Select a server...</option>';

        if (!servers || servers.length === 0) {
            const option = document.createElement('option');
            option.value = '';
            option.textContent = 'No servers available';
            option.disabled = true;
            select.appendChild(option);
            return;
        }

        servers.forEach(server => {
            const option = document.createElement('option');
            option.value = server.hostname || server.ip_address || server.id;
            
            const name = server.name || server.hostname || server.ip_address;
            const location = server.location || 'Unknown';
            const status = server.status || 'unknown';
            const statusIndicator = status.toLowerCase() === 'online' ? '🟢' : '🔴';

            option.textContent = `${statusIndicator} ${name} (${location})`;

            if (status.toLowerCase() !== 'online') {
                option.disabled = true;
                option.style.color = '#999';
            }

            select.appendChild(option);
        });
    }

    saveBandwidthResults() {
        const results = {
            timestamp: new Date().toISOString(),
            testConfig: this.getBandwidthConfig(),
            results: {
                downloadSpeed: document.getElementById('bandwidth-download')?.textContent || '0 Mbps',
                uploadSpeed: document.getElementById('bandwidth-upload')?.textContent || '0 Mbps',
                latency: document.getElementById('bandwidth-latency')?.textContent || '0 ms'
            }
        };

        const jsonString = JSON.stringify(results, null, 2);
        const blob = new Blob([jsonString], { type: 'application/json' });
        const url = window.URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = `bandwidth_test_${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        window.URL.revokeObjectURL(url);

        this.showSuccess('Bandwidth test results saved successfully');
    }

    // Ping test handlers
    setupPingTestHandlers() {
        const startBtn = document.getElementById('start-ping-test');
        const stopBtn = document.getElementById('stop-ping-test');
        const copyBtn = document.getElementById('copy-ping-results');

        if (startBtn) startBtn.addEventListener('click', () => this.startPingTest());
        if (stopBtn) stopBtn.addEventListener('click', () => this.stopPingTest());
        if (copyBtn) copyBtn.addEventListener('click', () => this.copyPingResults());
    }

    async startPingTest() {
        try {
            const config = this.getPingConfig();

            if (!config.targetHost) {
                this.showError('Please enter a target host');
                return;
            }

            this.log('Starting ping test:', config);

            const response = await this.makeApiRequest('POST', this.endpoints.ping.start, config);

            if (response.success) {
                const testId = response.testId;
                this.activeTests.set('ping', testId);

                this.updatePingUI('running');
                this.clearPingResults();
                this.startPingMonitoring(testId);

                this.log('Ping test started successfully:', testId);
            } else {
                throw new Error(response.message || 'Failed to start ping test');
            }
        } catch (error) {
            this.logError('Error starting ping test:', error);
            this.showError('Failed to start ping test: ' + error.message);
        }
    }

    async stopPingTest() {
        try {
            const testId = this.activeTests.get('ping');
            const response = await this.makeApiRequest('POST', this.endpoints.ping.stop, { testId });

            if (response.success) {
                this.stopPingMonitoring();
                this.updatePingUI('stopped');
                this.log('Ping test stopped successfully');
            }
        } catch (error) {
            this.logError('Error stopping ping test:', error);
            this.showError('Failed to stop ping test');
        }
    }

    startPingMonitoring(testId) {
        this.stopPingMonitoring();

        const interval = setInterval(async () => {
            await this.updatePingProgress(testId);
        }, 1000);

        this.updateIntervals.set('ping', interval);
    }

    stopPingMonitoring() {
        const interval = this.updateIntervals.get('ping');
        if (interval) {
            clearInterval(interval);
            this.updateIntervals.delete('ping');
        }
    }

    async updatePingProgress(testId) {
        try {
            const response = await this.makeApiRequest('GET', `${this.endpoints.ping.results}?testId=${testId}`);

            if (response.success && response.activeTests) {
                const test = response.activeTests.find(t => t.testId === testId);

                if (test) {
                    this.updatePingDisplay(test);

                    if (!test.isRunning) {
                        this.stopPingMonitoring();
                        this.updatePingUI('complete');
                    }
                }
            }
        } catch (error) {
            this.logError('Error updating ping progress:', error);
        }
    }

    updatePingDisplay(test) {
        // Update statistics
        this.updateElement('ping-packets-sent', test.packetsSent || 0);
        this.updateElement('ping-packets-received', test.packetsReceived || 0);

        const packetLoss = test.packetsSent > 0 ? 
            Math.round(((test.packetsSent - test.packetsReceived) / test.packetsSent) * 100) : 0;
        this.updateElement('ping-packet-loss', `${packetLoss}%`);

        const avgRtt = test.averageRtt || 0;
        this.updateElement('ping-avg-rtt', `${avgRtt.toFixed(1)}ms`);

        // Update results table
        if (test.realTimeResults && Array.isArray(test.realTimeResults)) {
            this.updatePingResults(test.realTimeResults);
        }
    }

    updatePingResults(results) {
        const tbody = document.getElementById('ping-results-tbody');
        if (!tbody) return;

        const noDataRow = document.getElementById('ping-no-data');
        if (noDataRow) {
            noDataRow.remove();
        }

        results.forEach(ping => {
            this.addOrUpdatePingResult(ping);
        });
    }

    addOrUpdatePingResult(ping) {
        const tbody = document.getElementById('ping-results-tbody');
        if (!tbody) return;

        let row = tbody.querySelector(`tr[data-seq="${ping.sequence}"]`);

        if (!row) {
            row = document.createElement('tr');
            row.className = 'hover:bg-neutral-50';
            row.dataset.seq = ping.sequence;
            tbody.appendChild(row);
        }

        const statusIcon = this.getPingStatusIcon(ping.status);
        const rtt = ping.rtt && ping.rtt > 0 ? `${ping.rtt.toFixed(1)}ms` : 'timeout';

        row.innerHTML = `
            <td class="py-3 px-4 text-neutral-800">${ping.sequence}</td>
            <td class="py-3 px-4 text-neutral-600">${ping.host || ping.targetHost}</td>
            <td class="py-3 px-4 text-neutral-600 text-right">${rtt}</td>
            <td class="py-3 px-4 text-neutral-600 text-right">${ping.ttl || '-'}</td>
            <td class="py-3 px-4 text-neutral-600 text-right">${ping.size || 64}</td>
            <td class="py-3 px-4 text-center">${statusIcon}</td>
            <td class="py-3 px-4 text-neutral-600">${new Date(ping.timestamp).toLocaleTimeString()}</td>
        `;
    }

    getPingStatusIcon(status) {
        switch (status?.toLowerCase()) {
            case 'success':
                return '<i class="fa-solid fa-check text-green-500"></i>';
            case 'timeout':
                return '<i class="fa-solid fa-clock text-red-500"></i>';
            case 'running':
                return '<i class="fa-solid fa-spinner fa-spin text-blue-500"></i>';
            case 'error':
                return '<i class="fa-solid fa-exclamation-triangle text-red-500"></i>';
            default:
                return '<i class="fa-solid fa-question text-gray-500"></i>';
        }
    }

    updatePingUI(state) {
        const startBtn = document.getElementById('start-ping-test');
        const stopBtn = document.getElementById('stop-ping-test');
        const statusIndicator = document.getElementById('ping-status-indicator');
        const statusText = document.getElementById('ping-status-text');

        switch (state) {
            case 'ready':
                if (startBtn) {
                    startBtn.disabled = false;
                    startBtn.innerHTML = '<i class="fa-solid fa-play mr-2"></i>Start Ping Test';
                }
                if (stopBtn) stopBtn.disabled = true;
                this.updatePingStatusIndicator('Ready', 'gray', statusIndicator, statusText);
                break;
            case 'running':
                if (startBtn) {
                    startBtn.disabled = true;
                    startBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Testing...';
                }
                if (stopBtn) stopBtn.disabled = false;
                this.updatePingStatusIndicator('Running', 'blue', statusIndicator, statusText);
                break;
            case 'complete':
                if (startBtn) {
                    startBtn.disabled = false;
                    startBtn.innerHTML = '<i class="fa-solid fa-play mr-2"></i>Start Ping Test';
                }
                if (stopBtn) stopBtn.disabled = true;
                this.updatePingStatusIndicator('Complete', 'green', statusIndicator, statusText);
                break;
            case 'stopped':
                if (startBtn) {
                    startBtn.disabled = false;
                    startBtn.innerHTML = '<i class="fa-solid fa-play mr-2"></i>Start Ping Test';
                }
                if (stopBtn) stopBtn.disabled = true;
                this.updatePingStatusIndicator('Stopped', 'yellow', statusIndicator, statusText);
                break;
        }
    }

    updatePingStatusIndicator(text, color, indicator, textElement) {
        if (indicator && textElement) {
            const colorClass = this.getStatusColorClass(color);
            indicator.className = `w-2 h-2 ${colorClass} rounded-full mr-2`;
            textElement.textContent = text;
        }
    }

    clearPingResults() {
        const tbody = document.getElementById('ping-results-tbody');
        if (tbody) {
            tbody.innerHTML = '<tr id="ping-no-data"><td colspan="7" class="py-8 text-center text-neutral-500">Click "Start Ping Test" to begin connectivity testing</td></tr>';
        }

        this.updateElement('ping-packets-sent', '0');
        this.updateElement('ping-packets-received', '0');
        this.updateElement('ping-packet-loss', '0%');
        this.updateElement('ping-avg-rtt', '0ms');
    }

    getPingConfig() {
        return {
            targetHost: document.getElementById('ping-target-host')?.value.trim() || '',
            count: document.getElementById('ping-continuous')?.checked ? 0 : 
                   (parseInt(document.getElementById('ping-count')?.value) || 10),
            packetSize: parseInt(document.getElementById('ping-size')?.value) || 64,
            interval: parseFloat(document.getElementById('ping-interval')?.value) || 1,
            timeout: parseInt(document.getElementById('ping-timeout')?.value) || 5,
            continuous: document.getElementById('ping-continuous')?.checked || false
        };
    }

    copyPingResults() {
        const tbody = document.getElementById('ping-results-tbody');
        if (!tbody) return;

        const rows = tbody.querySelectorAll('tr:not(#ping-no-data)');
        if (rows.length === 0) {
            this.showError('No ping results to copy');
            return;
        }

        let text = 'Seq\tHost\tRTT\tTTL\tSize\tStatus\tTimestamp\n';

        rows.forEach(row => {
            const cells = row.querySelectorAll('td');
            if (cells.length >= 7) {
                const rowData = [
                    cells[0].textContent.trim(),
                    cells[1].textContent.trim(),
                    cells[2].textContent.trim(),
                    cells[3].textContent.trim(),
                    cells[4].textContent.trim(),
                    cells[5].textContent.trim().replace(/\s+/g, ' '),
                    cells[6].textContent.trim()
                ];
                text += rowData.join('\t') + '\n';
            }
        });

        navigator.clipboard.writeText(text).then(() => {
            this.showSuccess('Ping results copied to clipboard');
        }).catch(() => {
            this.showError('Failed to copy results to clipboard');
        });
    }

    // DNS lookup handlers
    setupDNSLookupHandlers() {
        const startBtn = document.getElementById('start-dns-lookup');
        const copyBtn = document.getElementById('copy-dns-results');

        if (startBtn) startBtn.addEventListener('click', () => this.startDnsLookup());
        if (copyBtn) copyBtn.addEventListener('click', () => this.copyDnsResults());

        // Reset UI when DNS popup is opened
        const dnsLookupBtn = document.getElementById('dns-lookup-btn');
        if (dnsLookupBtn) {
            dnsLookupBtn.addEventListener('click', () => {
                this.clearDnsResults();
                this.updateDnsUI('ready');
            });
        }
    }

    async startDnsLookup() {
        try {
            const config = this.getDnsConfig();

            if (!config.domain) {
                this.showError('Please enter a domain or IP address');
                return;
            }

            this.log('Starting DNS lookup:', config);
            this.clearDnsResults();
            this.updateDnsUI('running');

            const response = await this.makeApiRequest('POST', this.endpoints.dns.lookup, config);

            if (response.success) {
                this.displayDnsResults(response);
                this.updateDnsUI('complete');
                this.log('DNS lookup completed successfully');
            } else {
                this.updateDnsUI('error');
                throw new Error(response.message || 'DNS lookup failed');
            }
        } catch (error) {
            this.logError('Error during DNS lookup:', error);
            this.updateDnsUI('error');
            this.showError('Failed to perform DNS lookup: ' + error.message);
        }
    }

    displayDnsResults(response) {
        const resultsSection = document.getElementById('dns-results-section');
        const recordsContainer = document.getElementById('dns-records-container');

        if (resultsSection) {
            resultsSection.classList.remove('hidden');
        }

        // Update status information
        const queryTime = response.queryTime || response.query_time_ms || 0;
        const serverUsed = response.serverUsed || response.dns_server || 'System Default';
        const recordsCount = response.records ? response.records.length : 0;
        const responseCode = response.responseCode || response.response_code || 'NOERROR';

        this.updateElement('dns-query-time', `${queryTime}ms`);
        this.updateElement('dns-server-used', serverUsed);
        this.updateElement('dns-records-count', recordsCount.toString());
        this.updateElement('dns-response-code', responseCode);

        // Display DNS records
        if (recordsContainer && response.records && Array.isArray(response.records)) {
            if (response.records.length === 0) {
                recordsContainer.innerHTML = `
                    <div class="text-center text-neutral-500 py-8">
                        <i class="fa-solid fa-exclamation-triangle text-neutral-400 text-2xl mb-2"></i>
                        <div>No DNS records found for this query</div>
                    </div>
                `;
            } else {
                recordsContainer.innerHTML = `
                    <div class="space-y-3">
                        ${response.records.map(record => this.createDnsRecordElement(record)).join('')}
                    </div>
                `;
            }
        }
    }

    createDnsRecordElement(record) {
        const recordType = record.type || record.record_type || 'UNKNOWN';
        const recordValue = record.value || record.data || record.address || 'No data';
        const ttl = record.ttl || record.time_to_live || 'Unknown';
        const priority = record.priority || record.preference;
        const name = record.name || record.hostname || 'Unknown';

        let additionalInfo = '';

        switch (recordType.toUpperCase()) {
            case 'MX':
                if (priority !== undefined) {
                    additionalInfo = `<div class="text-xs text-neutral-500 mt-1">Priority: ${priority}</div>`;
                }
                break;
            case 'SOA':
                if (record.serial) {
                    additionalInfo = `<div class="text-xs text-neutral-500 mt-1">Serial: ${record.serial}</div>`;
                }
                break;
            case 'SRV':
                if (record.port && record.weight) {
                    additionalInfo = `<div class="text-xs text-neutral-500 mt-1">Port: ${record.port}, Weight: ${record.weight}</div>`;
                }
                break;
        }

        return `
            <div class="bg-neutral-50 border border-neutral-200 rounded-lg p-4">
                <div class="flex items-center justify-between mb-2">
                    <span class="text-sm bg-neutral-100 text-neutral-800 px-2 py-1 rounded font-mono">${recordType}</span>
                    <span class="text-sm text-neutral-500">TTL: ${ttl}</span>
                </div>
                <div class="text-neutral-800 font-medium">${name}</div>
                <div class="text-neutral-600 font-mono text-sm mt-1">${recordValue}</div>
                ${additionalInfo}
            </div>
        `;
    }

    updateDnsUI(state) {
        const lookupBtn = document.getElementById('start-dns-lookup');
        const statusIndicator = document.getElementById('dns-status-indicator');
        const statusText = document.getElementById('dns-status-text');

        switch (state) {
            case 'ready':
                if (lookupBtn) {
                    lookupBtn.disabled = false;
                    lookupBtn.innerHTML = '<i class="fa-solid fa-search mr-2"></i>Lookup';
                }
                this.updateDnsStatusIndicator('Ready', 'gray', statusIndicator, statusText);
                break;
            case 'running':
                if (lookupBtn) {
                    lookupBtn.disabled = true;
                    lookupBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Looking up...';
                }
                this.updateDnsStatusIndicator('Looking up...', 'blue', statusIndicator, statusText);
                break;
            case 'complete':
                if (lookupBtn) {
                    lookupBtn.disabled = false;
                    lookupBtn.innerHTML = '<i class="fa-solid fa-search mr-2"></i>Lookup';
                }
                this.updateDnsStatusIndicator('Complete', 'green', statusIndicator, statusText);
                break;
            case 'error':
                if (lookupBtn) {
                    lookupBtn.disabled = false;
                    lookupBtn.innerHTML = '<i class="fa-solid fa-search mr-2"></i>Lookup';
                }
                this.updateDnsStatusIndicator('Error', 'red', statusIndicator, statusText);
                break;
        }
    }

    updateDnsStatusIndicator(text, color, indicator, textElement) {
        if (indicator && textElement) {
            const colorClass = this.getStatusColorClass(color);
            indicator.className = `w-2 h-2 ${colorClass} rounded-full mr-2`;
            textElement.textContent = text;
        }
    }

    clearDnsResults() {
        const resultsSection = document.getElementById('dns-results-section');
        const recordsContainer = document.getElementById('dns-records-container');

        if (resultsSection) {
            resultsSection.classList.add('hidden');
        }

        if (recordsContainer) {
            recordsContainer.innerHTML = `
                <div class="text-center text-neutral-500 py-8">
                    No DNS records to display
                </div>
            `;
        }

        // Reset status fields
        this.updateElement('dns-query-time', '0ms');
        this.updateElement('dns-server-used', 'System Default');
        this.updateElement('dns-records-count', '0');
        this.updateElement('dns-response-code', 'NOERROR');
    }

    getDnsConfig() {
        return {
            domain: document.getElementById('dns-domain')?.value.trim() || '',
            recordType: document.getElementById('dns-record-type')?.value || 'A',
            dnsServer: document.getElementById('dns-server')?.value.trim() || '',
            timeout: parseInt(document.getElementById('dns-timeout')?.value) || 5,
            trace: document.getElementById('dns-trace')?.checked || false,
            recursive: document.getElementById('dns-recursive')?.checked !== false
        };
    }

    copyDnsResults() {
        const recordsContainer = document.getElementById('dns-records-container');
        if (!recordsContainer) return;

        const domain = document.getElementById('dns-domain')?.value || 'unknown';
        const recordType = document.getElementById('dns-record-type')?.value || 'A';
        const queryTime = document.getElementById('dns-query-time')?.textContent || '0ms';
        const serverUsed = document.getElementById('dns-server-used')?.textContent || 'Unknown';

        let text = `DNS Lookup Results for ${domain} (${recordType})\n`;
        text += `Query Time: ${queryTime}\n`;
        text += `DNS Server: ${serverUsed}\n`;
        text += `Timestamp: ${new Date().toLocaleString()}\n\n`;

        const records = recordsContainer.querySelectorAll('.bg-neutral-50');
        if (records.length === 0) {
            text += 'No records found\n';
        } else {
            records.forEach((record, index) => {
                const type = record.querySelector('.bg-neutral-100')?.textContent?.trim() || 'Unknown';
                const name = record.querySelector('.text-neutral-800')?.textContent?.trim() || 'Unknown';
                const value = record.querySelector('.font-mono')?.textContent?.trim() || 'Unknown';
                const ttl = record.querySelector('.text-neutral-500')?.textContent?.replace('TTL: ', '').trim() || 'Unknown';

                text += `Record ${index + 1}:\n`;
                text += `  Type: ${type}\n`;
                text += `  Name: ${name}\n`;
                text += `  Value: ${value}\n`;
                text += `  TTL: ${ttl}\n\n`;
            });
        }

        navigator.clipboard.writeText(text).then(() => {
            this.showSuccess('DNS results copied to clipboard');
        }).catch(() => {
            this.showError('Failed to copy results to clipboard');
        });
    }

    // Server management handlers
    setupServerManagementHandlers() {
        const refreshBtn = document.getElementById('refresh-server-status-btn');
        const testAllBtn = document.getElementById('test-all-servers-btn');
        const autoRefreshToggle = document.getElementById('auto-refresh-toggle');
        const autoRefreshDelay = document.getElementById('auto-refresh-delay');
        const autoTestToggle = document.getElementById('auto-test-servers');
        const autoTestDelay = document.getElementById('auto-test-delay');

        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.loadServerList());
        }

        if (testAllBtn) {
            testAllBtn.addEventListener('click', () => this.testAllServers());
        }

        if (autoRefreshToggle) {
            autoRefreshToggle.addEventListener('change', () => this.handleAutoRefreshToggle());
        }

        if (autoRefreshDelay) {
            autoRefreshDelay.addEventListener('input', () => this.handleAutoRefreshToggle());
        }

        if (autoTestToggle) {
            autoTestToggle.addEventListener('change', () => this.handleAutoTestToggle());
        }

        if (autoTestDelay) {
            autoTestDelay.addEventListener('input', () => this.handleAutoTestToggle());
        }
    }

    async loadServerList() {
        try {
            this.log('Loading server list...');
            
            const loadingIndicator = this.showLoadingIndicator();

            const response = await this.makeApiRequest('GET', this.endpoints.servers.status);

            if (response.success && response.servers) {
                this.serverStatusData = response.servers;
                this.updateServerStatusDisplay(response.servers);
                this.updateServerStatusCards(response.servers);
                this.updateLastUpdateTime();
                this.log('Server list loaded:', response.servers.length);
            } else {
                throw new Error('Failed to load server list');
            }
        } catch (error) {
            this.logError('Failed to load server list:', error);
            this.showError('Network error while loading server status');
        } finally {
            this.hideLoadingIndicator();
        }
    }

    async testAllServers() {
        if (this.isTestingAllServers) {
            this.log('Server testing already in progress');
            return;
        }

        try {
            this.isTestingAllServers = true;
            const servers = this.serverStatusData || [];
            
            if (servers.length === 0) {
                this.showError('No servers available to test');
                return;
            }

            const testAllBtn = document.getElementById('test-all-servers-btn');
            if (testAllBtn) {
                testAllBtn.disabled = true;
                testAllBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Testing All...';
            }

            this.log(`Starting test of ${servers.length} servers`);

            // Test servers in batches to avoid overwhelming the system
            const batchSize = 3;
            let completed = 0;

            for (let i = 0; i < servers.length; i += batchSize) {
                const batch = servers.slice(i, i + batchSize);
                const promises = batch.map(server => this.testServer(server.id));

                await Promise.allSettled(promises);
                completed = Math.min(i + batchSize, servers.length);

                // Update progress
                const progress = Math.round((completed / servers.length) * 100);
                if (testAllBtn) {
                    testAllBtn.innerHTML = `<i class="fa-solid fa-spinner fa-spin mr-2"></i>Testing... ${progress}%`;
                }

                // Small delay between batches
                if (i + batchSize < servers.length) {
                    await new Promise(resolve => setTimeout(resolve, 1000));
                }
            }

            this.showSuccess(`Server testing completed. Tested ${servers.length} servers.`);
            this.log('All server testing completed');

            // Refresh the display after testing
            await this.loadServerList();

        } catch (error) {
            this.logError('Error during bulk server testing:', error);
            this.showError('Error occurred during server testing');
        } finally {
            this.isTestingAllServers = false;

            const testAllBtn = document.getElementById('test-all-servers-btn');
            if (testAllBtn) {
                testAllBtn.disabled = false;
                testAllBtn.innerHTML = '<i class="fa-solid fa-play mr-2"></i>Test All Servers';
            }
        }
    }

    updateServerStatusCards(servers) {
        const totalServers = servers.length;
        const onlineServers = servers.filter(s => s.status?.toLowerCase() === 'online').length;
        const offlineServers = totalServers - onlineServers;
        const avgPing = totalServers > 0 ? 
            (servers.reduce((sum, s) => sum + (s.ping_ms || 0), 0) / totalServers).toFixed(1) : 0;

        this.updateElement('total-servers', totalServers.toString());
        this.updateElement('online-servers', onlineServers.toString());
        this.updateElement('offline-servers', offlineServers.toString());
        this.updateElement('avg-ping', `${avgPing}ms`);
    }

    updateServerStatusDisplay(servers) {
        const tbody = document.getElementById('server-status-tbody');
        if (!tbody) return;

        if (servers.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="8" class="py-8 text-center text-neutral-500">
                        No servers available
                    </td>
                </tr>
            `;
            return;
        }

        tbody.innerHTML = servers.map(server => `
            <tr class="hover:bg-neutral-50">
                <td class="py-3 px-4 text-sm text-neutral-900">${server.name || server.hostname}</td>
                <td class="py-3 px-4 text-sm text-neutral-600">${server.location || 'Unknown'}</td>
                <td class="py-3 px-4 text-sm text-neutral-600">${server.ip_address || server.hostname}</td>
                <td class="py-3 px-4 text-sm text-neutral-600 text-right">${server.port}</td>
                <td class="py-3 px-4 text-sm text-neutral-600 text-right">${server.load_percent || 0}%</td>
                <td class="py-3 px-4 text-sm text-neutral-600 text-right">${(server.uptime_percent || 0).toFixed(1)}%</td>
                <td class="py-3 px-4 text-center">
                    <span class="px-2 py-1 text-xs rounded-full ${this.getServerStatusBadgeClass(server.status)}">
                        ${server.status}
                    </span>
                </td>
                <td class="py-3 px-4 text-center">
                    <button class="test-server-btn text-neutral-600 hover:text-neutral-800 disabled:text-neutral-400" 
                            onclick="window.networkUtilitySection.testServer('${server.id}')" 
                            data-server-id="${server.id}">
                        <i class="fa-solid fa-play"></i>
                    </button>
                </td>
            </tr>
        `).join('');
    }

    updateLastUpdateTime() {
        const now = new Date();
        this.updateElement('last-update-time', now.toLocaleTimeString());
    }

    handleAutoRefreshToggle() {
        const toggle = document.getElementById('auto-refresh-toggle');
        const delay = document.getElementById('auto-refresh-delay');

        if (toggle?.checked) {
            const interval = Math.max(parseInt(delay?.value) || 15, 5) * 1000;
            this.startServerStatusMonitoring(interval);
        } else {
            this.stopServerStatusMonitoring();
        }
    }

    handleAutoTestToggle() {
        const toggle = document.getElementById('auto-test-servers');
        const delay = document.getElementById('auto-test-delay');

        if (toggle?.checked) {
            const interval = Math.max(parseInt(delay?.value) || 60, 30) * 1000;
            this.startAutoServerTesting(interval);
        } else {
            this.stopAutoServerTesting();
        }
    }

    startAutoServerTesting(interval = 60000) {
        this.stopAutoServerTesting();

        this.autoTestInterval = setInterval(async () => {
            if (!this.isTestingAllServers && this.serverStatusData?.length > 0) {
                this.log('Auto-testing servers...');
                await this.testAllServers();
            }
        }, interval);

        this.log(`Auto server testing started with ${interval/1000}s interval`);
    }

    stopAutoServerTesting() {
        if (this.autoTestInterval) {
            clearInterval(this.autoTestInterval);
            this.autoTestInterval = null;
            this.log('Auto server testing stopped');
        }
    }

    showLoadingIndicator() {
        const tbody = document.getElementById('server-status-tbody');
        if (tbody) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="8" class="py-8 text-center text-neutral-500">
                        <i class="fa-solid fa-spinner fa-spin mr-2"></i>
                        Loading server status...
                    </td>
                </tr>
            `;
        }
    }

    hideLoadingIndicator() {
        // Loading indicator is replaced by actual data in updateServerStatusDisplay
    }

    showSuccess(message) {
        // Use existing success display method or implement notification system
        console.log(`[SUCCESS] ${message}`);
        // You can implement a toast notification system here
    }

    showError(message) {
        // Use existing error display method or implement notification system
        console.error(`[ERROR] ${message}`);
        // You can implement a toast notification system here
    }

    async loadServerList() {
        try {
            const response = await this.makeApiRequest('GET', this.endpoints.servers.status);
            if (response.success && response.servers) {
                this.log('Server list loaded:', response.servers.length);
                // Update server status display
                this.updateServerStatusDisplay(response.servers);
            }
        } catch (error) {
            this.logError('Failed to load server list:', error);
        }
    }

    updateServerStatusDisplay(servers) {
        // Update server status cards and tables
        const statusContainer = document.getElementById('server-status-cards');
        if (statusContainer && servers.length > 0) {
            this.renderServerStatusCards(servers);
        }

        const statusTable = document.getElementById('server-status-table');
        if (statusTable) {
            this.renderServerStatusTable(servers);
        }
    }

    renderServerStatusCards(servers) {
        const container = document.getElementById('server-status-cards');
        if (!container) return;

        const onlineCount = servers.filter(s => s.status === 'Online').length;
        const totalCount = servers.length;
        const avgPing = servers.reduce((sum, s) => sum + (s.ping_ms || 0), 0) / totalCount;

        container.innerHTML = `
            <div class="bg-white p-4 rounded-lg border border-neutral-200">
                <div class="flex items-center justify-between">
                    <div>
                        <p class="text-sm text-neutral-600">Total Servers</p>
                        <p class="text-2xl text-neutral-800">${totalCount}</p>
                    </div>
                    <i class="fa-solid fa-server text-neutral-600"></i>
                </div>
            </div>
            <div class="bg-white p-4 rounded-lg border border-neutral-200">
                <div class="flex items-center justify-between">
                    <div>
                        <p class="text-sm text-neutral-600">Online</p>
                        <p class="text-2xl text-green-600">${onlineCount}</p>
                    </div>
                    <i class="fa-solid fa-check-circle text-green-600"></i>
                </div>
            </div>
            <div class="bg-white p-4 rounded-lg border border-neutral-200">
                <div class="flex items-center justify-between">
                    <div>
                        <p class="text-sm text-neutral-600">Offline</p>
                        <p class="text-2xl text-red-600">${totalCount - onlineCount}</p>
                    </div>
                    <i class="fa-solid fa-times-circle text-red-600"></i>
                </div>
            </div>
            <div class="bg-white p-4 rounded-lg border border-neutral-200">
                <div class="flex items-center justify-between">
                    <div>
                        <p class="text-sm text-neutral-600">Avg Ping</p>
                        <p class="text-2xl text-neutral-800">${avgPing.toFixed(1)}ms</p>
                    </div>
                    <i class="fa-solid fa-clock text-neutral-600"></i>
                </div>
            </div>
        `;
    }

    renderServerStatusTable(servers) {
        const table = document.getElementById('server-status-table');
        if (!table) return;

        const tbody = table.querySelector('tbody');
        if (!tbody) return;

        tbody.innerHTML = servers.map(server => `
            <tr class="hover:bg-neutral-50">
                <td class="py-3 px-4 text-sm text-neutral-900">${server.name || server.hostname}</td>
                <td class="py-3 px-4 text-sm text-neutral-600">${server.location || 'Unknown'}</td>
                <td class="py-3 px-4 text-sm text-neutral-600">${server.ip_address || server.hostname}</td>
                <td class="py-3 px-4 text-sm text-neutral-600 text-right">${server.port}</td>
                <td class="py-3 px-4 text-sm text-neutral-600 text-right">${server.load_percent}%</td>
                <td class="py-3 px-4 text-sm text-neutral-600 text-right">${server.uptime_percent?.toFixed(1) || 0}%</td>
                <td class="py-3 px-4 text-center">
                    <span class="px-2 py-1 text-xs rounded-full ${this.getServerStatusBadgeClass(server.status)}">
                        ${server.status}
                    </span>
                </td>
                <td class="py-3 px-4 text-center">
                    <button class="text-neutral-600 hover:text-neutral-800" onclick="window.networkUtilitySection.testServer('${server.id}')">
                        <i class="fa-solid fa-play"></i>
                    </button>
                </td>
            </tr>
        `).join('');
    }

    getServerStatusBadgeClass(status) {
        switch (status?.toLowerCase()) {
            case 'online':
                return 'bg-green-100 text-green-800';
            case 'offline':
                return 'bg-red-100 text-red-800';
            case 'degraded':
                return 'bg-yellow-100 text-yellow-800';
            default:
                return 'bg-gray-100 text-gray-800';
        }
    }

    async testServer(serverId) {
        try {
            this.log('Testing server:', serverId);
            const response = await this.makeApiRequest('POST', this.endpoints.servers.test, { serverId });

            if (response.success) {
                this.log('Server test result:', response);
                // Refresh server list to show updated status
                await this.loadServerList();
            }
        } catch (error) {
            this.logError('Failed to test server:', error);
        }
    }

    startServerStatusMonitoring() {
        // Auto-refresh server status every 15 seconds
        this.serverStatusInterval = setInterval(() => {
            this.loadServerList();
        }, 15000);
    }

    // Utility methods
    async makeApiRequest(method, url, data = null) {
        try {
            const options = {
                method: method,
                headers: {
                    'Content-Type': 'application/json'
                }
            };

            if (data && (method === 'POST' || method === 'PUT')) {
                options.body = JSON.stringify(data);
            }

            const response = await fetch(url, options);

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${response.statusText}`);
            }

            return await response.json();
        } catch (error) {
            this.logError(`API request failed for ${url}:`, error);
            throw error;
        }
    }

    updateElement(id, content) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = content;
        }
    }

    showError(message) {
        console.error(`${this.logPrefix} ${message}`);
        // You can implement a more sophisticated error display here
        alert(message);
    }

    // Cleanup method
    destroy() {
        // Clear all intervals
        this.updateIntervals.forEach(interval => clearInterval(interval));
        this.updateIntervals.clear();

        if (this.serverStatusInterval) {
            clearInterval(this.serverStatusInterval);
        }

        this.log('NetworkUtilitySection destroyed');
    }
}

// Initialize when DOM is ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        window.networkUtilitySection = new NetworkUtilitySection();
    });
} else {
    window.networkUtilitySection = new NetworkUtilitySection();
}