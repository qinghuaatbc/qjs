// File system example
const fs = require('fs');

const tmpFile = '/tmp/jsr_test.txt';

// Write
fs.writeFileSync(tmpFile, 'Hello from JSRuntime!\nLine 2\nLine 3\n');
console.log('Written to', tmpFile);

// Read
const content = fs.readFileSync(tmpFile, 'utf8');
console.log('Contents:');
console.log(content);

// Exists
console.log('exists:', fs.existsSync(tmpFile));
console.log('not exists:', fs.existsSync('/tmp/nope_jsr.txt'));

// Stat
const st = fs.statSync(tmpFile);
console.log('size:', st.size, 'bytes');
console.log('isFile:', st.isFile());
console.log('isDir:', st.isDirectory());

// Readdir
const entries = fs.readdirSync('/tmp').slice(0, 5);
console.log('First 5 /tmp entries:', entries);

// Unlink
fs.unlinkSync(tmpFile);
console.log('Deleted:', !fs.existsSync(tmpFile));
