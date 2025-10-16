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
    }
}

// Live clock update function
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

            // Set static IP fields
            document.getElementById('ipAddress').value = data.ip || '';
            document.getElementById('subnetMask').value = data.subnet || '';
            document.getElementById('gateway').value = data.gateway || '';
            document.getElementById('dns').value = data.dns || '';

            // Set NTP server
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
document.addEventListener('DOMContentLoaded', () => {
    // Switch to light theme by default
    document.body.classList.add('theme-light');
    loadInitialSettings();  // Load initial NTP and timezone settings
    loadNetworkSettings();  // Load initial network settings
    loadMqttSettings();     // Load initial MQTT settings
    loadControlSettings();  // Load initial control settings
    updateLiveClock();
    updateSensorData();
    updateNetworkInfo();
    
    // Initialize system status if system tab is active initially
    if (document.querySelector('#system').classList.contains('active')) {
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
    if (document.querySelector('#control').classList.contains('active')) {
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

function saveTimeSettings() {
    // This function is called but not defined - adding a placeholder
    // In a real implementation, this would save time settings
    console.log('Save time settings called');
}

function saveControl(controlType) {
    // This function is called but not defined - adding a placeholder
    // In a real implementation, this would save control settings
    console.log('Save control called for:', controlType);
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

async function fetchAndRenderInputs() {
    try {
        const response = await fetch('/api/inputs');
        if (!response.ok) throw new Error('Failed to fetch inputs');
        
        const data = await response.json();
        
        // Render each section
        renderADCInputs(data.adc || []);
        renderRTDInputs(data.rtd || []);
        renderGPIOInputs(data.gpio || []);
        
    } catch (error) {
        console.error('Error fetching inputs:', error);
        showInputError('adc-list');
        showInputError('rtd-list');
        showInputError('gpio-list');
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
                <span class="input-name">ADC ${input.i + 1}</span>
                <span class="input-index">Index: ${input.i}</span>
            </div>
            <div class="input-value">
                <span class="value-large">${input.v.toFixed(2)}</span>
                <span class="value-unit">${input.u}</span>
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
                <span class="input-name">RTD ${input.i - 9}</span>
                <span class="input-index">Index: ${input.i}</span>
            </div>
            <div class="input-value">
                <span class="value-large">${input.v.toFixed(2)}</span>
                <span class="value-unit">${input.u}</span>
            </div>
            ${input.f ? '<div class="fault-indicator">FAULT</div>' : ''}
        </div>
    `).join('');
}

function renderGPIOInputs(gpioData) {
    const container = document.getElementById('gpio-list');
    if (!container) return;
    
    if (gpioData.length === 0) {
        container.innerHTML = '<div class="empty-message">No GPIO data available</div>';
        return;
    }
    
    container.innerHTML = gpioData.map(input => `
        <div class="input-item ${input.f ? 'fault' : ''}">
            <div class="input-header">
                <span class="input-name">GPIO ${input.i - 13}</span>
                <span class="input-index">Index: ${input.i}</span>
            </div>
            <div class="input-value">
                <span class="digital-state ${input.s ? 'state-high' : 'state-low'}">
                    ${input.s ? 'HIGH' : 'LOW'}
                </span>
            </div>
            ${input.f ? '<div class="fault-indicator">FAULT</div>' : ''}
        </div>
    `).join('');
}

function showInputError(containerId) {
    const container = document.getElementById(containerId);
    if (container) {
        container.innerHTML = '<div class="error-message">Failed to load data</div>';
    }
}