const routerDeps = ['is-promise', 'parseurl', 'debug'];
for (const d of routerDeps) {
    try { require(d); console.log(d, 'OK'); }
    catch(e) { console.log(d, 'FAIL:', e.message); }
}
// Check http.METHODS
const http = require('http');
console.log('http.METHODS:', http.METHODS);
// Test node:http METHODS
try {
    const { METHODS } = require('node:http');
    console.log('METHODS:', METHODS ? 'exists' : 'undefined');
} catch(e) { console.log('node:http METHODS fail:', e.message); }
