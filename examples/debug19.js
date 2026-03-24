try {
    require('express');
    console.log('express OK');
} catch(e) {
    console.log('Error:', e.message);
    console.log(e.stack.split('\n').slice(0,8).join('\n'));
}
