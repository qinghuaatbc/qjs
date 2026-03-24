const http = require('http');
const server = http.createServer((req, res) => {});
console.log('server type:', typeof server);
console.log('server.listen type:', typeof server.listen);
console.log('server keys:', Object.keys(server));
