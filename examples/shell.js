const { execSync, spawnSync, exec } = require('child_process');

// execSync — returns stdout as string
const uname = execSync('uname -a');
console.log('uname:', uname.trim());

// list files
const files = execSync('ls examples/');
console.log('examples/:\n' + files.trim());

// spawnSync — command + args array
const result = spawnSync('echo', ['hello', 'from', 'spawnSync']);
console.log('spawnSync:', result.stdout.trim());

// exec — async with callback
exec('date', (err, stdout) => {
    console.log('async date:', stdout.trim());
});

// capture exit code without throwing
const bad = execSync('exit 1', { throwOnError: false });
console.log('exit status:', bad.status);
