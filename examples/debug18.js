try { require('send'); console.log('send OK'); }
catch(e) { console.log('send FAIL:', e.message); console.log(e.stack.split('\n').slice(0,5).join('\n')); }
try { require('methods'); console.log('methods OK'); }
catch(e) { console.log('methods FAIL:', e.message); }
try { require('array-flatten'); console.log('array-flatten OK'); }
catch(e) { console.log('array-flatten FAIL:', e.message); }
