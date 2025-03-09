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

        if (!response.ok) {
            throw new Error(`HTTP error! status: ${response.status}`);
        }
        
        const result = await response.json();
        console.log('Time settings updated:', result);
    } catch (error) {
        console.error('Error saving time settings:', error);
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
    loadMqttSettings();    // Load initial MQTT settings
    updateLiveClock();
    updateSensorData();
    updateNetworkInfo();
    
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

// Update power supply status
async function updatePowerStatus() {
    try {
        const response = await fetch('/api/power');
        const data = await response.json();
        
        // Update main voltage
        document.getElementById('mainVoltage').textContent = data.mainVoltage.toFixed(1) + 'V';
        const mainStatus = document.getElementById('mainVoltageStatus');
        mainStatus.textContent = data.mainVoltageOK ? 'OK' : 'ERROR';
        mainStatus.className = 'status ' + (data.mainVoltageOK ? 'ok' : 'error');
        
        // Update 20V supply
        document.getElementById('v20Voltage').textContent = data.v20Voltage.toFixed(1) + 'V';
        const v20Status = document.getElementById('v20VoltageStatus');
        v20Status.textContent = data.v20VoltageOK ? 'OK' : 'ERROR';
        v20Status.className = 'status ' + (data.v20VoltageOK ? 'ok' : 'error');
        
        // Update 5V supply
        document.getElementById('v5Voltage').textContent = data.v5Voltage.toFixed(1) + 'V';
        const v5Status = document.getElementById('v5VoltageStatus');
        v5Status.textContent = data.v5VoltageOK ? 'OK' : 'ERROR';
        v5Status.className = 'status ' + (data.v5VoltageOK ? 'ok' : 'error');
    } catch (error) {
        console.error('Error updating power status:', error);
    }
}

// Add power status update to the periodic updates
setInterval(updatePowerStatus, 1000);

// Network settings handling
document.getElementById('networkForm').addEventListener('submit', async function(e) {
    e.preventDefault();
    const statusDiv = document.getElementById('networkStatus');
    
    // Validate IP addresses if static IP is selected
    if (document.getElementById('ipConfig').value === 'static') {
        const inputs = ['ipAddress', 'subnetMask', 'gateway', 'dns'];
        for (const id of inputs) {
            const input = document.getElementById(id);
            if (!input.checkValidity()) {
                statusDiv.textContent = `Invalid ${id.replace(/([A-Z])/g, ' $1').toLowerCase()}`;
                statusDiv.className = 'status-message error';
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
        ntp: document.getElementById('ntpServer').value
    };

    try {
        const response = await fetch('/api/network', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(networkConfig)
        });

        const result = await response.json();
        
        if (response.ok) {
            statusDiv.textContent = 'Network settings saved. The device will restart to apply changes...';
            statusDiv.className = 'status-message success';
            // Wait a moment before reloading to show the message
            setTimeout(() => {
                window.location.reload();
            }, 5000);
        } else {
            statusDiv.textContent = result.error || 'Failed to save network settings';
            statusDiv.className = 'status-message error';
        }
    } catch (error) {
        statusDiv.textContent = 'Network error: ' + error.message;
        statusDiv.className = 'status-message error';
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

    try {
        const response = await fetch('/api/mqtt', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(mqttData)
        });

        if (response.ok) {
            const statusDiv = document.createElement('div');
            statusDiv.className = 'status-message success';
            statusDiv.textContent = 'MQTT settings saved successfully';
            document.getElementById('mqttForm').appendChild(statusDiv);
            setTimeout(() => statusDiv.remove(), 3000);
        } else {
            throw new Error('Failed to save MQTT settings');
        }
    } catch (error) {
        console.error('Error saving MQTT settings:', error);
        const statusDiv = document.createElement('div');
        statusDiv.className = 'status-message error';
        statusDiv.textContent = 'Failed to save MQTT settings';
        document.getElementById('mqttForm').appendChild(statusDiv);
        setTimeout(() => statusDiv.remove(), 3000);
    }
});
