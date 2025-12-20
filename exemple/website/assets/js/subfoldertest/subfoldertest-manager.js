// Device management functionality
class DeviceManager {
    constructor() {
        this.devices = [];
        console.log('DeviceManager initialized from subfolder!');
    }
    
    addDevice(device) {
        this.devices.push({
            id: Math.random().toString(36).substr(2, 9),
            name: device.name,
            type: device.type,
            status: device.status || 'offline',
            lastSeen: Date.now()
        });
    }
    
    renderDevices() {
        const container = document.getElementById('device-list');
        if (!container) return;
        
        container.innerHTML = this.devices.map(device => `
            <div class="device-card">
                <div style="font-size: 3em;">${DeviceUtils.getDeviceIcon(device.type)}</div>
                <h3>${device.name}</h3>
                <p>Type: ${device.type}</p>
                <span class="device-status status-${device.status}">${device.status}</span>
                <p style="margin-top: 10px; color: #666; font-size: 0.9em;">
                    Last seen: ${DeviceUtils.formatLastSeen(device.lastSeen)}
                </p>
            </div>
        `).join('');
    }
}

const deviceManager = new DeviceManager();
