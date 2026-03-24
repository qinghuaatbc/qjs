console.log('dir check:', __fs.existsSync('./node_modules/router'));
console.log('idx check:', __fs.existsSync('./node_modules/router/index.js'));
const st = __fs.statSync('./node_modules/router');
console.log('stat:', JSON.stringify(st));
// Check _resolveFile logic manually
const p = './node_modules/router';
const pkgPath = p + '/package.json';
const pkg = JSON.parse(__fs.readFileSync(pkgPath, 'utf8'));
console.log('main:', pkg.main);
const idx = p + '/index.js';
console.log('idx exists:', __fs.existsSync(idx));
// Try reading it
try {
    const src = __fs.readFileSync('node_modules/router/index.js', 'utf8');
    console.log('router index.js first 100 chars:', src.substring(0, 100));
} catch(e) { console.log('read error:', e.message); }
