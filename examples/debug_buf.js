const buf = Buffer.from('hello world');
console.log('buf type:', buf.constructor.name, typeof buf);
console.log('isBuffer:', Buffer.isBuffer(buf));
try {
    const str = buf.toString('utf8');
    console.log('toString ok:', str);
} catch(e) { console.log('toString fail:', e.message, '\n', e.stack.split('\n').slice(0,4).join('\n')); }

// Test StringDecoder
const { StringDecoder } = require('string_decoder');
try {
    const sd = new StringDecoder('utf8');
    console.log('StringDecoder created, write type:', typeof sd.write);
    const r = sd.write(buf);
    console.log('StringDecoder.write ok:', r);
} catch(e) { console.log('StringDecoder.write fail:', e.message, '\n', e.stack.split('\n').slice(0,5).join('\n')); }
