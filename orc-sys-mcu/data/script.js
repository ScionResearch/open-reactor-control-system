// Navigation
document.querySelectorAll('nav a').forEach(link => {
    link.addEventListener('click', (e) => {
        e.preventDefault();
        const page = e.target.dataset.page;
        document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
        document.querySelector(`#${page}`).classList.add('active');
        document.querySelectorAll('nav a').forEach(a => a.classList.remove('active'));
        e.target.classList.add('active');
    });
});

// Chart initialization
const ctx = document.getElementById('sensorChart').getContext('2d');
const sensorChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Temperature',
            data: [],
            borderColor: 'rgb(255, 99, 132)',
            tension: 0.1
        }, {
            label: 'pH',
            data: [],
            borderColor: 'rgb(54, 162, 235)',
            tension: 0.1
        }, {
            label: 'DO',
            data: [],
            borderColor: 'rgb(75, 192, 192)',
            tension: 0.1
        }]
    },
    options: {
        responsive: true,
        scales: {
            y: {
                beginAtZero: true
            }
        }
    }
});

// Network and sensor data updates
async function updateSensorData() {
    try {
        const response = await fetch('/api/sensors');
        const data = await response.json();
        
        // Update each sensor value
        Object.keys(data).forEach(sensorId => {
            const element = document.getElementById(sensorId);
            if (element) {
                element.textContent = data[sensorId];
            }
        });
    } catch (error) {
        console.error('Error updating sensor data:', error);
    }
}

async function updateNetworkInfo() {
    try {
        const response = await fetch('/api/network');
        const settings = await response.json();
        const currentIPElement = document.getElementById('currentIP');
        const macAddressElement = document.getElementById('macAddress');
        
        if (currentIPElement) {
            currentIPElement.textContent = settings.ip || 'Not Connected';
        }
        if (macAddressElement) {
            macAddressElement.textContent = settings.mac || '';
        }
    } catch (error) {
        console.error('Error updating network info:', error);
    }
}

async function saveTimeSettings() {
    const date = document.getElementById('currentDate').value;
    const fullTime = document.getElementById('currentTime').value;
    // Extract only hours and minutes from the time value
    const timeParts = fullTime.split(':');
    const time = `${timeParts[0]}:${timeParts[1]}`; // HH:MM format
    const timezone = document.getElementById('timezone').value;
    const ntpEnabled = document.getElementById('enableNTP').checked;
    const dstEnabled = document.getElementById('enableDST').checked;

    // Show loading toast
    const loadingToast = showToast('info', 'Saving...', 'Updating date and time settings', 10000);

    try {
        const response = await fetch('/api/time', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                date,
                time,
                timezone,
                ntpEnabled,
                dstEnabled
            }),
        });

        // Remove loading toast
        if (loadingToast.parentNode) {
            loadingToast.parentNode.removeChild(loadingToast);
        }

        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const result = await response.json();
        console.log('Time settings updated:', result);
        
        // Show success toast
        showToast('success', 'Success', 'Date and time settings saved successfully');
        
        // Update the time display immediately
        updateLiveClock();
    } catch (error) {
        console.error('Error saving time settings:', error);
        
        // Show error toast
        showToast('error', 'Error', 'Failed to save date and time settings');
    }
}

// Update live clock display
async function updateLiveClock() {
    const clockElement = document.getElementById('liveClock');
    if (clockElement) {
        try {
            const response = await fetch('/api/time');
            const data = await response.json();
            console.log('Time data received:', data);

            if (data.date && data.time && data.timezone) {
                const isoDateTime = `${data.date}T${data.time}${data.timezone}`;

               const now = new Date(isoDateTime);

               const formattedTime = new Intl.DateTimeFormat('en-NZ', {
                    year: 'numeric',
                    month: 'long',
                    day: 'numeric',
                    hour: '2-digit',
                    minute: '2-digit',
                    second: '2-digit',
                    timeZone: data.timezone
                }).format(now) + ' ' + data.timezone + (data.dst ? ' (DST)' : '');
           
                clockElement.textContent = formattedTime;
            }

            // Update NTP status if NTP is enabled
            const ntpStatusContainer = document.getElementById('ntpStatusContainer');
            const ntpStatusElement = document.getElementById('ntpStatus');
            const lastNtpUpdateElement = document.getElementById('lastNtpUpdate');
            
            if (data.ntpEnabled) {
                ntpStatusContainer.style.display = 'block';
                
                if (ntpStatusElement && data.hasOwnProperty('ntpStatus')) {
                    // Clear previous classes
                    ntpStatusElement.className = 'ntp-status';
                    
                    // Add status text and class
                    let statusText = '';
                    switch (data.ntpStatus) {
                        case 0: // NTP_STATUS_CURRENT
                            statusText = 'Current';
                            ntpStatusElement.classList.add('ntp-status-current');
                            break;
                        case 1: // NTP_STATUS_STALE
                            statusText = 'Stale';
                            ntpStatusElement.classList.add('ntp-status-stale');
                            break;
                        case 2: // NTP_STATUS_FAILED
                        default:
                            statusText = 'Failed';
                            ntpStatusElement.classList.add('ntp-status-failed');
                            break;
                    }
                    ntpStatusElement.textContent = statusText;
                }
                
                if (lastNtpUpdateElement && data.hasOwnProperty('lastNtpUpdate')) {
                    lastNtpUpdateElement.textContent = data.lastNtpUpdate;
                }
            } else {
                ntpStatusContainer.style.display = 'none';
            }
        } catch (error) {
            console.error('Error updating clock:', error);
        }
    }
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

// Event listeners
document.addEventListener('DOMContentLoaded', () => {
    loadInitialSettings();  // Load initial NTP and timezone settings
    loadNetworkSettings();  // Load initial network settings
    loadMqttSettings();     // Load initial MQTT settings
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
});

// Update intervals
setInterval(updateLiveClock, 1000);    // Update clock every second
setInterval(updateSensorData, 1000);   // Update sensor data every second
setInterval(updateNetworkInfo, 5000);  // Update network info every 5 seconds

// Update system status information
async function updateSystemStatus() {
    try {
        const response = await fetch('/api/system/status');
        const data = await response.json();
        
        // Update power supplies
        if (data.power) {
            // Main voltage
            document.getElementById('mainVoltage').textContent = data.power.mainVoltage.toFixed(1) + 'V';
            const mainStatus = document.getElementById('mainVoltageStatus');
            mainStatus.textContent = data.power.mainVoltageOK ? 'OK' : 'OUT OF RANGE';
            mainStatus.className = 'status ' + (data.power.mainVoltageOK ? 'ok' : 'error');
            
            // 20V supply
            document.getElementById('v20Voltage').textContent = data.power.v20Voltage.toFixed(1) + 'V';
            const v20Status = document.getElementById('v20VoltageStatus');
            v20Status.textContent = data.power.v20VoltageOK ? 'OK' : 'OUT OF RANGE';
            v20Status.className = 'status ' + (data.power.v20VoltageOK ? 'ok' : 'error');
            
            // 5V supply
            document.getElementById('v5Voltage').textContent = data.power.v5Voltage.toFixed(1) + 'V';
            const v5Status = document.getElementById('v5VoltageStatus');
            v5Status.textContent = data.power.v5VoltageOK ? 'OK' : 'OUT OF RANGE';
            v5Status.className = 'status ' + (data.power.v5VoltageOK ? 'ok' : 'error');
        }
        
        // Update RTC status
        if (data.rtc) {
            const rtcStatus = document.getElementById('rtcStatus');
            rtcStatus.textContent = data.rtc.ok ? 'OK' : 'ERROR';
            rtcStatus.className = 'status ' + (data.rtc.ok ? 'ok' : 'error');
            
            document.getElementById('rtcTime').textContent = data.rtc.time;
        }
        
        // Update IPC status
        const ipcStatus = document.getElementById('ipcStatus');
        ipcStatus.textContent = data.ipc ? 'OK' : 'ERROR';
        ipcStatus.className = 'status ' + (data.ipc ? 'ok' : 'error');
        
        // Update MQTT status
        const mqttStatus = document.getElementById('mqttStatus');
        mqttStatus.textContent = data.mqtt ? 'CONNECTED' : 'NOT-CONNECTED';
        mqttStatus.className = 'status ' + (data.mqtt ? 'connected' : 'not-connected');
        
        // Update Modbus status
        const modbusStatus = document.getElementById('modbusStatus');
        modbusStatus.textContent = data.modbus ? 'CONNECTED' : 'NOT-CONNECTED';
        modbusStatus.className = 'status ' + (data.modbus ? 'connected' : 'not-connected');
        
        // Update SD card status
        if (data.sd) {
            const sdStatus = document.getElementById('sdStatus');
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
                document.getElementById('sdCapacityContainer').style.display = 'flex';
                document.getElementById('sdFreeSpaceContainer').style.display = 'flex';
                document.getElementById('sdLogSizeContainer').style.display = 'flex';
                document.getElementById('sdSensorSizeContainer').style.display = 'flex';
                
                // Update SD card details
                document.getElementById('sdCapacity').textContent = data.sd.capacityGB.toFixed(1) + ' GB';
                document.getElementById('sdFreeSpace').textContent = data.sd.freeSpaceGB.toFixed(1) + ' GB';
                document.getElementById('sdLogSize').textContent = data.sd.logFileSizeKB.toFixed(1) + ' kB';
                document.getElementById('sdSensorSize').textContent = data.sd.sensorFileSizeKB.toFixed(1) + ' kB';
            }
        }
    } catch (error) {
        console.error('Error updating system status:', error);
    }
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
setInterval(updateStatusIfSystemTabActive, 1000);

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
            toastContainer.removeChild(toast);
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

// Show/hide static IP settings
document.getElementById('ipConfig').addEventListener('change', function() {
    const staticSettings = document.getElementById('staticSettings');
    if (this.value === 'static') {
        staticSettings.classList.remove('hidden');
    } else {
        staticSettings.classList.add('hidden');
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
