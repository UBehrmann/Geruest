// Device utility functions
const DeviceUtils = {
    formatLastSeen(timestamp) {
        const now = Date.now();
        const diff = now - timestamp;
        const minutes = Math.floor(diff / 60000);
        
        if (minutes < 1) return 'Just now';
        if (minutes < 60) return `${minutes}m ago`;
        
        const hours = Math.floor(minutes / 60);
        if (hours < 24) return `${hours}h ago`;
        
        const days = Math.floor(hours / 24);
        return `${days}d ago`;
    },
    
    getDeviceIcon(type) {
        const icons = {
            'phone': '📱',
            'tablet': '📲',
            'laptop': '💻',
            'desktop': '🖥️',
            'watch': '⌚',
            'tv': '📺'
        };
        return icons[type] || '🔧';
    }
};

console.log('DeviceUtils loaded from subfolder!');
