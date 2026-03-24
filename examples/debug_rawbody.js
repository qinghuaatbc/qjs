const iconv = require('iconv-lite');

// Replicate what raw-body does
const decoder = iconv.getDecoder('utf8');
console.log('decoder type:', typeof decoder);
console.log('decoder constructor:', decoder && decoder.constructor && decoder.constructor.name);
console.log('decoder.write type:', decoder && typeof decoder.write);

// Try calling decoder.write
try {
    const r = decoder.write('{"hello":"world"}');
    console.log('write result:', r);
} catch(e) {
    console.log('write error:', e.message);
    console.log(e.stack.split('\n').slice(0,8).join('\n'));
}
