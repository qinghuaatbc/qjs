const http = require('http');

const PORT = 3000;

const server = http.createServer((req, res) => {
    console.log(`${req.method} ${req.url}`);

    // Router
    if (req.pathname === '/' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end(`
<!DOCTYPE html>
<html>
<head><title>JSRuntime HTTP Server</title></head>
<body>
  <h1>Hello from JSRuntime!</h1>
  <p>Built on <strong>QuickJS</strong></p>
  <ul>
    <li><a href="/api/hello">GET /api/hello</a> — JSON greeting</li>
    <li><a href="/api/time">GET /api/time</a>  — current timestamp</li>
  </ul>
</body>
</html>`);

    } else if (req.pathname === '/api/hello' && req.method === 'GET') {
        res.json({ message: 'Hello, World!', runtime: __runtime_version });

    } else if (req.pathname === '/api/time' && req.method === 'GET') {
        res.json({ timestamp: Date.now(), iso: new Date().toISOString() });

    } else if (req.pathname === '/api/echo' && req.method === 'POST') {
        // Echo the request body back as JSON
        res.json({
            method:  req.method,
            url:     req.url,
            headers: req.headers,
            body:    req.body,
        });

    } else {
        res.writeHead(404, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Not Found', path: req.pathname }));
    }
});

server.listen(PORT, () => {
    console.log(`Server running at http://localhost:${PORT}/`);
    console.log('Press Ctrl+C to stop');
});
