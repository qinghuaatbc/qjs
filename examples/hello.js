// Hello World example
console.log("Hello, World!");

const name = "JSRuntime";
console.log(`Welcome to ${name}!`);

function greet(name, greeting = "Hello") {
    return `${greeting}, ${name}!`;
}

console.log(greet("Alice"));
console.log(greet("Bob", "Hi"));

// Template literals
const a = 10, b = 20;
console.log(`${a} + ${b} = ${a + b}`);

// Array/object inspection
const arr = [1, 2, 3, 4, 5];
console.log("Array:", arr);
console.log("Mapped:", arr.map(x => x * x));

const obj = { lang: "JavaScript", runtime: "QuickJS", version: 1 };
console.log("Object:", obj);

// Destructuring
const { lang, version } = obj;
console.log(`Language: ${lang}, version: ${version}`);

// Spread
const arr2 = [...arr, 6, 7];
console.log("Spread:", arr2);

// Runtime version
console.log("Runtime:", __runtime_version);
