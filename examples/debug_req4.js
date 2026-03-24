// Manually trace _resolveFile for express
function _normPath(p) {
    const abs = p.startsWith('/');
    const parts = p.split('/');
    const out = [];
    for (const seg of parts) {
        if (seg === '' || seg === '.') continue;
        if (seg === '..') { if (out.length) out.pop(); }
        else out.push(seg);
    }
    return (abs ? '/' : '') + out.join('/') || '.';
}
function _joinPath(a, b) {
    if (!b) return a;
    if (b.startsWith('/')) return _normPath(b);
    return _normPath(a + '/' + b);
}

const p = './node_modules/express';
console.log('exists:', __fs.existsSync(p));
const st = __fs.statSync(p);
console.log('isFile:', st._isFile, 'isDir:', st._isDir);

const pkgPath = p + '/package.json';
console.log('pkgPath:', pkgPath, 'exists:', __fs.existsSync(pkgPath));

const pkg = JSON.parse(__fs.readFileSync(pkgPath, 'utf8'));
console.log('pkg.main:', pkg.main);
const main = pkg.main || 'index.js';
console.log('main:', main);

const joined = _joinPath(p, main);
console.log('joined:', joined, 'exists:', __fs.existsSync(joined));
