const express = require('express');
const app = express();
app.use(express.json());

app.get('/', (req, res) => res.json({ ok: true }));

app.get('/greet/:name', (req, res) => {
    try {
        console.log('params:', JSON.stringify(req.params));
        res.json({ greeting: `Hello, ${req.params.name}!` });
    } catch(e) {
        console.error('ERROR in /greet:', e.message);
        console.error(e.stack);
        res.status(500).send(e.message);
    }
});

app.use((err, req, res, next) => {
    console.error('Express error:', err.message);
    res.status(500).send(err.message);
});

app.listen(3001, () => console.log('listening on 3001'));
