try {
    require('express');
    console.log('express OK!');
} catch(e) {
    console.log('Error:', e.message);
    if (e.stack) {
        const lines = e.stack.split('\n').slice(0,5);
        console.log(lines.join('\n'));
    }
}
