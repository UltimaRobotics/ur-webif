

class BackupSection {
    constructor() {
        this.apiUrl = window.app?.apiUrl || '/api';
        this.isInitialized = false;
        this.backupInProgress = false;
        this.restoreInProgress = false;
        this.selectedFile = null;
        
        console.log('[BACKUP] BackupSection initialized with API URL:', this.apiUrl);
        this.initializeEventListeners();
    }

    initializeEventListeners() {
        console.log('[BACKUP] Setting up event listeners');
        
        // Initialize all the backup section functionality when DOM is loaded
        if (document.readyState === 'loading') {
            document.addEventListener('DOMContentLoaded', () => this.setupBackupSection());
        } else {
            this.setupBackupSection();
        }
    }

    async setupBackupSection() {
        if (this.isInitialized) return;
        
        console.log('[BACKUP] Setting up backup section components');
        
        try {
            this.setupEncryptionToggle();
            this.setupBackupTypeToggle();
            this.setupEstimateSizeButton();
            this.setupCreateBackupButton();
            this.setupFileInput();
            this.setupRestoreButton();
            this.setupCancelButton();
            this.setupRefreshButton();
            
            // Load initial data
            await this.loadBackupHistory();
            
            this.isInitialized = true;
            console.log('[BACKUP] Backup section setup completed');
            
            // Send page load event
            this.sendEvent('page_load', 'backup-section', { 
                url: window.location.href,
                timestamp: Date.now()
            });
        } catch (error) {
            console.error('[BACKUP] Failed to setup backup section:', error);
        }
    }

    setupEncryptionToggle() {
        const encryptCheckbox = document.getElementById('encrypt-checkbox');
        const encryptionPassword = document.getElementById('encryption-password');
        
        if (encryptCheckbox && encryptionPassword) {
            encryptCheckbox.addEventListener('change', function() {
                if (this.checked) {
                    encryptionPassword.style.display = 'block';
                } else {
                    encryptionPassword.style.display = 'none';
                }
                console.log('[BACKUP] Encryption toggle:', this.checked);
            });
        }
    }

    setupBackupTypeToggle() {
        const fullBackupCheckbox = document.getElementById('full-backup-checkbox');
        const partialBackupCheckbox = document.getElementById('partial-backup-checkbox');

        if (fullBackupCheckbox && partialBackupCheckbox) {
            fullBackupCheckbox.addEventListener('change', function() {
                if (this.checked) {
                    partialBackupCheckbox.checked = false;
                }
                console.log('[BACKUP] Full backup selected:', this.checked);
            });

            partialBackupCheckbox.addEventListener('change', function() {
                if (this.checked) {
                    fullBackupCheckbox.checked = false;
                }
                console.log('[BACKUP] Partial backup selected:', this.checked);
            });
        }
    }

    setupEstimateSizeButton() {
        const estimateSizeBtn = document.getElementById('estimate-size-btn');
        if (estimateSizeBtn) {
            estimateSizeBtn.addEventListener('click', () => this.estimateBackupSize());
        }
    }

    setupCreateBackupButton() {
        const createBackupBtn = document.getElementById('create-backup-btn');
        if (createBackupBtn) {
            createBackupBtn.addEventListener('click', () => this.createBackup());
        }
    }

    setupFileInput() {
        const chooseFileBtn = document.getElementById('choose-file-btn');
        const backupFileInput = document.getElementById('backup-file-input');
        const fileDropZone = document.getElementById('file-drop-zone');
        const removeFileBtn = document.getElementById('remove-file-btn');

        if (chooseFileBtn && backupFileInput) {
            chooseFileBtn.addEventListener('click', function() {
                backupFileInput.click();
            });

            backupFileInput.addEventListener('change', (e) => {
                if (e.target.files.length > 0) {
                    this.handleFileSelection(e.target.files[0]);
                }
            });
        }

        // Drag and drop functionality
        if (fileDropZone) {
            fileDropZone.addEventListener('dragover', (e) => {
                e.preventDefault();
                fileDropZone.classList.add('border-blue-400', 'bg-blue-50');
            });

            fileDropZone.addEventListener('dragleave', (e) => {
                e.preventDefault();
                fileDropZone.classList.remove('border-blue-400', 'bg-blue-50');
            });

            fileDropZone.addEventListener('drop', (e) => {
                e.preventDefault();
                fileDropZone.classList.remove('border-blue-400', 'bg-blue-50');
                
                if (e.dataTransfer.files.length > 0) {
                    this.handleFileSelection(e.dataTransfer.files[0]);
                }
            });
        }

        if (removeFileBtn) {
            removeFileBtn.addEventListener('click', () => {
                this.clearFileSelection();
            });
        }
    }

    setupRestoreButton() {
        const restoreBtn = document.getElementById('restore-btn');
        if (restoreBtn) {
            restoreBtn.addEventListener('click', () => this.restoreBackup());
        }
    }

    setupCancelButton() {
        const cancelBtn = document.getElementById('cancel-restore-btn');
        if (cancelBtn) {
            cancelBtn.addEventListener('click', () => this.cancelRestore());
        }
    }

    setupRefreshButton() {
        const refreshBtn = document.getElementById('refresh-backups-btn');
        if (refreshBtn) {
            refreshBtn.addEventListener('click', () => this.loadBackupHistory());
        }
    }

    handleFileSelection(file) {
        // Validate file type
        const validExtensions = ['.tar.gz', '.bin'];
        const fileName = file.name.toLowerCase();
        const isValid = validExtensions.some(ext => fileName.endsWith(ext));
        
        if (!isValid) {
            this.showError('Invalid file type. Please select a .tar.gz or .bin file.');
            return;
        }

        // Validate file size (10GB limit)
        const maxSize = 10 * 1024 * 1024 * 1024; // 10GB in bytes
        if (file.size > maxSize) {
            this.showError('File size exceeds 10GB limit.');
            return;
        }

        this.selectedFile = file;
        
        // Update UI
        const selectedFileInfo = document.getElementById('selected-file-info');
        const selectedFileName = document.getElementById('selected-file-name');
        const selectedFileSize = document.getElementById('selected-file-size');
        const restoreBtn = document.getElementById('restore-btn');

        if (selectedFileInfo && selectedFileName && selectedFileSize) {
            selectedFileName.textContent = file.name;
            selectedFileSize.textContent = this.formatFileSize(file.size);
            selectedFileInfo.style.display = 'block';
        }

        if (restoreBtn) {
            restoreBtn.disabled = false;
        }

        console.log('[BACKUP] File selected:', file.name, 'Size:', this.formatFileSize(file.size));
    }

    clearFileSelection() {
        this.selectedFile = null;
        
        const selectedFileInfo = document.getElementById('selected-file-info');
        const backupFileInput = document.getElementById('backup-file-input');
        const restoreBtn = document.getElementById('restore-btn');

        if (selectedFileInfo) {
            selectedFileInfo.style.display = 'none';
        }

        if (backupFileInput) {
            backupFileInput.value = '';
        }

        if (restoreBtn) {
            restoreBtn.disabled = true;
        }

        console.log('[BACKUP] File selection cleared');
    }

    async estimateBackupSize() {
        console.log('[BACKUP] Estimating backup size...');
        
        const backupOptions = this.getBackupOptions();
        const backupInformation = document.getElementById('backup-information');
        const estimatedSize = document.getElementById('estimated-size');
        const creationDate = document.getElementById('creation-date');

        if (estimatedSize) {
            estimatedSize.textContent = 'Calculating...';
        }
        if (backupInformation) {
            backupInformation.style.display = 'block';
        }

        try {
            const response = await this.makeRequest('/backup/estimate-size', {
                method: 'POST',
                body: backupOptions
            });

            console.log('[BACKUP] Size estimation response:', response);

            if (response.success) {
                if (estimatedSize) estimatedSize.textContent = response.estimated_size;
                if (creationDate) creationDate.textContent = response.creation_date;
            } else {
                if (estimatedSize) estimatedSize.textContent = 'Error calculating size';
                if (creationDate) creationDate.textContent = 'N/A';
                console.error('[BACKUP] Size estimation failed:', response.error);
            }

            // Send event
            this.sendEvent('button_click', 'estimate-size-btn', { 
                action: 'estimate_backup_size',
                options: backupOptions
            });
        } catch (error) {
            console.error('[BACKUP] Error estimating backup size:', error);
            if (estimatedSize) estimatedSize.textContent = 'Error calculating size';
            if (creationDate) creationDate.textContent = 'N/A';
            this.showError('Failed to estimate backup size: ' + error.message);
        }
    }

    async createBackup() {
        if (this.backupInProgress) {
            console.log('[BACKUP] Backup already in progress');
            return;
        }

        console.log('[BACKUP] Creating backup...');

        const backupOptions = this.getBackupOptions();

        // Validation
        if (!backupOptions.full_backup && !backupOptions.partial_backup) {
            this.showError('Please select either Full Backup or Partial Backup');
            return;
        }

        if (backupOptions.encrypt_backup) {
            const password = document.getElementById('backup-password')?.value;
            const confirmPassword = document.getElementById('confirm-password')?.value;
            
            if (!password || password.length < 12) {
                this.showError('Password must be at least 12 characters long');
                return;
            }
            
            if (password !== confirmPassword) {
                this.showError('Passwords do not match');
                return;
            }
        }

        this.backupInProgress = true;
        this.setButtonState('create-backup-btn', false);

        try {
            const response = await this.makeRequest('/backup/create', {
                method: 'POST',
                body: backupOptions
            });

            console.log('[BACKUP] Create backup response:', response);

            if (response.success) {
                this.showSuccessNotification('Backup created successfully', `${response.filename} has been created`);
                await this.loadBackupHistory(); // Refresh backup list
            } else {
                throw new Error(response.error || 'Failed to create backup');
            }

            // Send event
            this.sendEvent('button_click', 'create-backup-btn', { 
                action: 'create_backup',
                options: backupOptions
            });
        } catch (error) {
            console.error('[BACKUP] Error creating backup:', error);
            this.showError('Failed to create backup: ' + error.message);
        } finally {
            this.backupInProgress = false;
            this.setButtonState('create-backup-btn', true);
        }
    }

    async restoreBackup() {
        if (this.restoreInProgress) {
            console.log('[BACKUP] Restore already in progress');
            return;
        }

        if (!this.selectedFile) {
            this.showError('Please select a backup file first');
            return;
        }

        console.log('[BACKUP] Restoring backup...');

        const restoreOptions = this.getRestoreOptions();

        // Confirmation dialog
        const confirmed = confirm(
            'This will restore your system from the selected backup file. ' +
            'Current configuration will be overwritten. This action cannot be undone. ' +
            'Do you want to continue?'
        );

        if (!confirmed) {
            console.log('[BACKUP] Restore cancelled by user');
            return;
        }

        this.restoreInProgress = true;
        this.setButtonState('restore-btn', false);

        try {
            // Create FormData for file upload
            const formData = new FormData();
            formData.append('backup_file', this.selectedFile);
            formData.append('options', JSON.stringify(restoreOptions));

            const response = await fetch(`${this.apiUrl}/backup/restore`, {
                method: 'POST',
                body: formData
            });

            const data = await response.json();
            console.log('[BACKUP] Restore backup response:', data);

            if (data.success) {
                this.showSuccessNotification('Restore initiated successfully', 'The system will reboot after completion.');
            } else {
                throw new Error(data.error || 'Failed to restore backup');
            }

            // Send event
            this.sendEvent('button_click', 'restore-btn', { 
                action: 'restore_backup',
                filename: this.selectedFile.name,
                options: restoreOptions
            });
        } catch (error) {
            console.error('[BACKUP] Error restoring backup:', error);
            this.showError('Failed to restore backup: ' + error.message);
        } finally {
            this.restoreInProgress = false;
            this.setButtonState('restore-btn', true);
        }
    }

    cancelRestore() {
        console.log('[BACKUP] Cancelling restore...');
        this.clearFileSelection();
        
        // Reset restore options
        const wipeCheckbox = document.getElementById('wipe-before-restore');
        const createBackupCheckbox = document.getElementById('create-backup-before-restore');
        
        if (wipeCheckbox) wipeCheckbox.checked = false;
        if (createBackupCheckbox) createBackupCheckbox.checked = false;
    }

    async loadBackupHistory() {
        console.log('[BACKUP] Loading backup history...');
        
        const backupList = document.getElementById('backup-list');
        if (!backupList) return;

        // Show loading state
        backupList.innerHTML = `
            <div class="text-center py-8 text-neutral-500">
                <i class="fa-solid fa-spinner fa-spin text-4xl mb-3"></i>
                <p>Loading backup history...</p>
            </div>
        `;

        try {
            const response = await this.makeRequest('/backup/list', {
                method: 'GET'
            });

            console.log('[BACKUP] Backup history response:', response);

            if (response.success && response.backups) {
                this.renderBackupHistory(response.backups);
            } else {
                throw new Error(response.error || 'Failed to load backup history');
            }
        } catch (error) {
            console.error('[BACKUP] Error loading backup history:', error);
            backupList.innerHTML = `
                <div class="text-center py-8 text-red-500">
                    <i class="fa-solid fa-exclamation-triangle text-4xl mb-3"></i>
                    <p>Failed to load backup history</p>
                    <p class="text-sm mt-2">${error.message}</p>
                </div>
            `;
        }
    }

    renderBackupHistory(backups) {
        const backupList = document.getElementById('backup-list');
        if (!backupList) return;

        if (!backups || backups.length === 0) {
            backupList.innerHTML = `
                <div class="text-center py-8 text-neutral-500">
                    <i class="fa-solid fa-folder-open text-4xl mb-3"></i>
                    <p>No backups found</p>
                    <p class="text-sm mt-2">Create your first backup to see it here</p>
                </div>
            `;
            return;
        }

        const backupItems = backups.map(backup => `
            <div class="bg-neutral-50 rounded-lg border border-neutral-200 p-4 flex items-center justify-between">
                <div class="flex items-center">
                    <div class="w-10 h-10 bg-blue-100 rounded-lg flex items-center justify-center mr-3">
                        <i class="fa-solid fa-file-archive text-blue-600"></i>
                    </div>
                    <div>
                        <h4 class="text-sm font-medium text-neutral-900">${backup.filename}</h4>
                        <div class="flex items-center space-x-4 text-xs text-neutral-500 mt-1">
                            <span>Size: ${backup.size}</span>
                            <span>Created: ${backup.created_date}</span>
                            <span class="px-2 py-1 rounded-full text-xs ${this.getBackupTypeColor(backup.type)}">${backup.type}</span>
                        </div>
                    </div>
                </div>
                <div class="flex items-center space-x-2">
                    <button class="bg-blue-100 hover:bg-blue-200 text-blue-700 px-3 py-1 rounded-md text-xs" 
                            onclick="window.backupSectionInstance?.downloadBackup('${backup.id}')">
                        <i class="fa-solid fa-download mr-1"></i>
                        Download
                    </button>
                    <button class="bg-red-100 hover:bg-red-200 text-red-700 px-3 py-1 rounded-md text-xs"
                            onclick="window.backupSectionInstance?.deleteBackup('${backup.id}')">
                        <i class="fa-solid fa-trash mr-1"></i>
                        Delete
                    </button>
                </div>
            </div>
        `).join('');

        backupList.innerHTML = backupItems;
    }

    getBackupTypeColor(type) {
        switch (type?.toLowerCase()) {
            case 'full': return 'bg-green-100 text-green-800';
            case 'partial': return 'bg-yellow-100 text-yellow-800';
            case 'encrypted': return 'bg-purple-100 text-purple-800';
            default: return 'bg-gray-100 text-gray-800';
        }
    }

    async downloadBackup(backupId) {
        console.log('[BACKUP] Downloading backup:', backupId);
        
        try {
            const response = await fetch(`${this.apiUrl}/backup/download/${backupId}`, {
                method: 'GET'
            });

            if (!response.ok) {
                throw new Error(`HTTP error! status: ${response.status}`);
            }

            // Create blob and download
            const blob = await response.blob();
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `backup-${backupId}.tar.gz`;
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);

            this.showSuccessNotification('Download started', 'Backup file download has been initiated');

            // Send event
            this.sendEvent('button_click', 'download-backup', { 
                action: 'download_backup',
                backup_id: backupId
            });
        } catch (error) {
            console.error('[BACKUP] Error downloading backup:', error);
            this.showError('Failed to download backup: ' + error.message);
        }
    }

    async deleteBackup(backupId) {
        const confirmed = confirm('Are you sure you want to delete this backup? This action cannot be undone.');
        if (!confirmed) return;

        console.log('[BACKUP] Deleting backup:', backupId);
        
        try {
            const response = await this.makeRequest(`/backup/delete/${backupId}`, {
                method: 'DELETE'
            });

            if (response.success) {
                this.showSuccessNotification('Backup deleted', 'The backup has been permanently removed');
                await this.loadBackupHistory(); // Refresh list
            } else {
                throw new Error(response.error || 'Failed to delete backup');
            }

            // Send event
            this.sendEvent('button_click', 'delete-backup', { 
                action: 'delete_backup',
                backup_id: backupId
            });
        } catch (error) {
            console.error('[BACKUP] Error deleting backup:', error);
            this.showError('Failed to delete backup: ' + error.message);
        }
    }

    getBackupOptions() {
        return {
            full_backup: document.getElementById('full-backup-checkbox')?.checked || false,
            partial_backup: document.getElementById('partial-backup-checkbox')?.checked || false,
            include_credentials: document.getElementById('include-credentials-checkbox')?.checked || false,
            encrypt_backup: document.getElementById('encrypt-checkbox')?.checked || false,
            password: document.getElementById('backup-password')?.value || ''
        };
    }

    getRestoreOptions() {
        return {
            wipe_before_restore: document.getElementById('wipe-before-restore')?.checked || false,
            validate_integrity: document.getElementById('validate-integrity')?.checked || false,
            create_backup_before_restore: document.getElementById('create-backup-before-restore')?.checked || false
        };
    }

    async makeRequest(endpoint, options = {}) {
        const url = `${this.apiUrl}${endpoint}`;
        
        const defaultOptions = {
            method: 'GET',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json'
            }
        };

        // Add session token if available
        if (window.app?.sessionToken) {
            defaultOptions.headers['Authorization'] = `Bearer ${window.app.sessionToken}`;
        }

        const requestOptions = { ...defaultOptions, ...options };

        if (requestOptions.body && typeof requestOptions.body === 'object' && !requestOptions.body instanceof FormData) {
            requestOptions.body = JSON.stringify(requestOptions.body);
        }

        console.log('[BACKUP] Making request to:', url, 'Options:', requestOptions);

        try {
            const response = await fetch(url, requestOptions);
            const data = await response.json();

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}: ${data.message || response.statusText}`);
            }

            return data;
        } catch (error) {
            console.error(`[BACKUP] Request failed for ${endpoint}:`, error);
            throw error;
        }
    }

    showSuccessNotification(title, message) {
        const notification = document.getElementById('success-notification');
        const titleElement = notification?.querySelector('h3');
        const messageElement = document.getElementById('success-message');
        
        if (titleElement) titleElement.textContent = title;
        if (messageElement) messageElement.textContent = message;
        if (notification) {
            notification.style.display = 'flex';
            
            // Auto-hide after 5 seconds
            setTimeout(() => {
                notification.style.display = 'none';
            }, 5000);
        }
    }

    showError(message) {
        const notification = document.getElementById('error-notification');
        const messageElement = document.getElementById('error-message');
        
        if (messageElement) messageElement.textContent = message;
        if (notification) {
            notification.style.display = 'flex';
            
            // Auto-hide after 7 seconds
            setTimeout(() => {
                notification.style.display = 'none';
            }, 7000);
        }
    }

    setButtonState(buttonId, enabled) {
        const button = document.getElementById(buttonId);
        if (button) {
            button.disabled = !enabled;
            
            const span = button.querySelector('span');
            const icon = button.querySelector('i');
            
            if (!enabled) {
                if (icon) icon.className = 'fa-solid fa-spinner fa-spin mr-2';
                if (span) span.textContent = buttonId.includes('create') ? 'Creating...' : 'Processing...';
            } else {
                if (icon) {
                    icon.className = buttonId.includes('create') ? 'fa-solid fa-box-archive mr-2' : 'fa-solid fa-rotate-left mr-2';
                }
                if (span) {
                    span.textContent = buttonId.includes('create') ? 'Create Backup' : 'Restore';
                }
            }
        }
    }

    formatFileSize(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    }

    closeNotification() {
        const notification = document.getElementById('success-notification');
        if (notification) {
            notification.style.display = 'none';
        }
    }

    sendEvent(eventType, elementId, data) {
        // Use the global app's sendEvent if available, otherwise use window.sendEvent
        if (window.app && typeof window.app.sendEvent === 'function') {
            window.app.sendEvent(eventType, elementId, data);
        } else if (typeof window.sendEvent === 'function') {
            window.sendEvent(eventType, elementId, data);
        } else {
            console.log('[BACKUP] Event would be sent:', { eventType, elementId, data });
        }
    }

    loadBackupSection() {
        console.log('[BACKUP] Loading backup section content');
        return this.setupBackupSection();
    }
}

// Export for use in other modules
if (typeof module !== 'undefined' && module.exports) {
    module.exports = BackupSection;
}

// Global instantiation
window.BackupSection = BackupSection;

