try {
    const buffer = require('buffer');
    console.log('buffer.Buffer:', typeof buffer.Buffer);
    const safer = require('safer-buffer');
    console.log('safer.Buffer:', typeof safer.Buffer);
    const B = safer.Buffer;
    console.log('safer.Buffer.from:', typeof B.from);
    console.log('safer.Buffer.isBuffer:', typeof B.isBuffer);
    const r = B.from('hello');
    console.log('from result:', r.constructor.name, r.length);
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack.split('\n').slice(0,6).join('\n'));
}
