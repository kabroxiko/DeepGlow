// config.js
// Handles configuration page logic for config.html

// Example: load and save configuration using localStorage or API
function saveConfiguration() {
    // Collect values from form
    const config = {
        ledCount: document.getElementById('ledCount').value,
        ledType: document.getElementById('ledType').value,
        relayPin: document.getElementById('relayPin').value,
        relayActiveHigh: document.getElementById('relayActiveHigh').value,
        maxBrightness: document.getElementById('maxBrightness').value,
        minTransition: document.getElementById('minTransition').value,
        timezoneOffset: document.getElementById('timezoneOffset').value,
        latitude: document.getElementById('latitude').value,
        longitude: document.getElementById('longitude').value
    };
    // TODO: Replace with actual API call if needed
    localStorage.setItem('aquariumConfig', JSON.stringify(config));
    alert('Configuration saved!');
}

// Configuration page logic for Aquarium LED Controller

function loadConfig() {
    fetch(BASE_URL + '/api/config')
        .then(async response => {
            const text = await response.text();
            if (!text) return {};
            try { return JSON.parse(text); } catch { return {}; }
        })
        .then(data => {
            config = data;
            displayConfig();
        })
        .catch(error => console.error('Error loading config:', error));
}

function displayConfig() {
    // LED
    if (config.led) {
        if (config.led.count !== undefined) document.getElementById('ledCount').value = config.led.count;
        if (config.led.type) document.getElementById('ledType').value = config.led.type;
        if (config.led.relayPin !== undefined) document.getElementById('relayPin').value = config.led.relayPin;
        if (typeof config.led.relayActiveHigh !== 'undefined') document.getElementById('relayActiveHigh').value = String(config.led.relayActiveHigh);
    }
    // Safety
    if (config.safety) {
        if (config.safety.maxBrightness !== undefined) {
            // Convert hardware value (1–255) to percent
            const percent = Math.round(((config.safety.maxBrightness - 1) / 254) * 100);
            document.getElementById('maxBrightness').value = percent;
            document.getElementById('maxBrightnessValue').textContent = percent + '%';
        }
        if (config.safety.minTransitionTime !== undefined) {
            const minTransSeconds = Math.floor(Number(config.safety.minTransitionTime) / 1000);
            document.getElementById('minTransition').value = minTransSeconds;
            document.getElementById('minTransitionValue').textContent = minTransSeconds;
        }
    }
    // Time
    if (config.time) {
        if (config.time.timezone) document.getElementById('timezone').value = config.time.timezone;
        if (config.time.latitude !== undefined) document.getElementById('latitude').value = config.time.latitude;
        if (config.time.longitude !== undefined) document.getElementById('longitude').value = config.time.longitude;
    }
}

// Optionally, load config on page load
window.addEventListener('DOMContentLoaded', () => {
    const config = JSON.parse(localStorage.getItem('aquariumConfig') || '{}');
    if (config.ledCount) document.getElementById('ledCount').value = config.ledCount;
    if (config.ledType) document.getElementById('ledType').value = config.ledType;
    if (config.relayPin) document.getElementById('relayPin').value = config.relayPin;
    if (config.relayActiveHigh) document.getElementById('relayActiveHigh').value = config.relayActiveHigh;
    if (config.maxBrightness) document.getElementById('maxBrightness').value = config.maxBrightness;
    if (config.minTransition) document.getElementById('minTransition').value = config.minTransition;
    if (config.timezone) document.getElementById('timezone').value = config.timezone;
    // GPS button for location
    const getLocationBtn = document.getElementById('getLocationBtn');
    if (getLocationBtn) {
        getLocationBtn.addEventListener('click', () => {
            if (navigator.geolocation) {
                getLocationBtn.disabled = true;
                getLocationBtn.textContent = 'Locating...';
                navigator.geolocation.getCurrentPosition(
                    (pos) => {
                        document.getElementById('latitude').value = pos.coords.latitude.toFixed(6);
                        document.getElementById('longitude').value = pos.coords.longitude.toFixed(6);
                        getLocationBtn.textContent = 'Get from GPS';
                        getLocationBtn.disabled = false;
                    },
                    (err) => {
                        alert('Could not get location: ' + err.message);
                        getLocationBtn.textContent = 'Get from GPS';
                        getLocationBtn.disabled = false;
                    }
                );
            } else {
                alert('Geolocation not supported');
            }
        });
    }
    if (config.latitude) document.getElementById('latitude').value = config.latitude;
    if (config.longitude) document.getElementById('longitude').value = config.longitude;
    // Update display values if needed
    document.getElementById('maxBrightnessValue').textContent = document.getElementById('maxBrightness').value + '%';
    document.getElementById('minTransitionValue').textContent = document.getElementById('minTransition').value;

    // Only run config logic if config elements exist (i.e., on config.html)
    if (document.getElementById('ledCount')) {
        loadConfig();
        const maxBrightness = document.getElementById('maxBrightness');
        const maxBrightnessValue = document.getElementById('maxBrightnessValue');
        if (maxBrightness && maxBrightnessValue) {
            maxBrightness.addEventListener('input', (e) => {
                maxBrightnessValue.textContent = e.target.value + '%';
            });
        }
        const minTransition = document.getElementById('minTransition');
        const minTransitionValue = document.getElementById('minTransitionValue');
        if (minTransition && minTransitionValue) {
            minTransition.addEventListener('input', (e) => {
                minTransitionValue.textContent = e.target.value;
            });
        }
    }

    // Save config button
    const saveConfigButton = document.getElementById('saveConfigButton');
    if (saveConfigButton) {
        saveConfigButton.addEventListener('click', () => {
            // Build configUpdate with nested structure
            const configUpdate = {};
            // LED
            const ledUpdate = {};
            const ledCount = parseInt(document.getElementById('ledCount').value);
            ledUpdate.count = ledCount;
            ledUpdate.type = document.getElementById('ledType').value;
            ledUpdate.relayPin = parseInt(document.getElementById('relayPin').value);
            ledUpdate.relayActiveHigh = document.getElementById('relayActiveHigh').value === 'true';
            configUpdate.led = ledUpdate;
            // Safety
            const safetyUpdate = {};
            // Convert percent (0–100) to hardware value (1–255)
            const percent = parseInt(document.getElementById('maxBrightness').value);
            safetyUpdate.maxBrightness = percent <= 0 ? 1 : percent >= 100 ? 255 : Math.round(1 + (254 * percent / 100));
            safetyUpdate.minTransitionTime = Number(document.getElementById('minTransition').value) * 1000;
            configUpdate.safety = safetyUpdate;
            // Time
            const timeUpdate = {};
            timeUpdate.timezone = document.getElementById('timezone').value;
            timeUpdate.latitude = parseFloat(document.getElementById('latitude').value);
            timeUpdate.longitude = parseFloat(document.getElementById('longitude').value);
            configUpdate.time = timeUpdate;
            fetch(BASE_URL + '/api/config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify(configUpdate)
            })
            .then(async response => {
                const text = await response.text();
                if (!text) return {};
                try { return JSON.parse(text); } catch { return {}; }
            })
            .then(data => {
                if (data.success === undefined || data.success) {
                    console.log('Config saved');
                    loadConfig();
                } else {
                    console.error('Failed to save config');
                }
            })
            .catch(error => console.error('Error saving config:', error));
        });
    }

    // System commands
    const rebootButton = document.getElementById('rebootButton');
    const updateButton = document.getElementById('updateButton');
    const systemStatus = document.getElementById('systemStatus');
    if (rebootButton) {
        rebootButton.addEventListener('click', async () => {
            rebootButton.disabled = true;
            if (systemStatus) systemStatus.textContent = 'Rebooting...';
            try {
                const result = await sendCommandWithStatus('reboot');
                if (result.success && result.message && result.message.toLowerCase().includes('reboot')) {
                    systemStatus.textContent = 'Device rebooting...';
                    setTimeout(() => { systemStatus.textContent = ''; }, 5000);
                } else if (result.error) {
                    systemStatus.textContent = 'Failed to reboot: ' + result.error;
                } else {
                    systemStatus.textContent = 'Failed to reboot';
                }
            } catch (e) {
                systemStatus.textContent = 'Error: ' + e.message;
            }
            rebootButton.disabled = false;
        });
    }
    if (updateButton) {
        updateButton.addEventListener('click', async () => {
            updateButton.disabled = true;
            if (systemStatus) systemStatus.textContent = 'Checking for updates...';
            try {
                const result = await sendCommandWithStatus('update');
                if (result.success && result.message && result.message.toLowerCase().includes('update')) {
                    systemStatus.textContent = 'Update started!';
                } else if (result.error) {
                    systemStatus.textContent = 'Update failed: ' + result.error;
                } else {
                    systemStatus.textContent = 'No update or failed.';
                }
            } catch (e) {
                systemStatus.textContent = 'Error: ' + e.message;
            }
            setTimeout(() => { systemStatus.textContent = ''; }, 5000);
            updateButton.disabled = false;
        });
    }
});

// Update display values for sliders
['maxBrightness', 'minTransition'].forEach(id => {
    const input = document.getElementById(id);
    if (input) {
        input.addEventListener('input', e => {
            document.getElementById(id + 'Value').textContent = e.target.value;
        });
    }
});


async function sendCommandWithStatus(command) {
    try {
        const response = await fetch(BASE_URL + '/api/command', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command })
        });
        const text = await response.text();
        if (!text) return { success: false };
        try {
            return JSON.parse(text);
        } catch {
            return { success: false };
        }
    } catch (e) {
        return { success: false, error: e.message };
    }
}
