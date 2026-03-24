// Patch _loadFile to log what file fails
const origLoad = globalThis._loadFile_debug;
// Instead, manually walk express deps
const exDeps = ['body-parser', 'accepts', 'type-is', 'finalhandler', 'on-finished', 
                'serve-static', 'send', 'setprototypeof', 'statuses', 'vary',
                'encodeurl', 'escape-html', 'etag', 'fresh', 'content-type',
                'content-disposition', 'proxy-addr', 'parseurl', 'qs',
                'range-parser', 'router', 'merge-descriptors', 'methods',
                'path-to-regexp', 'array-flatten'];
for (const d of exDeps) {
    try { require(d); console.log(d, 'OK'); }
    catch(e) { console.log(d, 'FAIL:', e.message); }
}
