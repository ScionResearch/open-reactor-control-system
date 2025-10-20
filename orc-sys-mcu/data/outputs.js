// ============================================================================
// OUTPUTS TAB - Hardware Outputs Monitoring & Control
// ============================================================================

let outputsRefreshInterval = null;
let activeControls = new Set(); // Track controls being manipulated
let lastOutputModes = new Map(); // Track output modes to detect changes
let pendingCommands = new Map(); // Track pending commands {index: setValue}
let stepperInputFocused = false; // Track if stepper RPM input has focus
let dcMotorSlidersFocused = new Map(); // Track which DC motor sliders have focus {index: boolean}

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
        renderDigitalOutputs(data.digitalOutputs || []);
        renderStepperMotor(data.stepperMotor);
        renderDCMotors(data.dcMotors || []);
        
    } catch (error) {
        console.error('Error fetching outputs:', error);
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
                        <span class="value-display" id="slider-display-${output.index}">${output.value || 0}%</span>
                        <span class="status-value-compact status-synced" id="slider-actual-${output.index}" data-actual="${output.value || 0}">${output.value || 0}%</span>
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
                            <svg viewBox="0 0 24 24"><path d="M21,16V4H3V16H21M21,2A2,2 0 0,1 23,4V16A2,2 0 0,1 21,18H14V20H16V22H8V20H10V18H3C1.89,18 1,17.1 1,16V4C1,2.89 1.89,2 3,2H21M5,6H14V11H5V6M15,6H19V8H15V6M19,9V14H15V9H19M5,12H9V14H5V12M10,12H14V14H10V12Z" /></svg>
                        </span>
                        <button class="icon-btn" onclick="openDigitalOutputConfigModal(${output.index})" title="Configure">
                            <svg viewBox="0 0 24 24"><path d="M5,3C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V12H19V19H5V5H12V3H5M17.78,4C17.61,4 17.43,4.07 17.3,4.2L16.08,5.41L18.58,7.91L19.8,6.7C20.06,6.44 20.06,6 19.8,5.75L18.25,4.2C18.12,4.07 17.95,4 17.78,4M15.37,6.12L8,13.5V16H10.5L17.87,8.62L15.37,6.12Z" /></svg>
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
                    <span class="output-status-badge" style="background-color: #6c757d; margin-left: 4px;">
                        ${stepper.direction ? '→ FORWARD' : '← REVERSE'}
                    </span>
                </div>
                <div class="output-header-right">
                    <span class="dashboard-icon ${stepper.d ? 'active' : 'inactive'}" 
                          title="${stepper.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                        <svg viewBox="0 0 24 24"><path d="M21,16V4H3V16H21M21,2A2,2 0 0,1 23,4V16A2,2 0 0,1 21,18H14V20H16V22H8V20H10V18H3C1.89,18 1,17.1 1,16V4C1,2.89 1.89,2 3,2H21M5,6H14V11H5V6M15,6H19V8H15V6M19,9V14H15V9H19M5,12H9V14H5V12M10,12H14V14H10V12Z" /></svg>
                    </span>
                    <button class="icon-btn" onclick="openStepperConfigModal()" title="Configure">
                        <svg viewBox="0 0 24 24"><path d="M5,3C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V12H19V19H5V5H12V3H5M17.78,4C17.61,4 17.43,4.07 17.3,4.2L16.08,5.41L18.58,7.91L19.8,6.7C20.06,6.44 20.06,6 19.8,5.75L18.25,4.2C18.12,4.07 17.95,4 17.78,4M15.37,6.12L8,13.5V16H10.5L17.87,8.62L15.37,6.12Z" /></svg>
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
                    <label class="control-label">Direction</label>
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
            statusBadges[1].textContent = stepper.direction ? '→ FORWARD' : '← REVERSE';
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
                        <span class="output-status-badge" style="background-color: #6c757d; margin-left: 4px;">
                            ${motor.direction ? '→ FORWARD' : '← REVERSE'}
                        </span>
                    </div>
                    <div class="output-header-right">
                        <span class="dashboard-icon ${motor.d ? 'active' : 'inactive'}" 
                              title="${motor.d ? 'Shown on Dashboard' : 'Hidden from Dashboard'}">
                            <svg viewBox="0 0 24 24"><path d="M21,16V4H3V16H21M21,2A2,2 0 0,1 23,4V16A2,2 0 0,1 21,18H14V20H16V22H8V20H10V18H3C1.89,18 1,17.1 1,16V4C1,2.89 1.89,2 3,2H21M5,6H14V11H5V6M15,6H19V8H15V6M19,9V14H15V9H19M5,12H9V14H5V12M10,12H14V14H10V12Z" /></svg>
                        </span>
                        <button class="icon-btn" onclick="openDCMotorConfigModal(${motor.index})" title="Configure">
                            <svg viewBox="0 0 24 24"><path d="M5,3C3.89,3 3,3.89 3,5V19A2,2 0 0,0 5,21H19A2,2 0 0,0 21,19V12H19V19H5V5H12V3H5M17.78,4C17.61,4 17.43,4.07 17.3,4.2L16.08,5.41L18.58,7.91L19.8,6.7C20.06,6.44 20.06,6 19.8,5.75L18.25,4.2C18.12,4.07 17.95,4 17.78,4M15.37,6.12L8,13.5V16H10.5L17.87,8.62L15.37,6.12Z" /></svg>
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
                    <div class="motor-info-row">
                        <div class="motor-info-item">
                            <span class="motor-info-label">Actual Power</span>
                            <span class="motor-info-value" id="motor-actual-power-${motor.index}">${motor.power || 0}%</span>
                        </div>
                        <div class="motor-info-item">
                            <span class="motor-info-label">Current</span>
                            <span class="motor-info-value" id="motor-current-${motor.index}">${(motor.current || 0).toFixed(2)}A</span>
                        </div>
                    </div>
                    
                    <!-- Direction Buttons -->
                    <div class="control-group">
                        <label class="control-label">Direction</label>
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
                statusBadges[1].textContent = motor.direction ? '→ FORWARD' : '← REVERSE';
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
        document.getElementById('stepperHoldCurrent').value = stepperConfigData.holdCurrent_mA || 500;
        document.getElementById('stepperRunCurrent').value = stepperConfigData.runCurrent_mA || 1000;
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
    
    try {
        const response = await fetch('/api/config/stepper', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(configData)
        });
        
        if (!response.ok) throw new Error('Failed to save config');
        
        showToast('success', 'Success', 'Configuration saved successfully');
        closeStepperConfigModal();
        fetchAndRenderOutputs();
        
    } catch (error) {
        console.error('Error saving stepper config:', error);
        showToast('error', 'Error', 'Failed to save configuration');
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
