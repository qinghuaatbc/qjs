try {
    const debug = require('debug');
    console.log('debug type:', typeof debug);
    const d = debug('router');
    console.log('debug instance type:', typeof d);
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack);
}
