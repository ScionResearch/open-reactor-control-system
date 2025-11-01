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

// Tab switching function
function openTab(evt, tabName) {
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
    
    // Initialize specific page content if needed
    if (tabName === 'system') {
        updateSystemStatus();
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
        // Initialize devices tab with control object polling
        if (typeof initDevicesTab === 'function') {
            initDevicesTab();
        }
    } else if (tabName === 'sensors') {
        // Initialize sensors tab with continuous polling
        initSensorsTab();
    } else if (tabName === 'controllers') {
        // Initialize controllers tab with continuous polling
        if (typeof initControllersTab === 'function') {
            initControllersTab();
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
    
    // Function to check SD card status
    function checkSDCardStatus() {
        if (!fileManagerActive) return;
        
        fetch('/api/system/status')
            .then(response => response.json())
            .then(data => {
                if (!data.sd.inserted) {
                    // SD card is physically not inserted
                    sdStatusElement.innerHTML = '<div class="status-error">SD Card not inserted</div>';
                    fileListContainer.innerHTML = '<div class="error-message">SD Card is not inserted. Please insert an SD card to view files.</div>';
                    // Disable the file manager functionality when SD card is not available
                    pathNavigator.innerHTML = '';
                } else {
                    // SD card is physically present - show basic info regardless of ready status
                    if (data.sd.ready && data.sd.capacityGB) {
                        sdStatusElement.innerHTML = `<div class="status-good">SD Card Ready - ${data.sd.freeSpaceGB.toFixed(2)} GB free of ${data.sd.capacityGB.toFixed(2)} GB</div>`;
                    } else {
                        sdStatusElement.innerHTML = '<div class="status-info">SD Card inserted</div>';
                    }
                    
                    // Only reload directory contents if the file manager shows the "not inserted" error message
                    const errorMsg = fileListContainer.querySelector('.error-message');
                    if (errorMsg && errorMsg.textContent.includes('not inserted')) {
                        loadDirectory(currentPath);
                    }
                }
            })
            .catch(error => {
                console.error('Error checking SD status:', error);
                // Don't update the UI on fetch errors to avoid flickering
            });
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
        currentPath = path;
        loadDirectory(path);
    }
    
    // Function to display directory contents
    function displayDirectoryContents(data) {
        fileListContainer.innerHTML = '';
        
        // Constant for maximum file size (should match server MAX_DOWNLOAD_SIZE)
        const MAX_DOWNLOAD_SIZE = 5242880; // 5MB in bytes
        
        // Check if there are no files or directories
        if (data.directories.length === 0 && data.files.length === 0) {
            fileListContainer.innerHTML = '<div class="empty-message">This directory is empty</div>';
            return;
        }
        
        // Display directories first
        data.directories.forEach(dir => {
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
        
        // Then display files
        data.files.forEach(file => {
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
                    <button class="${downloadBtnClass}" data-path="${file.path}" title="${downloadBtnTitle}">Download</button>
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
                    const filePath = event.target.getAttribute('data-path');
                    downloadFile(filePath);
                });
            }
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
    
    // Override the initial checkSDCardStatus call to use the improved function
    checkSDCardStatus = () => {
        if (!fileManagerActive) return;
        
        fetch('/api/system/status')
            .then(response => response.json())
            .then(data => {
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
            });
    };
    
    // Initialize with our improved function
    checkAndLoadDirectory();
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

async function updateSensorData() {
    // Prevent multiple simultaneous calls
    if (sensorDataUpdating) {
        return;
    }
    
    sensorDataUpdating = true;
    
    try {
        const response = await fetch('/api/sensors');
        
        // Check if response is ok before trying to parse JSON
        if (!response.ok) {
            // If it's a 404, the API doesn't exist yet - this is expected
            if (response.status === 404) {
                // Show placeholder data or "API not available" message
                updateSensorPlaceholders();
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
        
        // Update each sensor value and calculate trends
        if (data.temp !== undefined) {
            const tempElement = document.getElementById('temp-reading');
            const tempControlElement = document.getElementById('current-temp');
            if (tempElement) {
                const oldTemp = parseFloat(tempElement.textContent) || 0;
                tempElement.textContent = data.temp.toFixed(1);
                
                // Update trend indicator
                updateTrend('temp-trend', data.temp, oldTemp);
                
                // Add to history
                sensorHistory.temp.push(data.temp);
                if (sensorHistory.temp.length > 50) sensorHistory.temp.shift();
            }
            if (tempControlElement) {
                tempControlElement.textContent = data.temp.toFixed(1) + ' °C';
            }
        }
        
        if (data.ph !== undefined) {
            const phElement = document.getElementById('ph-reading');
            const phControlElement = document.getElementById('current-ph');
            if (phElement) {
                const oldPh = parseFloat(phElement.textContent) || 0;
                phElement.textContent = data.ph.toFixed(2);
                
                // Update trend indicator
                updateTrend('ph-trend', data.ph, oldPh);
                
                // Add to history
                sensorHistory.ph.push(data.ph);
                if (sensorHistory.ph.length > 50) sensorHistory.ph.shift();
            }
            if (phControlElement) {
                phControlElement.textContent = data.ph.toFixed(2);
            }
        }
        
        if (data.do !== undefined) {
            const doElement = document.getElementById('do-reading');
            const doControlElement = document.getElementById('current-do');
            if (doElement) {
                const oldDo = parseFloat(doElement.textContent) || 0;
                doElement.textContent = data.do.toFixed(1);
                
                // Update trend indicator
                updateTrend('do-trend', data.do, oldDo);
                
                // Add to history
                sensorHistory.do.push(data.do);
                if (sensorHistory.do.length > 50) sensorHistory.do.shift();
            }
            if (doControlElement) {
                doControlElement.textContent = data.do.toFixed(1) + '%';
            }
        }
        
        if (data.stirrer !== undefined) {
            const stirrerElement = document.getElementById('stirrer-reading');
            const stirrerControlElement = document.getElementById('current-stirrer-rpm');
            if (stirrerElement) {
                stirrerElement.textContent = data.stirrer.toFixed(0);
                
                // Add to history
                sensorHistory.stirrer.push(data.stirrer);
                if (sensorHistory.stirrer.length > 50) sensorHistory.stirrer.shift();
            }
            if (stirrerControlElement) {
                stirrerControlElement.textContent = data.stirrer.toFixed(0) + ' RPM';
            }
        }
        
        // Update new sensor parameters
        if (data.gasFlow !== undefined) {
            const gasFlowElement = document.getElementById('current-gasflow');
            if (gasFlowElement) {
                gasFlowElement.textContent = data.gasFlow.toFixed(1) + ' mL/min';
            }
        }
        
        if (data.pressure !== undefined) {
            const pressureElement = document.getElementById('current-pressure');
            if (pressureElement) {
                pressureElement.textContent = data.pressure.toFixed(1) + ' kPa';
            }
        }
        
        if (data.weight !== undefined) {
            const weightElement = document.getElementById('current-weight');
            if (weightElement) {
                weightElement.textContent = data.weight.toFixed(1) + ' g';
            }
        }
        
        if (data.opticalDensity !== undefined) {
            const odElement = document.getElementById('current-od');
            if (odElement) {
                odElement.textContent = data.opticalDensity.toFixed(3) + ' OD';
            }
        }
        
        // Update power sensor readings
        if (data.powerVolts !== undefined) {
            const powerVoltsElement = document.getElementById('current-power-volts');
            if (powerVoltsElement) {
                powerVoltsElement.textContent = data.powerVolts.toFixed(1) + ' V';
            }
        }
        
        if (data.powerAmps !== undefined) {
            const powerAmpsElement = document.getElementById('current-power-amps');
            if (powerAmpsElement) {
                powerAmpsElement.textContent = data.powerAmps.toFixed(2) + ' A';
            }
        }
        
        if (data.powerWatts !== undefined) {
            const powerWattsElement = document.getElementById('current-power-watts');
            if (powerWattsElement) {
                powerWattsElement.textContent = data.powerWatts.toFixed(1) + ' W';
            }
        }
        
        // Update last update time
        const lastUpdateElement = document.getElementById('lastUpdate');
        if (lastUpdateElement) {
            lastUpdateElement.textContent = new Date().toLocaleString();
        }

        // Removed system-status update (Sensors Online/Offline)
        // const systemStatusElement = document.getElementById('system-status');
        // if (systemStatusElement) {
        //     systemStatusElement.innerHTML = '<span class="status ok">Sensors Online</span>';
        // }
        
    } catch (error) {
        console.error('Error updating sensor data:', error);
        updateSensorPlaceholders();
    } finally {
        sensorDataUpdating = false;
    }
}

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
        
        document.getElementById('mqttBroker').value = data.mqttBroker || '';
        document.getElementById('mqttPort').value = data.mqttPort || '1883';
        document.getElementById('mqttUsername').value = data.mqttUsername || '';
        document.getElementById('mqttPassword').value = data.mqttPassword || '';
    } catch (error) {
        console.error('Error loading MQTT settings:', error);
    }
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
    // Load settings sequentially to avoid overwhelming single-threaded server
    await loadInitialSettings();  // Load initial NTP and timezone settings
    await loadNetworkSettings();  // Load initial network settings
    await loadMqttSettings();     // Load initial MQTT settings
    loadControlSettings();  // Load initial control settings (no API call)
    updateLiveClock();
    updateSensorData();
    updateNetworkInfo();
    
    // Initialize system status if system tab is active initially
    const systemTab = document.querySelector('#system');
    if (systemTab && systemTab.classList.contains('active')) {
        updateSystemStatus();
    }
    
    // Set up event listeners
    const ntpCheckbox = document.getElementById('enableNTP');
    if (ntpCheckbox) {
        ntpCheckbox.addEventListener('change', updateInputStates);
    }
    
    // Set up save button handler
    const saveButton = document.getElementById('saveTimeBtn');
    if (saveButton) {
        saveButton.addEventListener('click', saveTimeSettings);
    }

    // Set up IP configuration mode handler
    const ipConfig = document.getElementById('ipConfig');
    if (ipConfig) {
        const updateStaticFields = () => {
            const staticFields = document.getElementById('staticSettings');
            if (staticFields) {
                staticFields.style.display = ipConfig.value === 'static' ? 'block' : 'none';
            }
        };
        ipConfig.addEventListener('change', updateStaticFields);
        // Set initial state
        updateStaticFields();
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

// Update intervals
setInterval(updateLiveClock, 1000);    // Update clock every second
setInterval(updateSensorData, 5000);   // Update sensor data every 5 seconds (reduced frequency)
setInterval(updateNetworkInfo, 10000); // Update network info every 10 seconds (reduced frequency)

// Missing functions that are referenced but not defined
function updateNetworkInfo() {
    // This function is called but not defined - adding a placeholder
    // In a real implementation, this would update network information
    console.log('Network info update called');
}

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
                    const sdSensorSizeContainer = document.getElementById('sdSensorSizeContainer');
                    
                    if (sdCapacityContainer) sdCapacityContainer.style.display = 'flex';
                    if (sdFreeSpaceContainer) sdFreeSpaceContainer.style.display = 'flex';
                    if (sdLogSizeContainer) sdLogSizeContainer.style.display = 'flex';
                    if (sdSensorSizeContainer) sdSensorSizeContainer.style.display = 'flex';
                    
                    // Update SD card details
                    const sdCapacity = document.getElementById('sdCapacity');
                    if (sdCapacity) sdCapacity.textContent = data.sd.capacityGB.toFixed(1) + ' GB';
                    
                    const sdFreeSpace = document.getElementById('sdFreeSpace');
                    if (sdFreeSpace) sdFreeSpace.textContent = data.sd.freeSpaceGB.toFixed(1) + ' GB';
                    
                    const sdLogSize = document.getElementById('sdLogSize');
                    if (sdLogSize) sdLogSize.textContent = data.sd.logFileSizeKB.toFixed(1) + ' kB';
                    
                    const sdSensorSize = document.getElementById('sdSensorSize');
                    if (sdSensorSize) sdSensorSize.textContent = data.sd.sensorFileSizeKB.toFixed(1) + ' kB';
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

// Update system status when the system tab is active
function updateStatusIfSystemTabActive() {
    if (document.querySelector('#system').classList.contains('active')) {
        updateSystemStatus();
    }
}

// Add system status update to the periodic updates
setInterval(updateStatusIfSystemTabActive, 5000); // Reduced frequency to every 5 seconds

// When switching to the system tab, update the status immediately
document.querySelectorAll('nav a[data-page="system"]').forEach(link => {
    link.addEventListener('click', () => {
        updateSystemStatus();
    });
});

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

// Add MQTT form submission handler
document.getElementById('mqttForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    
    const mqttData = {
        mqttBroker: document.getElementById('mqttBroker').value,
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
            throw new Error('Failed to save MQTT settings');
        }
    } catch (error) {
        console.error('Error saving MQTT settings:', error);
        
        // Remove loading toast if it still exists
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }
        
        showToast('error', 'Error', 'Failed to save MQTT settings');
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

let inputsRefreshInterval = null;
let sensorsRefreshInterval = null;

function initInputsTab() {
    // Stop any existing interval
    if (inputsRefreshInterval) {
        clearInterval(inputsRefreshInterval);
    }
    
    // Load inputs immediately
    fetchAndRenderInputs();
    
    // Start polling every 2 seconds while tab is active
    inputsRefreshInterval = setInterval(() => {
        if (document.getElementById('inputs').classList.contains('active')) {
            fetchAndRenderInputs();
        } else {
            // Stop polling if tab is not active
            clearInterval(inputsRefreshInterval);
            inputsRefreshInterval = null;
        }
    }, 2000);
}

function initSensorsTab() {
    // Stop any existing interval
    if (sensorsRefreshInterval) {
        clearInterval(sensorsRefreshInterval);
    }
    
    // Load sensors immediately
    fetchAndRenderInputs();
    
    // Start polling every 2 seconds while tab is active
    sensorsRefreshInterval = setInterval(() => {
        if (document.getElementById('sensors').classList.contains('active')) {
            fetchAndRenderInputs();
        } else {
            // Stop polling if tab is not active
            clearInterval(sensorsRefreshInterval);
            sensorsRefreshInterval = null;
        }
    }, 2000);
}

async function fetchAndRenderInputs() {
    try {
        const response = await fetch('/api/inputs');
        if (!response.ok) throw new Error('Failed to fetch inputs');
        
        const data = await response.json();
        console.log('Inputs data received:', data);
        console.log('  ADC:', data.adc ? data.adc.length : 0, 'entries');
        console.log('  RTD:', data.rtd ? data.rtd.length : 0, 'entries');
        console.log('  GPIO:', data.gpio ? data.gpio.length : 0, 'entries');
        console.log('  Energy:', data.energy ? data.energy.length : 0, 'entries');
        console.log('  Devices:', data.devices ? data.devices.length : 0, 'entries');
        
        // Render each section
        renderADCInputs(data.adc || []);
        renderRTDInputs(data.rtd || []);
        renderGPIOInputs(data.gpio || []);
        renderEnergySensors(data.energy || []);
        renderDeviceSensors(data.devices || []);
        
    } catch (error) {
        console.error('Error fetching inputs:', error);
        showInputError('adc-list');
        showInputError('rtd-list');
        showInputError('gpio-list');
        showInputError('energy-list');
        showInputError('device-sensors-list');
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
                        <span class="value-large">${sensor.v.toFixed(2)}</span>
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
        const response = await fetch(`/api/dac/${index}/config`);
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
        const response = await fetch(`/api/dac/${currentDACIndex}/config`, {
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
        document.getElementById('rtdConfigUnit').value = rtdConfigData.unit || 'C';
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
    document.getElementById('rtdConfigUnit').value = 'C';
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
    const currentUnit = rtdConfigData.unit || 'C';
    
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

let outputsRefreshInterval = null;
let activeControls = new Set(); // Track controls being manipulated
let lastOutputModes = new Map(); // Track output modes to detect changes
let pendingCommands = new Map(); // Track pending commands {index: setValue}
let stepperInputFocused = false; // Track if stepper RPM input has focus
let dcMotorSlidersFocused = new Map(); // Track which DC motor sliders have focus {index: boolean}
let dacSlidersFocused = new Map(); // Track which DAC sliders have focus {index: boolean}

function initOutputsTab() {
    // Stop any existing interval
    if (outputsRefreshInterval) {
        clearInterval(outputsRefreshInterval);
    }
    
    // Load outputs immediately
    fetchAndRenderOutputs();
    
    // Start polling every 2 seconds while tab is active
    outputsRefreshInterval = setInterval(() => {
        if (document.getElementById('outputs').classList.contains('active')) {
            fetchAndRenderOutputs();
        } else {
            // Stop polling if tab is not active
            clearInterval(outputsRefreshInterval);
            outputsRefreshInterval = null;
        }
    }, 2000);
}

async function fetchAndRenderOutputs() {
    try {
        const response = await fetch('/api/outputs');
        if (!response.ok) throw new Error('Failed to fetch outputs');
        
        const data = await response.json();
        console.log('Outputs data received:', data);
        
        // Render each section
        renderDACOutputs(data.dacOutputs || []);
        renderDigitalOutputs(data.digitalOutputs || []);
        renderStepperMotor(data.stepperMotor);
        renderDCMotors(data.dcMotors || []);
        
    } catch (error) {
        console.error('Error fetching outputs:', error);
        showOutputError('dac-outputs-list');
        showOutputError('digital-outputs-list');
        showOutputError('stepper-motor-list');
        showOutputError('dc-motors-list');
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
                const actualValue = output.value || 0;
                
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
        showOnDashboard: document.getElementById('stepperShowOnDashboard').checked
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
    
    if (configData.maxRPM < 1 || configData.maxRPM > 900) {
        showToast('error', 'Validation Error', 'Max RPM must be 1-900');
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

let comportsRefreshInterval = null;

function initComPortsTab() {
    // Stop any existing interval
    if (comportsRefreshInterval) {
        clearInterval(comportsRefreshInterval);
    }
    
    // Load COM ports immediately
    fetchAndRenderComPorts();
    
    // Start polling every 2 seconds while tab is active
    comportsRefreshInterval = setInterval(() => {
        if (document.getElementById('comports').classList.contains('active')) {
            fetchAndRenderComPorts();
        } else {
            // Stop polling if tab is not active
            clearInterval(comportsRefreshInterval);
            comportsRefreshInterval = null;
        }
    }, 2000);
}

async function fetchAndRenderComPorts() {
    try {
        const response = await fetch('/api/comports');
        if (!response.ok) throw new Error('Failed to fetch COM ports');
        
        const data = await response.json();
        console.log('COM ports data received:', data);
        
        // Render COM ports
        renderComPorts(data.ports || []);
        
    } catch (error) {
        console.error('Error fetching COM ports:', error);
        showComPortError();
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
    3: { name: 'Alicat Mass Flow Controller', interface: 0, description: 'Alicat Modbus MFC', hasSetpoint: true },
    
    // Analogue IO drivers
    10: { name: 'Pressure Controller', interface: 1, description: '0-10V analogue pressure controller', hasSetpoint: true }
};

const INTERFACE_NAMES = {
    0: 'Modbus RTU',
    1: 'Analogue I/O'
};

let currentDeviceIndex = -1; // For editing devices
let deviceControlData = {}; // Cache for device control objects
let devicesPollingInterval = null;

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
            const isConnected = device.connected !== false;  // Default to true if not specified
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
        
        if (message && device.message) {
            message.textContent = device.message;
            message.style.display = device.message ? 'block' : 'none';
        }
    });
}

function startDevicesPolling() {
    // Poll every 2 seconds
    if (devicesPollingInterval) {
        clearInterval(devicesPollingInterval);
    }
    
    fetchDeviceControlData();  // Initial fetch
    devicesPollingInterval = setInterval(fetchDeviceControlData, 2000);
}

function stopDevicesPolling() {
    if (devicesPollingInterval) {
        clearInterval(devicesPollingInterval);
        devicesPollingInterval = null;
    }
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
                    <button class="output-btn output-btn-secondary" 
                            onclick="resetFault(${controlIndex})" 
                            title="Reset Fault">
                        Reset Fault
                    </button>
                </div>
            ` : ''}
            
            <div class="device-message" style="display: none;"></div>
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

async function resetFault(controlIndex) {
    console.log(`[DEVICE] Resetting fault for control index ${controlIndex}`);
    
    try {
        const response = await fetch(`/api/device/${controlIndex}/fault/reset`, {
            method: 'POST'
        });
        
        if (!response.ok) {
            const error = await response.json();
            throw new Error(error.error || 'Failed to reset fault');
        }
        
        const result = await response.json();
        showToast('success', 'Fault Reset', 'Device fault reset command sent');
        console.log('[DEVICE] Fault reset command sent successfully');
        
        // Refresh data immediately
        fetchDeviceControlData();
    } catch (error) {
        console.error('Error resetting fault:', error);
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
    startDevicesPolling();
    console.log('[DEVICES] Polling started');
}

// Export for use in main script.js
window.initDevicesTab = initDevicesTab;
window.initControllersTab = initControllersTab;

// ============================================================================
// CONTROLLERS TAB
// ============================================================================

// Controller type definitions (matching ioConfig indices)
const CONTROLLER_TYPES = {
    TEMPERATURE: { value: 0, name: 'Temperature Controller', indices: [40, 41, 42] },
    PH: { value: 1, name: 'pH Controller', indices: [43] },
    DO: { value: 2, name: 'Dissolved Oxygen Controller', indices: [44] }
};

// State management
let controllersData = [];
let controllersPolling = null;
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
    await loadControllers();
    
    // Start polling
    if (!controllersPolling) {
        controllersPolling = setInterval(loadControllers, 2000);
    }
}

/**
 * Load all controllers from API
 */
async function loadControllers() {
    try {
        const response = await fetch('/api/controllers');
        console.log('[CONTROLLERS] Fetch response:', response.status, response.ok);
        
        if (!response.ok) throw new Error('Failed to load controllers');
        
        const data = await response.json();
        console.log('[CONTROLLERS] Received data:', data);
        
        controllersData = data.controllers || [];
        console.log('[CONTROLLERS] Controllers array:', controllersData.length, 'items');
        
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
        // Don't show toast on every poll failure
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
    
    // Update process value
    const processValueDisplays = card.querySelectorAll('.controller-value-display');
    if (processValueDisplays[0]) {
        processValueDisplays[0].textContent = ctrl.processValue !== null ? ctrl.processValue.toFixed(1) + ctrl.unit : '--' + ctrl.unit;
    }
    
    // Update setpoint display
    if (processValueDisplays[1]) {
        processValueDisplays[1].textContent = ctrl.setpoint.toFixed(1) + ctrl.unit;
    }
    
    // Update output display
    if (processValueDisplays[2]) {
        const outputText = ctrl.output !== null 
            ? (ctrl.controlMethod === 0 
                ? (ctrl.output > 0 ? 'ON' : 'OFF')
                : ctrl.output.toFixed(0) + '%')
            : '--';
        processValueDisplays[2].textContent = outputText;
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
    
    // Update message
    const infoMessage = card.querySelector('.info-message');
    if (ctrl.message) {
        if (infoMessage) {
            infoMessage.textContent = ctrl.message;
        } else {
            const msgDiv = document.createElement('div');
            msgDiv.className = 'info-message';
            msgDiv.textContent = ctrl.message;
            card.querySelector('.controller-info-row').insertAdjacentElement('afterend', msgDiv);
        }
    } else if (infoMessage) {
        infoMessage.remove();
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
                <span class="output-name">[${ctrl.index}] ${ctrl.name}</span>
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
            <div class="controller-value-item">
                <div class="controller-value-label">Process Value</div>
                <div class="controller-value-display">${ctrl.processValue !== null ? ctrl.processValue.toFixed(1) : '--'}${ctrl.unit}</div>
            </div>
            <div class="controller-value-item">
                <div class="controller-value-label">Setpoint</div>
                <div class="controller-value-display">${ctrl.setpoint.toFixed(1)}${ctrl.unit}</div>
            </div>
            <div class="controller-value-item">
                <div class="controller-value-label">Output</div>
                <div class="controller-value-display">${
                    ctrl.output !== null 
                        ? (ctrl.controlMethod === 0 
                            ? (ctrl.output > 0 ? 'ON' : 'OFF')  // On/Off mode: show state
                            : ctrl.output.toFixed(0) + '%')      // PID mode: show percentage
                        : '--'
                }</div>
            </div>
        </div>
        
        <div class="controller-info-row">
            <span><strong>Mode:</strong> ${ctrl.controlMethod === 0 ? 'On/Off' : 'PID'}</span>
            ${ctrl.controlMethod === 1 ? `
                <span id="ctrl-gains-${ctrl.index}"><strong>Gains:</strong> P=${ctrl.kP.toFixed(2)}, I=${ctrl.kI.toFixed(2)}, D=${ctrl.kD.toFixed(2)}</span>
            ` : `
                <span><strong>Hysteresis:</strong> ${ctrl.hysteresis}${ctrl.unit}</span>
            `}
        </div>
        
        ${ctrl.message ? `<div class="info-message">${ctrl.message}</div>` : ''}
        
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
            ` : ''}
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
    const sensors = await loadAvailableSensors();
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
    expandedDiv.innerHTML = `
        <hr style="margin: 20px 0; border: none; border-top: 1px solid #dee2e6;">
        
        <div class="form-group">
            <label for="ctrlName">Name:</label>
            <input type="text" id="ctrlName" value="${selectedControllerType.name} ${availableIndices[0]}" maxlength="39">
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
    
    configForm.appendChild(expandedDiv);
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
    
    // Gather config data from the expanded form
    const configData = {
        isActive: true,
        name: document.getElementById('ctrlName').value,
        showOnDashboard: document.getElementById('ctrlShowDashboard').checked,
        unit: 'C',
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
        console.log(`[CONTROLLERS] Creating controller at index ${index}`, configData);
        
        const response = await fetch(`/api/controller/${index}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to create controller');
        }
        
        showToast('success', 'Success', 'Controller created successfully');
        closeAddControllerModal();
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error creating:', error);
        showToast('error', 'Error', error.message);
    }
}

async function openControllerConfig(index) {
    currentConfigIndex = index;
    console.log(`[CONTROLLERS] Opening config for controller ${index}`);
    
    try {
        const response = await fetch(`/api/controller/${index}`);
        if (!response.ok) throw new Error('Failed to load config');
        
        const config = await response.json();
        await renderConfigForm(config);
        
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
    
    const sensors = await loadAvailableSensors();
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

async function loadAvailableSensors() {
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
        
        if (data.deviceSensors) {
            data.deviceSensors.forEach(ds => {
                if (ds.n.toLowerCase().includes('temp')) {
                    sensors.push({
                        index: ds.i,
                        name: `[${ds.i}] ${ds.n}`
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
        
        // Only show digital outputs (21-25) for temperature controllers
        // DAC outputs (8-9) are not suitable for heater control
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

async function saveControllerConfig() {
    if (currentConfigIndex === null) return;
    
    const configData = {
        isActive: true,
        name: document.getElementById('ctrlName').value,
        showOnDashboard: document.getElementById('ctrlShowDashboard').checked,
        unit: 'C',
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
        const response = await fetch(`/api/controller/${currentConfigIndex}`, {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) {
            const error = await response.json().catch(() => ({}));
            throw new Error(error.error || 'Failed to save configuration');
        }
        
        showToast('success', 'Success', 'Controller configuration saved');
        closeControllerConfigModal();
        await loadControllers();
        
    } catch (error) {
        console.error('[CONTROLLERS] Error saving:', error);
        showToast('error', 'Error', error.message);
    }
}

async function deleteController() {
    if (currentConfigIndex === null) return;
    
    if (!confirm('Are you sure you want to delete this controller?')) {
        return;
    }
    
    try {
        const response = await fetch(`/api/controller/${currentConfigIndex}`, {
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
    
    try {
        const response = await fetch(`/api/controller/${index}/setpoint`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ setpoint })
        });
        
        if (!response.ok) throw new Error('Failed to update setpoint');
        
        console.log(`[CONTROLLERS] Updated setpoint for ${index} to ${setpoint}`);
        showToast('success', 'Success', 'Setpoint updated');
        
    } catch (error) {
        console.error('[CONTROLLERS] Error updating setpoint:', error);
        showToast('error', 'Error', 'Failed to update setpoint');
    }
}

async function enableController(index) {
    try {
        const response = await fetch(`/api/controller/${index}/enable`, {
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
    try {
        const response = await fetch(`/api/controller/${index}/disable`, {
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

// Cleanup
window.addEventListener('beforeunload', () => {
    if (controllersPolling) {
        clearInterval(controllersPolling);
        controllersPolling = null;
    }
});

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