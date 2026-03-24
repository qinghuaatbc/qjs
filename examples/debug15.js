const src = __fs.readFileSync('node_modules/accepts/index.js', 'utf8');
const lines = src.split('\n');
// Find which line is 3 in the eval'd form (after the wrapper)
// wrapper adds: (function(module, exports, require, __dirname, __filename) {\n
// so line 3 in <input> = line 2 in file
console.log('Line 1:', lines[0]);
console.log('Line 2:', lines[1]);
console.log('Line 3:', lines[2]);
