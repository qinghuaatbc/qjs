try {
    require('events');
    console.log('events OK');
} catch(e) { console.log('events fail:', e.message); }
try {
    require('node:events');
    console.log('node:events OK');
} catch(e) { console.log('node:events fail:', e.message); }
try {
    const express = require('express');
    console.log('express OK');
} catch(e) { 
    console.log('express fail:', e.message);
    if (e.stack) console.log(e.stack.split('\n').slice(0,8).join('\n'));
}
