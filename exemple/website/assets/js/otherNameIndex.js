
function showResult(data) {
    const resultDiv = document.getElementById('result');
    resultDiv.textContent = typeof data === 'string' ? data : JSON.stringify(data, null, 2);
}

async function sendRequest(method, url, body = null) {
    try {
        console.log(`${method} ${url}${body ? ` with body: ${body}` : ''}`);
        const options = {
            method,
            headers: { 'Content-Type': 'application/json' },
        };
        if (body && (method === 'POST' || method === 'PUT')) {
            options.body = body;
        }
        const response = await fetch(url, options);
        const text = await response.text();
        let data;
        try { data = JSON.parse(text); } catch { data = text; }
        
        // Show both status and response
        const statusInfo = `Status: ${response.status} ${response.statusText}\nURL: ${url}\n\n`;
        showResult(statusInfo + (typeof data === 'string' ? data : JSON.stringify(data, null, 2)));
    } catch (err) {
        showResult('Error: ' + err);
    }
}

// Exact route handlers
document.getElementById('getBtn').onclick = () => {
    sendRequest('GET', '/api/get');
};

document.getElementById('postBtn').onclick = () => {
    const body = document.getElementById('requestBody').value;
    sendRequest('POST', '/api/post', body);
};

document.getElementById('putBtn').onclick = () => {
    const body = document.getElementById('requestBody').value;
    sendRequest('PUT', '/api/put', body);
};

document.getElementById('deleteBtn').onclick = () => {
    sendRequest('DELETE', '/api/delete');
};

document.getElementById('testBtn').onclick = () => {
    sendRequest('GET', '/test');
};

// Wildcard route handlers - API wildcard (/api/*)
document.getElementById('wildcardApiBtn1').onclick = () => {
    sendRequest('GET', '/api/unknown');
};

document.getElementById('wildcardApiBtn2').onclick = () => {
    sendRequest('GET', '/api/v1/users');
};

document.getElementById('wildcardApiBtn3').onclick = () => {
    sendRequest('GET', '/api/anything/else');
};

// User profile pattern (/users/*/profile)
document.getElementById('userProfileBtn1').onclick = () => {
    sendRequest('GET', '/users/123/profile');
};

document.getElementById('userProfileBtn2').onclick = () => {
    sendRequest('GET', '/users/john/profile');
};

document.getElementById('userProfileBtn3').onclick = () => {
    sendRequest('GET', '/users/admin/profile');
};

// ZIP download pattern (/downloads/*.zip)
document.getElementById('zipDownloadBtn1').onclick = () => {
    sendRequest('GET', '/downloads/file.zip');
};

document.getElementById('zipDownloadBtn2').onclick = () => {
    sendRequest('GET', '/downloads/archive.zip');
};

document.getElementById('zipDownloadBtn3').onclick = () => {
    sendRequest('GET', '/downloads/app-v1.2.zip');
};

// Static images pattern (/static/*/images/*)
document.getElementById('staticImageBtn1').onclick = () => {
    sendRequest('GET', '/static/v1/images/logo.png');
};

document.getElementById('staticImageBtn2').onclick = () => {
    sendRequest('GET', '/static/v2/images/header.jpg');
};

document.getElementById('staticImageBtn3').onclick = () => {
    sendRequest('GET', '/static/latest/images/favicon.ico');
};

// Non-matching tests
document.getElementById('noMatchBtn1').onclick = () => {
    sendRequest('GET', '/users/profile');
};

document.getElementById('noMatchBtn2').onclick = () => {
    sendRequest('GET', '/downloads/file.txt');
};

document.getElementById('noMatchBtn3').onclick = () => {
    sendRequest('GET', '/nonexistent/path');
};
