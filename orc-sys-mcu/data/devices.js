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
    10: { name: 'Pressure Controller', interface: 1, description: '0-10V analogue pressure controller', hasSetpoint: true },
    
    // Motor driven drivers
    20: { name: 'Stirrer', interface: 2, description: 'Stepper or DC motor stirrer', hasSetpoint: true },
    21: { name: 'Pump', interface: 2, description: 'Stepper or DC motor pump', hasSetpoint: true }
};

const INTERFACE_NAMES = {
    0: 'Modbus RTU',
    1: 'Analogue I/O',
    2: 'Motor Driven'
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

function updateDeviceControlStatus(devices) {
    devices.forEach(device => {
        // Calculate control index from sensor index
        const controlIndex = device.dynamicIndex - 20;  // 70 - 20 = 50
        
        // Find the device card
        const card = document.querySelector(`.device-card[data-control-index="${controlIndex}"]`);
        if (!card) return;
        
        // Update connected status
        const statusDiv = card.querySelector('.device-status');
        if (statusDiv) {
            const isConnected = device.connected !== false;  // Default to true if not specified
            statusDiv.className = `device-status ${isConnected ? 'status-online' : 'status-offline'}`;
            statusDiv.textContent = isConnected ? 'Connected' : 'Disconnected';
            
            if (device.fault) {
                statusDiv.className = 'device-status status-fault';
                statusDiv.textContent = 'FAULT';
            }
        }
        
        // Update control values if they exist
        const setpointValue = card.querySelector('.setpoint-value');
        const actualValue = card.querySelector('.actual-value');
        const message = card.querySelector('.device-message');
        
        if (setpointValue && device.setpoint !== undefined) {
            setpointValue.textContent = `${device.setpoint.toFixed(2)} ${device.unit || ''}`;
        }
        
        if (actualValue && device.actualValue !== undefined) {
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
    document.getElementById('motorDrivenFields').style.display = 'none';
    
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
    document.getElementById('motorDrivenFields').style.display = 'none';
}

function updateDeviceFields() {
    const driverType = parseInt(document.getElementById('deviceDriverType').value);
    const interfaceType = parseInt(document.getElementById('deviceInterfaceType').value);
    
    if (isNaN(driverType)) {
        document.getElementById('deviceCommonFields').style.display = 'none';
        document.getElementById('modbusFields').style.display = 'none';
        document.getElementById('analogueIOFields').style.display = 'none';
        document.getElementById('motorDrivenFields').style.display = 'none';
        return;
    }
    
    // Show common fields
    document.getElementById('deviceCommonFields').style.display = 'block';
    
    // Show interface-specific fields
    document.getElementById('modbusFields').style.display = (interfaceType === 0) ? 'block' : 'none';
    document.getElementById('analogueIOFields').style.display = (interfaceType === 1) ? 'block' : 'none';
    document.getElementById('motorDrivenFields').style.display = (interfaceType === 2) ? 'block' : 'none';
    
    // Set default device name based on driver
    const driverInfo = DRIVER_DEFINITIONS[driverType];
    if (driverInfo) {
        document.getElementById('deviceName').value = driverInfo.name;
    }
}

function updateMotorIndexOptions() {
    const motorType = document.getElementById('deviceMotorType').value;
    const dcMotorGroup = document.getElementById('dcMotorIndexGroup');
    
    dcMotorGroup.style.display = (motorType === 'dc') ? 'block' : 'none';
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
    
    // Calculate control index from sensor index
    const controlIndex = device.dynamicIndex - 20;  // e.g., 70 - 20 = 50
    card.setAttribute('data-control-index', controlIndex);
    
    const driverInfo = DRIVER_DEFINITIONS[device.driverType];
    const interfaceName = INTERFACE_NAMES[device.interfaceType];
    
    // Status indicator - will be updated by polling
    const statusClass = 'status-offline';
    const statusText = 'Disconnected';
    
    card.innerHTML = `
        <div class="output-header">
            <div class="output-header-left">
                <span class="output-name">${device.name}</span>
                <span class="output-mode-badge">${driverInfo ? driverInfo.name : 'Unknown'}</span>
            </div>
            <div class="output-header-right">
                <div class="device-status ${statusClass}">${statusText}</div>
                <button class="icon-btn" onclick="openDeviceConfig(${device.dynamicIndex})" title="Configure">
                    <svg viewBox="0 0 24 24"><path d="M5,3C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V12H19V19H5V5H12V3H5M17.78,4C17.61,4 17.43,4.07 17.3,4.2L16.08,5.41L18.58,7.91L19.8,6.7C20.06,6.44 20.06,6 19.8,5.75L18.25,4.2C18.12,4.07 17.95,4 17.78,4M15.37,6.12L8,13.5V16H10.5L17.87,8.62L15.37,6.12Z" /></svg>
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
                <strong>Range:</strong> ${device.minPressure}-${device.maxPressure} ${device.unit}
            </div>
        `;
    } else if (device.interfaceType === 2) { // Motor Driven
        const motorType = device.usesStepper ? 'Stepper' : 'DC Motor';
        html += `
            <div class="device-detail">
                <strong>Motor:</strong> ${motorType} (Index ${device.motorIndex})
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
        deviceData.minOutput = 0.0;
        deviceData.maxOutput = 10.0;
        deviceData.minPressure = parseFloat(document.getElementById('deviceMinPressure').value);
        deviceData.maxPressure = parseFloat(document.getElementById('deviceMaxPressure').value);
        
        if (isNaN(deviceData.minPressure) || isNaN(deviceData.maxPressure)) {
            showToast('error', 'Validation Error', 'Please enter valid pressure values');
            return;
        }
    } else if (interfaceType === 2) { // Motor Driven
        const motorType = document.getElementById('deviceMotorType').value;
        deviceData.usesStepper = (motorType === 'stepper');
        
        if (deviceData.usesStepper) {
            deviceData.motorIndex = 26;
        } else {
            deviceData.motorIndex = parseInt(document.getElementById('deviceMotorIndex').value);
        }
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
                <label for="configPressureUnit">Pressure Unit:</label>
                <select id="configPressureUnit">
                    <option value="bar" ${device.unit === 'bar' ? 'selected' : ''}>bar</option>
                    <option value="kPa" ${device.unit === 'kPa' ? 'selected' : ''}>kPa</option>
                    <option value="psi" ${device.unit === 'psi' ? 'selected' : ''}>psi</option>
                </select>
            </div>
            <div class="form-group">
                <label for="configMinPressure">Minimum Pressure:</label>
                <input type="number" id="configMinPressure" value="${device.minPressure}" step="0.1" required>
            </div>
            <div class="form-group">
                <label for="configMaxPressure">Maximum Pressure:</label>
                <input type="number" id="configMaxPressure" value="${device.maxPressure}" step="0.1" required>
            </div>
            <div class="info-box">
                <strong>Note:</strong> Output voltage range is fixed at 0-10V
            </div>
        `;
    } else if (device.interfaceType === 2) { // Motor Driven
        const usesStepperMotor = device.usesStepper;
        html += `
            <div class="form-group">
                <label for="configMotorType">Motor Type:</label>
                <select id="configMotorType" onchange="updateMotorIndexOptions()">
                    <option value="stepper" ${usesStepperMotor ? 'selected' : ''}>Stepper Motor</option>
                    <option value="dc" ${!usesStepperMotor ? 'selected' : ''}>DC Motor</option>
                </select>
            </div>
            <div class="form-group" id="configMotorIndexGroup">
                <label for="configMotorIndex">Motor Index:</label>
                <select id="configMotorIndex">
                    ${usesStepperMotor ? 
                        '<option value="26" selected>Stepper Motor (26)</option>' :
                        `<option value="27" ${device.motorIndex === 27 ? 'selected' : ''}>DC Motor 1 (27)</option>
                         <option value="28" ${device.motorIndex === 28 ? 'selected' : ''}>DC Motor 2 (28)</option>
                         <option value="29" ${device.motorIndex === 29 ? 'selected' : ''}>DC Motor 3 (29)</option>
                         <option value="30" ${device.motorIndex === 30 ? 'selected' : ''}>DC Motor 4 (30)</option>`
                    }
                </select>
            </div>
        `;
    }
    
    container.innerHTML = html;
}

// Helper function for motor config modal
function updateMotorIndexOptions() {
    const motorType = document.getElementById('configMotorType').value;
    const motorIndexSelect = document.getElementById('configMotorIndex');
    
    if (motorType === 'stepper') {
        motorIndexSelect.innerHTML = '<option value="26">Stepper Motor (26)</option>';
    } else {
        motorIndexSelect.innerHTML = `
            <option value="27">DC Motor 1 (27)</option>
            <option value="28">DC Motor 2 (28)</option>
            <option value="29">DC Motor 3 (29)</option>
            <option value="30">DC Motor 4 (30)</option>
        `;
    }
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
            updateData.minPressure = parseFloat(document.getElementById('configMinPressure').value);
            updateData.maxPressure = parseFloat(document.getElementById('configMaxPressure').value);
            updateData.minOutput = 0.0;
            updateData.maxOutput = 10.0;
            
            if (isNaN(updateData.minPressure) || isNaN(updateData.maxPressure)) {
                showToast('error', 'Validation Error', 'Please enter valid pressure values');
                return;
            }
        } else if (device.interfaceType === 2) { // Motor Driven
            const motorType = document.getElementById('configMotorType').value;
            updateData.usesStepper = (motorType === 'stepper');
            updateData.motorIndex = parseInt(document.getElementById('configMotorIndex').value);
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
