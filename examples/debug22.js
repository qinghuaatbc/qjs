const express = require('express');
const app = express();
app.get('/', (req, res) => res.json({ok: true}));
try {
    const server = app.listen(3000, () => console.log('listening'));
    console.log('server:', typeof server);
} catch(e) {
    console.log('listen error:', e.message);
    console.log(e.stack.split('\n').slice(0,6).join('\n'));
}
