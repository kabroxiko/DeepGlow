// Aquarium LED Controller - Web Interface JavaScript

let ws = null;
let reconnectInterval = null;
let currentState = {};
let presets = [];
let timers = [];
let config = {};

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    initializeWebSocket();
    setupEventListeners();
    loadPresets();
    loadTimers();
    loadConfig();
});

// WebSocket Connection
function initializeWebSocket() {
    const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${wsProtocol}//${window.location.host}/ws`;
    
    ws = new WebSocket(wsUrl);
    
    ws.onopen = () => {
        console.log('WebSocket connected');
        document.getElementById('statusIndicator').style.color = '#00cc88';
        if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
        }
    };
    
    ws.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            updateState(data);
        } catch (e) {
            console.error('Failed to parse WebSocket message:', e);
        }
    };
    
    ws.onclose = () => {
        console.log('WebSocket disconnected');
        document.getElementById('statusIndicator').style.color = '#ff4466';
        
        // Attempt to reconnect
        if (!reconnectInterval) {
            reconnectInterval = setInterval(() => {
                console.log('Attempting to reconnect...');
                initializeWebSocket();
            }, 5000);
        }
    };
    
    ws.onerror = (error) => {
        console.error('WebSocket error:', error);
    };
}

// Update UI with state from server
function updateState(state) {
    currentState = state;
    
    // Update controls without triggering events
    const powerToggle = document.getElementById('powerToggle');
    if (powerToggle.checked !== state.power) {
        powerToggle.checked = state.power;
    }
    
    const brightnessSlider = document.getElementById('brightnessSlider');
    if (brightnessSlider.value != state.brightness) {
        brightnessSlider.value = state.brightness;
        document.getElementById('brightnessValue').textContent = state.brightness;
    }
    
    const effectSelect = document.getElementById('effectSelect');
    if (effectSelect.value != state.effect) {
        effectSelect.value = state.effect;
    }
    
    if (state.params) {
        document.getElementById('speedSlider').value = state.params.speed;
        document.getElementById('speedValue').textContent = state.params.speed;
        
        document.getElementById('intensitySlider').value = state.params.intensity;
        document.getElementById('intensityValue').textContent = state.params.intensity;
        
        document.getElementById('color1Picker').value = '#' + state.params.color1.toString(16).padStart(6, '0');
        document.getElementById('color2Picker').value = '#' + state.params.color2.toString(16).padStart(6, '0');
    }
    
    // Update time display
    if (state.time) {
        document.getElementById('currentTime').textContent = state.time;
    }
    
    if (state.sunrise) {
        document.getElementById('sunriseTime').textContent = state.sunrise;
    }
    
    if (state.sunset) {
        document.getElementById('sunsetTime').textContent = state.sunset;
    }
    
    // Highlight active preset
    if (state.currentPreset !== undefined) {
        document.querySelectorAll('.preset-card').forEach((card, index) => {
            if (index === state.currentPreset) {
                card.classList.add('active');
            } else {
                card.classList.remove('active');
            }
        });
    }
}

// Setup event listeners
function setupEventListeners() {
    // Power toggle
    document.getElementById('powerToggle').addEventListener('change', (e) => {
        sendState({ power: e.target.checked });
    });
    
    // Brightness slider
    let brightnessTimeout;
    document.getElementById('brightnessSlider').addEventListener('input', (e) => {
        document.getElementById('brightnessValue').textContent = e.target.value;
        clearTimeout(brightnessTimeout);
        brightnessTimeout = setTimeout(() => {
            sendState({ brightness: parseInt(e.target.value) });
        }, 300);
    });
    
    // Transition time slider
    document.getElementById('transitionSlider').addEventListener('input', (e) => {
        const seconds = parseInt(e.target.value);
        document.getElementById('transitionValue').textContent = seconds;
        sendState({ transitionTime: seconds * 1000 });
    });
    
    // Effect selector
    document.getElementById('effectSelect').addEventListener('change', (e) => {
        sendState({ effect: parseInt(e.target.value) });
    });
    
    // Speed slider
    let speedTimeout;
    document.getElementById('speedSlider').addEventListener('input', (e) => {
        document.getElementById('speedValue').textContent = e.target.value;
        clearTimeout(speedTimeout);
        speedTimeout = setTimeout(() => {
            sendState({ 
                params: {
                    ...currentState.params,
                    speed: parseInt(e.target.value)
                }
            });
        }, 300);
    });
    
    // Intensity slider
    let intensityTimeout;
    document.getElementById('intensitySlider').addEventListener('input', (e) => {
        document.getElementById('intensityValue').textContent = e.target.value;
        clearTimeout(intensityTimeout);
        intensityTimeout = setTimeout(() => {
            sendState({ 
                params: {
                    ...currentState.params,
                    intensity: parseInt(e.target.value)
                }
            });
        }, 300);
    });
    
    // Color pickers
    document.getElementById('color1Picker').addEventListener('change', (e) => {
        const color = parseInt(e.target.value.substring(1), 16);
        sendState({ 
            params: {
                ...currentState.params,
                color1: color
            }
        });
    });
    
    document.getElementById('color2Picker').addEventListener('change', (e) => {
        const color = parseInt(e.target.value.substring(1), 16);
        sendState({ 
            params: {
                ...currentState.params,
                color2: color
            }
        });
    });
    
    // Configuration sliders
    document.getElementById('maxBrightness').addEventListener('input', (e) => {
        document.getElementById('maxBrightnessValue').textContent = e.target.value;
    });
    
    document.getElementById('minTransition').addEventListener('input', (e) => {
        document.getElementById('minTransitionValue').textContent = e.target.value;
    });
}

// Send state update to server
function sendState(updates) {
    fetch('/api/state', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(updates)
    })
    .then(response => response.json())
    .then(data => {
        if (!data.success) {
            console.error('Failed to update state');
        }
    })
    .catch(error => console.error('Error:', error));
}

// Load and display presets
function loadPresets() {
    fetch('/api/presets')
        .then(response => response.json())
        .then(data => {
            presets = data.presets;
            displayPresets();
        })
        .catch(error => console.error('Error loading presets:', error));
}

function displayPresets() {
    const grid = document.getElementById('presetGrid');
    grid.innerHTML = '';
    
    const effectNames = ['Solid', 'Ripple', 'Wave', 'Sunrise', 'Shimmer', 'Deep Ocean', 'Moonlight'];
    
    presets.forEach((preset, index) => {
        if (!preset.enabled && index > 0) return;
        
        const card = document.createElement('div');
        card.className = 'preset-card';
        if (currentState.currentPreset === index) {
            card.classList.add('active');
        }
        
        card.innerHTML = `
            <div class="preset-name">${preset.name}</div>
            <div class="preset-info">Effect: ${effectNames[preset.effect]}</div>
            <div class="preset-info">Brightness: ${preset.brightness}</div>
            <div class="preset-color-preview" style="background: linear-gradient(135deg, #${preset.params.color1.toString(16).padStart(6, '0')}, #${preset.params.color2.toString(16).padStart(6, '0')})"></div>
        `;
        
        card.addEventListener('click', () => applyPreset(index));
        grid.appendChild(card);
    });
}

function applyPreset(presetId) {
    fetch('/api/preset', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            id: presetId,
            apply: true
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            console.log('Preset applied:', presetId);
        }
    })
    .catch(error => console.error('Error applying preset:', error));
}

// Load and display timers
function loadTimers() {
    fetch('/api/timers')
        .then(response => response.json())
        .then(data => {
            timers = data.timers;
            displayTimers();
        })
        .catch(error => console.error('Error loading timers:', error));
}

function displayTimers() {
    const container = document.getElementById('scheduleTable');
    
    const timerTypes = ['Regular', 'Sunrise', 'Sunset'];
    const dayNames = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
    
    let html = '<table><thead><tr><th>Time</th><th>Type</th><th>Days</th><th>Preset</th><th>Status</th></tr></thead><tbody>';
    
    timers.forEach(timer => {
        if (!timer.enabled) return;
        
        let timeStr = '';
        if (timer.type === 0) {
            timeStr = `${timer.hour.toString().padStart(2, '0')}:${timer.minute.toString().padStart(2, '0')}`;
        } else if (timer.type === 1) {
            timeStr = `Sunrise ${timer.offset >= 0 ? '+' : ''}${timer.offset}m`;
        } else {
            timeStr = `Sunset ${timer.offset >= 0 ? '+' : ''}${timer.offset}m`;
        }
        
        let daysStr = '';
        for (let i = 0; i < 7; i++) {
            if (timer.days & (1 << i)) {
                daysStr += dayNames[i] + ' ';
            }
        }
        
        const statusClass = timer.enabled ? 'timer-enabled' : 'timer-disabled';
        
        html += `
            <tr class="${statusClass}">
                <td>${timeStr}</td>
                <td>${timerTypes[timer.type]}</td>
                <td>${daysStr}</td>
                <td>${presets[timer.presetId]?.name || 'N/A'}</td>
                <td>${timer.enabled ? '✓ Active' : '✗ Disabled'}</td>
            </tr>
        `;
    });
    
    html += '</tbody></table>';
    container.innerHTML = html;
}

function showTimerEditor() {
    showToast('Timer editor UI - To be implemented in full version');
}

// Load configuration
function loadConfig() {
    fetch('/api/config')
        .then(response => response.json())
        .then(data => {
            config = data;

            // Update config UI
            if (data.led) {
                document.getElementById('ledCount').value = data.led.count;
                if (data.led.type) {
                    document.getElementById('ledType').value = data.led.type;
                }
            }

            if (data.safety) {
                document.getElementById('maxBrightness').value = data.safety.maxBrightness;
                document.getElementById('maxBrightnessValue').textContent = data.safety.maxBrightness;

                const minTransSeconds = Math.floor(data.safety.minTransitionTime / 1000);
                document.getElementById('minTransition').value = minTransSeconds;
                document.getElementById('minTransitionValue').textContent = minTransSeconds;
            }

            if (data.time) {
                document.getElementById('timezoneOffset').value = data.time.timezoneOffset;
                document.getElementById('latitude').value = data.time.latitude;
                document.getElementById('longitude').value = data.time.longitude;
            }
        })
        .catch(error => console.error('Error loading config:', error));
}

function saveConfiguration() {
    const configUpdate = {
        led: {
            count: parseInt(document.getElementById('ledCount').value),
            type: document.getElementById('ledType').value
        },
        safety: {
            maxBrightness: parseInt(document.getElementById('maxBrightness').value),
            minTransitionTime: parseInt(document.getElementById('minTransition').value) * 1000
        },
        time: {
            timezoneOffset: parseInt(document.getElementById('timezoneOffset').value),
            latitude: parseFloat(document.getElementById('latitude').value),
            longitude: parseFloat(document.getElementById('longitude').value)
        }
    };
    
    fetch('/api/config', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(configUpdate)
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showToast('Configuration saved! Changes take effect immediately.');
            sendDebugWS('Config saved successfully');
        } else {
            showToast('Failed to save configuration');
            sendDebugWS('Config save failed');
        }
    })
    .catch(error => {
        console.error('Error:', error);
        showToast('Error saving configuration');
        sendDebugWS('Config save error: ' + error);
    });
// Toast notification
function showToast(message, duration = 3000) {
    const toast = document.getElementById('toast');
    toast.textContent = message;
    toast.style.display = 'block';
    toast.style.opacity = '0.95';
    setTimeout(() => {
        toast.style.opacity = '0';
        setTimeout(() => { toast.style.display = 'none'; }, 300);
    }, duration);
}

// Send debug message through WebSocket
function sendDebugWS(msg) {
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ debug: msg }));
    }
}
}
