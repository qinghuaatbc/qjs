// Load express deps one by one
const expressDeps = [
    ['body-parser', null],
    ['node:events', null],
    ['merge-descriptors', null],
    ['./node_modules/express/lib/application', null],
    ['router', null],
    ['./node_modules/express/lib/request', null],
    ['./node_modules/express/lib/response', null],
];
for (const [d] of expressDeps) {
    try { require(d); console.log(d, 'OK'); }
    catch(e) { console.log(d, 'FAIL:', e.message, '\n  at', e.stack.split('\n')[1]); }
}
