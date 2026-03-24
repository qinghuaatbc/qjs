const express = require('express');
const path = require('path');
console.log('__dirname:', __dirname);
try { console.log('uptime:', process.uptime()); } catch(e) { console.log('uptime err:', e.message); }
try {
    const mw = express.static(path.join(__dirname, 'public'));
    console.log('static OK, type:', typeof mw);
} catch(e) { console.log('static err:', e.message, '\n', e.stack.split('\n').slice(0,5).join('\n')); }
