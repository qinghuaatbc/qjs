const deps2 = ['negotiator', 'mime-types', 'mime-db'];
for (const d of deps2) {
    try { require(d); console.log(d, 'OK'); }
    catch(e) { console.log(d, 'FAIL:', e.message); }
}
