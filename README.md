# qjs

A Node.js-compatible JavaScript runtime built on [QuickJS](https://bellard.org/quickjs/).

Runs CommonJS modules, npm packages (including Express), and provides a familiar Node.js API surface — implemented in C++ and pure JavaScript shims on top of a lightweight ES2023 engine.

## Features

- CommonJS `require()` with `node_modules` resolution
- npm package support (tested with Express 5)
- Built-in modules: `fs`, `http`, `path`, `crypto`, `net`, `os`, `child_process`, `vm`, `events`, `stream`, `buffer`, `util`, `url`, `assert`, `querystring`, `timers`, `readline`, `perf_hooks`
- Web APIs: `TextEncoder`/`TextDecoder`, `URL`, `URLSearchParams`, `Buffer`
- Globals: `setTimeout`, `setInterval`, `setImmediate`, `queueMicrotask`, `console`, `process`
- Interactive REPL
- macOS and Linux support

## Requirements

**macOS**
```bash
xcode-select --install          # Xcode Command Line Tools
brew install readline
```

**Ubuntu / Debian**
```bash
sudo apt install build-essential libreadline-dev
```

**CentOS / RHEL / Fedora**
```bash
sudo yum install gcc gcc-c++ make readline-devel
# or on Fedora:
sudo dnf install gcc gcc-c++ make readline-devel
```

## Build

```bash
git clone https://github.com/qinghuaatbc/qjs.git
cd qjs
make
```

Produces a `qjs` binary in the project root.

## Install

To make `qjs` available globally:

```bash
sudo make install
```

This copies the binary to `/usr/local/bin/qjs`. To uninstall:

```bash
sudo make uninstall
```

## Usage

```bash
qjs script.js       # run a JavaScript file
qjs                 # start interactive REPL
```

## Examples

```bash
# Hello World
./qjs examples/hello.js

# HTTP server (visit http://localhost:3000)
./qjs examples/http_server.js

# Express app with frontend UI
cd examples/express-app
npm install
npm start
# open http://localhost:3000
```

## Make targets

| Target | Description |
|---|---|
| `make` | Build `qjs` binary |
| `make install` | Install `qjs` to `/usr/local/bin` |
| `make uninstall` | Remove `qjs` from `/usr/local/bin` |
| `make clean` | Remove build artifacts |
| `make run FILE=path` | Build and run a script |
| `make repl` | Start interactive REPL |
| `make test-all` | Run all example tests |

## Built-in modules

| Module | Key exports |
|---|---|
| `fs` | `readFileSync`, `writeFileSync`, `existsSync`, `statSync`, `readdirSync`, `mkdirSync` |
| `http` | `createServer`, `request`, `METHODS`, `IncomingMessage`, `ServerResponse` |
| `path` | `join`, `resolve`, `dirname`, `basename`, `extname`, `relative`, `normalize` |
| `crypto` | `createHash`, `createHmac`, `randomBytes`, `randomUUID` |
| `net` | `createServer`, `createConnection` |
| `os` | `hostname`, `platform`, `arch`, `cpus`, `freemem`, `totalmem`, `homedir` |
| `child_process` | `execSync`, `spawnSync` |
| `vm` | `runInNewContext`, `runInThisContext`, `createContext`, `Script` |
| `events` | `EventEmitter` |
| `stream` | `Readable`, `Writable`, `Transform`, `PassThrough` |
| `buffer` | `Buffer` |
| `util` | `format`, `inspect`, `promisify`, `inherits` |
| `url` | `URL`, `URLSearchParams`, `parse`, `format` |
| `assert` | `ok`, `equal`, `strictEqual`, `deepEqual`, `throws` |
| `querystring` | `stringify`, `parse` |
| `timers` | `setTimeout`, `clearTimeout`, `setInterval`, `setImmediate` |
