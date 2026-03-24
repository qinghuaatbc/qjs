#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
#include <readline/readline.h>
#include <readline/history.h>

extern "C" {
#include "../quickjs/quickjs.h"
#include "../quickjs/quickjs-libc.h"
}

#include "console.hpp"
#include "process.hpp"
#include "fs.hpp"
#include "http.hpp"
#include "child_process.hpp"
#include "os.hpp"
#include "crypto.hpp"
#include "net.hpp"

// ── error reporting ───────────────────────────────────────────────────────────

static void dump_error(JSContext *ctx) {
    JSValue exception = JS_GetException(ctx);

    // Try to get a nice stack trace
    if (JS_IsError(ctx, exception)) {
        JSValue stack = JS_GetPropertyStr(ctx, exception, "stack");
        if (!JS_IsUndefined(stack)) {
            const char *s = JS_ToCString(ctx, stack);
            if (s) { fprintf(stderr, "\033[31m%s\033[0m\n", s); JS_FreeCString(ctx, s); }
            JS_FreeValue(ctx, stack);
            JS_FreeValue(ctx, exception);
            return;
        }
        JS_FreeValue(ctx, stack);
    }

    const char *s = JS_ToCString(ctx, exception);
    if (s) { fprintf(stderr, "\033[31mUncaught: %s\033[0m\n", s); JS_FreeCString(ctx, s); }
    JS_FreeValue(ctx, exception);
}

// ── file runner ───────────────────────────────────────────────────────────────

static std::string read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) throw std::runtime_error(std::string("Cannot open: ") + path);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f); rewind(f);
    std::string buf(sz, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);
    return buf;
}

static int run_file(JSContext *ctx, const char *filename) {
    std::string src;
    try { src = read_file(filename); }
    catch (std::exception &e) {
        fprintf(stderr, "\033[31m%s\033[0m\n", e.what());
        return 1;
    }

    JSValue result = JS_Eval(ctx, src.c_str(), src.size(),
                             filename, JS_EVAL_TYPE_GLOBAL);

    if (JS_IsException(result)) {
        dump_error(ctx);
        JS_FreeValue(ctx, result);
        return 1;
    }
    JS_FreeValue(ctx, result);

    // Run pending jobs (Promise callbacks, setTimeout, etc.)
    js_std_loop(ctx);
    return 0;
}

// ── REPL ──────────────────────────────────────────────────────────────────────

static void run_repl(JSContext *ctx) {
    printf("Welcome to QJS Runtime (QuickJS)\n");
    printf("Type .exit to quit, .help for help\n\n");

    using_history();
    int line_no = 1;

    while (true) {
        char prompt[32];
        snprintf(prompt, sizeof(prompt), "\033[32mqjs[%d]> \033[0m", line_no++);

        char *line = readline(prompt);
        if (!line) { printf("\n"); break; }  // Ctrl+D

        size_t len = strlen(line);

        if (strcmp(line, ".exit") == 0 || strcmp(line, "quit") == 0) {
            free(line); break;
        }

        if (strcmp(line, ".help") == 0) {
            printf("  .exit       Quit the REPL\n");
            printf("  .help       Show this help\n");
            printf("  ↑ / ↓       Navigate history\n");
            printf("  ← / →       Move cursor\n");
            printf("  Ctrl+A/E    Jump to start/end of line\n");
            printf("  Ctrl+C      Cancel current line\n");
            printf("  Ctrl+D      Exit\n");
            free(line);
            continue;
        }

        if (len == 0) { free(line); continue; }

        add_history(line);  // save to history for ↑↓ navigation

        JSValue result = JS_Eval(ctx, line, len, "<repl>",
                                 JS_EVAL_TYPE_GLOBAL | JS_EVAL_FLAG_BACKTRACE_BARRIER);
        free(line);

        if (JS_IsException(result)) {
            dump_error(ctx);
        } else if (!JS_IsUndefined(result)) {
            const char *s = JS_ToCString(ctx, result);
            if (s) { printf("\033[90m<= %s\033[0m\n", s); JS_FreeCString(ctx, s); }
        }

        JS_FreeValue(ctx, result);
        js_std_loop(ctx);
    }
}

// ── custom APIs ───────────────────────────────────────────────────────────────

static void setup_globals(JSContext *ctx, JSValue global, int argc, char **argv) {
    // Runtime version banner
    JS_SetPropertyStr(ctx, global, "__runtime_version",
        JS_NewString(ctx, "JSRuntime/1.0 (QuickJS)"));

    // console
    js_init_console(ctx, global);

    // process
    js_init_process(ctx, global, argc, argv);

    // fs (as __fs, exposed via require shimming below)
    js_init_fs(ctx, global);

    // http low-level socket primitives
    js_init_http(ctx, global);

    // child_process: execSync, spawnSync
    js_init_child_process(ctx, global);

    // os: hostname, platform, freemem, cpus, …
    js_init_os_module(ctx, global);

    // crypto: hash, hmac, randomBytes, randomUUID
    js_init_crypto(ctx, global);

    // net: TCP client connect/read/write
    js_init_net(ctx, global);

    // ── expose os module functions globally via a module-mode eval ────────────
    // Imports `os` and copies timers + I/O handlers to globalThis so that
    // both our timer polyfills and the http module can use them without importing.
    const char *timer_setup = R"js(
import { setTimeout as _st, clearTimeout as _ct, now as _osNow,
         setReadHandler as _srh, setWriteHandler as _swh } from 'os';
globalThis.setTimeout      = _st;
globalThis.clearTimeout    = _ct;
globalThis.performance     = { now() { return _osNow(); } };
globalThis.__setReadHandler  = _srh;
globalThis.__setWriteHandler = _swh;

// setInterval polyfill using setTimeout
globalThis.setInterval = function setInterval(fn, ms, ...args) {
    let id;
    function repeat() {
        fn(...args);
        id = setTimeout(repeat, ms);
    }
    id = setTimeout(repeat, ms);
    return id;
};
globalThis.clearInterval = globalThis.clearTimeout;
globalThis.setImmediate  = (fn, ...a) => setTimeout(() => fn(...a), 0);
globalThis.clearImmediate = clearTimeout;
globalThis.queueMicrotask = (fn) => Promise.resolve().then(fn);
)js";
    {
        JSValue mod = JS_Eval(ctx, timer_setup, strlen(timer_setup),
                              "<timers>", JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(mod)) {
            JSValue promise = JS_EvalFunction(ctx, mod);
            if (JS_IsException(promise)) dump_error(ctx);
            // Drain pending jobs so the import runs synchronously
            int err;
            do { err = JS_ExecutePendingJob(JS_GetRuntime(ctx), nullptr); } while (err > 0);
            JS_FreeValue(ctx, promise);
        } else {
            dump_error(ctx);
        }
    }

    // ── require shim + CommonJS globals ───────────────────────────────────────
    const char *require_shim = R"js(
(function() {

// ── http module (pure JS, uses __http_* C++ primitives + __setReadHandler) ──
const HTTP_STATUS = {
    100:'Continue', 101:'Switching Protocols',
    200:'OK', 201:'Created', 202:'Accepted', 204:'No Content',
    301:'Moved Permanently', 302:'Found', 304:'Not Modified',
    400:'Bad Request', 401:'Unauthorized', 403:'Forbidden',
    404:'Not Found', 405:'Method Not Allowed', 409:'Conflict',
    422:'Unprocessable Entity', 429:'Too Many Requests',
    500:'Internal Server Error', 501:'Not Implemented',
    502:'Bad Gateway', 503:'Service Unavailable',
};

function parseRequest(raw) {
    const headerEnd = raw.indexOf('\r\n\r\n');
    if (headerEnd === -1) return null;
    const headerSection = raw.substring(0, headerEnd);
    const body = raw.substring(headerEnd + 4);
    const lines = headerSection.split('\r\n');
    const firstLine = lines[0] || '';
    const parts = firstLine.split(' ');
    const method  = parts[0] || 'GET';
    const rawUrl  = parts[1] || '/';
    const headers = {};
    for (let i = 1; i < lines.length; i++) {
        const colon = lines[i].indexOf(':');
        if (colon > 0) {
            const k = lines[i].substring(0, colon).trim().toLowerCase();
            const v = lines[i].substring(colon + 1).trim();
            headers[k] = v;
        }
    }
    // Parse URL into pathname + query
    const qIdx = rawUrl.indexOf('?');
    const pathname = qIdx === -1 ? rawUrl : rawUrl.substring(0, qIdx);
    const search   = qIdx === -1 ? '' : rawUrl.substring(qIdx);
    const query    = {};
    if (search) {
        search.substring(1).split('&').forEach(pair => {
            const [k, v = ''] = pair.split('=');
            query[decodeURIComponent(k)] = decodeURIComponent(v);
        });
    }
    // Make req stream-like so body-parser (express.json, etc.) can read via
    // req.on('data'/'end') — emit the body asynchronously when listeners register.
    const req = { method, url: rawUrl, pathname, search, query, headers,
                  body: undefined,   // body-parser will populate this
                  _rawBody: body,
                  readable: true, httpVersion: '1.1', httpVersionMajor: 1, httpVersionMinor: 1,
                  connection: { remoteAddress: '127.0.0.1' },
                  socket: { remoteAddress: '127.0.0.1', destroy() {} },
                  _events: Object.create(null), _bodyConsumed: false };
    req.on = function(ev, fn) {
        if (!req._events[ev]) req._events[ev] = [];
        req._events[ev].push(fn);
        // When a 'data' or 'end' listener is added, schedule body emission
        if ((ev === 'data' || ev === 'end') && !req._bodyScheduled) {
            req._bodyScheduled = true;
            setTimeout(function() {
                if (req._rawBody && req._rawBody.length > 0) {
                    (req._events['data'] || []).forEach(function(f){ f(req._rawBody); });
                }
                (req._events['end'] || []).forEach(function(f){ f(); });
            }, 0);
        }
        return req;
    };
    req.once = function(ev, fn) {
        const w = function(...a) { req.removeListener(ev, w); fn(...a); };
        w._orig = fn;
        return req.on(ev, w);
    };
    req.removeListener = function(ev, fn) {
        if (!req._events[ev]) return req;
        req._events[ev] = req._events[ev].filter(h => h !== fn && h._orig !== fn);
        return req;
    };
    req.emit = function(ev, ...args) {
        (req._events[ev] || []).forEach(f => f(...args));
        return true;
    };
    req.resume = function() { return req; };
    req.pause  = function() { return req; };
    req.pipe   = function(dest) { return dest; };
    req.unpipe = function() { return req; };
    req.setEncoding = function() { return req; };
    req.destroy = function() {};
    return req;
}

function makeResponse(fd) {
    let _status = 200;
    let _headers = {};
    let _chunks = [];
    let _sent = false;
    let _headersSent = false;

    const res = {
        statusCode: 200,
        writeHead(code, hdrs) {
            _status = code;
            res.statusCode = code;
            if (hdrs) {
                for (const [k, v] of Object.entries(hdrs))
                    _headers[k.toLowerCase()] = String(v);
            }
        },
        setHeader(name, value) {
            _headers[name.toLowerCase()] = String(value);
        },
        getHeader(name) { return _headers[name.toLowerCase()]; },
        removeHeader(name) { delete _headers[name.toLowerCase()]; },
        write(chunk) {
            if (_sent) return;
            _chunks.push(typeof chunk === 'string' ? chunk : String(chunk));
        },
        end(chunk) {
            if (_sent) return;
            _sent = true;
            if (chunk !== undefined && chunk !== null)
                _chunks.push(typeof chunk === 'string' ? chunk : String(chunk));
            const body = _chunks.join('');
            if (!_headers['content-type'])    _headers['content-type'] = 'text/plain; charset=utf-8';
            if (!_headers['content-length'])  _headers['content-length'] = String(body.length);
            _headers['connection'] = 'close';
            const statusText = HTTP_STATUS[_status] || 'Unknown';
            let response = `HTTP/1.1 ${_status} ${statusText}\r\n`;
            for (const [k, v] of Object.entries(_headers))
                response += `${k}: ${v}\r\n`;
            response += `\r\n${body}`;
            __http_write(fd, response);
            __http_close(fd);
        },
        json(obj) {
            res.setHeader('content-type', 'application/json; charset=utf-8');
            res.end(JSON.stringify(obj));
        },
        send: (body) => res.end(body),
    };
    return res;
}

function handleClient(clientFd, requestHandler) {
    let raw = '';
    __setReadHandler(clientFd, function onData() {
        const chunk = __http_read(clientFd, 65536);
        if (chunk === null) {
            // EOF / connection closed before we got a full request
            __setReadHandler(clientFd, null);
            __http_close(clientFd);
            return;
        }
        raw += chunk;
        const req = parseRequest(raw);
        if (!req) return;  // headers not complete yet

        // Got a full request – unregister read handler before calling user code
        __setReadHandler(clientFd, null);
        const res = makeResponse(clientFd);
        try {
            requestHandler(req, res);
        } catch (e) {
            if (!res._sent) { res.writeHead(500); res.end('Internal Server Error\n'); }
            console.error('[http] handler error:', e && e.message || e);
        }
    });
}

const _httpModule = {
    createServer(requestHandler) {
        let _serverFd = -1;
        let _listening = false;
        const _eventHandlers = {};

        const server = {
            listen(port, hostOrCb, cb) {
                let host = '0.0.0.0';
                let callback = cb;
                if (typeof hostOrCb === 'function') { callback = hostOrCb; }
                else if (typeof hostOrCb === 'string') { host = hostOrCb; }

                _serverFd = __http_create_server_socket(port, host);
                _listening = true;

                __setReadHandler(_serverFd, function onConnect() {
                    const info = __http_accept(_serverFd);
                    if (!info || info.fd < 0) return;
                    handleClient(info.fd, requestHandler);
                });

                if (typeof callback === 'function') callback();

                // Emit 'listening' event
                const handlers = _eventHandlers['listening'];
                if (handlers) handlers.forEach(h => h());

                return server;
            },
            close(cb) {
                if (_serverFd >= 0) {
                    __setReadHandler(_serverFd, null);
                    __http_close(_serverFd);
                    _serverFd = -1;
                    _listening = false;
                }
                if (typeof cb === 'function') cb();
                return server;
            },
            on(event, handler) {
                if (!_eventHandlers[event]) _eventHandlers[event] = [];
                _eventHandlers[event].push(handler);
                return server;
            },
            once(event, handler) {
                const w = (...a) => { server.removeListener(event, w); handler(...a); };
                w._orig = handler;
                return server.on(event, w);
            },
            removeListener(event, handler) {
                if (!_eventHandlers[event]) return server;
                _eventHandlers[event] = _eventHandlers[event].filter(h => h !== handler && h._orig !== handler);
                return server;
            },
            emit(event, ...args) {
                const hs = _eventHandlers[event];
                if (hs) [...hs].forEach(h => h(...args));
                return !!hs;
            },
            address() { return { address: '0.0.0.0', family: 'IPv4', port: 0 }; },
            get listening() { return _listening; },
        };
        return server;
    },

    STATUS_CODES: HTTP_STATUS,
};

// ── child_process module ─────────────────────────────────────────────────────
const _childProcess = {
    execSync(cmd, opts) {
        const r = __cp_exec_sync(cmd, opts);
        // Match Node.js: execSync returns stdout string directly
        if (opts && opts.encoding === 'buffer') return r.stdout;
        return r.stdout;
    },
    spawnSync(cmd, args, opts) {
        return __cp_spawn_sync(cmd, args || [], opts);
    },
    exec(cmd, opts, callback) {
        if (typeof opts === 'function') { callback = opts; opts = {}; }
        // Run async via setTimeout to not block
        setTimeout(() => {
            try {
                const r = __cp_exec_sync(cmd, { throwOnError: false, captureStderr: true, ...opts });
                const err = r.status !== 0 ? new Error(`Command failed: ${cmd}`) : null;
                if (callback) callback(err, r.stdout, r.stderr);
            } catch(e) {
                if (callback) callback(e, '', '');
            }
        }, 0);
    },
};

// ── module registry (built-ins) ───────────────────────────────────────────────
const _HTTP_METHODS = ['ACL','BIND','CHECKOUT','CONNECT','COPY','DELETE','GET','HEAD','LINK','LOCK',
    'M-SEARCH','MERGE','MKACTIVITY','MKCALENDAR','MKCOL','MOVE','NOTIFY','OPTIONS','PATCH','POST',
    'PROPFIND','PROPPATCH','PURGE','PUT','REBIND','REPORT','SEARCH','SOURCE','SUBSCRIBE','TRACE',
    'UNBIND','UNLINK','UNLOCK','UNSUBSCRIBE'];
_httpModule.METHODS = _HTTP_METHODS;
_httpModule.STATUS_CODES = {100:'Continue',101:'Switching Protocols',200:'OK',201:'Created',
    202:'Accepted',204:'No Content',301:'Moved Permanently',302:'Found',304:'Not Modified',
    400:'Bad Request',401:'Unauthorized',403:'Forbidden',404:'Not Found',
    405:'Method Not Allowed',409:'Conflict',410:'Gone',422:'Unprocessable Entity',
    429:'Too Many Requests',500:'Internal Server Error',501:'Not Implemented',
    502:'Bad Gateway',503:'Service Unavailable',504:'Gateway Timeout'};
// Minimal IncomingMessage and ServerResponse stubs for npm compat
function IncomingMessage() { this.headers={}; this.method='GET'; this.url='/'; this.httpVersion='1.1'; }
IncomingMessage.prototype.on = function(ev,fn){return this;};
IncomingMessage.prototype.pipe = function(d){return d;};
function ServerResponse() { this.statusCode=200; this.headersSent=false; this._headers={}; }
ServerResponse.prototype.setHeader = function(k,v){this._headers[k.toLowerCase()]=v;};
ServerResponse.prototype.getHeader = function(k){return this._headers[k.toLowerCase()];};
ServerResponse.prototype.removeHeader = function(k){delete this._headers[k.toLowerCase()];};
ServerResponse.prototype.writeHead = function(code,hdrs){this.statusCode=code;if(hdrs)for(const k of Object.keys(hdrs))this.setHeader(k,hdrs[k]);};
ServerResponse.prototype.write = function(){return true;};
ServerResponse.prototype.end = function(){this.headersSent=true;};
ServerResponse.prototype.on = function(){return this;};
_httpModule.IncomingMessage = IncomingMessage;
_httpModule.ServerResponse = ServerResponse;

const _modules = {
    'fs':            __fs,
    'http':          _httpModule,
    'child_process': _childProcess,
    'path': (function() {
        function normalize(p) {
            const abs = p.startsWith('/');
            const parts = p.split('/');
            const out = [];
            for (const s of parts) {
                if (s === '' || s === '.') continue;
                if (s === '..') { if (out.length) out.pop(); }
                else out.push(s);
            }
            let r = (abs ? '/' : '') + out.join('/');
            return r || '.';
        }
        function resolve(...parts) {
            // Prepend cwd for relative resolution (matches Node.js behaviour)
            const allParts = [process.cwd(), ...parts];
            let r = '';
            for (let i = allParts.length - 1; i >= 0; i--) {
                const p = allParts[i];
                if (p.startsWith('/')) { r = normalize(p + '/' + r); break; }
                r = p + '/' + r;
                if (i === 0) r = normalize(r);
            }
            return r || '/';
        }
        function relative(from, to) {
            const f = normalize(from).split('/').filter(Boolean);
            const t = normalize(to).split('/').filter(Boolean);
            let i = 0;
            while (i < f.length && i < t.length && f[i] === t[i]) i++;
            return [...Array(f.length - i).fill('..'), ...t.slice(i)].join('/') || '.';
        }
        const path = {
            sep: '/', delimiter: ':',
            normalize,
            resolve,
            relative,
            join(...parts)    { return normalize(parts.join('/')); },
            dirname(p)        { const i = p.lastIndexOf('/'); return i > 0 ? p.slice(0,i) : i === 0 ? '/' : '.'; },
            basename(p, ext)  { let b = p.replace(/.*\//, ''); return ext && b.endsWith(ext) ? b.slice(0,-ext.length) : b; },
            extname(p)        { const m = p.match(/\.[^.\/]*$/); return m ? m[0] : ''; },
            isAbsolute(p)     { return p.startsWith('/'); },
            parse(p)          { const d=path.dirname(p),b=path.basename(p),e=path.extname(p); return {root:p.startsWith('/')?'/':'',dir:d,base:b,ext:e,name:b.slice(0,b.length-e.length)}; },
            format(o)         { return (o.dir?o.dir+'/':'')+(o.base||(o.name||'')+(o.ext||'')); },
        };
        path.posix = path;
        path.win32 = path;
        return path;
    })(),
};

// ── shared module registry (built-ins + extras added by modules_shim) ────────
globalThis.__qjs_registry = _modules;

// ── module cache ──────────────────────────────────────────────────────────────
const _cache = {};

// ── path helpers ──────────────────────────────────────────────────────────────
function _normPath(p) {
    const abs = p.startsWith('/');
    const parts = p.split('/');
    const out = [];
    for (const seg of parts) {
        if (seg === '' || seg === '.') continue;
        if (seg === '..') { if (out.length) out.pop(); }
        else out.push(seg);
    }
    return (abs ? '/' : '') + out.join('/') || '.';
}

function _joinPath(a, b) {
    if (!b) return a;
    if (b.startsWith('/')) return _normPath(b);
    return _normPath(a + '/' + b);
}

function _dirnamePath(p) {
    const i = p.lastIndexOf('/');
    return i <= 0 ? (i === 0 ? '/' : '.') : p.substring(0, i);
}

// ── file resolver ─────────────────────────────────────────────────────────────
function _resolveFile(p) {
    if (__fs.existsSync(p)) {
        try {
            const st = __fs.statSync(p);
            if (st._isFile) return p;
            // directory: try package.json "main", then index.js
            const pkgPath = p + '/package.json';
            if (__fs.existsSync(pkgPath)) {
                try {
                    const pkg = JSON.parse(__fs.readFileSync(pkgPath, 'utf8'));
                    const main = pkg.main || 'index.js';
                    const r = _resolveFile(_joinPath(p, main));
                    if (r) return r;
                } catch(e) {}
            }
            const idx = p + '/index.js';
            if (__fs.existsSync(idx)) return idx;
            return null;
        } catch(e) { return null; }
    }
    if (__fs.existsSync(p + '.js'))   return p + '.js';
    if (__fs.existsSync(p + '.json')) return p + '.json';
    if (__fs.existsSync(p + '/index.js')) return p + '/index.js';
    const pkgPath2 = p + '/package.json';
    if (__fs.existsSync(pkgPath2)) {
        try {
            const pkg = JSON.parse(__fs.readFileSync(pkgPath2, 'utf8'));
            const main = pkg.main || 'index.js';
            const r = _resolveFile(_joinPath(p, main));
            if (r) return r;
        } catch(e) {}
    }
    return null;
}

// ── CJS file loader ───────────────────────────────────────────────────────────
function _loadFile(filepath) {
    if (_cache[filepath]) return _cache[filepath];
    const src = __fs.readFileSync(filepath, 'utf8');
    const mod = { exports: {}, filename: filepath, loaded: false, id: filepath };
    _cache[filepath] = mod;  // cache before eval to handle circular deps
    // JSON files: parse directly instead of eval-ing
    if (filepath.endsWith('.json')) {
        try { mod.exports = JSON.parse(src); }
        catch(e) { delete _cache[filepath]; throw new SyntaxError('JSON parse error in ' + filepath + ': ' + e.message); }
        mod.loaded = true;
        return mod;
    }
    const dir = _dirnamePath(filepath);
    const wrapped = '(function(module, exports, require, __dirname, __filename) {\n' + src + '\n})';
    try {
        const fn = eval(wrapped);
        fn(mod, mod.exports, _makeRequire(dir), dir, filepath);
    } catch(e) {
        delete _cache[filepath];
        throw e;
    }
    mod.loaded = true;
    return mod;
}

// ── require factory ───────────────────────────────────────────────────────────
function _makeRequire(currentDir) {
    function req(id) {
        // Support 'node:' prefix (e.g. 'node:events' → 'events')
        if (id.startsWith('node:')) id = id.slice(5);
        if (_modules[id]) return _modules[id];

        // Relative or absolute path
        if (id.startsWith('./') || id.startsWith('../') || id.startsWith('/')) {
            const base = id.startsWith('/') ? id : _joinPath(currentDir, id);
            const resolved = _resolveFile(base);
            if (!resolved) throw new Error("Cannot find module '" + id + "'");
            return _loadFile(resolved).exports;
        }

        // npm package: walk up directory tree searching node_modules
        let dir = currentDir;
        const visited = new Set();
        while (true) {
            if (visited.has(dir)) break;
            visited.add(dir);
            const candidate = _resolveFile(dir + '/node_modules/' + id);
            if (candidate) return _loadFile(candidate).exports;
            const parent = _dirnamePath(dir);
            if (parent === dir) break;
            dir = parent;
        }

        throw new Error("Cannot find module '" + id + "'");
    }
    req.resolve = (id) => id;
    return req;
}

// ── V8-compat polyfills needed by npm packages ────────────────────────────────
// Parse a QuickJS stack line like "    at funcName (file.js:10:5)"
function _parseStackLine(line) {
    const m = line.match(/^\s+at\s+(.+?)\s+\((.+):(\d+):(\d+)\)$/) ||
              line.match(/^\s+at\s+(.+):(\d+):(\d+)$/);
    if (!m) return null;
    if (m.length === 5) return { fn: m[1], file: m[2], line: +m[3], col: +m[4] };
    return { fn: '<anonymous>', file: m[1], line: +m[2], col: +m[3] };
}
function _makeCallSite(frame) {
    return {
        getFileName()     { return frame ? frame.file : '<anonymous>'; },
        getLineNumber()   { return frame ? frame.line : 0; },
        getColumnNumber() { return frame ? frame.col : 0; },
        getFunctionName() { return frame ? frame.fn : null; },
        getMethodName()   { return null; },
        getTypeName()     { return null; },
        getThis()         { return null; },
        isToplevel()      { return true; },
        isEval()          { return frame && (frame.file === '<eval>' || frame.file.startsWith('<')); },
        isNative()        { return false; },
        isConstructor()   { return false; },
        getEvalOrigin()   { return ''; },
        toString()        { return frame ? `${frame.fn} (${frame.file}:${frame.line}:${frame.col})` : '<anonymous>'; },
    };
}
// ── TextEncoder / TextDecoder polyfills ───────────────────────────────────────
if (typeof TextEncoder === 'undefined') {
    globalThis.TextEncoder = class TextEncoder {
        get encoding() { return 'utf-8'; }
        encode(str) {
            str = String(str);
            const bytes = [];
            for (let i = 0; i < str.length; i++) {
                let cp = str.charCodeAt(i);
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < str.length) {
                    const lo = str.charCodeAt(i + 1);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        i++;
                    }
                }
                if (cp < 0x80)        { bytes.push(cp); }
                else if (cp < 0x800)  { bytes.push(0xC0 | (cp >> 6), 0x80 | (cp & 0x3F)); }
                else if (cp < 0x10000){ bytes.push(0xE0|(cp>>12), 0x80|((cp>>6)&0x3F), 0x80|(cp&0x3F)); }
                else                  { bytes.push(0xF0|(cp>>18), 0x80|((cp>>12)&0x3F), 0x80|((cp>>6)&0x3F), 0x80|(cp&0x3F)); }
            }
            return new Uint8Array(bytes);
        }
        encodeInto(str, buf) {
            const encoded = this.encode(str);
            const len = Math.min(encoded.length, buf.length);
            buf.set(encoded.subarray(0, len));
            return { read: str.length, written: len };
        }
    };
}
if (typeof TextDecoder === 'undefined') {
    globalThis.TextDecoder = class TextDecoder {
        constructor(enc = 'utf-8') { this.encoding = enc.toLowerCase(); }
        decode(buf) {
            const bytes = buf instanceof Uint8Array ? buf : new Uint8Array(buf);
            let out = '';
            for (let i = 0; i < bytes.length; ) {
                let cp, b = bytes[i];
                if      (b < 0x80)               { cp = b; i++; }
                else if ((b & 0xE0) === 0xC0)    { cp = (b & 0x1F) << 6  | (bytes[++i] & 0x3F); i++; }
                else if ((b & 0xF0) === 0xE0)    { cp = (b & 0x0F) << 12 | (bytes[++i] & 0x3F) << 6 | (bytes[++i] & 0x3F); i++; }
                else                              { cp = (b & 0x07) << 18 | (bytes[++i] & 0x3F) << 12 | (bytes[++i] & 0x3F) << 6 | (bytes[++i] & 0x3F); i++; }
                if (cp > 0xFFFF) {
                    cp -= 0x10000;
                    out += String.fromCharCode(0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
                } else {
                    out += String.fromCharCode(cp);
                }
            }
            return out;
        }
    };
}

if (!Error.captureStackTrace) {
    Error.captureStackTrace = function(obj, constructorOpt) {
        const e = new Error();
        const rawLines = (e.stack || '').split('\n').filter(l => l.match(/^\s+at\s/));
        // Skip frames up to and including captureStackTrace + constructorOpt
        const frames = rawLines.map(_parseStackLine).filter(Boolean);
        const callSites = frames.map(_makeCallSite);
        if (typeof Error.prepareStackTrace === 'function') {
            obj.stack = Error.prepareStackTrace(obj, callSites);
        } else {
            obj.stack = rawLines.join('\n');
        }
    };
}
if (Error.prepareStackTrace === undefined) Error.prepareStackTrace = null;
if (Error.stackTraceLimit === undefined) Error.stackTraceLimit = 10;

// Global require uses live __dirname so the search root is always correct.
globalThis.require = function require(id) {
    return _makeRequire(globalThis.__dirname || '.')(id);
};
globalThis.require.resolve = (id) => id;
globalThis.module  = { exports: {} };
globalThis.exports = globalThis.module.exports;
globalThis.__dirname  = '.';
globalThis.__filename = '';

// expose child_process helpers globally for convenience
globalThis.execSync  = _childProcess.execSync;
globalThis.spawnSync = _childProcess.spawnSync;
globalThis.exec      = _childProcess.exec;

})();
)js";

    JSValue v = JS_Eval(ctx, require_shim, strlen(require_shim),
                        "<setup>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v)) dump_error(ctx);
    JS_FreeValue(ctx, v);

    // ── extra modules: events, util, assert, url, querystring, buffer, ─────
    // ── os, crypto, net, timers, stream, string_decoder, dns, tty, zlib ────
    const char *modules_shim = R"js(
(function() {

// ── EventEmitter ──────────────────────────────────────────────────────────────
class EventEmitter {
    constructor() { this._events = Object.create(null); this._maxListeners = 10; }
    _init() { if (!this._events) { this._events = Object.create(null); this._maxListeners = 10; } }
    on(event, listener) {
        this._init();
        (this._events[event] || (this._events[event] = [])).push(listener);
        return this;
    }
    addListener(event, listener) { return this.on(event, listener); }
    once(event, listener) {
        const w = (...a) => { this.off(event, w); listener(...a); };
        w._orig = listener;
        return this.on(event, w);
    }
    off(event, listener) {
        this._init();
        if (!this._events[event]) return this;
        this._events[event] = this._events[event].filter(l => l !== listener && l._orig !== listener);
        return this;
    }
    removeListener(event, listener) { return this.off(event, listener); }
    emit(event, ...args) {
        this._init();
        if (!this._events[event]) return false;
        [...this._events[event]].forEach(l => l(...args));
        return true;
    }
    removeAllListeners(e) { this._init(); if(e) delete this._events[e]; else this._events=Object.create(null); return this; }
    listenerCount(e) { this._init(); return (this._events[e]||[]).length; }
    listeners(e)     { this._init(); return [...(this._events[e]||[])]; }
    eventNames()     { this._init(); return Object.keys(this._events); }
    setMaxListeners(n){ this._maxListeners=n; return this; }
    getMaxListeners() { return this._maxListeners||10; }
}
EventEmitter.defaultMaxListeners = 10;

// ── util ──────────────────────────────────────────────────────────────────────
const _util = {
    format(fmt, ...args) {
        if (typeof fmt !== 'string') return [fmt,...args].map(a=>_util.inspect(a)).join(' ');
        let i=0;
        const s = fmt.replace(/%([sdifjoO%])/g, (_,sp) => {
            if(sp==='%') return '%';
            if(i>=args.length) return '%'+sp;
            const a=args[i++];
            if(sp==='s') return String(a);
            if(sp==='d'||sp==='i') return parseInt(a);
            if(sp==='f') return parseFloat(a);
            if(sp==='j') return JSON.stringify(a);
            return _util.inspect(a);
        });
        const rest=args.slice(i).map(a=>_util.inspect(a));
        return rest.length ? s+' '+rest.join(' ') : s;
    },
    inspect(obj, opts={}) {
        const depth = opts.depth !== undefined ? opts.depth : 2;
        const seen = new Set();
        function ins(v, d) {
            if(v===null) return 'null';
            if(v===undefined) return 'undefined';
            if(typeof v==='string') return JSON.stringify(v);
            if(typeof v==='number') return Object.is(v,-0)?'-0':String(v);
            if(typeof v==='boolean'||typeof v==='bigint'||typeof v==='symbol') return String(v);
            if(typeof v==='function') return `[Function: ${v.name||'(anonymous)'}]`;
            if(d<0) return Array.isArray(v)?'[Array]':'[Object]';
            if(seen.has(v)) return '[Circular]';
            seen.add(v);
            let r;
            if(v instanceof RegExp)      r=String(v);
            else if(v instanceof Date)   r=v.toISOString();
            else if(v instanceof Error)  r=v.stack||String(v);
            else if(v instanceof Map)    r=`Map(${v.size}) { ${[...v].map(([k,val])=>`${ins(k,d-1)} => ${ins(val,d-1)}`).join(', ')} }`;
            else if(v instanceof Set)    r=`Set(${v.size}) { ${[...v].map(val=>ins(val,d-1)).join(', ')} }`;
            else if(Array.isArray(v))    r=`[ ${v.map(e=>ins(e,d-1)).join(', ')} ]`;
            else { const e=Object.entries(v).map(([k,val])=>`${k}: ${ins(val,d-1)}`); r=e.length?`{ ${e.join(', ')} }`:'{}'; }
            seen.delete(v);
            return r;
        }
        return ins(obj, depth);
    },
    promisify(fn) {
        return (...args) => new Promise((res,rej) => fn(...args,(err,...r)=>err?rej(err):res(r.length<=1?r[0]:r)));
    },
    callbackify(fn) {
        return (...args) => { const cb=args.pop(); Promise.resolve().then(()=>fn(...args)).then(v=>cb(null,v),e=>cb(e instanceof Error?e:new Error(String(e)))); };
    },
    inherits(ctor, sup) { ctor.super_=sup; Object.setPrototypeOf(ctor.prototype, sup.prototype); },
    deprecate(fn,msg) { let w=false; return function(...a){if(!w){console.warn('DeprecationWarning:',msg);w=true;}return fn.apply(this,a);}; },
    isArray:Array.isArray, isString:v=>typeof v==='string', isNumber:v=>typeof v==='number',
    isBoolean:v=>typeof v==='boolean', isNull:v=>v===null, isUndefined:v=>v===undefined,
    isObject:v=>v!==null&&typeof v==='object', isFunction:v=>typeof v==='function',
    isRegExp:v=>v instanceof RegExp, isDate:v=>v instanceof Date, isError:v=>v instanceof Error,
    types:{ isArray:Array.isArray, isMap:v=>v instanceof Map, isSet:v=>v instanceof Set,
            isDate:v=>v instanceof Date, isRegExp:v=>v instanceof RegExp, isPromise:v=>v instanceof Promise }
};

// ── assert ────────────────────────────────────────────────────────────────────
class AssertionError extends Error { constructor(m){super(m);this.name='AssertionError';} }
function assert(v,m){if(!v) throw new AssertionError(m||`Expected truthy, got ${_util.inspect(v)}`);}
assert.ok=assert;
assert.fail=(m)=>{throw new AssertionError(m||'Failed');};
assert.equal=(a,b,m)=>{if(a!=b) throw new AssertionError(m||`${_util.inspect(a)} == ${_util.inspect(b)}`);};
assert.notEqual=(a,b,m)=>{if(a==b) throw new AssertionError(m||`${_util.inspect(a)} != ${_util.inspect(b)}`);};
assert.strictEqual=(a,b,m)=>{if(a!==b) throw new AssertionError(m||`${_util.inspect(a)} === ${_util.inspect(b)}`);};
assert.notStrictEqual=(a,b,m)=>{if(a===b) throw new AssertionError(m||`${_util.inspect(a)} !== ${_util.inspect(b)}`);};
assert.deepEqual=(a,b,m)=>{if(JSON.stringify(a)!==JSON.stringify(b)) throw new AssertionError(m||`Deep equal:\n  ${_util.inspect(a)}\n  ${_util.inspect(b)}`);};
assert.deepStrictEqual=assert.deepEqual;
assert.throws=(fn,exp,m)=>{try{fn();}catch(e){if(!exp)return;if(typeof exp==='function'&&!(e instanceof exp))throw new AssertionError(m||`Wrong error type`);return;}throw new AssertionError(m||'Expected throw');};
assert.doesNotThrow=(fn,m)=>{try{fn();}catch(e){throw new AssertionError(m||`Unexpected throw: ${e.message}`);}};
assert.rejects=async(fn,m)=>{try{await(typeof fn==='function'?fn():fn);}catch(e){return;}throw new AssertionError(m||'Expected rejection');};
assert.AssertionError=AssertionError;

// ── querystring ───────────────────────────────────────────────────────────────
const _qs = {
    escape:encodeURIComponent, unescape:decodeURIComponent,
    stringify(obj,sep='&',eq='='){
        return Object.entries(obj).filter(([,v])=>v!==undefined).map(([k,v])=>{
            if(Array.isArray(v)) return v.map(i=>`${encodeURIComponent(k)}${eq}${encodeURIComponent(i)}`).join(sep);
            return `${encodeURIComponent(k)}${eq}${encodeURIComponent(v)}`;
        }).join(sep);
    },
    parse(str,sep='&',eq='='){
        const o={};
        if(!str) return o;
        str.split(sep).forEach(pair=>{
            const i=pair.indexOf(eq); if(i<0) return;
            const k=decodeURIComponent(pair.slice(0,i).replace(/\+/g,' '));
            const v=decodeURIComponent(pair.slice(i+1).replace(/\+/g,' '));
            if(o[k]===undefined) o[k]=v;
            else if(Array.isArray(o[k])) o[k].push(v);
            else o[k]=[o[k],v];
        });
        return o;
    },
};

// ── URLSearchParams & URL ─────────────────────────────────────────────────────
class URLSearchParams {
    constructor(init='') {
        this._p=[];
        if(typeof init==='string'){
            if(init.startsWith('?')) init=init.slice(1);
            if(init) init.split('&').forEach(p=>{ const i=p.indexOf('='); this._p.push(i>=0?[decodeURIComponent(p.slice(0,i).replace(/\+/g,' ')),decodeURIComponent(p.slice(i+1).replace(/\+/g,' '))]:[decodeURIComponent(p.replace(/\+/g,' ')),'']); });
        } else if(Array.isArray(init)) { init.forEach(([k,v])=>this._p.push([String(k),String(v)])); }
        else if(init&&typeof init==='object') { Object.entries(init).forEach(([k,v])=>this._p.push([k,String(v)])); }
    }
    append(k,v){this._p.push([k,String(v)]);}
    delete(k){this._p=this._p.filter(([pk])=>pk!==k);}
    get(k){const f=this._p.find(([pk])=>pk===k);return f?f[1]:null;}
    getAll(k){return this._p.filter(([pk])=>pk===k).map(([,v])=>v);}
    has(k){return this._p.some(([pk])=>pk===k);}
    set(k,v){this.delete(k);this._p.push([k,String(v)]);}
    sort(){this._p.sort(([a],[b])=>a<b?-1:a>b?1:0);}
    toString(){return this._p.map(([k,v])=>`${encodeURIComponent(k)}=${encodeURIComponent(v)}`).join('&');}
    keys(){return this._p.map(([k])=>k)[Symbol.iterator]();}
    values(){return this._p.map(([,v])=>v)[Symbol.iterator]();}
    entries(){return this._p[Symbol.iterator]();}
    forEach(cb){this._p.forEach(([k,v])=>cb(v,k,this));}
    get size(){return this._p.length;}
    [Symbol.iterator](){return this._p[Symbol.iterator]();}
}
class URL {
    constructor(url,base){
        let s=String(url);
        if(base){const b=new URL(base);if(!/^[a-zA-Z][a-zA-Z0-9+\-.]*:\/\//.test(s)){s=s.startsWith('/')?b.origin+s:b.origin+b.pathname.replace(/\/[^\/]*$/,'/')+s;}}
        const m=s.match(/^([a-zA-Z][a-zA-Z0-9+\-.]*):\/\/(?:([^:@\/]*)(?::([^@\/]*))?@)?([^/:?#]*)(?::(\d+))?(\/[^?#]*)?(\?[^#]*)?(#.*)?$/);
        if(!m) throw new TypeError(`Invalid URL: ${url}`);
        this._protocol=m[1]+':'; this._username=m[2]?decodeURIComponent(m[2]):''; this._password=m[3]?decodeURIComponent(m[3]):'';
        this._hostname=m[4]||''; this._port=m[5]||''; this._pathname=m[6]||'/';
        this._search=m[7]||''; this._hash=m[8]||''; this._sp=new URLSearchParams(this._search);
    }
    get protocol(){return this._protocol;} set protocol(v){this._protocol=v.endsWith(':')?v:v+':';}
    get username(){return this._username;} set username(v){this._username=v;}
    get password(){return this._password;} set password(v){this._password=v;}
    get hostname(){return this._hostname;} set hostname(v){this._hostname=v;}
    get port(){return this._port;}         set port(v){this._port=String(v);}
    get host(){return this._port?`${this._hostname}:${this._port}`:this._hostname;}
    get pathname(){return this._pathname;} set pathname(v){this._pathname=v.startsWith('/')?v:'/'+v;}
    get search(){const q=this._sp.toString();return q?'?'+q:'';}
    set search(v){this._sp=new URLSearchParams(v);}
    get hash(){return this._hash;} set hash(v){this._hash=v.startsWith('#')?v:'#'+v;}
    get origin(){return `${this._protocol}//${this.host}`;}
    get searchParams(){return this._sp;}
    get href(){let s=`${this._protocol}//`;if(this._username){s+=encodeURIComponent(this._username);if(this._password)s+=':'+encodeURIComponent(this._password);s+='@';}s+=this.host+this._pathname;const q=this.search;if(q)s+=q;if(this._hash)s+=this._hash;return s;}
    toString(){return this.href;} toJSON(){return this.href;}
}

// ── Buffer ────────────────────────────────────────────────────────────────────
class Buffer extends Uint8Array {
    static from(d,enc='utf8'){
        if(typeof d==='string'){
            if(enc==='hex'){const b=d.match(/../g)||[];const u=new Buffer(b.length);b.forEach((x,i)=>u[i]=parseInt(x,16));return u;}
            if(enc==='base64'){const s=atob(d);const u=new Buffer(s.length);for(let i=0;i<s.length;i++)u[i]=s.charCodeAt(i);return u;}
            const e=new TextEncoder().encode(d);const u=new Buffer(e.length);u.set(e);return u;
        }
        if(d instanceof Uint8Array||Array.isArray(d)){const u=new Buffer(d.length);u.set(d);return u;}
        throw new TypeError('Buffer.from: unsupported type');
    }
    static alloc(n,fill=0){const b=new Buffer(n);b.fill(fill);return b;}
    static allocUnsafe(n){return new Buffer(n);}
    static isBuffer(o){return o instanceof Buffer;}
    static concat(list,tl){if(tl===undefined)tl=list.reduce((s,b)=>s+b.length,0);const u=new Buffer(tl);let o=0;for(const b of list){u.set(b,o);o+=b.length;}return u;}
    static compare(a,b){for(let i=0;i<Math.min(a.length,b.length);i++){if(a[i]<b[i])return -1;if(a[i]>b[i])return 1;}return a.length-b.length;}
    toString(enc='utf8'){
        if(enc==='hex') return Array.from(this).map(b=>b.toString(16).padStart(2,'0')).join('');
        if(enc==='base64'){let s='';for(const b of this)s+=String.fromCharCode(b);return btoa(s);}
        return new TextDecoder('utf-8').decode(this);
    }
    equals(o){return Buffer.compare(this,o)===0;}
    slice(s,e){return Buffer.from(super.slice(s,e));}
    readUInt8(o=0){return this[o];}
    readUInt16BE(o=0){return(this[o]<<8)|this[o+1];}
    readUInt32BE(o=0){return((this[o]<<24)|(this[o+1]<<16)|(this[o+2]<<8)|this[o+3])>>>0;}
    writeUInt8(v,o=0){this[o]=v&0xff;return o+1;}
    writeUInt16BE(v,o=0){this[o]=(v>>8)&0xff;this[o+1]=v&0xff;return o+2;}
    writeUInt32BE(v,o=0){this[o]=(v>>24)&0xff;this[o+1]=(v>>16)&0xff;this[o+2]=(v>>8)&0xff;this[o+3]=v&0xff;return o+4;}
}
globalThis.Buffer = Buffer;

// ── stream (minimal) ──────────────────────────────────────────────────────────
class Readable extends EventEmitter {
    constructor(o={}){super();this._enc=null;this._buf=[];this._tfn=o.read||null;}
    setEncoding(e){this._enc=e;return this;}
    read(){return this._buf.shift()||null;}
    push(c){if(c===null)this.emit('end');else{this._buf.push(c);this.emit('data',c);}}
    pipe(d){this.on('data',c=>d.write(c));this.on('end',()=>d.end());return d;}
    resume(){return this;} pause(){return this;}
    destroy(e){if(e)this.emit('error',e);this.emit('close');return this;}
}
class Writable extends EventEmitter {
    constructor(o={}){super();this._wfn=o.write||null;}
    write(c,enc,cb){if(typeof enc==='function'){cb=enc;}if(this._wfn)this._wfn(c,enc,cb||(()=>{}));else if(cb)cb();return true;}
    end(c,enc,cb){if(c)this.write(c,enc);this.emit('finish');this.emit('close');if(cb)cb();return this;}
    destroy(e){if(e)this.emit('error',e);this.emit('close');return this;}
}
class Transform extends EventEmitter {
    constructor(o={}){super();this._tfn=o.transform||null;this._flushfn=o.flush||null;}
    write(c,enc,cb){if(typeof enc==='function')cb=enc;if(this._tfn)this._tfn(c,enc,(e,d)=>{if(!e&&d!=null)this.emit('data',d);if(cb)cb();});else{this.emit('data',c);if(cb)cb();}return true;}
    end(c){if(c)this.write(c);if(this._flushfn)this._flushfn((e,d)=>{if(d!=null)this.emit('data',d);this.emit('end');this.emit('finish');});else{this.emit('end');this.emit('finish');}return this;}
    read(){return null;}
    pipe(d){this.on('data',c=>d.write(c));this.on('end',()=>d.end());return d;}
    push(c){this.emit('data',c);}
}
class PassThrough extends Transform { constructor(o){super({...o,transform:(c,e,cb)=>cb(null,c)});} }
// In Node.js, require('stream') returns the Stream base class itself (a function),
// with Readable/Writable/etc. as properties on it.
function Stream(opts){Readable.call(this,opts);}
Stream.prototype=Object.create(Readable.prototype,{constructor:{value:Stream}});
Stream.Readable=Readable;Stream.Writable=Writable;Stream.Transform=Transform;
Stream.PassThrough=PassThrough;Stream.Stream=Stream;
const _stream=Stream;

// ── os module ─────────────────────────────────────────────────────────────────
const _os={
    hostname:()=>__os_hostname(), homedir:()=>__os_homedir(), tmpdir:()=>__os_tmpdir(),
    uptime:()=>__os_uptime(), freemem:()=>__os_freemem(), totalmem:()=>__os_totalmem(),
    cpus:()=>__os_cpus(), arch:()=>__os_arch(), platform:()=>__os_platform(),
    networkInterfaces:()=>__os_network_interfaces(),
    EOL:'\n', devNull:'/dev/null',
    type(){ const p=__os_platform(); return p==='darwin'?'Darwin':p==='linux'?'Linux':'Unknown'; },
    release(){ return ''; }, loadavg(){return[0,0,0];}, endianness(){return'LE';}
};

// ── crypto module ─────────────────────────────────────────────────────────────
const _crypto={
    createHash(algo){
        const chunks=[];
        return{update(d){chunks.push(typeof d==='string'?d:Buffer.from(d).toString());return this;},
               digest(enc='hex'){return __crypto_hash(algo.toLowerCase(),chunks.join(''),enc);}};
    },
    createHmac(algo,key){
        const chunks=[];
        return{update(d){chunks.push(typeof d==='string'?d:Buffer.from(d).toString());return this;},
               digest(enc='hex'){return __crypto_hmac(algo.toLowerCase(),typeof key==='string'?key:Buffer.from(key).toString(),chunks.join(''),enc);}};
    },
    randomBytes(n){ const h=__crypto_random_bytes(n); return Buffer.from(h,'hex'); },
    randomUUID:()=>__crypto_random_uuid(),
    randomInt(min,max){ if(max===undefined){max=min;min=0;} return min+Math.floor(Math.random()*(max-min)); },
    getHashes:()=>['md5','sha1','sha256','sha512'],
    createCipheriv(){throw new Error('Not implemented');},
    createDecipheriv(){throw new Error('Not implemented');},
};

// ── net module ────────────────────────────────────────────────────────────────
const _net={
    createConnection(opts,onConnect){
        let port,host;
        if(typeof opts==='object'){port=opts.port;host=opts.host||'127.0.0.1';}
        else{port=opts;host='127.0.0.1';}
        const sock=new EventEmitter();
        let fd=-1,connected=false,destroyed=false;
        sock.write=(d)=>{ if(!connected||destroyed)return false; return __net_write(fd,typeof d==='string'?d:Buffer.from(d).toString()); };
        sock.end=(d)=>{ if(d)sock.write(d); if(fd>=0){__net_close(fd);fd=-1;} destroyed=true; sock.emit('close'); };
        sock.destroy=()=>sock.end();
        sock.setEncoding=()=>sock; sock.setNoDelay=()=>sock; sock.setKeepAlive=()=>sock;
        sock.remoteAddress=host; sock.remotePort=port;
        if(onConnect) sock.once('connect',onConnect);
        setTimeout(()=>{
            try{
                fd=__net_connect(port,host);
                if(fd<0){sock.emit('error',new Error(`Connection refused: ${host}:${port}`));return;}
                connected=true; sock.emit('connect');
                __setReadHandler(fd,()=>{
                    if(destroyed)return;
                    const c=__net_read(fd,65536);
                    if(c===null){__setReadHandler(fd,null);connected=false;sock.emit('end');sock.emit('close');return;}
                    sock.emit('data',c);
                });
            }catch(e){sock.emit('error',e);}
        },0);
        return sock;
    },
    connect(...a){return _net.createConnection(...a);},
    createServer(opts,onConn){
        if(typeof opts==='function'){onConn=opts;}
        const srv=new EventEmitter();
        let sfd=-1;
        srv.listen=(port,host,cb)=>{
            if(typeof host==='function'){cb=host;host='0.0.0.0';}
            host=host||'0.0.0.0';
            sfd=__http_create_server_socket(port,host);
            __setReadHandler(sfd,()=>{
                const info=__http_accept(sfd);
                if(!info||info.fd<0)return;
                const sock=new EventEmitter();
                let _d=false;
                sock.write=(d)=>__net_write(info.fd,typeof d==='string'?d:Buffer.from(d).toString());
                sock.end=(d)=>{if(d)sock.write(d);__net_close(info.fd);_d=true;sock.emit('close');};
                sock.destroy=()=>sock.end();
                sock.remoteAddress=info.remoteAddr;
                __setReadHandler(info.fd,()=>{
                    if(_d)return;
                    const c=__net_read(info.fd,65536);
                    if(c===null){__setReadHandler(info.fd,null);sock.emit('end');sock.emit('close');return;}
                    sock.emit('data',c);
                });
                if(onConn)onConn(sock);
                srv.emit('connection',sock);
            });
            if(cb)cb();srv.emit('listening');return srv;
        };
        srv.close=(cb)=>{if(sfd>=0){__setReadHandler(sfd,null);__http_close(sfd);sfd=-1;}if(cb)cb();return srv;};
        srv.address=()=>({port:0,family:'IPv4',address:'0.0.0.0'});
        return srv;
    },
    isIP(s){if(/^(\d{1,3}\.){3}\d{1,3}$/.test(s))return 4;if(/^[0-9a-fA-F:]+$/.test(s)&&s.includes(':'))return 6;return 0;},
    isIPv4(s){return _net.isIP(s)===4;}, isIPv6(s){return _net.isIP(s)===6;},
};

// ── timers module ─────────────────────────────────────────────────────────────
const _timers={setTimeout,clearTimeout,setInterval,clearInterval,
    setImmediate:(fn,...a)=>setTimeout(()=>fn(...a),0),clearImmediate:clearTimeout};

// ── string_decoder ────────────────────────────────────────────────────────────
class StringDecoder{constructor(e='utf8'){this.encoding=e;}write(b){return typeof b==='string'?b:Buffer.from(b).toString(this.encoding);}end(b){return b?this.write(b):'';}}

// ── register all extra modules ────────────────────────────────────────────────
const _extra={
    'events':        {EventEmitter,default:EventEmitter},
    'util':          _util,
    'assert':        assert,
    'querystring':   _qs,
    'url':           {URL,URLSearchParams,parse(u){try{return new URL(u);}catch(e){return null;}},format(u){return u.href||String(u);}},
    'buffer':        {Buffer, constants: {MAX_LENGTH: 2147483647, MAX_STRING_LENGTH: 536870888}, kMaxLength: 2147483647},
    'os':            _os,
    'crypto':        _crypto,
    'net':           _net,
    'timers':        _timers,
    'string_decoder':{StringDecoder},
    'stream':        _stream,
    'tty':           {isatty:()=>false,ReadStream:Readable,WriteStream:Writable},
    // safer-buffer: shim to avoid "class constructors must be invoked with new"
    // The real safer-buffer calls Buffer() without new; we expose a safe wrapper.
    'safer-buffer':  (function(){
        const B = Buffer;
        return { Buffer: B, default: B };
    })(),
    'zlib':          {createGzip:()=>new Transform(),createGunzip:()=>new Transform(),gzipSync:d=>d,gunzipSync:d=>d},
    'dns':           {lookup:(h,cb)=>setTimeout(()=>cb(null,h,4),0),resolve:(h,cb)=>setTimeout(()=>cb(null,[h]),0)},
    'readline':      {createInterface(o){const r=new EventEmitter();r.question=(q,cb)=>{process.stdout.write(q);cb('');};r.close=()=>r.emit('close');return r;}},
    'perf_hooks':    {performance:{now:()=>performance.now(),mark:()=>{},measure:()=>{}}},
    'worker_threads':{isMainThread:true,threadId:0,workerData:null},
    'vm': (() => {
        class Script {
            constructor(code, _options) { this._code = code; }
            runInThisContext() { return (0, eval)(this._code); }
            runInNewContext(sandbox) { return _vm.runInNewContext(this._code, sandbox); }
            runInContext(ctx) { return _vm.runInNewContext(this._code, ctx); }
        }
        const _vm = {
            Script,
            createContext(sandbox) { return sandbox || Object.create(null); },
            isContext(val) { return typeof val === 'object' && val !== null; },
            runInThisContext(code) { return (0, eval)(code); },
            runInNewContext(code, sandbox) {
                const keys = Object.keys(sandbox || {});
                const vals = keys.map(k => sandbox[k]);
                return new Function(...keys, code)(...vals);
            },
            runInContext(code, ctx) { return _vm.runInNewContext(code, ctx); },
        };
        return _vm;
    })(),
};

// Merge _extra into the shared registry so _makeRequire (used inside npm packages)
// can also find built-ins like 'events', 'util', 'stream', etc.
const _reg = globalThis.__qjs_registry;
for (const k of Object.keys(_extra)) { _reg[k] = _extra[k]; }

const _origRequire=globalThis.require;
globalThis.require=function require(id){
    if(id.startsWith('node:')) id=id.slice(5);
    try{return _origRequire(id);}catch(e){
        // Only fall through to _extra for "not found" errors; re-throw load errors
        if(!e.message||!e.message.startsWith("Cannot find module")) throw e;
    }
    if(_extra[id]) return _extra[id];
    throw new Error(`Cannot find module '${id}'`);
};
globalThis.require.resolve=(id)=>id;
globalThis.EventEmitter=EventEmitter;
globalThis.Buffer=Buffer;
globalThis.URL=URL;
globalThis.URLSearchParams=URLSearchParams;

})();
)js";

    JSValue v2 = JS_Eval(ctx, modules_shim, strlen(modules_shim),
                         "<modules>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(v2)) dump_error(ctx);
    JS_FreeValue(ctx, v2);
}

// ── main ──────────────────────────────────────────────────────────────────────

int main(int argc, char **argv) {
    JSRuntime *rt = JS_NewRuntime();
    JS_SetMaxStackSize(rt, 64 * 1024 * 1024);   // 64 MB stack
    JS_SetMemoryLimit(rt, 512 * 1024 * 1024);    // 512 MB heap

    JSContext *ctx = JS_NewContext(rt);

    // Initialize the runtime thread state (needed by js_std_loop / timers)
    js_std_init_handlers(rt);

    // Install module loader so `import 'os'` / `import 'std'` work
    // js_module_loader has the 4-arg signature (ctx, name, opaque, attrs) = JSModuleLoaderFunc2
    JS_SetModuleLoaderFunc2(rt, NULL, js_module_loader, NULL, NULL);

    // Install quickjs standard library (Promise, timers, OS helpers)
    js_std_add_helpers(ctx, -1, NULL);
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

    // Install our custom APIs
    JSValue global = JS_GetGlobalObject(ctx);
    setup_globals(ctx, global, argc, argv);
    JS_FreeValue(ctx, global);

    int exit_code = 0;

    if (argc > 1 && argv[1][0] != '-') {
        // Run script file
        const char *filename = argv[1];

        // Set __filename and __dirname before running
        JSValue g = JS_GetGlobalObject(ctx);
        JS_SetPropertyStr(ctx, g, "__filename", JS_NewString(ctx, filename));

        // dirname
        std::string fn(filename);
        std::string dir = fn.substr(0, fn.find_last_of("/\\"));
        if (dir == fn) dir = ".";
        JS_SetPropertyStr(ctx, g, "__dirname", JS_NewString(ctx, dir.c_str()));
        JS_FreeValue(ctx, g);

        exit_code = run_file(ctx, filename);
    } else {
        // REPL
        run_repl(ctx);
    }

    JS_FreeContext(ctx);
    js_std_free_handlers(rt);
    JS_FreeRuntime(rt);
    return exit_code;
}
