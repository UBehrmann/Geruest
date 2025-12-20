// Sub-subfolder test utility functions
const NestedUtils = {
    getDepthIndicator(level) {
        const indicators = {
            1: '📁',
            2: '📂',
            3: '🗂️',
            4: '📋'
        };
        return indicators[level] || '📄';
    },
    
    formatPath(parts) {
        return parts.join(' > ');
    },
    
    calculateNestingLevel() {
        const path = window.location.pathname;
        return (path.match(/\//g) || []).length;
    }
};

console.log('NestedUtils loaded from deeply nested subfolder!');
console.log('Current nesting level:', NestedUtils.calculateNestingLevel());
