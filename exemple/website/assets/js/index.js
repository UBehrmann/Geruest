
function showResult(data) {
    const resultDiv = document.getElementById('result');
    resultDiv.textContent = typeof data === 'string' ? data : JSON.stringify(data, null, 2);
}

async function sendRequest(method, url, body = null) {
    try {
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
        showResult(data);
    } catch (err) {
        showResult('Error: ' + err);
    }
}

document.getElementById('getBtn').onclick = () => {
    sendRequest('GET', 'http://localhost/api/get');
};

document.getElementById('postBtn').onclick = () => {
    const body = document.getElementById('requestBody').value;
    sendRequest('POST', 'http://localhost/api/post', body);
};

document.getElementById('putBtn').onclick = () => {
    const body = document.getElementById('requestBody').value;
    sendRequest('PUT', 'http://localhost/api/put', body);
};

document.getElementById('deleteBtn').onclick = () => {
    sendRequest('DELETE', 'http://localhost/api/delete');
};
