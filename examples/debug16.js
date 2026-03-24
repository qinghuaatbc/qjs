// Test loading accepts directly
try {
    const src = __fs.readFileSync('node_modules/accepts/index.js', 'utf8');
    const wrapped = '(function(module, exports, require, __dirname, __filename) {\n' + src + '\n})';
    const fn = eval(wrapped);
    console.log('eval OK, fn type:', typeof fn);
} catch(e) {
    console.log('eval FAIL:', e.message, 'at', e.fileName, e.lineNumber);
}
