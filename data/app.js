// Aquarium LED Controller - Web Interface JavaScript

let ws = null;
let reconnectInterval = null;
let currentState = {};
let clockInterval = null;
let clockSynced = false;
let localClock = null;
let presets = [];
let timers = [];
let config = {};

// Base URL logic for local development and file://
let BASE_URL = '';
if (location.protocol === 'file:' || location.hostname === 'localhost' || location.hostname === '127.0.0.1') {
    BASE_URL = localStorage.getItem('BASE_URL') || '';
    if (!BASE_URL) {
        BASE_URL = prompt('Enter backend base URL (e.g. http://192.168.1.100:80):', '');
        if (BASE_URL) localStorage.setItem('BASE_URL', BASE_URL);
    }
}

// Initialize on page load
document.addEventListener('DOMContentLoaded', () => {
    // Hide Quick Controls until first WebSocket message
    const quickControls = document.querySelector('.card');
    if (quickControls && quickControls.style) quickControls.style.display = 'none';
    initializeWebSocket();
    setupEventListeners();
    loadPresets();
    loadTimers();

    // OTA upload form handler
    const otaForm = document.getElementById('otaForm');
    if (otaForm) {
        otaForm.addEventListener('submit', async (e) => {
            e.preventDefault();
            const otaFileInput = document.getElementById('otaFile');
            const otaFile = otaFileInput ? otaFileInput.files[0] : null;
            if (!otaFile) {
                showToast('Please select a firmware file.');
                return;
            }
            const otaProgressBar = document.getElementById('otaProgressBar');
            const otaProgressFill = document.getElementById('otaProgressFill');
            if (otaProgressBar && otaProgressFill) {
                otaProgressBar.style.display = '';
                otaProgressFill.style.width = '0%';
            }
            try {
                let fileToSend = otaFile;
                // If .gz, decompress in browser using fflate
                if (otaFile.name.endsWith('.gz')) {
                    const arrayBuffer = await otaFile.arrayBuffer();
                    // fflate is loaded globally
                    const decompressed = fflate.gunzipSync(new Uint8Array(arrayBuffer));
                    fileToSend = new Blob([decompressed], { type: 'application/octet-stream' });
                }
                const xhr = new XMLHttpRequest();
                xhr.open('POST', BASE_URL + '/ota', true);
                xhr.setRequestHeader('Accept', 'application/json');
                xhr.upload.onprogress = function(e) {
                    if (e.lengthComputable && otaProgressFill) {
                        const percent = Math.round((e.loaded / e.total) * 100);
                        otaProgressFill.style.width = percent + '%';
                    }
                };
                xhr.onload = function() {
                    if (otaProgressBar && otaProgressFill) {
                        otaProgressFill.style.width = '100%';
                    }
                    if (xhr.status === 200) {
                        showToast('Firmware uploaded! Rebooting...', 4000);
                        setTimeout(function() {
                            location.reload();
                        }, 4500);
                    } else {
                        showToast('OTA failed: ' + (xhr.responseText || xhr.statusText), 6000);
                    }
                };
                xhr.onerror = function() {
                    showToast('OTA upload error.', 6000);
                };
                // Send as raw binary, not FormData
                xhr.send(fileToSend);
            } catch (err) {
                showToast('OTA error: ' + err, 6000);
            }
        });
    }

    // OTA file input filename display and progress bar
    const otaFileInput = document.getElementById('otaFile');
    const otaFileName = document.getElementById('otaFileName');
    if (otaFileInput && otaFileName) {
        otaFileInput.addEventListener('change', function() {
            otaFileName.textContent = this.files && this.files.length > 0 ? this.files[0].name : 'No file chosen';
        });
    }
});

// WebSocket Connection
function initializeWebSocket() {
    let wsUrl;
    if (BASE_URL) {
        // Convert BASE_URL to ws(s)://
        wsUrl = BASE_URL.replace(/^http/, 'ws') + '/ws';
    } else {
        const wsProtocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        wsUrl = `${wsProtocol}//${window.location.host}/ws`;
    }
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
        console.log('[WS] Connected');
        const statusIndicator = document.getElementById('statusIndicator');
        if (statusIndicator && statusIndicator.style) statusIndicator.style.color = '#00cc88';
        if (reconnectInterval) {
            clearInterval(reconnectInterval);
            reconnectInterval = null;
        }
    };

    ws.onmessage = (event) => {
        console.log('[WS] Message:', event.data);
        try {
            const data = JSON.parse(event.data);
            updateState(data);
        } catch (e) {
            console.error('[WS] Failed to parse message:', e);
        }
    };

    ws.onclose = () => {
        console.log('[WS] Disconnected');
        const statusIndicator = document.getElementById('statusIndicator');
        if (statusIndicator && statusIndicator.style) statusIndicator.style.color = '#ff4466';
        // Attempt to reconnect
        if (!reconnectInterval) {
            reconnectInterval = setInterval(() => {
                console.log('[WS] Attempting to reconnect...');
                initializeWebSocket();
            }, 5000);
        }
    };

    ws.onerror = (error) => {
        console.error('[WS] Error:', error);
    };
}

// Update UI with state from server
function updateState(state) {
    currentState = state;
    // Show Quick Controls on first WebSocket message
    const quickControls = document.querySelector('.card');
    if (quickControls && quickControls.style && quickControls.style.display === 'none') {
        quickControls.style.display = '';
    }

    // Update controls without triggering events
    const powerToggle = document.getElementById('powerToggle');
    if (powerToggle && powerToggle.checked !== state.power) {
        powerToggle.checked = state.power;
    }

    const brightnessSlider = document.getElementById('brightnessSlider');
    if (brightnessSlider && brightnessSlider.value != state.brightness) {
        brightnessSlider.value = state.brightness;
        const brightnessValue = document.getElementById('brightnessValue');
        if (brightnessValue) brightnessValue.textContent = state.brightness;
    }

    const effectSelect = document.getElementById('effectSelect');
    if (effectSelect && effectSelect.value != state.effect) {
        effectSelect.value = state.effect;
    }

    // Update transition slider and label from state
    if (typeof state.transitionTime !== 'undefined') {
        const transitionSlider = document.getElementById('transitionSlider');
        const transitionSeconds = Math.round(Number(state.transitionTime) / 1000);
        if (transitionSlider && transitionSlider.value != transitionSeconds) {
            transitionSlider.value = transitionSeconds;
        }
        const transitionValue = document.getElementById('transitionValue');
        if (transitionValue) transitionValue.textContent = transitionSeconds;
    }

    if (state.params) {
        const speedSlider = document.getElementById('speedSlider');
        const speedValue = document.getElementById('speedValue');
        if (speedSlider) speedSlider.value = state.params.speed;
        if (speedValue) speedValue.textContent = state.params.speed;

        const intensitySlider = document.getElementById('intensitySlider');
        const intensityValue = document.getElementById('intensityValue');
        if (intensitySlider) intensitySlider.value = state.params.intensity;
        if (intensityValue) intensityValue.textContent = state.params.intensity;

        const color1Picker = document.getElementById('color1Picker');
        if (color1Picker) color1Picker.value = '#' + state.params.color1.toString(16).padStart(6, '0');
        const color2Picker = document.getElementById('color2Picker');
        if (color2Picker) color2Picker.value = '#' + state.params.color2.toString(16).padStart(6, '0');
    }

    // Synchronize clock with backend on first WS message, then advance locally
    if (state.time) {
        if (!clockSynced) {
            const currentTime = document.getElementById('currentTime');
            if (currentTime) currentTime.textContent = state.time;
            // Parse time as HH:MM:SS
            const [h, m, s] = state.time.split(':').map(Number);
            const now = new Date();
            localClock = new Date(now.getFullYear(), now.getMonth(), now.getDate(), h, m, s);
            if (clockInterval) clearInterval(clockInterval);
            clockInterval = setInterval(() => {
                localClock.setSeconds(localClock.getSeconds() + 1);
                const hh = localClock.getHours().toString().padStart(2, '0');
                const mm = localClock.getMinutes().toString().padStart(2, '0');
                const ss = localClock.getSeconds().toString().padStart(2, '0');
                const currentTime = document.getElementById('currentTime');
                if (currentTime) currentTime.textContent = `${hh}:${mm}:${ss}`;
            }, 1000);
            clockSynced = true;
        }
    }

    if (state.sunrise) {
        const sunriseTime = document.getElementById('sunriseTime');
        if (sunriseTime) sunriseTime.textContent = state.sunrise;
    }

    if (state.sunset) {
        const sunsetTime = document.getElementById('sunsetTime');
        if (sunsetTime) sunsetTime.textContent = state.sunset;
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
    const powerToggle = document.getElementById('powerToggle');
    if (powerToggle) {
        powerToggle.addEventListener('change', (e) => {
            sendState({ power: e.target.checked });
        });
    }
    // Brightness slider
    const brightnessSlider = document.getElementById('brightnessSlider');
    if (brightnessSlider) {
        brightnessSlider.addEventListener('input', (e) => {
            const brightnessValue = document.getElementById('brightnessValue');
            if (brightnessValue) brightnessValue.textContent = e.target.value;
        });
        brightnessSlider.addEventListener('change', (e) => {
            sendState({ brightness: parseInt(e.target.value) });
        });
    }
    // Transition time slider
    const transitionSlider = document.getElementById('transitionSlider');
    if (transitionSlider) {
        transitionSlider.addEventListener('input', (e) => {
            const seconds = parseInt(e.target.value);
            const transitionValue = document.getElementById('transitionValue');
            if (transitionValue) transitionValue.textContent = seconds;
        });
        transitionSlider.addEventListener('change', (e) => {
            const seconds = parseInt(e.target.value);
            sendState({ transitionTime: Number(seconds) * 1000 });
        });
    }
    // Effect selector
    const effectSelect = document.getElementById('effectSelect');
    if (effectSelect) {
        effectSelect.addEventListener('change', (e) => {
            sendState({ effect: parseInt(e.target.value) });
        });
    }
    // Speed slider
    const speedSlider = document.getElementById('speedSlider');
    if (speedSlider) {
        let speedTimeout;
        speedSlider.addEventListener('input', (e) => {
            const speedValue = document.getElementById('speedValue');
            if (speedValue) speedValue.textContent = e.target.value;
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
    }
    // Intensity slider
    const intensitySlider = document.getElementById('intensitySlider');
    if (intensitySlider) {
        let intensityTimeout;
        intensitySlider.addEventListener('input', (e) => {
            const intensityValue = document.getElementById('intensityValue');
            if (intensityValue) intensityValue.textContent = e.target.value;
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
    }
    // Color pickers
    const color1Picker = document.getElementById('color1Picker');
    if (color1Picker) {
        color1Picker.addEventListener('change', (e) => {
            const color = parseInt(e.target.value.substring(1), 16);
            sendState({ 
                params: {
                    ...currentState.params,
                    color1: color
                }
            });
        });
    }
    const color2Picker = document.getElementById('color2Picker');
    if (color2Picker) {
        color2Picker.addEventListener('change', (e) => {
            const color = parseInt(e.target.value.substring(1), 16);
            sendState({ 
                params: {
                    ...currentState.params,
                    color2: color
                }
            });
        });
    }
}

// Send state update to server
function sendState(updates) {
    fetch(BASE_URL + '/api/state', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(updates)
    })
    .then(async response => {
        const text = await response.text();
        if (!text) return {};
        try { return JSON.parse(text); } catch { return {}; }
    })
    .then(data => {
        // Treat empty response or missing success property as success
        if (data && typeof data.success !== 'undefined' && !data.success) {
            console.error('Failed to update state');
        }
    })
    .catch(error => console.error('Error:', error));
}

// Load and display presets
function loadPresets() {
    fetch(BASE_URL + '/api/presets')
        .then(async response => {
            const text = await response.text();
            if (!text) return {};
            try { return JSON.parse(text); } catch { return {}; }
        })
        .then(data => {
            presets = data.presets || [];
            displayPresets();
        })
        .catch(error => console.error('Error loading presets:', error));
}

function displayPresets() {
    const grid = document.getElementById('presetGrid');
    if (!grid) return;
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
    fetch(BASE_URL + '/api/preset', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({
            id: presetId,
            apply: true
        })
    })
    .then(async response => {
        const text = await response.text();
        if (!text) return {};
        try { return JSON.parse(text); } catch { return {}; }
    })
    .then(data => {
        if (data.success) {
            console.log('Preset applied:', presetId);
        }
    })
    .catch(error => console.error('Error applying preset:', error));
}

// Load and display timers
function loadTimers() {
    fetch(BASE_URL + '/api/timers')
        .then(async response => {
            const text = await response.text();
            if (!text) return {};
            try { return JSON.parse(text); } catch { return {}; }
        })
        .then(data => {
            timers = data.timers || [];
            displayTimers();
        })
        .catch(error => console.error('Error loading timers:', error));
}

function displayTimers() {
    const list = document.getElementById('timerList');
    if (!list) return;
    list.innerHTML = '';
    
    timers.forEach((timer, index) => {
        const listItem = document.createElement('div');
        listItem.className = 'timer-item';
        listItem.innerHTML = `
            <div class="timer-name">${timer.name}</div>
            <div class="timer-time">${new Date(timer.time).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })}</div>
            <div class="timer-days">${timer.days.join(', ')}</div>
        `;
        
        listItem.addEventListener('click', () => editTimer(index));
        list.appendChild(listItem);
    });
}

function editTimer(timerIndex) {
    const timer = timers[timerIndex];
    if (!timer) return;
    
    document.getElementById('timerName').value = timer.name;
    document.getElementById('timerTime').value = new Date(timer.time).toISOString().substring(11, 16);
    const days = timer.days || [];
    ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat'].forEach((day, index) => {
        const checkbox = document.getElementById(`day-${day}`);
        if (checkbox) checkbox.checked = days.includes(day);
    });
    
    document.getElementById('saveTimerButton').onclick = () => {
        const name = document.getElementById('timerName').value;
        const time = new Date(`1970-01-01T${document.getElementById('timerTime').value}:00Z`).getTime();
        const days = [];
        ['sun', 'mon', 'tue', 'wed', 'thu', 'fri', 'sat'].forEach((day) => {
            const checkbox = document.getElementById(`day-${day}`);
            if (checkbox && checkbox.checked) {
                days.push(day);
            }
        });
        
        saveTimer({ name, time, days });
    };
}

function saveTimer(timer) {
    fetch(BASE_URL + '/api/timer', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(timer)
    })
    .then(async response => {
        const text = await response.text();
        if (!text) return {};
        try { return JSON.parse(text); } catch { return {}; }
    })
    .then(data => {
        if (data.success) {
            console.log('Timer saved');
            loadTimers();
        } else {
            console.error('Failed to save timer');
        }
    })
    .catch(error => console.error('Error saving timer:', error));
}

// Toast notifications
function showToast(message, duration = 3000) {
    const toast = document.createElement('div');
    toast.className = 'toast';
    toast.textContent = message;
    document.body.appendChild(toast);
    
    setTimeout(() => {
        toast.classList.add('show');
    }, 100);
    
    setTimeout(() => {
        toast.classList.remove('show');
        setTimeout(() => {
            document.body.removeChild(toast);
        }, 300);
    }, duration);
}
