// Intercept the inner require to trace errors
const origReq = globalThis.require;
globalThis.require = function(id) {
    try {
        const r = origReq(id);
        // console.log('required:', id, 'OK');
        return r;
    } catch(e) {
        console.log('require FAIL:', id, '->', e.message);
        throw e;
    }
};
try {
    require('router');
    console.log('router OK');
} catch(e) {
    console.log('router FAIL:', e.message);
}
