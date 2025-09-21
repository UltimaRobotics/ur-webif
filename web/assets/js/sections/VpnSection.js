class VpnSection extends BaseSectionManager {
    constructor() {
        super('VPN');
        this.autoRefreshInterval = 30000; // 30 seconds
        this.init();
    }

    init() {
        this.log('Initializing VPN Section...');
        this.startAutoRefresh(this.autoRefreshInterval);
        this.refreshData();
        // Initialize file upload handlers
        this.initFileUploadHandlers();
        this.initVpnConfigParser();
    }

    async refreshData() {
        if (this.isRefreshing) {
            return;
        }

        this.isRefreshing = true;
        this.log('Auto-refreshing VPN status...');

        try {
            await this.fetchVpnStatus();
        } catch (error) {
            this.error('Failed to refresh VPN data:', error);
        } finally {
            this.isRefreshing = false;
        }
    }

    async fetchVpnStatus() {
        this.log('Fetching VPN status...');
        this.log('Making GET request to /vpn/status');

        try {
            const data = await this.httpManager.get('/vpn/status');
            this.log('Response received:', data);

            if (data && data.success) {
                this.log('VPN status received:', data.vpn);
                this.setState({ 
                    vpn: data.vpn,
                    timestamp: data.timestamp
                });
                this.updateVpnDisplay(data.vpn);
            } else {
                this.error('Invalid VPN status response');
            }
        } catch (error) {
            this.error('VPN status fetch failed:', error);
        }
    }

    updateVpnDisplay(vpnData) {
        // Update connection status
        this.updateElement('vpn-connection-status', vpnData.connection_status || 'Unknown');
        this.updateElement('vpn-current-profile', vpnData.current_profile || 'None');
        this.updateElement('vpn-server-location', vpnData.server_location || 'N/A');
        this.updateElement('vpn-server-ip', vpnData.server_ip || 'N/A');
        this.updateElement('vpn-local-ip', vpnData.local_ip || 'N/A');
        this.updateElement('vpn-public-ip', vpnData.public_ip || 'N/A');
        this.updateElement('vpn-protocol', vpnData.protocol || 'N/A');
        this.updateElement('vpn-encryption', vpnData.encryption || 'N/A');

        // Update data transfer
        this.updateElement('vpn-bytes-sent', this.formatBytes(vpnData.bytes_sent || 0));
        this.updateElement('vpn-bytes-received', this.formatBytes(vpnData.bytes_received || 0));

        // Update connection time
        this.updateElement('vpn-connection-time', this.formatUptime(vpnData.connection_time || 0));

        // Update last connected
        this.updateElement('vpn-last-connected', vpnData.last_connected || 'Never');

        // Update DNS servers
        const dnsServers = Array.isArray(vpnData.dns_servers) ? vpnData.dns_servers.join(', ') : 'None';
        this.updateElement('vpn-dns-servers', dnsServers);

        // Update status indicators
        const isConnected = vpnData.is_connected || false;
        const statusElement = document.getElementById('vpn-status-indicator');
        if (statusElement) {
            statusElement.className = isConnected ? 'status-connected' : 'status-disconnected';
        }

        // Update toggle switches
        this.updateToggle('vpn-auto-connect', vpnData.auto_connect || false);
        this.updateToggle('vpn-kill-switch', vpnData.kill_switch || false);
    }

    updateToggle(id, state) {
        const element = document.getElementById(id);
        if (element) {
            element.checked = state;
        }
    }

    onStateChange(changes) {
        if (changes.vpn) {
            this.log('VPN state changed:', changes.vpn);
        }
        if (changes.timestamp) {
            this.log('State changed:', { timestamp: changes.timestamp });
        }
    }

    initVpnConfigParser() {
        console.log('[VPN] Initializing enhanced VPN config parser...');

        // Get elements
        this.vpnDropZone = document.getElementById('vpn-config-drop-zone');
        this.vpnFileInput = document.getElementById('vpn-config-file-input');
        this.vpnBrowseBtn = document.getElementById('vpn-browse-files-btn');

        // State tracking
        this.currentVpnFile = null;
        this.currentVpnFileContent = null;
        this.parsedVpnData = null;
        this.isConfigParsed = false;

        if (!this.vpnDropZone || !this.vpnFileInput || !this.vpnBrowseBtn) {
            console.warn('[VPN] VPN config parser elements not found');
            return;
        }

        // Initialize create button as disabled
        const createBtn = document.getElementById('create-vpn-profile-btn');
        if (createBtn) {
            createBtn.disabled = true;
            createBtn.classList.add('opacity-50', 'cursor-not-allowed');
        }

        // Bind event handlers
        this.vpnBrowseBtn.addEventListener('click', () => {
            console.log('[VPN] Browse files clicked');
            this.vpnFileInput.click();
        });

        this.vpnDropZone.addEventListener('click', (e) => {
            if (e.target.closest('#vpn-upload-content')) {
                console.log('[VPN] Drop zone clicked');
                this.vpnFileInput.click();
            }
        });

        // Drag and drop handlers
        this.vpnDropZone.addEventListener('dragover', (e) => this.handleVpnDragOver(e));
        this.vpnDropZone.addEventListener('dragleave', (e) => this.handleVpnDragLeave(e));
        this.vpnDropZone.addEventListener('drop', (e) => this.handleVpnFileDrop(e));

        // File input handler
        this.vpnFileInput.addEventListener('change', (e) => this.handleVpnFileSelect(e));

        // Button handlers
        const changeFileBtn = document.getElementById('vpn-change-file-btn');
        const clearFileBtn = document.getElementById('vpn-clear-file-btn');
        const reparseFileBtn = document.getElementById('vpn-reparse-file-btn');
        const tryAgainBtn = document.getElementById('vpn-try-again-btn');
        const manualConfigBtn = document.getElementById('vpn-manual-config-btn');

        if (changeFileBtn) {
            changeFileBtn.addEventListener('click', () => {
                console.log('[VPN] Change file clicked');
                this.vpnFileInput.click();
            });
        }

        if (clearFileBtn) {
            clearFileBtn.addEventListener('click', () => {
                console.log('[VPN] Clear file clicked');
                this.resetVpnUploadState();
            });
        }

        if (reparseFileBtn) {
            reparseFileBtn.addEventListener('click', () => {
                console.log('[VPN] Reparse file clicked');
                if (this.currentVpnFileContent) {
                    this.showVpnParsingState();
                    setTimeout(() => {
                        this.parseVpnConfigFile(this.currentVpnFileContent, 'reparse');
                    }, 300);
                } else {
                    this.resetVpnUploadState();
                }
            });
        }

        if (tryAgainBtn) {
            tryAgainBtn.addEventListener('click', () => {
                console.log('[VPN] Try again clicked');
                this.resetVpnUploadState();
            });
        }

        if (manualConfigBtn) {
            manualConfigBtn.addEventListener('click', () => {
                console.log('[VPN] Manual config clicked');
                this.resetVpnUploadState();
                // Focus on first form field for manual configuration
                const profileNameField = document.querySelector('input[placeholder*="profile name"], input[placeholder*="Profile Name"]');
                if (profileNameField) {
                    profileNameField.focus();
                }
            });
        }

        console.log('[VPN] VPN config parser initialized successfully');
        
        // Set up form submission handler
        const vpnForm = document.getElementById('vpn-profile-form');
        const createBtn = document.getElementById('create-vpn-profile-btn');
        
        if (vpnForm) {
            vpnForm.addEventListener('submit', (e) => {
                e.preventDefault();
                this.handleVpnProfileCreation();
            });
        }
        
        if (createBtn) {
            createBtn.addEventListener('click', (e) => {
                e.preventDefault();
                this.handleVpnProfileCreation();
            });
        }
    }

    handleVpnDragOver(e) {
        e.preventDefault();
        e.stopPropagation();
        this.vpnDropZone.classList.add('border-neutral-400', 'bg-neutral-50');
    }

    handleVpnDragLeave(e) {
        e.preventDefault();
        e.stopPropagation();
        this.vpnDropZone.classList.remove('border-neutral-400', 'bg-neutral-50');
    }

    handleVpnFileDrop(e) {
        e.preventDefault();
        e.stopPropagation();
        this.vpnDropZone.classList.remove('border-neutral-400', 'bg-neutral-50');

        const files = e.dataTransfer.files;
        if (files.length > 0) {
            this.processVpnSelectedFile(files[0]);
        }
    }

    handleVpnFileSelect(e) {
        const files = e.target.files;
        if (files.length > 0) {
            this.processVpnSelectedFile(files[0]);
        }
    }

    processVpnSelectedFile(file) {
        console.log('[VPN] File selected:', file.name, file.size);

        // Validate file
        if (!this.validateVpnFile(file)) {
            return;
        }

        this.currentVpnFile = file;

        // Show file selected state
        this.showVpnFileSelectedState(file);

        // Automatically start parsing after a brief delay
        setTimeout(() => {
            this.startVpnParsingProcess(file);
        }, 500);

        this.sendVpnEvent('file_select', 'vpn-config-drop-zone', {
            filename: file.name,
            filesize: file.size
        });
    }

    validateVpnFile(file) {
        console.log('[VPN] Validating file:', file.name, 'size:', file.size);

        const maxSize = 5 * 1024 * 1024; // 5MB
        const allowedExtensions = ['.ovpn', '.conf', '.wg', '.config'];

        if (file.size === 0) {
            this.showVpnErrorState('File is empty', ['Please select a valid configuration file']);
            return false;
        }

        if (file.size > maxSize) {
            this.showVpnErrorState('File size exceeds 5MB limit', [
                'Maximum file size is 5MB',
                'Please use a smaller configuration file'
            ]);
            return false;
        }

        const extension = '.' + file.name.split('.').pop().toLowerCase();
        if (!allowedExtensions.includes(extension)) {
            this.showVpnErrorState('Unsupported file format', [
                `File extension '${extension}' is not supported`,
                'Please use .ovpn, .conf, .wg, or .config files',
                'Or try configuring the VPN profile manually'
            ]);
            return false;
        }

        console.log('[VPN] File validation passed');
        return true;
    }

    showVpnFileSelectedState(file) {
        const uploadContent = document.getElementById('vpn-upload-content');
        const fileSelectedContent = document.getElementById('vpn-file-selected-content');
        const fileName = document.getElementById('vpn-selected-file-name');
        const fileSize = document.getElementById('vpn-selected-file-size');

        // Update file details
        if (fileName) fileName.textContent = file.name;
        if (fileSize) fileSize.textContent = this.formatFileSize(file.size);

        // Show file selected state
        if (uploadContent) uploadContent.classList.add('hidden');
        if (fileSelectedContent) fileSelectedContent.classList.remove('hidden');

        // Hide other states
        this.hideAllVpnParsingStates();
    }

    startVpnParsingProcess(file) {
        console.log('[VPN] Starting parsing process for file:', file.name);

        this.sendVpnEvent('config_parsing_started', 'vpn-config-drop-zone', {
            filename: file.name,
            filesize: file.size
        });

        // Show parsing state
        this.showVpnParsingState();

        // Read file content
        const reader = new FileReader();
        reader.onload = (e) => {
            this.currentVpnFileContent = e.target.result;
            console.log('[VPN] File content loaded, length:', this.currentVpnFileContent.length);

            // Validate content is not empty
            if (!this.currentVpnFileContent || this.currentVpnFileContent.trim().length === 0) {
                console.error('[VPN] Empty file content detected');
                this.showVpnErrorState('Empty configuration file', [
                    'The selected file appears to be empty',
                    'Please select a valid VPN configuration file'
                ]);
                return;
            }

            this.sendVpnEvent('config_file_read', 'vpn-config-drop-zone', {
                filename: file.name,
                content_length: this.currentVpnFileContent.length
            });

            // Start parsing process with a small delay for better UX
            setTimeout(() => {
                this.parseVpnConfigFile(this.currentVpnFileContent, file.name);
            }, 300);
        };

        reader.onerror = (error) => {
            console.error('[VPN] File read error:', error);
            this.sendVpnEvent('config_file_read_error', 'vpn-config-drop-zone', {
                filename: file.name,
                error: 'FileReader error'
            });

            this.showVpnErrorState('Failed to read file content', [
                'Unable to read the selected file',
                'File may be corrupted, in use, or access denied',
                'Please try selecting the file again'
            ]);
        };

        reader.readAsText(file);
    }

    showVpnParsingState() {
        const fileSelectedContent = document.getElementById('vpn-file-selected-content');
        const parsingContent = document.getElementById('vpn-parsing-content');

        if (fileSelectedContent) fileSelectedContent.classList.add('hidden');
        if (parsingContent) parsingContent.classList.remove('hidden');

        // Hide other states
        const successContent = document.getElementById('vpn-success-content');
        const errorContent = document.getElementById('vpn-error-content');
        if (successContent) successContent.classList.add('hidden');
        if (errorContent) errorContent.classList.add('hidden');

        // Animate parsing steps
        this.animateVpnParsingSteps();
    }

    animateVpnParsingSteps() {
        const steps = [
            'Detecting protocol...',
            'Validating configuration...',
            'Extracting parameters...',
            'Processing data...'
        ];

        let currentStep = 0;
        const stepElement = document.getElementById('vpn-parsing-step');

        const interval = setInterval(() => {
            if (currentStep < steps.length) {
                if (stepElement) stepElement.textContent = steps[currentStep];
                currentStep++;
            } else {
                clearInterval(interval);
            }
        }, 800);

        // Store interval for cleanup
        this.vpnParsingInterval = interval;
    }

    async parseVpnConfigFile(content, filename) {
        try {
            console.log('[VPN] Starting config file parsing for:', filename);
            console.log('[VPN] Content preview:', content.substring(0, 100) + (content.length > 100 ? '...' : ''));

            // Validate content before parsing
            if (!content || content.trim().length < 10) {
                throw new Error('Configuration file is too short or empty');
            }

            // Update parsing step
            const stepElement = document.getElementById('vpn-parsing-step');
            if (stepElement) stepElement.textContent = 'Analyzing configuration format...';

            // Try to detect protocol and parse
            const parseResult = await this.attemptVpnParsing(content);

            if (parseResult.success && parseResult.profile_data) {
                console.log('[VPN] Parsing successful:', parseResult);

                // Set default profile name if not provided
                if (!parseResult.profile_data.name || parseResult.profile_data.name.trim() === '') {
                    parseResult.profile_data.name = this.generateVpnProfileName(filename, parseResult.protocol_detected);
                }

                // Ensure required fields have default values
                if (!parseResult.profile_data.server) {
                    parseResult.profile_data.server = '';
                }
                if (!parseResult.profile_data.port) {
                    parseResult.profile_data.port = this.getDefaultVpnPort(parseResult.protocol_detected);
                }
                if (!parseResult.profile_data.protocol) {
                    parseResult.profile_data.protocol = parseResult.protocol_detected;
                }

                this.parsedVpnData = parseResult;

                // Wait a moment before showing success for better UX
                setTimeout(() => {
                    this.showVpnSuccessState(parseResult);
                }, 500);

            } else {
                console.error('[VPN] Parsing failed:', parseResult);
                const errorType = parseResult.error_type || null;
                const errorList = this.generateVpnErrorList(parseResult, content, errorType);
                this.showVpnErrorState(
                    parseResult.error || 'Unable to parse configuration file', 
                    errorList, 
                    errorType
                );
            }

        } catch (error) {
            console.error('[VPN] Parsing error:', error);

            this.sendVpnEvent('config_parsing_exception', 'vpn-config-drop-zone', {
                filename: filename,
                error: error.message,
                content_length: content.length
            });

            this.showVpnErrorState('Failed to parse configuration file', [
                error.message || 'Unknown error occurred',
                'Please check if the file is a valid VPN configuration',
                'Supported formats: OpenVPN (.ovpn), IKEv2 (.conf), WireGuard (.wg, .config)'
            ]);
        }
    }

    async attemptVpnParsing(content) {
        console.log('[VPN] Starting centralized VPN parsing with content length:', content.length);

        try {
            const requestBody = {
                config_content: content
            };

            console.log('[VPN] Request payload:', {
                content_length: content.length,
                content_preview: content.substring(0, 200) + (content.length > 200 ? '...' : '')
            });

            // Update parsing step display
            const stepElement = document.getElementById('vpn-parsing-step');
            if (stepElement) stepElement.textContent = 'Analyzing configuration format...';

            const response = await fetch('/api/vpn-parser/parse', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Accept': 'application/json'
                },
                body: JSON.stringify(requestBody)
            });

            console.log('[VPN] Centralized parser response status:', response.status, response.statusText);

            if (!response.ok) {
                const errorText = await response.text();
                console.warn('[VPN] Centralized parser failed with status:', response.status, errorText);
                return {
                    success: false,
                    error: `HTTP ${response.status}: ${errorText}`,
                    error_type: 'NETWORK_ERROR'
                };
            }

            const result = await response.json();
            console.log('[VPN] Centralized parser result:', result);

            if (result.success && result.profile_data) {
                console.log('[VPN] Successfully parsed with centralized parser');
                if (stepElement) stepElement.textContent = `Successfully parsed ${result.protocol_detected} configuration!`;

                this.sendVpnEvent('config_parsed_success', 'vpn-config-drop-zone', {
                    parser_used: 'Centralized Parser Manager',
                    protocol_detected: result.protocol_detected,
                    profile_name: result.profile_data.name || 'Unknown'
                });

                return result;
            } else {
                console.error('[VPN] Centralized parser failed:', result);
                this.sendVpnEvent('config_parsing_failed', 'vpn-config-drop-zone', {
                    error: result.error || 'Unknown error',
                    error_type: result.error_type || 'PARSING_FAILED',
                    content_length: content.length
                });

                return result;
            }

        } catch (error) {
            console.error('[VPN] Centralized parser exception:', error);
            
            this.sendVpnEvent('config_parsing_exception', 'vpn-config-drop-zone', {
                error: error.message,
                content_length: content.length
            });

            return {
                success: false,
                error: 'Network error while parsing configuration: ' + error.message,
                error_type: 'NETWORK_EXCEPTION',
                details: 'Failed to communicate with the VPN parser service'
            };
        }
    }

    showVpnSuccessState(result) {
        // Clear any running animations
        if (this.vpnParsingInterval) {
            clearInterval(this.vpnParsingInterval);
        }

        const parsingContent = document.getElementById('vpn-parsing-content');
        const successContent = document.getElementById('vpn-success-content');
        const successDetails = document.getElementById('vpn-success-details');

        // Update success details
        if (successDetails) {
            successDetails.textContent = `${result.protocol_detected} configuration detected and processed successfully`;
        }

        // Update detected values
        const profile = result.profile_data;
        const detectedProtocol = document.getElementById('vpn-detected-protocol');
        const detectedServer = document.getElementById('vpn-detected-server');
        const detectedPort = document.getElementById('vpn-detected-port');
        const detectedAuth = document.getElementById('vpn-detected-auth');

        if (detectedProtocol) detectedProtocol.textContent = profile.protocol || result.protocol_detected;
        if (detectedServer) detectedServer.textContent = profile.server || 'Not specified';
        if (detectedPort) detectedPort.textContent = profile.port || 'Default';
        if (detectedAuth) detectedAuth.textContent = profile.auth_method || 'Unknown';

        // Show success state
        if (parsingContent) parsingContent.classList.add('hidden');
        if (successContent) successContent.classList.remove('hidden');

        const errorContent = document.getElementById('vpn-error-content');
        if (errorContent) errorContent.classList.add('hidden');

        // Auto-fill form fields and show protocol-specific details
        this.populateVpnFormFields(profile, result.protocol_detected);

        console.log('[VPN] Success state displayed with parsed data:', profile);
    }

    showVpnErrorState(message, errorList = [], errorType = null) {
        console.error('[VPN] Showing error state:', message, errorList, 'Type:', errorType);

        // Clear any running animations
        if (this.vpnParsingInterval) {
            clearInterval(this.vpnParsingInterval);
        }

        const parsingContent = document.getElementById('vpn-parsing-content');
        const errorContent = document.getElementById('vpn-error-content');
        const errorMessage = document.getElementById('vpn-error-message');
        const errorListElement = document.getElementById('vpn-error-list');

        // Update error message based on error type
        let displayMessage = message;
        if (errorType) {
            switch (errorType) {
                case 'PROTOCOL_IDENTIFICATION_FAILED':
                    displayMessage = 'VPN Protocol Not Recognized';
                    break;
                case 'PARSING_FAILED':
                    displayMessage = 'Configuration Parsing Failed';
                    break;
                case 'MISSING_PROFILE_DATA':
                    displayMessage = 'Invalid Configuration Structure';
                    break;
                case 'PARSING_EXCEPTION':
                    displayMessage = 'Configuration Processing Error';
                    break;
                default:
                    displayMessage = message;
            }
        }

        if (errorMessage) errorMessage.textContent = displayMessage;

        // Update error list with specific error details
        if (errorListElement) {
            errorListElement.innerHTML = '';

            // Generate error-type specific error lists
            if (errorList.length === 0) {
                switch (errorType) {
                    case 'PROTOCOL_IDENTIFICATION_FAILED':
                        errorList = [
                            'The uploaded file does not match any supported VPN format',
                            'Supported formats: OpenVPN (.ovpn), IKEv2 (.conf), WireGuard (.wg/.config)',
                            'Ensure the file contains valid VPN configuration directives',
                            'Try uploading a different configuration file'
                        ];
                        break;
                    case 'PARSING_FAILED':
                        errorList = [
                            'The VPN configuration file structure is invalid or corrupted',
                            'Missing required configuration parameters',
                            'Try re-downloading the configuration from your VPN provider',
                            'Verify the file is complete and not truncated'
                        ];
                        break;
                    default:
                        errorList = [
                            'Configuration file could not be processed',
                            'Please ensure the file is a valid VPN configuration',
                            'Try using a different file or configure manually'
                        ];
                }
            }

            errorList.forEach(error => {
                const li = document.createElement('li');
                li.textContent = `• ${error}`;
                errorListElement.appendChild(li);
            });
        }

        // Show error state
        if (parsingContent) parsingContent.classList.add('hidden');
        if (errorContent) errorContent.classList.remove('hidden');

        const successContent = document.getElementById('vpn-success-content');
        if (successContent) successContent.classList.add('hidden');

        console.log('[VPN] Error state displayed with type:', errorType);
    }

    hideAllVpnParsingStates() {
        const parsingContent = document.getElementById('vpn-parsing-content');
        const successContent = document.getElementById('vpn-success-content');
        const errorContent = document.getElementById('vpn-error-content');

        if (parsingContent) parsingContent.classList.add('hidden');
        if (successContent) successContent.classList.add('hidden');
        if (errorContent) errorContent.classList.add('hidden');
    }

    resetVpnUploadState() {
        const uploadContent = document.getElementById('vpn-upload-content');
        const fileSelectedContent = document.getElementById('vpn-file-selected-content');

        if (uploadContent) uploadContent.classList.remove('hidden');
        if (fileSelectedContent) fileSelectedContent.classList.add('hidden');

        this.hideAllVpnParsingStates();

        // Clear file input
        if (this.vpnFileInput) this.vpnFileInput.value = '';
        this.currentVpnFile = null;
        this.currentVpnFileContent = null;
        this.parsedVpnData = null;
    }

    populateVpnFormFields(profileData, protocolType) {
        console.log('[VPN] Populating form fields with:', profileData, 'Protocol:', protocolType);

        // Map the correct field IDs based on the updated HTML structure
        const fieldsMap = {
            'server-address': profileData.server,
            'port': profileData.port,
            'protocol': profileData.protocol,
            'username': profileData.username,
            'password': profileData.password || '',
            'auto-connect-profile': profileData.auto_connect || false
        };

        Object.entries(fieldsMap).forEach(([fieldId, value]) => {
            const field = document.getElementById(fieldId);
            if (field && value !== undefined && value !== null) {
                if (field.type === 'checkbox') {
                    field.checked = Boolean(value);
                } else {
                    field.value = value;
                    // Add visual feedback for populated fields
                    field.classList.add('bg-green-50', 'border-green-300');
                    setTimeout(() => {
                        field.classList.remove('bg-green-50', 'border-green-300');
                    }, 2000);
                }
                console.log(`[VPN] Populated field ${fieldId} with value:`, value);
            }
        });

        // Generate profile name if not already set
        const profileNameField = document.getElementById('profile-name');
        if (profileNameField && !profileNameField.value.trim() && profileData.name) {
            profileNameField.value = profileData.name;
            profileNameField.classList.add('bg-blue-50', 'border-blue-300');
            setTimeout(() => {
                profileNameField.classList.remove('bg-blue-50', 'border-blue-300');
            }, 2000);
        }

        // Show protocol-specific details
        this.displayProtocolSpecificDetails(profileData, protocolType);

        // Update authentication section based on protocol
        this.updateAuthenticationSection(profileData, protocolType);

        // Enable create button and update its appearance
        const createBtn = document.getElementById('create-vpn-profile-btn');
        if (createBtn) {
            createBtn.disabled = false;
            createBtn.classList.remove('opacity-50', 'cursor-not-allowed');
            createBtn.classList.add('shadow-lg');
            this.isConfigParsed = true;
        }

        // Store parsed data for form submission
        this.parsedVpnData = {
            ...this.parsedVpnData,
            profile_data: profileData,
            config_content: this.currentVpnFileContent,
            config_filename: this.currentVpnFile ? this.currentVpnFile.name : null
        };

        console.log('[VPN] Form fields population completed, create button enabled');
    }

    displayProtocolSpecificDetails(profileData, protocolType) {
        console.log('[VPN] Displaying protocol-specific details for:', protocolType);

        // Hide all protocol-specific sections first
        const protocolSections = ['openvpn-details', 'ikev2-details', 'wireguard-details'];
        protocolSections.forEach(sectionId => {
            const section = document.getElementById(sectionId);
            if (section) section.classList.add('hidden');
        });

        // Show the main protocol-specific container
        const protocolSpecificContainer = document.getElementById('protocol-specific-details');
        if (protocolSpecificContainer) {
            protocolSpecificContainer.classList.remove('hidden');
        }

        // Show and populate the appropriate section
        switch (protocolType) {
            case 'OpenVPN':
                this.displayOpenVPNDetails(profileData);
                break;
            case 'IKEv2':
                this.displayIKEv2Details(profileData);
                break;
            case 'WireGuard':
                this.displayWireGuardDetails(profileData);
                break;
        }
    }

    displayOpenVPNDetails(profileData) {
        const section = document.getElementById('openvpn-details');
        if (!section) return;

        section.classList.remove('hidden');

        const openvpnData = profileData.openvpn_specific || {};
        
        this.updateElement('openvpn-device-type', openvpnData.device_type || 'tun');
        this.updateElement('openvpn-auth-algo', openvpnData.auth_algorithm || 'SHA1');
        this.updateElement('openvpn-tls-auth', openvpnData.tls_auth ? 'Enabled' : 'Disabled');
        this.updateElement('openvpn-certificates', openvpnData.has_certificates ? 'Present' : 'Not found');
    }

    displayIKEv2Details(profileData) {
        const section = document.getElementById('ikev2-details');
        if (!section) return;

        section.classList.remove('hidden');

        const ikev2Data = profileData.ikev2_specific || {};
        
        this.updateElement('ikev2-conn-type', ikev2Data.connection_type || 'tunnel');
        this.updateElement('ikev2-auto', ikev2Data.auto_setting || 'start');
        this.updateElement('ikev2-ike-params', ikev2Data.ike_parameters || 'Default');
        this.updateElement('ikev2-esp-params', ikev2Data.esp_parameters || 'Default');
        this.updateElement('ikev2-left-id', ikev2Data.left_id || 'Not specified');
        this.updateElement('ikev2-right-id', ikev2Data.right_id || 'Not specified');
    }

    displayWireGuardDetails(profileData) {
        const section = document.getElementById('wireguard-details');
        if (!section) return;

        section.classList.remove('hidden');

        const wireguardData = profileData.wireguard_specific || {};
        
        this.updateElement('wireguard-interface-addr', wireguardData.interface_address || 'Auto-assigned');
        this.updateElement('wireguard-listen-port', wireguardData.listen_port || '51820');
        this.updateElement('wireguard-allowed-ips', wireguardData.allowed_ips || '0.0.0.0/0');
        this.updateElement('wireguard-dns', wireguardData.dns_servers || 'Not specified');
        this.updateElement('wireguard-psk', wireguardData.preshared_key_present ? 'Present' : 'Not used');
        this.updateElement('wireguard-keepalive', wireguardData.persistent_keepalive || 'Disabled');
    }

    updateAuthenticationSection(profileData, protocolType) {
        const authSection = document.getElementById('auth-section');
        if (!authSection) return;

        const usernameField = document.getElementById('username');
        const passwordField = document.getElementById('password');
        const usernameContainer = usernameField?.closest('div');
        const passwordContainer = passwordField?.closest('div');

        // Handle WireGuard differently (key-based, no username/password)
        if (protocolType === 'WireGuard') {
            if (usernameContainer) usernameContainer.style.display = 'none';
            if (passwordContainer) passwordContainer.style.display = 'none';
            
            // Update help text
            const helpTexts = authSection.querySelectorAll('.text-xs');
            helpTexts.forEach(text => {
                text.textContent = 'WireGuard uses key-based authentication - no username/password required';
                text.classList.add('text-blue-600');
            });
        } else {
            if (usernameContainer) usernameContainer.style.display = 'block';
            if (passwordContainer) passwordContainer.style.display = 'block';
        }
    }

    generateVpnProfileName(filename, protocol) {
        const baseName = filename.replace(/\.(ovpn|conf|wg|config)$/i, '');
        return `${baseName} (${protocol})`;
    }

    getDefaultVpnPort(protocol) {
        switch (protocol) {
            case 'OpenVPN':
                return 1194;
            case 'IKEv2':
                return 500;
            case 'WireGuard':
                return 51820;
            default:
                return 1194;
        }
    }

    generateVpnErrorList(parseResult, content, errorType) {
        const errors = [];

        // Return empty array to let error state handler generate type-specific errors
        if (errorType) {
            return errors;
        }

        // Legacy error handling for backwards compatibility
        if (parseResult.error && parseResult.error.includes('Unable to parse')) {
            errors.push('Unsupported or corrupted configuration format');
        }

        // Check for common missing elements
        if (!content.includes('remote') && !content.includes('Endpoint') && !content.includes('left=')) {
            errors.push('No server endpoint found in configuration');
        }

        if (content.length < 50) {
            errors.push('Configuration file appears to be too short or empty');
        }

        if (errors.length === 0) {
            errors.push('Unknown configuration format');
            errors.push('Please ensure the file is a valid VPN configuration');
        }

        return errors;
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    sendVpnEvent(type, elementId, data) {
        const event = {
            type: type,
            element: elementId,
            timestamp: Date.now(),
            section: 'vpn',
            ...data
        };

        console.log('[VPN] Event sent:', event);
    }

    async handleVpnProfileCreation() {
        console.log('[VPN] Handling VPN profile creation...');

        const createBtn = document.getElementById('create-vpn-profile-btn');
        if (!createBtn) return;

        // Disable button during submission
        createBtn.disabled = true;
        const originalText = createBtn.innerHTML;
        createBtn.innerHTML = '<i class="fa-solid fa-spinner fa-spin mr-2"></i>Creating...';

        try {
            // Collect form data
            const formData = this.collectVpnFormData();
            
            if (!formData) {
                throw new Error('Failed to collect form data');
            }

            // Add configuration file data if available
            if (this.parsedVpnData && this.currentVpnFileContent) {
                formData.config_file_content = this.currentVpnFileContent;
                formData.config_file_name = this.currentVpnFile ? this.currentVpnFile.name : 'config.conf';
                formData.config_parsed_from = this.parsedVpnData.protocol_detected || 'manual';
                formData.parser_used = this.parsedVpnData.parser_used || 'manual';
            }

            console.log('[VPN] Submitting VPN profile:', formData);

            // Submit to backend
            const response = await fetch('/api/vpn/profiles', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                    'Accept': 'application/json'
                },
                body: JSON.stringify(formData)
            });

            const result = await response.json();

            if (result.success) {
                console.log('[VPN] Profile created successfully:', result);
                
                // Show success message
                this.showSuccessMessage('VPN profile created successfully!');
                
                // Close popup and refresh profiles
                this.closeVpnCreatePopup();
                await this.fetchVpnProfiles();
                
                // Send success event
                this.sendVpnEvent('profile_created', 'create-vpn-profile-btn', {
                    profile_id: result.profile.id,
                    profile_name: result.profile.name,
                    config_stored: result.config_stored || false
                });
                
            } else {
                throw new Error(result.error || 'Failed to create VPN profile');
            }

        } catch (error) {
            console.error('[VPN] Profile creation failed:', error);
            this.showErrorMessage('Failed to create VPN profile: ' + error.message);
            
            this.sendVpnEvent('profile_creation_failed', 'create-vpn-profile-btn', {
                error: error.message
            });
        } finally {
            // Re-enable button
            createBtn.disabled = false;
            createBtn.innerHTML = originalText;
        }
    }

    collectVpnFormData() {
        try {
            const profileName = document.getElementById('profile-name')?.value?.trim();
            const serverAddress = document.getElementById('server-address')?.value?.trim();
            const protocol = document.getElementById('protocol')?.value;
            const port = document.getElementById('port')?.value;
            const username = document.getElementById('username')?.value?.trim();
            const password = document.getElementById('password')?.value;
            const autoConnect = document.getElementById('auto-connect-profile')?.checked || false;

            if (!profileName) {
                throw new Error('Profile name is required');
            }
            
            if (!serverAddress) {
                throw new Error('Server address is required');
            }

            return {
                name: profileName,
                server: serverAddress,
                protocol: protocol || 'OpenVPN',
                port: port ? parseInt(port) : (protocol === 'WireGuard' ? 51820 : protocol === 'IKEv2' ? 500 : 1194),
                username: username || '',
                password: password || '',
                auto_connect: autoConnect,
                auth_method: username ? 'Password' : 'Certificate',
                encryption: 'AES-256'
            };
        } catch (error) {
            console.error('[VPN] Error collecting form data:', error);
            this.showErrorMessage(error.message);
            return null;
        }
    }

    closeVpnCreatePopup() {
        const popup = document.getElementById('vpn-create-popup');
        if (popup) {
            popup.style.opacity = '0';
            popup.style.visibility = 'hidden';
            
            setTimeout(() => {
                popup.style.display = 'none';
                this.resetVpnUploadState();
            }, 300);
        }
    }

    showSuccessMessage(message) {
        // Implementation for showing success messages
        console.log('[VPN] Success:', message);
        // You can enhance this with a toast notification system
    }

    showErrorMessage(message) {
        // Implementation for showing error messages
        console.error('[VPN] Error:', message);
        // You can enhance this with a toast notification system
    }

    destroy() {
        console.log('[VPN] VPN section destroyed');

        // Clear any running intervals
        if (this.vpnParsingInterval) {
            clearInterval(this.vpnParsingInterval);
        }

        super.destroy();
    }
}

// Initialize when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    if (window.location.pathname.includes('vpn-configuration-section.html')) {
        window.vpnSection = new VpnSection();
    }
});