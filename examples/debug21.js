try {
    const express = require('express');
    console.log('express loaded OK, type:', typeof express);
    const app = express();
    console.log('app created OK');
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack.split('\n').slice(0,6).join('\n'));
}
