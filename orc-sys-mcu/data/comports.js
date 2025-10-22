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
                        <svg viewBox="0 0 24 24"><path d="M21,16V4H3V16H21M21,2A2,2 0 0,1 23,4V16A2,2 0 0,1 21,18H14V20H16V22H8V20H10V18H3C1.89,18 1,17.1 1,16V4C1,2.89 1.89,2 3,2H21M5,6H14V11H5V6M15,6H19V8H15V6M19,9V14H15V9H19M5,12H9V14H5V12M10,12H14V14H10V12Z" /></svg>
                    </span>
                    <button class="icon-btn" onclick="openComPortConfigModal(${port.index})" title="Configure">
                        <svg viewBox="0 0 24 24"><path d="M5,3C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V12H19V19H5V5H12V3H5M17.78,4C17.61,4 17.43,4.07 17.3,4.2L16.08,5.41L18.58,7.91L19.8,6.7C20.06,6.44 20.06,6 19.8,5.75L18.25,4.2C18.12,4.07 17.95,4 17.78,4M15.37,6.12L8,13.5V16H10.5L17.87,8.62L15.37,6.12Z" /></svg>
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
