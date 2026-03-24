console.log('exists check:', __fs.existsSync('./node_modules/express'));
const st = __fs.statSync('./node_modules/express');
console.log('stat:', JSON.stringify(st));
console.log('pkg exists:', __fs.existsSync('./node_modules/express/package.json'));
console.log('idx exists:', __fs.existsSync('./node_modules/express/index.js'));
const pkg = JSON.parse(__fs.readFileSync('./node_modules/express/package.json', 'utf8'));
console.log('pkg.main:', pkg.main);
