// Check global Buffer
console.log('globalThis.Buffer:', typeof globalThis.Buffer);
console.log('Buffer:', typeof Buffer);
// Check if Buffer.isBuffer works
console.log('Buffer.isBuffer:', typeof Buffer.isBuffer);
const b = Buffer.from('test');
console.log('Buffer.isBuffer(b):', Buffer.isBuffer(b));
// What iconv does:
try {
    const iconv = require('iconv-lite');
    const dec = iconv.getDecoder('utf8');
    // Set a breakpoint manually
    const origWrite = dec.write;
    dec.write = function(buf) {
        console.log('dec.write called, buf type:', typeof buf, Buffer.isBuffer(buf));
        return origWrite.call(this, buf);
    };
    const r = dec.write('hello');
    console.log('result:', r);
} catch(e) {
    console.log('Error:', e.message, '\n', e.stack.split('\n').slice(0,6).join('\n'));
}
