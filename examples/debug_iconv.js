try {
    const iconv = require('iconv-lite');
    console.log('iconv type:', typeof iconv);
    const decoder = iconv.getDecoder('utf8');
    console.log('decoder type:', typeof decoder, Object.keys(decoder||{}));
    const result = decoder.write(Buffer.from('hello'));
    console.log('decode result:', result);
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack.split('\n').slice(0,6).join('\n'));
}
