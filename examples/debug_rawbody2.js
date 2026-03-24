const iconv = require('iconv-lite');
const dec = iconv.getDecoder('utf8');
try {
    const r = dec.write('{"hello":"world"}');
    console.log('decode ok:', r);
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack.split('\n').slice(0,5).join('\n'));
}
