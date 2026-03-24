console.log('__dirname:', __dirname);
try {
    const express = require('express');
    console.log('express loaded OK');
} catch(e) {
    console.log('Error:', e.message);
    // trace what's failing
    const stack = e.stack || '';
    console.log(stack.split('\n').slice(0,6).join('\n'));
}
