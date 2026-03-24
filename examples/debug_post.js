const express = require('express');
const app = express();

// Test raw body-parser behavior
app.use((req, res, next) => {
    console.log('Before body-parser, method:', req.method, 'has on:', typeof req.on);
    next();
});

app.use(express.json());

app.use((req, res, next) => {
    console.log('After body-parser, body:', JSON.stringify(req.body));
    next();
});

app.post('/echo', (req, res) => {
    console.log('In handler, body:', JSON.stringify(req.body));
    res.json({ received: req.body });
});

app.listen(3000, () => console.log('listening'));
