// Unified handler for all control tab Save buttons
document.addEventListener('DOMContentLoaded', function() {
    const saveMap = [
        // Calibration
        {id: 'calibrate-save', fields: ['calibrate-offset', 'calibrate-scale']},
        // Sensors
        {id: 'analog-save', fields: ['analog-value']},
        {id: 'digital-save', fields: ['digital-state', 'digital-pin']},
        {id: 'sensor-temp-save', fields: ['sensor-temp']},
        {id: 'sensor-ph-save', fields: ['sensor-ph']},
        {id: 'sensor-od-save', fields: ['sensor-od']},
        {id: 'sensor-flow-save', fields: ['sensor-flow']},
        {id: 'sensor-pressure-save', fields: ['sensor-pressure']},
        {id: 'sensor-power-save', fields: ['sensor-power']},
        // Outputs
        {id: 'analog-output-save', fields: ['analog-output']},
        {id: 'digital-output-save', fields: ['digital-output-state', 'digital-output-pin']},
        // Devices
        {id: 'stepper-save', fields: ['stepper-position', 'stepper-speed']},
        {id: 'motor-save', fields: ['motor-rpm', 'motor-direction']},
        // Communication
        {id: 'rs232-save', fields: ['rs232-port', 'rs232-baud']},
        {id: 'rs485-save', fields: ['rs485-port', 'rs485-baud']}
    ];
    saveMap.forEach(item => {
        const btn = document.getElementById(item.id);
        if (btn) {
            btn.addEventListener('click', function() {
                const data = {};
                item.fields.forEach(f => {
                    const el = document.getElementById(f);
                    if (el) {
                        if (el.type === 'checkbox') {
                            data[f] = el.checked;
                        } else if (el.tagName === 'SELECT') {
                            data[f] = el.value;
                        } else {
                            data[f] = el.value;
                        }
                    }
                });
                console.log(`[${item.id}]`, data);
                showToast('info', 'Saved', `Settings for ${item.id.replace('-save','').replace(/-/g,' ')} updated.`);
            });
        }
    });
});

// ============================================================================
// REUSABLE SVG ICONS
// ============================================================================

/**
 * Dashboard icon (monitor/display)
 * Used to indicate if item is shown on dashboard
 */
function getDashboardIconSVG() {
    return `<svg viewBox="0 0 24 24"><path d="M21,16V4H3V16H21M21,2A2,2 0 0,1 23,4V16A2,2 0 0,1 21,18H14V20H16V22H8V20H10V18H3C1.89,18 1,17.1 1,16V4C1,2.89 1.89,2 3,2H21M5,6H14V11H5V6M15,6H19V8H15V6M19,9V14H15V9H19M5,12H9V14H5V12M10,12H14V14H10V12Z" /></svg>`;
}

/**
 * Configure icon (pencil/edit)
 * Used for configuration buttons
 */
function getConfigIconSVG() {
    return `<svg viewBox="0 0 24 24"><path d="M5,3C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V12H19V19H5V5H12V3H5M17.78,4C17.61,4 17.43,4.07 17.3,4.2L16.08,5.41L18.58,7.91L19.8,6.7C20.06,6.44 20.06,6 19.8,5.75L18.25,4.2C18.12,4.07 17.95,4 17.78,4M15.37,6.12L8,13.5V16H10.5L17.87,8.62L15.37,6.12Z" /></svg>`;
}

/**
 * Download icon (download-box)
 * Used for file download buttons
 */
function getDownloadIconSVG() {
    return `<svg viewBox="0 0 24 24"><path d="M8 17V15H16V17H8M16 10L12 14L8 10H10.5V7H13.5V10H16M5 3H19C20.11 3 21 3.9 21 5V19C21 20.11 20.11 21 19 21H5C3.9 21 3 20.11 3 19V5C3 3.9 3.9 3 5 3M5 5V19H19V5H5Z" /></svg>`;
}

/**
 * Delete/Trash icon (trash-can)
 * Used for file delete buttons
 */
function getDeleteIconSVG() {
    return `<svg viewBox="0 0 24 24"><path d="M9,3V4H4V6H5V19A2,2 0 0,0 7,21H17A2,2 0 0,0 19,19V6H20V4H15V3H9M7,6H17V19H7V6M9,8V17H11V8H9M13,8V17H15V8H13Z" /></svg>`;
}

// ============================================================================
// CENTRALIZED POLLING MANAGER
// ============================================================================
// Optimized for resource-constrained embedded web server (RP2040 + W5500)
// - Only polls data needed for the currently active tab
// - Error tolerance: waits for 3 consecutive failures before showing errors
// - Centralized polling control to prevent duplicate requests

const PollingManager = {
    activeTab: null,
    intervals: new Map(),
    errorCounts: new Map(),  // Track consecutive errors per endpoint
    cachedData: new Map(),   // Cache last successful data per endpoint
    ERROR_THRESHOLD: 3,      // Show error after this many consecutive failures
    
    // Set active tab and stop all other polling
    setActiveTab(tabName) {
        this.activeTab = tabName;
        console.log(`[PollingManager] Active tab: ${tabName}`);
    },
    
    // Start an interval for a specific tab/endpoint
    startPolling(key, callback, intervalMs) {
        this.stopPolling(key);
        callback(); // Immediate first call
        const id = setInterval(() => {
            if (this.shouldPoll(key)) {
                callback();
            }
        }, intervalMs);
        this.intervals.set(key, id);
    },
    
    // Stop polling for a specific key
    stopPolling(key) {
        const id = this.intervals.get(key);
        if (id) {
            clearInterval(id);
            this.intervals.delete(key);
        }
    },
    
    // Stop all polling
    stopAllPolling() {
        for (const [key, id] of this.intervals) {
            clearInterval(id);
        }
        this.intervals.clear();
    },
    
    // Check if we should continue polling (tab still active)
    shouldPoll(key) {
        // Extract tab name from key (e.g., 'inputs-api' -> 'inputs')
        const tabName = key.split('-')[0];
        return this.activeTab === tabName;
    },
    
    // Record a successful fetch - reset error count, cache data
    recordSuccess(endpoint, data) {
        this.errorCounts.set(endpoint, 0);
        this.cachedData.set(endpoint, data);
    },
    
    // Record a failed fetch - increment error count
    recordError(endpoint) {
        const count = (this.errorCounts.get(endpoint) || 0) + 1;
        this.errorCounts.set(endpoint, count);
        return count;
    },
    
    // Check if we should show error UI (enough consecutive failures)
    shouldShowError(endpoint) {
        return (this.errorCounts.get(endpoint) || 0) >= this.ERROR_THRESHOLD;
    },
    
    // Get cached data for an endpoint (use during transient errors)
    getCachedData(endpoint) {
        return this.cachedData.get(endpoint);
    },
    
    // Get current error count for an endpoint
    getErrorCount(endpoint) {
        return this.errorCounts.get(endpoint) || 0;
    }
};

// Tab switching function
function openTab(evt, tabName) {
    // Stop all polling from previous tab to save bandwidth
    PollingManager.stopAllPolling();
    PollingManager.setActiveTab(tabName);
    
    // Get all tab content elements and hide them
    var tabcontent = document.getElementsByClassName("tab-content");
    for (var i = 0; i < tabcontent.length; i++) {
        tabcontent[i].classList.remove("active");
    }

    // Get all tab links and remove the active class
    var tablinks = document.getElementsByClassName("tab-link");
    for (var i = 0; i < tablinks.length; i++) {
        tablinks[i].classList.remove("active");
    }

    // Show the current tab and add an "active" class to the button that opened the tab
    document.getElementById(tabName).classList.add("active");
    evt.currentTarget.classList.add("active");
    
    // Initialize specific page content if needed - each tab manages its own polling
    if (tabName === 'dashboard') {
        if (typeof initDashboardTab === 'function') {
            initDashboardTab();
        }
    } else if (tabName === 'system') {
        initSystemTab();
    } else if (tabName === 'filemanager') {
        initFileManager();
    } else if (tabName === 'control') {
        initControlBoard();
    } else if (tabName === 'inputs') {
        initInputsTab();
    } else if (tabName === 'outputs') {
        initOutputsTab();
    } else if (tabName === 'comports') {
        initComPortsTab();
    } else if (tabName === 'devices') {
        if (typeof initDevicesTab === 'function') {
            initDevicesTab();
        }
    } else if (tabName === 'sensors') {
        initSensorsTab();
    } else if (tabName === 'controllers') {
        if (typeof initControllersTab === 'function') {
            initControllersTab();
        }
    } else if (tabName === 'settings') {
        // Load recording configuration when settings tab is opened
        if (typeof loadRecordingConfig === 'function') {
            loadRecordingConfig();
        }
    }
}

// Live clock - uses browser time (backend time shown in input fields)
function updateLiveClock() {
    const clockElement = document.getElementById('liveClock');
    if (clockElement) {
        const now = new Date();
        clockElement.textContent = now.toLocaleString();
    }
}

// File Manager functionality
let currentPath = '/';
let fileManagerActive = false; // Track if file manager tab is active
let sdStatusInterval = null; // Interval for SD status updates
let currentSortColumn = 'modified'; // Default sort column
let currentSortDirection = 'desc'; // Default sort direction (newest first)
let cachedDirectoryData = null; // Cache for sorting without refetching

// Global function to sort file list
function sortFileList(column) {
    // Toggle direction if same column, otherwise reset to desc for modified, asc for others
    if (column === currentSortColumn) {
        currentSortDirection = currentSortDirection === 'asc' ? 'desc' : 'asc';
    } else {
        currentSortColumn = column;
        currentSortDirection = column === 'modified' ? 'desc' : 'asc';
    }
    
    // Update header UI
    document.querySelectorAll('.file-list-header .sortable').forEach(el => {
        el.classList.remove('active', 'asc', 'desc');
        el.querySelector('.sort-icon').textContent = '';
    });
    
    const activeHeader = document.querySelector(`.file-list-header .sortable[data-sort="${column}"]`);
    if (activeHeader) {
        activeHeader.classList.add('active', currentSortDirection);
        activeHeader.querySelector('.sort-icon').textContent = currentSortDirection === 'asc' ? '▲' : '▼';
    }
    
    // Re-render with cached data if available
    if (window.cachedDirectoryData && window.displayDirectoryContentsSorted) {
        window.displayDirectoryContentsSorted(window.cachedDirectoryData);
    }
}

// Global function to refresh file list (used after backup export)
function refreshFileList() {
    if (window.loadDirectoryFunc) {
        window.loadDirectoryFunc(currentPath);
    }
}

const initFileManager = () => {
    if (!document.getElementById('filemanager')) return;
    
    fileManagerActive = true;
    const fileListContainer = document.getElementById('file-list-items');
    const pathNavigator = document.getElementById('path-navigator');
    const sdStatusElement = document.getElementById('sd-status');
    
    // Initialize file manager
    checkSDCardStatus();
    
    // Start periodic SD card status checks
    if (sdStatusInterval) {
        clearInterval(sdStatusInterval);
    }
    sdStatusInterval = setInterval(checkSDCardStatus, 3000); // Check every 3 seconds
    
    // SD card status error tolerance
    let sdErrorCount = 0;
    let lastValidSDStatus = null;
    const SD_ERROR_THRESHOLD = 3;
    
    // Function to check SD card status with error tolerance
    function checkSDCardStatus() {
        if (!fileManagerActive) return;
        
        fetch('/api/system/status')
            .then(response => {
                if (!response.ok) throw new Error('Failed to fetch status');
                return response.json();
            })
            .then(data => {
                // Validate response has expected structure
                if (!data || !data.sd) {
                    throw new Error('Invalid response: missing sd property');
                }
                
                // Reset error count and cache valid data
                sdErrorCount = 0;
                lastValidSDStatus = data.sd;
                
                updateSDCardUI(data.sd);
            })
            .catch(error => {
                console.error('Error checking SD status:', error);
                sdErrorCount++;
                
                // Use cached data if available and under error threshold
                if (sdErrorCount < SD_ERROR_THRESHOLD && lastValidSDStatus) {
                    console.log(`[SD] Using cached status (error ${sdErrorCount}/${SD_ERROR_THRESHOLD})`);
                    // Don't update UI - keep showing last valid state
                } else if (sdErrorCount >= SD_ERROR_THRESHOLD) {
                    // Too many consecutive errors - show error state
                    sdStatusElement.innerHTML = '<div class="status-error">Unable to get SD status</div>';
                }
            });
    }
    
    // Separate UI update function for cleaner code
    function updateSDCardUI(sd) {
        if (!sd.inserted) {
            // SD card is physically not inserted
            sdStatusElement.innerHTML = '<div class="status-error">SD Card not inserted</div>';
            fileListContainer.innerHTML = '<div class="error-message">SD Card is not inserted. Please insert an SD card to view files.</div>';
            pathNavigator.innerHTML = '';
        } else {
            // SD card is physically present - show basic info regardless of ready status
            if (sd.ready && sd.capacityGB) {
                sdStatusElement.innerHTML = `<div class="status-good">SD Card Ready - ${sd.freeSpaceGB.toFixed(2)} GB free of ${sd.capacityGB.toFixed(2)} GB</div>`;
            } else {
                sdStatusElement.innerHTML = '<div class="status-info">SD Card inserted</div>';
            }
            
            // Only reload directory contents if the file manager shows the "not inserted" error message
            const errorMsg = fileListContainer.querySelector('.error-message');
            if (errorMsg && errorMsg.textContent.includes('not inserted')) {
                loadDirectory(currentPath);
            }
        }
    }
    
    // Function to load directory contents with retry
    function loadDirectory(path, retryCount = 0) {
        const maxRetries = 3;
        fileListContainer.innerHTML = '<div class="loading">Loading files...</div>';
        updatePathNavigator(path);
        
        // Add a short delay before first API call after initialization
        // This helps ensure the SD card is fully ready
        const initialDelay = (retryCount === 0) ? 500 : 0;
        
        setTimeout(() => {
            fetch(`/api/sd/list?path=${encodeURIComponent(path)}`)
                .then(response => {
                    if (!response.ok) {
                        // If we get a non-OK response but the SD is inserted according to the system status
                        // it could be a timing issue with the SD card initialization
                        if (response.status === 503 && retryCount < maxRetries) {
                            // Wait longer with each retry
                            const delay = 1000 * (retryCount + 1);
                            console.log(`SD card not ready, retrying in ${delay}ms (attempt ${retryCount + 1}/${maxRetries})`);
                            fileListContainer.innerHTML = `<div class="loading">SD card initializing, please wait... (${retryCount + 1}/${maxRetries})</div>`;
                            
                            setTimeout(() => {
                                loadDirectory(path, retryCount + 1);
                            }, delay);
                            return null; // Skip further processing for this attempt
                        }
                        
                        throw new Error(response.status === 503 ? 
                            'SD card not ready. It may be initializing or experiencing an issue.' :
                            'Failed to list directory');
                    }
                    return response.json();
                })
                .then(data => {
                    if (data) {
                        displayDirectoryContents(data);
                    }
                })
                .catch(error => {
                    console.error('Error loading directory:', error);
                    fileListContainer.innerHTML = `
                        <div class="error-message">
                            ${error.message || 'Failed to load directory contents'}
                            ${retryCount >= maxRetries ? 
                                '<div class="retry-action"><button id="retryButton" class="download-btn">Retry</button></div>' : ''}
                        </div>
                    `;
                    
                    // Add retry button functionality
                    const retryButton = document.getElementById('retryButton');
                    if (retryButton) {
                        retryButton.addEventListener('click', () => {
                            loadDirectory(path, 0); // Reset retry count on manual retry
                        });
                    }
                });
        }, initialDelay);
    }
    
    // Function to format file size in a human-readable format
    function formatFileSize(bytes) {
        if (bytes === 0) return '0 B';
        
        const units = ['B', 'KB', 'MB', 'GB', 'TB'];
        const i = Math.floor(Math.log(bytes) / Math.log(1024));
        return parseFloat((bytes / Math.pow(1024, i)).toFixed(2)) + ' ' + units[i];
    }
    
    // Function to update the path navigator
    function updatePathNavigator(path) {
        pathNavigator.innerHTML = '';
        
        const parts = path.split('/').filter(part => part !== '');
        
        // Add root
        const rootElement = document.createElement('span');
        rootElement.className = 'path-part';
        rootElement.textContent = 'Root';
        rootElement.onclick = () => navigateTo('/');
        pathNavigator.appendChild(rootElement);
        
        let currentPathBuilder = '';
        parts.forEach((part, index) => {
            // Add separator
            const separator = document.createElement('span');
            separator.className = 'path-separator';
            separator.textContent = ' / ';
            pathNavigator.appendChild(separator);
            
            // Add path part
            currentPathBuilder += '/' + part;
            const pathPart = document.createElement('span');
            pathPart.className = 'path-part';
            pathPart.textContent = part;
            
            // Only make it clickable if it's not the last part
            if (index < parts.length - 1) {
                const pathCopy = currentPathBuilder; // Create a closure copy
                pathPart.onclick = () => navigateTo(pathCopy);
            }
            pathNavigator.appendChild(pathPart);
        });
    }
    
    // Function to navigate to a directory
    function navigateTo(path) {
        currentPath = path;  // Update module-level variable for backup refresh check
        loadDirectory(path);
    }
    
    // Function to display directory contents (uses global sort state)
    function displayDirectoryContents(data) {
        // Cache data globally for sorting
        window.cachedDirectoryData = data;
        
        fileListContainer.innerHTML = '';
        
        // Constant for maximum file size (should match server MAX_DOWNLOAD_SIZE)
        const MAX_DOWNLOAD_SIZE = 5242880; // 5MB in bytes
        
        // Check if there are no files or directories
        if (data.directories.length === 0 && data.files.length === 0) {
            fileListContainer.innerHTML = '<div class="empty-message">This directory is empty</div>';
            return;
        }
        
        // Sort files based on current sort settings
        const sortedFiles = [...data.files].sort((a, b) => {
            let comparison = 0;
            switch (currentSortColumn) {
                case 'name':
                    comparison = a.name.localeCompare(b.name);
                    break;
                case 'size':
                    comparison = a.size - b.size;
                    break;
                case 'modified':
                    // Parse date strings for comparison (format: YYYY-MM-DD HH:MM:SS)
                    const dateA = a.modified ? new Date(a.modified.replace(' ', 'T')) : new Date(0);
                    const dateB = b.modified ? new Date(b.modified.replace(' ', 'T')) : new Date(0);
                    comparison = dateA - dateB;
                    break;
            }
            return currentSortDirection === 'asc' ? comparison : -comparison;
        });
        
        // Display directories first (always sorted by name)
        const sortedDirs = [...data.directories].sort((a, b) => a.name.localeCompare(b.name));
        sortedDirs.forEach(dir => {
            const dirElement = document.createElement('div');
            dirElement.className = 'directory-item';
            dirElement.innerHTML = `
                <div class="file-name">${dir.name}</div>
                <div class="file-size">Directory</div>
                <div class="file-modified">-</div>
                <div class="file-actions"></div>
            `;
            dirElement.onclick = () => navigateTo(dir.path);
            fileListContainer.appendChild(dirElement);
        });
        
        // Then display sorted files
        sortedFiles.forEach(file => {
            const fileElement = document.createElement('div');
            fileElement.className = 'file-item';
            
            // Check if file is too large for download
            const isTooLarge = file.size > MAX_DOWNLOAD_SIZE;
            const downloadBtnClass = isTooLarge ? 'download-btn disabled' : 'download-btn';
            const downloadBtnTitle = isTooLarge 
                ? `File is too large to download (${formatFileSize(file.size)}). Maximum size is ${formatFileSize(MAX_DOWNLOAD_SIZE)}.`
                : 'Download this file';
            
            fileElement.innerHTML = `
                <div class="file-name" data-path="${file.path}">${file.name}</div>
                <div class="file-size">${formatFileSize(file.size)}</div>
                <div class="file-modified">${file.modified || '-'}</div>
                <div class="file-actions">
                    <button class="${downloadBtnClass}" data-path="${file.path}" title="${downloadBtnTitle}">
                        ${getDownloadIconSVG()}
                    </button>
                    <button class="delete-btn" data-path="${file.path}" title="Delete this file">
                        ${getDeleteIconSVG()}
                    </button>
                </div>
            `;
            fileListContainer.appendChild(fileElement);
            
            // Add click handler to filename for viewing
            const fileName = fileElement.querySelector('.file-name');
            fileName.addEventListener('click', (event) => {
                event.stopPropagation(); // Prevent bubbling
                const filePath = event.target.getAttribute('data-path');
                viewFile(filePath);
            });
            
            // Add download button event listener (only if not too large)
            const downloadBtn = fileElement.querySelector('.download-btn');
            if (!isTooLarge) {
                downloadBtn.addEventListener('click', (event) => {
                    event.stopPropagation(); // Prevent directory click event
                    const btn = event.target.closest('.download-btn');
                    const filePath = btn.getAttribute('data-path');
                    downloadFile(filePath);
                });
            }
            
            // Add delete button event listener
            const deleteBtn = fileElement.querySelector('.delete-btn');
            deleteBtn.addEventListener('click', (event) => {
                event.stopPropagation(); // Prevent bubbling
                const btn = event.target.closest('.delete-btn');
                const filePath = btn.getAttribute('data-path');
                deleteFile(filePath, btn);
            });
        });
    }
    
    // Function to download a file
    function downloadFile(path) {
        // Extract the filename from the path
        const filename = path.split('/').pop();
        const downloadUrl = `/api/sd/download?path=${encodeURIComponent(path)}`;
        
        // Create a link and click it to start download
        const a = document.createElement('a');
        a.href = downloadUrl;
        // Explicitly set the download attribute with the filename
        a.download = filename; 
        a.setAttribute('download', filename); // For older browsers
        // Don't use _blank as it tends to open in browser
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
    }
    
    // Function to view a file in the browser
    function viewFile(path) {
        // Construct the URL for viewing the file
        const viewUrl = `/api/sd/view?path=${encodeURIComponent(path)}`;
        
        // Open file in a new tab
        window.open(viewUrl, '_blank');
    }
    
    // Function to delete a file from SD card
    function deleteFile(path, buttonElement) {
        // Extract the filename from the path for the confirmation message
        const filename = path.split('/').pop();
        
        // Confirm deletion with user
        if (!confirm(`Are you sure you want to delete "${filename}"?\n\nThis action cannot be undone.`)) {
            return;
        }
        
        // Find the file row element before making the request
        const fileRow = buttonElement.closest('.file-item');
        
        // Send delete request
        fetch(`/api/sd/delete?path=${encodeURIComponent(path)}`, {
            method: 'DELETE'
        })
        .then(response => {
            if (!response.ok) {
                return response.json().then(data => {
                    throw new Error(data.error || 'Failed to delete file');
                });
            }
            return response.json();
        })
        .then(data => {
            console.log('File deleted successfully:', path);
            // Remove the file row from DOM with a fade-out effect
            if (fileRow) {
                fileRow.style.transition = 'opacity 0.3s ease';
                fileRow.style.opacity = '0';
                setTimeout(() => fileRow.remove(), 300);
            }
        })
        .catch(error => {
            console.error('Error deleting file:', error);
            alert(`Failed to delete file: ${error.message}`);
        });
    }
    
    // Fix to ensure SD card operation succeeds after system boot
    function checkAndLoadDirectory() {
        if (!fileManagerActive) return;
        
        fetch('/api/system/status')
            .then(response => response.json())
            .then(data => {
                if (data.sd && data.sd.inserted && data.sd.ready) {
                    // If SD card is ready according to status API, try loading the directory
                    loadDirectory(currentPath);
                } else if (data.sd && data.sd.inserted) {
                    // SD card is inserted but not fully ready - wait and retry
                    sdStatusElement.innerHTML = '<div class="status-info">SD Card initializing, please wait...</div>';
                    setTimeout(checkAndLoadDirectory, 1500);
                } else {
                    // SD card not inserted
                    sdStatusElement.innerHTML = '<div class="status-error">SD Card not inserted</div>';
                    fileListContainer.innerHTML = '<div class="error-message">SD Card is not inserted. Please insert an SD card to view files.</div>';
                    pathNavigator.innerHTML = '';
                }
            })
            .catch(error => {
                console.error('Error checking SD status:', error);
                // Retry after a delay
                setTimeout(checkAndLoadDirectory, 2000);
            });
    }
    
    // Override the initial checkSDCardStatus call to use the improved function with error tolerance
    // Uses the same sdErrorCount, lastValidSDStatus, and SD_ERROR_THRESHOLD from outer scope
    checkSDCardStatus = () => {
        if (!fileManagerActive) return;
        
        fetch('/api/system/status')
            .then(response => {
                if (!response.ok) throw new Error('Failed to fetch status');
                return response.json();
            })
            .then(data => {
                // Validate response has expected structure
                if (!data || !data.sd) {
                    throw new Error('Invalid response: missing sd property');
                }
                
                // Reset error count and cache valid data
                sdErrorCount = 0;
                lastValidSDStatus = data.sd;
                
                if (!data.sd.inserted) {
                    // SD card is physically not inserted
                    sdStatusElement.innerHTML = '<div class="status-error">SD Card not inserted</div>';
                    fileListContainer.innerHTML = '<div class="error-message">SD Card is not inserted. Please insert an SD card to view files.</div>';
                    pathNavigator.innerHTML = '';
                } else {
                    // SD card is physically present - show basic info regardless of ready status
                    if (data.sd.ready && data.sd.capacityGB) {
                        sdStatusElement.innerHTML = `<div class="status-good">SD Card Ready - ${data.sd.freeSpaceGB.toFixed(2)} GB free of ${data.sd.capacityGB.toFixed(2)} GB</div>`;
                        
                        // If we show Loading... but the card is ready, trigger a directory load
                        const loadingMsg = fileListContainer.querySelector('.loading');
                        if (loadingMsg) {
                            loadDirectory(currentPath);
                        }
                    } else {
                        sdStatusElement.innerHTML = '<div class="status-info">SD Card inserted</div>';
                    }
                    
                    // Only reload directory contents if the file manager shows the "not inserted" error message
                    const errorMsg = fileListContainer.querySelector('.error-message');
                    if (errorMsg && errorMsg.textContent.includes('not inserted')) {
                        checkAndLoadDirectory();
                    }
                }
            })
            .catch(error => {
                console.error('Error checking SD status:', error);
                sdErrorCount++;
                
                // Use cached data if available and under error threshold
                if (sdErrorCount < SD_ERROR_THRESHOLD && lastValidSDStatus) {
                    console.log(`[SD] Using cached status (error ${sdErrorCount}/${SD_ERROR_THRESHOLD})`);
                    // Don't update UI - keep showing last valid state
                } else if (sdErrorCount >= SD_ERROR_THRESHOLD) {
                    // Too many consecutive errors - show error state
                    sdStatusElement.innerHTML = '<div class="status-error">Unable to get SD status</div>';
                }
            });
    };
    
    // Initialize with our improved function
    checkAndLoadDirectory();
    
    // Expose functions globally for sorting and refresh
    window.loadDirectoryFunc = loadDirectory;
    window.displayDirectoryContentsSorted = displayDirectoryContents;
};

// Navigation
document.querySelectorAll('nav a').forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        const page = e.target.dataset.page;
        document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
        document.querySelector(`#${page}`).classList.add('active');
        document.querySelectorAll('nav a').forEach(a => a.classList.remove('active'));
        e.target.classList.add('active');
        
        // Initialize specific page content if needed
        if (page === 'system') {
            updateSystemStatus();
        } else if (page === 'filemanager') {
            initFileManager();
        }
    });
});

// Graphs removed: trending handled in InfluxDB. Chart.js initialization removed.

// Store historical data for trends
let sensorHistory = {
    temp: [],
    ph: [],
    do: [],
    stirrer: []
};

// Network and sensor data updates
let sensorDataUpdating = false; // Flag to prevent multiple simultaneous calls

// Function to show placeholder data when sensors are not available
function updateSensorPlaceholders() {
    // Update sensor readings with placeholder values
    const tempElement = document.getElementById('temp-reading');
    if (tempElement) tempElement.textContent = '--';
    
    const phElement = document.getElementById('ph-reading');
    if (phElement) phElement.textContent = '--';
    
    const doElement = document.getElementById('do-reading');
    if (doElement) doElement.textContent = '--';
    
    const stirrerElement = document.getElementById('stirrer-reading');
    if (stirrerElement) stirrerElement.textContent = '--';
    
    // Update control tab placeholders
    const tempControlElement = document.getElementById('current-temp');
    if (tempControlElement) tempControlElement.textContent = '-- °C';
    
    const phControlElement = document.getElementById('current-ph');
    if (phControlElement) phControlElement.textContent = '--';
    
    const doControlElement = document.getElementById('current-do');
    if (doControlElement) doControlElement.textContent = '--%';
    
    const stirrerControlElement = document.getElementById('current-stirrer-rpm');
    if (stirrerControlElement) stirrerControlElement.textContent = '-- RPM';
    
    const gasFlowElement = document.getElementById('current-gasflow');
    if (gasFlowElement) gasFlowElement.textContent = '-- mL/min';
    
    const pressureElement = document.getElementById('current-pressure');
    if (pressureElement) pressureElement.textContent = '-- kPa';
    
    const weightElement = document.getElementById('current-weight');
    if (weightElement) weightElement.textContent = '-- g';
    
    const odElement = document.getElementById('current-od');
    if (odElement) odElement.textContent = '-- OD';
    
    const powerVoltsElement = document.getElementById('current-power-volts');
    if (powerVoltsElement) powerVoltsElement.textContent = '-- V';
    
    const powerAmpsElement = document.getElementById('current-power-amps');
    if (powerAmpsElement) powerAmpsElement.textContent = '-- A';
    
    const powerWattsElement = document.getElementById('current-power-watts');
    if (powerWattsElement) powerWattsElement.textContent = '-- W';
}

// Update date/time input fields based on NTP state
function updateInputStates() {
    const ntpEnabled = document.getElementById('enableNTP').checked;
    const dateInput = document.getElementById('currentDate');
    const timeInput = document.getElementById('currentTime');
    
    if (dateInput && timeInput) {
        dateInput.disabled = ntpEnabled;
        timeInput.disabled = ntpEnabled;
    }
}

// Function to load initial settings
async function loadInitialSettings() {
    try {
        const response = await fetch('/api/time');
        const data = await response.json();
        
        if (data) {
            // Update form fields
            if (data.date) {
                document.getElementById('currentDate').value = data.date;
            }
            if (data.time) {
                // Remove seconds from time string if present (format: HH:MM:SS -> HH:MM)
                const timeWithoutSeconds = data.time.substring(0, 5);
                document.getElementById('currentTime').value = timeWithoutSeconds;
            }
            document.getElementById('enableNTP').checked = data.ntpEnabled;
            document.getElementById('enableDST').checked = data.dst;
            document.getElementById('timezone').value = data.timezone;
            updateInputStates();
        }
    } catch (error) {
        console.error('Error loading initial settings:', error);
    }
}

// Function to load initial network settings
async function loadNetworkSettings() {
    try {
        const response = await fetch('/api/network');
        const data = await response.json();
        console.log('Network settings received:', data);
        
        if (data) {
            // Set IP configuration mode
            const ipConfig = document.getElementById('ipConfig');
            if (ipConfig) {
                ipConfig.value = data.mode;
                // Trigger the change event to show/hide static fields
                ipConfig.dispatchEvent(new Event('change'));
            }

            // Set static IP configuration fields (for editing)
            document.getElementById('ipAddress').value = data.ip || '';
            document.getElementById('subnetMask').value = data.subnet || '';
            document.getElementById('gateway').value = data.gateway || '';
            document.getElementById('dns').value = data.dns || '';

            // Set current network info (read-only display)
            const macElement = document.getElementById('macAddress');
            if (macElement) {
                macElement.textContent = data.mac || 'Unknown';
            }
            const currentIpElement = document.getElementById('currentIP');
            if (currentIpElement) {
                currentIpElement.textContent = data.ip || 'Unknown';
            }

            // Set hostname and NTP server
            document.getElementById('hostName').value = data.hostname || '';
            document.getElementById('ntpServer').value = data.ntp || '';
        }
    } catch (error) {
        console.error('Error loading network settings:', error);
    }
}

// Function to load MQTT settings
async function loadMqttSettings() {
    try {
        const response = await fetch('/api/mqtt');
        const data = await response.json();
        
        document.getElementById('mqttEnabled').checked = data.mqttEnabled || false;
        document.getElementById('mqttBroker').value = data.mqttBroker || '';
        document.getElementById('mqttPort').value = data.mqttPort || '1883';
        document.getElementById('mqttUsername').value = data.mqttUsername || '';
        document.getElementById('mqttPassword').value = data.mqttPassword || '';
        
        // Update broker field required state based on enabled checkbox
        updateMqttBrokerRequired();
    } catch (error) {
        console.error('Error loading MQTT settings:', error);
    }
}

// Update broker field required state
function updateMqttBrokerRequired() {
    const enabled = document.getElementById('mqttEnabled').checked;
    const brokerInput = document.getElementById('mqttBroker');
    brokerInput.required = enabled;
}

// Function to load control settings
async function loadControlSettings() {
    try {
        // This would be replaced with actual API calls when implemented
        console.log('Loading control settings...');
        
        // For now, we'll just use default values in the HTML
        // In the future, this would fetch actual values from the backend
    } catch (error) {
        console.error('Error loading control settings:', error);
    }
}

// Event listeners
document.addEventListener('DOMContentLoaded', async () => {
    // Set up event listeners FIRST (before loading data that may trigger events)
    const ntpCheckbox = document.getElementById('enableNTP');
    if (ntpCheckbox) {
        ntpCheckbox.addEventListener('change', updateInputStates);
    }
    
    // Set up save button handler
    const saveButton = document.getElementById('saveTimeBtn');
    if (saveButton) {
        saveButton.addEventListener('click', (e) => {
            e.preventDefault();  // Prevent any default button behavior
            saveTimeSettings();
        });
        console.log('[Settings] Time save button handler attached');
    } else {
        console.warn('[Settings] saveTimeBtn not found in DOM');
    }

    // Set up IP configuration mode handler
    const ipConfig = document.getElementById('ipConfig');
    const updateStaticFields = () => {
        const staticFields = document.getElementById('staticSettings');
        if (staticFields && ipConfig) {
            staticFields.style.display = ipConfig.value === 'static' ? 'block' : 'none';
        }
    };
    if (ipConfig) {
        ipConfig.addEventListener('change', updateStaticFields);
    }
    
    // Load settings sequentially to avoid overwhelming single-threaded server
    // Event listeners are already attached, so change events will be handled
    await loadInitialSettings();  // Load initial NTP and timezone settings
    await loadNetworkSettings();  // Load initial network settings
    await loadMqttSettings();     // Load initial MQTT settings
    loadControlSettings();  // Load initial control settings (no API call)
    updateLiveClock();
    
    // Set initial state for static fields after network settings loaded
    updateStaticFields();
    
    // Initialize system status if system tab is active initially
    const systemTab = document.querySelector('#system');
    if (systemTab && systemTab.classList.contains('active')) {
        updateSystemStatus();
    }
    
    // Set up control save buttons
    const controlButtons = document.querySelectorAll('.control-btn');
    controlButtons.forEach(button => {
        button.addEventListener('click', function(e) {
            const controlType = e.target.id.split('-')[0]; // temp, ph, do, stirrer
            saveControlSettings(controlType);
        });
    });

    // Initialize control board if Control tab is active on load
    const controlTab = document.querySelector('#control');
    if (controlTab && controlTab.classList.contains('active')) {
        initControlBoard();
    }
    
    // Initialize dashboard if it's the default/active tab on load
    const dashboardTab = document.querySelector('#dashboard');
    if (dashboardTab && dashboardTab.classList.contains('active')) {
        setTimeout(() => {
            if (typeof initDashboardTab === 'function') {
                initDashboardTab();
            } else {
                console.error('[INIT] initDashboardTab not available!');
            }
        }, 100);
    }
});

// Function to save control settings
async function saveControlSettings(controlType) {
    // Get values from form
    const setpoint = document.getElementById(`${controlType}-setpoint`).value;
    const enabled = document.getElementById(`${controlType}-enabled`).checked;
    
    // Build data object based on control type
    let data = {
        setpoint: parseFloat(setpoint),
        enabled: enabled
    };
    
    // Add PID parameters if applicable
    if (controlType === 'temp' || controlType === 'do' || controlType === 'stirrer') {
        data.kp = parseFloat(document.getElementById(`${controlType}-kp`).value);
        data.ki = parseFloat(document.getElementById(`${controlType}-ki`).value);
        data.kd = parseFloat(document.getElementById(`${controlType}-kd`).value);
    }
    
    // Add pH specific parameters
    if (controlType === 'ph') {
        data.period = parseFloat(document.getElementById('ph-period').value);
        data.maxDoseTime = parseFloat(document.getElementById('ph-max-dose').value);
    }
    
    // Show loading toast
    const loadingToast = showToast('info', 'Saving...', `Updating ${controlType} control settings`, 5000);
    
    try {
        // This would be replaced with actual API call when implemented
        console.log(`Saving ${controlType} control settings:`, data);
        
        // Simulate API call success
        await new Promise(resolve => setTimeout(resolve, 1000));
        
        // Remove loading toast
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }
        
        // Show success toast
        showToast('success', 'Success', `${controlType.toUpperCase()} control settings saved successfully`);
    } catch (error) {
        console.error(`Error saving ${controlType} control settings:`, error);
        
        // Remove loading toast if it still exists
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }
        
        // Show error toast
        showToast('error', 'Error', `Failed to save ${controlType} control settings`);
    }
}

// Function to save time settings
async function saveTimeSettings() {
    console.log('Save time settings called');
    
    try {
        // Get values from form
        const date = document.getElementById('currentDate').value;
        const time = document.getElementById('currentTime').value;
        const timezone = document.getElementById('timezone').value;
        const ntpEnabled = document.getElementById('enableNTP').checked;
        const dstEnabled = document.getElementById('enableDST').checked;
        
        // Validate required fields - only need date/time if NTP is disabled
        if (!ntpEnabled && (!date || !time)) {
            showToast('error', 'Validation Error', 'Please provide both date and time for manual time setting');
            return;
        }
        
        // Build request payload
        const payload = {
            date: date,
            time: time,
            timezone: timezone,
            ntpEnabled: ntpEnabled,
            dstEnabled: dstEnabled
        };
        
        console.log('Sending time settings:', payload);
        
        // Send to backend
        const response = await fetch('/api/time', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(payload)
        });
        
        if (!response.ok) {
            const errorData = await response.json().catch(() => ({ error: 'Unknown error' }));
            throw new Error(errorData.error || 'Failed to save time settings');
        }
        
        const data = await response.json();
        showToast('success', 'Success', data.message || 'Time settings saved successfully');
        
        // Reload time settings to reflect changes
        await loadInitialSettings();
        
    } catch (error) {
        console.error('Error saving time settings:', error);
        showToast('error', 'Error', error.message || 'Failed to save time settings');
    }
}

// Update intervals - ONLY clock runs globally, other polling is tab-specific
setInterval(updateLiveClock, 1000);    // Update clock every second

function updateTrend(elementId, newValue, oldValue) {
    // This function is called but not defined - adding a placeholder
    // In a real implementation, this would update trend indicators
    console.log('Trend update called for', elementId, 'new:', newValue, 'old:', oldValue);
}

// Update system status information
let systemStatusUpdating = false; // Flag to prevent multiple simultaneous calls

async function updateSystemStatus() {
    // Prevent multiple simultaneous calls
    if (systemStatusUpdating) {
        return;
    }
    
    systemStatusUpdating = true;
    
    try {
        const response = await fetch('/api/system/status');
        
        // Check if response is ok before trying to parse JSON
        if (!response.ok) {
            // If it's a 404, the API doesn't exist yet - this is expected
            if (response.status === 404) {
                updateSystemStatusPlaceholders();
                return;
            }
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        // Check if response is JSON
        const contentType = response.headers.get('content-type');
        if (!contentType || !contentType.includes('application/json')) {
            throw new Error('Response is not JSON');
        }
        
        const data = await response.json();
        
        // Update power supplies
        if (data.power) {
            // Main voltage
            const mainVoltageElement = document.getElementById('mainVoltage');
            if (mainVoltageElement) {
                mainVoltageElement.textContent = data.power.mainVoltage.toFixed(1) + 'V';
            }
            const mainStatus = document.getElementById('mainVoltageStatus');
            if (mainStatus) {
                mainStatus.textContent = data.power.mainVoltageOK ? 'OK' : 'OUT OF RANGE';
                mainStatus.className = 'status ' + (data.power.mainVoltageOK ? 'ok' : 'error');
            }
            
            // 20V supply
            const v20VoltageElement = document.getElementById('v20Voltage');
            if (v20VoltageElement) {
                v20VoltageElement.textContent = data.power.v20Voltage.toFixed(1) + 'V';
            }
            const v20Status = document.getElementById('v20VoltageStatus');
            if (v20Status) {
                v20Status.textContent = data.power.v20VoltageOK ? 'OK' : 'OUT OF RANGE';
                v20Status.className = 'status ' + (data.power.v20VoltageOK ? 'ok' : 'error');
            }
            
            // 5V supply
            const v5VoltageElement = document.getElementById('v5Voltage');
            if (v5VoltageElement) {
                v5VoltageElement.textContent = data.power.v5Voltage.toFixed(1) + 'V';
            }
            const v5Status = document.getElementById('v5VoltageStatus');
            if (v5Status) {
                v5Status.textContent = data.power.v5VoltageOK ? 'OK' : 'OUT OF RANGE';
                v5Status.className = 'status ' + (data.power.v5VoltageOK ? 'ok' : 'error');
            }
        }
        
        // Update RTC status
        if (data.rtc) {
            const rtcStatus = document.getElementById('rtcStatus');
            if (rtcStatus) {
                rtcStatus.textContent = data.rtc.ok ? 'OK' : 'ERROR';
                rtcStatus.className = 'status ' + (data.rtc.ok ? 'ok' : 'error');
            }
            
            const rtcTimeElement = document.getElementById('rtcTime');
            if (rtcTimeElement) {
                rtcTimeElement.textContent = data.rtc.time;
            }
        }
        
        // Update IPC status
        const ipcStatus = document.getElementById('ipcStatus');
        if (ipcStatus) {
            ipcStatus.textContent = data.ipc ? 'OK' : 'ERROR';
            ipcStatus.className = 'status ' + (data.ipc ? 'ok' : 'error');
        }
        
        // Update MQTT status
        const mqttStatus = document.getElementById('mqttStatus');
        if (mqttStatus) {
            mqttStatus.textContent = data.mqtt ? 'CONNECTED' : 'NOT-CONNECTED';
            mqttStatus.className = 'status ' + (data.mqtt ? 'connected' : 'not-connected');
        }
        
        // Update Modbus status
        const modbusStatus = document.getElementById('modbusStatus');
        if (modbusStatus) {
            modbusStatus.textContent = data.modbus ? 'CONNECTED' : 'NOT-CONNECTED';
            modbusStatus.className = 'status ' + (data.modbus ? 'connected' : 'not-connected');
        }
        
        // Update SD card status
        if (data.sd) {
            const sdStatus = document.getElementById('sdStatus');
            if (sdStatus) {
                if (!data.sd.inserted) {
                    sdStatus.textContent = 'NOT INSERTED';
                    sdStatus.className = 'status warning';
                    hideSDDetails();
                } else if (!data.sd.ready) {
                    sdStatus.textContent = 'ERROR';
                    sdStatus.className = 'status error';
                    hideSDDetails();
                } else {
                    sdStatus.textContent = 'OK';
                    sdStatus.className = 'status ok';
                    
                    // Show SD card details
                    const sdCapacityContainer = document.getElementById('sdCapacityContainer');
                    const sdFreeSpaceContainer = document.getElementById('sdFreeSpaceContainer');
                    const sdLogSizeContainer = document.getElementById('sdLogSizeContainer');
                    
                    if (sdCapacityContainer) sdCapacityContainer.style.display = 'flex';
                    if (sdFreeSpaceContainer) sdFreeSpaceContainer.style.display = 'flex';
                    if (sdLogSizeContainer) sdLogSizeContainer.style.display = 'flex';
                    
                    // Update SD card details
                    const sdCapacity = document.getElementById('sdCapacity');
                    if (sdCapacity) sdCapacity.textContent = data.sd.capacityGB.toFixed(1) + ' GB';
                    
                    const sdFreeSpace = document.getElementById('sdFreeSpace');
                    if (sdFreeSpace) sdFreeSpace.textContent = data.sd.freeSpaceGB.toFixed(1) + ' GB';
                    
                    const sdLogSize = document.getElementById('sdLogSize');
                    if (sdLogSize && data.sd.logFileSizeKB !== undefined) {
                        sdLogSize.textContent = data.sd.logFileSizeKB.toFixed(1) + ' kB';
                    }
                }
            }
        }
    } catch (error) {
        console.error('Error updating system status:', error);
        updateSystemStatusPlaceholders();
    } finally {
        systemStatusUpdating = false;
    }
}

// Function to show placeholder data when system status API is not available
function updateSystemStatusPlaceholders() {
    // Update power supplies with placeholder values
    const mainVoltageElement = document.getElementById('mainVoltage');
    if (mainVoltageElement) mainVoltageElement.textContent = '--V';
    
    const v20VoltageElement = document.getElementById('v20Voltage');
    if (v20VoltageElement) v20VoltageElement.textContent = '--V';
    
    const v5VoltageElement = document.getElementById('v5Voltage');
    if (v5VoltageElement) v5VoltageElement.textContent = '--V';
    
    // Update status indicators
    const statusElements = [
        'mainVoltageStatus', 'v20VoltageStatus', 'v5VoltageStatus',
        'rtcStatus', 'ipcStatus', 'mqttStatus', 'modbusStatus', 'sdStatus'
    ];
    
    statusElements.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = 'API UNAVAILABLE';
            element.className = 'status warning';
        }
    });
    
    // Update RTC time
    const rtcTimeElement = document.getElementById('rtcTime');
    if (rtcTimeElement) rtcTimeElement.textContent = '--';
    
    // Hide SD card details
    hideSDDetails();
}

// Helper function to hide SD card details
function hideSDDetails() {
    document.getElementById('sdCapacityContainer').style.display = 'none';
    document.getElementById('sdFreeSpaceContainer').style.display = 'none';
    document.getElementById('sdLogSizeContainer').style.display = 'none';
    document.getElementById('sdSensorSizeContainer').style.display = 'none';
}

// Initialize system tab with polling
function initSystemTab() {
    // Use PollingManager for system status updates
    PollingManager.startPolling('system-status', updateSystemStatus, 5000);
}

// Toast notification functions
function showToast(type, title, message, duration = 3000) {
    const toastContainer = document.getElementById('toastContainer');
    
    // Create toast element
    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    
    // Add icon based on type
    let iconClass = '';
    switch (type) {
        case 'success':
            iconClass = '✓';
            break;
        case 'error':
            iconClass = '✗';
            break;
        case 'info':
            iconClass = 'ℹ';
            break;
        default:
            iconClass = 'ℹ';
    }
    
    // Create toast content
    toast.innerHTML = `
        <div class="toast-icon">${iconClass}</div>
        <div class="toast-content">
            <div class="toast-title">${title}</div>
            <div class="toast-message">${message}</div>
        </div>
    `;
    
    // Add to container
    toastContainer.appendChild(toast);
    
    // Remove after duration
    setTimeout(() => {
        toast.classList.add('toast-exit');
        setTimeout(() => {
            if (toast.parentNode) {
                toastContainer.removeChild(toast);
            }
        }, 300); // Wait for exit animation to complete
    }, duration);
    
    return toast;
}

// Network settings handling
document.getElementById('networkForm').addEventListener('submit', async function(e) {
    e.preventDefault();
    const statusDiv = document.getElementById('networkStatus');
    statusDiv.textContent = '';
    statusDiv.className = '';
    
    // Validate IP addresses if static IP is selected
    if (document.getElementById('ipConfig').value === 'static') {
        const inputs = ['ipAddress', 'subnetMask', 'gateway', 'dns'];
        for (const id of inputs) {
            const input = document.getElementById(id);
            if (!input.checkValidity()) {
                showToast('error', 'Validation Error', `Invalid ${id.replace(/([A-Z])/g, ' $1').toLowerCase()}`);
                return;
            }
        }
    }

    const networkConfig = {
        mode: document.getElementById('ipConfig').value,
        ip: document.getElementById('ipAddress').value,
        subnet: document.getElementById('subnetMask').value,
        gateway: document.getElementById('gateway').value,
        dns: document.getElementById('dns').value,
        hostname: document.getElementById('hostName').value,
        ntp: document.getElementById('ntpServer').value,
        dst: document.getElementById('enableDST').checked
    };

    // Show loading toast
    const loadingToast = showToast('info', 'Saving...', 'Updating network settings', 10000);

    try {
        const response = await fetch('/api/network', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(networkConfig)
        });

        // Remove loading toast
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }

        const result = await response.json();
        
        if (response.ok) {
            const restartToast = showToast('success', 'Success', 'Network settings saved. The device will restart to apply changes...', 5000);
            
            // Wait a moment before reloading to show the message
            setTimeout(() => {
                window.location.reload();
            }, 5000);
        } else {
            showToast('error', 'Error', result.error || 'Failed to save network settings');
        }
    } catch (error) {
        // Remove loading toast if it still exists
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }
        
        showToast('error', 'Network Error', error.message || 'Failed to connect to the server');
    }
});

// Add MQTT enabled checkbox change listener
document.getElementById('mqttEnabled').addEventListener('change', updateMqttBrokerRequired);

// Add MQTT form submission handler
document.getElementById('mqttForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const mqttEnabled = document.getElementById('mqttEnabled').checked;
    const mqttBroker = document.getElementById('mqttBroker').value;
    
    // Validate broker address if MQTT is enabled
    if (mqttEnabled && !mqttBroker.trim()) {
        showToast('error', 'Validation Error', 'Broker address is required when MQTT is enabled');
        return;
    }
    
    const mqttData = {
        mqttEnabled: mqttEnabled,
        mqttBroker: mqttBroker,
        mqttPort: parseInt(document.getElementById('mqttPort').value) || 1883,
        mqttUsername: document.getElementById('mqttUsername').value,
        mqttPassword: document.getElementById('mqttPassword').value
    };

    // Show loading toast
    const loadingToast = showToast('info', 'Saving...', 'Updating MQTT settings', 10000);

    try {
        const response = await fetch('/api/mqtt', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(mqttData)
        });

        // Remove loading toast
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }

        if (response.ok) {
            showToast('success', 'Success', 'MQTT settings saved successfully');
        } else {
            const result = await response.json().catch(() => ({ error: 'Failed to save MQTT settings' }));
            throw new Error(result.error || 'Failed to save MQTT settings');
        }
    } catch (error) {
        console.error('Error saving MQTT settings:', error);
        
        // Remove loading toast if it still exists
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }
        
        showToast('error', 'Error', error.message || 'Failed to save MQTT settings');
    }
});

// ============================================================================
// DATA RECORDING CONFIGURATION
// ============================================================================

// Load recording configuration when settings tab is opened
async function loadRecordingConfig() {
    try {
        const response = await fetch('/api/config/recording');
        if (!response.ok) throw new Error('Failed to load recording config');
        
        const config = await response.json();
        
        // Master enable
        document.getElementById('recordingEnabled').checked = config.enabled;
        
        // Individual types
        document.getElementById('recordInputs').checked = config.inputs.enabled;
        document.getElementById('inputsInterval').value = config.inputs.interval;
        
        document.getElementById('recordOutputs').checked = config.outputs.enabled;
        document.getElementById('outputsInterval').value = config.outputs.interval;
        
        document.getElementById('recordMotors').checked = config.motors.enabled;
        document.getElementById('motorsInterval').value = config.motors.interval;
        
        document.getElementById('recordSensors').checked = config.sensors.enabled;
        document.getElementById('sensorsInterval').value = config.sensors.interval;
        
        document.getElementById('recordEnergy').checked = config.energy.enabled;
        document.getElementById('energyInterval').value = config.energy.interval;
        
        document.getElementById('recordControllers').checked = config.controllers.enabled;
        document.getElementById('controllersInterval').value = config.controllers.interval;
        
        document.getElementById('recordDevices').checked = config.devices.enabled;
        document.getElementById('devicesInterval').value = config.devices.interval;
        
    } catch (error) {
        console.error('Error loading recording config:', error);
    }
}

// Recording form submission handler
document.getElementById('recordingForm').addEventListener('submit', async function(e) {
    e.preventDefault();
    const statusDiv = document.getElementById('recordingStatus');
    statusDiv.textContent = '';
    statusDiv.className = 'status-message';
    
    const recordingConfig = {
        enabled: document.getElementById('recordingEnabled').checked,
        inputs: {
            enabled: document.getElementById('recordInputs').checked,
            interval: parseInt(document.getElementById('inputsInterval').value) || 60
        },
        outputs: {
            enabled: document.getElementById('recordOutputs').checked,
            interval: parseInt(document.getElementById('outputsInterval').value) || 60
        },
        motors: {
            enabled: document.getElementById('recordMotors').checked,
            interval: parseInt(document.getElementById('motorsInterval').value) || 60
        },
        sensors: {
            enabled: document.getElementById('recordSensors').checked,
            interval: parseInt(document.getElementById('sensorsInterval').value) || 60
        },
        energy: {
            enabled: document.getElementById('recordEnergy').checked,
            interval: parseInt(document.getElementById('energyInterval').value) || 60
        },
        controllers: {
            enabled: document.getElementById('recordControllers').checked,
            interval: parseInt(document.getElementById('controllersInterval').value) || 60
        },
        devices: {
            enabled: document.getElementById('recordDevices').checked,
            interval: parseInt(document.getElementById('devicesInterval').value) || 60
        }
    };
    
    // Show loading toast
    const loadingToast = showToast('info', 'Saving...', 'Updating recording settings', 10000);
    
    try {
        const response = await fetch('/api/config/recording', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(recordingConfig)
        });
        
        // Remove loading toast
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }
        
        if (response.ok) {
            showToast('success', 'Success', 'Recording settings saved successfully');
            statusDiv.textContent = 'Settings saved successfully';
            statusDiv.className = 'status-message success';
        } else {
            const result = await response.json();
            throw new Error(result.error || 'Failed to save recording settings');
        }
    } catch (error) {
        console.error('Error saving recording settings:', error);
        
        // Remove loading toast if it still exists
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }
        
        showToast('error', 'Error', error.message || 'Failed to save recording settings');
        statusDiv.textContent = error.message || 'Failed to save settings';
        statusDiv.className = 'status-message error';
    }
});

// System reboot functionality
document.addEventListener('DOMContentLoaded', () => {
    const rebootButton = document.getElementById('rebootButton');
    const rebootModal = document.getElementById('rebootModal');
    const cancelReboot = document.getElementById('cancelReboot');
    const confirmReboot = document.getElementById('confirmReboot');

    if (rebootButton) {
        rebootButton.addEventListener('click', () => {
            rebootModal.classList.add('active');
        });
    }

    if (cancelReboot) {
        cancelReboot.addEventListener('click', () => {
            rebootModal.classList.remove('active');
        });
    }

    if (confirmReboot) {
        confirmReboot.addEventListener('click', async () => {
            try {
                // Show a loading state
                confirmReboot.textContent = 'Rebooting...';
                confirmReboot.disabled = true;
                
                // Call the reboot API
                const response = await fetch('/api/system/reboot', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    }
                });
                
                if (response.ok) {
                    // Hide the modal
                    rebootModal.classList.remove('active');
                    
                    // Show a toast notification
                    showToast('info', 'System Rebooting', 'The system is rebooting. This page will be unavailable for a few moments.');
                    
                    // Add a countdown to reconnect
                    let countdown = 12;
                    const reconnectMessage = document.createElement('div');
                    reconnectMessage.className = 'reconnect-message';
                    reconnectMessage.innerHTML = `<p>Reconnecting in <span id="countdown">${countdown}</span> seconds...</p>`;
                    document.body.appendChild(reconnectMessage);
                    
                    const countdownInterval = setInterval(() => {
                        countdown--;
                        document.getElementById('countdown').textContent = countdown;
                        
                        if (countdown <= 0) {
                            clearInterval(countdownInterval);
                            window.location.reload();
                        }
                    }, 1000);
                } else {
                    throw new Error('Failed to reboot system');
                }
            } catch (error) {
                console.error('Error rebooting system:', error);
                showToast('error', 'Reboot Failed', 'Failed to reboot the system. Please try again.');
                
                // Reset the button state
                confirmReboot.textContent = 'Yes, Reboot';
                confirmReboot.disabled = false;
                
                // Hide the modal
                rebootModal.classList.remove('active');
            }
        });
    }
});

// Initialize File Manager when switching to the tab
document.addEventListener('DOMContentLoaded', () => {
    // Add tab switching behavior for File Manager tab
    const fileManagerTab = document.querySelector('a[data-page="filemanager"]');
    if (fileManagerTab) {
        fileManagerTab.addEventListener('click', () => {
            fileManagerActive = true;
            initFileManager();
        });
    }
    
    // Handle switching away from file manager
    document.querySelectorAll('nav a:not([data-page="filemanager"])').forEach(tab => {
        tab.addEventListener('click', () => {
            fileManagerActive = false;
            // Clear the interval when leaving the file manager tab
            if (sdStatusInterval) {
                clearInterval(sdStatusInterval);
                sdStatusInterval = null;
            }
        });
    });
    
    // Initialize if file manager is the active tab on page load
    if (document.querySelector('#filemanager.active')) {
        fileManagerActive = true;
        initFileManager();
    }
});

// ================= Modular Control Board =================
// Contract:
// - Define parameter registry entries with id, name, type, units, and demo endpoints
// - Render as draggable cards; reorder persists in localStorage
// - Visibility toggles stored in localStorage via Customize modal
// - Save calls demo POST; Read polling maps to update values

const CONTROL_LS_KEYS = {
    order: 'orc.control.order',
    visibility: 'orc.control.visibility'
};

// Demo parameter registry. Replace endpoints when real IO MCU endpoints exist.
const PARAM_REGISTRY = [
    {
        id: 'temp',
        name: 'Temperature',
        kind: 'pid', // pid with setpoint
        units: '°C',
        min: 0, max: 100, step: 0.1,
        read: { method: 'GET', url: '/api/demo/temp' },
        write: { method: 'POST', url: '/api/demo/temp' }
    },
    {
        id: 'ph',
        name: 'pH',
        kind: 'doser', // setpoint + enable + dosing params
        units: '',
        min: 0, max: 14, step: 0.01,
        read: { method: 'GET', url: '/api/demo/ph' },
        write: { method: 'POST', url: '/api/demo/ph' }
    },
    {
        id: 'do',
        name: 'Dissolved Oxygen',
        kind: 'pid',
        units: '%',
        min: 0, max: 100, step: 1,
        read: { method: 'GET', url: '/api/demo/do' },
        write: { method: 'POST', url: '/api/demo/do' }
    },
    {
        id: 'stirrer',
        name: 'Stirrer Speed',
        kind: 'pid',
        units: 'RPM',
        min: 0, max: 2000, step: 10,
        read: { method: 'GET', url: '/api/demo/stirrer' },
        write: { method: 'POST', url: '/api/demo/stirrer' }
    },
    {
        id: 'gasflow',
        name: 'Gas Flow',
        kind: 'setpoint',
        units: 'mL/min',
        min: 0, max: 1000, step: 10,
        read: { method: 'GET', url: '/api/demo/gasflow' },
        write: { method: 'POST', url: '/api/demo/gasflow' }
    },
    {
        id: 'pump1',
        name: 'Pump 1 (Acid)',
        kind: 'toggle', // enable only
        units: '',
        read: { method: 'GET', url: '/api/demo/pump1' },
        write: { method: 'POST', url: '/api/demo/pump1' }
    },
    {
        id: 'pump2',
        name: 'Pump 2 (Base)',
        kind: 'toggle',
        units: '',
        read: { method: 'GET', url: '/api/demo/pump2' },
        write: { method: 'POST', url: '/api/demo/pump2' }
    },
    {
        id: 'pressure',
        name: 'Pressure',
        kind: 'readOnly',
        units: 'kPa',
        read: { method: 'GET', url: '/api/demo/pressure' }
    },
    {
        id: 'weight',
        name: 'Weight',
        kind: 'readOnly',
        units: 'g',
        read: { method: 'GET', url: '/api/demo/weight' }
    },
    {
        id: 'od',
        name: 'Optical Density',
        kind: 'readOnly',
        units: 'OD',
        read: { method: 'GET', url: '/api/demo/od' }
    },
    {
        id: 'pwrV',
        name: 'Power (Volts)',
        kind: 'readOnly',
        units: 'V',
        read: { method: 'GET', url: '/api/demo/power/volts' }
    },
    {
        id: 'pwrA',
        name: 'Power (Amps)',
        kind: 'readOnly',
        units: 'A',
        read: { method: 'GET', url: '/api/demo/power/amps' }
    },
    {
        id: 'pwrW',
        name: 'Power (Watts)',
        kind: 'readOnly',
        units: 'W',
        read: { method: 'GET', url: '/api/demo/power/watts' }
    }
];

let controlBoardInitialized = false;
let controlPollTimer = null;

function initControlBoard() {
    if (controlBoardInitialized) return; // idempotent per session
    controlBoardInitialized = true;
    const board = document.getElementById('controlBoard');
    if (!board) return;

    const visibility = loadVisibility();
    const order = loadOrder();

    const paramsById = Object.fromEntries(PARAM_REGISTRY.map(p => [p.id, p]));
    const orderedIds = order && order.length ? order.filter(id => paramsById[id]) : PARAM_REGISTRY.map(p => p.id);

    // Render according to order and visibility
    for (const id of orderedIds) {
        const p = paramsById[id];
        if (visibility[id] === false) continue;
        board.appendChild(renderParamCard(p));
    }

    setupDragAndDrop(board);
    setupCustomizeUI();
    setupFilterUI();
    startControlPolling();
}

function renderParamCard(p) {
    const card = document.createElement('div');
    card.className = 'param-card';
    card.setAttribute('draggable', 'true');
    card.dataset.paramId = p.id;

    card.innerHTML = `
        <div class="param-card-header">
            <div class="param-card-title">${p.name}</div>
            <div class="param-card-actions">
                <button class="icon-btn" title="Drag" aria-label="Drag to reorder">≡</button>
            </div>
        </div>
        <div class="param-card-body">${getParamBodyHTML(p)}</div>
        ${p.kind !== 'readOnly' ? `<div class="param-footer"><button class="btn-tiny" data-action="save">Save</button></div>` : ''}
    `;

    // Wire handlers
    if (p.kind !== 'readOnly') {
        const saveBtn = card.querySelector('[data-action="save"]');
        saveBtn.addEventListener('click', () => saveParam(p, card));
    }

    return card;
}

function getParamBodyHTML(p) {
    // Compose ID prefixes per card for scoping
    const id = p.id;
    const numberInput = (name) => `<input type="number" id="${id}-${name}" step="${p.step ?? 1}" ${p.min!=null?`min="${p.min}"`:''} ${p.max!=null?`max="${p.max}"`:''}>`;
    const switchHTML = (name, checked = false) => `<label class="switch"><input type="checkbox" id="${id}-${name}" ${checked?'checked':''}><span class="slider"></span></label>`;

    switch (p.kind) {
        case 'pid':
            return `
                <div class="param-row"><label>Current</label><div><span id="${id}-current">--</span> ${p.units}</div></div>
                <div class="param-row"><label>Setpoint</label><div>${numberInput('setpoint')} ${p.units}</div></div>
                <div class="pid-params">
                    <div class="form-group"><label>Kp</label>${numberInput('kp')}</div>
                    <div class="form-group"><label>Ki</label>${numberInput('ki')}</div>
                    <div class="form-group"><label>Kd</label>${numberInput('kd')}</div>
                </div>
                <div class="param-row"><label>Enabled</label><div>${switchHTML('enabled', true)}</div></div>
            `;
        case 'doser':
            return `
                <div class="param-row"><label>Current</label><div><span id="${id}-current">--</span> ${p.units}</div></div>
                <div class="param-row"><label>Setpoint</label><div>${numberInput('setpoint')}</div></div>
                <div class="param-row"><label>Period (s)</label><div>${numberInput('period')}</div></div>
                <div class="param-row"><label>Max Dose (s)</label><div>${numberInput('maxDose')}</div></div>
                <div class="param-row"><label>Enabled</label><div>${switchHTML('enabled')}</div></div>
            `;
        case 'setpoint':
            return `
                <div class="param-row"><label>Current</label><div><span id="${id}-current">--</span> ${p.units}</div></div>
                <div class="param-row"><label>Setpoint</label><div>${numberInput('setpoint')} ${p.units}</div></div>
                <div class="param-row"><label>Enabled</label><div>${switchHTML('enabled')}</div></div>
            `;
        case 'toggle':
            return `
                <div class="param-row"><label>Status</label><div><span id="${id}-current">--</span></div></div>
                <div class="param-row"><label>Enabled</label><div>${switchHTML('enabled')}</div></div>
            `;
        case 'readOnly':
        default:
            return `
                <div class="param-row"><label>Value</label><div><span id="${id}-current">--</span> ${p.units}</div></div>
            `;
    }
}

function setupDragAndDrop(board) {
    let dragItem = null;
    board.addEventListener('dragstart', (e) => {
        const card = e.target.closest('.param-card');
        if (!card) return;
        dragItem = card;
        card.classList.add('dragging');
        e.dataTransfer.effectAllowed = 'move';
    });
    board.addEventListener('dragend', (e) => {
        const card = e.target.closest('.param-card');
        if (card) card.classList.remove('dragging');
        dragItem = null;
        saveOrder(board);
    });
    board.addEventListener('dragover', (e) => {
        e.preventDefault();
        const afterElement = getDragAfterElement(board, e.clientY);
        if (afterElement == null) {
            board.appendChild(dragItem);
        } else {
            board.insertBefore(dragItem, afterElement);
        }
    });
}

function getDragAfterElement(container, y) {
    const draggableElements = [...container.querySelectorAll('.param-card:not(.dragging)')];
    return draggableElements.reduce((closest, child) => {
        const box = child.getBoundingClientRect();
        const offset = y - box.top - box.height / 2;
        if (offset < 0 && offset > closest.offset) {
            return { offset, element: child };
        } else {
            return closest;
        }
    }, { offset: Number.NEGATIVE_INFINITY }).element;
}

function saveOrder(board) {
    const ids = [...board.querySelectorAll('.param-card')].map(c => c.dataset.paramId);
    localStorage.setItem(CONTROL_LS_KEYS.order, JSON.stringify(ids));
}

function loadOrder() {
    try { return JSON.parse(localStorage.getItem(CONTROL_LS_KEYS.order)) || []; } catch { return []; }
}

function loadVisibility() {
    try { return JSON.parse(localStorage.getItem(CONTROL_LS_KEYS.visibility)) || {}; } catch { return {}; }
}

function saveVisibility(map) {
    localStorage.setItem(CONTROL_LS_KEYS.visibility, JSON.stringify(map));
}

function setupCustomizeUI() {
    const btn = document.getElementById('customizeControlBtn');
    const modal = document.getElementById('controlCustomizeModal');
    const closeBtn = document.getElementById('closeCustomize');
    const saveBtn = document.getElementById('saveCustomize');
    const selectAllBtn = document.getElementById('selectAllParams');
    const clearAllBtn = document.getElementById('clearAllParams');
    const resetLayoutBtn = document.getElementById('resetLayout');
    const list = document.getElementById('paramVisibilityList');
    if (!btn || !modal || !list) return;

    const open = () => {
        list.innerHTML = '';
        const vis = loadVisibility();
        PARAM_REGISTRY.forEach(p => {
            const row = document.createElement('div');
            row.className = 'param-toggle';
            row.innerHTML = `
                <input type="checkbox" id="vis-${p.id}" ${vis[p.id] !== false ? 'checked' : ''}/>
                <label class="name" for="vis-${p.id}">${p.name}</label>
                <span class="desc">${p.kind}</span>
            `;
            list.appendChild(row);
        });
        modal.classList.add('active');
    };
    const close = () => modal.classList.remove('active');

    btn.addEventListener('click', open);
    closeBtn.addEventListener('click', close);
    saveBtn.addEventListener('click', () => {
        const newVis = {};
        PARAM_REGISTRY.forEach(p => {
            const cb = document.getElementById(`vis-${p.id}`);
            newVis[p.id] = cb ? cb.checked : true;
        });
        saveVisibility(newVis);
        rerenderControlBoard();
        close();
    });
    selectAllBtn.addEventListener('click', () => {
        list.querySelectorAll('input[type="checkbox"]').forEach(cb => cb.checked = true);
    });
    clearAllBtn.addEventListener('click', () => {
        list.querySelectorAll('input[type="checkbox"]').forEach(cb => cb.checked = false);
    });
    resetLayoutBtn.addEventListener('click', () => {
        localStorage.removeItem(CONTROL_LS_KEYS.order);
        rerenderControlBoard();
    });
}

function setupFilterUI() {
    const input = document.getElementById('controlSearch');
    const board = document.getElementById('controlBoard');
    if (!input || !board) return;
    input.addEventListener('input', () => {
        const q = input.value.trim().toLowerCase();
        board.querySelectorAll('.param-card').forEach(card => {
            const name = card.querySelector('.param-card-title')?.textContent?.toLowerCase() || '';
            card.style.display = name.includes(q) ? '' : 'none';
        });
    });
}

function rerenderControlBoard() {
    const board = document.getElementById('controlBoard');
    if (!board) return;
    board.innerHTML = '';
    controlBoardInitialized = false;
    initControlBoard();
}

async function saveParam(p, card) {
    // Gather values from card
    const getNum = (name) => {
        const el = card.querySelector(`#${p.id}-${name}`);
        return el && el.value !== '' ? Number(el.value) : undefined;
    };
    const getBool = (name) => {
        const el = card.querySelector(`#${p.id}-${name}`);
        return el ? el.checked : undefined;
    };

    let payload = {};
    if (['pid', 'doser', 'setpoint'].includes(p.kind)) payload.setpoint = getNum('setpoint');
    if (p.kind === 'pid') {
        payload.kp = getNum('kp');
        payload.ki = getNum('ki');
        payload.kd = getNum('kd');
    }
    if (p.kind === 'doser') {
        payload.period = getNum('period');
        payload.maxDose = getNum('maxDose');
    }
    if (p.kind !== 'readOnly') payload.enabled = getBool('enabled');

    const toast = showToast('info', 'Saving...', `Updating ${p.name}`, 5000);
    try {
        // Demo behavior: if backend isn't available, simulate with timeout
        const res = await fetch(p.write.url, { method: p.write.method, headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(payload) });
        if (!res.ok) throw new Error(`HTTP ${res.status}`);
        if (toast.parentNode) toast.parentNode.removeChild(toast);
        showToast('success', 'Saved', `${p.name} updated`);
    } catch (e) {
        console.warn('Demo endpoint likely missing, simulating success:', e.message);
        if (toast.parentNode) toast.parentNode.removeChild(toast);
        showToast('success', 'Saved (Demo)', `${p.name} updated locally`);
        // Update current display optimistically
        const curEl = card.querySelector(`#${p.id}-current`);
        if (curEl && payload.setpoint != null) curEl.textContent = payload.setpoint;
    }
}

function startControlPolling() {
    // Clear existing
    if (controlPollTimer) clearInterval(controlPollTimer);
    const poll = async () => {
        const board = document.getElementById('controlBoard');
        if (!board || !document.getElementById('control').classList.contains('active')) return;
        for (const p of PARAM_REGISTRY) {
            const curEl = board.querySelector(`#${p.id}-current`);
            if (!curEl || !p.read) continue;
            try {
                const res = await fetch(p.read.url);
                if (!res.ok) throw new Error('not ok');
                const data = await res.json().catch(() => ({}));
                const value = data.value != null ? data.value : data.current != null ? data.current : undefined;
                if (value != null) curEl.textContent = typeof value === 'number' ? value.toString() : value;
            } catch {
                // demo fallback: do nothing
            }
        }
    };
    poll();
    controlPollTimer = setInterval(poll, 4000);
}

// ============================================================================
// INPUTS TAB - Hardware Inputs Monitoring
// ============================================================================

function initInputsTab() {
    // Use PollingManager - polls every 2 seconds while inputs tab is active
    PollingManager.startPolling('inputs-api', fetchAndRenderInputs, 2000);
}

function initSensorsTab() {
    // Sensors tab uses same data as inputs tab - use PollingManager
    PollingManager.startPolling('sensors-api', fetchAndRenderInputs, 2000);
}

async function fetchAndRenderInputs() {
    const ENDPOINT = '/api/inputs';
    
    try {
        const response = await fetch(ENDPOINT);
        if (!response.ok) throw new Error('Failed to fetch inputs');
        
        const data = await response.json();
        
        // Record success and cache data
        PollingManager.recordSuccess(ENDPOINT, data);
        
        // Render each section
        renderADCInputs(data.adc || []);
        renderRTDInputs(data.rtd || []);
        renderGPIOInputs(data.gpio || []);
        renderEnergySensors(data.energy || []);
        renderDeviceSensors(data.devices || []);
        
    } catch (error) {
        console.error('Error fetching inputs:', error);
        
        // Record error and check if we should show error UI
        const errorCount = PollingManager.recordError(ENDPOINT);
        
        if (PollingManager.shouldShowError(ENDPOINT)) {
            // Enough consecutive errors - show error UI
            showInputError('adc-list');
            showInputError('rtd-list');
            showInputError('gpio-list');
            showInputError('energy-list');
            showInputError('device-sensors-list');
        } else {
            // Transient error - use cached data if available
            const cachedData = PollingManager.getCachedData(ENDPOINT);
            if (cachedData) {
                console.log(`[Inputs] Using cached data (error ${errorCount}/${PollingManager.ERROR_THRESHOLD})`);
                renderADCInputs(cachedData.adc || []);
                renderRTDInputs(cachedData.rtd || []);
                renderGPIOInputs(cachedData.gpio || []);
                renderEnergySensors(cachedData.energy || []);
                renderDeviceSensors(cachedData.devices || []);
            }
        }
    }
}

function renderADCInputs(adcData) {
    const container = document.getElementById('adc-list');
    if (!container) return;
    
    if (adcData.length === 0) {
        container.innerHTML = '<div class="empty-message">No ADC data available</div>';
        return;
    }
    
    container.innerHTML = adcData.map(input => `
        <div class="input-item ${input.f ? 'fault' : ''}">
            <div class="input-header">
                <div class="input-header-left">
                    <span class="input-name">${input.n || `ADC ${input.i}`}</span>
                    <span class="input-value-inline">
                        <span class="value-large">${input.v.toFixed(2)}</span>
                        <span class="value-unit">${input.u}</span>
                    </span>
                </div>
                <span class="dashboard-icon ${input.d ? 'active' : 'inactive'}" title="${input.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                    ${getDashboardIconSVG()}
                </span>
                <button class="icon-btn" onclick="openADCConfigModal(${input.i})" title="Configure">
                    ${getConfigIconSVG()}
                </button>
            </div>
            ${input.f ? '<div class="fault-indicator">FAULT</div>' : ''}
        </div>
    `).join('');
}

function renderRTDInputs(rtdData) {
    const container = document.getElementById('rtd-list');
    if (!container) return;
    
    if (rtdData.length === 0) {
        container.innerHTML = '<div class="empty-message">No RTD data available</div>';
        return;
    }
    
    container.innerHTML = rtdData.map(input => `
        <div class="input-item ${input.f ? 'fault' : ''}">
            <div class="input-header">
                <div class="input-header-left">
                    <span class="input-name">${input.n || `RTD ${input.i - 9}`}</span>
                    <span class="input-value-inline">
                        <span class="value-large">${input.v.toFixed(2)}</span>
                        <span class="value-unit">${input.u}</span>
                    </span>
                </div>
                <span class="dashboard-icon ${input.d ? 'active' : 'inactive'}" title="${input.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                    ${getDashboardIconSVG()}
                </span>
                <button class="icon-btn" onclick="openRTDConfigModal(${input.i})" title="Configure">
                    ${getConfigIconSVG()}
                </button>
            </div>
            ${input.f ? '<div class="fault-indicator">FAULT</div>' : ''}
        </div>
    `).join('');
}

function renderGPIOInputs(gpioData) {
    const container = document.getElementById('gpio-list');
    if (!container) return;
    
    if (gpioData.length === 0) {
        container.innerHTML = '<div class="empty-message">No Input data available</div>';
        return;
    }
    
    container.innerHTML = gpioData.map(input => {
        // Defensive check: ensure input.i is valid
        const inputNumber = (typeof input.i === 'number' && !isNaN(input.i)) ? input.i - 12 : '?';
        const inputName = input.n || `Input ${inputNumber}`;
        
        return `
            <div class="input-item ${input.f ? 'fault' : ''}">
                <div class="input-header">
                    <div class="input-header-left">
                        <span class="input-name">${inputName}</span>
                        <span class="input-value-inline">
                            <span class="digital-state ${input.s ? 'state-high' : 'state-low'}">
                                ${input.s ? 'HIGH' : 'LOW'}
                            </span>
                        </span>
                    </div>
                    <span class="dashboard-icon ${input.d ? 'active' : 'inactive'}" title="${input.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                        ${getDashboardIconSVG()}
                    </span>
                    <button class="icon-btn" onclick="openGPIOConfigModal(${input.i})" title="Configure">
                        ${getConfigIconSVG()}
                    </button>
                </div>
                ${input.f ? '<div class="fault-indicator">FAULT</div>' : ''}
            </div>
        `;
    }).join('');
}

function renderEnergySensors(energyData) {
    const container = document.getElementById('energy-list');
    if (!container) return;
    
    if (energyData.length === 0) {
        container.innerHTML = '<div class="empty-message">No energy monitor data available</div>';
        return;
    }
    
    container.innerHTML = energyData.map(sensor => `
        <div class="input-item energy-sensor-item ${sensor.f ? 'fault' : ''}">
            <div class="input-header">
                <div class="input-header-left">
                    <span class="input-name">${sensor.n || `Energy Monitor ${sensor.i - 30}`}</span>
                </div>
                <span class="dashboard-icon ${sensor.d ? 'active' : 'inactive'}" title="${sensor.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                    ${getDashboardIconSVG()}
                </span>
                <button class="icon-btn" onclick="openEnergySensorConfigModal(${sensor.i})" title="Configure">
                    ${getConfigIconSVG()}
                </button>
            </div>
            <div class="energy-values">
                <div class="energy-value-row">
                    <span class="energy-label">Voltage:</span>
                    <span class="value-large">${sensor.v.toFixed(2)}</span>
                    <span class="value-unit">V</span>
                </div>
                <div class="energy-value-row">
                    <span class="energy-label">Current:</span>
                    <span class="value-large">${sensor.c.toFixed(3)}</span>
                    <span class="value-unit">A</span>
                </div>
                <div class="energy-value-row">
                    <span class="energy-label">Power:</span>
                    <span class="value-large">${sensor.p.toFixed(2)}</span>
                    <span class="value-unit">W</span>
                </div>
            </div>
            ${sensor.f ? '<div class="fault-indicator">FAULT</div>' : ''}
        </div>
    `).join('');
}

function renderDeviceSensors(deviceData) {
    const container = document.getElementById('device-sensors-list');
    if (!container) return;
    
    if (deviceData.length === 0) {
        container.innerHTML = '<div class="empty-message">No device sensors available<br><small>Create devices in the Devices tab to see their sensors here</small></div>';
        return;
    }
    
    // Map object type codes to friendly names (from ObjectType enum in objects.h)
    const getTypeName = (typeCode) => {
        const types = {
            0: 'Analog Input',
            1: 'Digital Input',
            2: 'Temperature',
            3: 'pH',
            4: 'Dissolved Oxygen',
            5: 'Optical Density',
            6: 'Flow',
            7: 'Pressure',
            8: 'Voltage',
            9: 'Current',
            10: 'Power',
            11: 'Energy',
            // Add more types as devices are supported
        };
        return types[typeCode] || 'Sensor';
    };
    
    container.innerHTML = deviceData.map(sensor => `
        <div class="input-item ${sensor.f ? 'fault' : ''}">
            <div class="input-header">
                <div class="input-header-left">
                    <span class="input-name">${sensor.n || `Device Sensor ${sensor.i}`}</span>
                    <span class="sensor-type-badge">${getTypeName(sensor.t)}</span>
                    <span class="input-value-inline">
                        <span class="value-large">${(sensor.v != null && !isNaN(sensor.v)) ? sensor.v.toFixed(2) : '--'}</span>
                        <span class="value-unit">${sensor.u}</span>
                    </span>
                </div>
                <span class="dashboard-icon ${sensor.d ? 'active' : 'inactive'}" title="${sensor.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                    ${getDashboardIconSVG()}
                </span>
                <button class="icon-btn" onclick="openDeviceSensorConfigModal(${sensor.i})" title="Configure">
                    ${getConfigIconSVG()}
                </button>
            </div>
            ${sensor.f ? '<div class="fault-indicator">FAULT</div>' : ''}
        </div>
    `).join('');
}

function showInputError(containerId) {
    const container = document.getElementById(containerId);
    if (container) {
        container.innerHTML = '<div class="error-message">Failed to load data</div>';
    }
}

// ============================================================================
// Energy Sensor Configuration Modal
// ============================================================================

let currentEnergySensorIndex = null;
let energySensorConfigData = null;

async function openEnergySensorConfigModal(index) {
    currentEnergySensorIndex = index;
    
    // Fetch current configuration
    try {
        const response = await fetch(`/api/config/energy/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        energySensorConfigData = await response.json();
        
        // Populate form
        document.getElementById('energySensorConfigIndex').textContent = `[${index}]`;
        document.getElementById('energySensorConfigName').value = energySensorConfigData.name || '';
        document.getElementById('energySensorShowOnDashboard').checked = energySensorConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('energySensorConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error fetching energy sensor config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeEnergySensorConfigModal() {
    document.getElementById('energySensorConfigModal').classList.remove('active');
    currentEnergySensorIndex = null;
    energySensorConfigData = null;
    
    // Reset form
    document.getElementById('energySensorConfigName').value = '';
}

async function saveEnergySensorConfig() {
    if (currentEnergySensorIndex === null) return;
    
    // Get values from form
    const name = document.getElementById('energySensorConfigName').value;
    
    const configData = {
        index: currentEnergySensorIndex,
        name: name,
        showOnDashboard: document.getElementById('energySensorShowOnDashboard').checked
    };
    
    try {
        const response = await fetch(`/api/config/energy/${currentEnergySensorIndex}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        const result = await response.json();
        
        showToast('success', 'Success', `Configuration saved for ${name}`);
        closeEnergySensorConfigModal();
        
        // Refresh inputs to show new name
        fetchAndRenderInputs();
        
    } catch (error) {
        console.error('Error saving energy sensor config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

// ============================================================================
// Device Sensor Configuration Modal
// ============================================================================

let currentDeviceSensorIndex = null;
let deviceSensorConfigData = null;

async function openDeviceSensorConfigModal(index) {
    currentDeviceSensorIndex = index;
    
    // Fetch current configuration
    try {
        const response = await fetch(`/api/config/devicesensor/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        deviceSensorConfigData = await response.json();
        
        // Populate form
        document.getElementById('deviceSensorConfigIndex').textContent = `[${index}]`;
        document.getElementById('deviceSensorConfigName').value = deviceSensorConfigData.name || '';
        document.getElementById('deviceSensorShowOnDashboard').checked = deviceSensorConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('deviceSensorConfigModal').classList.add('active');
    } catch (error) {
        console.error('Error fetching device sensor config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeDeviceSensorConfigModal() {
    document.getElementById('deviceSensorConfigModal').classList.remove('active');
    currentDeviceSensorIndex = null;
    deviceSensorConfigData = null;
    
    // Clear form
    document.getElementById('deviceSensorConfigName').value = '';
}

async function saveDeviceSensorConfig() {
    if (currentDeviceSensorIndex === null) return;
    
    // Get values from form
    const name = document.getElementById('deviceSensorConfigName').value.trim();
    const showOnDashboard = document.getElementById('deviceSensorShowOnDashboard').checked;
    
    const config = {
        name: name,
        showOnDashboard: showOnDashboard
    };
    
    try {
        const response = await fetch(`/api/config/devicesensor/${currentDeviceSensorIndex}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        const result = await response.json();
        
        showToast('success', 'Success', `Configuration saved for sensor ${currentDeviceSensorIndex}`);
        closeDeviceSensorConfigModal();
        
        // Refresh inputs to show new name
        fetchAndRenderInputs();
        
    } catch (error) {
        console.error('Error saving device sensor config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

// ============================================================================
// ADC Configuration Modal
// ============================================================================

let currentADCIndex = null;
let adcConfigData = null;

async function openADCConfigModal(index) {
    currentADCIndex = index;
    
    // Fetch current configuration
    try {
        const response = await fetch(`/api/config/adc/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        adcConfigData = await response.json();
        
        // Populate form
        document.getElementById('adcConfigIndex').textContent = `[${index}]`;
        document.getElementById('adcConfigName').value = adcConfigData.name || '';
        document.getElementById('adcConfigUnit').value = adcConfigData.unit || 'mV';
        document.getElementById('calScale').value = adcConfigData.cal.scale || 1.0;
        document.getElementById('calOffset').value = adcConfigData.cal.offset || 0.0;
        document.getElementById('resultScale').textContent = adcConfigData.cal.scale.toFixed(4);
        document.getElementById('resultOffset').textContent = adcConfigData.cal.offset.toFixed(2);
        document.getElementById('adcShowOnDashboard').checked = adcConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('adcConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error fetching ADC config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeADCConfigModal() {
    document.getElementById('adcConfigModal').classList.remove('active');
    currentADCIndex = null;
    adcConfigData = null;
    
    // Reset form
    document.getElementById('adcConfigName').value = '';
    document.getElementById('adcConfigUnit').value = 'mV';
    document.getElementById('calP1Raw').value = '';
    document.getElementById('calP1Real').value = '';
    document.getElementById('calP2Raw').value = '';
    document.getElementById('calP2Real').value = '';
    document.getElementById('calScale').value = '';
    document.getElementById('calOffset').value = '';
    document.getElementById('resultScale').textContent = '--';
    document.getElementById('resultOffset').textContent = '--';
}

async function saveADCConfig() {
    if (currentADCIndex === null) return;
    
    // Get values from form
    const name = document.getElementById('adcConfigName').value;
    const unit = document.getElementById('adcConfigUnit').value;
    const scale = parseFloat(document.getElementById('calScale').value) || 1.0;
    const offset = parseFloat(document.getElementById('calOffset').value) || 0.0;
    
    const configData = {
        index: currentADCIndex,
        name: name,
        unit: unit,
        showOnDashboard: document.getElementById('adcShowOnDashboard').checked,
        cal: {
            scale: scale,
            offset: offset
        }
    };
    
    try {
        const response = await fetch(`/api/config/adc/${currentADCIndex}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        const result = await response.json();
        
        showToast('success', 'Success', `Configuration saved for ADC ${currentADCIndex}`);
        closeADCConfigModal();
        
        // Refresh inputs to show new name
        fetchAndRenderInputs();
        
    } catch (error) {
        console.error('Error saving ADC config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

function resetCalibration() {
    // Reset to default calibration
    document.getElementById('calScale').value = '1.0';
    document.getElementById('calOffset').value = '0.0';
    document.getElementById('resultScale').textContent = '1.0000';
    document.getElementById('resultOffset').textContent = '0.00';
    
    // Clear two-point calibration fields
    document.getElementById('calP1Raw').value = '';
    document.getElementById('calP1Real').value = '';
    document.getElementById('calP2Raw').value = '';
    document.getElementById('calP2Real').value = '';
    
    showToast('info', 'Reset', 'Calibration reset to default values');
}

function calculateTwoPointCal() {
    const p1Displayed = parseFloat(document.getElementById('calP1Raw').value);
    const p1Actual = parseFloat(document.getElementById('calP1Real').value);
    const p2Displayed = parseFloat(document.getElementById('calP2Raw').value);
    const p2Actual = parseFloat(document.getElementById('calP2Real').value);
    
    if (isNaN(p1Displayed) || isNaN(p1Actual) || isNaN(p2Displayed) || isNaN(p2Actual)) {
        showToast('error', 'Error', 'Please fill in all calibration points');
        return;
    }
    
    if (Math.abs(p2Displayed - p1Displayed) < 0.001) {
        showToast('error', 'Error', 'Displayed values must be different');
        return;
    }
    
    // Get current calibration and unit to reverse transformations
    const currentScale = adcConfigData.cal.scale || 1.0;
    const currentOffset = adcConfigData.cal.offset || 0.0;
    const currentUnit = adcConfigData.unit || 'mV';
    
    // Unit conversion factors (from IO MCU drv_adc.h)
    const unitFactors = {
        'mV': 0.314,
        'V': 0.000314,
        'mA': 0.001308333,
        'uV': 314.0
    };
    const unitFactor = unitFactors[currentUnit] || 0.314;
    
    // Step 1: Convert displayed values back to raw ADC counts
    // IO MCU applies: displayed = (raw * scale + offset) * unitFactor
    // Reverse: result = displayed / unitFactor
    //          raw = (result - offset) / scale
    
    const p1Result = p1Displayed / unitFactor;
    const p1Raw = (p1Result - currentOffset) / currentScale;
    
    const p2Result = p2Displayed / unitFactor;
    const p2Raw = (p2Result - currentOffset) / currentScale;
    
    if (Math.abs(p2Raw - p1Raw) < 0.001) {
        showToast('error', 'Error', 'Calculated raw values too close together');
        return;
    }
    
    // Step 2: Convert actual measured values to intermediate "result" values
    // These are what the calibrated result should be (before unit conversion)
    const p1ActualResult = p1Actual / unitFactor;
    const p2ActualResult = p2Actual / unitFactor;
    
    // Step 3: Calculate NEW calibration: actualResult = newScale * raw + newOffset
    // This ensures calibration is always based on raw ADC values
    const scale = (p2ActualResult - p1ActualResult) / (p2Raw - p1Raw);
    const offset = p1ActualResult - (scale * p1Raw);
    
    // Update form fields
    document.getElementById('calScale').value = scale.toFixed(6);
    document.getElementById('calOffset').value = offset.toFixed(4);
    document.getElementById('resultScale').textContent = scale.toFixed(4);
    document.getElementById('resultOffset').textContent = offset.toFixed(2);
    
    showToast('success', 'Success', 'Calibration calculated successfully');
}

/* ============================================================================
   DAC Output Configuration Modal Functions
   ============================================================================ */

let currentDACIndex = null;
let dacConfigData = null;

async function showDACConfig(index) {
    currentDACIndex = index;
    
    try {
        const response = await fetch(`/api/config/dac/${index}`);
        if (!response.ok) throw new Error('Failed to load DAC config');
        
        dacConfigData = await response.json();
        
        // Populate form fields
        document.getElementById('dacConfigIndex').textContent = `[${index}]`;
        document.getElementById('dacConfigName').value = dacConfigData.name || '';
        document.getElementById('calScaleDAC').value = dacConfigData.cal.scale || 1.0;
        document.getElementById('calOffsetDAC').value = dacConfigData.cal.offset || 0.0;
        document.getElementById('resultScaleDAC').textContent = dacConfigData.cal.scale.toFixed(4);
        document.getElementById('resultOffsetDAC').textContent = dacConfigData.cal.offset.toFixed(2);
        document.getElementById('dacShowOnDashboard').checked = dacConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('dacConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error loading DAC config:', error);
        showToast('error', 'Error', 'Failed to load DAC configuration');
    }
}

function closeDACConfigModal() {
    document.getElementById('dacConfigModal').classList.remove('active');
    currentDACIndex = null;
    dacConfigData = null;
    
    // Clear form fields
    document.getElementById('dacConfigName').value = '';
    document.getElementById('calP1RawDAC').value = '';
    document.getElementById('calP1RealDAC').value = '';
    document.getElementById('calP2RawDAC').value = '';
    document.getElementById('calP2RealDAC').value = '';
    document.getElementById('calScaleDAC').value = '';
    document.getElementById('calOffsetDAC').value = '';
    document.getElementById('resultScaleDAC').textContent = '--';
    document.getElementById('resultOffsetDAC').textContent = '--';
}

async function saveDACConfig() {
    if (currentDACIndex === null) return;
    
    // Get values from form
    const name = document.getElementById('dacConfigName').value;
    const scale = parseFloat(document.getElementById('calScaleDAC').value) || 1.0;
    const offset = parseFloat(document.getElementById('calOffsetDAC').value) || 0.0;
    
    const configData = {
        index: currentDACIndex,
        name: name,
        cal: {
            scale: scale,
            offset: offset
        },
        showOnDashboard: document.getElementById('dacShowOnDashboard').checked
    };
    
    try {
        const response = await fetch(`/api/config/dac/${currentDACIndex}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to save config');
        }
        
        showToast('success', 'Success', 'DAC output configuration saved');
        closeDACConfigModal();
        
        // Refresh outputs to show new name/calibration
        fetchAndRenderOutputs();
    } catch (error) {
        console.error('Error saving DAC config:', error);
        showToast('error', 'Error', error.message || 'Failed to save configuration');
    }
}

function resetCalibrationDAC() {
    // Reset to default calibration
    document.getElementById('calScaleDAC').value = '1.0';
    document.getElementById('calOffsetDAC').value = '0.0';
    document.getElementById('resultScaleDAC').textContent = '1.0000';
    document.getElementById('resultOffsetDAC').textContent = '0.00';
    
    // Clear two-point calibration fields
    document.getElementById('calP1RawDAC').value = '';
    document.getElementById('calP1RealDAC').value = '';
    document.getElementById('calP2RawDAC').value = '';
    document.getElementById('calP2RealDAC').value = '';
    
    showToast('info', 'Reset', 'Calibration reset to default values');
}

function calculateTwoPointCalDAC() {
    const p1Displayed = parseFloat(document.getElementById('calP1RawDAC').value);
    const p1Actual = parseFloat(document.getElementById('calP1RealDAC').value);
    const p2Displayed = parseFloat(document.getElementById('calP2RawDAC').value);
    const p2Actual = parseFloat(document.getElementById('calP2RealDAC').value);
    
    if (isNaN(p1Displayed) || isNaN(p1Actual) || isNaN(p2Displayed) || isNaN(p2Actual)) {
        showToast('error', 'Error', 'Please fill in all calibration points');
        return;
    }
    
    if (p1Displayed === p2Displayed) {
        showToast('error', 'Error', 'Displayed values must be different');
        return;
    }
    
    // Get current calibration to reverse transformations
    const currentScale = dacConfigData.cal.scale || 1.0;
    const currentOffset = dacConfigData.cal.offset || 0.0;
    
    // Step 1: Reverse current calibration to get raw values
    // displayed = scale * raw + offset  -->  raw = (displayed - offset) / scale
    const p1Raw = (p1Displayed - currentOffset) / currentScale;
    const p2Raw = (p2Displayed - currentOffset) / currentScale;
    
    if (p1Raw === p2Raw) {
        showToast('error', 'Error', 'Invalid calibration points (same raw values)');
        return;
    }
    
    // Step 2: Calculate NEW calibration from raw to actual
    // actual = newScale * raw + newOffset
    const scale = (p2Actual - p1Actual) / (p2Raw - p1Raw);
    const offset = p1Actual - (scale * p1Raw);
    
    // Update form fields
    document.getElementById('calScaleDAC').value = scale.toFixed(6);
    document.getElementById('calOffsetDAC').value = offset.toFixed(4);
    document.getElementById('resultScaleDAC').textContent = scale.toFixed(4);
    document.getElementById('resultOffsetDAC').textContent = offset.toFixed(2);
    
    showToast('success', 'Success', 'Calibration calculated successfully');
}

/* ============================================================================
   RTD Configuration Modal Functions
   ============================================================================ */

let currentRTDIndex = null;
let rtdConfigData = null;

async function openRTDConfigModal(index) {
    currentRTDIndex = index;
    
    // Fetch current configuration
    try {
        const response = await fetch(`/api/config/rtd/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        rtdConfigData = await response.json();
        
        // Populate form
        document.getElementById('rtdConfigIndex').textContent = `[${index}]`;
        document.getElementById('rtdConfigName').value = rtdConfigData.name || '';
        document.getElementById('rtdConfigUnit').value = rtdConfigData.unit || '°C';
        document.getElementById('rtdConfigWires').value = rtdConfigData.wires || '3';
        document.getElementById('rtdConfigType').value = rtdConfigData.type || '100';
        document.getElementById('calScaleRTD').value = rtdConfigData.cal.scale || 1.0;
        document.getElementById('calOffsetRTD').value = rtdConfigData.cal.offset || 0.0;
        document.getElementById('resultScaleRTD').textContent = rtdConfigData.cal.scale.toFixed(4);
        document.getElementById('resultOffsetRTD').textContent = rtdConfigData.cal.offset.toFixed(2);
        document.getElementById('rtdShowOnDashboard').checked = rtdConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('rtdConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error fetching RTD config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeRTDConfigModal() {
    document.getElementById('rtdConfigModal').classList.remove('active');
    currentRTDIndex = null;
    rtdConfigData = null;
    
    // Reset form
    document.getElementById('rtdConfigName').value = '';
    document.getElementById('rtdConfigUnit').value = '°C';
    document.getElementById('rtdConfigWires').value = '3';
    document.getElementById('rtdConfigType').value = '100';
    document.getElementById('calP1RawRTD').value = '';
    document.getElementById('calP1RealRTD').value = '';
    document.getElementById('calP2RawRTD').value = '';
    document.getElementById('calP2RealRTD').value = '';
    document.getElementById('calScaleRTD').value = '';
    document.getElementById('calOffsetRTD').value = '';
    document.getElementById('resultScaleRTD').textContent = '--';
    document.getElementById('resultOffsetRTD').textContent = '--';
}

async function saveRTDConfig() {
    if (currentRTDIndex === null) return;
    
    // Get values from form
    const name = document.getElementById('rtdConfigName').value;
    const unit = document.getElementById('rtdConfigUnit').value;
    const wires = parseInt(document.getElementById('rtdConfigWires').value);
    const type = parseInt(document.getElementById('rtdConfigType').value);
    const scale = parseFloat(document.getElementById('calScaleRTD').value) || 1.0;
    const offset = parseFloat(document.getElementById('calOffsetRTD').value) || 0.0;
    
    const configData = {
        index: currentRTDIndex,
        name: name,
        unit: unit,
        wires: wires,
        type: type,
        showOnDashboard: document.getElementById('rtdShowOnDashboard').checked,
        cal: {
            scale: scale,
            offset: offset
        }
    };
    
    try {
        const response = await fetch(`/api/config/rtd/${currentRTDIndex}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        const result = await response.json();
        
        showToast('success', 'Success', `Configuration saved for RTD ${currentRTDIndex - 9}`);
        closeRTDConfigModal();
        
        // Refresh inputs to show new name
        fetchAndRenderInputs();
        
    } catch (error) {
        console.error('Error saving RTD config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

function resetCalibrationRTD() {
    // Reset to default calibration
    document.getElementById('calScaleRTD').value = '1.0';
    document.getElementById('calOffsetRTD').value = '0.0';
    document.getElementById('resultScaleRTD').textContent = '1.0000';
    document.getElementById('resultOffsetRTD').textContent = '0.00';
    
    // Clear two-point calibration fields
    document.getElementById('calP1RawRTD').value = '';
    document.getElementById('calP1RealRTD').value = '';
    document.getElementById('calP2RawRTD').value = '';
    document.getElementById('calP2RealRTD').value = '';
    
    showToast('info', 'Reset', 'Calibration reset to default values');
}

function calculateTwoPointCalRTD() {
    const p1Displayed = parseFloat(document.getElementById('calP1RawRTD').value);
    const p1Actual = parseFloat(document.getElementById('calP1RealRTD').value);
    const p2Displayed = parseFloat(document.getElementById('calP2RawRTD').value);
    const p2Actual = parseFloat(document.getElementById('calP2RealRTD').value);
    
    if (isNaN(p1Displayed) || isNaN(p1Actual) || isNaN(p2Displayed) || isNaN(p2Actual)) {
        showToast('error', 'Error', 'Please fill in all calibration points');
        return;
    }
    
    if (Math.abs(p2Displayed - p1Displayed) < 0.001) {
        showToast('error', 'Error', 'Displayed values must be different');
        return;
    }
    
    // Get current calibration and unit
    const currentScale = rtdConfigData.cal.scale || 1.0;
    const currentOffset = rtdConfigData.cal.offset || 0.0;
    const currentUnit = rtdConfigData.unit || '°C';
    
    // For RTD, we need to reverse any temperature unit conversion
    // Calibration is done in base unit (Celsius in IO MCU)
    // Convert displayed and actual values to Celsius if needed
    function toBaseTempUnit(value, unit) {
        if (unit === 'F') return (value - 32) * 5/9;  // F to C
        if (unit === 'K') return value - 273.15;       // K to C
        return value;  // Already C
    }
    
    const p1DisplayedC = toBaseTempUnit(p1Displayed, currentUnit);
    const p1ActualC = toBaseTempUnit(p1Actual, currentUnit);
    const p2DisplayedC = toBaseTempUnit(p2Displayed, currentUnit);
    const p2ActualC = toBaseTempUnit(p2Actual, currentUnit);
    
    // Reverse current calibration to get raw values
    const p1Raw = (p1DisplayedC - currentOffset) / currentScale;
    const p2Raw = (p2DisplayedC - currentOffset) / currentScale;
    
    if (Math.abs(p2Raw - p1Raw) < 0.001) {
        showToast('error', 'Error', 'Calculated raw values too close together');
        return;
    }
    
    // Calculate NEW calibration: actualC = newScale * raw + newOffset
    const scale = (p2ActualC - p1ActualC) / (p2Raw - p1Raw);
    const offset = p1ActualC - (scale * p1Raw);
    
    // Update form fields
    document.getElementById('calScaleRTD').value = scale.toFixed(6);
    document.getElementById('calOffsetRTD').value = offset.toFixed(4);
    document.getElementById('resultScaleRTD').textContent = scale.toFixed(4);
    document.getElementById('resultOffsetRTD').textContent = offset.toFixed(2);
    
    showToast('success', 'Success', 'Calibration calculated successfully');
}

// ============================================================================
// GPIO Configuration Modal
// ============================================================================

let currentGPIOIndex = null;
let gpioConfigData = null;

async function openGPIOConfigModal(index) {
    currentGPIOIndex = index;
    
    // Fetch current configuration
    try {
        const response = await fetch(`/api/config/gpio/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        gpioConfigData = await response.json();
        
        // Populate form
        document.getElementById('gpioConfigIndex').textContent = `${index - 12}`;  // Display as 1-8
        document.getElementById('gpioConfigName').value = gpioConfigData.name || '';
        document.getElementById('gpioConfigPullMode').value = gpioConfigData.pullMode || '1';
        document.getElementById('gpioShowOnDashboard').checked = gpioConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('gpioConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error loading GPIO config:', error);
        showToast('error', 'Error', 'Failed to load GPIO configuration');
    }
}

function closeGPIOConfigModal() {
    document.getElementById('gpioConfigModal').classList.remove('active');
    currentGPIOIndex = null;
    gpioConfigData = null;
}

async function saveGPIOConfig() {
    if (currentGPIOIndex === null) return;
    
    // Get values from form
    const name = document.getElementById('gpioConfigName').value;
    const pullMode = parseInt(document.getElementById('gpioConfigPullMode').value);
    
    const configData = {
        index: currentGPIOIndex,
        name: name,
        pullMode: pullMode,
        showOnDashboard: document.getElementById('gpioShowOnDashboard').checked,
        enabled: true
    };
    
    try {
        const response = await fetch(`/api/config/gpio/${currentGPIOIndex}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        const result = await response.json();
        
        showToast('success', 'Success', `Configuration saved for Input ${currentGPIOIndex - 12}`);
        closeGPIOConfigModal();
        
        // Refresh inputs to show new name
        fetchAndRenderInputs();
        
    } catch (error) {
        console.error('Error saving GPIO config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

// Calibration tab switching
document.addEventListener('DOMContentLoaded', function() {
    const calTabBtns = document.querySelectorAll('.cal-tab-btn');
    calTabBtns.forEach(btn => {
        btn.addEventListener('click', function() {
            const tab = this.getAttribute('data-tab');
            
            // Remove active from all tabs
            document.querySelectorAll('.cal-tab-btn').forEach(b => b.classList.remove('active'));
            document.querySelectorAll('.cal-tab-content').forEach(c => c.classList.remove('active'));
            
            // Add active to clicked tab
            this.classList.add('active');
            if (tab === 'two-point') {
                document.getElementById('twoPointCal').classList.add('active');
            } else if (tab === 'direct') {
                document.getElementById('directCal').classList.add('active');
            } else if (tab === 'two-point-dac') {
                document.getElementById('twoPointCalDAC').classList.add('active');
            } else if (tab === 'direct-dac') {
                document.getElementById('directCalDAC').classList.add('active');
            } else if (tab === 'two-point-rtd') {
                document.getElementById('twoPointCalRTD').classList.add('active');
            } else if (tab === 'direct-rtd') {
                document.getElementById('directCalRTD').classList.add('active');
            }
        });
    });
    
    // Update result display when direct entry changes
    document.getElementById('calScale').addEventListener('input', function() {
        const scale = parseFloat(this.value) || 0;
        document.getElementById('resultScale').textContent = scale.toFixed(4);
    });
    
    document.getElementById('calOffset').addEventListener('input', function() {
        const offset = parseFloat(this.value) || 0;
        document.getElementById('resultOffset').textContent = offset.toFixed(2);
    });
    
    // DAC direct entry listeners
    document.getElementById('calScaleDAC').addEventListener('input', function() {
        const scale = parseFloat(this.value) || 0;
        document.getElementById('resultScaleDAC').textContent = scale.toFixed(4);
    });
    
    document.getElementById('calOffsetDAC').addEventListener('input', function() {
        const offset = parseFloat(this.value) || 0;
        document.getElementById('resultOffsetDAC').textContent = offset.toFixed(2);
    });
    
    // RTD direct entry listeners
    document.getElementById('calScaleRTD').addEventListener('input', function() {
        const scale = parseFloat(this.value) || 0;
        document.getElementById('resultScaleRTD').textContent = scale.toFixed(4);
    });
    
    document.getElementById('calOffsetRTD').addEventListener('input', function() {
        const offset = parseFloat(this.value) || 0;
        document.getElementById('resultOffsetRTD').textContent = offset.toFixed(2);
    });
});

// ============================================================================
// OUTPUTS TAB - Hardware Outputs Monitoring & Control
// ============================================================================

// Note: outputsRefreshInterval removed - now using PollingManager
let activeControls = new Set(); // Track controls being manipulated
let lastOutputModes = new Map(); // Track output modes to detect changes
let pendingCommands = new Map(); // Track pending commands {index: setValue}
let stepperInputFocused = false; // Track if stepper RPM input has focus
let dcMotorSlidersFocused = new Map(); // Track which DC motor sliders have focus {index: boolean}
let dacSlidersFocused = new Map(); // Track which DAC sliders have focus {index: boolean}

function initOutputsTab() {
    // Use PollingManager - polls every 2 seconds while outputs tab is active
    PollingManager.startPolling('outputs-api', fetchAndRenderOutputs, 2000);
}

async function fetchAndRenderOutputs() {
    const ENDPOINT = '/api/outputs';
    
    try {
        const response = await fetch(ENDPOINT);
        if (!response.ok) throw new Error('Failed to fetch outputs');
        
        const data = await response.json();
        
        // Record success and cache data
        PollingManager.recordSuccess(ENDPOINT, data);
        
        // Render each section
        renderDACOutputs(data.dacOutputs || []);
        renderDigitalOutputs(data.digitalOutputs || []);
        renderStepperMotor(data.stepperMotor);
        renderDCMotors(data.dcMotors || []);
        
    } catch (error) {
        console.error('Error fetching outputs:', error);
        
        // Record error and check if we should show error UI
        const errorCount = PollingManager.recordError(ENDPOINT);
        
        if (PollingManager.shouldShowError(ENDPOINT)) {
            // Enough consecutive errors - show error UI
            showOutputError('dac-outputs-list');
            showOutputError('digital-outputs-list');
            showOutputError('stepper-motor-list');
            showOutputError('dc-motors-list');
        } else {
            // Transient error - use cached data if available
            const cachedData = PollingManager.getCachedData(ENDPOINT);
            if (cachedData) {
                console.log(`[Outputs] Using cached data (error ${errorCount}/${PollingManager.ERROR_THRESHOLD})`);
                renderDACOutputs(cachedData.dacOutputs || []);
                renderDigitalOutputs(cachedData.digitalOutputs || []);
                renderStepperMotor(cachedData.stepperMotor);
                renderDCMotors(cachedData.dcMotors || []);
            }
        }
    }
}

function showOutputError(containerId) {
    const container = document.getElementById(containerId);
    if (container) {
        container.innerHTML = '<div class="empty-message">Error loading data</div>';
    }
}

// ============================================================================
// DAC ANALOG OUTPUTS
// ============================================================================

function renderDACOutputs(dacOutputs) {
    const container = document.getElementById('dac-outputs-list');
    if (!container) return;
    
    if (dacOutputs.length === 0) {
        container.innerHTML = '<div class="empty-message">No DAC outputs configured</div>';
        return;
    }
    
    // Check if initial render needed
    const needsFullRender = !container.querySelector('.output-item');    
    if (needsFullRender) {
        // Full render
        container.innerHTML = dacOutputs.map(dac => `
            <div class="output-item" id="dac-item-${dac.index}">
                <div class="output-header">
                    <div class="output-header-left">
                        <span class="output-name">${dac.name}</span>
                    </div>
                    <span class="dashboard-icon ${dac.d ? 'active' : 'inactive'}" title="${dac.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                        ${getDashboardIconSVG()}
                    </span>
                    <button class="icon-btn" onclick="showDACConfig(${dac.index})" title="Configure">
                        ${getConfigIconSVG()}
                    </button>
                </div>
                <div class="output-controls">
                    <div class="control-group">
                        <label class="control-label">Set Value:</label>
                        <div class="control-slider-group-full">
                            <input type="range" 
                                   id="dac-slider-${dac.index}" 
                                   min="0" 
                                   max="10240" 
                                   step="50" 
                                   value="${dac.value}"
                                   onfocus="dacSlidersFocused.set(${dac.index}, true)"
                                   onblur="dacSlidersFocused.set(${dac.index}, false)"
                                   oninput="updateDACDisplay(${dac.index}, this.value)"
                                   onchange="setDACOutputValue(${dac.index}, parseFloat(this.value))">
                            <span class="value-display" id="dac-display-${dac.index}">${dac.value.toFixed(0)} ${dac.unit}</span>
                        </div>
                    </div>
                    <div class="status-info">
                        <span class="status-label">Actual:</span>
                        <span id="dac-actual-${dac.index}" class="status-value-compact status-synced">${dac.value.toFixed(0)} ${dac.unit}</span>
                    </div>
                </div>
            </div>
        `).join('');
    } else {
        // Selective update - only update values, not controls
        dacOutputs.forEach(dac => {
            const item = document.getElementById(`dac-item-${dac.index}`);
            if (!item) return;
            
            const slider = document.getElementById(`dac-slider-${dac.index}`);
            const display = document.getElementById(`dac-display-${dac.index}`);
            const actual = document.getElementById(`dac-actual-${dac.index}`);
            
            // Update dashboard icon
            const dashIcon = item.querySelector('.dashboard-icon');
            if (dashIcon) {
                dashIcon.className = `dashboard-icon ${dac.d ? 'active' : 'inactive'}`;
                dashIcon.title = dac.d ? 'Shown on Dashboard' : 'Hidden from Dashboard';
            }
            
            // Don't update slider if user is interacting with it
            if (slider && !dacSlidersFocused.get(dac.index)) {
                slider.value = dac.value;
                if (display) {
                    display.textContent = dac.value.toFixed(0);
                }
            }
            
            // Update actual value display
            if (actual) {
                actual.textContent = `${dac.value.toFixed(0)} ${dac.unit}`;
                
                // Update status styling
                const sliderValue = slider ? parseFloat(slider.value) : dac.value;
                const pending = pendingCommands.has(`dac-${dac.index}`);
                const synced = Math.abs(sliderValue - dac.value) < 10; // Within 10 mV
                
                actual.classList.toggle('status-synced', synced && !pending);
                actual.classList.toggle('status-pending', pending);
            }
        });
    }
}

function updateDACDisplay(index, value) {
    const display = document.getElementById(`dac-display-${index}`);
    if (display) {
        display.textContent = parseFloat(value).toFixed(0);
    }
}

async function setDACOutputValue(index, value) {
    console.log(`[DAC] Set output ${index} to ${value} mV`);
    
    // Mark as pending
    pendingCommands.set(`dac-${index}`, value);
    
    try {
        const response = await fetch(`/api/dac/${index}/value`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ value: value })
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to set DAC value');
        }
        
        console.log(`[DAC] Command sent for output ${index}`);
        
        // Remove from pending after a short delay (command has been sent)
        setTimeout(() => {
            pendingCommands.delete(`dac-${index}`);
        }, 500);
        
    } catch (error) {
        console.error('Error setting DAC output:', error);
        showToast(`Error: ${error.message}`, 'error');
        pendingCommands.delete(`dac-${index}`);
    }
}

// ============================================================================
// DIGITAL OUTPUTS RENDERING
// ============================================================================

function renderDigitalOutputs(outputs) {
    const container = document.getElementById('digital-outputs-list');
    if (!container) return;
    
    if (outputs.length === 0) {
        container.innerHTML = '<div class="empty-message">No outputs available</div>';
        return;
    }
    
    // Check if initial render needed or if any modes changed
    let needsFullRender = container.children.length !== outputs.length;
    
    // Check for mode changes
    if (!needsFullRender) {
        for (const output of outputs) {
            const lastMode = lastOutputModes.get(output.index);
            if (lastMode !== undefined && lastMode !== output.mode) {
                needsFullRender = true;
                console.log(`Output ${output.index} mode changed from ${lastMode} to ${output.mode}, forcing re-render`);
                break;
            }
        }
    }
    
    // Update mode tracking
    outputs.forEach(output => {
        lastOutputModes.set(output.index, output.mode);
    });
    
    if (needsFullRender) {
        // Full render on first load or structure change
        container.innerHTML = outputs.map(output => {
            const stateControl = output.mode === 1 ? 
                `<div class="output-control-compact">
                    <div class="control-slider-group">
                        <input type="range" min="0" max="100" 
                               value="${output.value || 0}" 
                               id="slider-${output.index}"
                               data-index="${output.index}"
                               onmousedown="markControlActive(${output.index})"
                               oninput="updateOutputSliderDisplay(${output.index}, this.value)"
                               onchange="setOutputValue(${output.index}, this.value); markControlInactive(${output.index})" 
                               style="flex: 1;">
                        <span class="value-display" id="slider-display-${output.index}">${output.value.toFixed(1) || 0}%</span>
                        <span class="status-value-compact status-synced" id="slider-actual-${output.index}" data-actual="${output.value.toFixed(1) || 0}">${output.value.toFixed(1) || 0}%</span>
                    </div>
                </div>` :
                `<div class="output-control-compact">
                    <label class="switch">
                        <input type="checkbox" 
                               ${output.state ? 'checked' : ''}
                               id="switch-${output.index}"
                               data-index="${output.index}"
                               onmousedown="markControlActive(${output.index})"
                               onchange="setOutputState(${output.index}, this.checked); markControlInactive(${output.index})">
                        <span class="slider"></span>
                    </label>
                    <span class="status-indicator-compact ${output.state ? 'status-on' : 'status-off'}" id="status-${output.index}">
                        ${output.state ? '● ON' : '○ OFF'}
                    </span>
                </div>`;
            
            return `
                <div class="output-item" id="output-item-${output.index}">
                    <div class="output-header">
                        <div class="output-header-left">
                            <span class="output-name">${output.name || `Output ${output.index - 20}`}</span>
                            <span class="output-mode-badge">${output.mode === 1 ? 'PWM' : 'ON/OFF'}</span>
                        </div>
                        <span class="dashboard-icon ${output.d ? 'active' : 'inactive'}" 
                              title="${output.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                            ${getDashboardIconSVG()}
                        </span>
                        <button class="icon-btn" onclick="openDigitalOutputConfigModal(${output.index})" title="Configure">
                            ${getConfigIconSVG()}
                        </button>
                    </div>
                    <div class="output-control">
                        ${stateControl}
                    </div>
                </div>
            `;
        }).join('');
    } else {
        // Selective update - only update values, not controls
        outputs.forEach(output => {
            const item = document.getElementById(`output-item-${output.index}`);
            if (!item) return;
            
            // Update dashboard icon
            const dashIcon = item.querySelector('.dashboard-icon');
            if (dashIcon) {
                dashIcon.className = `dashboard-icon ${output.d ? 'active' : 'inactive'}`;
                dashIcon.title = output.d ? 'Shown on Dashboard' : 'Hidden from Dashboard';
            }
            
            const isActive = activeControls.has(output.index);
            
            if (output.mode === 1) {
                // PWM Mode
                const slider = document.getElementById(`slider-${output.index}`);
                const actualDisplay = document.getElementById(`slider-actual-${output.index}`);
                const display = document.getElementById(`slider-display-${output.index}`);
                
                const pendingValue = pendingCommands.get(output.index);
                const actualValue = output.value.toFixed(1) || 0;
                
                if (!isActive && slider) {
                    // Only update slider if no pending command or actual value matches pending
                    if (!pendingValue || pendingValue === actualValue) {
                        slider.value = actualValue;
                        if (display) display.textContent = actualValue + '%';
                        pendingCommands.delete(output.index);
                    }
                }
                
                if (actualDisplay) {
                    actualDisplay.textContent = actualValue + '%';
                    actualDisplay.setAttribute('data-actual', actualValue);
                    
                    // Color code based on sync state
                    if (pendingValue && pendingValue !== actualValue) {
                        actualDisplay.className = 'status-value-compact status-pending';
                    } else {
                        actualDisplay.className = 'status-value-compact status-synced';
                    }
                }
            } else {
                // ON/OFF Mode
                const switchElem = document.getElementById(`switch-${output.index}`);
                const statusDisplay = document.getElementById(`status-${output.index}`);
                
                const pendingValue = pendingCommands.get(output.index);
                
                if (!isActive && switchElem) {
                    // Only update switch if no pending command or actual state matches pending
                    if (pendingValue === undefined || pendingValue === output.state) {
                        switchElem.checked = output.state;
                        pendingCommands.delete(output.index);
                    }
                }
                
                if (statusDisplay) {
                    const baseClass = output.state ? 'status-on' : 'status-off';
                    
                    // Add pending indicator if command in flight
                    if (pendingValue !== undefined && pendingValue !== output.state) {
                        statusDisplay.className = `status-indicator-compact ${baseClass} status-pending`;
                    } else {
                        statusDisplay.className = `status-indicator-compact ${baseClass}`;
                    }
                    statusDisplay.textContent = output.state ? '● ON' : '○ OFF';
                }
            }
        });
    }
}

// ============================================================================
// STEPPER MOTOR RENDERING
// ============================================================================

function renderStepperMotor(stepper) {
    const container = document.getElementById('stepper-motor-list');
    if (!container) return;
    
    if (!stepper) {
        container.innerHTML = '<div class="empty-message">Stepper motor not available</div>';
        return;
    }
    
    // Check if initial render needed
    const needsFullRender = !container.querySelector('.output-item');
    
    if (needsFullRender) {
        // Full render on first load
        container.innerHTML = `
        <div class="output-item">
            <div class="output-header">
                <div class="output-header-left">
                    <span class="output-name">${stepper.name || 'Stepper Motor'}</span>
                    <span class="output-status-badge ${stepper.running ? 'running' : 'stopped'}">
                        ${stepper.running ? 'ENABLED' : 'DISABLED'}
                    </span>
                    <span class="output-status-badge" style="background-color: #6c757d; color: white; margin-left: 4px;">
                        <i class="mdi ${stepper.direction ? 'mdi-axis-z-rotate-counterclockwise' : 'mdi-axis-z-rotate-clockwise'}"></i>
                        ${stepper.direction ? 'FORWARD' : 'REVERSE'}
                    </span>
                </div>
                <div class="output-header-right">
                    <span class="dashboard-icon ${stepper.d ? 'active' : 'inactive'}" 
                          title="${stepper.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                        ${getDashboardIconSVG()}
                    </span>
                    <button class="icon-btn" onclick="openStepperConfigModal()" title="Configure">
                        ${getConfigIconSVG()}
                    </button>
                </div>
            </div>
            
            <div class="stepper-controls">
                <!-- RPM Input -->
                <div class="control-group">
                    <label class="control-label">Target RPM (Max: ${stepper.maxRPM || 500})</label>
                    <div class="control-input-group">
                        <input type="number" id="stepperRPM" min="0" max="${stepper.maxRPM || 500}" 
                               value="${stepper.rpm || 0}" placeholder="Enter RPM"
                               onfocus="stepperInputFocused = true"
                               onblur="stepperInputFocused = false">
                        <button class="output-btn output-btn-primary output-btn-sm" onclick="setStepperRPM()">Set</button>
                    </div>
                </div>
                
                <!-- Direction Buttons -->
                <div class="control-group">
                    <div class="button-group">
                        <button class="output-btn ${stepper.direction ? 'output-btn-primary' : 'output-btn-secondary'}" 
                                onclick="setStepperDirection(true)">
                            <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M8.59,16.58L13.17,12L8.59,7.41L10,6L16,12L10,18L8.59,16.58Z" /></svg>
                            Forward
                        </button>
                        <button class="output-btn ${!stepper.direction ? 'output-btn-primary' : 'output-btn-secondary'}" 
                                onclick="setStepperDirection(false)">
                            <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M15.41,16.58L10.83,12L15.41,7.41L14,6L8,12L14,18L15.41,16.58Z" /></svg>
                            Reverse
                        </button>
                    </div>
                </div>
                
                <!-- Start/Stop Buttons -->
                <div class="control-group">
                    <div class="button-group">
                        <button class="output-btn output-btn-success output-btn-wide" onclick="startStepper()" ${stepper.running ? 'disabled' : ''}>
                            <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M8,5.14V19.14L19,12.14L8,5.14Z" /></svg>
                            Enable
                        </button>
                        <button class="output-btn output-btn-danger output-btn-wide" onclick="stopStepper()" ${!stepper.running ? 'disabled' : ''}>
                            <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M18,18H6V6H18V18Z" /></svg>
                            Disable
                        </button>
                    </div>
                </div>
            </div>
        </div>
    `;
    } else {
        // Selective update - only update values if input not focused
        const rpmInput = document.getElementById('stepperRPM');
        if (rpmInput && !stepperInputFocused) {
            rpmInput.value = stepper.rpm || 0;
            rpmInput.max = stepper.maxRPM || 500;
        }
        
        // Update max RPM label
        const label = container.querySelector('.control-label');
        if (label) {
            label.textContent = `Target RPM (Max: ${stepper.maxRPM || 500})`;
        }
        
        // Update status badges (running and direction)
        const statusBadges = container.querySelectorAll('.output-status-badge');
        if (statusBadges.length >= 1) {
            // First badge: running status
            statusBadges[0].className = `output-status-badge ${stepper.running ? 'running' : 'stopped'}`;
            statusBadges[0].textContent = stepper.running ? 'ENABLED' : 'DISABLED';
        }
        if (statusBadges.length >= 2) {
            // Second badge: direction
            statusBadges[1].style.color = 'white';
            statusBadges[1].innerHTML = `<i class="mdi ${stepper.direction ? 'mdi-axis-z-rotate-counterclockwise' : 'mdi-axis-z-rotate-clockwise'}"></i> ${stepper.direction ? 'FORWARD' : 'REVERSE'}`;
        }
        
        // Update direction buttons
        const buttons = container.querySelectorAll('.button-group button');
        if (buttons.length >= 2) {
            buttons[0].className = `output-btn ${stepper.direction ? 'output-btn-primary' : 'output-btn-secondary'}`;
            buttons[1].className = `output-btn ${!stepper.direction ? 'output-btn-primary' : 'output-btn-secondary'}`;
        }
        
        // Update enable/disable buttons
        const controlButtons = container.querySelectorAll('.control-group:last-child .button-group button');
        if (controlButtons.length >= 2) {
            // Enable button: disabled when motor is enabled
            controlButtons[0].disabled = stepper.running;
            // Disable button: disabled when motor is disabled
            controlButtons[1].disabled = !stepper.running;
        }
        
        // Update name
        const nameSpan = container.querySelector('.output-name');
        if (nameSpan) {
            nameSpan.textContent = stepper.name || 'Stepper Motor';
        }
        
        // Update dashboard icon
        const dashIcon = container.querySelector('.dashboard-icon');
        if (dashIcon) {
            dashIcon.className = `dashboard-icon ${stepper.d ? 'active' : 'inactive'}`;
            dashIcon.title = stepper.d ? 'Shown on Dashboard' : 'Hidden from Dashboard';
        }
    }
}

// ============================================================================
// DC MOTORS RENDERING
// ============================================================================

function renderDCMotors(motors) {
    const container = document.getElementById('dc-motors-list');
    if (!container) return;
    
    if (motors.length === 0) {
        container.innerHTML = '<div class="empty-message">No DC motors available</div>';
        return;
    }
    
    // Check if initial render is needed (no motor items exist)
    const needsFullRender = container.querySelectorAll('.output-item').length !== motors.length;
    
    if (needsFullRender) {
        // Full render on first load
        container.innerHTML = motors.map(motor => `
            <div class="output-item" data-motor-index="${motor.index}">
                <div class="output-header">
                    <div class="output-header-left">
                        <span class="output-name">${motor.name || `DC Motor ${motor.index - 26}`}</span>
                        <span class="output-status-badge ${motor.running ? 'running' : 'stopped'}">
                            ${motor.running ? 'RUNNING' : 'STOPPED'}
                        </span>
                        <span class="output-status-badge" style="background-color: #6c757d; color: white; margin-left: 4px;">
                            <i class="mdi ${motor.direction ? 'mdi-axis-z-rotate-counterclockwise' : 'mdi-axis-z-rotate-clockwise'}"></i>
                            ${motor.direction ? 'FORWARD' : 'REVERSE'}
                        </span>
                    </div>
                    <div class="output-header-right">
                        <span class="dashboard-icon ${motor.d ? 'active' : 'inactive'}" 
                              title="${motor.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                            ${getDashboardIconSVG()}
                        </span>
                        <button class="icon-btn" onclick="openDCMotorConfigModal(${motor.index})" title="Configure">
                            ${getConfigIconSVG()}
                        </button>
                    </div>
                </div>
                
                <div class="dc-motor-controls">
                    <!-- Power Control -->
                    <div class="control-group">
                        <label class="control-label">Set Power:</label>
                        <div class="control-slider-group-full">
                            <input type="range" min="0" max="100" value="${motor.power || 0}" 
                                   id="motor-slider-${motor.index}"
                                   onfocus="dcMotorSlidersFocused.set(${motor.index}, true)"
                                   onblur="dcMotorSlidersFocused.set(${motor.index}, false)"
                                   oninput="updateMotorPowerDisplay(${motor.index}, this.value)"
                                   onchange="setDCMotorPower(${motor.index}, this.value)">
                            <span class="value-display" id="motor-power-display-${motor.index}">${motor.power || 0}%</span>
                        </div>
                    </div>
                    
                    <!-- Status Display -->
                    <div class="status-info">
                        <span class="status-label">Power:</span>
                        <span id="motor-actual-power-${motor.index}" class="status-value-compact status-synced">${motor.power || 0}%</span>
                        <span class="status-separator">•</span>
                        <span class="status-label">Current:</span>
                        <span id="motor-current-${motor.index}" class="status-value-compact status-synced">${(motor.current || 0).toFixed(2)}A</span>
                    </div>
                    
                    <!-- Direction Buttons -->
                    <div class="control-group">
                        <div class="button-group" id="motor-direction-buttons-${motor.index}">
                            <button class="output-btn ${motor.direction ? 'output-btn-primary' : 'output-btn-secondary'}" 
                                    onclick="setDCMotorDirection(${motor.index}, true)">
                                <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M8.59,16.58L13.17,12L8.59,7.41L10,6L16,12L10,18L8.59,16.58Z" /></svg>
                                Forward
                            </button>
                            <button class="output-btn ${!motor.direction ? 'output-btn-primary' : 'output-btn-secondary'}" 
                                    onclick="setDCMotorDirection(${motor.index}, false)">
                                <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M15.41,16.58L10.83,12L15.41,7.41L14,6L8,12L14,18L15.41,16.58Z" /></svg>
                                Reverse
                            </button>
                        </div>
                    </div>
                    
                    <!-- Start/Stop Buttons -->
                    <div class="control-group">
                        <div class="button-group" id="motor-control-buttons-${motor.index}">
                            <button class="output-btn output-btn-success output-btn-wide" 
                                    onclick="startDCMotor(${motor.index})" 
                                    ${motor.running ? 'disabled' : ''}>
                                <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M8,5.14V19.14L19,12.14L8,5.14Z" /></svg>
                                Start
                            </button>
                            <button class="output-btn output-btn-danger output-btn-wide" 
                                    onclick="stopDCMotor(${motor.index})"
                                    ${!motor.running ? 'disabled' : ''}>
                                <svg viewBox="0 0 24 24" width="16" height="16" style="vertical-align: middle; margin-right: 4px;"><path fill="currentColor" d="M18,18H6V6H18V18Z" /></svg>
                                Stop
                            </button>
                        </div>
                    </div>
                </div>
            </div>
        `).join('');
    } else {
        // Selective update - only update values for each motor
        motors.forEach(motor => {
            const motorItem = container.querySelector(`[data-motor-index="${motor.index}"]`);
            if (!motorItem) return;
            
            // Update slider value only if not focused
            const slider = document.getElementById(`motor-slider-${motor.index}`);
            if (slider && !dcMotorSlidersFocused.get(motor.index)) {
                slider.value = motor.power || 0;
            }
            
            // Update slider display
            const display = document.getElementById(`motor-power-display-${motor.index}`);
            if (display && !dcMotorSlidersFocused.get(motor.index)) {
                display.textContent = `${motor.power || 0}%`;
            }
            
            // Update actual power display
            const actualPower = document.getElementById(`motor-actual-power-${motor.index}`);
            if (actualPower) {
                actualPower.textContent = `${motor.power || 0}%`;
            }
            
            // Update current display
            const current = document.getElementById(`motor-current-${motor.index}`);
            if (current) {
                current.textContent = `${(motor.current || 0).toFixed(2)}A`;
            }
            
            // Update status badges (running and direction)
            const statusBadges = motorItem.querySelectorAll('.output-status-badge');
            if (statusBadges.length >= 1) {
                // First badge: running status
                statusBadges[0].className = `output-status-badge ${motor.running ? 'running' : 'stopped'}`;
                statusBadges[0].textContent = motor.running ? 'RUNNING' : 'STOPPED';
            }
            if (statusBadges.length >= 2) {
                // Second badge: direction
                statusBadges[1].style.color = 'white';
                statusBadges[1].innerHTML = `<i class="mdi ${motor.direction ? 'mdi-axis-z-rotate-counterclockwise' : 'mdi-axis-z-rotate-clockwise'}"></i> ${motor.direction ? 'FORWARD' : 'REVERSE'}`;
            }
            
            // Update direction buttons
            const dirButtons = motorItem.querySelectorAll('#motor-direction-buttons-' + motor.index + ' button');
            if (dirButtons.length >= 2) {
                dirButtons[0].className = `output-btn ${motor.direction ? 'output-btn-primary' : 'output-btn-secondary'}`;
                dirButtons[1].className = `output-btn ${!motor.direction ? 'output-btn-primary' : 'output-btn-secondary'}`;
            }
            
            // Update start/stop buttons
            const controlButtons = motorItem.querySelectorAll('#motor-control-buttons-' + motor.index + ' button');
            if (controlButtons.length >= 2) {
                // Start button: disabled when motor is running
                controlButtons[0].disabled = motor.running;
                // Stop button: disabled when motor is stopped
                controlButtons[1].disabled = !motor.running;
            }
            
            // Update name
            const nameSpan = motorItem.querySelector('.output-name');
            if (nameSpan) {
                nameSpan.textContent = motor.name || `DC Motor ${motor.index - 26}`;
            }
            
            // Update dashboard icon
            const dashIcon = motorItem.querySelector('.dashboard-icon');
            if (dashIcon) {
                dashIcon.className = `dashboard-icon ${motor.d ? 'active' : 'inactive'}`;
            }
        });
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

function updateSliderValue(slider, displayId) {
    const display = document.getElementById(displayId);
    if (display) {
        display.textContent = slider.value + '%';
    }
}

function markControlActive(index) {
    activeControls.add(index);
}

function markControlInactive(index) {
    // Delay removal to allow command to complete
    setTimeout(() => {
        activeControls.delete(index);
    }, 100);
}

function updateOutputSliderDisplay(index, value) {
    // Update slider display as user drags (before command is sent)
    const display = document.getElementById(`slider-display-${index}`);
    if (display) {
        display.textContent = value + '%';
    }
}

function updateMotorPowerDisplay(motorIndex, value) {
    // Update the slider display as user drags
    const display = document.getElementById(`motor-power-display-${motorIndex}`);
    if (display) {
        display.textContent = value + '%';
    }
}

// ============================================================================
// OUTPUT CONTROL FUNCTIONS (Runtime - Not Saved)
// ============================================================================

async function setOutputState(index, state) {
    // Track pending command
    pendingCommands.set(index, state);
    
    try {
        const response = await fetch(`/api/output/${index}/state`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ state: state })
        });
        if (!response.ok) throw new Error('Failed to set output state');
    } catch (error) {
        console.error('Error setting output state:', error);
        showToast('error', 'Error', 'Failed to control output');
        pendingCommands.delete(index);
    }
}

async function setOutputValue(index, value) {
    // Track pending command
    const numValue = parseInt(value);
    pendingCommands.set(index, numValue);
    
    try {
        const response = await fetch(`/api/output/${index}/value`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ value: numValue })
        });
        if (!response.ok) throw new Error('Failed to set output value');
    } catch (error) {
        console.error('Error setting output value:', error);
        showToast('error', 'Error', 'Failed to control output');
        pendingCommands.delete(index);
    }
}

// Stepper motor runtime control functions
let stepperDirection = true;

async function setStepperRPM() {
    const rpm = document.getElementById('stepperRPM').value;
    try {
        const response = await fetch('/api/stepper/rpm', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ rpm: parseInt(rpm) })
        });
        if (!response.ok) throw new Error('Failed to set RPM');
    } catch (error) {
        console.error('Error setting stepper RPM:', error);
        showToast('error', 'Error', 'Failed to set RPM');
    }
}

async function setStepperDirection(forward) {
    stepperDirection = forward;
    try {
        const response = await fetch('/api/stepper/direction', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ forward: forward })
        });
        if (!response.ok) throw new Error('Failed to set direction');
        // Don't refresh immediately - let normal polling handle it to avoid race condition
        // The next poll (every 2 seconds) will update the UI with confirmed direction
    } catch (error) {
        console.error('Error setting stepper direction:', error);
        showToast('error', 'Error', 'Failed to set direction');
    }
}

async function startStepper() {
    const rpm = document.getElementById('stepperRPM').value;
    console.log(`[STEPPER] Start/Update clicked: RPM=${rpm}, direction=${stepperDirection ? 'Forward' : 'Reverse'}`);
    try {
        const response = await fetch('/api/stepper/start', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                rpm: parseInt(rpm),
                forward: stepperDirection
            })
        });
        if (!response.ok) throw new Error('Failed to start stepper');
        console.log('[STEPPER] Command sent successfully');
        showToast('success', 'Success', 'Stepper command sent');
        fetchAndRenderOutputs();
    } catch (error) {
        console.error('[STEPPER] Error:', error);
        showToast('error', 'Error', 'Failed to send command');
    }
}

async function stopStepper() {
    try {
        const response = await fetch('/api/stepper/stop', {
            method: 'POST'
        });
        if (!response.ok) throw new Error('Failed to stop stepper');
        showToast('success', 'Success', 'Stepper motor stopped');
        fetchAndRenderOutputs();
    } catch (error) {
        console.error('Error stopping stepper:', error);
        showToast('error', 'Error', 'Failed to stop motor');
    }
}

// DC Motor runtime control functions
async function setDCMotorPower(index, power) {
    try {
        const response = await fetch(`/api/dcmotor/${index}/power`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ power: parseInt(power) })
        });
        if (!response.ok) throw new Error('Failed to set power');
    } catch (error) {
        console.error('Error setting DC motor power:', error);
        showToast('error', 'Error', 'Failed to set power');
    }
}

async function setDCMotorDirection(index, forward) {
    try {
        const response = await fetch(`/api/dcmotor/${index}/direction`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ forward: forward })
        });
        if (!response.ok) throw new Error('Failed to set direction');
        // Don't refresh immediately - let normal polling handle it
    } catch (error) {
        console.error('Error setting DC motor direction:', error);
        showToast('error', 'Error', 'Failed to set direction');
    }
}

async function startDCMotor(index) {
    try {
        // Get current power setting from slider
        const slider = document.getElementById(`motor-slider-${index}`);
        const power = slider ? parseFloat(slider.value) : 0;
        
        // Get current direction from data (find motor in last fetched data)
        // For now, we'll read from the UI button states
        const dirButtons = document.querySelectorAll(`#motor-direction-buttons-${index} button`);
        const forward = dirButtons.length >= 2 ? 
            dirButtons[0].classList.contains('output-btn-primary') : true;
        
        console.log(`[DC MOTOR] Start clicked: index=${index}, power=${power}%, direction=${forward ? 'Forward' : 'Reverse'}`);
        
        const response = await fetch(`/api/dcmotor/${index}/start`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ 
                power: power,
                forward: forward
            })
        });
        if (!response.ok) throw new Error('Failed to start motor');
        console.log('[DC MOTOR] Start command sent successfully');
        showToast('success', 'Success', 'Motor started');
        fetchAndRenderOutputs();
    } catch (error) {
        console.error('[DC MOTOR] Start error:', error);
        showToast('error', 'Error', 'Failed to start motor');
    }
}

async function stopDCMotor(index) {
    try {
        console.log(`[DC MOTOR] Stop clicked: index=${index}`);
        const response = await fetch(`/api/dcmotor/${index}/stop`, {
            method: 'POST'
        });
        if (!response.ok) throw new Error('Failed to stop motor');
        console.log('[DC MOTOR] Stop command sent successfully');
        showToast('success', 'Success', 'Motor stopped');
        fetchAndRenderOutputs();
    } catch (error) {
        console.error('[DC MOTOR] Stop error:', error);
        showToast('error', 'Error', 'Failed to stop motor');
    }
}

// ============================================================================
// CONFIGURATION MODALS - Digital Outputs
// ============================================================================

let currentOutputIndex = null;
let outputConfigData = null;

async function openDigitalOutputConfigModal(index) {
    currentOutputIndex = index;
    
    try {
        const response = await fetch(`/api/config/output/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        outputConfigData = await response.json();
        
        // Populate form
        document.getElementById('outputConfigIndex').textContent = `[${index}]`;
        document.getElementById('outputConfigName').value = outputConfigData.name || '';
        document.getElementById('outputConfigMode').value = outputConfigData.mode || 0;
        document.getElementById('outputShowOnDashboard').checked = outputConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('digitalOutputConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error fetching output config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeDigitalOutputConfigModal() {
    document.getElementById('digitalOutputConfigModal').classList.remove('active');
    currentOutputIndex = null;
    outputConfigData = null;
}

async function saveDigitalOutputConfig() {
    if (currentOutputIndex === null) return;
    
    const configData = {
        index: currentOutputIndex,
        name: document.getElementById('outputConfigName').value,
        mode: parseInt(document.getElementById('outputConfigMode').value),
        showOnDashboard: document.getElementById('outputShowOnDashboard').checked
    };
    
    try {
        const response = await fetch(`/api/config/output/${currentOutputIndex}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        showToast('success', 'Success', 'Configuration saved successfully');
        closeDigitalOutputConfigModal();
        fetchAndRenderOutputs();
        
    } catch (error) {
        console.error('Error saving output config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

// ============================================================================
// CONFIGURATION MODALS - Stepper Motor
// ============================================================================

let stepperConfigData = null;

async function openStepperConfigModal() {
    try {
        const response = await fetch('/api/config/stepper');
        if (!response.ok) throw new Error('Failed to fetch config');
        
        stepperConfigData = await response.json();
        
        // Populate form
        document.getElementById('stepperConfigName').value = stepperConfigData.name || '';
        document.getElementById('stepperStepsPerRev').value = stepperConfigData.stepsPerRev || 200;
        document.getElementById('stepperMaxRPM').value = stepperConfigData.maxRPM || 500;
        document.getElementById('stepperHoldCurrent').value = stepperConfigData.holdCurrent_mA || 50;  // Safe default
        document.getElementById('stepperRunCurrent').value = stepperConfigData.runCurrent_mA || 100;  // Safe default
        document.getElementById('stepperAcceleration').value = stepperConfigData.acceleration || 100;
        document.getElementById('stepperInvertDirection').checked = stepperConfigData.invertDirection || false;
        document.getElementById('stepperShowOnDashboard').checked = stepperConfigData.showOnDashboard || false;
        
        // Populate TMC5130 advanced features
        document.getElementById('stepperStealthChopEnabled').checked = stepperConfigData.stealthChopEnabled || false;
        document.getElementById('stepperCoolStepEnabled').checked = stepperConfigData.coolStepEnabled || false;
        document.getElementById('stepperFullStepEnabled').checked = stepperConfigData.fullStepEnabled || false;
        document.getElementById('stepperStealthChopMaxRPM').value = stepperConfigData.stealthChopMaxRPM || 100.0;
        document.getElementById('stepperCoolStepMinRPM').value = stepperConfigData.coolStepMinRPM || 200.0;
        document.getElementById('stepperFullStepMinRPM').value = stepperConfigData.fullStepMinRPM || 300.0;
        
        // Show modal
        document.getElementById('stepperConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error fetching stepper config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeStepperConfigModal() {
    document.getElementById('stepperConfigModal').classList.remove('active');
    stepperConfigData = null;
}

async function saveStepperConfig() {
    const configData = {
        name: document.getElementById('stepperConfigName').value,
        stepsPerRev: parseInt(document.getElementById('stepperStepsPerRev').value),
        maxRPM: parseInt(document.getElementById('stepperMaxRPM').value),
        holdCurrent_mA: parseInt(document.getElementById('stepperHoldCurrent').value),
        runCurrent_mA: parseInt(document.getElementById('stepperRunCurrent').value),
        acceleration: parseInt(document.getElementById('stepperAcceleration').value),
        invertDirection: document.getElementById('stepperInvertDirection').checked,
        showOnDashboard: document.getElementById('stepperShowOnDashboard').checked,
        
        // TMC5130 advanced features
        stealthChopEnabled: document.getElementById('stepperStealthChopEnabled').checked,
        coolStepEnabled: document.getElementById('stepperCoolStepEnabled').checked,
        fullStepEnabled: document.getElementById('stepperFullStepEnabled').checked,
        stealthChopMaxRPM: parseFloat(document.getElementById('stepperStealthChopMaxRPM').value),
        coolStepMinRPM: parseFloat(document.getElementById('stepperCoolStepMinRPM').value),
        fullStepMinRPM: parseFloat(document.getElementById('stepperFullStepMinRPM').value)
    };
    
    // Validate configuration (TMC5130 hardware constraints)
    if (configData.holdCurrent_mA < 1 || configData.holdCurrent_mA > 1000) {
        showToast('error', 'Validation Error', 'Hold current must be 1-1000 mA');
        return;
    }
    
    if (configData.runCurrent_mA < 1 || configData.runCurrent_mA > 1800) {
        showToast('error', 'Validation Error', 'Run current must be 1-1800 mA');
        return;
    }
    
    if (configData.maxRPM < 1 || configData.maxRPM > 3000) {
        showToast('error', 'Validation Error', 'Max RPM must be 1-3000');
        return;
    }
    
    if (configData.acceleration < 1 || configData.acceleration > configData.maxRPM) {
        showToast('error', 'Validation Error', `Acceleration must be 1-${configData.maxRPM} RPM/s`);
        return;
    }
    
    if (configData.stepsPerRev < 1 || configData.stepsPerRev > 10000) {
        showToast('error', 'Validation Error', 'Steps per revolution must be 1-10000');
        return;
    }
    
    // Validate RPM thresholds: StealthChopMaxRPM < CoolStepMinRPM < FullStepMinRPM < MaxRPM
    if (configData.stealthChopMaxRPM >= configData.coolStepMinRPM) {
        showToast('error', 'Validation Error', 'StealthChop Max RPM must be less than CoolStep Min RPM');
        return;
    }
    
    if (configData.coolStepMinRPM >= configData.fullStepMinRPM) {
        showToast('error', 'Validation Error', 'CoolStep Min RPM must be less than FullStep Min RPM');
        return;
    }
    
    if (configData.fullStepMinRPM >= configData.maxRPM) {
        showToast('error', 'Validation Error', 'FullStep Min RPM must be less than Max RPM');
        return;
    }
    
    try {
        const response = await fetch('/api/config/stepper', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const errorData = await response.json().catch(() => ({}));
            throw new Error(errorData.error || 'Failed to save config');
        }
        
        showToast('success', 'Success', 'Configuration saved successfully');
        closeStepperConfigModal();
        fetchAndRenderOutputs();
        
    } catch (error) {
        console.error('Error saving stepper config:', error);
        showToast('error', 'Error', error.message || 'Failed to save configuration');
    }
}

// ============================================================================
// CONFIGURATION MODALS - DC Motors
// ============================================================================

let currentDCMotorIndex = null;
let dcMotorConfigData = null;

async function openDCMotorConfigModal(index) {
    currentDCMotorIndex = index;
    
    try {
        const response = await fetch(`/api/config/dcmotor/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        dcMotorConfigData = await response.json();
        
        // Populate form
        document.getElementById('dcMotorConfigIndex').textContent = `${index - 26}`;
        document.getElementById('dcMotorConfigName').value = dcMotorConfigData.name || '';
        document.getElementById('dcMotorInvertDirection').checked = dcMotorConfigData.invertDirection || false;
        document.getElementById('dcMotorShowOnDashboard').checked = dcMotorConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('dcMotorConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error fetching DC motor config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeDCMotorConfigModal() {
    document.getElementById('dcMotorConfigModal').classList.remove('active');
    currentDCMotorIndex = null;
    dcMotorConfigData = null;
}

async function saveDCMotorConfig() {
    if (currentDCMotorIndex === null) return;
    
    const configData = {
        index: currentDCMotorIndex,
        name: document.getElementById('dcMotorConfigName').value,
        invertDirection: document.getElementById('dcMotorInvertDirection').checked,
        showOnDashboard: document.getElementById('dcMotorShowOnDashboard').checked
    };
    
    try {
        const response = await fetch(`/api/config/dcmotor/${currentDCMotorIndex}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        showToast('success', 'Success', 'Configuration saved successfully');
        closeDCMotorConfigModal();
        fetchAndRenderOutputs();
        
    } catch (error) {
        console.error('Error saving DC motor config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

// ============================================================================
// COM PORTS TAB - Serial Communication Port Configuration
// ============================================================================

function initComPortsTab() {
    // Use PollingManager - polls every 2 seconds while comports tab is active
    PollingManager.startPolling('comports-api', fetchAndRenderComPorts, 2000);
}

async function fetchAndRenderComPorts() {
    const ENDPOINT = '/api/comports';
    
    try {
        const response = await fetch(ENDPOINT);
        if (!response.ok) throw new Error('Failed to fetch COM ports');
        
        const data = await response.json();
        
        // Record success and cache data
        PollingManager.recordSuccess(ENDPOINT, data);
        
        // Render COM ports
        renderComPorts(data.ports || []);
        
    } catch (error) {
        console.error('Error fetching COM ports:', error);
        
        // Record error and check if we should show error UI
        const errorCount = PollingManager.recordError(ENDPOINT);
        
        if (PollingManager.shouldShowError(ENDPOINT)) {
            // Enough consecutive errors - show error UI
            showComPortError();
        } else {
            // Transient error - use cached data if available
            const cachedData = PollingManager.getCachedData(ENDPOINT);
            if (cachedData) {
                console.log(`[ComPorts] Using cached data (error ${errorCount}/${PollingManager.ERROR_THRESHOLD})`);
                renderComPorts(cachedData.ports || []);
            }
        }
    }
}

function renderComPorts(ports) {
    const container = document.getElementById('comports-list');
    if (!container) return;
    
    if (ports.length === 0) {
        container.innerHTML = '<div class="empty-message">No COM ports configured</div>';
        return;
    }
    
    // Port type names
    const portTypes = {
        0: 'RS-232 Port 1',
        1: 'RS-232 Port 2',
        2: 'RS-485 Port 1',
        3: 'RS-485 Port 2'
    };
    
    // Status badge styling
    const getStatusBadge = (hasError) => {
        if (hasError) {
            return '<span class="status-badge status-error">Comms Error</span>';
        }
        return '<span class="status-badge status-ok">OK</span>';
    };
    
    container.innerHTML = ports.map(port => `
        <div class="comport-item">
            <div class="comport-header">
                <div class="comport-header-left">
                    <span class="comport-type">${portTypes[port.index] || `Port ${port.index + 1}`}</span>
                    <span class="comport-name">${port.name || 'Unnamed'}</span>
                </div>
                <div class="comport-header-right">
                    ${getStatusBadge(port.error)}
                    <span class="dashboard-icon ${port.d ? 'active' : 'inactive'}" title="${port.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                        ${getDashboardIconSVG()}
                    </span>
                    <button class="icon-btn" onclick="openComPortConfigModal(${port.index})" title="Configure">
                        ${getConfigIconSVG()}
                    </button>
                </div>
            </div>
            <div class="comport-details">
                <div class="comport-config-item">
                    <span class="config-label">Baud Rate:</span>
                    <span class="config-value">${port.baud}</span>
                </div>
                <div class="comport-config-item">
                    <span class="config-label">Data Bits:</span>
                    <span class="config-value">${port.dataBits || 8}</span>
                </div>
                <div class="comport-config-item">
                    <span class="config-label">Parity:</span>
                    <span class="config-value">${getParityName(port.parity)}</span>
                </div>
                <div class="comport-config-item">
                    <span class="config-label">Stop Bits:</span>
                    <span class="config-value">${port.stopBits || 1}</span>
                </div>
            </div>
        </div>
    `).join('');
}

function getParityName(parity) {
    const parityNames = {
        0: 'None',
        1: 'Odd',
        2: 'Even'
    };
    return parityNames[parity] || 'Unknown';
}

function showComPortError() {
    const container = document.getElementById('comports-list');
    if (container) {
        container.innerHTML = '<div class="error-message">Failed to load COM ports</div>';
    }
}

// ============================================================================
// COM PORT CONFIGURATION MODAL
// ============================================================================

let currentComPortIndex = null;
let comPortConfigData = null;

async function openComPortConfigModal(index) {
    currentComPortIndex = index;
    
    // Fetch current configuration
    try {
        const response = await fetch(`/api/config/comport/${index}`);
        if (!response.ok) throw new Error('Failed to fetch config');
        
        comPortConfigData = await response.json();
        
        // Port type names
        const portTypes = {
            0: 'RS-232 Port 1',
            1: 'RS-232 Port 2',
            2: 'RS-485 Port 1',
            3: 'RS-485 Port 2'
        };
        
        // Populate form
        document.getElementById('comportConfigTitle').textContent = portTypes[index] || `Port ${index + 1}`;
        document.getElementById('comportConfigName').value = comPortConfigData.name || '';
        document.getElementById('comportConfigBaud').value = comPortConfigData.baudRate || 9600;
        document.getElementById('comportConfigParity').value = comPortConfigData.parity || 0;
        document.getElementById('comportConfigStopBits').value = comPortConfigData.stopBits || 1;
        document.getElementById('comportShowOnDashboard').checked = comPortConfigData.showOnDashboard || false;
        
        // Show modal
        document.getElementById('comportConfigModal').classList.add('active');
        
    } catch (error) {
        console.error('Error fetching COM port config:', error);
        showToast('error', 'Error', 'Failed to load configuration');
    }
}

function closeComPortConfigModal() {
    document.getElementById('comportConfigModal').classList.remove('active');
    currentComPortIndex = null;
    comPortConfigData = null;
    
    // Reset form
    document.getElementById('comportConfigName').value = '';
    document.getElementById('comportConfigBaud').value = '9600';
    document.getElementById('comportConfigParity').value = '0';
    document.getElementById('comportConfigStopBits').value = '1';
    document.getElementById('comportShowOnDashboard').checked = false;
}

async function saveComPortConfig() {
    if (currentComPortIndex === null) return;
    
    // Get values from form
    const name = document.getElementById('comportConfigName').value;
    const baudRate = parseInt(document.getElementById('comportConfigBaud').value);
    const parity = parseInt(document.getElementById('comportConfigParity').value);
    const stopBits = parseFloat(document.getElementById('comportConfigStopBits').value);
    
    const configData = {
        index: currentComPortIndex,
        name: name,
        baudRate: baudRate,
        dataBits: 8,  // Fixed to 8 for Modbus
        parity: parity,
        stopBits: stopBits,
        showOnDashboard: document.getElementById('comportShowOnDashboard').checked,
        enabled: true
    };
    
    try {
        const response = await fetch(`/api/config/comport/${currentComPortIndex}`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        const result = await response.json();
        
        // Port type names
        const portTypes = ['RS-232 Port 1', 'RS-232 Port 2', 'RS-485 Port 1', 'RS-485 Port 2'];
        const portName = portTypes[currentComPortIndex] || `Port ${currentComPortIndex + 1}`;
        
        showToast('success', 'Success', `Configuration saved for ${portName}`);
        closeComPortConfigModal();
        
        // Refresh COM ports display
        fetchAndRenderComPorts();
        
    } catch (error) {
        console.error('Error saving COM port config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
    }
}

// ============================================================================
// Devices Management JavaScript
// ============================================================================

// Driver definitions
const DRIVER_DEFINITIONS = {
    // Modbus drivers
    0: { name: 'Hamilton pH Probe', interface: 0, description: 'Hamilton Modbus pH/Temperature probe', hasSetpoint: false },
    1: { name: 'Hamilton DO Probe', interface: 0, description: 'Hamilton Modbus Dissolved Oxygen probe', hasSetpoint: false },
    2: { name: 'Hamilton OD Probe', interface: 0, description: 'Hamilton Modbus Optical Density probe', hasSetpoint: false },
    3: { name: 'Alicat MFC', interface: 0, description: 'Alicat Modbus MFC', hasSetpoint: true },
    
    // Analogue IO drivers
    10: { name: 'Pressure Controller', interface: 1, description: '0-10V analogue pressure controller', hasSetpoint: true }
};

const INTERFACE_NAMES = {
    0: 'Modbus RTU',
    1: 'Analogue I/O'
};

let currentDeviceIndex = -1; // For editing devices
let deviceControlData = {}; // Cache for device control objects
// Note: devicesPollingInterval removed - now using PollingManager

// ============================================================================
// Device Control Data Polling
// ============================================================================

async function fetchDeviceControlData() {
    try {
        // Fetch object cache data for control indices 50-69
        const response = await fetch('/api/inputs');  // This returns all cached objects
        if (!response.ok) return;
        
        const data = await response.json();
        
        // Extract device control objects (indices 50-69)
        // Note: The API doesn't have a specific endpoint yet, so we'll poll devices endpoint
        // which should include control object data
        const devicesResponse = await fetch('/api/devices');
        if (devicesResponse.ok) {
            const devicesData = await devicesResponse.json();
            
            // Update device cards with control data
            if (devicesData.devices && devicesData.devices.length > 0) {
                updateDeviceControlStatus(devicesData.devices);
            }
        }
    } catch (error) {
        console.error('Error fetching device control data:', error);
    }
}

/**
 * Calculate device control index
 * 
 * This mirrors the C++ function getDeviceControlIndex() in ioConfig.cpp
 * 
 * Simplified architecture: ALL devices follow same pattern:
 * - Sensor index (dynamicIndex): 70-99
 * - Control index: dynamicIndex - 20 → 50-69
 * 
 * Even control-only devices (like pressure controller) allocate a sensor slot
 * for storing actual/feedback values for diagnostics.
 * 
 * @param {number} dynamicIndex - Device sensor index (70-99)
 * @returns {number} Control object index (50-69)
 */
function getDeviceControlIndex(dynamicIndex) {
    // All devices: control index = sensor index - 20
    // Maps 70-99 → 50-69
    return dynamicIndex - 20;
}

function updateDeviceControlStatus(devices) {
    devices.forEach(device => {
        // Use centralized function for control index calculation
        const controlIndex = getDeviceControlIndex(device.dynamicIndex);
        
        // Find the device card
        const card = document.querySelector(`.device-card[data-control-index="${controlIndex}"]`);
        if (!card) return;
        
        // Update connected status (only for non-analogue devices)
        const statusDiv = card.querySelector('.device-status');
        if (statusDiv && device.interfaceType !== 1) {  // Skip analogue devices
            const isConnected = device.connected === true;  // Only true if explicitly connected
            statusDiv.className = `device-status ${isConnected ? 'status-online' : 'status-offline'}`;
            statusDiv.textContent = isConnected ? 'Connected' : 'Disconnected';
            
            if (device.fault) {
                statusDiv.className = 'device-status status-fault';
                statusDiv.textContent = 'FAULT';
            }
        } else if (statusDiv && device.interfaceType === 1) {
            // For analogue devices, only show fault status
            if (device.fault) {
                statusDiv.className = 'device-status status-fault';
                statusDiv.textContent = 'FAULT';
            } else {
                statusDiv.style.display = 'none';  // Hide status completely
            }
        }
        
        // Update control values if they exist
        const setpointValue = card.querySelector('.setpoint-value');
        const actualValue = card.querySelector('.actual-value');
        const message = card.querySelector('.device-message');
        
        console.log(`[DEVICES] Device ${controlIndex}: setpoint=${device.setpoint}, actualValue=${device.actualValue}, unit=${device.unit}`);
        
        if (setpointValue && device.setpoint !== undefined && device.setpoint !== null) {
            setpointValue.textContent = `${device.setpoint.toFixed(2)} ${device.unit || ''}`;
        }
        
        if (actualValue && device.actualValue !== undefined && device.actualValue !== null) {
            actualValue.textContent = `${device.actualValue.toFixed(2)} ${device.unit || ''}`;
        }
        
        if (message) {
            if (device.message) {
                message.textContent = device.message;
                message.style.display = 'block';
                
                // Update message class based on device state
                if (device.fault) {
                    message.className = 'device-message device-message-fault';
                } else if (device.connected) {
                    message.className = 'device-message device-message-connected';
                } else {
                    message.className = 'device-message device-message-disconnected';
                }
            } else {
                message.style.display = 'none';
            }
        }
    });
}

// NOTE: startDevicesPolling and stopDevicesPolling are deprecated
// Polling is now handled by PollingManager in initDevicesTab()
function startDevicesPolling() {
    // Legacy function - now handled by PollingManager
    console.log('[DEVICES] startDevicesPolling called - now using PollingManager');
}

function stopDevicesPolling() {
    // Legacy function - now handled by PollingManager
    console.log('[DEVICES] stopDevicesPolling called - now using PollingManager');
}

// ============================================================================
// Modal Management
// ============================================================================

function openAddDeviceModal() {
    // Reset form
    document.getElementById('deviceInterfaceType').value = '';
    document.getElementById('deviceDriverType').value = '';
    document.getElementById('deviceName').value = '';
    
    // Hide all dynamic sections
    document.getElementById('driverTypeGroup').style.display = 'none';
    document.getElementById('deviceCommonFields').style.display = 'none';
    document.getElementById('modbusFields').style.display = 'none';
    document.getElementById('analogueIOFields').style.display = 'none';
    
    // Show modal
    const modal = document.getElementById('addDeviceModal');
    modal.style.display = 'flex';
    modal.classList.add('active');
}

function closeAddDeviceModal() {
    const modal = document.getElementById('addDeviceModal');
    modal.style.display = 'none';
    modal.classList.remove('active');
}

function closeDeviceConfigModal() {
    const modal = document.getElementById('deviceConfigModal');
    modal.style.display = 'none';
    modal.classList.remove('active');
}

// ============================================================================
// Dynamic Form Updates
// ============================================================================

function updateDriverOptions() {
    const interfaceType = parseInt(document.getElementById('deviceInterfaceType').value);
    const driverSelect = document.getElementById('deviceDriverType');
    const driverGroup = document.getElementById('driverTypeGroup');
    
    // Clear existing options
    driverSelect.innerHTML = '<option value="">-- Select Driver --</option>';
    
    if (isNaN(interfaceType)) {
        driverGroup.style.display = 'none';
        return;
    }
    
    // Populate drivers for selected interface
    for (const [driverId, driverInfo] of Object.entries(DRIVER_DEFINITIONS)) {
        if (driverInfo.interface === interfaceType) {
            const option = document.createElement('option');
            option.value = driverId;
            option.textContent = driverInfo.name;
            driverSelect.appendChild(option);
        }
    }
    
    driverGroup.style.display = 'block';
    
    // Hide other sections
    document.getElementById('deviceCommonFields').style.display = 'none';
    document.getElementById('modbusFields').style.display = 'none';
    document.getElementById('analogueIOFields').style.display = 'none';
}

function updateDeviceFields() {
    const driverType = parseInt(document.getElementById('deviceDriverType').value);
    const interfaceType = parseInt(document.getElementById('deviceInterfaceType').value);
    
    if (isNaN(driverType)) {
        document.getElementById('deviceCommonFields').style.display = 'none';
        document.getElementById('modbusFields').style.display = 'none';
        document.getElementById('analogueIOFields').style.display = 'none';
        return;
    }
    
    // Show common fields
    document.getElementById('deviceCommonFields').style.display = 'block';
    
    // Show interface-specific fields
    document.getElementById('modbusFields').style.display = (interfaceType === 0) ? 'block' : 'none';
    document.getElementById('analogueIOFields').style.display = (interfaceType === 1) ? 'block' : 'none';
    
    // Set default device name based on driver
    const driverInfo = DRIVER_DEFINITIONS[driverType];
    if (driverInfo) {
        document.getElementById('deviceName').value = driverInfo.name;
    }
}

// ============================================================================
// Device Management
// ============================================================================

async function loadDevices() {
    try {
        const response = await fetch('/api/devices');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        
        const data = await response.json();
        displayDevices(data.devices || []);
    } catch (error) {
        console.error('Error loading devices:', error);
        document.getElementById('devices-list').innerHTML = 
            '<div class="error-message">Failed to load devices</div>';
    }
}

function displayDevices(devices) {
    const container = document.getElementById('devices-list');
    
    if (!devices || devices.length === 0) {
        container.innerHTML = '<div class="empty-message">No devices configured. Click "Add New Device" to get started.</div>';
        return;
    }
    
    container.innerHTML = '';
    
    devices.forEach(device => {
        const card = createDeviceCard(device);
        container.appendChild(card);
    });
}

function createDeviceCard(device) {
    const card = document.createElement('div');
    card.className = 'device-card';
    
    // Use centralized function for control index calculation
    const controlIndex = getDeviceControlIndex(device.dynamicIndex);
    card.setAttribute('data-control-index', controlIndex);
    
    const driverInfo = DRIVER_DEFINITIONS[device.driverType];
    const interfaceName = INTERFACE_NAMES[device.interfaceType];
    
    // Status indicator - only show for non-analogue devices
    const showStatus = device.interfaceType !== 1;  // Don't show for Analogue IO
    const statusClass = device.fault ? 'status-fault' : 'status-online';
    const statusText = device.fault ? 'FAULT' : 'Connected';
    
    card.innerHTML = `
        <div class="output-header">
            <div class="output-header-left">
                <span class="output-name">${device.name}</span>
                <span class="output-mode-badge">${driverInfo ? driverInfo.name : 'Unknown'}</span>
            </div>
            <div class="output-header-right">
                ${showStatus ? `<div class="device-status ${statusClass}">${statusText}</div>` : ''}
                <button class="icon-btn" onclick="openDeviceConfig(${device.dynamicIndex})" title="Configure">
                    ${getConfigIconSVG()}
                </button>
            </div>
        </div>
        
        <!-- Device Configuration Info -->
        <div class="device-info">
            <div class="device-detail">
                <strong>Interface:</strong> ${interfaceName}
            </div>
            <div class="device-detail">
                <strong>Sensor Index:</strong> ${device.dynamicIndex}
            </div>
            <div class="device-detail">
                <strong>Control Index:</strong> ${controlIndex}
            </div>
            ${getDeviceDetailsHTML(device)}
        </div>
        
        <!-- Device Control Section -->
        <div class="device-control-section">
            <div class="device-control-row">
                ${driverInfo && driverInfo.hasSetpoint ? `
                    <div class="control-item">
                        <span class="control-label">Setpoint:</span>
                        <span class="setpoint-value">-- --</span>
                    </div>
                ` : ''}
                <div class="control-item">
                    <span class="control-label">${driverInfo && driverInfo.hasSetpoint ? 'Actual:' : 'Value:'}</span>
                    <span class="actual-value">-- --</span>
                </div>
            </div>
            
            ${driverInfo && driverInfo.hasSetpoint ? `
                <div class="device-control-input">
                    <input type="number" 
                           id="setpoint-input-${controlIndex}" 
                           class="setpoint-input" 
                           step="0.1" 
                           placeholder="Enter setpoint">
                    <button class="output-btn output-btn-primary" 
                            onclick="sendSetpoint(${controlIndex})">
                        Set
                    </button>
                </div>
            ` : ''}
            
            <div class="device-message device-message-disconnected" style="display: none;"></div>
        </div>
    `;
    
    return card;
}

function getDeviceDetailsHTML(device) {
    let html = '';
    
    if (device.interfaceType === 0) { // Modbus
        const portNames = ['RS-232 Port 1', 'RS-232 Port 2', 'RS-485 Port 1', 'RS-485 Port 2'];
        html += `
            <div class="device-detail">
                <strong>Port:</strong> ${portNames[device.portIndex] || 'Unknown'}
            </div>
            <div class="device-detail">
                <strong>Slave ID:</strong> ${device.slaveID}
            </div>
        `;
    } else if (device.interfaceType === 1) { // Analogue IO
        html += `
            <div class="device-detail">
                <strong>DAC Output:</strong> ${device.dacOutputIndex + 1}
            </div>
            <div class="device-detail">
                <strong>Calibration:</strong> scale=${device.scale ? device.scale.toFixed(2) : 'N/A'} Pa/mV
            </div>
            <div class="device-detail">
                <strong>Unit:</strong> ${device.unit}
            </div>
        `;
    }
    
    return html;
}

async function createDevice() {
    const interfaceType = parseInt(document.getElementById('deviceInterfaceType').value);
    const driverType = parseInt(document.getElementById('deviceDriverType').value);
    const name = document.getElementById('deviceName').value.trim();
    
    // Validation
    if (isNaN(interfaceType) || isNaN(driverType)) {
        showToast('error', 'Validation Error', 'Please select interface and driver type');
        return;
    }
    
    if (!name) {
        showToast('error', 'Validation Error', 'Please enter a device name');
        return;
    }
    
    // Build device object
    const deviceData = {
        interfaceType: interfaceType,
        driverType: driverType,
        name: name
    };
    
    // Add interface-specific parameters
    if (interfaceType === 0) { // Modbus
        deviceData.portIndex = parseInt(document.getElementById('deviceModbusPort').value);
        deviceData.slaveID = parseInt(document.getElementById('deviceModbusSlaveID').value);
        
        if (isNaN(deviceData.slaveID) || deviceData.slaveID < 1 || deviceData.slaveID > 247) {
            showToast('error', 'Validation Error', 'Slave ID must be between 1 and 247');
            return;
        }
    } else if (interfaceType === 1) { // Analogue IO
        deviceData.dacOutputIndex = parseInt(document.getElementById('deviceDacOutput').value);
        deviceData.unit = document.getElementById('devicePressureUnit').value;
        
        // Get scale/offset from hidden fields (populated by Calculate button)
        const scale = parseFloat(document.getElementById('deviceCalScale').value);
        const offset = parseFloat(document.getElementById('deviceCalOffset').value);
        
        if (isNaN(scale) || isNaN(offset)) {
            showToast('error', 'Validation Error', 'Invalid scale or offset values');
            return;
        }
        
        deviceData.scale = scale;
        deviceData.offset = offset;
        
        console.log(`[PRESSURE] Calibration: scale=${scale.toFixed(2)}, offset=${offset.toFixed(2)}, unit=${deviceData.unit}`);
    }
    
    // Send to API
    try {
        const response = await fetch('/api/devices', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(deviceData)
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.message || 'Failed to create device');
        }
        
        const result = await response.json();
        showToast('success', 'Device Created', `Device "${name}" created successfully (Index ${result.dynamicIndex})`);
        closeAddDeviceModal();
        loadDevices();
    } catch (error) {
        console.error('Error creating device:', error);
        showToast('error', 'Error', error.message);
    }
}

async function openDeviceConfig(dynamicIndex) {
    currentDeviceIndex = dynamicIndex;
    
    try {
        const response = await fetch(`/api/devices/${dynamicIndex}`);
        if (!response.ok) throw new Error('Failed to load device configuration');
        
        const device = await response.json();
        displayDeviceConfig(device);
        
        const modal = document.getElementById('deviceConfigModal');
        modal.style.display = 'flex';
        modal.classList.add('active');
    } catch (error) {
        console.error('Error loading device config:', error);
        showToast('error', 'Error', 'Failed to load device configuration');
    }
}

function displayDeviceConfig(device) {
    const container = document.getElementById('deviceConfigContent');
    const driverInfo = DRIVER_DEFINITIONS[device.driverType];
    
    let html = `
        <div class="config-info">
            <p><strong>Driver:</strong> ${driverInfo ? driverInfo.name : 'Unknown'}</p>
            <p><strong>Interface:</strong> ${INTERFACE_NAMES[device.interfaceType]}</p>
            <p><strong>Dynamic Index:</strong> ${device.dynamicIndex}</p>
        </div>
        
        <!-- Device Name (always editable) -->
        <div class="form-group">
            <label for="configDeviceName">Device Name:</label>
            <input type="text" id="configDeviceName" value="${device.name}" maxlength="39" required>
        </div>
    `;
    
    if (device.interfaceType === 0) { // Modbus
        html += `
            <div class="form-group">
                <label for="configModbusPort">COM Port:</label>
                <select id="configModbusPort">
                    <option value="0" ${device.portIndex === 0 ? 'selected' : ''}>RS-232 Port 1</option>
                    <option value="1" ${device.portIndex === 1 ? 'selected' : ''}>RS-232 Port 2</option>
                    <option value="2" ${device.portIndex === 2 ? 'selected' : ''}>RS-485 Port 1</option>
                    <option value="3" ${device.portIndex === 3 ? 'selected' : ''}>RS-485 Port 2</option>
                </select>
            </div>
            <div class="form-group">
                <label for="configModbusSlaveID">Slave ID (1-247):</label>
                <input type="number" id="configModbusSlaveID" value="${device.slaveID}" min="1" max="247" required>
            </div>
        `;
    } else if (device.interfaceType === 1) { // Analogue IO
        html += `
            <div class="form-group">
                <label for="configDacOutput">DAC Output:</label>
                <select id="configDacOutput">
                    <option value="0" ${device.dacOutputIndex === 0 ? 'selected' : ''}>Analog Output 1</option>
                    <option value="1" ${device.dacOutputIndex === 1 ? 'selected' : ''}>Analog Output 2</option>
                </select>
            </div>
            <div class="form-group">
                <label for="configPressureUnit">Unit:</label>
                <select id="configPressureUnit" onchange="updatePressureCalibrationUnits()">
                    <option value="Pa" ${device.unit === 'Pa' ? 'selected' : ''}>Pa (Pascal)</option>
                    <option value="kPa" ${device.unit === 'kPa' ? 'selected' : ''}>kPa (Kilopascal)</option>
                    <option value="bar" ${device.unit === 'bar' ? 'selected' : ''}>bar</option>
                    <option value="PSI" ${device.unit === 'PSI' ? 'selected' : ''}>PSI (Pounds/sq.in)</option>
                    <option value="atm" ${device.unit === 'atm' ? 'selected' : ''}>atm (Atmosphere)</option>
                    <option value="mbar" ${device.unit === 'mbar' ? 'selected' : ''}>mbar (Millibar)</option>
                </select>
            </div>
            
            <h4 style="margin-top: 20px;">Calibration</h4>
            
            <div style="background: #f5f5f5; padding: 15px; border-radius: 4px; margin: 10px 0;">
                <strong>Point 1 (Low)</strong>
                <div class="form-row" style="margin-top: 10px;">
                    <div class="form-group">
                        <label for="configCalVoltage1">Voltage (mV):</label>
                        <input type="number" id="configCalVoltage1" placeholder="e.g., 0" step="0.1">
                    </div>
                    <div class="form-group">
                        <label for="configCalPressure1"><span id="configPressureLabel1">Pressure (${device.unit}):</span></label>
                        <input type="number" id="configCalPressure1" placeholder="e.g., 0" step="0.001">
                    </div>
                </div>
            </div>
            
            <div style="background: #f5f5f5; padding: 15px; border-radius: 4px; margin: 10px 0;">
                <strong>Point 2 (High)</strong>
                <div class="form-row" style="margin-top: 10px;">
                    <div class="form-group">
                        <label for="configCalVoltage2">Voltage (mV):</label>
                        <input type="number" id="configCalVoltage2" placeholder="e.g., 10000" step="0.1">
                    </div>
                    <div class="form-group">
                        <label for="configCalPressure2"><span id="configPressureLabel2">Pressure (${device.unit}):</span></label>
                        <input type="number" id="configCalPressure2" placeholder="e.g., 10" step="0.001">
                    </div>
                </div>
            </div>
            
            <button type="button" onclick="calculatePressureCalibration()" 
                    style="width: 100%; padding: 10px; margin: 15px 0 10px 0; background: #ddd; border: none; border-radius: 4px; cursor: pointer;">
                Calculate
            </button>
            
            <div style="background: #f5f5f5; padding: 15px; border-radius: 4px; margin: 10px 0;">
                <div style="display: flex; justify-content: space-between; margin-bottom: 8px;">
                    <span>Calculated Scale:</span>
                    <span id="configCalScaleResult" style="color: #27ae60; font-weight: bold;">${device.scale ? device.scale.toFixed(4) : '100.0000'}</span>
                </div>
                <div style="display: flex; justify-content: space-between;">
                    <span>Calculated Offset:</span>
                    <span id="configCalOffsetResult" style="color: #27ae60; font-weight: bold;">${device.offset ? device.offset.toFixed(2) : '0.00'}</span>
                </div>
                
                <input type="hidden" id="configCalScale" value="${device.scale || 100}">
                <input type="hidden" id="configCalOffset" value="${device.offset || 0}">
                
                <button type="button" onclick="resetPressureCalibration()" 
                        style="width: 100%; padding: 8px; margin-top: 15px; background: #ddd; border: none; border-radius: 4px; cursor: pointer;">
                    Reset to Default (100.0, 0.0)
                </button>
            </div>
        `;
    }
    
    container.innerHTML = html;
}

async function saveDeviceConfig() {
    if (currentDeviceIndex < 0) return;
    
    try {
        // Get device to check interface type
        const getResponse = await fetch(`/api/devices/${currentDeviceIndex}`);
        if (!getResponse.ok) throw new Error('Failed to load device');
        const device = await getResponse.json();
        
        // Build update data
        const updateData = {
            name: document.getElementById('configDeviceName').value.trim()
        };
        
        // Validate name
        if (!updateData.name) {
            showToast('error', 'Validation Error', 'Please enter a device name');
            return;
        }
        
        // Add interface-specific fields
        if (device.interfaceType === 0) { // Modbus
            updateData.portIndex = parseInt(document.getElementById('configModbusPort').value);
            updateData.slaveID = parseInt(document.getElementById('configModbusSlaveID').value);
            
            if (isNaN(updateData.slaveID) || updateData.slaveID < 1 || updateData.slaveID > 247) {
                showToast('error', 'Validation Error', 'Slave ID must be between 1 and 247');
                return;
            }
        } else if (device.interfaceType === 1) { // Analogue IO
            updateData.dacOutputIndex = parseInt(document.getElementById('configDacOutput').value);
            updateData.unit = document.getElementById('configPressureUnit').value;
            
            // Get scale/offset directly from input fields (populated by Calculate button)
            const scale = parseFloat(document.getElementById('configCalScale').value);
            const offset = parseFloat(document.getElementById('configCalOffset').value);
            
            if (isNaN(scale) || isNaN(offset)) {
                showToast('error', 'Validation Error', 'Invalid scale or offset values');
                return;
            }
            
            updateData.scale = scale;
            updateData.offset = offset;
            
            console.log(`[PRESSURE] Calibration: scale=${scale.toFixed(2)}, offset=${offset.toFixed(2)}, unit=${updateData.unit}`);
        }
        
        // Send update request
        const response = await fetch(`/api/devices/${currentDeviceIndex}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(updateData)
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to update device');
        }
        
        showToast('success', 'Device Updated', 'Device configuration updated successfully');
        closeDeviceConfigModal();
        loadDevices();
    } catch (error) {
        console.error('Error updating device:', error);
        showToast('error', 'Error', error.message);
    }
}

async function deleteDevice() {
    if (currentDeviceIndex < 0) return;
    
    if (!confirm('Are you sure you want to delete this device? Any associated sensors will also be removed.')) {
        return;
    }
    
    try {
        const response = await fetch(`/api/devices/${currentDeviceIndex}`, {
            method: 'DELETE'
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.message || 'Failed to delete device');
        }
        
        showToast('success', 'Device Deleted', 'Device deleted successfully');
        closeDeviceConfigModal();
        loadDevices();
    } catch (error) {
        console.error('Error deleting device:', error);
        showToast('error', 'Error', error.message);
    }
}

// ============================================================================
// Device Control Commands
// ============================================================================

async function sendSetpoint(controlIndex) {
    const input = document.getElementById(`setpoint-input-${controlIndex}`);
    if (!input) {
        showToast('error', 'Error', 'Setpoint input not found');
        return;
    }
    
    const setpoint = parseFloat(input.value);
    if (isNaN(setpoint)) {
        showToast('error', 'Validation Error', 'Please enter a valid setpoint value');
        return;
    }
    
    console.log(`[DEVICE] Sending setpoint to control index ${controlIndex}: ${setpoint}`);
    
    try {
        const response = await fetch(`/api/device/${controlIndex}/setpoint`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ setpoint: setpoint })
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to set setpoint');
        }
        
        const result = await response.json();
        showToast('success', 'Setpoint Set', `Setpoint ${setpoint} sent to device`);
        console.log('[DEVICE] Setpoint command sent successfully');
        
        // Clear input
        input.value = '';
        
        // Refresh data immediately
        fetchDeviceControlData();
    } catch (error) {
        console.error('Error setting setpoint:', error);
        showToast('error', 'Error', error.message);
    }
}

// ============================================================================
// Pressure Controller Calibration Helper
// ============================================================================

function calculatePressureCalibration() {
    // Get calibration point values
    const v1 = parseFloat(document.getElementById('configCalVoltage1').value);
    const p1 = parseFloat(document.getElementById('configCalPressure1').value);
    const v2 = parseFloat(document.getElementById('configCalVoltage2').value);
    const p2 = parseFloat(document.getElementById('configCalPressure2').value);
    
    // Get selected unit
    const unit = document.getElementById('configPressureUnit').value;
    
    // Validate inputs
    if (isNaN(v1) || isNaN(p1) || isNaN(v2) || isNaN(p2)) {
        showToast('error', 'Error', 'Please fill in all calibration points');
        return;
    }
    
    if (Math.abs(v2 - v1) < 0.1) {
        showToast('error', 'Error', 'Voltage points must be different');
        return;
    }
    
    // Convert pressure values to Pascals
    const unitFactors = {
        'Pa': 1.0,
        'kPa': 1000.0,
        'bar': 100000.0,
        'PSI': 6894.757,
        'atm': 101325.0,
        'mbar': 100.0
    };
    
    const factor = unitFactors[unit] || 100000.0;
    const p1_Pa = p1 * factor;
    const p2_Pa = p2 * factor;
    
    // Calculate calibration: pressure_Pa = scale * voltage_mV + offset
    const scale = (p2_Pa - p1_Pa) / (v2 - v1);
    const offset = p1_Pa - (scale * v1);
    
    // Update hidden fields and display values
    document.getElementById('configCalScale').value = scale;
    document.getElementById('configCalOffset').value = offset;
    document.getElementById('configCalScaleResult').textContent = scale.toFixed(4);
    document.getElementById('configCalOffsetResult').textContent = offset.toFixed(2);
    
    showToast('success', 'Success', 'Calibration calculated successfully');
    
    console.log(`[PRESSURE CAL] Unit=${unit}, P1=${p1} ${unit} @ ${v1}mV, P2=${p2} ${unit} @ ${v2}mV`);
    console.log(`[PRESSURE CAL] Result: scale=${scale.toFixed(4)} Pa/mV, offset=${offset.toFixed(2)} Pa`);
}

function resetPressureCalibration() {
    // Reset to default calibration (100.0 Pa/mV, 0.0 Pa offset)
    document.getElementById('configCalScale').value = 100.0;
    document.getElementById('configCalOffset').value = 0.0;
    document.getElementById('configCalScaleResult').textContent = '100.0000';
    document.getElementById('configCalOffsetResult').textContent = '0.00';
    
    // Clear calibration points
    document.getElementById('configCalVoltage1').value = '';
    document.getElementById('configCalPressure1').value = '';
    document.getElementById('configCalVoltage2').value = '';
    document.getElementById('configCalPressure2').value = '';
    
    showToast('info', 'Reset', 'Calibration reset to default values');
}

function updatePressureCalibrationUnits() {
    // Update pressure labels when unit changes
    const unit = document.getElementById('configPressureUnit').value;
    const label1 = document.getElementById('configPressureLabel1');
    const label2 = document.getElementById('configPressureLabel2');
    
    if (label1) label1.textContent = `Pressure (${unit}):`;
    if (label2) label2.textContent = `Pressure (${unit}):`;
}

// ============================================================================
// Add Device Pressure Controller Calibration Helpers
// ============================================================================

function calculateAddDevicePressureCalibration() {
    // Get calibration point values
    const v1 = parseFloat(document.getElementById('deviceCalVoltage1').value);
    const p1 = parseFloat(document.getElementById('deviceCalPressure1').value);
    const v2 = parseFloat(document.getElementById('deviceCalVoltage2').value);
    const p2 = parseFloat(document.getElementById('deviceCalPressure2').value);
    
    // Get selected unit
    const unit = document.getElementById('devicePressureUnit').value;
    
    // Validate inputs
    if (isNaN(v1) || isNaN(p1) || isNaN(v2) || isNaN(p2)) {
        showToast('error', 'Error', 'Please fill in all calibration points');
        return;
    }
    
    if (Math.abs(v2 - v1) < 0.1) {
        showToast('error', 'Error', 'Voltage points must be different');
        return;
    }
    
    // Convert pressure values to Pascals
    const unitFactors = {
        'Pa': 1.0,
        'kPa': 1000.0,
        'bar': 100000.0,
        'PSI': 6894.757,
        'atm': 101325.0,
        'mbar': 100.0
    };
    
    const factor = unitFactors[unit] || 100000.0;
    const p1_Pa = p1 * factor;
    const p2_Pa = p2 * factor;
    
    // Calculate calibration: pressure_Pa = scale * voltage_mV + offset
    const scale = (p2_Pa - p1_Pa) / (v2 - v1);
    const offset = p1_Pa - (scale * v1);
    
    // Update hidden fields and display values
    document.getElementById('deviceCalScale').value = scale;
    document.getElementById('deviceCalOffset').value = offset;
    document.getElementById('deviceCalScaleResult').textContent = scale.toFixed(4);
    document.getElementById('deviceCalOffsetResult').textContent = offset.toFixed(2);
    
    showToast('success', 'Success', 'Calibration calculated successfully');
    
    console.log(`[ADD DEVICE] Unit=${unit}, P1=${p1} ${unit} @ ${v1}mV, P2=${p2} ${unit} @ ${v2}mV`);
    console.log(`[ADD DEVICE] Result: scale=${scale.toFixed(4)} Pa/mV, offset=${offset.toFixed(2)} Pa`);
}

function resetAddDevicePressureCalibration() {
    // Reset to default calibration (100.0 Pa/mV, 0.0 Pa offset)
    document.getElementById('deviceCalScale').value = 100.0;
    document.getElementById('deviceCalOffset').value = 0.0;
    document.getElementById('deviceCalScaleResult').textContent = '100.0000';
    document.getElementById('deviceCalOffsetResult').textContent = '0.00';
    
    // Clear calibration points
    document.getElementById('deviceCalVoltage1').value = '';
    document.getElementById('deviceCalPressure1').value = '';
    document.getElementById('deviceCalVoltage2').value = '';
    document.getElementById('deviceCalPressure2').value = '';
    
    showToast('info', 'Reset', 'Calibration reset to default values');
}

function updateAddDevicePressureUnits() {
    // Update pressure labels when unit changes
    const unit = document.getElementById('devicePressureUnit').value;
    const label1 = document.getElementById('devicePressureLabel1');
    const label2 = document.getElementById('devicePressureLabel2');
    
    if (label1) label1.textContent = `Pressure (${unit}):`;
    if (label2) label2.textContent = `Pressure (${unit}):`;
}

// ============================================================================
// Initialization
// ============================================================================

// Load devices when page loads
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        loadDevices();
        startDevicesPolling();
    });
} else {
    loadDevices();
    startDevicesPolling();
}

// Hook into tab switching to start/stop polling
function initDevicesTab() {
    // Re-render device cards when tab is opened (needed after config restore)
    loadDevices();
    
    // Use PollingManager - polls every 2 seconds while devices tab is active
    PollingManager.startPolling('devices-api', fetchDeviceControlData, 2000);
    console.log('[DEVICES] Polling started via PollingManager');
}

// Export for use in main script.js
window.initDevicesTab = initDevicesTab;
window.initControllersTab = initControllersTab;

// ============================================================================
// CONTROLLERS TAB
// ============================================================================

const CONTROLLER_TYPES = {
    TEMPERATURE: { value: 0, name: 'Temperature Controller', indices: [40, 41, 42] },
    PH: { value: 1, name: 'pH Controller', indices: [43] },
    FLOW: { value: 2, name: 'Flow Controller', indices: [44, 45, 46, 47] },
    DO: { value: 3, name: 'Dissolved Oxygen Controller', indices: [48] }
};

// State management
let controllersData = [];
// Note: controllersPolling removed - now using PollingManager
let currentConfigIndex = null;
let selectedControllerType = null;
let focusedControllerSetpoints = new Set();  // Track focused setpoint inputs
let lastTuningState = new Map();  // Track last tuning state {index: boolean}
let autotuneJustCompleted = new Map();  // Track which controllers just completed autotune {index: boolean}

/**
 * Initialize Controllers tab
 */
async function initControllersTab() {
    console.log('[CONTROLLERS] Initializing Controllers tab');
    
    // Load DO profiles first so profile dropdowns can be populated
    try {
        await loadDOProfiles();
    } catch (e) {
        console.warn('[CONTROLLERS] Failed to load DO profiles:', e);
    }
    
    // Use PollingManager - polls every 2 seconds while controllers tab is active
    PollingManager.startPolling('controllers-api', loadControllers, 2000);
}

/**
 * Load all controllers from API
 */
async function loadControllers() {
    const ENDPOINT = '/api/controllers';
    
    try {
        const response = await fetch(ENDPOINT);
        
        if (!response.ok) throw new Error('Failed to load controllers');
        
        const data = await response.json();
        
        // Record success and cache data
        PollingManager.recordSuccess(ENDPOINT, data);
        
        controllersData = data.controllers || [];
        
        // Track autotune completion (transition from tuning=true to tuning=false)
        controllersData.forEach(ctrl => {
            const wasTuning = lastTuningState.get(ctrl.index);
            const isTuning = ctrl.tuning;
            
            // Detect autotune completion: was tuning, now not tuning
            if (wasTuning && !isTuning) {
                autotuneJustCompleted.set(ctrl.index, true);
                console.log(`[CONTROLLERS] Autotune completed for controller ${ctrl.index}`);
            }
            
            // Update last state
            lastTuningState.set(ctrl.index, isTuning);
        });
        
        renderControllers();
    } catch (error) {
        console.error('[CONTROLLERS] Error loading:', error);
        
        // Record error and check if we should show error UI
        const errorCount = PollingManager.recordError(ENDPOINT);
        
        if (PollingManager.shouldShowError(ENDPOINT)) {
            // Enough consecutive errors - show error message in container
            const container = document.getElementById('controllers-list');
            if (container) {
                container.innerHTML = '<div class="error-message">Failed to load controllers</div>';
            }
        } else {
            // Transient error - use cached data if available
            const cachedData = PollingManager.getCachedData(ENDPOINT);
            if (cachedData) {
                console.log(`[Controllers] Using cached data (error ${errorCount}/${PollingManager.ERROR_THRESHOLD})`);
                controllersData = cachedData.controllers || [];
                renderControllers();
            }
        }
    }
}

/**
 * Render controllers list
 */
function renderControllers() {
    const container = document.getElementById('controllers-list');
    console.log('[CONTROLLERS] Rendering, container found:', !!container, 'data length:', controllersData.length);
    
    if (!container) return;
    
    if (controllersData.length === 0) {
        container.innerHTML = '<div class="empty-message">No controllers configured. Click "Add Controller" above to create a new control loop.</div>';
        return;
    }
    
    // Check if initial render needed
    // Full render if: no children, has empty message, has loading indicator, or number of cards differs from data
    const needsFullRender = container.children.length === 0 || 
                           container.querySelector('.empty-message') !== null ||
                           container.querySelector('.loading') !== null ||
                           container.children.length !== controllersData.length;
    
    console.log('[CONTROLLERS] needsFullRender:', needsFullRender, 
                'children:', container.children.length,
                'data length:', controllersData.length,
                'has loading:', !!container.querySelector('.loading'));
    
    if (needsFullRender) {
        // Full render on first load
        container.innerHTML = '';
        controllersData.forEach(ctrl => {
            console.log('[CONTROLLERS] Creating card for controller', ctrl.index);
            const card = createControllerCard(ctrl);
            container.appendChild(card);
            
            // Populate DO profile dropdown if this is a DO controller
            if (ctrl.controlMethod === 4) {
                populateDOProfileSelect(ctrl.index, ctrl.activeProfileIndex);
            }
        });
        console.log('[CONTROLLERS] Full render complete, container children:', container.children.length);
    } else {
        // Selective update - only update existing cards
        controllersData.forEach(ctrl => {
            updateControllerCard(ctrl);
        });
    }
}

/**
 * Update existing controller card (selective DOM updates)
 */
function updateControllerCard(ctrl) {
    const card = document.querySelector(`.controller-card[data-index="${ctrl.index}"]`);
    if (!card) return;
    
    // Update status badge
    const statusBadge = card.querySelector('.device-status');
    if (statusBadge) {
        const statusClass = ctrl.fault ? 'status-fault' : ctrl.enabled ? 'status-online' : 'status-offline';
        const statusText = ctrl.fault ? 'FAULT' : ctrl.enabled ? 'ENABLED' : 'DISABLED';
        statusBadge.className = `device-status ${statusClass}`;
        statusBadge.textContent = statusText;
    }
    
    // Update dashboard icon
    const dashIcon = card.querySelector('.dashboard-icon');
    if (dashIcon) {
        dashIcon.className = `dashboard-icon ${ctrl.showOnDashboard ? 'active' : 'inactive'}`;
        dashIcon.title = ctrl.showOnDashboard ? 'Shown on Dashboard' : 'Hidden from Dashboard';
    }
    
    // Update controller values (indices differ based on whether PV exists)
    const processValueDisplays = card.querySelectorAll('.controller-value-display');
    
    if (ctrl.controlMethod === 3) {
        // Flow controllers: no PV, so indices are [0]=Setpoint, [1]=Output
        if (processValueDisplays[0]) {
            processValueDisplays[0].textContent = ctrl.processValue !== null ? ctrl.processValue.toFixed(1) + ctrl.unit : '--' + ctrl.unit;
        }
        if (processValueDisplays[1]) {
            const outputText = ctrl.output === 1 ? 'DOSING' : 'OFF';
            processValueDisplays[1].textContent = outputText;
        }
    } else {
        // Other controllers: PV exists, so indices are [0]=PV, [1]=Setpoint, [2]=Output
        if (processValueDisplays[0]) {
            processValueDisplays[0].textContent = ctrl.processValue !== null ? ctrl.processValue.toFixed(1) + ctrl.unit : '--' + ctrl.unit;
        }
        if (processValueDisplays[1]) {
            processValueDisplays[1].textContent = ctrl.setpoint.toFixed(1) + ctrl.unit;
        }
        if (processValueDisplays[2]) {
            const outputText = ctrl.output !== null 
                ? (ctrl.controlMethod === 0 
                    ? (ctrl.output > 0 ? 'ON' : 'OFF')
                    : ctrl.controlMethod === 1
                    ? ctrl.output.toFixed(0) + '%'
                    : ctrl.controlMethod === 2
                    ? (ctrl.output === 0 ? 'OFF' 
                       : ctrl.output === 1 ? 'DOSING ACID' 
                       : ctrl.output === 2 ? 'DOSING ALKALINE' : 'OFF')
                    : ctrl.controlMethod === 4
                    ? ctrl.output.toFixed(2) + ' mg/L'          // DO mode: show error value
                    : '--')
                : '--';
            processValueDisplays[2].textContent = outputText;
        }
    }
    
    // Update setpoint input (only if not focused)
    if (!focusedControllerSetpoints.has(ctrl.index)) {
        const setpointInput = card.querySelector(`#ctrl-setpoint-${ctrl.index}`);
        if (setpointInput) {
            setpointInput.value = ctrl.setpoint;
            // Setpoint should always be accessible, even when controller is disabled
        }
    }
    
    // Update buttons
    const buttons = card.querySelectorAll('.output-btn');
    buttons.forEach(btn => {
        const onclick = btn.getAttribute('onclick');
        if (onclick && onclick.includes('updateControllerSetpoint')) {
            // Setpoint update should always be enabled
        } else if (onclick && (onclick.includes('enableController') || onclick.includes('disableController'))) {
            btn.className = `output-btn ${ctrl.enabled ? 'output-btn-secondary' : 'output-btn-success'}`;
            btn.textContent = ctrl.enabled ? 'Disable' : 'Enable';
            btn.setAttribute('onclick', `${ctrl.enabled ? 'disableController' : 'enableController'}(${ctrl.index})`);
        } else if (onclick && onclick.includes('startAutotune')) {
            btn.disabled = ctrl.tuning;  // Only disable while tuning, allow when disabled
            btn.textContent = ctrl.tuning ? 'Tuning...' : 'Autotune';
        } else if (onclick && onclick.includes('savePIDValues')) {
            btn.disabled = !autotuneJustCompleted.get(ctrl.index);  // Only enable if autotune just completed
        }
    });
    
    // Update K values (PID gains) if in PID mode
    if (ctrl.controlMethod === 1) {
        const gainsSpan = card.querySelector(`#ctrl-gains-${ctrl.index}`);
        if (gainsSpan) {
            gainsSpan.innerHTML = `<strong>Gains:</strong> P=${ctrl.kP.toFixed(2)}, I=${ctrl.kI.toFixed(2)}, D=${ctrl.kD.toFixed(2)}`;
        }
    }
    
    // Update controller name
    const nameElement = card.querySelector('.output-name');
    if (nameElement) {
        nameElement.textContent = ctrl.name;
    }
    
    // Update config info row fields (deadband, hysteresis, dose volumes)
    if (ctrl.controlMethod === 0) {
        // On/Off mode - update hysteresis
        const hysteresisSpan = card.querySelector(`#ctrl-hysteresis-${ctrl.index}`);
        if (hysteresisSpan) {
            hysteresisSpan.textContent = ctrl.hysteresis.toFixed(2) + ctrl.unit;
        }
    } else if (ctrl.controlMethod === 2) {
        // pH mode - update deadband and dose volumes
        const deadbandSpan = card.querySelector(`#ctrl-deadband-${ctrl.index}`);
        if (deadbandSpan) {
            deadbandSpan.textContent = '±' + ctrl.deadband.toFixed(2) + ctrl.unit;
        }
        
        // Calculate dose volume based on output type
        let acidDoseVol = ctrl.acidVolumePerDose_mL || 0;
        if (ctrl.acidOutputType === 2) {
            // MFC: calculate from flow rate and dose time
            acidDoseVol = (ctrl.acidMfcFlowRate_mL_min * ctrl.acidDosingTime_ms) / 60000.0;
        }
        
        let alkalineDoseVol = ctrl.alkalineVolumePerDose_mL || 0;
        if (ctrl.alkalineOutputType === 2) {
            // MFC: calculate from flow rate and dose time
            alkalineDoseVol = (ctrl.alkalineMfcFlowRate_mL_min * ctrl.alkalineDosingTime_ms) / 60000.0;
        }
        
        const acidDoseSpan = card.querySelector(`#ctrl-acid-dose-vol-${ctrl.index}`);
        if (acidDoseSpan) {
            acidDoseSpan.textContent = acidDoseVol.toFixed(2) + ' mL';
        }
        
        const baseDoseSpan = card.querySelector(`#ctrl-base-dose-vol-${ctrl.index}`);
        if (baseDoseSpan) {
            baseDoseSpan.textContent = alkalineDoseVol.toFixed(2) + ' mL';
        }
    }
    
    // Update cumulative volumes if in pH mode
    if (ctrl.controlMethod === 2) {
        const volumeDisplay = card.querySelector(`#ph-volume-${ctrl.index}`);
        if (volumeDisplay) {
            const volumeValues = volumeDisplay.querySelectorAll('.ph-volume-value');
            if (volumeValues[0]) {
                volumeValues[0].textContent = (ctrl.acidVolumeTotal_mL || 0).toFixed(2) + ' mL';
            }
            if (volumeValues[1]) {
                volumeValues[1].textContent = (ctrl.alkalineVolumeTotal_mL || 0).toFixed(2) + ' mL';
            }
        }
    }
    
    // Update flow controller info if in flow mode
    if (ctrl.controlMethod === 3) {
        const doseIntervalSpan = card.querySelector(`#ctrl-dose-interval-${ctrl.index}`);
        if (doseIntervalSpan && ctrl.dosingInterval_ms !== undefined) {
            doseIntervalSpan.textContent = (ctrl.dosingInterval_ms / 1000).toFixed(1) + ' s';
        }
        
        const cumulativeVolSpan = card.querySelector(`#flow-cumulative-vol-${ctrl.index}`);
        if (cumulativeVolSpan && ctrl.cumulativeVolume_mL !== undefined) {
            cumulativeVolSpan.textContent = ctrl.cumulativeVolume_mL.toFixed(2) + ' mL';
        }
    }
    
    // Update DO controller info if in DO mode
    if (ctrl.controlMethod === 4) {
        // Populate profile select dropdown
        populateDOProfileSelect(ctrl.index, ctrl.activeProfileIndex);
        
        const profileNameSpan = card.querySelector(`#ctrl-profile-name-${ctrl.index}`);
        if (profileNameSpan) {
            profileNameSpan.textContent = ctrl.activeProfileName || 'None';
        }
        
        const stirrerOutputSpan = card.querySelector(`#ctrl-stirrer-output-${ctrl.index}`);
        if (stirrerOutputSpan) {
            const stirrerUnit = ctrl.stirrerUnit || '%';  // Default to % if not specified
            stirrerOutputSpan.textContent = ctrl.stirrerEnabled ? `${(ctrl.stirrerOutput || 0).toFixed(1)} ${stirrerUnit}` : 'Disabled';
        }
        
        const mfcOutputSpan = card.querySelector(`#ctrl-mfc-output-${ctrl.index}`);
        if (mfcOutputSpan) {
            mfcOutputSpan.textContent = ctrl.mfcEnabled ? (ctrl.mfcOutput || 0).toFixed(1) + ' mL/min' : 'Disabled';
        }
    }
    
    // Update message with appropriate styling based on controller state
    const infoMessage = card.querySelector(`#ctrl-message-${ctrl.index}`);
    if (infoMessage) {
        if (ctrl.message) {
            infoMessage.textContent = ctrl.message;
            infoMessage.style.display = 'block';
            
            // Update message class based on state
            if (ctrl.fault) {
                infoMessage.className = 'controller-message controller-message-fault';
            } else if (ctrl.enabled) {
                infoMessage.className = 'controller-message controller-message-enabled';
            } else {
                infoMessage.className = 'controller-message controller-message-disabled';
            }
        } else {
            infoMessage.style.display = 'none';
        }
    }
}

/**
 * Create controller card element
 */
function createControllerCard(ctrl) {
    const card = document.createElement('div');
    card.className = 'controller-card';
    card.setAttribute('data-index', ctrl.index);
    
    // Status badge
    const statusClass = ctrl.fault ? 'status-fault' : ctrl.enabled ? 'status-online' : 'status-offline';
    const statusText = ctrl.fault ? 'FAULT' : ctrl.enabled ? 'ENABLED' : 'DISABLED';
    
    card.innerHTML = `
        <div class="output-header">
            <div class="output-header-left">
                <span class="output-name">${ctrl.name}</span>
            </div>
            <div class="output-header-right">
                <div class="device-status ${statusClass}">${statusText}</div>
                <span class="dashboard-icon ${ctrl.showOnDashboard ? 'active' : 'inactive'}" title="${ctrl.showOnDashboard ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                    ${getDashboardIconSVG()}
                </span>
                <button class="icon-btn" onclick="openControllerConfig(${ctrl.index})" title="Configure">
                    ${getConfigIconSVG()}
                </button>
            </div>
        </div>
        
        <div class="controller-values">
            ${ctrl.controlMethod !== 3 ? `
            <div class="controller-value-item">
                <div class="controller-value-label">Process Value</div>
                <div class="controller-value-display">${ctrl.processValue !== null ? ctrl.processValue.toFixed(1) : '--'}${ctrl.unit}</div>
            </div>
            ` : ''}
            <div class="controller-value-item">
                <div class="controller-value-label">Setpoint</div>
                <div class="controller-value-display">${ctrl.setpoint.toFixed(1)}${ctrl.unit}</div>
            </div>
            <div class="controller-value-item">
                <div class="controller-value-label">${ctrl.controlMethod === 4 ? 'Delta' : 'Output'}</div>
                <div class="controller-value-display">${
                    ctrl.output !== null 
                        ? (ctrl.controlMethod === 0 
                            ? (ctrl.output > 0 ? 'ON' : 'OFF')          // On/Off mode: show state
                            : ctrl.controlMethod === 1
                            ? ctrl.output.toFixed(0) + '%'              // PID mode: show percentage
                            : ctrl.controlMethod === 2
                            ? (ctrl.output === 0 ? 'OFF' 
                               : ctrl.output === 1 ? 'DOSING ACID' 
                               : ctrl.output === 2 ? 'DOSING ALKALINE' : 'OFF')  // pH mode: show dosing state
                            : ctrl.controlMethod === 3
                            ? (ctrl.output === 1 ? 'DOSING' : 'OFF')    // Flow mode: show dosing state
                            : ctrl.controlMethod === 4
                            ? ctrl.output.toFixed(2) + ' mg/L'          // DO mode: show error value
                            : '--')
                        : '--'
                }</div>
            </div>
        </div>
        
        <div class="controller-info-row">
            ${ctrl.controlMethod !== 2 && ctrl.controlMethod !== 3 && ctrl.controlMethod !== 4 ? `<span><strong>Mode:</strong> ${ctrl.controlMethod === 0 ? 'On/Off' : 'PID'}</span>` : ''}
            ${ctrl.controlMethod === 1 ? `
                <span id="ctrl-gains-${ctrl.index}"><strong>Gains:</strong> P=${ctrl.kP.toFixed(2)}, I=${ctrl.kI.toFixed(2)}, D=${ctrl.kD.toFixed(2)}</span>
            ` : ctrl.controlMethod === 2 ? `
                <span><strong>Deadband:</strong> <span id="ctrl-deadband-${ctrl.index}">±${ctrl.deadband.toFixed(2)}${ctrl.unit}</span></span>
                <span id="ctrl-acid-info-${ctrl.index}"><strong>Acid Dose Vol:</strong> <span id="ctrl-acid-dose-vol-${ctrl.index}">${(ctrl.acidOutputType === 2 ? (ctrl.acidMfcFlowRate_mL_min * ctrl.acidDosingTime_ms / 60000.0) : (ctrl.acidVolumePerDose_mL || 0)).toFixed(2)} mL</span></span>
                <span id="ctrl-base-info-${ctrl.index}"><strong>Base Dose Vol:</strong> <span id="ctrl-base-dose-vol-${ctrl.index}">${(ctrl.alkalineOutputType === 2 ? (ctrl.alkalineMfcFlowRate_mL_min * ctrl.alkalineDosingTime_ms / 60000.0) : (ctrl.alkalineVolumePerDose_mL || 0)).toFixed(2)} mL</span></span>
            ` : ctrl.controlMethod === 3 ? `
                <span><strong>Dosing Interval:</strong> <span id="ctrl-dose-interval-${ctrl.index}">${ctrl.dosingInterval_ms ? (ctrl.dosingInterval_ms / 1000).toFixed(1) + ' s' : '--'}</span></span>
                <span><strong>Calib Vol:</strong> ${ctrl.calibrationVolume_mL ? ctrl.calibrationVolume_mL.toFixed(2) + ' mL' : '--'}</span>
                <span><strong>Calib Time:</strong> ${ctrl.calibrationDoseTime_ms ? (ctrl.calibrationDoseTime_ms / 1000).toFixed(1) + ' s' : '--'}</span>
            ` : ctrl.controlMethod === 4 ? `
                <span><strong>Profile:</strong> <span id="ctrl-profile-name-${ctrl.index}">${ctrl.activeProfileName || 'None'}</span></span>
                <span><strong>Stirrer:</strong> <span id="ctrl-stirrer-output-${ctrl.index}">${ctrl.stirrerEnabled ? `${(ctrl.stirrerOutput || 0).toFixed(1)} ${ctrl.stirrerUnit || '%'}` : 'Disabled'}</span></span>
                <span><strong>MFC:</strong> <span id="ctrl-mfc-output-${ctrl.index}">${ctrl.mfcEnabled ? (ctrl.mfcOutput || 0).toFixed(1) + ' mL/min' : 'Disabled'}</span></span>
            ` : `
                <span><strong>Hysteresis:</strong> <span id="ctrl-hysteresis-${ctrl.index}">${ctrl.hysteresis.toFixed(2)}${ctrl.unit}</span></span>
            `}
        </div>
        
        ${ctrl.controlMethod === 2 ? `
        <div class="ph-volume-display" id="ph-volume-${ctrl.index}">
            <div class="ph-volume-row">
                <span class="ph-volume-label">Acid Dosed:</span>
                <span class="ph-volume-value">${(ctrl.acidVolumeTotal_mL || 0).toFixed(2)} mL</span>
                <button class="output-btn output-btn-sm output-btn-secondary" 
                        onclick="resetpHVolume(${ctrl.index}, 'acid')">
                    Reset
                </button>
            </div>
            <div class="ph-volume-row">
                <span class="ph-volume-label">Base Dosed:</span>
                <span class="ph-volume-value">${(ctrl.alkalineVolumeTotal_mL || 0).toFixed(2)} mL</span>
                <button class="output-btn output-btn-sm output-btn-secondary" 
                        onclick="resetpHVolume(${ctrl.index}, 'alkaline')">
                    Reset
                </button>
            </div>
        </div>
        ` : ctrl.controlMethod === 3 ? `
        <div class="ph-volume-display" id="flow-volume-${ctrl.index}">
            <div class="ph-volume-row">
                <span class="ph-volume-label">Total Volume:</span>
                <span class="ph-volume-value" id="flow-cumulative-vol-${ctrl.index}">${(ctrl.cumulativeVolume_mL || 0).toFixed(2)} mL</span>
                <button class="output-btn output-btn-sm output-btn-secondary" 
                        onclick="resetFlowVolume(${ctrl.index})">
                    Reset
                </button>
            </div>
        </div>
        ` : ''}
        
        <div class="controller-setpoint-control">
            <label>Setpoint:</label>
            <input type="number" 
                   id="ctrl-setpoint-${ctrl.index}" 
                   value="${ctrl.setpoint}" 
                   step="0.1" 
                   onfocus="focusedControllerSetpoints.add(${ctrl.index})"
                   onblur="focusedControllerSetpoints.delete(${ctrl.index})">
            <span class="unit-label">${ctrl.unit}</span>
            <button class="output-btn output-btn-primary" 
                    onclick="updateControllerSetpoint(${ctrl.index})">
                Update
            </button>
        </div>
        
        <div class="controller-button-group">
            <button class="output-btn ${ctrl.enabled ? 'output-btn-secondary' : 'output-btn-success'}" 
                    onclick="${ctrl.enabled ? 'disableController' : 'enableController'}(${ctrl.index})">
                ${ctrl.enabled ? 'Disable' : 'Enable'}
            </button>
            ${ctrl.controlMethod === 1 ? `
                <button class="output-btn output-btn-warning" 
                        onclick="startAutotune(${ctrl.index})"
                        ${ctrl.tuning ? 'disabled' : ''}>
                    ${ctrl.tuning ? 'Tuning...' : 'Autotune'}
                </button>
                <button id="save-pid-btn-${ctrl.index}" 
                        class="output-btn output-btn-success" 
                        onclick="savePIDValues(${ctrl.index})"
                        ${autotuneJustCompleted.get(ctrl.index) ? '' : 'disabled'}>
                    Save PID Values
                </button>
            ` : ctrl.controlMethod === 2 ? `
                ${ctrl.acidEnabled ? `
                    <button class="output-btn output-btn-warning" 
                            onclick="dosepHAcid(${ctrl.index})">
                        Dose Acid
                    </button>
                ` : ''}
                ${ctrl.alkalineEnabled ? `
                    <button class="output-btn output-btn-info" 
                            onclick="dosepHAlkaline(${ctrl.index})">
                        Dose Alkaline
                    </button>
                ` : ''}
            ` : ctrl.controlMethod === 3 ? `
                <button class="output-btn output-btn-warning" 
                        onclick="manualFlowDose(${ctrl.index})">
                    Manual Dose
                </button>
            ` : ctrl.controlMethod === 4 ? `
                <div style="display: flex; align-items: center; gap: 10px; flex-wrap: wrap;">
                    <div style="display: flex; align-items: center; gap: 5px;">
                        <label style="margin: 0; font-weight: 500;">Profile:</label>
                        <select id="do-profile-select-${ctrl.index}" 
                                style="padding: 8px; border-radius: 4px; border: 1px solid #ddd;"
                                onchange="switchDOProfile(${ctrl.index}, this.value)">
                        </select>
                    </div>
                    <button class="output-btn output-btn-success mdi mdi-plus-box"
                            onclick="openDOProfileModal()"> Add</button>
                    <button class="output-btn output-btn-info mdi mdi-square-edit-outline"
                            onclick="openDOProfileModal(document.getElementById('do-profile-select-${ctrl.index}').value)"> Edit</button>
                </div>
            ` : ''}
        </div>
        
        <div class="controller-message ${ctrl.fault ? 'controller-message-fault' : ctrl.enabled ? 'controller-message-enabled' : 'controller-message-disabled'}" 
             id="ctrl-message-${ctrl.index}" 
             style="display: ${ctrl.message ? 'block' : 'none'};">
            ${ctrl.message || ''}
        </div>
    `;
    
    return card;
}

// ============================================================================
// MODAL HANDLERS
// ============================================================================

function openAddControllerModal() {
    const modal = document.getElementById('addControllerModal');
    if (!modal) return;
    
    // Reset controller type dropdown
    document.getElementById('controllerType').value = '';
    selectedControllerType = null;
    
    // Clear any previously expanded fields to start fresh
    const oldSection = document.getElementById('controllerExpandedFields');
    if (oldSection) oldSection.remove();
    
    modal.style.display = 'flex';
    modal.classList.add('active');
}

function closeAddControllerModal() {
    const modal = document.getElementById('addControllerModal');
    if (modal) {
        modal.style.display = 'none';
        modal.classList.remove('active');
        
        // Clear the expanded fields to prevent ID conflicts
        const expandedSection = document.getElementById('controllerExpandedFields');
        if (expandedSection) {
            expandedSection.remove();
        }
    }
}

async function updateSensorOptions() {
    const typeSelect = document.getElementById('controllerType');
    const typeValue = parseInt(typeSelect.value);
    
    if (isNaN(typeValue)) {
        selectedControllerType = null;
        // Remove expanded fields if they exist
        const expandedSection = document.getElementById('controllerExpandedFields');
        if (expandedSection) expandedSection.remove();
        return;
    }
    
    selectedControllerType = Object.values(CONTROLLER_TYPES).find(t => t.value === typeValue);
    console.log('[CONTROLLERS] Selected type:', selectedControllerType?.name);
    
    // Load sensors and outputs
    const sensors = await loadTempSensors();
    const outputs = await loadAvailableOutputs();
    
    // Check for available slots
    const availableIndices = selectedControllerType.indices.filter(idx => {
        return !controllersData.find(c => c.index === idx);
    });
    
    if (availableIndices.length === 0) {
        showToast('warning', 'No Slots Available', `All ${selectedControllerType.name} slots are in use`);
        typeSelect.value = '';
        selectedControllerType = null;
        return;
    }
    
    // Remove old expanded section if exists
    const oldSection = document.getElementById('controllerExpandedFields');
    if (oldSection) oldSection.remove();
    
    // Create expanded configuration section
    const configForm = document.querySelector('#addControllerModal .config-form');
    const expandedDiv = document.createElement('div');
    expandedDiv.id = 'controllerExpandedFields';
    
    // Render appropriate form based on controller type
    if (selectedControllerType.value === 1) {
        // pH Controller
        await renderpHAddForm(expandedDiv, sensors, outputs, availableIndices[0]);
    } else if (selectedControllerType.value === 2) {
        // Flow Controller
        await renderFlowAddForm(expandedDiv, sensors, outputs, availableIndices[0]);
    } else if (selectedControllerType.value === 3) {
        // Dissolved Oxygen Controller
        await renderDOAddForm(expandedDiv, sensors, outputs, availableIndices[0]);
    } else {
        // Temperature Controller
        expandedDiv.innerHTML = `
            <hr style="margin: 20px 0; border: none; border-top: 1px solid #dee2e6;">
            
            <div class="form-group">
                <label for="ctrlName">Name:</label>
                <input type="text" id="ctrlName" value="Temperature Controller ${availableIndices[0]-39}" maxlength="39">
            </div>
            
            <div class="form-group">
                <label>
                    <input type="checkbox" id="ctrlShowDashboard">
                    Show on Dashboard
                </label>
            </div>
            
            <div class="form-group">
                <label for="ctrlTempSensor">Temperature Sensor:</label>
                <select id="ctrlTempSensor">
                    <option value="">-- Select Sensor --</option>
                    ${sensors.map(s => `<option value="${s.index}">${s.name}</option>`).join('')}
                </select>
            </div>
            
            <div class="form-group">
                <label for="ctrlOutput">Heater Output:</label>
                <select id="ctrlOutput">
                    <option value="">-- Select Output --</option>
                    ${outputs.map(o => `<option value="${o.index}">${o.name}</option>`).join('')}
                </select>
            </div>
            
            <div class="form-group">
                <label for="ctrlMode">Control Mode:</label>
                <select id="ctrlMode" onchange="toggleControlModeFields()">
                    <option value="0">On/Off</option>
                    <option value="1">PID</option>
                </select>
            </div>
            
            <div id="onOffParams">
                <div class="form-group">
                    <label for="ctrlHysteresis">Hysteresis (°C):</label>
                    <input type="number" id="ctrlHysteresis" value="0.5" step="0.1" min="0.1">
                </div>
            </div>
            
            <div id="pidParams" style="display: none;">
                <div class="form-group">
                    <label for="ctrlKp">Proportional Gain (kP):</label>
                    <input type="number" id="ctrlKp" value="2.0" step="0.1">
                </div>
                <div class="form-group">
                    <label for="ctrlKi">Integral Gain (kI):</label>
                    <input type="number" id="ctrlKi" value="0.5" step="0.01">
                </div>
                <div class="form-group">
                    <label for="ctrlKd">Derivative Gain (kD):</label>
                <input type="number" id="ctrlKd" value="0.1" step="0.01">
            </div>
            <div class="form-group">
                <label for="ctrlWindup">Integral Windup Limit:</label>
                <input type="number" id="ctrlWindup" value="100.0" step="1">
            </div>
            <div class="form-group">
                <label for="ctrlOutMin">Output Min (%):</label>
                <input type="number" id="ctrlOutMin" value="0" step="1" min="0" max="100">
            </div>
            <div class="form-group">
                <label for="ctrlOutMax">Output Max (%):</label>
                <input type="number" id="ctrlOutMax" value="100" step="1" min="0" max="100">
            </div>
        </div>
        
        <div class="form-group">
            <label for="ctrlSetpoint">Initial Setpoint (°C):</label>
            <input type="number" id="ctrlSetpoint" value="25.0" step="0.1">
        </div>
    `;
    }
    configForm.appendChild(expandedDiv);
}

async function renderpHAddForm(expandedDiv, sensors, outputs, indexNum) {
    // Get pH sensors from device sensors
    let phSensors = [];
    try {
        const inputsResponse = await fetch('/api/inputs');
        if (inputsResponse.ok) {
            const inputsData = await inputsResponse.json();
            if (inputsData.devices) {
                phSensors = inputsData.devices
                    .filter(ds => ds.t && ds.t == 3)
                    .map(ds => ({ index: ds.i, name: `[${ds.i}] ${ds.n || 'pH Sensor'} (${ds.v.toFixed(1)}${ds.u})` }));
            }
        }
    } catch (error) {
        console.error('[pH ADD] Error loading pH sensors:', error);
    }
    
    // Get DC motors for dosing options
    let dcMotors = [];
    try {
        const outputsResponse = await fetch('/api/outputs');
        if (outputsResponse.ok) {
            const outputsData = await outputsResponse.json();
            if (outputsData.dcMotors) {
                dcMotors = outputsData.dcMotors.map(m => ({ index: m.index, name: m.name }));
            }
        }
    } catch (error) {
        console.error('[pH ADD] Error loading DC motors:', error);
    }
    
    expandedDiv.innerHTML = `
        <hr style="margin: 20px 0; border: none; border-top: 1px solid #dee2e6;">
        
        <div class="form-group">
            <label for="addCtrlName">Name:</label>
            <input type="text" id="addCtrlName" value="pH Controller" maxlength="39">
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="addCtrlShowDashboard">
                Show on Dashboard
            </label>
        </div>
        
        <div class="form-group">
            <label for="addCtrlpHSensor">pH Sensor:</label>
            <select id="addCtrlpHSensor">
                <option value="">-- Select pH Sensor --</option>
                ${phSensors.map(s => `<option value="${s.index}">${s.name}</option>`).join('')}
            </select>
        </div>
        
        <div class="form-group">
            <label for="addCtrlSetpoint">Setpoint (pH):</label>
            <input type="number" id="addCtrlSetpoint" value="7.0" step="0.1" min="0" max="14">
        </div>
        
        <div class="form-group">
            <label for="addCtrlDeadband">Deadband (pH):</label>
            <input type="number" id="addCtrlDeadband" value="0.2" step="0.05" min="0.05">
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        
        <h4 style="margin-bottom: 15px;">Acid Dosing</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="ctrlAcidEnabled" checked onchange="toggleAddDosingFields()">
                Enable Acid Dosing
            </label>
        </div>
        
        <div id="acidDosingAddFields">
            <div class="form-group">
                <label for="ctrlAcidOutputType">Output Type:</label>
                <select id="ctrlAcidOutputType" onchange="updateAddpHOutputOptions('acid')">
                    <option value="0">Digital Output</option>
                    <option value="1">DC Motor</option>
                    <option value="2">Mass Flow Controller</option>
                </select>
            </div>
            
            <div class="form-group">
                <label for="ctrlAcidOutput">Output:</label>
                <select id="ctrlAcidOutput">
                    <option value="">-- Select Output --</option>
                </select>
            </div>
            
            <div class="form-group" id="acidAddMotorPowerGroup" style="display: none;">
                <label for="ctrlAcidMotorPower">Motor Power (%):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="ctrlAcidMotorPower" value="50" min="1" max="100" style="flex: 1;">
                    <span>%</span>
                </div>
            </div>
            
            <div class="form-group" id="acidAddMfcFlowRateGroup" style="display: none;">
                <label for="ctrlAcidMfcFlowRate">Flow Rate (mL/min):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="ctrlAcidMfcFlowRate" value="100" min="0" max="10000" step="0.1" style="flex: 1;">
                    <span>mL/min</span>
                </div>
            </div>
            
            <div class="form-group">
                <label for="ctrlAcidDoseTime">Dose Time (ms):</label>
                <input type="number" id="ctrlAcidDoseTime" value="1000" min="100" max="60000" step="100">
            </div>
            
            <div class="form-group">
                <label for="ctrlAcidDoseInterval">Dose Interval (ms):</label>
                <input type="number" id="ctrlAcidDoseInterval" value="60000" min="1000" max="3600000" step="1000">
            </div>
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        
        <h4 style="margin-bottom: 15px;">Alkaline Dosing</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="ctrlAlkalineEnabled" onchange="toggleAddDosingFields()">
                Enable Alkaline Dosing
            </label>
        </div>
        
        <div id="alkalineDosingAddFields" style="display: none;">
            <div class="form-group">
                <label for="ctrlAlkalineOutputType">Output Type:</label>
                <select id="ctrlAlkalineOutputType" onchange="updateAddpHOutputOptions('alkaline')">
                    <option value="0">Digital Output</option>
                    <option value="1">DC Motor</option>
                    <option value="2">Mass Flow Controller</option>
                </select>
            </div>
            
            <div class="form-group">
                <label for="ctrlAlkalineOutput">Output:</label>
                <select id="ctrlAlkalineOutput">
                    <option value="">-- Select Output --</option>
                </select>
            </div>
            
            <div class="form-group" id="alkalineAddMotorPowerGroup" style="display: none;">
                <label for="ctrlAlkalineMotorPower">Motor Power (%):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="ctrlAlkalineMotorPower" value="50" min="1" max="100" style="flex: 1;">
                    <span>%</span>
                </div>
            </div>
            
            <div class="form-group" id="alkalineAddMfcFlowRateGroup" style="display: none;">
                <label for="ctrlAlkalineMfcFlowRate">Flow Rate (mL/min):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="ctrlAlkalineMfcFlowRate" value="100" min="0" max="10000" step="0.1" style="flex: 1;">
                    <span>mL/min</span>
                </div>
            </div>
            
            <div class="form-group">
                <label for="ctrlAlkalineDoseTime">Dose Time (ms):</label>
                <input type="number" id="ctrlAlkalineDoseTime" value="1000" min="100" max="60000" step="100">
            </div>
            
            <div class="form-group">
                <label for="ctrlAlkalineDoseInterval">Dose Interval (ms):</label>
                <input type="number" id="ctrlAlkalineDoseInterval" value="60000" min="1000" max="3600000" step="1000">
            </div>
        </div>
    `;
    
    // Store outputs and motors for later use (do this BEFORE calling update functions)
    window.phAddOutputs = outputs;
    window.phAddMotors = dcMotors;
    
    // Wait a tick to ensure DOM is ready, then initialize output dropdowns
    setTimeout(() => {
        updateAddpHOutputOptions('acid');
        updateAddpHOutputOptions('alkaline');
    }, 0);
}

async function renderFlowAddForm(expandedDiv, sensors, outputs, indexNum) {
    // Get DC motors for flow control
    let dcMotors = [];
    try {
        const outputsResponse = await fetch('/api/outputs');
        if (outputsResponse.ok) {
            const outputsData = await outputsResponse.json();
            if (outputsData.dcMotors) {
                dcMotors = outputsData.dcMotors.map(m => ({ index: m.index, name: m.name }));
            }
        }
    } catch (error) {
        console.error('[FLOW ADD] Error loading DC motors:', error);
    }
    
    expandedDiv.innerHTML = `
        <hr style="margin: 20px 0; border: none; border-top: 1px solid #dee2e6;">
        
        <div class="form-group">
            <label for="addFlowCtrlName">Name:</label>
            <input type="text" id="addFlowCtrlName" value="Flow Controller ${indexNum-43}" maxlength="39">
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="addFlowCtrlShowDashboard">
                Show on Dashboard
            </label>
        </div>
        
        <div class="form-group">
            <label for="addFlowCtrlFlowRate">Target Flow Rate (mL/min):</label>
            <input type="number" id="addFlowCtrlFlowRate" value="10.0" step="0.1" min="0.1">
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Output Configuration</h4>
        
        <div class="form-group">
            <label for="addFlowCtrlOutputType">Output Type:</label>
            <select id="addFlowCtrlOutputType" onchange="updateFlowAddOutputOptions()">
                <option value="0">Digital Output</option>
                <option value="1">DC Motor</option>
            </select>
        </div>
        
        <div class="form-group">
            <label for="addFlowCtrlOutput">Output:</label>
            <select id="addFlowCtrlOutput">
                <option value="">-- Select Output --</option>
            </select>
        </div>
        
        <div class="form-group" id="flowAddMotorPowerGroup" style="display: none;">
            <label for="addFlowCtrlMotorPower">Motor Power (%):</label>
            <div style="display: flex; align-items: center; gap: 8px;">
                <input type="number" id="addFlowCtrlMotorPower" value="50" min="1" max="100" style="flex: 1;">
                <span>%</span>
            </div>
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Calibration Data</h4>
        
        <div class="form-group">
            <label for="addFlowCtrlCalibVolume">Calibration Volume (mL):</label>
            <input type="number" id="addFlowCtrlCalibVolume" value="10.0" step="0.1" min="0.1">
        </div>
        
        <div class="form-group">
            <label for="addFlowCtrlCalibTime">Calibration Dose Time (ms):</label>
            <input type="number" id="addFlowCtrlCalibTime" value="5000" min="100" max="60000" step="100">
        </div>
        
        <div class="form-group" id="flowAddCalibPowerGroup" style="display: none;">
            <label for="addFlowCtrlCalibPower">Calibration Motor Power (%):</label>
            <input type="number" id="addFlowCtrlCalibPower" value="50" min="1" max="100">
            <small style="color: #7f8c8d;">Motor power used during calibration (applies to both calibration and dosing)</small>
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Safety Limits</h4>
        
        <div class="form-group">
            <label for="addFlowCtrlMinInterval">Min Dosing Interval (ms):</label>
            <input type="number" id="addFlowCtrlMinInterval" value="1000" min="100" max="60000" step="100">
        </div>
        
        <div class="form-group">
            <label for="addFlowCtrlMaxTime">Max Dosing Time (ms):</label>
            <input type="number" id="addFlowCtrlMaxTime" value="10000" min="100" max="60000" step="100">    
        </div>
    `;
    
    // Store references for output options update
    window.flowAddOutputs = outputs;
    window.flowAddMotors = dcMotors;
    
    // Initialize output dropdown
    setTimeout(() => {
        updateFlowAddOutputOptions();
    }, 0);
}

async function renderDOAddForm(expandedDiv, sensors, outputs, indexNum) {
    // Get DO sensors from device sensors (indices 70-99, type 4)
    let doSensors = [];
    try {
        const inputsResponse = await fetch('/api/inputs');
        if (inputsResponse.ok) {
            const inputsData = await inputsResponse.json();
            if (inputsData.devices) {
                doSensors = inputsData.devices
                    .filter(ds => ds.t && ds.t == 4 && ds.i >= 70 && ds.i <= 99)
                    .map(ds => ({ index: ds.i, name: `[${ds.i}] ${ds.n || 'DO Sensor'} (${ds.v?.toFixed(2) || '--'}${ds.u || ''})` }));
            }
        }
    } catch (error) {
        console.error('[DO ADD] Error loading DO sensors:', error);
    }
    
    // Get MFC devices from device sensors (indices 70-99, type = flow sensor or custom MFC type)
    let mfcDevices = [];
    try {
        const inputsResponse = await fetch('/api/inputs');
        if (inputsResponse.ok) {
            const inputsData = await inputsResponse.json();
            if (inputsData.devices) {
                // MFC devices are in the 70-99 range, typically type 6 (flow sensor) or similar
                // Filter for devices that look like MFCs (contain "MFC" in name or are flow sensors)
                // Use control index (c) for device commands, not sensor index (i)
                mfcDevices = inputsData.devices
                    .filter(ds => {
                        const nameLower = (ds.n || '').toLowerCase();
                        return (nameLower.includes('mfc') || nameLower.includes('mass flow') || ds.t === 6) && ds.c;
                    })
                    .map(ds => ({ index: ds.c, name: ds.n || `MFC Device ${ds.c}` }));
            }
        }
    } catch (error) {
        console.error('[DO ADD] Error loading MFC devices:', error);
    }
    
    // Get DC motors and stepper for stirring
    let dcMotors = [];
    let stepperMotor = null;
    try {
        const outputsResponse = await fetch('/api/outputs');
        if (outputsResponse.ok) {
            const outputsData = await outputsResponse.json();
            if (outputsData.dcMotors) {
                dcMotors = outputsData.dcMotors.map(m => ({ index: m.index, name: m.name }));
            }
            // API returns "stepperMotor" not "stepper"
            if (outputsData.stepperMotor) {
                stepperMotor = { index: 26, name: outputsData.stepperMotor.name || 'Stepper Motor' };
            }
        }
    } catch (error) {
        console.error('[DO ADD] Error loading motors:', error);
    }
    
    // Load DO profiles
    let profiles = [];
    try {
        console.log('[DO ADD] Loading profiles...');
        await loadDOProfiles();
        profiles = doProfiles.filter(p => p.isActive && p.numPoints >= 2);
        console.log('[DO ADD] Loaded', profiles.length, 'profiles');
    } catch (error) {
        console.error('[DO ADD] Error loading profiles:', error);
    }
    
    expandedDiv.innerHTML = `
        <hr style="margin: 20px 0; border: none; border-top: 1px solid #dee2e6;">
        
        <div class="form-group">
            <label for="addDOCtrlName">Name:</label>
            <input type="text" id="addDOCtrlName" value="DO Controller" maxlength="39">
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="addDOCtrlShowDashboard">
                Show on Dashboard
            </label>
        </div>
        
        <div class="form-group">
            <label for="addDOCtrlSensor">DO Sensor:</label>
            <select id="addDOCtrlSensor">
                <option value="">-- Select DO Sensor --</option>
                ${doSensors.map(s => `<option value="${s.index}">${s.name}</option>`).join('')}
            </select>
            ${doSensors.length === 0 ? '<small style="color: #e74c3c;">No DO sensors found. Configure a DO sensor device first.</small>' : ''}
        </div>
        
        <div class="form-group">
            <label for="addDOCtrlSetpoint">Setpoint (mg/L or %):</label>
            <input type="number" id="addDOCtrlSetpoint" value="8.0" step="0.1" min="0">
        </div>
        
        <div class="form-group">
            <label for="addDOCtrlProfile">Control Profile:</label>
            <select id="addDOCtrlProfile" style="min-width: 250px;">
                <option value="">-- Select Profile --</option>
                ${profiles.map((p, idx) => `<option value="${idx}">${p.name} (${p.numPoints} points)</option>`).join('')}
            </select>
            <div style="margin-top: 5px;">
                <button type="button" class="btn-primary" style="padding: 8px 16px; font-size: 0.9em; margin: 0 10px;" onclick="openDOProfileModal();">
                    New
                </button>
            </div>
            ${profiles.length === 0 ? '<small style="color: #295855ff;">No profiles configured, create a profile first.</small>' : ''}
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Stirring Control</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="addDOCtrlStirrerEnabled" onchange="toggleDOStirrerFields()">
                Enable Stirring
            </label>
        </div>
        
        <div id="doStirrerAddFields" style="display: none;">
            <div class="form-group">
                <label for="addDOCtrlStirrerType">Stirrer Type:</label>
                <select id="addDOCtrlStirrerType" onchange="updateDOStirrerOptions()">
                    <option value="0">DC Motor</option>
                    <option value="1">Stepper Motor</option>
                </select>
            </div>
            
            <div class="form-group">
                <label for="addDOCtrlStirrerIndex">Stirrer Motor:</label>
                <select id="addDOCtrlStirrerIndex">
                    <option value="">-- Select Motor --</option>
                </select>
            </div>
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Gas Flow Control (MFC)</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="addDOCtrlMFCEnabled" onchange="toggleDOMFCFields()">
                Enable MFC Gas Flow
            </label>
        </div>
        
        <div id="doMFCAddFields" style="display: none;">
            <div class="form-group">
                <label for="addDOCtrlMFCIndex">Mass Flow Controller:</label>
                <select id="addDOCtrlMFCIndex">
                    <option value="">-- Select MFC --</option>
                    ${mfcDevices.map(m => `<option value="${m.index}">${m.name}</option>`).join('')}
                </select>
                ${mfcDevices.length === 0 ? '<small style="color: #e74c3c;">No MFC devices found. Configure an MFC device (Alicat, etc.) first.</small>' : ''}
            </div>
        </div>
        
        <p style="color: #7f8c8d; font-size: 0.9em; margin-top: 15px;">
            <strong>Note:</strong> At least one output (stirrer or MFC) must be enabled. Profile defines how outputs respond to DO error.
        </p>
    `;
    
    // Store references
    window.doAddMotors = dcMotors;
    window.doAddStepper = stepperMotor;
    
    // Initialize dropdowns
    setTimeout(() => {
        updateDOStirrerOptions();
    }, 0);
}

function updateFlowAddOutputOptions() {
    const outputTypeSelect = document.getElementById('addFlowCtrlOutputType');
    const outputSelect = document.getElementById('addFlowCtrlOutput');
    const motorPowerGroup = document.getElementById('flowAddMotorPowerGroup');
    const calibPowerGroup = document.getElementById('flowAddCalibPowerGroup');
    
    if (!outputTypeSelect || !outputSelect) return;
    
    const outputType = parseInt(outputTypeSelect.value);
    const outputs = window.flowAddOutputs || [];
    const motors = window.flowAddMotors || [];
    
    // Clear current options
    outputSelect.innerHTML = '<option value="">-- Select Output --</option>';
    
    if (outputType === 0) {
        // Digital outputs (21-25)
        outputs.forEach(o => {
            if (o.index >= 21 && o.index <= 25) {
                outputSelect.innerHTML += `<option value="${o.index}">${o.name}</option>`;
            }
        });
        if (calibPowerGroup) calibPowerGroup.style.display = 'none';
    } else {
        // DC motors (27-30)
        motors.forEach(m => {
            outputSelect.innerHTML += `<option value="${m.index}">${m.name}</option>`;
        });
        if (calibPowerGroup) calibPowerGroup.style.display = 'block';
    }
}

function toggleDOStirrerFields() {
    const enabled = document.getElementById('addDOCtrlStirrerEnabled')?.checked || false;
    const fields = document.getElementById('doStirrerAddFields');
    if (fields) fields.style.display = enabled ? 'block' : 'none';
}

function toggleDOMFCFields() {
    const enabled = document.getElementById('addDOCtrlMFCEnabled')?.checked || false;
    const fields = document.getElementById('doMFCAddFields');
    if (fields) fields.style.display = enabled ? 'block' : 'none';
}

function updateDOStirrerOptions() {
    const typeSelect = document.getElementById('addDOCtrlStirrerType');
    const motorSelect = document.getElementById('addDOCtrlStirrerIndex');
    
    if (!typeSelect || !motorSelect) return;
    
    const stirrerType = parseInt(typeSelect.value);
    const motors = window.doAddMotors || [];
    const stepper = window.doAddStepper;
    
    // Clear current options
    motorSelect.innerHTML = '<option value="">-- Select Motor --</option>';
    
    if (stirrerType === 0) {
        // DC Motors (27-30)
        motors.forEach(m => {
            motorSelect.innerHTML += `<option value="${m.index}">${m.name}</option>`;
        });
    } else {
        // Stepper Motor (26)
        if (stepper) {
            motorSelect.innerHTML += `<option value="${stepper.index}">${stepper.name}</option>`;
        }
    }
}

function toggleAddDosingFields() {
    const acidEnabled = document.getElementById('ctrlAcidEnabled')?.checked || false;
    const alkalineEnabled = document.getElementById('ctrlAlkalineEnabled')?.checked || false;
    
    const acidFields = document.getElementById('acidDosingAddFields');
    const alkalineFields = document.getElementById('alkalineDosingAddFields');
    
    if (acidFields) acidFields.style.display = acidEnabled ? 'block' : 'none';
    if (alkalineFields) alkalineFields.style.display = alkalineEnabled ? 'block' : 'none';
}

async function updateAddpHOutputOptions(type) {
    const outputTypeSelect = document.getElementById(`ctrl${type.charAt(0).toUpperCase() + type.slice(1)}OutputType`);
    const outputSelect = document.getElementById(`ctrl${type.charAt(0).toUpperCase() + type.slice(1)}Output`);
    const motorPowerGroup = document.getElementById(`${type}AddMotorPowerGroup`);
    const mfcFlowRateGroup = document.getElementById(`${type}AddMfcFlowRateGroup`);
    
    if (!outputTypeSelect || !outputSelect) return;
    
    const outputType = parseInt(outputTypeSelect.value);
    const outputs = window.phAddOutputs || [];
    const motors = window.phAddMotors || [];
    
    // Show/hide motor power field
    if (motorPowerGroup) {
        motorPowerGroup.style.display = outputType === 1 ? 'block' : 'none';
    }
    
    // Show/hide MFC flow rate field
    if (mfcFlowRateGroup) {
        mfcFlowRateGroup.style.display = outputType === 2 ? 'block' : 'none';
    }
    
    // Populate output dropdown
    outputSelect.innerHTML = '<option value="">-- Select Output --</option>';
    
    if (outputType === 0) {
        // Digital outputs (21-25)
        outputs.forEach(output => {
            outputSelect.innerHTML += `<option value="${output.index}">${output.name}</option>`;
        });
    } else if (outputType === 1) {
        // DC motors (27-30)
        motors.forEach(motor => {
            outputSelect.innerHTML += `<option value="${motor.index}">${motor.name}</option>`;
        });
    } else if (outputType === 2) {
        // MFC devices (50-69) - fetch from device sensors
        // Use control index (c) for device commands, not sensor index (i)
        try {
            const response = await fetch('/api/inputs');
            if (response.ok) {
                const data = await response.json();
                if (data.devices) {
                    const mfcDevices = data.devices
                        .filter(d => {
                            const nameLower = (d.n || '').toLowerCase();
                            return (nameLower.includes('mfc') || nameLower.includes('mass flow') || d.t === 6) && d.c;
                        })
                        .map(d => ({ index: d.c, name: d.n || `MFC Device ${d.c}` }));
                    mfcDevices.forEach(mfc => {
                        outputSelect.innerHTML += `<option value="${mfc.index}">${mfc.name}</option>`;
                    });
                }
            }
        } catch (error) {
            console.error('[pH ADD] Error loading MFC devices:', error);
        }
    }
}

async function createController() {
    if (!selectedControllerType) {
        showToast('warning', 'Selection Required', 'Please select a controller type');
        return;
    }
    
    // Get the available index
    const availableIndices = selectedControllerType.indices.filter(idx => {
        return !controllersData.find(c => c.index === idx);
    });
    
    if (availableIndices.length === 0) {
        showToast('warning', 'No Slots Available', `All ${selectedControllerType.name} slots are in use`);
        return;
    }
    
    const index = availableIndices[0];
    
    // Route to appropriate creation function based on controller type
    if (selectedControllerType.value === 1) {
        // pH Controller
        await createpHController(index);
    } else if (selectedControllerType.value === 2) {
        // Flow Controller
        await createFlowController(index);
    } else if (selectedControllerType.value === 3) {
        // Dissolved Oxygen Controller
        await createDOController(index);
    } else {
        // Temperature Controller
        await createTempController(index);
    }
}

async function createTempController(index) {
    // Gather config data from the expanded form
    const configData = {
        isActive: true,
        name: document.getElementById('ctrlName').value,
        showOnDashboard: document.getElementById('ctrlShowDashboard').checked,
        unit: '°C',
        pvSourceIndex: parseInt(document.getElementById('ctrlTempSensor').value),
        outputIndex: parseInt(document.getElementById('ctrlOutput').value),
        controlMethod: parseInt(document.getElementById('ctrlMode').value),
        setpoint: parseFloat(document.getElementById('ctrlSetpoint').value),
        hysteresis: parseFloat(document.getElementById('ctrlHysteresis').value),
        kP: parseFloat(document.getElementById('ctrlKp').value),
        kI: parseFloat(document.getElementById('ctrlKi').value),
        kD: parseFloat(document.getElementById('ctrlKd').value),
        integralWindup: parseFloat(document.getElementById('ctrlWindup').value),
        outputMin: parseFloat(document.getElementById('ctrlOutMin').value),
        outputMax: parseFloat(document.getElementById('ctrlOutMax').value),
        enabled: false
    };
    
    // Validation
    if (!configData.name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    if (!configData.pvSourceIndex) {
        showToast('warning', 'Validation Error', 'Please select a temperature sensor');
        return;
    }
    
    if (!configData.outputIndex) {
        showToast('warning', 'Validation Error', 'Please select a heater output');
        return;
    }
    
    try {
        console.log(`[CONTROLLERS] Creating temp controller at index ${index}`, configData);
        
        const response = await fetch(`/api/config/tempcontroller/${index}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to create controller');
        }
        
        showToast('success', 'Success', 'Temperature controller created successfully');
        closeAddControllerModal();
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error creating:', error);
        showToast('error', 'Error', error.message);
    }
}

async function createpHController(index) {
    console.log(`[DEBUG] Creating pH controller - getting config data`);
    // Gather config data from the expanded form
    const configData = {
        isActive: true,
        name: document.getElementById('addCtrlName').value,
        showOnDashboard: document.getElementById('addCtrlShowDashboard').checked,
        pvSourceIndex: parseInt(document.getElementById('addCtrlpHSensor').value),
        setpoint: parseFloat(document.getElementById('addCtrlSetpoint').value),
        deadband: parseFloat(document.getElementById('addCtrlDeadband').value),
        acidDosing: {
            enabled: document.getElementById('ctrlAcidEnabled').checked,
            outputType: parseInt(document.getElementById('ctrlAcidOutputType').value),
            outputIndex: parseInt(document.getElementById('ctrlAcidOutput').value),
            motorPower: parseInt(document.getElementById('ctrlAcidMotorPower').value),
            mfcFlowRate_mL_min: parseFloat(document.getElementById('ctrlAcidMfcFlowRate')?.value || 100),
            dosingTime_ms: parseInt(document.getElementById('ctrlAcidDoseTime').value),
            dosingInterval_ms: parseInt(document.getElementById('ctrlAcidDoseInterval').value)
        },
        alkalineDosing: {
            enabled: document.getElementById('ctrlAlkalineEnabled').checked,
            outputType: parseInt(document.getElementById('ctrlAlkalineOutputType').value),
            outputIndex: parseInt(document.getElementById('ctrlAlkalineOutput').value),
            motorPower: parseInt(document.getElementById('ctrlAlkalineMotorPower').value),
            mfcFlowRate_mL_min: parseFloat(document.getElementById('ctrlAlkalineMfcFlowRate')?.value || 100),
            dosingTime_ms: parseInt(document.getElementById('ctrlAlkalineDoseTime').value),
            dosingInterval_ms: parseInt(document.getElementById('ctrlAlkalineDoseInterval').value)
        }
    };
    console.log(`[DEBUG] Creating pH controller - config data:`, configData);
    console.log(`[DEBUG] Creating pH controller - validating`);
    
    // Validation
    if (!configData.name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    if (!configData.pvSourceIndex || isNaN(configData.pvSourceIndex)) {
        showToast('warning', 'Validation Error', 'Please select a pH sensor');
        return;
    }
    
    if (!configData.acidDosing.enabled && !configData.alkalineDosing.enabled) {
        showToast('warning', 'Validation Error', 'At least one dosing direction must be enabled');
        return;
    }
    
    if (configData.acidDosing.enabled && (!configData.acidDosing.outputIndex || isNaN(configData.acidDosing.outputIndex))) {
        showToast('warning', 'Validation Error', 'Please select an acid dosing output');
        return;
    }
    
    if (configData.alkalineDosing.enabled && (!configData.alkalineDosing.outputIndex || isNaN(configData.alkalineDosing.outputIndex))) {
        showToast('warning', 'Validation Error', 'Please select an alkaline dosing output');
        return;
    }
    console.log(`[DEBUG] Creating pH controller - validation passed`);
    
    try {
        console.log(`[CONTROLLERS] Creating pH controller at index ${index}`, configData);
        
        const response = await fetch(`/api/config/phcontroller/${index}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to create controller');
        }
        
        showToast('success', 'Success', 'pH Controller created successfully');
        closeAddControllerModal();
        await loadControllers();
        
    } catch (error) {
        console.error('[pH CTRL] Error creating:', error);
        showToast('error', 'Error', error.message);
    }
}

async function createFlowController(index) {
    // Gather config data from the expanded form
    const outputType = parseInt(document.getElementById('addFlowCtrlOutputType').value);
    const calibMotorPower = outputType === 1 ? parseInt(document.getElementById('addFlowCtrlCalibPower').value) : 50;
    
    const configData = {
        isActive: true,
        name: document.getElementById('addFlowCtrlName').value,
        showOnDashboard: document.getElementById('addFlowCtrlShowDashboard').checked,
        flowRate_mL_min: parseFloat(document.getElementById('addFlowCtrlFlowRate').value),
        outputType: outputType,
        outputIndex: parseInt(document.getElementById('addFlowCtrlOutput').value),
        motorPower: calibMotorPower,  // Use calibration power for runtime dosing too
        calibrationVolume_mL: parseFloat(document.getElementById('addFlowCtrlCalibVolume').value),
        calibrationDoseTime_ms: parseInt(document.getElementById('addFlowCtrlCalibTime').value),
        calibrationMotorPower: calibMotorPower,
        minDosingInterval_ms: parseInt(document.getElementById('addFlowCtrlMinInterval').value),
        maxDosingTime_ms: parseInt(document.getElementById('addFlowCtrlMaxTime').value)
    };
    
    // Validation
    if (!configData.name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    if (!configData.outputIndex || isNaN(configData.outputIndex)) {
        showToast('warning', 'Validation Error', 'Please select an output');
        return;
    }
    
    if (configData.calibrationVolume_mL <= 0) {
        showToast('warning', 'Validation Error', 'Calibration volume must be greater than 0');
        return;
    }
    
    if (configData.calibrationDoseTime_ms <= 0) {
        showToast('warning', 'Validation Error', 'Calibration dose time must be greater than 0');
        return;
    }
    
    try {
        console.log(`[CONTROLLERS] Creating flow controller at index ${index}`, configData);
        
        const response = await fetch(`/api/config/flowcontroller/${index}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to create flow controller');
        }
        
        showToast('success', 'Success', 'Flow Controller created successfully');
        closeAddControllerModal();
        await loadControllers();
        
    } catch (error) {
        console.error('[FLOW CTRL] Error creating:', error);
        showToast('error', 'Error', error.message);
    }
}

async function createDOController(index) {
    // Load profiles first
    await loadDOProfiles();
    
    // Read form values
    const sensorValue = document.getElementById('addDOCtrlSensor')?.value;
    const profileValue = document.getElementById('addDOCtrlProfile')?.value;
    const stirrerEnabled = document.getElementById('addDOCtrlStirrerEnabled')?.checked || false;
    const mfcEnabled = document.getElementById('addDOCtrlMFCEnabled')?.checked || false;
    const stirrerIndexValue = document.getElementById('addDOCtrlStirrerIndex')?.value;
    const mfcIndexValue = document.getElementById('addDOCtrlMFCIndex')?.value;
    
    // Validation
    const name = document.getElementById('addDOCtrlName').value.trim();
    if (!name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    if (!sensorValue) {
        showToast('warning', 'Validation Error', 'Please select a DO sensor');
        return;
    }
    
    if (!profileValue) {
        showToast('warning', 'Validation Error', 'Please select a control profile');
        return;
    }
    
    if (!stirrerEnabled && !mfcEnabled) {
        showToast('warning', 'Validation Error', 'At least one output (stirrer or MFC) must be enabled');
        return;
    }
    
    if (stirrerEnabled && !stirrerIndexValue) {
        showToast('warning', 'Validation Error', 'Please select a stirrer motor');
        return;
    }
    
    if (mfcEnabled && !mfcIndexValue) {
        showToast('warning', 'Validation Error', 'Please select an MFC device');
        return;
    }
    
    // Check if selected profile exists and is active
    const profileIndex = parseInt(profileValue);
    const selectedProfile = doProfiles[profileIndex];
    if (!selectedProfile || !selectedProfile.isActive || selectedProfile.numPoints < 2) {
        showToast('warning', 'Validation Error', 'Please select a valid profile with at least 2 points. Create a profile first if needed.');
        return;
    }
    
    // Gather config data
    const configData = {
        isActive: true,
        name: name,
        showOnDashboard: document.getElementById('addDOCtrlShowDashboard')?.checked || false,
        pvSourceIndex: parseInt(sensorValue),
        setpoint_mg_L: parseFloat(document.getElementById('addDOCtrlSetpoint')?.value || 8.0),
        activeProfileIndex: profileIndex,
        stirrerEnabled: stirrerEnabled,
        stirrerType: stirrerEnabled ? parseInt(document.getElementById('addDOCtrlStirrerType')?.value || 0) : 0,
        stirrerIndex: stirrerEnabled ? parseInt(stirrerIndexValue) : 0,
        mfcEnabled: mfcEnabled,
        mfcDeviceIndex: mfcEnabled ? parseInt(mfcIndexValue) : 0
    };
    
    try {
        console.log(`[CONTROLLERS] Creating DO controller at index ${index}`, configData);
        
        const response = await fetch(`/api/config/docontroller/48`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to create DO controller');
        }
        
        showToast('success', 'Success', 'DO Controller created successfully');
        closeAddControllerModal();
        await loadControllers();
        
    } catch (error) {
        console.error('[DO CTRL] Error creating:', error);
        showToast('error', 'Error', error.message);
    }
}

async function openControllerConfig(index) {
    currentConfigIndex = index;
    console.log(`[CONTROLLERS] Opening config for controller ${index}`);
    
    try {
        // Determine endpoint based on controller type
        let endpoint;
        if (index === 43) {
            endpoint = `/api/config/phcontroller/${index}`;
        } else if (index >= 44 && index <= 47) {
            endpoint = `/api/config/flowcontroller/${index}`;
        } else if (index === 48) {
            endpoint = `/api/config/docontroller/${index}`;
        } else if (index >= 40 && index <= 42) {
            endpoint = `/api/config/tempcontroller/${index}`;
        } else {
            endpoint = `/api/controller/${index}`;
        }
        
        const response = await fetch(endpoint);
        if (!response.ok) throw new Error('Failed to load config');
        
        const config = await response.json();
        
        // Render appropriate form based on controller type
        if (index === 43) {
            await renderpHConfigForm(config);
        } else if (index >= 44 && index <= 47) {
            await renderFlowConfigForm(config);
        } else if (index === 48) {
            await renderDOConfigForm(config);
        } else {
            await renderConfigForm(config);
        }
        
        const modal = document.getElementById('controllerConfigModal');
        modal.style.display = 'flex';
        modal.classList.add('active');
        
    } catch (error) {
        console.error('[CONTROLLERS] Error loading config:', error);
        showToast('error', 'Error', 'Failed to load controller configuration');
    }
}

function closeControllerConfigModal() {
    const modal = document.getElementById('controllerConfigModal');
    if (modal) {
        modal.style.display = 'none';
        modal.classList.remove('active');
    }
    currentConfigIndex = null;
}

async function renderConfigForm(config) {
    const container = document.getElementById('controllerConfigContent');
    if (!container) return;
    
    const sensors = await loadTempSensors();
    const outputs = await loadAvailableOutputs();
    
    const controlMethod = config.controlMethod || 0;
    
    container.innerHTML = `
        <div class="form-group">
            <label for="ctrlName">Name:</label>
            <input type="text" id="ctrlName" value="${config.name || ''}" maxlength="39">
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="ctrlShowDashboard" ${config.showOnDashboard ? 'checked' : ''}>
                Show on Dashboard
            </label>
        </div>
        
        <div class="form-group">
            <label for="ctrlTempSensor">Temperature Sensor:</label>
            <select id="ctrlTempSensor">
                <option value="">-- Select Sensor --</option>
                ${sensors.map(s => `<option value="${s.index}" ${config.pvSourceIndex === s.index ? 'selected' : ''}>${s.name}</option>`).join('')}
            </select>
        </div>
        
        <div class="form-group">
            <label for="ctrlOutput">Heater Output:</label>
            <select id="ctrlOutput">
                <option value="">-- Select Output --</option>
                ${outputs.map(o => `<option value="${o.index}" ${config.outputIndex === o.index ? 'selected' : ''}>${o.name}</option>`).join('')}
            </select>
        </div>
        
        <div class="form-group">
            <label for="ctrlMode">Control Mode:</label>
            <select id="ctrlMode" onchange="toggleControlModeFields()">
                <option value="0" ${controlMethod === 0 ? 'selected' : ''}>On/Off</option>
                <option value="1" ${controlMethod === 1 ? 'selected' : ''}>PID</option>
            </select>
        </div>
        
        <div id="onOffParams" style="display: ${controlMethod === 0 ? 'block' : 'none'};">
            <div class="form-group">
                <label for="ctrlHysteresis">Hysteresis (°C):</label>
                <input type="number" id="ctrlHysteresis" value="${config.hysteresis || 0.5}" step="0.1" min="0.1">
            </div>
        </div>
        
        <div id="pidParams" style="display: ${controlMethod === 1 ? 'block' : 'none'};">
            <div class="form-group">
                <label for="ctrlKp">Proportional Gain (kP):</label>
                <input type="number" id="ctrlKp" value="${config.kP || 2.0}" step="0.1">
            </div>
            <div class="form-group">
                <label for="ctrlKi">Integral Gain (kI):</label>
                <input type="number" id="ctrlKi" value="${config.kI || 0.5}" step="0.01">
            </div>
            <div class="form-group">
                <label for="ctrlKd">Derivative Gain (kD):</label>
                <input type="number" id="ctrlKd" value="${config.kD || 0.1}" step="0.01">
            </div>
            <div class="form-group">
                <label for="ctrlWindup">Integral Windup Limit:</label>
                <input type="number" id="ctrlWindup" value="${config.integralWindup || 100.0}" step="1">
            </div>
            <div class="form-group">
                <label for="ctrlOutMin">Output Min (%):</label>
                <input type="number" id="ctrlOutMin" value="${config.outputMin || 0}" step="1" min="0" max="100">
            </div>
            <div class="form-group">
                <label for="ctrlOutMax">Output Max (%):</label>
                <input type="number" id="ctrlOutMax" value="${config.outputMax || 100}" step="1" min="0" max="100">
            </div>
        </div>
        
        <div class="form-group">
            <label for="ctrlSetpoint">Initial Setpoint (°C):</label>
            <input type="number" id="ctrlSetpoint" value="${config.setpoint || 25.0}" step="0.1">
        </div>
    `;
}

function toggleControlModeFields() {
    const mode = parseInt(document.getElementById('ctrlMode').value);
    document.getElementById('onOffParams').style.display = mode === 0 ? 'block' : 'none';
    document.getElementById('pidParams').style.display = mode === 1 ? 'block' : 'none';
}

async function loadTempSensors() {
    const sensors = [];
    
    try {
        const response = await fetch('/api/inputs');
        if (!response.ok) return sensors;
        
        const data = await response.json();
        
        if (data.rtd) {
            data.rtd.forEach(rtd => {
                sensors.push({
                    index: rtd.i,
                    name: `[${rtd.i}] ${rtd.n} (${rtd.v.toFixed(1)}${rtd.u})`
                });
            });
        }
        
        if (data.devices) {
            data.devices.forEach(ds => {
                if (ds.t == 2) {
                    sensors.push({
                        index: ds.i,
                        name: `[${ds.i}] ${ds.n || 'Temperature Sensor'} (${ds.v.toFixed(1)}${ds.u})`
                    });
                }
            });
        }
        
    } catch (error) {
        console.error('[CONTROLLERS] Error loading sensors:', error);
    }
    
    return sensors;
}

async function loadAvailableOutputs() {
    const outputs = [];
    
    try {
        const response = await fetch('/api/outputs');
        if (!response.ok) return outputs;
        
        const data = await response.json();
        if (data.digitalOutputs) {
            data.digitalOutputs.forEach(output => {
                outputs.push({
                    index: output.index,
                    name: `[${output.index}] ${output.name}`
                });
            });
        }
        
    } catch (error) {
        console.error('[CONTROLLERS] Error loading outputs:', error);
    }
    
    return outputs;
}

async function renderpHConfigForm(config) {
    const container = document.getElementById('controllerConfigContent');
    if (!container) return;
    
    // Get pH sensors from device sensors
    let phSensors = [];
    try {
        const inputsResponse = await fetch('/api/inputs');
        if (inputsResponse.ok) {
            const inputsData = await inputsResponse.json();
            if (inputsData.devices) {
                phSensors = inputsData.devices
                    .filter(ds => ds.t && ds.t == 3)
                    .map(ds => ({ index: ds.i, name: `[${ds.i}] ${ds.n || 'pH Sensor'} (${ds.v.toFixed(1)}${ds.u})` }));
            }
        }
    } catch (error) {
        console.error('[pH CONFIG] Error loading pH sensors:', error);
    }
    
    const outputs = await loadAvailableOutputs();
    
    // Get DC motors for dosing options
    let dcMotors = [];
    try {
        const outputsResponse = await fetch('/api/outputs');
        if (outputsResponse.ok) {
            const outputsData = await outputsResponse.json();
            if (outputsData.dcMotors) {
                dcMotors = outputsData.dcMotors.map(m => ({ index: m.index, name: m.name }));
            }
        }
    } catch (error) {
        console.error('[pH CONFIG] Error loading DC motors:', error);
    }
    
    container.innerHTML = `
        <div class="form-group">
            <label for="phCtrlName">Name:</label>
            <input type="text" id="phCtrlName" value="${config.name || ''}" maxlength="39">
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="phCtrlShowDashboard" ${config.showOnDashboard ? 'checked' : ''}>
                Show on Dashboard
            </label>
        </div>
        
        <div class="form-group">
            <label for="phCtrlSensor">pH Sensor:</label>
            <select id="phCtrlSensor">
                <option value="">-- Select pH Sensor --</option>
                ${phSensors.map(s => `<option value="${s.index}" ${config.pvSourceIndex === s.index ? 'selected' : ''}>${s.name}</option>`).join('')}
            </select>
        </div>
        
        <div class="form-group">
            <label for="phCtrlSetpoint">Setpoint (pH):</label>
            <input type="number" id="phCtrlSetpoint" value="${config.setpoint || 7.0}" step="0.1" min="0" max="14">
        </div>
        
        <div class="form-group">
            <label for="phCtrlDeadband">Deadband (pH):</label>
            <input type="number" id="phCtrlDeadband" value="${config.deadband || 0.2}" step="0.05" min="0.05">
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        
        <h4 style="margin-bottom: 15px;">Acid Dosing Configuration</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="phAcidEnabled" ${config.acidDosing?.enabled ? 'checked' : ''} onchange="toggleDosingFields()">
                Enable Acid Dosing
            </label>
        </div>
        
        <div id="acidDosingFields" style="display: ${config.acidDosing?.enabled ? 'block' : 'none'};">
            <div class="form-group">
                <label for="phAcidOutputType">Output Type:</label>
                <select id="phAcidOutputType" onchange="updatepHOutputOptions('acid')">
                    <option value="0" ${config.acidDosing?.outputType === 0 ? 'selected' : ''}>Digital Output</option>
                    <option value="1" ${config.acidDosing?.outputType === 1 ? 'selected' : ''}>DC Motor</option>
                    <option value="2" ${config.acidDosing?.outputType === 2 ? 'selected' : ''}>Mass Flow Controller</option>
                </select>
            </div>
            
            <div class="form-group">
                <label for="phAcidOutput">Output:</label>
                <select id="phAcidOutput">
                    <option value="">-- Select Output --</option>
                </select>
            </div>
            
            <div class="form-group" id="acidMotorPowerGroup" style="display: ${config.acidDosing?.outputType === 1 ? 'block' : 'none'};">
                <label for="phAcidMotorPower">Motor Power (%):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="phAcidMotorPower" value="${config.acidDosing?.motorPower || 50}" min="1" max="100" style="flex: 1;">
                    <span>%</span>
                </div>
            </div>
            
            <div class="form-group" id="acidMfcFlowRateGroup" style="display: ${config.acidDosing?.outputType === 2 ? 'block' : 'none'};">
                <label for="phAcidMfcFlowRate">Flow Rate (mL/min):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="phAcidMfcFlowRate" value="${config.acidDosing?.mfcFlowRate_mL_min || 100}" min="0" max="10000" step="0.1" style="flex: 1;">
                    <span>mL/min</span>
                </div>
            </div>
            
            <div class="form-group">
                <label for="phAcidDoseTime">Dose Time (ms):</label>
                <input type="number" id="phAcidDoseTime" value="${config.acidDosing?.dosingTime_ms || 1000}" min="100" max="60000" step="100">
            </div>
            
            <div class="form-group">
                <label for="phAcidDoseInterval">Dose Interval (ms):</label>
                <input type="number" id="phAcidDoseInterval" value="${config.acidDosing?.dosingInterval_ms || 60000}" min="1000" max="3600000" step="1000">
            </div>
            
            <div class="form-group" id="acidVolumePerDoseGroup" style="display: ${config.acidDosing?.outputType === 2 ? 'none' : 'block'};">
                <label for="phAcidVolumePerDose">Volume Per Dose (mL):</label>
                <input type="number" id="phAcidVolumePerDose" value="${config.acidDosing?.volumePerDose_mL || 0.5}" min="0.1" max="100" step="0.1">
            </div>
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        
        <h4 style="margin-bottom: 15px;">Alkaline Dosing Configuration</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="phAlkalineEnabled" ${config.alkalineDosing?.enabled ? 'checked' : ''} onchange="toggleDosingFields()">
                Enable Alkaline Dosing
            </label>
        </div>
        
        <div id="alkalineDosingFields" style="display: ${config.alkalineDosing?.enabled ? 'block' : 'none'};">
            <div class="form-group">
                <label for="phAlkalineOutputType">Output Type:</label>
                <select id="phAlkalineOutputType" onchange="updatepHOutputOptions('alkaline')">
                    <option value="0" ${config.alkalineDosing?.outputType === 0 ? 'selected' : ''}>Digital Output</option>
                    <option value="1" ${config.alkalineDosing?.outputType === 1 ? 'selected' : ''}>DC Motor</option>
                    <option value="2" ${config.alkalineDosing?.outputType === 2 ? 'selected' : ''}>Mass Flow Controller</option>
                </select>
            </div>
            
            <div class="form-group">
                <label for="phAlkalineOutput">Output:</label>
                <select id="phAlkalineOutput">
                    <option value="">-- Select Output --</option>
                </select>
            </div>
            
            <div class="form-group" id="alkalineMotorPowerGroup" style="display: ${config.alkalineDosing?.outputType === 1 ? 'block' : 'none'};">
                <label for="phAlkalineMotorPower">Motor Power (%):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="phAlkalineMotorPower" value="${config.alkalineDosing?.motorPower || 50}" min="1" max="100" style="flex: 1;">
                    <span>%</span>
                </div>
            </div>
            
            <div class="form-group" id="alkalineMfcFlowRateGroup" style="display: ${config.alkalineDosing?.outputType === 2 ? 'block' : 'none'};">
                <label for="phAlkalineMfcFlowRate">Flow Rate (mL/min):</label>
                <div style="display: flex; align-items: center; gap: 8px;">
                    <input type="number" id="phAlkalineMfcFlowRate" value="${config.alkalineDosing?.mfcFlowRate_mL_min || 100}" min="0" max="10000" step="0.1" style="flex: 1;">
                    <span>mL/min</span>
                </div>
            </div>
            
            <div class="form-group">
                <label for="phAlkalineDoseTime">Dose Time (ms):</label>
                <input type="number" id="phAlkalineDoseTime" value="${config.alkalineDosing?.dosingTime_ms || 1000}" min="100" max="60000" step="100">
            </div>
            
            <div class="form-group">
                <label for="phAlkalineDoseInterval">Dose Interval (ms):</label>
                <input type="number" id="phAlkalineDoseInterval" value="${config.alkalineDosing?.dosingInterval_ms || 60000}" min="1000" max="3600000" step="1000">
            </div>
            
            <div class="form-group" id="alkalineVolumePerDoseGroup" style="display: ${config.alkalineDosing?.outputType === 2 ? 'none' : 'block'};">
                <label for="phAlkalineVolumePerDose">Volume Per Dose (mL):</label>
                <input type="number" id="phAlkalineVolumePerDose" value="${config.alkalineDosing?.volumePerDose_mL || 0.5}" min="0.1" max="100" step="0.1">
            </div>
        </div>
    `;
    
    // Store outputs and motors for later use
    window.phConfigOutputs = outputs;
    window.phConfigMotors = dcMotors;
    window.phConfigData = config;
    
    // Wait a tick to ensure DOM is ready, then initialize output dropdowns
    setTimeout(() => {
        updatepHOutputOptions('acid');
        updatepHOutputOptions('alkaline');
    }, 0);
}

function toggleDosingFields() {
    const acidEnabled = document.getElementById('phAcidEnabled')?.checked || false;
    const alkalineEnabled = document.getElementById('phAlkalineEnabled')?.checked || false;
    
    const acidFields = document.getElementById('acidDosingFields');
    const alkalineFields = document.getElementById('alkalineDosingFields');
    
    if (acidFields) acidFields.style.display = acidEnabled ? 'block' : 'none';
    if (alkalineFields) alkalineFields.style.display = alkalineEnabled ? 'block' : 'none';
}

async function updatepHOutputOptions(type) {
    const outputTypeSelect = document.getElementById(`ph${type.charAt(0).toUpperCase() + type.slice(1)}OutputType`);
    const outputSelect = document.getElementById(`ph${type.charAt(0).toUpperCase() + type.slice(1)}Output`);
    const motorPowerGroup = document.getElementById(`${type}MotorPowerGroup`);
    const mfcFlowRateGroup = document.getElementById(`${type}MfcFlowRateGroup`);
    const volumePerDoseGroup = document.getElementById(`${type}VolumePerDoseGroup`);
    
    if (!outputTypeSelect || !outputSelect) return;
    
    const outputType = parseInt(outputTypeSelect.value);
    const outputs = window.phConfigOutputs || [];
    const motors = window.phConfigMotors || [];
    const config = window.phConfigData || {};
    
    // Show/hide motor power field
    if (motorPowerGroup) {
        motorPowerGroup.style.display = outputType === 1 ? 'block' : 'none';
    }
    
    // Show/hide MFC flow rate field
    if (mfcFlowRateGroup) {
        mfcFlowRateGroup.style.display = outputType === 2 ? 'block' : 'none';
    }
    
    // Show/hide volume per dose field (hide for MFC since it's calculated)
    if (volumePerDoseGroup) {
        volumePerDoseGroup.style.display = outputType === 2 ? 'none' : 'block';
    }
    
    // Populate output dropdown
    outputSelect.innerHTML = '<option value="">-- Select Output --</option>';
    
    if (outputType === 0) {
        // Digital outputs (21-25)
        outputs.forEach(output => {
            const selected = (type === 'acid' && config.acidDosing?.outputIndex === output.index) ||
                           (type === 'alkaline' && config.alkalineDosing?.outputIndex === output.index);
            outputSelect.innerHTML += `<option value="${output.index}" ${selected ? 'selected' : ''}>${output.name}</option>`;
        });
    } else if (outputType === 1) {
        // DC motors (27-30)
        motors.forEach(motor => {
            const selected = (type === 'acid' && config.acidDosing?.outputIndex === motor.index) ||
                           (type === 'alkaline' && config.alkalineDosing?.outputIndex === motor.index);
            outputSelect.innerHTML += `<option value="${motor.index}" ${selected ? 'selected' : ''}>${motor.name}</option>`;
        });
    } else if (outputType === 2) {
        // MFC devices (50-69) - fetch from device sensors
        // Use control index (c) for device commands, not sensor index (i)
        try {
            const response = await fetch('/api/inputs');
            if (response.ok) {
                const data = await response.json();
                if (data.devices) {
                    const mfcDevices = data.devices
                        .filter(d => {
                            const nameLower = (d.n || '').toLowerCase();
                            return (nameLower.includes('mfc') || nameLower.includes('mass flow') || d.t === 6) && d.c;
                        })
                        .map(d => ({ index: d.c, name: d.n || `MFC Device ${d.c}` }));
                    mfcDevices.forEach(mfc => {
                        const selected = (type === 'acid' && config.acidDosing?.outputIndex === mfc.index) ||
                                       (type === 'alkaline' && config.alkalineDosing?.outputIndex === mfc.index);
                        outputSelect.innerHTML += `<option value="${mfc.index}" ${selected ? 'selected' : ''}>${mfc.name}</option>`;
                    });
                }
            }
        } catch (error) {
            console.error('[pH CONFIG] Error loading MFC devices:', error);
        }
    }
}

async function saveControllerConfig() {
    if (currentConfigIndex === null) return;
    
    // Route to appropriate save function based on controller type
    if (currentConfigIndex === 43) {
        await savepHControllerConfig();
        return;
    } else if (currentConfigIndex >= 44 && currentConfigIndex <= 47) {
        await saveFlowControllerConfig();
        return;
    } else if (currentConfigIndex === 48) {
        await saveDOControllerConfig();
        return;
    }
    
    const configData = {
        isActive: true,
        name: document.getElementById('ctrlName').value,
        showOnDashboard: document.getElementById('ctrlShowDashboard').checked,
        unit: '°C',
        pvSourceIndex: parseInt(document.getElementById('ctrlTempSensor').value),
        outputIndex: parseInt(document.getElementById('ctrlOutput').value),
        controlMethod: parseInt(document.getElementById('ctrlMode').value),
        setpoint: parseFloat(document.getElementById('ctrlSetpoint').value),
        hysteresis: parseFloat(document.getElementById('ctrlHysteresis').value),
        kP: parseFloat(document.getElementById('ctrlKp').value),
        kI: parseFloat(document.getElementById('ctrlKi').value),
        kD: parseFloat(document.getElementById('ctrlKd').value),
        integralWindup: parseFloat(document.getElementById('ctrlWindup').value),
        outputMin: parseFloat(document.getElementById('ctrlOutMin').value),
        outputMax: parseFloat(document.getElementById('ctrlOutMax').value),
        enabled: false
    };
    
    if (!configData.name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    if (!configData.pvSourceIndex) {
        showToast('warning', 'Validation Error', 'Please select a temperature sensor');
        return;
    }
    
    if (!configData.outputIndex) {
        showToast('warning', 'Validation Error', 'Please select a heater output');
        return;
    }
    
    try {
        const response = await fetch(`/api/config/tempcontroller/${currentConfigIndex}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to save configuration');
        }
        
        showToast('success', 'Success', 'Controller configuration saved');
        // Force full card rebuild by removing the existing card
        const existingCard = document.querySelector(`.controller-card[data-index="${currentConfigIndex}"]`);
        if (existingCard) existingCard.remove();
        await loadControllers();  // Refresh data - will trigger full render since card count differs
        closeControllerConfigModal();  // Then close modal
        
    } catch (error) {
        console.error('[CONTROLLERS] Error saving:', error);
        showToast('error', 'Error', error.message);
    }
}

async function savepHControllerConfig() {
    const configData = {
        isActive: true,
        name: document.getElementById('phCtrlName').value,
        showOnDashboard: document.getElementById('phCtrlShowDashboard').checked,
        pvSourceIndex: parseInt(document.getElementById('phCtrlSensor').value),
        setpoint: parseFloat(document.getElementById('phCtrlSetpoint').value),
        deadband: parseFloat(document.getElementById('phCtrlDeadband').value),
        acidDosing: {
            enabled: document.getElementById('phAcidEnabled').checked,
            outputType: parseInt(document.getElementById('phAcidOutputType').value),
            outputIndex: parseInt(document.getElementById('phAcidOutput').value),
            motorPower: parseInt(document.getElementById('phAcidMotorPower').value),
            mfcFlowRate_mL_min: parseFloat(document.getElementById('phAcidMfcFlowRate')?.value || 100),
            dosingTime_ms: parseInt(document.getElementById('phAcidDoseTime').value),
            dosingInterval_ms: parseInt(document.getElementById('phAcidDoseInterval').value),
            volumePerDose_mL: parseFloat(document.getElementById('phAcidVolumePerDose').value)
        },
        alkalineDosing: {
            enabled: document.getElementById('phAlkalineEnabled').checked,
            outputType: parseInt(document.getElementById('phAlkalineOutputType').value),
            outputIndex: parseInt(document.getElementById('phAlkalineOutput').value),
            motorPower: parseInt(document.getElementById('phAlkalineMotorPower').value),
            mfcFlowRate_mL_min: parseFloat(document.getElementById('phAlkalineMfcFlowRate')?.value || 100),
            dosingTime_ms: parseInt(document.getElementById('phAlkalineDoseTime').value),
            dosingInterval_ms: parseInt(document.getElementById('phAlkalineDoseInterval').value),
            volumePerDose_mL: parseFloat(document.getElementById('phAlkalineVolumePerDose').value)
        }
    };
    
    // Validation
    if (!configData.name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    if (!configData.pvSourceIndex || isNaN(configData.pvSourceIndex)) {
        showToast('warning', 'Validation Error', 'Please select a pH sensor');
        return;
    }
    
    if (!configData.acidDosing.enabled && !configData.alkalineDosing.enabled) {
        showToast('warning', 'Validation Error', 'At least one dosing direction must be enabled');
        return;
    }
    
    if (configData.acidDosing.enabled && (!configData.acidDosing.outputIndex || isNaN(configData.acidDosing.outputIndex))) {
        showToast('warning', 'Validation Error', 'Please select an acid dosing output');
        return;
    }
    
    if (configData.alkalineDosing.enabled && (!configData.alkalineDosing.outputIndex || isNaN(configData.alkalineDosing.outputIndex))) {
        showToast('warning', 'Validation Error', 'Please select an alkaline dosing output');
        return;
    }
    
    try {
        const response = await fetch(`/api/config/phcontroller/43`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to save configuration');
        }
        
        showToast('success', 'Success', 'pH Controller configuration saved');
        // Force full card rebuild by removing the existing card
        const existingCard = document.querySelector(`.controller-card[data-index="43"]`);
        if (existingCard) existingCard.remove();
        await loadControllers();  // Refresh data - will trigger full render since card count differs
        closeControllerConfigModal();  // Then close modal
        
    } catch (error) {
        console.error('[pH CTRL] Error saving:', error);
        showToast('error', 'Error', error.message);
    }
}

async function renderFlowConfigForm(config) {
    const container = document.getElementById('controllerConfigContent');
    if (!container) return;
    
    // Get available outputs and DC motors
    let outputs = [];
    let dcMotors = [];
    try {
        const outputsResponse = await fetch('/api/outputs');
        if (outputsResponse.ok) {
            const outputsData = await outputsResponse.json();
            outputs = outputsData.digitalOutputs || [];
            dcMotors = outputsData.dcMotors || [];
        }
    } catch (error) {
        console.error('[FLOW CONFIG] Error loading outputs:', error);
    }
    
    container.innerHTML = `
        <h4>Flow Controller Configuration</h4>
        
        <div class="form-group">
            <label for="flowCtrlName">Name:</label>
            <input type="text" id="flowCtrlName" value="${config.name || ''}" maxlength="39">
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="flowCtrlShowDashboard" ${config.showOnDashboard ? 'checked' : ''}>
                Show on Dashboard
            </label>
        </div>
        
        <div class="form-group">
            <label for="flowCtrlFlowRate">Target Flow Rate (mL/min):</label>
            <input type="number" id="flowCtrlFlowRate" value="${config.flowRate_mL_min || 10.0}" step="0.1" min="0.1">
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Output Configuration</h4>
        
        <div class="form-group">
            <label for="flowCtrlOutputType">Output Type:</label>
            <select id="flowCtrlOutputType" onchange="updateFlowConfigOutputOptions()">
                <option value="0" ${config.outputType === 0 ? 'selected' : ''}>Digital Output</option>
                <option value="1" ${config.outputType === 1 ? 'selected' : ''}>DC Motor</option>
            </select>
        </div>
        
        <div class="form-group">
            <label for="flowCtrlOutput">Output:</label>
            <select id="flowCtrlOutput">
                <option value="">-- Select Output --</option>
            </select>
        </div>
            
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Calibration Data</h4>
            
        <div class="form-group">
            <label for="flowCtrlCalibVolume">Calibration Volume (mL):</label>
            <input type="number" id="flowCtrlCalibVolume" value="${config.calibrationVolume_mL || 10.0}" step="0.1" min="0.1">
        </div>
        
        <div class="form-group">
            <label for="flowCtrlCalibTime">Calibration Dose Time (ms):</label>
            <input type="number" id="flowCtrlCalibTime" value="${config.calibrationDoseTime_ms || 5000}" min="100" max="60000" step="100">
        </div>
        
        <div class="form-group" id="flowConfigCalibPowerGroup" style="display: ${config.outputType === 1 ? 'block' : 'none'};">
            <label for="flowCtrlCalibPower">Calibration Motor Power (%):</label>
            <input type="number" id="flowCtrlCalibPower" value="${config.calibrationMotorPower || 50}" min="1" max="100">
            <small style="color: #7f8c8d;">Motor power used during calibration (applies to both calibration and dosing)</small>
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Safety Limits</h4>
        
        <div class="form-group">
            <label for="flowCtrlMinInterval">Min Dosing Interval (ms):</label>
            <input type="number" id="flowCtrlMinInterval" value="${config.minDosingInterval_ms || 1000}" min="100" max="60000" step="100">
        </div>
        
        <div class="form-group">
            <label for="flowCtrlMaxTime">Max Dosing Time (ms):</label>
            <input type="number" id="flowCtrlMaxTime" value="${config.maxDosingTime_ms || 10000}" min="100" max="60000" step="100">
        </div>
    `;
    
    // Store references for output options update
    window.flowConfigOutputs = outputs;
    window.flowConfigMotors = dcMotors;
    window.flowConfigData = config;
    
    // Initialize output dropdown
    setTimeout(() => {
        updateFlowConfigOutputOptions();
    }, 0);
}

function updateFlowConfigOutputOptions() {
    const outputTypeSelect = document.getElementById('flowCtrlOutputType');
    const outputSelect = document.getElementById('flowCtrlOutput');
    const calibPowerGroup = document.getElementById('flowConfigCalibPowerGroup');
    
    if (!outputTypeSelect || !outputSelect) return;
    
    const outputType = parseInt(outputTypeSelect.value);
    const outputs = window.flowConfigOutputs || [];
    const motors = window.flowConfigMotors || [];
    const config = window.flowConfigData || {};
    
    // Clear current options
    outputSelect.innerHTML = '<option value="">-- Select Output --</option>';
    
    if (outputType === 0) {
        // Digital outputs (21-25)
        outputs.forEach(o => {
            if (o.index >= 21 && o.index <= 25) {
                const selected = o.index === config.outputIndex ? 'selected' : '';
                outputSelect.innerHTML += `<option value="${o.index}" ${selected}>${o.name}</option>`;
            }
        });
        if (calibPowerGroup) calibPowerGroup.style.display = 'none';
    } else {
        // DC motors (27-30)
        motors.forEach(m => {
            const selected = m.index === config.outputIndex ? 'selected' : '';
            outputSelect.innerHTML += `<option value="${m.index}" ${selected}>${m.name}</option>`;
        });
        if (calibPowerGroup) calibPowerGroup.style.display = 'block';
    }
}

async function saveFlowControllerConfig() {
    const outputType = parseInt(document.getElementById('flowCtrlOutputType').value);
    const calibMotorPower = outputType === 1 ? parseInt(document.getElementById('flowCtrlCalibPower').value) : 50;
    
    const configData = {
        isActive: true,
        name: document.getElementById('flowCtrlName').value,
        showOnDashboard: document.getElementById('flowCtrlShowDashboard').checked,
        flowRate_mL_min: parseFloat(document.getElementById('flowCtrlFlowRate').value),
        outputType: outputType,
        outputIndex: parseInt(document.getElementById('flowCtrlOutput').value),
        motorPower: calibMotorPower,  // Use calibration power for runtime dosing too
        calibrationVolume_mL: parseFloat(document.getElementById('flowCtrlCalibVolume').value),
        calibrationDoseTime_ms: parseInt(document.getElementById('flowCtrlCalibTime').value),
        calibrationMotorPower: calibMotorPower,
        minDosingInterval_ms: parseInt(document.getElementById('flowCtrlMinInterval').value),
        maxDosingTime_ms: parseInt(document.getElementById('flowCtrlMaxTime').value)
    };
    
    // Validation
    if (!configData.name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    if (!configData.outputIndex || isNaN(configData.outputIndex)) {
        showToast('warning', 'Validation Error', 'Please select an output');
        return;
    }
    
    if (configData.calibrationVolume_mL <= 0) {
        showToast('warning', 'Validation Error', 'Calibration volume must be greater than 0');
        return;
    }
    
    try {
        const response = await fetch(`/api/config/flowcontroller/${currentConfigIndex}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to save configuration');
        }
        
        showToast('success', 'Success', 'Flow Controller configuration saved');
        // Force full card rebuild by removing the existing card
        const existingCard = document.querySelector(`.controller-card[data-index="${currentConfigIndex}"]`);
        if (existingCard) existingCard.remove();
        await loadControllers();  // Refresh data - will trigger full render since card count differs
        closeControllerConfigModal();
        
    } catch (error) {
        console.error('[FLOW CTRL] Error saving:', error);
        showToast('error', 'Error', error.message);
    }
}

async function renderDOConfigForm(config) {
    const container = document.getElementById('controllerConfigContent');
    if (!container) return;
    
    // Fetch available motors and MFC devices
    let dcMotors = [];
    let stepperMotor = null;
    let mfcDevices = [];
    
    try {
        const outputsResponse = await fetch('/api/outputs');
        if (outputsResponse.ok) {
            const outputsData = await outputsResponse.json();
            if (outputsData.dcMotors) {
                dcMotors = outputsData.dcMotors.map(m => ({ index: m.index, name: m.name }));
            }
            if (outputsData.stepperMotor) {
                stepperMotor = { index: 26, name: outputsData.stepperMotor.name || 'Stepper Motor' };
            }
        }
        
        const inputsResponse = await fetch('/api/inputs');
        if (inputsResponse.ok) {
            const inputsData = await inputsResponse.json();
            if (inputsData.devices) {
                // MFC devices - filter for MFC in name or flow sensors
                mfcDevices = inputsData.devices
                    .filter(ds => {
                        const nameLower = (ds.n || '').toLowerCase();
                        return (nameLower.includes('mfc') || nameLower.includes('mass flow') || ds.t === 6) && ds.c;
                    })
                    .map(ds => ({ index: ds.c, name: ds.n || `MFC Device ${ds.c}` }));
            }
        }
    } catch (error) {
        console.error('[DO CONFIG] Error loading devices:', error);
    }
    
    container.innerHTML = `
        <h4>Dissolved Oxygen Controller Configuration</h4>
        
        <div class="form-group">
            <label for="doCtrlName">Name:</label>
            <input type="text" id="doCtrlName" value="${config.name || ''}" maxlength="39">
        </div>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="doCtrlShowDashboard" ${config.showOnDashboard ? 'checked' : ''}>
                Show on Dashboard
            </label>
        </div>
        
        <div class="form-group">
            <label for="doCtrlSetpoint">Setpoint (mg/L or %):</label>
            <input type="number" id="doCtrlSetpoint" value="${config.setpoint_mg_L || 8.0}" step="0.1" min="0">
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Stirring Control</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="doCtrlStirrerEnabled" ${config.stirrerEnabled ? 'checked' : ''} onchange="toggleDOConfigStirrerFields()">
                Enable Stirring
            </label>
        </div>
        
        <div id="doStirrerConfigFields" style="display: ${config.stirrerEnabled ? 'block' : 'none'};">
            <div class="form-group">
                <label for="doCtrlStirrerType">Stirrer Type:</label>
                <select id="doCtrlStirrerType" onchange="updateDOConfigStirrerOptions()">
                    <option value="0" ${config.stirrerType === 0 ? 'selected' : ''}>DC Motor</option>
                    <option value="1" ${config.stirrerType === 1 ? 'selected' : ''}>Stepper Motor</option>
                </select>
            </div>
            
            <div class="form-group">
                <label for="doCtrlStirrerIndex">Stirrer Motor:</label>
                <select id="doCtrlStirrerIndex">
                    <option value="">-- Select Motor --</option>
                    ${config.stirrerType === 0 ? 
                        dcMotors.map(m => `<option value="${m.index}" ${m.index === config.stirrerIndex ? 'selected' : ''}>${m.name}</option>`).join('') :
                        (stepperMotor ? `<option value="${stepperMotor.index}" ${stepperMotor.index === config.stirrerIndex ? 'selected' : ''}>${stepperMotor.name}</option>` : '')}
                </select>
            </div>
            
        </div>
        
        <hr style="margin: 20px 0; border: 0; border-top: 1px solid #ecf0f1;">
        <h4 style="margin-bottom: 15px;">Gas Flow Control (MFC)</h4>
        
        <div class="form-group">
            <label>
                <input type="checkbox" id="doCtrlMFCEnabled" ${config.mfcEnabled ? 'checked' : ''} onchange="toggleDOConfigMFCFields()">
                Enable MFC Gas Flow
            </label>
        </div>
        
        <div id="doMFCConfigFields" style="display: ${config.mfcEnabled ? 'block' : 'none'};">
            <div class="form-group">
                <label for="doCtrlMFCIndex">Mass Flow Controller:</label>
                <select id="doCtrlMFCIndex">
                    <option value="">-- Select MFC --</option>
                    ${mfcDevices.map(mfc => `<option value="${mfc.index}" ${mfc.index === config.mfcDeviceIndex ? 'selected' : ''}>${mfc.name}</option>`).join('')}
                </select>
            </div>
        </div>
    `;
    
    // Store motor/device lists for later use
    window.doConfigMotors = dcMotors;
    window.doConfigStepper = stepperMotor;
    window.doConfigMFCs = mfcDevices;
}

async function saveDOControllerConfig() {
    const configData = {
        isActive: true,
        name: document.getElementById('doCtrlName').value,
        showOnDashboard: document.getElementById('doCtrlShowDashboard').checked,
        setpoint_mg_L: parseFloat(document.getElementById('doCtrlSetpoint').value)
    };
    
    // Validation
    if (!configData.name) {
        showToast('warning', 'Validation Error', 'Please enter a name');
        return;
    }
    
    // Stirrer configuration
    const stirrerEnabled = document.getElementById('doCtrlStirrerEnabled').checked;
    configData.stirrerEnabled = stirrerEnabled;
    if (stirrerEnabled) {
        configData.stirrerType = parseInt(document.getElementById('doCtrlStirrerType').value);
        configData.stirrerIndex = parseInt(document.getElementById('doCtrlStirrerIndex').value);
        
        if (!configData.stirrerIndex) {
            showToast('warning', 'Validation Error', 'Please select a stirrer motor');
            return;
        }
    }
    
    // MFC configuration
    const mfcEnabled = document.getElementById('doCtrlMFCEnabled').checked;
    configData.mfcEnabled = mfcEnabled;
    if (mfcEnabled) {
        configData.mfcDeviceIndex = parseInt(document.getElementById('doCtrlMFCIndex').value);
        
        if (!configData.mfcDeviceIndex) {
            showToast('warning', 'Validation Error', 'Please select an MFC device');
            return;
        }
    }
    
    // Must have at least one output
    if (!stirrerEnabled && !mfcEnabled) {
        showToast('warning', 'Validation Error', 'At least one output (stirrer or MFC) must be enabled');
        return;
    }
    
    try {
        const response = await fetch('/api/config/docontroller/48', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const errorData = await response.json().catch(() => ({}));
            throw new Error(errorData.error || 'Failed to save configuration');
        }
        
        showToast('success', 'Configuration Saved', 'DO controller configuration updated successfully');
        // Force full card rebuild by removing the existing card
        const existingCard = document.querySelector(`.controller-card[data-index="48"]`);
        if (existingCard) existingCard.remove();
        await loadControllers();  // Refresh data - will trigger full render since card count differs
        closeControllerConfigModal();
    } catch (error) {
        console.error('Error saving DO controller config:', error);
        showToast('error', 'Save Failed', error.message);
    }
}

// Helper functions for DO controller config form
function toggleDOConfigStirrerFields() {
    const enabled = document.getElementById('doCtrlStirrerEnabled').checked;
    const fields = document.getElementById('doStirrerConfigFields');
    if (fields) {
        fields.style.display = enabled ? 'block' : 'none';
    }
}

function toggleDOConfigMFCFields() {
    const enabled = document.getElementById('doCtrlMFCEnabled').checked;
    const fields = document.getElementById('doMFCConfigFields');
    if (fields) {
        fields.style.display = enabled ? 'block' : 'none';
    }
}

function updateDOConfigStirrerOptions() {
    const stirrerType = parseInt(document.getElementById('doCtrlStirrerType').value);
    const motorSelect = document.getElementById('doCtrlStirrerIndex');
    
    motorSelect.innerHTML = '<option value="">-- Select Motor --</option>';
    
    if (stirrerType === 0) {
        // DC Motors (27-30)
        if (window.doConfigMotors) {
            window.doConfigMotors.forEach(m => {
                motorSelect.innerHTML += `<option value="${m.index}">${m.name}</option>`;
            });
        }
    } else {
        // Stepper Motor (26)
        if (window.doConfigStepper) {
            motorSelect.innerHTML += `<option value="${window.doConfigStepper.index}">${window.doConfigStepper.name}</option>`;
        }
    }
}

async function deleteController() {
    if (currentConfigIndex === null) return;
    
    if (!confirm('Are you sure you want to delete this controller?')) {
        return;
    }
    
    try {
        // Use RESTful endpoint for all controllers (temp 40-42, pH 43, flow 44-47, DO 48)
        const endpoint = `/api/controller/${currentConfigIndex}`;
        
        const response = await fetch(endpoint, {
            method: 'DELETE'
        });
        
        if (!response.ok) throw new Error('Failed to delete controller');
        
        showToast('success', 'Success', 'Controller deleted');
        closeControllerConfigModal();
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error deleting:', error);
        showToast('error', 'Error', 'Failed to delete controller');
    }
}

// ============================================================================
// CONTROLLER ACTIONS
// ============================================================================

async function updateControllerSetpoint(index) {
    const input = document.getElementById(`ctrl-setpoint-${index}`);
    if (!input) return;
    
    const setpoint = parseFloat(input.value);
    
    // Determine endpoint based on controller type
    let endpoint, body;
    if (index === 43) {
        endpoint = `/api/phcontroller/${index}/setpoint`;
        body = JSON.stringify({ setpoint });
    } else if (index >= 44 && index <= 47) {
        // Flow controller - use flowrate endpoint
        endpoint = `/api/flowcontroller/${index}/flowrate`;
        body = JSON.stringify({ flowRate: setpoint });
    } else if (index === 48) {
        // DO controller
        endpoint = `/api/docontroller/${index}/setpoint`;
        body = JSON.stringify({ setpoint });
    } else {
        // Temperature controller
        endpoint = `/api/controller/${index}/setpoint`;
        body = JSON.stringify({ setpoint });
    }
    
    try {
        const response = await fetch(endpoint, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: body
        });
        
        if (!response.ok) throw new Error('Failed to update setpoint');
        
        console.log(`[CONTROLLERS] Updated setpoint for ${index} to ${setpoint}`);
        showToast('success', 'Success', index >= 44 && index <= 47 ? 'Flow rate updated' : 'Setpoint updated');
        
    } catch (error) {
        console.error('[CONTROLLERS] Error updating setpoint:', error);
        showToast('error', 'Error', 'Failed to update setpoint');
    }
}

async function enableController(index) {
    // Check if controller is in fault state
    const ctrl = controllersData.find(c => c.index === index);
    if (ctrl && ctrl.fault) {
        showToast('error', 'Cannot Enable', `Controller is in fault state: ${ctrl.message || 'Unknown fault'}`);
        return;
    }
    
    // Determine endpoint based on controller type
    let endpoint;
    if (index === 43) {
        endpoint = `/api/phcontroller/${index}/enable`;
    } else if (index >= 44 && index <= 47) {
        endpoint = `/api/flowcontroller/${index}/enable`;
    } else if (index === 48) {
        endpoint = `/api/docontroller/${index}/enable`;
    } else {
        endpoint = `/api/controller/${index}/enable`;
    }
    
    try {
        const response = await fetch(endpoint, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error('Failed to enable controller');
        
        console.log(`[CONTROLLERS] Enabled controller ${index}`);
        showToast('success', 'Success', 'Controller enabled');
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error enabling:', error);
        showToast('error', 'Error', 'Failed to enable controller');
    }
}

async function disableController(index) {
    // Determine endpoint based on controller type
    let endpoint;
    if (index === 43) {
        endpoint = `/api/phcontroller/${index}/disable`;
    } else if (index >= 44 && index <= 47) {
        endpoint = `/api/flowcontroller/${index}/disable`;
    } else if (index === 48) {
        endpoint = `/api/docontroller/${index}/disable`;
    } else {
        endpoint = `/api/controller/${index}/disable`;
    }
    
    try {
        const response = await fetch(endpoint, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error('Failed to disable controller');
        
        console.log(`[CONTROLLERS] Disabled controller ${index}`);
        showToast('success', 'Success', 'Controller disabled');
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error disabling:', error);
        showToast('error', 'Error', 'Failed to disable controller');
    }
}

async function dosepHAcid(index) {
    try {
        const response = await fetch(`/api/phcontroller/${index}/dose-acid`, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error('Failed to dose acid');
        
        console.log(`[pH CTRL] Manual acid dose for ${index}`);
        showToast('info', 'Dosing', 'Acid dose started');
        await loadControllers();
        
    } catch (error) {
        console.error('[pH CTRL] Error dosing acid:', error);
        showToast('error', 'Error', 'Failed to dose acid');
    }
}

async function dosepHAlkaline(index) {
    try {
        const response = await fetch(`/api/phcontroller/${index}/dose-alkaline`, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error('Failed to dose alkaline');
        
        console.log(`[pH CTRL] Manual alkaline dose for ${index}`);
        showToast('info', 'Dosing', 'Alkaline dose started');
        await loadControllers();
        
    } catch (error) {
        console.error('[pH CTRL] Error dosing alkaline:', error);
        showToast('error', 'Error', 'Failed to dose alkaline');
    }
}

async function resetpHVolume(index, type) {
    const endpoint = type === 'acid' ? 'reset-acid-volume' : 'reset-alkaline-volume';
    const label = type === 'acid' ? 'Acid' : 'Alkaline';
    
    try {
        const response = await fetch(`/api/phcontroller/${index}/${endpoint}`, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error(`Failed to reset ${type} volume`);
        
        console.log(`[pH CTRL] ${label} volume reset for controller ${index}`);
        showToast('success', 'Volume Reset', `${label} cumulative volume reset to 0.0 mL`);
        await loadControllers();
        
    } catch (error) {
        console.error(`[pH CTRL] Error resetting ${type} volume:`, error);
        showToast('error', 'Error', `Failed to reset ${type} volume`);
    }
}

// ============================================================================
// FLOW CONTROLLER CONTROL FUNCTIONS
// ============================================================================

async function manualFlowDose(index) {
    try {
        const response = await fetch(`/api/flowcontroller/${index}/dose`, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error('Failed to trigger manual dose');
        
        console.log(`[FLOW CTRL] Manual dose for ${index}`);
        showToast('info', 'Dosing', 'Manual dose started');
        await loadControllers();
        
    } catch (error) {
        console.error('[FLOW CTRL] Error triggering manual dose:', error);
        showToast('error', 'Error', 'Failed to trigger manual dose');
    }
}

async function resetFlowVolume(index) {
    try {
        const response = await fetch(`/api/flowcontroller/${index}/reset-volume`, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error('Failed to reset volume');
        
        console.log(`[FLOW CTRL] Volume reset for controller ${index}`);
        showToast('success', 'Volume Reset', 'Cumulative volume reset to 0.0 mL');
        await loadControllers();
        
    } catch (error) {
        console.error('[FLOW CTRL] Error resetting volume:', error);
        showToast('error', 'Error', 'Failed to reset volume');
    }
}

async function startAutotune(index) {
    try {
        const response = await fetch(`/api/controller/${index}/autotune`, {
            method: 'POST'
        });
        
        if (!response.ok) throw new Error('Failed to start autotune');
        
        console.log(`[CONTROLLERS] Started autotune for ${index}`);
        showToast('info', 'Autotune', 'PID autotune started - this may take several minutes');
        
        // Clear autotune completion flag when starting new autotune
        autotuneJustCompleted.set(index, false);
        
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error starting autotune:', error);
        showToast('error', 'Error', 'Failed to start autotune');
    }
}

async function savePIDValues(index) {
    try {
        // Get current controller data
        const ctrl = controllersData.find(c => c.index === index);
        if (!ctrl) {
            throw new Error('Controller not found');
        }
        
        // Get current configuration
        const configResponse = await fetch(`/api/config/tempcontroller/${index}`);
        if (!configResponse.ok) throw new Error('Failed to load current configuration');
        
        const config = await configResponse.json();
        
        // Update only the PID values with the current runtime values
        config.kP = ctrl.kP;
        config.kI = ctrl.kI;
        config.kD = ctrl.kD;
        
        // Save the updated configuration
        const saveResponse = await fetch(`/api/config/tempcontroller/${index}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(config)
        });
        
        if (!saveResponse.ok) throw new Error('Failed to save PID values');
        
        console.log(`[CONTROLLERS] Saved PID values for controller ${index}: P=${ctrl.kP}, I=${ctrl.kI}, D=${ctrl.kD}`);
        showToast('success', 'PID Values Saved', `New gains: P=${ctrl.kP}, I=${ctrl.kI}, D=${ctrl.kD}`);
        
        // Clear the autotune completion flag after saving
        autotuneJustCompleted.set(index, false);
        
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error saving PID values:', error);
        showToast('error', 'Error', 'Failed to save PID values');
    }
}

// ============================================================================
// DO PROFILE MANAGEMENT
// ============================================================================

let doProfiles = [];
let currentDOProfile = null;
let doProfileChart = null;

async function loadDOProfiles() {
    try {
        const response = await fetch('/api/doprofiles');
        if (!response.ok) throw new Error('Failed to load DO profiles');
        const data = await response.json();
        // Convert efficient array format to object format for easier manipulation
        doProfiles = (data.profiles || []).map(p => {
            const points = [];
            if (p.errors && p.stirrers && p.mfcs) {
                for (let i = 0; i < p.numPoints; i++) {
                    points.push({
                        error: p.errors[i] || 0,
                        stirrer: p.stirrers[i] || 0,
                        mfc: p.mfcs[i] || 0
                    });
                }
            }
            return {
                index: p.index,
                isActive: p.isActive,
                name: p.name,
                numPoints: p.numPoints,
                points: points
            };
        });
        console.log('[DO PROFILES] Loaded', doProfiles.length, 'profiles');
    } catch (error) {
        console.error('[DO PROFILES] Error loading profiles:', error);
        showToast('error', 'Error', 'Failed to load DO profiles');
    }
}

function openDOProfileModal(profileIndex = null) {
    loadDOProfiles().then(() => {
        const modal = document.getElementById('doProfileModal');
        if (!modal) {
            createDOProfileModal();
            setTimeout(() => openDOProfileModal(profileIndex), 100);
            return;
        }
        
        if (profileIndex !== null) {
            // Edit existing profile
            currentDOProfile = doProfiles[profileIndex] || null;
            document.getElementById('doProfileName').value = currentDOProfile?.name || '';
            document.getElementById('doProfileIndex').value = profileIndex;
            
            // Load points into table
            const tbody = document.getElementById('doProfilePoints');
            tbody.innerHTML = '';
            
            if (currentDOProfile && currentDOProfile.points) {
                currentDOProfile.points.forEach((point, idx) => {
                    addDOProfilePointRow(point.error, point.stirrer, point.mfc);
                });
            }
        } else {
            // New profile - find first available slot
            currentDOProfile = null;
            const availableIndex = doProfiles.findIndex(p => !p.isActive);
            if (availableIndex === -1) {
                showToast('error', 'Error', 'All 3 profile slots are in use');
                return;
            }
            document.getElementById('doProfileIndex').value = availableIndex;
            document.getElementById('doProfileName').value = `Profile ${availableIndex + 1}`;
            document.getElementById('doProfilePoints').innerHTML = '';
            // Add initial row
            addDOProfilePointRow(0, 0, 0);
        }
        
        updateDOProfileChart();
        modal.style.display = 'block';
    });
}

function closeDOProfileModal() {
    const modal = document.getElementById('doProfileModal');
    if (modal) modal.style.display = 'none';
    currentDOProfile = null;
    if (doProfileChart) {
        doProfileChart.destroy();
        doProfileChart = null;
    }
}

function createDOProfileModal() {
    // Check if modal already exists
    if (document.getElementById('doProfileModal')) {
        return;
    }
    
    const modalHtml = `
        <div id="doProfileModal" class="modal" style="display:none;">
            <div class="modal-content" style="max-width: 900px;">
                <div class="modal-header">
                    <h2>DO Control Profile</h2>
                    <span class="close" onclick="closeDOProfileModal()">&times;</span>
                </div>
                <div class="modal-body">
                    <input type="hidden" id="doProfileIndex">
                    
                    <div class="form-group">
                        <label for="doProfileName">Profile Name:</label>
                        <input type="text" id="doProfileName" placeholder="e.g., Standard Aeration">
                    </div>
                    
                    <h3>Profile Points</h3>
                    <p style="color: #7f8c8d; font-size: 0.9em; margin-bottom: 15px;">
                        Define how stirrer and MFC outputs respond to DO error (Setpoint - Current DO). 
                        Points will be sorted by error value automatically.
                    </p>
                    
                    <table class="config-table">
                        <thead>
                            <tr>
                                <th>Error (mg/L)</th>
                                <th>Stirrer Output (% or RPM)</th>
                                <th>MFC Output (mL/min)</th>
                                <th>Actions</th>
                            </tr>
                        </thead>
                        <tbody id="doProfilePoints"></tbody>
                    </table>
                    
                    <button class="btn-primary" onclick="addDOProfilePointRow()" style="margin-top: 10px;">
                        Add Point
                    </button>
                    
                    <h3 style="margin-top: 25px;">Profile Visualization</h3>
                    <canvas id="doProfileChart" width="400" height="200"></canvas>
                </div>
                <div class="modal-buttons">
                    <button class="btn-danger" onclick="deleteDOProfile()" style="margin-right: auto;">Delete Profile</button>
                    <button class="modal-cancel" onclick="closeDOProfileModal()">Cancel</button>
                    <button class="btn-primary" onclick="saveDOProfile()">Save Profile</button>
                </div>
            </div>
        </div>
    `;
    
    document.body.insertAdjacentHTML('beforeend', modalHtml);
}

function addDOProfilePointRow(error = 0, stirrer = 0, mfc = 0) {
    const tbody = document.getElementById('doProfilePoints');
    const row = document.createElement('tr');
    row.innerHTML = `
        <td><input type="number" class="profile-error" value="${error}" step="0.1"></td>
        <td><input type="number" class="profile-stirrer" value="${stirrer}" step="1"></td>
        <td><input type="number" class="profile-mfc" value="${mfc}" step="1"></td>
        <td><button class="btn-danger" style="padding: 5px 10px; font-size: 0.9em;" onclick="this.closest('tr').remove(); updateDOProfileChart();">Remove</button></td>
    `;
    tbody.appendChild(row);
    
    // Add listeners to update chart
    row.querySelectorAll('input').forEach(input => {
        input.addEventListener('input', () => updateDOProfileChart());
    });
    
    updateDOProfileChart();
}

function updateDOProfileChart() {
    // Check if Chart.js is loaded
    if (typeof Chart === 'undefined') {
        console.warn('[DO PROFILE] Chart.js not loaded yet, retrying...');
        setTimeout(updateDOProfileChart, 100);
        return;
    }
    
    const canvas = document.getElementById('doProfileChart');
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    
    // Collect points from table
    const rows = document.querySelectorAll('#doProfilePoints tr');
    const points = Array.from(rows).map(row => ({
        error: parseFloat(row.querySelector('.profile-error').value) || 0,
        stirrer: parseFloat(row.querySelector('.profile-stirrer').value) || 0,
        mfc: parseFloat(row.querySelector('.profile-mfc').value) || 0
    })).sort((a, b) => a.error - b.error);
    
    if (doProfileChart) {
        doProfileChart.destroy();
    }
    
    doProfileChart = new Chart(ctx, {
        type: 'line',
        data: {
            datasets: [{
                label: 'Stirrer Output',
                data: points.map(p => ({ x: p.error, y: p.stirrer })),
                borderColor: '#3498db',
                backgroundColor: 'rgba(52, 152, 219, 0.1)',
                yAxisID: 'y'
            }, {
                label: 'MFC Output (mL/min)',
                data: points.map(p => ({ x: p.error, y: p.mfc })),
                borderColor: '#e74c3c',
                backgroundColor: 'rgba(231, 76, 60, 0.1)',
                yAxisID: 'y1'
            }]
        },
        options: {
            responsive: true,
            interaction: {
                mode: 'index',
                intersect: false
            },
            scales: {
                x: {
                    type: 'linear',
                    title: {
                        display: true,
                        text: 'DO Error (mg/L)'
                    }
                },
                y: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: {
                        display: true,
                        text: 'Stirrer Output'
                    }
                },
                y1: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: {
                        display: true,
                        text: 'MFC (mL/min)'
                    },
                    grid: {
                        drawOnChartArea: false
                    }
                }
            }
        }
    });
}

async function saveDOProfile() {
    try {
        const index = parseInt(document.getElementById('doProfileIndex').value);
        const name = document.getElementById('doProfileName').value.trim();
        
        if (!name) {
            showToast('error', 'Validation Error', 'Please enter a profile name');
            return;
        }
        
        // Collect points from table
        const rows = document.querySelectorAll('#doProfilePoints tr');
        const points = Array.from(rows).map(row => ({
            error: parseFloat(row.querySelector('.profile-error').value) || 0,
            stirrer: parseFloat(row.querySelector('.profile-stirrer').value) || 0,
            mfc: parseFloat(row.querySelector('.profile-mfc').value) || 0
        })).sort((a, b) => a.error - b.error);
        
        if (points.length < 2) {
            showToast('error', 'Validation Error', 'Profile must have at least 2 points');
            return;
        }
        
        if (points.length > 20) {
            showToast('error', 'Validation Error', 'Profile cannot have more than 20 points');
            return;
        }
        
        // Convert to efficient array format (saves ~40 bytes per point)
        const profileData = {
            isActive: true,
            name: name,
            numPoints: points.length,
            errors: points.map(p => p.error),
            stirrers: points.map(p => p.stirrer),
            mfcs: points.map(p => p.mfc)
        };
        
        const response = await fetch(`/api/doprofile/${index}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(profileData)
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to save profile');
        }
        
        showToast('success', 'Profile Saved', `Profile "${name}" saved successfully`);
        closeDOProfileModal();
        await loadDOProfiles();
        
        // Refresh profile dropdown in add controller modal if it's open
        refreshDOProfileDropdown();
        
        // Refresh controller card profile dropdowns
        await loadControllers();
        
    } catch (error) {
        console.error('[DO PROFILES] Error saving profile:', error);
        showToast('error', 'Error', error.message);
    }
}

async function deleteDOProfile() {
    const index = parseInt(document.getElementById('doProfileIndex').value);
    
    if (!confirm('Are you sure you want to delete this profile?')) return;
    
    try {
        const response = await fetch(`/api/doprofile/${index}`, {
            method: 'DELETE'
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to delete profile');
        }
        
        showToast('success', 'Profile Deleted', 'Profile deleted successfully');
        closeDOProfileModal();
        await loadDOProfiles();
        
        // Refresh profile dropdowns
        refreshDOProfileDropdown();
        await loadControllers();
        
    } catch (error) {
        console.error('[DO PROFILES] Error deleting profile:', error);
        showToast('error', 'Error', error.message);
    }
}

// Cleanup
window.addEventListener('beforeunload', () => {
    if (controllersPolling) {
        clearInterval(controllersPolling);
        controllersPolling = null;
    }
});

// Populate DO profile select dropdown on controller card
function populateDOProfileSelect(controllerIndex, activeProfileIndex) {
    const dropdown = document.getElementById(`do-profile-select-${controllerIndex}`);
    if (!dropdown) return;
    
    const profiles = doProfiles.filter(p => p.isActive && p.numPoints >= 2);
    
    dropdown.innerHTML = profiles.map((p, idx) => 
        `<option value="${idx}" ${idx === activeProfileIndex ? 'selected' : ''}>${p.name}</option>`
    ).join('');
}

// Switch DO controller to a different profile
async function switchDOProfile(controllerIndex, profileIndexStr) {
    const profileIndex = parseInt(profileIndexStr);
    
    if (isNaN(profileIndex) || profileIndex < 0) {
        showToast('error', 'Invalid Profile', 'Please select a valid profile');
        return;
    }
    
    try {
        const response = await fetch(`/api/config/docontroller/${controllerIndex}`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                activeProfileIndex: profileIndex
            })
        });
        
        if (!response.ok) {
            const errorData = await response.json().catch(() => ({}));
            throw new Error(errorData.error || 'Failed to switch profile');
        }
        
        showToast('success', 'Profile Switched', `Switched to profile: ${doProfiles[profileIndex]?.name || 'Unknown'}`);
        await loadControllers();
        
    } catch (error) {
        console.error('[DO CTRL] Error switching profile:', error);
        showToast('error', 'Error', error.message);
        // Restore previous selection
        await loadControllers();
    }
}

// Refresh DO profile dropdown in add controller modal
function refreshDOProfileDropdown() {
    const dropdown = document.getElementById('addDOCtrlProfile');
    if (!dropdown) return; // Add modal not open or not DO controller
    
    const currentValue = dropdown.value;
    const profiles = doProfiles.filter(p => p.isActive && p.numPoints >= 2);
    
    dropdown.innerHTML = '<option value="">-- Select Profile --</option>' +
        profiles.map((p, idx) => `<option value="${idx}">${p.name} (${p.numPoints} points)</option>`).join('');
    
    // Try to restore previous selection if still valid
    if (currentValue && currentValue < profiles.length) {
        dropdown.value = currentValue;
    }
}

// Export functions
window.openAddControllerModal = openAddControllerModal;
window.closeAddControllerModal = closeAddControllerModal;
window.updateSensorOptions = updateSensorOptions;
window.createController = createController;
window.openControllerConfig = openControllerConfig;
window.closeControllerConfigModal = closeControllerConfigModal;
window.saveControllerConfig = saveControllerConfig;
window.deleteController = deleteController;
window.toggleControlModeFields = toggleControlModeFields;
window.updateControllerSetpoint = updateControllerSetpoint;
window.enableController = enableController;
window.disableController = disableController;
window.startAutotune = startAutotune;
window.savePIDValues = savePIDValues;
window.openDOProfileModal = openDOProfileModal;
window.closeDOProfileModal = closeDOProfileModal;
window.addDOProfilePointRow = addDOProfilePointRow;
window.updateDOProfileChart = updateDOProfileChart;
window.saveDOProfile = saveDOProfile;
window.deleteDOProfile = deleteDOProfile;
window.switchDOProfile = switchDOProfile;
window.toggleDOStirrerFields = toggleDOStirrerFields;
window.toggleDOMFCFields = toggleDOMFCFields;
window.updateDOStirrerOptions = updateDOStirrerOptions;

// ============================================================================
// BACKUP/RESTORE FUNCTIONALITY
// ============================================================================

let backupCurrentStep = 1;
let backupAction = null;
let pendingBackupData = null;
let pendingBackupFilename = null;
let backupStepHistory = [];

function openBackupRestoreModal() {
    const modal = document.getElementById('backupRestoreModal');
    if (modal) {
        modal.classList.add('active');
        resetBackupModal();
        checkSDCardForBackup();
    }
}

function closeBackupRestoreModal() {
    const modal = document.getElementById('backupRestoreModal');
    if (modal) {
        modal.classList.remove('active');
        resetBackupModal();
    }
}

function resetBackupModal() {
    backupCurrentStep = 1;
    backupAction = null;
    pendingBackupData = null;
    pendingBackupFilename = null;
    backupStepHistory = [];
    
    document.querySelectorAll('.backup-step').forEach(s => s.classList.remove('active'));
    const step1 = document.getElementById('backupStep1');
    if (step1) step1.classList.add('active');
    
    const ioOnlyRadio = document.querySelector('input[name="importScope"][value="io_only"]');
    if (ioOnlyRadio) ioOnlyRadio.checked = true;
    
    const rebootWarning = document.getElementById('rebootWarning');
    if (rebootWarning) rebootWarning.style.display = 'none';
}

function checkSDCardForBackup() {
    fetch('/api/system/status')
        .then(response => response.json())
        .then(data => {
            const sdReady = data.sd && data.sd.inserted && data.sd.ready;
            
            const exportToSD = document.getElementById('exportToSDBtn');
            const importFromSD = document.getElementById('importFromSDBtn');
            
            if (exportToSD) {
                exportToSD.disabled = !sdReady;
                exportToSD.title = sdReady ? 'Save to SD card' : 'SD card not available';
            }
            if (importFromSD) {
                importFromSD.disabled = !sdReady;
                importFromSD.title = sdReady ? 'Load from SD card' : 'SD card not available';
            }
        })
        .catch(() => {
            const exportToSD = document.getElementById('exportToSDBtn');
            const importFromSD = document.getElementById('importFromSDBtn');
            if (exportToSD) exportToSD.disabled = true;
            if (importFromSD) importFromSD.disabled = true;
        });
}

function showBackupStep(stepId) {
    document.querySelectorAll('.backup-step').forEach(s => s.classList.remove('active'));
    const step = document.getElementById(stepId);
    if (step) step.classList.add('active');
}

function selectBackupAction(action) {
    backupAction = action;
    backupStepHistory.push('backupStep1');
    
    if (action === 'export') {
        showBackupStep('backupStep2Export');
    } else {
        showBackupStep('backupStep2Import');
    }
}

function backupGoBack() {
    const prevStep = backupStepHistory.pop();
    if (prevStep) {
        showBackupStep(prevStep);
    } else {
        showBackupStep('backupStep1');
    }
}

async function exportBackup(destination) {
    showBackupStep('backupStep4Result');
    document.getElementById('backupResultContent').innerHTML = '<div class="loading">Generating backup...</div>';
    
    try {
        const response = await fetch('/api/config/backup');
        if (!response.ok) {
            const error = await response.json().catch(() => ({ error: 'Failed to generate backup' }));
            throw new Error(error.error || 'Failed to generate backup');
        }
        
        const backupData = await response.json();
        const now = new Date();
        const timestamp = now.toISOString().replace(/[:.]/g, '-').replace('T', '_').slice(0, 19);
        
        if (destination === 'download') {
            const filename = `orc_backup_${timestamp}.json`;
            const blob = new Blob([JSON.stringify(backupData, null, 2)], { type: 'application/json' });
            const url = URL.createObjectURL(blob);
            
            const a = document.createElement('a');
            a.href = url;
            a.download = filename;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            URL.revokeObjectURL(url);
            
            showBackupResult(true, 'Backup Downloaded', `Configuration saved as ${filename}`);
        } else {
            const filename = `backup_${timestamp}.json`;
            
            const saveResponse = await fetch('/api/config/backup/sd', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ filename: filename, data: backupData })
            });
            
            if (!saveResponse.ok) {
                const error = await saveResponse.json().catch(() => ({ error: 'Failed to save to SD card' }));
                throw new Error(error.error || 'Failed to save to SD card');
            }
            
            showBackupResult(true, 'Backup Saved to SD', `Configuration saved as /backups/${filename}`);
            
            // Always refresh file list after SD backup if file manager is initialized
            // This ensures the new backup appears immediately in the /backups folder
            if (window.loadDirectoryFunc) {
                // Small delay to ensure file is fully written
                setTimeout(() => {
                    const normalizedPath = currentPath.replace(/\/$/, '');
                    console.log('[BACKUP] Refreshing file list, currentPath:', normalizedPath);
                    // Force refresh to /backups since that's where backups are stored
                    currentPath = '/backups';
                    window.loadDirectoryFunc('/backups');
                }, 500);
            }
        }
        
    } catch (error) {
        console.error('[BACKUP] Export error:', error);
        showBackupResult(false, 'Export Failed', error.message);
    }
}

function importBackup(source) {
    if (source === 'upload') {
        document.getElementById('backupFileInput').click();
    }
}

function handleBackupFileSelect(event) {
    const file = event.target.files[0];
    if (!file) return;
    
    const reader = new FileReader();
    reader.onload = function(e) {
        try {
            pendingBackupData = JSON.parse(e.target.result);
            pendingBackupFilename = file.name;
            
            if (!pendingBackupData.io_config) {
                throw new Error('Invalid backup file: missing io_config section');
            }
            
            showImportOptions();
        } catch (error) {
            console.error('[BACKUP] Parse error:', error);
            showToast('error', 'Invalid File', error.message);
        }
    };
    reader.readAsText(file);
    event.target.value = '';
}

async function showSDBackupList() {
    backupStepHistory.push('backupStep2Import');
    showBackupStep('backupStep3SDList');
    
    const listContainer = document.getElementById('sdBackupList');
    listContainer.innerHTML = '<div class="loading">Loading backups...</div>';
    
    try {
        const response = await fetch('/api/config/backup/sd/list');
        if (!response.ok) {
            throw new Error('Failed to load backup list');
        }
        
        const data = await response.json();
        
        if (!data.backups || data.backups.length === 0) {
            listContainer.innerHTML = '<div class="empty-message">No backup files found in /backups folder</div>';
            return;
        }
        
        listContainer.innerHTML = data.backups.map(backup => `
            <div class="sd-backup-item" onclick="selectSDBackup('${backup.path}', '${backup.name}')">
                <div>
                    <div class="backup-name">${backup.name}</div>
                    <div class="backup-date">${backup.modified || ''}</div>
                </div>
                <div class="backup-size">${formatFileSizeBackup(backup.size)}</div>
            </div>
        `).join('');
        
    } catch (error) {
        console.error('[BACKUP] List error:', error);
        listContainer.innerHTML = `<div class="error-message">${error.message}</div>`;
    }
}

function formatFileSizeBackup(bytes) {
    if (bytes === 0) return '0 B';
    const units = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(1024));
    return parseFloat((bytes / Math.pow(1024, i)).toFixed(2)) + ' ' + units[i];
}

async function selectSDBackup(path, name) {
    showBackupStep('backupStep4Result');
    document.getElementById('backupResultContent').innerHTML = '<div class="loading">Loading backup file...</div>';
    
    try {
        const response = await fetch(`/api/sd/view?path=${encodeURIComponent(path)}`);
        if (!response.ok) {
            throw new Error('Failed to load backup file');
        }
        
        const text = await response.text();
        pendingBackupData = JSON.parse(text);
        pendingBackupFilename = name;
        
        if (!pendingBackupData.io_config) {
            throw new Error('Invalid backup file: missing io_config section');
        }
        
        showImportOptions();
        
    } catch (error) {
        console.error('[BACKUP] Load error:', error);
        showBackupResult(false, 'Load Failed', error.message);
    }
}

function showImportOptions() {
    backupStepHistory.push(backupStepHistory[backupStepHistory.length - 1] === 'backupStep2Import' ? 'backupStep2Import' : 'backupStep3SDList');
    showBackupStep('backupStep3ImportOptions');
    
    const fileInfo = document.getElementById('backupFileInfo');
    const hasSystemConfig = !!pendingBackupData.system_config;
    
    fileInfo.innerHTML = `
        <div class="file-name">${pendingBackupFilename}</div>
        <div class="file-details">
            Contains: IO Configuration${hasSystemConfig ? ' + System Configuration' : ' only'}
        </div>
    `;
    
    const bothRadio = document.querySelector('input[name="importScope"][value="both"]');
    const bothLabel = bothRadio ? bothRadio.closest('.import-option') : null;
    if (bothRadio && bothLabel) {
        if (!hasSystemConfig) {
            bothRadio.disabled = true;
            bothLabel.style.opacity = '0.5';
            bothLabel.style.cursor = 'not-allowed';
        } else {
            bothRadio.disabled = false;
            bothLabel.style.opacity = '1';
            bothLabel.style.cursor = 'pointer';
        }
    }
    
    document.querySelectorAll('input[name="importScope"]').forEach(radio => {
        radio.addEventListener('change', function() {
            const rebootWarning = document.getElementById('rebootWarning');
            if (rebootWarning) {
                rebootWarning.style.display = this.value === 'both' ? 'flex' : 'none';
            }
        });
    });
}

async function confirmImport() {
    const importScope = document.querySelector('input[name="importScope"]:checked').value;
    const importBoth = importScope === 'both';
    
    if (importBoth) {
        if (!confirm('Importing system configuration will cause the system to reboot. Continue?')) {
            return;
        }
    }
    
    showBackupStep('backupStep4Result');
    document.getElementById('backupResultContent').innerHTML = '<div class="loading">Restoring configuration...</div>';
    
    try {
        const response = await fetch('/api/config/restore', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                data: pendingBackupData,
                import_system: importBoth
            })
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({ error: 'Restore failed' }));
            throw new Error(error.error || 'Restore failed');
        }
        
        const result = await response.json();
        
        if (importBoth && result.reboot) {
            showBackupResult(true, 'Configuration Restored', 'System is rebooting to apply changes...');
            setTimeout(() => {
                closeBackupRestoreModal();
                showReconnectMessage();
            }, 2000);
        } else {
            showBackupResult(true, 'Configuration Restored', 'IO configuration restored successfully. Changes are active immediately.');
        }
        
    } catch (error) {
        console.error('[BACKUP] Restore error:', error);
        showBackupResult(false, 'Restore Failed', error.message);
    }
}

function showBackupResult(success, title, message) {
    const resultContent = document.getElementById('backupResultContent');
    resultContent.className = 'backup-result ' + (success ? 'success' : 'error');
    resultContent.innerHTML = `
        <span class="mdi ${success ? 'mdi-check-circle' : 'mdi-alert-circle'}"></span>
        <div class="result-title">${title}</div>
        <div class="result-message">${message}</div>
    `;
}

function showReconnectMessage() {
    if (typeof window.showReconnectCountdown === 'function') {
        window.showReconnectCountdown();
        return;
    }
    
    const overlay = document.createElement('div');
    overlay.className = 'modal-overlay active';
    overlay.innerHTML = `
        <div class="confirmation-modal">
            <h3>System Rebooting</h3>
            <p>Reconnecting in <span id="reconnectCountdown">15</span> seconds...</p>
        </div>
    `;
    document.body.appendChild(overlay);
    
    let countdown = 15;
    const countdownEl = document.getElementById('reconnectCountdown');
    
    const interval = setInterval(() => {
        countdown--;
        if (countdownEl) countdownEl.textContent = countdown;
        
        if (countdown <= 0) {
            clearInterval(interval);
            window.location.reload();
        }
    }, 1000);
}

function loadConfigFromFile(path) {
    if (!confirm('Load this configuration file? This will restore the configuration from this backup.')) {
        return;
    }
    openBackupRestoreModal();
    selectSDBackup(path, path.split('/').pop());
}

function getLoadConfigIconSVG() {
    return `<svg viewBox="0 0 24 24"><path d="M14,2H6A2,2 0 0,0 4,4V20A2,2 0 0,0 6,22H18C18.45,22 18.85,21.85 19.19,21.6L14.76,17.17C13.79,17.69 12.7,18 11.5,18A6.5,6.5 0 0,1 5,11.5A6.5,6.5 0 0,1 11.5,5C14.59,5 17.16,7.18 17.8,10.08L20,12.28V19.59Z"/></svg>`;
}

window.openBackupRestoreModal = openBackupRestoreModal;
window.closeBackupRestoreModal = closeBackupRestoreModal;
window.selectBackupAction = selectBackupAction;
window.backupGoBack = backupGoBack;
window.exportBackup = exportBackup;
window.importBackup = importBackup;
window.handleBackupFileSelect = handleBackupFileSelect;
window.showSDBackupList = showSDBackupList;
window.selectSDBackup = selectSDBackup;
window.confirmImport = confirmImport;
window.loadConfigFromFile = loadConfigFromFile;
window.getLoadConfigIconSVG = getLoadConfigIconSVG;

// =============================================================================
// DASHBOARD MODULE
// =============================================================================

/* Downloaded SVGs (temp)

*/

// Dashboard SVG Icons (embedded for offline use)
const DASHBOARD_ICONS = {
    // Object type icons
    sensor: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M7.705 2.344c-1.274.013-2.667.439-3.646 1.562-2.014 2.31-2.068 6.129.03 8.533.95 1.087 1.995 1.875 2.696 2.637.702.763 1.064 1.372.998 2.39-.057.886-.457 1.312-.892 1.624-.436.312-.83.404-.83.404a1 1 0 0 0-.754 1.195 1 1 0 0 0 1.195.754s.778-.173 1.553-.728a4.153 4.153 0 0 0 1.724-3.12c.106-1.634-.64-2.915-1.521-3.872-.88-.957-1.89-1.717-2.66-2.6-1.339-1.533-1.276-4.477-.032-5.904.55-.631 1.44-.902 2.329-.873.887.029 1.681.465 1.804.629.291.388.393.53.524.707.245.333.357.488.357.488a1 1 0 0 0 .256.281.993.993 0 0 1-.256-.281s-.112-.155-.357-.488a.86.86 0 0 0-.244.408l-.276 1.066a.86.86 0 0 0 .086.637l1.627 2.867a.86.86 0 0 0 .49.397l.85.265 5.455 9.614a.86.86 0 0 0 .6.421l1.156.202a.86.86 0 0 0 .947-.536l.406-1.043a.86.86 0 0 0-.054-.736l-5.487-9.67.178-.69a.86.86 0 0 0-.084-.638l-1.68-2.959a.86.86 0 0 0-.523-.406l-1.127-.305a.86.86 0 0 0-.605.065c-.15-.208-.306-.424-.637-.866-.752-1.001-2.003-1.386-3.342-1.43a5.969 5.969 0 0 0-.254-.001Zm4.59 2.799c.021.035.04.072.057.109a1 1 0 0 0-.057-.11zm.066.136c.01.023.018.046.026.069a1 1 0 0 0-.026-.069zm.065.23.004.034a1 1 0 0 0-.004-.033zm.01.178c0 .015-.003.029-.004.043a1 1 0 0 0 .004-.043zm-.09.38-.012.027a1 1 0 0 0 .012-.028zm.1.263.429.117 1.33 2.344-.178.69a.86.86 0 0 0 .086.638l5.457 9.617-.004.008-.06-.01-5.422-9.554a.86.86 0 0 0-.49-.395l-.85-.267-1.289-2.27.1-.387Zm-.323.05c-.017.017-.034.034-.053.05a1 1 0 0 0 .053-.05zm-.068.061a1 1 0 0 1-1.207.021 1 1 0 0 0 .347.163 1 1 0 0 0 .389.02 1 1 0 0 0 .367-.131 1 1 0 0 0 .104-.073z" style="stroke-linecap:round;stroke-linejoin:round;paint-order:stroke fill markers"/></svg>',
    input: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M8.895 2.87a1 1 0 0 0-1 1v3.136a1 1 0 0 0 2 0V4.869h9.136v14.262H9.895v-2.445a1 1 0 1 0-2 0v3.445a1 1 0 0 0 1 1H20.03a1 1 0 0 0 1-1V3.869a1 1 0 0 0-1-1zm4.447 4.368a1.05 1.05 0 0 0-1.014 1.05v1.169h-8.59c-.58 0-1.05.47-1.05 1.049v2.974c-.001.581.47 1.052 1.05 1.051h8.58v1.182c.003.987 1.244 1.425 1.866.658l3.021-3.744a1.05 1.05 0 0 0-.004-1.324l-3.01-3.68a1.05 1.05 0 0 0-.85-.385Z" style="stroke-linecap:round;stroke-linejoin:round;paint-order:stroke fill markers"/></svg>',
    output: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M3.8 2.87a1 1 0 0 0-1 1v16.26a1 1 0 0 0 1 1h11.138a1 1 0 0 0 1-1v-3.444a1 1 0 1 0-2 0v2.445H4.8V4.869h9.136v2.137a1 1 0 1 0 2 0V3.869a1 1 0 0 0-1-1zm13.698 4.368a1.05 1.05 0 0 0-1.014 1.05v1.169h-8.59c-.58 0-1.05.47-1.05 1.049v2.974c0 .581.47 1.052 1.05 1.051h8.58v1.182c.004.987 1.244 1.425 1.866.658l3.021-3.744a1.05 1.05 0 0 0-.004-1.324l-3.01-3.68a1.05 1.05 0 0 0-.849-.385Z" style="stroke-linecap:round;stroke-linejoin:round;paint-order:stroke fill markers"/></svg>',
    motor: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M12 2.502c-4.998 0-9.082 4.084-9.082 9.082 0 1.238.252 2.42.705 3.498v5.414a1 1 0 0 0 1 1H8.35a1 1 0 0 0 1-1v-.228a9.011 9.011 0 0 0 5.298.002v.226a1 1 0 0 0 1 1h3.729a1 1 0 0 0 1-1v-5.414a9.004 9.004 0 0 0 .705-3.498c0-4.998-4.084-9.082-9.082-9.082zm0 3a6.06 6.06 0 0 1 4.557 2.043 1.739 1.739 0 0 0-1.92 1.71c0 .94.784 1.722 1.722 1.722.659 0 1.24-.386 1.528-.94.128.494.195 1.012.195 1.547a6.057 6.057 0 0 1-4.67 5.918c.194-.278.309-.615.309-.975 0-.922-.755-1.692-1.672-1.718a3.241 3.241 0 0 0 3.176-3.225A3.24 3.24 0 0 0 12 8.359a3.24 3.24 0 0 0-3.225 3.225 3.241 3.241 0 0 0 3.176 3.225 1.735 1.735 0 0 0-1.672 1.718c0 .36.115.697.309.975a6.057 6.057 0 0 1-4.67-5.918c0-.535.067-1.053.195-1.547.289.554.869.94 1.528.94.938 0 1.72-.782 1.72-1.721a1.737 1.737 0 0 0-1.918-1.711A6.06 6.06 0 0 1 12 5.502ZM7.64 8.977c.143 0 .28.137.28.279a.295.295 0 0 1-.28.277.295.295 0 0 1-.279-.277c0-.142.138-.28.28-.28zm8.72 0c.141 0 .277.137.277.279a.293.293 0 0 1-.278.277.295.295 0 0 1-.279-.277c0-.142.137-.28.28-.28zM12 10.359a1.21 1.21 0 0 1 1.225 1.225A1.21 1.21 0 0 1 12 12.809a1.21 1.21 0 0 1-1.225-1.225A1.21 1.21 0 0 1 12 10.359zm0 5.889c.142 0 .28.137.28.28 0 .141-.138.279-.28.279a.297.297 0 0 1-.28-.28c0-.142.138-.279.28-.279zm-6.377 1.79a9.174 9.174 0 0 0 1.727 1.339v.12H5.623Zm12.754 0v1.458h-1.729v-.117a9.175 9.175 0 0 0 1.729-1.342z" style="stroke-linecap:round;stroke-linejoin:round;paint-order:stroke fill markers"/></svg>',
    controller: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M14.73 0c-.428-.002-.857.012-1.272.04-1.106.073-2.037.193-2.709.505-1.103.512-1.823 1.475-1.871 2.522-.041.91.406 1.797 1.24 2.486.392.324.416.992.416.992a.75.75 0 0 0 0 .024v.974H7.741c-.023-.055-.178-.441-.472-.904-.107-.168-.323-.25-.592-.307a4.667 4.667 0 0 0-.94-.082c-.68 0-1.4.096-1.785.264-.285.125-.597.467-.945.955a14.593 14.593 0 0 0-1.078 1.826C1.215 10.703.566 12.39.335 13.748.058 15.377-.051 16.7.022 17.715c.074 1.015.332 1.73.815 2.115a1.474 1.474 0 0 0 1.826 0c.548-.445.953-1.087 1.287-1.779.328-.678.633-1.11.633-1.11a.102.102 0 0 0 .004-.005l.014-.01a.34.34 0 0 1 .115-.053c.127-.036.37-.059.803.028.89.178 1.388.19 1.388.19h4.244a.1.1 0 0 0 .07-.03c.019.017.044.03.071.03h4.244s.497-.012 1.387-.19c.432-.087.677-.064.805-.028a.34.34 0 0 1 .115.053l.012.01a8.604 8.604 0 0 0 .639 1.115c.334.692.738 1.334 1.286 1.78.49.398 1.268.445 1.827 0 .482-.386.738-1.101.812-2.116.074-1.015-.035-2.338-.312-3.967-.232-1.358-.879-3.045-1.592-4.453a14.61 14.61 0 0 0-1.078-1.826c-.348-.488-.66-.83-.946-.955-.385-.168-1.106-.264-1.787-.264-.34 0-.668.025-.937.082-.27.058-.488.14-.594.307-.295.463-.45.849-.473.904h-2.666v-.95s.087-1.332-.959-2.196c-.603-.499-.718-.8-.697-1.264.015-.328.357-.928 1.004-1.228.203-.095 1.163-.302 2.174-.37 1.01-.067 2.146-.037 2.93.114 1.9.367 3.53.197 3.53.197a.75.75 0 0 0 .67-.822.75.75 0 0 0-.822-.67S18.432.497 16.77.176C16.147.056 15.441.004 14.73 0Zm2.828 7.803a.976.976 0 1 1-.002 1.951.976.976 0 0 1 .002-1.951zM4.8 8.225a.75.75 0 0 1 .75.75v.633h.793a.75.75 0 1 1 0 1.5H5.55v.633a.75.75 0 1 1-1.5 0v-.633h-.793a.75.75 0 0 1 0-1.5h.793v-.633a.75.75 0 0 1 .75-.75zm5.734.281a.75.75 0 0 0 .055.28.742.742 0 0 1-.055-.28zm1.5 0a.75.75 0 0 1-.055.28.75.75 0 0 0 .055-.28zm-.058.291a.744.744 0 0 1-.155.233.75.75 0 0 0 .155-.233zm-1.375.01a.75.75 0 0 0 .136.207.742.742 0 0 1-.136-.207zm1.2.238a.753.753 0 0 1-.228.153.75.75 0 0 0 .229-.153zm-1.025.008a.75.75 0 0 0 .196.129.751.751 0 0 1-.196-.129zm.79.148a.745.745 0 0 1-.274.055h-.008c-.011 0-.022-.003-.033-.004a.746.746 0 0 1-.236-.049.75.75 0 0 0 .27.053.75.75 0 0 0 .28-.055zm4.382.165a.976.976 0 1 1-.002 1.95.976.976 0 0 1 .002-1.95zm3.281 0a.976.976 0 1 1-.002 1.95.976.976 0 0 1 .002-1.95zm-10.523.431h1.188a.6.6 0 0 1 0 1.2H8.737a.6.6 0 0 1-.031-1.2Zm3.74 0h1.19a.6.6 0 0 1 0 1.2h-1.158a.6.6 0 0 1-.032-1.2zm5.112 1.162a.976.976 0 1 1-.002 1.952.976.976 0 0 1 .002-1.952zm-9.899 1.604c1.055 0 1.922.865 1.922 1.92a1.93 1.93 0 0 1-1.922 1.922 1.928 1.928 0 0 1-1.92-1.922c0-1.055.865-1.92 1.92-1.92zm6.95 0c1.054 0 1.921.865 1.921 1.92a1.93 1.93 0 0 1-1.922 1.922 1.928 1.928 0 0 1-1.92-1.922c0-1.055.865-1.92 1.92-1.92zm-6.95 1a.912.912 0 0 0-.92.92c0 .514.406.922.92.922a.915.915 0 0 0 .922-.922.913.913 0 0 0-.922-.92zm6.95 0a.912.912 0 0 0-.92.92c0 .514.405.922.92.922a.915.915 0 0 0 .921-.922.913.913 0 0 0-.922-.92zm3.331 3.322s.078.108.198.303c-.12-.195-.198-.303-.198-.303z" style="stroke-linecap:round;stroke-linejoin:round;paint-order:stroke fill markers"/></svg>',
    energy: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M11.32 3.648a9.376 9.376 0 0 0-3.847 1.149l.869 1.506a.5.5 0 1 1-.867.5l-.852-1.475a9.483 9.483 0 0 0-2.482 2.565c.039.01.077.026.113.046l1.437.83a.5.5 0 0 1-.5.866l-1.437-.828a.493.493 0 0 1-.102-.078 9.372 9.372 0 0 0-1.011 3.49h2.093a.75.75 0 1 1 0 1.5H2.625A9.391 9.391 0 0 0 4.02 18.03h4.927a3.106 3.106 0 0 1 2.979-3.062l5.347-6.694a.5.5 0 0 1 .852.506l-3.332 7.885c.21.412.334.875.338 1.365h4.928a9.392 9.392 0 0 0 1.392-4.279c-.014 0-.027.004-.04.004h-2.145a.75.75 0 1 1 0-1.5h2.144c.01 0 .02.003.031.004a9.374 9.374 0 0 0-.96-3.424l-1.413.816a.5.5 0 1 1-.5-.867l1.418-.818a9.472 9.472 0 0 0-2.47-2.592l-.82 1.422a.5.5 0 1 1-.866-.5l.842-1.46a9.382 9.382 0 0 0-3.852-1.183v2.133a.75.75 0 0 1-1.5 0z" transform="translate(-2.625 -3.648)"/><path fill="#5a6268" d="M12.04 15.963a2.094 2.094 0 0 0-2.095 2.094 2.094 2.094 0 0 0 2.094 2.093 2.094 2.094 0 0 0 2.094-2.093 2.094 2.094 0 0 0-2.094-2.094z" transform="translate(-2.625 -3.648)"/><path fill="#5a6268" d="m12.031 18 5.375-8.969-4.093 8.688z" transform="translate(-2.625 -3.648)"/></svg>',
    
    // Status icons
    ok: '<svg viewBox="0 0 24 24"><path fill="currentColor" d="M10,17L5,12L6.41,10.58L10,14.17L17.59,6.58L19,8M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2Z"/></svg>',
    run: '<svg viewBox="0 0 24 24"><path d="m13.683 7.388 8.367 4.83-8.367 4.83v2.324l12.393-7.154-12.393-7.154Z" style="fill:#28a745;stroke:#28a745;stroke-linecap:round;stroke-linejoin:round;paint-order:stroke fill markers" transform="matrix(.79856 0 0 1.0072 -3.663 -4.596)"/><path d="M.2 13.828V-.482l12.393 7.155Z" style="fill:#28a745;stroke:#28a745;stroke-linecap:round;stroke-linejoin:round;paint-order:stroke fill markers" transform="matrix(.79856 0 0 1.0072 .24 .989)"/></svg>',
    stopped: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M18,18H6V6H18V18Z"/></svg>',
    fault: '<svg viewBox="0 0 24 24"><path fill="currentColor" d="M13,14H11V10H13M13,18H11V16H13M1,21H23L12,2L1,21Z"/></svg>',
    offline: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M12,2A10,10 0 0,0 2,12A10,10 0 0,0 12,22A10,10 0 0,0 22,12A10,10 0 0,0 12,2M12,4C16.41,4 20,7.59 20,12C20,16.41 16.41,20 12,20C7.59,20 4,16.41 4,12C4,7.59 7.59,4 12,4M9,9V15H15V9"/></svg>',
    
    // Action icons
    clearVolumes: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M19.36,2.72L20.78,4.14L15.06,9.85C16.13,11.39 16.28,13.24 15.38,14.44L9.06,8.12C10.26,7.22 12.11,7.37 13.65,8.44L19.36,2.72M5.93,17.57C3.92,15.56 2.69,13.16 2.35,10.92L7.23,8.83L14.67,16.27L12.58,21.15C10.34,20.81 7.94,19.58 5.93,17.57Z"/></svg>',
    saveLayout: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M15,9H5V5H15M12,19A3,3 0 0,1 9,16A3,3 0 0,1 12,13A3,3 0 0,1 15,16A3,3 0 0,1 12,19M17,3H5C3.89,3 3,3.9 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V7L17,3Z"/></svg>',
    
    // Direction indicators
    forward: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M8.59,16.58L13.17,12L8.59,7.41L10,6L16,12L10,18L8.59,16.58Z"/></svg>',
    reverse: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M15.41,16.58L10.83,12L15.41,7.41L14,6L8,12L14,18L15.41,16.58Z"/></svg>',
    
    // Drag handle
    drag: '<svg viewBox="0 0 24 24"><path fill="#5a6268" d="M9,3H11V5H9V3M13,3H15V5H13V3M9,7H11V9H9V7M13,7H15V9H13V7M9,11H11V13H9V11M13,11H15V13H13V11M9,15H11V17H9V15M13,15H15V17H13V15M9,19H11V21H9V19M13,19H15V21H13V19Z"/></svg>'
};

// Dashboard State
let dashboardData = null;
let dashboardLayout = [];
let layoutModified = false;
let dashboardUserInteracting = false;  // Track if user is interacting with dashboard
let dashboardDragging = false;         // Track if user is dragging tiles

// Sparkline data storage - stores last N points for each controller
const SPARKLINE_MAX_POINTS = 30;
const sparklineData = {};  // Key: "type-index", Value: { pv: [], sp: [] }

// Update sparkline data for a controller
function updateSparklineData(obj) {
    const key = `${obj.type}-${obj.index}`;
    if (!sparklineData[key]) {
        sparklineData[key] = { pv: [], sp: [] };
    }
    
    const data = sparklineData[key];
    const pv = obj.processValue !== undefined ? obj.processValue : obj.value;
    const sp = obj.setpoint !== undefined ? obj.setpoint : null;
    
    // Add new values
    if (pv !== undefined && pv !== null && !isNaN(pv)) {
        data.pv.push(pv);
        if (data.pv.length > SPARKLINE_MAX_POINTS) data.pv.shift();
    }
    if (sp !== undefined && sp !== null && !isNaN(sp)) {
        data.sp.push(sp);
        if (data.sp.length > SPARKLINE_MAX_POINTS) data.sp.shift();
    }
}

// Render SVG sparkline for a controller with Y-axis labels
function renderSparkline(type, index, width = 180, height = 100) {
    const key = `${type}-${index}`;
    const data = sparklineData[key];
    
    // Layout constants
    const marginLeft = 26;   // Space for Y-axis labels
    const marginRight = 0;   // Trend extends to right edge
    const marginTop = 12;    // More room for top/bottom values toward center
    const marginBottom = 12;
    const chartWidth = width - marginLeft - marginRight;
    const chartHeight = height - marginTop - marginBottom;
    
    if (!data || data.pv.length < 2) {
        return `<svg class="sparkline" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">
            <text x="${marginLeft + chartWidth/2}" y="${height/2}" text-anchor="middle" fill="#bdc3c7" font-size="10">Collecting data...</text>
        </svg>`;
    }
    
    // Combine PV and SP to find global min/max for consistent scaling
    const allValues = [...data.pv, ...data.sp.filter(v => v !== null)];
    const min = Math.min(...allValues);
    const max = Math.max(...allValues);
    const range = max - min || 1;  // Avoid division by zero
    
    // Add 10% padding to range
    const padding = range * 0.1;
    const yMin = min - padding;
    const yMax = max + padding;
    const yRange = yMax - yMin;
    
    // Format axis value (compact)
    const formatAxisValue = (v) => {
        if (Math.abs(v) >= 100) return v.toFixed(0);
        if (Math.abs(v) >= 10) return v.toFixed(1);
        return v.toFixed(2);
    };
    
    // Y-axis tics (top, middle, bottom)
    const yMid = (yMin + yMax) / 2;
    const yToPixel = (v) => marginTop + chartHeight - ((v - yMin) / yRange) * chartHeight;
    
    // Generate paths - always use SPARKLINE_MAX_POINTS for consistent X spacing
    const pvPath = generateSparklinePath(data.pv, chartWidth, chartHeight, yMin, yRange, marginLeft, marginTop);
    const spPath = data.sp.length >= 2 ? generateSparklinePath(data.sp, chartWidth, chartHeight, yMin, yRange, marginLeft, marginTop) : '';
    
    // Grid lines
    const gridLines = `
        <line x1="${marginLeft}" y1="${yToPixel(yMax)}" x2="${marginLeft + chartWidth}" y2="${yToPixel(yMax)}" stroke="#e9ecef" stroke-width="1"/>
        <line x1="${marginLeft}" y1="${yToPixel(yMid)}" x2="${marginLeft + chartWidth}" y2="${yToPixel(yMid)}" stroke="#e9ecef" stroke-width="1" stroke-dasharray="2,2"/>
        <line x1="${marginLeft}" y1="${yToPixel(yMin)}" x2="${marginLeft + chartWidth}" y2="${yToPixel(yMin)}" stroke="#e9ecef" stroke-width="1"/>
    `;
    
    // Y-axis labels
    const labelX = marginLeft - 1;
    const axisLabels = `
        <text x="${labelX}" y="${yToPixel(yMax)}" text-anchor="end" fill="#7f8c8d" font-size="9" dominant-baseline="middle">${formatAxisValue(yMax)}</text>
        <text x="${labelX}" y="${yToPixel(yMid)}" text-anchor="end" fill="#7f8c8d" font-size="9" dominant-baseline="middle">${formatAxisValue(yMid)}</text>
        <text x="${labelX}" y="${yToPixel(yMin)}" text-anchor="end" fill="#7f8c8d" font-size="9" dominant-baseline="middle">${formatAxisValue(yMin)}</text>
    `;
    
    return `
        <svg class="sparkline" viewBox="0 0 ${width} ${height}" preserveAspectRatio="none">
            ${gridLines}
            ${axisLabels}
            ${spPath ? `<path d="${spPath}" fill="none" stroke="#6c757d" stroke-width="1" stroke-dasharray="3,2" opacity="0.7"/>` : ''}
            <path d="${pvPath}" fill="none" stroke="#0d6efd" stroke-width="1.5"/>
        </svg>
    `;
}

// Generate SVG path from data points
function generateSparklinePath(values, chartWidth, chartHeight, yMin, yRange, offsetX = 0, offsetY = 0) {
    if (values.length < 2) return '';
    
    // Use SPARKLINE_MAX_POINTS for consistent X spacing regardless of data length
    const xStep = chartWidth / (SPARKLINE_MAX_POINTS - 1);
    const startX = chartWidth - (values.length - 1) * xStep; // Right-align data
    
    const points = values.map((v, i) => {
        const x = offsetX + startX + i * xStep;
        const y = offsetY + chartHeight - ((v - yMin) / yRange) * chartHeight;
        return `${x.toFixed(1)},${y.toFixed(1)}`;
    });
    
    return `M${points.join(' L')}`;
}

// Run Timer State
let runState = 'idle';  // 'idle', 'running', 'paused', 'stopped'
let runStartTime = null;  // RTC timestamp string when run started
let runStartEpoch = null; // JS epoch time when run started (for duration calc)
let runAccumulatedMs = 0; // Accumulated runtime in ms (for pause/resume)
let runTimerInterval = null; // Interval ID for timer updates

// Dashboard Initialization
async function initDashboardTab() {
    
    // IMPORTANT: Set active tab so PollingManager knows to continue polling
    PollingManager.setActiveTab('dashboard');
    
    const container = document.getElementById('dashboard-tiles');
    if (container) {
        container.innerHTML = '<div class="loading">Loading dashboard...</div>';
    }
    
    try {
        // Load layout first, then fetch data immediately (not just start polling)
        await loadDashboardLayout();
        await fetchAndRenderDashboard();
        
        // Then start polling for updates
        PollingManager.startPolling('dashboard-api', fetchAndRenderDashboard, 2000);
    } catch (error) {
        console.error('[DASHBOARD] Init error:', error);
    }
}

// Track user interactions to prevent refresh interruptions
function markDashboardInteracting(isInteracting) {
    dashboardUserInteracting = isInteracting;
}

// Dashboard Data Fetching
async function fetchAndRenderDashboard() {
    const ENDPOINT = '/api/dashboard';
    
    try {
        const response = await fetch(ENDPOINT);
        if (!response.ok) throw new Error('Failed to fetch dashboard data');
        
        dashboardData = await response.json();
        PollingManager.recordSuccess(ENDPOINT, dashboardData);
        
        renderDashboard();
        renderAlarmSummary();
        
    } catch (error) {
        console.error('[DASHBOARD] Fetch error:', error);
        PollingManager.recordError(ENDPOINT);
        
        if (PollingManager.shouldShowError(ENDPOINT)) {
            const container = document.getElementById('dashboard-tiles');
            if (container) {
                container.innerHTML = '<div class="error-message">Failed to load dashboard data. Retrying...</div>';
            }
        } else {
            const cached = PollingManager.getCachedData(ENDPOINT);
            if (cached) {
                dashboardData = cached;
                renderDashboard();
                renderAlarmSummary();
            }
        }
    } // Added closing bracket here
}

async function loadDashboardLayout() {
    try {
        const response = await fetch('/api/dashboard/layout');
        if (response.ok) {
            const data = await response.json();
            dashboardLayout = data.tiles || [];
            layoutModified = false;
        }
    } catch (error) {
        console.error('[DASHBOARD] Failed to load layout:', error);
        dashboardLayout = [];
    }
}

function renderDashboard() {
    const container = document.getElementById('dashboard-tiles');
    if (!container || !dashboardData) return;
    
    // Skip full re-render if user is interacting (dragging or editing)
    if (dashboardDragging || dashboardUserInteracting) {
        // Only update non-interactive values
        updateDashboardValues();
        return;
    }
    
    const objects = dashboardData.objects || [];
    
    if (objects.length === 0) {
        container.innerHTML = `
            <div class="dashboard-empty">
                <p>No objects configured for dashboard display.</p>
                <p class="hint">Enable "Show on Dashboard" in each object's configuration.</p>
            </div>
        `;
        return;
    }
    
    // Check if any input is focused - don't re-render
    const activeEl = document.activeElement;
    if (activeEl && activeEl.closest('.dashboard-tile') && activeEl.tagName === 'INPUT') {
        updateDashboardValues();
        return;
    }
    
    // Sort objects by layout if available
    let sortedObjects = [...objects];
    if (dashboardLayout.length > 0) {
        sortedObjects.sort((a, b) => {
            const aPos = dashboardLayout.findIndex(t => t.type === a.type && t.index === a.index);
            const bPos = dashboardLayout.findIndex(t => t.type === b.type && t.index === b.index);
            if (aPos === -1 && bPos === -1) return 0;
            if (aPos === -1) return 1;
            if (bPos === -1) return -1;
            return aPos - bPos;
        });
    }
    
    container.innerHTML = sortedObjects.map(obj => createDashboardTile(obj)).join('');
    
    // Make tiles draggable
    container.querySelectorAll('.dashboard-tile').forEach(tile => {
        tile.draggable = true;
    });
    initDragAndDrop();
    
    updateAlarmBadge();
}

// Update only the values without full re-render (preserves user interactions)
function updateDashboardValues() {
    if (!dashboardData) return;
    
    const objects = dashboardData.objects || [];
    objects.forEach(obj => {
        const tile = document.querySelector(`.dashboard-tile[data-type="${obj.type}"][data-index="${obj.index}"]`);
        if (!tile) return;
        
        // Update status class
        tile.className = tile.className.replace(/tile-(normal|running|fault|offline)/g, '').trim();
        tile.classList.add(getDashboardStatusClass(obj));
        
        // Update values based on type - skip input fields
        const isController = ['temp_controller', 'ph_controller', 'flow_controller', 'do_controller'].includes(obj.type);
        
        if (isController) {
            // Update controller-specific values (but not input fields)
            const pvEl = tile.querySelector('.ctrl-pv-value');
            const spEl = tile.querySelector('.ctrl-sp-value');
            const dosedEl = tile.querySelector('.ctrl-dosed-value');
            const rpmEl = tile.querySelector('.ctrl-rpm-value');
            const statusIndicator = tile.querySelector('.ctrl-status-indicator');
            
            if (pvEl) pvEl.textContent = formatDashboardValue(obj.processValue !== undefined ? obj.processValue : obj.value);
            if (spEl) spEl.textContent = formatDashboardValue(obj.setpoint);
            if (dosedEl) dosedEl.textContent = formatDashboardValue(obj.cumulativeVolume);
            if (rpmEl) rpmEl.textContent = formatDashboardValue(obj.stirrerRpm);
            
            if (statusIndicator) {
                statusIndicator.className = 'ctrl-status-indicator ' + (obj.enabled ? 'enabled' : 'disabled');
                statusIndicator.innerHTML = obj.enabled ? DASHBOARD_ICONS.run : DASHBOARD_ICONS.stopped;
            }
            
            // Update sparkline (skip flow controllers - no feedback)
            if (obj.type !== 'flow_controller') {
                updateSparklineData(obj);
                const sparklineEl = tile.querySelector('.ctrl-sparkline');
                if (sparklineEl) {
                    sparklineEl.innerHTML = renderSparkline(obj.type, obj.index, 180, 100);
                }
            }
            
            // Update enable/disable button states - use 'running' or 'enabled'
            const enableBtn = tile.querySelector('.ctrl-enable-btn');
            const disableBtn = tile.querySelector('.ctrl-disable-btn');
            if (enableBtn) enableBtn.disabled = obj.enabled;
            if (disableBtn) disableBtn.disabled = !obj.enabled;
        } else {
            // Update simple value display
            const valueEl = tile.querySelector('.tile-value');
            if (valueEl && obj.online) {
                valueEl.textContent = formatDashboardValue(obj.value);
            }
        }
        
        // Update footer badge for non-controllers
        if (!isController) {
            const footer = tile.querySelector('.tile-footer');
            if (footer) {
                if (obj.type === 'stepper' || obj.type === 'dc_motor') {
                    footer.innerHTML = getMotorStatusBadge(obj);
                } else {
                    footer.innerHTML = getDashboardStatusBadge(obj);
                }
            }
        }
    });
}

function createDashboardTile(obj) {
    const icon = getDashboardObjectTypeIcon(obj.type);
    const statusClass = getDashboardStatusClass(obj);
    
    // Wide tiles for controllers only
    const isController = ['temp_controller', 'ph_controller', 'flow_controller', 'do_controller'].includes(obj.type);
    const sizeClass = isController ? 'tile-wide' : '';
    
    let valueDisplay = '';
    let statusBadge = '';
    
    if (obj.online) {
        if (obj.type === 'energy') {
            valueDisplay = formatDashboardEnergyDisplay(obj);
        } else if (obj.type === 'stepper' || obj.type === 'dc_motor') {
            valueDisplay = formatDashboardMotorDisplay(obj);
            // For motors, combine direction with running status
            statusBadge = getMotorStatusBadge(obj);
        } else if (isController) {
            valueDisplay = formatDashboardControllerDisplay(obj);
            statusBadge = getControllerStatusBadge(obj);
        } else {
            valueDisplay = `<span class="tile-value">${formatDashboardValue(obj.value)}</span>
                           <span class="tile-unit">${obj.unit || ''}</span>`;
        }
    } else {
        valueDisplay = '<span class="tile-offline">OFFLINE</span>';
        statusBadge = `<span class="tile-badge tile-badge-offline">${DASHBOARD_ICONS.offline} OFFLINE</span>`;
    }
    
    // For non-motor/controller, use standard status badge
    if (!statusBadge && obj.online) {
        statusBadge = getDashboardStatusBadge(obj);
    }
    
    // For controllers, add status indicator at top right instead of footer badge
    const statusIndicator = isController ? 
        `<div class="ctrl-status-indicator ${obj.enabled ? 'enabled' : 'disabled'}" title="${obj.enabled ? 'Running' : 'Stopped'}">
            ${obj.enabled ? DASHBOARD_ICONS.run : DASHBOARD_ICONS.stopped}
        </div>` : '';
    
    return `
        <div class="dashboard-tile ${statusClass} ${sizeClass}" data-type="${obj.type}" data-index="${obj.index}">
            <div class="tile-drag-handle" title="Drag to reorder">
                ${DASHBOARD_ICONS.drag}
            </div>
            ${statusIndicator}
            <div class="tile-header">
                <span class="tile-icon">${icon}</span>
                <span class="tile-name">${obj.name}</span>
            </div>
            <div class="tile-content">
                ${valueDisplay}
            </div>
            ${!isController ? `<div class="tile-footer">${statusBadge}</div>` : ''}
        </div>
    `;
}

function getMotorStatusBadge(obj) {
    if (!obj.online) return `<span class="tile-badge tile-badge-offline">${DASHBOARD_ICONS.offline} OFFLINE</span>`;
    if (obj.fault) return `<span class="tile-badge tile-badge-fault">${DASHBOARD_ICONS.fault} FAULT</span>`;
    if (obj.running) {
        const dirIcon = obj.direction ? DASHBOARD_ICONS.forward : DASHBOARD_ICONS.reverse;
        const dirText = obj.direction ? 'FWD' : 'REV';
        return `<span class="tile-badge tile-badge-running">${dirIcon} ${dirText}</span>`;
    }
    return '';
}

function getControllerStatusBadge(obj) {
    if (!obj.online) return `<span class="tile-badge tile-badge-offline">${DASHBOARD_ICONS.offline} OFFLINE</span>`;
    if (obj.fault) return `<span class="tile-badge tile-badge-fault">${DASHBOARD_ICONS.fault} FAULT</span>`;
    if (obj.enabled) return `<span class="tile-badge tile-badge-running">${DASHBOARD_ICONS.run} ENABLED</span>`;
    return '';
}

function getDashboardObjectTypeIcon(type) {
    switch (type) {
        case 'adc':
        case 'gpio':
            return DASHBOARD_ICONS.input;
        case 'rtd':
        case 'device_sensor':
            return DASHBOARD_ICONS.sensor;
        case 'dac':
        case 'digital_output':
            return DASHBOARD_ICONS.output;
        case 'stepper':
        case 'dc_motor':
            return DASHBOARD_ICONS.motor;
        case 'temp_controller':
        case 'ph_controller':
        case 'flow_controller':
        case 'do_controller':
            return DASHBOARD_ICONS.controller;
        case 'energy':
            return DASHBOARD_ICONS.energy;
        default:
            return DASHBOARD_ICONS.sensor;
    }
}

function getDashboardStatusClass(obj) {
    if (!obj.online) return 'tile-offline';
    if (obj.fault) return 'tile-fault';
    if (obj.running || obj.enabled) return 'tile-running';
    return 'tile-normal';
}

function getDashboardStatusBadge(obj) {
    if (!obj.online) {
        return `<span class="tile-badge tile-badge-offline">${DASHBOARD_ICONS.offline} OFFLINE</span>`;
    }
    if (obj.fault) {
        return `<span class="tile-badge tile-badge-fault">${DASHBOARD_ICONS.fault} FAULT</span>`;
    }
    if (obj.running || obj.enabled) {
        return `<span class="tile-badge tile-badge-running">${DASHBOARD_ICONS.ok} RUNNING</span>`;
    }
    return '';
}

// Dashboard Value Formatting
function formatDashboardValue(value) {
    if (value === null || value === undefined || isNaN(value)) return '--';
    return Number(value).toFixed(2);
}

function formatDashboardEnergyDisplay(obj) {
    const voltage = formatDashboardValue(obj.value);
    let current = '--';
    let power = '--';
    
    if (obj.additionalValues && obj.additionalValues.length >= 2) {
        current = formatDashboardValue(obj.additionalValues[0].value);
        power = formatDashboardValue(obj.additionalValues[1].value);
    }
    
    return `
        <div class="energy-display">
            <div class="energy-row"><span class="label">Voltage:</span> <span class="value">${voltage}</span> <span class="unit">V</span></div>
            <div class="energy-row"><span class="label">Current:</span> <span class="value">${current}</span> <span class="unit">A</span></div>
            <div class="energy-row"><span class="label">Power:</span> <span class="value">${power}</span> <span class="unit">W</span></div>
        </div>
    `;
}

function formatDashboardMotorDisplay(obj) {
    const valueLabel = obj.type === 'stepper' ? 'RPM' : 'Power';
    const valueUnit = obj.type === 'stepper' ? 'RPM' : '%';
    
    return `
        <span class="tile-value">${formatDashboardValue(obj.value)}</span>
        <span class="tile-unit">${valueUnit}</span>
    `;
}

function formatDashboardControllerDisplay(obj) {
    const pv = formatDashboardValue(obj.processValue !== undefined ? obj.processValue : obj.value);
    const sp = obj.setpoint !== undefined ? obj.setpoint : 0;
    const spDisplay = formatDashboardValue(sp);
    const unit = obj.unit || '';
    const type = obj.type;
    const index = obj.index;
    const inputId = `sp-input-${type}-${index}`;
    
    // Update sparkline data
    updateSparklineData(obj);

    // Control rows: SP input + Update on one line, Enable/Disable on separate line at bottom
    const controlRows = (unitOverride) => `
        <div class="ctrl-setpoint-row">
            <span class="label">Setpoint:</span>
            <input type="number" id="${inputId}" value="${sp}" step="0.1" 
                   onfocus="markDashboardInteracting(true)" onblur="markDashboardInteracting(false)" />
            <span class="unit">${unitOverride || unit}</span>
            <button class="ctrl-setpoint-btn" onclick="submitControllerSetpoint('${type}', ${index}, '${inputId}')">Update</button>
        </div>
        <div class="ctrl-action-row">
            <button class="ctrl-enable-btn" onclick="dashboardEnableController('${type}', ${index})" ${obj.enabled ? 'disabled' : ''}>Enable</button>
            <button class="ctrl-disable-btn" onclick="dashboardDisableController('${type}', ${index})" ${!obj.enabled ? 'disabled' : ''}>Disable</button>
        </div>
    `;
    
    // Sparkline container - only for controllers with feedback (not flow controllers)
    const hasSparkline = type !== 'flow_controller';
    const sparklineHtml = hasSparkline ? 
        `<div class="ctrl-sparkline" data-sparkline="${type}-${index}">${renderSparkline(type, index, 180, 100)}</div>` : '';

    // Build display based on controller type
    if (type === 'temp_controller') {
        return `
            <div class="controller-display">
                <div class="ctrl-main-row">
                    <div class="ctrl-data-rows">
                        <div class="ctrl-row"><span class="label">Value:</span> <span class="value ctrl-pv-value">${pv}</span> <span class="unit">${unit}</span></div>
                        <div class="ctrl-row"><span class="label">SP:</span> <span class="value ctrl-sp-value">${spDisplay}</span> <span class="unit">${unit}</span></div>
                    </div>
                    ${sparklineHtml}
                </div>
                <div class="ctrl-controls">
                    ${controlRows()}
                </div>
            </div>
        `;
    } else if (type === 'ph_controller') {
        const dosedAcid = formatDashboardValue(obj.cumulativeAcidVolume);
        const dosedBase = formatDashboardValue(obj.cumulativeBaseVolume);
        return `
            <div class="controller-display">
                <div class="ctrl-main-row">
                    <div class="ctrl-data-rows">
                        <div class="ctrl-row"><span class="label">Value:</span> <span class="value ctrl-pv-value">${pv}</span> <span class="unit">${unit}</span></div>
                        <div class="ctrl-row"><span class="label">SP:</span> <span class="value ctrl-sp-value">${spDisplay}</span> <span class="unit">${unit}</span></div>
                        <div class="ctrl-row"><span class="label">Acid:</span> <span class="value ctrl-dosed-value">${dosedAcid}</span> <span class="unit">mL</span></div>
                        <div class="ctrl-row"><span class="label">Base:</span> <span class="value ctrl-dosed-value">${dosedBase}</span> <span class="unit">mL</span></div>
                    </div>
                    ${sparklineHtml}
                </div>
                <div class="ctrl-controls">
                    ${controlRows()}
                </div>
            </div>
        `;
    } else if (type === 'flow_controller') {
        const dosed = formatDashboardValue(obj.cumulativeVolume);
        return `
            <div class="controller-display">
                <div class="ctrl-main-row">
                    <div class="ctrl-data-rows">
                        <div class="ctrl-row"><span class="label">Flow:</span> <span class="value ctrl-pv-value">${pv}</span> <span class="unit">${unit}</span></div>
                        <div class="ctrl-row"><span class="label">SP:</span> <span class="value ctrl-sp-value">${spDisplay}</span> <span class="unit">mL/min</span></div>
                        <div class="ctrl-row"><span class="label">Dosed:</span> <span class="value ctrl-dosed-value">${dosed}</span> <span class="unit">mL</span></div>
                    </div>
                </div>
                <div class="ctrl-controls">
                    ${controlRows('mL/min')}
                </div>
            </div>
        `;
    } else if (type === 'do_controller') {
        const stirrerRpm = formatDashboardValue(obj.stirrerRpm);
        return `
            <div class="controller-display">
                <div class="ctrl-main-row">
                    <div class="ctrl-data-rows">
                        <div class="ctrl-row"><span class="label">DO:</span> <span class="value ctrl-pv-value">${pv}</span> <span class="unit">${unit}</span></div>
                        <div class="ctrl-row"><span class="label">SP:</span> <span class="value ctrl-sp-value">${spDisplay}</span> <span class="unit">${unit}</span></div>
                        <div class="ctrl-row"><span class="label">Stirrer:</span> <span class="value ctrl-rpm-value">${stirrerRpm}</span> <span class="unit">RPM</span></div>
                    </div>
                    ${sparklineHtml}
                </div>
                <div class="ctrl-controls">
                    ${controlRows()}
                </div>
            </div>
        `;
    }
    
    // Fallback
    return `
        <div class="controller-display">
            <div class="ctrl-main-row">
                <div class="ctrl-data-rows">
                    <div class="ctrl-row"><span class="label">PV:</span> <span class="value ctrl-pv-value">${pv}</span> <span class="unit">${unit}</span></div>
                    <div class="ctrl-row"><span class="label">SP:</span> <span class="value ctrl-sp-value">${spDisplay}</span> <span class="unit">${unit}</span></div>
                </div>
            </div>
            <div class="ctrl-controls">
                ${controlRows()}
            </div>
        </div>
    `;
}

// Controller Setpoint Submit from input field
function submitControllerSetpoint(type, index, inputId) {
    const input = document.getElementById(inputId);
    if (!input) {
        showToast('error', 'Error', 'Could not find setpoint input');
        return;
    }
    
    const newValue = parseFloat(input.value);
    if (isNaN(newValue)) {
        showToast('error', 'Invalid Input', 'Please enter a valid number');
        return;
    }
    
    updateControllerSetpoint(type, index, newValue);
}

async function updateControllerSetpoint(type, index, setpoint) {
    // Map controller type to API endpoint (using object indices)
    // Temp controllers: 40-42, pH: 43, Flow: 44-47, DO: 48
    let endpoint = '';
    let body = {};
    
    if (type === 'temp_controller') {
        endpoint = `/api/controller/${index}/setpoint`;
        body = { setpoint: setpoint };
    } else if (type === 'ph_controller') {
        endpoint = '/api/phcontroller/43/setpoint';
        body = { setpoint: setpoint };
    } else if (type === 'flow_controller') {
        endpoint = `/api/flowcontroller/${index}/flowrate`;
        body = { flowRate: setpoint };
    } else if (type === 'do_controller') {
        endpoint = '/api/docontroller/48/setpoint';
        body = { setpoint: setpoint };
    } else {
        showToast('error', 'Error', 'Unknown controller type');
        return;
    }
    
    try {
        const response = await fetch(endpoint, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(body)
        });
        
        if (response.ok) {
            showToast('success', 'Setpoint Updated', `New setpoint: ${setpoint}`);
        } else {
            throw new Error('Failed to update setpoint');
        }
    } catch (error) {
        console.error('[DASHBOARD] Setpoint update error:', error);
        showToast('error', 'Error', 'Failed to update setpoint');
    }
}

// Alarm Summary
function renderAlarmSummary() {
    const container = document.getElementById('alarm-summary');
    if (!container || !dashboardData) return;
    
    const faultCount = dashboardData.faultCount || 0;
    const alarms = dashboardData.alarms || [];
    
    if (faultCount === 0) {
        container.innerHTML = `
            <div class="alarm-summary alarm-ok">
                <span class="alarm-icon">${DASHBOARD_ICONS.ok}</span>
                <span class="alarm-text">All systems normal</span>
            </div>
        `;
        return;
    }
    
    container.innerHTML = `
        <div class="alarm-summary alarm-fault">
            <div class="alarm-header">
                <span class="alarm-icon">${DASHBOARD_ICONS.fault}</span>
                <span class="alarm-count">${faultCount} Active Alarm${faultCount > 1 ? 's' : ''}</span>
            </div>
            <ul class="alarm-list">
                ${alarms.map(a => `<li><strong>${a.name}</strong>: ${a.message || 'Fault detected'}</li>`).join('')}
            </ul>
        </div>
    `;
}

function updateAlarmBadge() {
    const badge = document.getElementById('alarm-badge');
    if (!badge || !dashboardData) return;
    
    const faultCount = dashboardData.faultCount || 0;
    if (faultCount > 0) {
        badge.textContent = faultCount;
        badge.classList.add('visible');
    } else {
        badge.classList.remove('visible');
    }
}

// Dashboard Global Controls - Run/Pause/Stop State Machine
async function runAllControllers() {
    if (runState === 'running') return;
    
    if (!confirm('Start all controllers?')) return;
    
    try {
        const response = await fetch('/api/dashboard/enable-all', { method: 'POST' });
        if (response.ok) {
            const data = await response.json();
            
            // Set run state
            runState = 'running';
            runStartTime = data.startTime || new Date().toLocaleTimeString();
            runStartEpoch = Date.now();
            runAccumulatedMs = 0;
            
            // Start the timer
            startRunTimer();
            updateRunControlButtons();
            updateTimerDisplay();
            
            showToast('success', 'Run Started', 'All controllers enabled');
        } else {
            throw new Error('Failed to enable all');
        }
    } catch (error) {
        console.error('[DASHBOARD] Run all error:', error);
        showToast('error', 'Error', 'Failed to start controllers');
    }
}

async function pauseControllers() {
    if (runState !== 'running') return;
    
    try {
        const response = await fetch('/api/dashboard/pause', { method: 'POST' });
        if (response.ok) {
            // Pause the timer - save accumulated time
            runAccumulatedMs += Date.now() - runStartEpoch;
            runState = 'paused';
            
            stopRunTimer();
            updateRunControlButtons();
            updateTimerDisplay();
            
            showToast('warning', 'Paused', 'pH, flow, and DO controllers paused (temperature still active)');
        } else {
            throw new Error('Failed to pause');
        }
    } catch (error) {
        console.error('[DASHBOARD] Pause error:', error);
        showToast('error', 'Error', 'Failed to pause controllers');
    }
}

async function resumeControllers() {
    if (runState !== 'paused') return;
    
    try {
        const response = await fetch('/api/dashboard/enable-all', { method: 'POST' });
        if (response.ok) {
            // Resume the timer from where we left off
            runState = 'running';
            runStartEpoch = Date.now();
            
            startRunTimer();
            updateRunControlButtons();
            updateTimerDisplay();
            
            showToast('success', 'Resumed', 'All controllers re-enabled');
        } else {
            throw new Error('Failed to resume');
        }
    } catch (error) {
        console.error('[DASHBOARD] Resume error:', error);
        showToast('error', 'Error', 'Failed to resume controllers');
    }
}

function togglePauseResume() {
    if (runState === 'running') {
        pauseControllers();
    } else if (runState === 'paused') {
        resumeControllers();
    }
}

async function stopAllControllers() {
    if (runState === 'idle') return;
    
    if (!confirm('Stop all controllers?')) return;
    
    try {
        const response = await fetch('/api/dashboard/disable-all', { method: 'POST' });
        if (response.ok) {
            // Stop the timer but keep display values
            if (runState === 'running') {
                runAccumulatedMs += Date.now() - runStartEpoch;
            }
            runState = 'stopped';
            
            stopRunTimer();
            updateRunControlButtons();
            updateTimerDisplay();
            
            showToast('info', 'Stopped', 'All controllers disabled');
        } else {
            throw new Error('Failed to stop all');
        }
    } catch (error) {
        console.error('[DASHBOARD] Stop all error:', error);
        showToast('error', 'Error', 'Failed to stop controllers');
    }
}

function updateRunControlButtons() {
    const runBtn = document.getElementById('run-all-btn');
    const pauseBtn = document.getElementById('pause-btn');
    const stopBtn = document.getElementById('stop-all-btn');
    
    if (runBtn) runBtn.disabled = runState === 'running';
    if (pauseBtn) {
        pauseBtn.disabled = runState !== 'running' && runState !== 'paused';
        // Toggle button appearance between Pause and Resume
        if (runState === 'paused') {
            pauseBtn.innerHTML = `<svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M8,5.14V19.14L19,12.14L8,5.14Z"/></svg> Resume`;
            pauseBtn.classList.remove('dashboard-btn-warning');
            pauseBtn.classList.add('dashboard-btn-success');
        } else {
            pauseBtn.innerHTML = `<svg viewBox="0 0 24 24" width="16" height="16"><path fill="currentColor" d="M14,19H18V5H14M6,19H10V5H6V19Z"/></svg> Pause`;
            pauseBtn.classList.remove('dashboard-btn-success');
            pauseBtn.classList.add('dashboard-btn-warning');
        }
    }
    if (stopBtn) stopBtn.disabled = runState === 'idle';
}

async function clearAllVolumes() {
    if (!confirm('Clear all cumulative dosing volumes?')) return;
    
    try {
        const response = await fetch('/api/dashboard/clear-volumes', { method: 'POST' });
        if (response.ok) {
            showToast('success', 'Clear Volumes', 'All cumulative volumes reset');
        } else {
            throw new Error('Failed to clear volumes');
        }
    } catch (error) {
        console.error('[DASHBOARD] Clear volumes error:', error);
        showToast('error', 'Error', 'Failed to clear volumes');
    }
}

// Run Timer Functions
function startRunTimer() {
    stopRunTimer(); // Clear any existing interval
    runTimerInterval = setInterval(updateTimerDisplay, 1000);
}

function stopRunTimer() {
    if (runTimerInterval) {
        clearInterval(runTimerInterval);
        runTimerInterval = null;
    }
}

function updateTimerDisplay() {
    const startTimeEl = document.getElementById('run-start-time');
    const durationEl = document.getElementById('run-duration');
    const timerDisplay = document.querySelector('.run-timer-display');
    
    if (!startTimeEl || !durationEl || !timerDisplay) return;
    
    // Update display class based on state
    timerDisplay.className = 'run-timer-display ' + runState;
    
    if (runState === 'idle') {
        startTimeEl.textContent = '--:--:--';
        durationEl.textContent = '00:00:00';
    } else {
        // Show start time (just time portion)
        if (runStartTime) {
            const timePart = runStartTime.includes(' ') ? runStartTime.split(' ')[1] : runStartTime;
            startTimeEl.textContent = timePart;
        }
        
        // Calculate duration
        let totalMs = runAccumulatedMs;
        if (runState === 'running') {
            totalMs += Date.now() - runStartEpoch;
        }
        
        durationEl.textContent = formatDuration(totalMs);
    }
}

function formatDuration(ms) {
    const totalSeconds = Math.floor(ms / 1000);
    const hours = Math.floor(totalSeconds / 3600);
    const minutes = Math.floor((totalSeconds % 3600) / 60);
    const seconds = totalSeconds % 60;
    
    return `${hours.toString().padStart(2, '0')}:${minutes.toString().padStart(2, '0')}:${seconds.toString().padStart(2, '0')}`;
}

// Legacy function names for backward compatibility
function enableAllControllers() { return runAllControllers(); }
function disableAllControllers() { return stopAllControllers(); }

// Dashboard Layout Management
function onTileOrderChanged() {
    const container = document.getElementById('dashboard-tiles');
    if (!container) return;
    
    const tiles = container.querySelectorAll('.dashboard-tile');
    dashboardLayout = Array.from(tiles).map(tile => ({
        type: tile.dataset.type,
        index: parseInt(tile.dataset.index)
    }));
    
    layoutModified = true;
    updateSaveLayoutButton();
}

function updateSaveLayoutButton() {
    const btn = document.getElementById('save-layout-btn');
    if (btn) {
        btn.disabled = !layoutModified;
        btn.classList.toggle('modified', layoutModified);
    }
}

async function saveDashboardLayout() {
    if (!layoutModified) return;
    
    try {
        const response = await fetch('/api/dashboard/layout', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ tiles: dashboardLayout })
        });
        
        if (response.ok) {
            layoutModified = false;
            updateSaveLayoutButton();
            showToast('success', 'Layout Saved', 'Dashboard layout saved to flash');
        } else {
            throw new Error('Failed to save layout');
        }
    } catch (error) {
        console.error('[DASHBOARD] Save layout error:', error);
        showToast('error', 'Error', 'Failed to save layout');
    }
}

// Dashboard Drag and Drop - optimized for grid layout
let dragState = { tile: null, dropTarget: null };

function initDragAndDrop() {
    const container = document.getElementById('dashboard-tiles');
    if (!container) return;
    
    const tiles = container.querySelectorAll('.dashboard-tile');
    
    tiles.forEach(tile => {
        tile.addEventListener('dragstart', handleDragStart);
        tile.addEventListener('dragend', handleDragEnd);
        tile.addEventListener('dragover', handleDragOver);
        tile.addEventListener('dragenter', handleDragEnter);
        tile.addEventListener('dragleave', handleDragLeave);
        tile.addEventListener('drop', handleDrop);
    });
}

function handleDragStart(e) {
    dashboardDragging = true;
    dragState.tile = this;
    this.classList.add('dragging');
    e.dataTransfer.effectAllowed = 'move';
    e.dataTransfer.setData('text/plain', '');
}

function handleDragEnd(e) {
    dashboardDragging = false;
    this.classList.remove('dragging');
    document.querySelectorAll('.drag-over').forEach(el => el.classList.remove('drag-over'));
    dragState.tile = null;
    dragState.dropTarget = null;
}

function handleDragOver(e) {
    e.preventDefault();
    e.dataTransfer.dropEffect = 'move';
}

function handleDragEnter(e) {
    e.preventDefault();
    if (this !== dragState.tile && !this.classList.contains('dragging')) {
        if (dragState.dropTarget && dragState.dropTarget !== this) {
            dragState.dropTarget.classList.remove('drag-over');
        }
        this.classList.add('drag-over');
        dragState.dropTarget = this;
    }
}

function handleDragLeave(e) {
    if (!this.contains(e.relatedTarget)) {
        this.classList.remove('drag-over');
    }
}

function handleDrop(e) {
    e.preventDefault();
    e.stopPropagation();
    
    this.classList.remove('drag-over');
    
    if (dragState.tile && dragState.tile !== this) {
        const container = document.getElementById('dashboard-tiles');
        const tiles = Array.from(container.querySelectorAll('.dashboard-tile'));
        const dragIdx = tiles.indexOf(dragState.tile);
        const dropIdx = tiles.indexOf(this);
        
        if (dragIdx !== -1 && dropIdx !== -1) {
            if (dragIdx < dropIdx) {
                container.insertBefore(dragState.tile, this.nextSibling);
            } else {
                container.insertBefore(dragState.tile, this);
            }
            onTileOrderChanged();
        }
    }
}

// Dashboard individual controller enable/disable
async function dashboardEnableController(type, index) {
    let endpoint = '';
    
    if (type === 'temp_controller') {
        endpoint = `/api/controller/${index}/enable`;
    } else if (type === 'ph_controller') {
        endpoint = '/api/phcontroller/43/enable';
    } else if (type === 'flow_controller') {
        endpoint = `/api/flowcontroller/${index}/enable`;
    } else if (type === 'do_controller') {
        endpoint = '/api/docontroller/48/enable';
    } else {
        showToast('error', 'Error', 'Unknown controller type');
        return;
    }
    
    try {
        const response = await fetch(endpoint, { method: 'POST' });
        if (response.ok) {
            showToast('success', 'Controller Enabled', 'Controller has been enabled');
        } else {
            throw new Error('Failed to enable controller');
        }
    } catch (error) {
        console.error('[DASHBOARD] Enable controller error:', error);
        showToast('error', 'Error', 'Failed to enable controller');
    }
}

async function dashboardDisableController(type, index) {
    let endpoint = '';
    
    if (type === 'temp_controller') {
        endpoint = `/api/controller/${index}/disable`;
    } else if (type === 'ph_controller') {
        endpoint = '/api/phcontroller/43/disable';
    } else if (type === 'flow_controller') {
        endpoint = `/api/flowcontroller/${index}/disable`;
    } else if (type === 'do_controller') {
        endpoint = '/api/docontroller/48/disable';
    } else {
        showToast('error', 'Error', 'Unknown controller type');
        return;
    }
    
    try {
        const response = await fetch(endpoint, { method: 'POST' });
        if (response.ok) {
            showToast('success', 'Controller Disabled', 'Controller has been disabled');
        } else {
            throw new Error('Failed to disable controller');
        }
    } catch (error) {
        console.error('[DASHBOARD] Disable controller error:', error);
        showToast('error', 'Error', 'Failed to disable controller');
    }
}

// Export dashboard functions to window
window.initDashboardTab = initDashboardTab;
window.runAllControllers = runAllControllers;
window.pauseControllers = pauseControllers;
window.resumeControllers = resumeControllers;
window.togglePauseResume = togglePauseResume;
window.stopAllControllers = stopAllControllers;
window.enableAllControllers = enableAllControllers;
window.disableAllControllers = disableAllControllers;
window.clearAllVolumes = clearAllVolumes;
window.saveDashboardLayout = saveDashboardLayout;
window.submitControllerSetpoint = submitControllerSetpoint;
window.dashboardEnableController = dashboardEnableController;
window.dashboardDisableController = dashboardDisableController;
window.markDashboardInteracting = markDashboardInteracting;