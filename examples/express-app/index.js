const express = require('express');
const fs = require('fs');
const path = require('path');
const app = express();

const html = fs.readFileSync(path.join(__dirname, 'public/index.html'), 'utf8');

app.use(express.json());

app.get('/', (_req, res) => {
    res.setHeader('Content-Type', 'text/html');
    res.send(html);
});

app.get('/api/info', (_req, res) => {
    res.json({ runtime: __runtime_version, uptime: Math.floor(process.uptime()) });
});

app.get('/greet/:name', (req, res) => {
    res.json({ greeting: `Hello, ${req.params.name}!` });
});

app.post('/echo', (req, res) => {
    res.json({ received: req.body });
});

app.listen(3000, () => {
    console.log('Express server running at http://localhost:3000');
});
