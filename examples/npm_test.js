// Test npm-style require
const { chunk, sum } = require('lodash-like');
console.log('chunk([1,2,3,4,5], 2):', JSON.stringify(chunk([1,2,3,4,5], 2)));
console.log('sum([1,2,3,4]):', sum([1,2,3,4]));

// Test relative require
const helper = require('./npm_helper');
console.log('helper.double(7):', helper.double(7));

// Test built-in still works
const path = require('path');
console.log('path.join("a","b","c"):', path.join('a','b','c'));

console.log('All npm tests passed!');
