/**
 * API client for making HTTP requests
 */

const ApiClient = {
    baseUrl: '',
    
    async request(method, path) {
        log(`Making ${method} request to ${path}`, 'api');
        
        try {
            const response = await fetch(path, {
                method: method,
                headers: {
                    'Content-Type': 'application/json'
                }
            });
            
            const data = await response.json();
            log(`Response received: ${response.status}`, 'api');
            return data;
        } catch (error) {
            log(`Request failed: ${error.message}`, 'error');
            throw error;
        }
    },
    
    get(path) {
        return this.request('GET', path);
    },
    
    post(path) {
        return this.request('POST', path);
    },
    
    put(path) {
        return this.request('PUT', path);
    },
    
    delete(path) {
        return this.request('DELETE', path);
    }
};
