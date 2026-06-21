// Sub-subfolder test manager
class NestedManager {
    constructor() {
        this.depth = NestedUtils.calculateNestingLevel();
        console.log('NestedManager initialized at depth:', this.depth);
    }
    
    displayPathInfo() {
        const container = document.getElementById('path-info');
        if (!container) return;
        
        const pathParts = ['Root', 'SubfolderTest', 'SubSubfolderTest'];
        const icon = NestedUtils.getDepthIndicator(this.depth);
        
        container.innerHTML = `
            <div class="nested-info">
                <h3>${icon} Current Location</h3>
                <p><strong>Path:</strong> ${NestedUtils.formatPath(pathParts)}</p>
                <p><strong>Nesting Depth:</strong> ${this.depth} levels</p>
                <p><strong>Page URL:</strong> <code>${window.location.pathname}</code></p>
            </div>
        `;
    }
    
    showTestResults() {
        const container = document.getElementById('test-results');
        if (!container) return;
        
        const cssLoaded = document.querySelectorAll('link[rel="stylesheet"]').length > 0;
        const jsLoaded = typeof NestedUtils !== 'undefined';
        
        container.innerHTML = `
            <div class="nested-card">
                <h3>✅ Asset Loading Status</h3>
                <p>CSS Files: ${cssLoaded ? '✅ Loaded' : '❌ Failed'}</p>
                <p>JS Files: ${jsLoaded ? '✅ Loaded' : '❌ Failed'}</p>
                <p>Nesting works: ${cssLoaded && jsLoaded ? '🎉 Perfect!' : '⚠️ Check console'}</p>
            </div>
        `;
    }
}

const nestedManager = new NestedManager();
