try {
    const iconv = require('iconv-lite');
    console.log('iconv ok');
    const decoder = iconv.getDecoder('utf8');
    console.log('decoder:', typeof decoder, Object.keys(decoder||{}));
    try {
        const r = decoder.write('hello world');
        console.log('write string ok:', r);
    } catch(e) { console.log('write string fail:', e.message, '\n', e.stack.split('\n').slice(0,5).join('\n')); }
    try {
        const buf = Buffer.from('hello world');
        console.log('buffer created:', buf.constructor.name, buf.length);
        const r2 = decoder.write(buf);
        console.log('write buffer ok:', r2);
    } catch(e) { console.log('write buffer fail:', e.message, '\n', e.stack.split('\n').slice(0,5).join('\n')); }
} catch(e) {
    console.log('Error:', e.message, '\n', e.stack.split('\n').slice(0,5).join('\n'));
}
