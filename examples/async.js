console.log("start");

setTimeout(() => console.log("timeout 1"), 100);
setTimeout(() => console.log("timeout 0"), 0);

Promise.resolve()
  .then(() => console.log("promise 1"))
  .then(() => console.log("promise 2"));

console.log("end");
// Expected: start, end, promise 1, promise 2, timeout 0, timeout 1
