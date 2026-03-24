// Test each of express's direct deps
const deps = ['body-parser', 'merge-descriptors', 'router'];
for (const d of deps) {
    try { require(d); console.log(d, 'OK'); }
    catch(e) { console.log(d, 'FAIL:', e.message); }
}
// Try loading express file directly
try {
    const src = __fs.readFileSync('node_modules/express/index.js', 'utf8');
    console.log('index.js content:', src);
} catch(e) { console.log('read fail:', e.message); }
