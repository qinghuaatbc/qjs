// Patch require to log errors from loaded modules
const origMakeReq = globalThis._makeRequire_patched;

try {
    // Test raw body reading directly
    const rawBody = require('raw-body');
    console.log('raw-body type:', typeof rawBody);
    
    // Simulate what body-parser does
    const fakeReq = {
        headers: { 'content-type': 'application/json', 'content-length': '17' },
        _events: Object.create(null),
        on(ev, fn) {
            if (!this._events[ev]) this._events[ev] = [];
            this._events[ev].push(fn);
            if ((ev === 'data' || ev === 'end') && !this._scheduled) {
                this._scheduled = true;
                setTimeout(() => {
                    (this._events['data']||[]).forEach(f => f('{"hello":"world"}'));
                    (this._events['end']||[]).forEach(f => f());
                }, 0);
            }
            return this;
        },
        once(ev, fn) { return this.on(ev, fn); },
        removeListener(ev, fn) { return this; },
        emit(ev, ...a) { (this._events[ev]||[]).forEach(f=>f(...a)); return true; },
        readable: true
    };
    
    rawBody(fakeReq, { length: 17, limit: '100kb', encoding: 'utf8' }, (err, body) => {
        if (err) console.log('raw-body error:', err.message, '\n', err.stack);
        else console.log('raw-body result:', body);
    });
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack.split('\n').slice(0,8).join('\n'));
}
