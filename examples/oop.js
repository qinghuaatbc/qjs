class Animal {
  constructor(name) {
    this.name = name;
  }
  speak() {
    return `${this.name} makes a sound.`;
  }
  toString() {
    return `Animal(${this.name})`;
  }
}

class Dog extends Animal {
  constructor(name, breed) {
    super(name);
    this.breed = breed;
  }
  speak() {
    return `${this.name} barks.`;
  }
}

const d = new Dog("Rex", "Labrador");
console.log(d.speak());
console.log(d instanceof Dog);
console.log(d instanceof Animal);
console.log(d.name);
