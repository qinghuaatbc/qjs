// Test path normalization
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
const joined = _normPath('./node_modules/express' + '/' + 'index.js');
console.log('joined:', joined);
console.log('exists with ./:', __fs.existsSync('./node_modules/express/index.js'));
console.log('exists without ./:', __fs.existsSync('node_modules/express/index.js'));
