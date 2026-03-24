const http = require('http');

const PORT = 3000;

const HTML = `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>QJS Runtime</title>
  <style>
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background: #0f0f13;
      color: #e2e8f0;
      min-height: 100vh;
    }

    /* ── hero ── */
    .hero {
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      padding: 80px 24px 60px;
      text-align: center;
      background: radial-gradient(ellipse 80% 50% at 50% -10%, #7c3aed33, transparent);
    }

    .badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 4px 14px;
      border-radius: 999px;
      background: #7c3aed22;
      border: 1px solid #7c3aed55;
      color: #a78bfa;
      font-size: 12px;
      font-weight: 600;
      letter-spacing: .06em;
      text-transform: uppercase;
      margin-bottom: 28px;
    }

    .hero h1 {
      font-size: clamp(2.4rem, 6vw, 4.5rem);
      font-weight: 800;
      letter-spacing: -.03em;
      line-height: 1.1;
      background: linear-gradient(135deg, #f8fafc 0%, #a78bfa 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
      margin-bottom: 20px;
    }

    .hero p {
      max-width: 520px;
      color: #94a3b8;
      font-size: 1.1rem;
      line-height: 1.7;
      margin-bottom: 40px;
    }

    .btn-row { display: flex; gap: 12px; flex-wrap: wrap; justify-content: center; }

    .btn {
      padding: 12px 28px;
      border-radius: 10px;
      font-size: .95rem;
      font-weight: 600;
      cursor: pointer;
      border: none;
      text-decoration: none;
      transition: transform .15s, box-shadow .15s;
    }
    .btn:hover { transform: translateY(-2px); }
    .btn-primary {
      background: #7c3aed;
      color: #fff;
      box-shadow: 0 4px 24px #7c3aed66;
    }
    .btn-primary:hover { box-shadow: 0 8px 32px #7c3aed88; }
    .btn-ghost {
      background: transparent;
      color: #cbd5e1;
      border: 1px solid #334155;
    }
    .btn-ghost:hover { border-color: #7c3aed; color: #a78bfa; }

    /* ── stats bar ── */
    .stats {
      display: flex;
      justify-content: center;
      gap: 0;
      border-top: 1px solid #1e293b;
      border-bottom: 1px solid #1e293b;
      background: #0d1117;
    }
    .stat {
      flex: 1;
      max-width: 200px;
      padding: 24px 16px;
      text-align: center;
      border-right: 1px solid #1e293b;
    }
    .stat:last-child { border-right: none; }
    .stat-num {
      font-size: 1.8rem;
      font-weight: 800;
      color: #a78bfa;
      line-height: 1;
      margin-bottom: 6px;
    }
    .stat-label { font-size: .78rem; color: #64748b; text-transform: uppercase; letter-spacing: .08em; }

    /* ── features grid ── */
    .section {
      max-width: 1100px;
      margin: 0 auto;
      padding: 72px 24px;
    }
    .section-title {
      text-align: center;
      font-size: 1.8rem;
      font-weight: 700;
      margin-bottom: 12px;
      letter-spacing: -.02em;
    }
    .section-sub {
      text-align: center;
      color: #64748b;
      margin-bottom: 48px;
      font-size: .95rem;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
      gap: 20px;
    }

    .card {
      background: #111827;
      border: 1px solid #1e293b;
      border-radius: 16px;
      padding: 28px;
      transition: border-color .2s, transform .2s;
    }
    .card:hover { border-color: #7c3aed55; transform: translateY(-3px); }

    .card-icon {
      width: 44px;
      height: 44px;
      border-radius: 10px;
      background: #7c3aed22;
      display: flex;
      align-items: center;
      justify-content: center;
      font-size: 1.3rem;
      margin-bottom: 16px;
    }
    .card h3 { font-size: 1rem; font-weight: 700; margin-bottom: 8px; }
    .card p  { font-size: .88rem; color: #64748b; line-height: 1.6; }

    /* ── terminal demo ── */
    .terminal-wrap {
      background: #0d1117;
      border-top: 1px solid #1e293b;
      border-bottom: 1px solid #1e293b;
      padding: 72px 24px;
    }
    .terminal {
      max-width: 720px;
      margin: 0 auto;
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 12px;
      overflow: hidden;
      box-shadow: 0 24px 64px #00000066;
    }
    .terminal-bar {
      display: flex;
      align-items: center;
      gap: 6px;
      padding: 12px 16px;
      background: #21262d;
      border-bottom: 1px solid #30363d;
    }
    .dot { width: 12px; height: 12px; border-radius: 50%; }
    .dot-red   { background: #ff5f57; }
    .dot-yellow{ background: #ffbd2e; }
    .dot-green { background: #28c840; }
    .terminal-title { margin-left: 8px; font-size: .8rem; color: #8b949e; }
    .terminal-body {
      padding: 24px;
      font-family: 'SF Mono', 'Fira Code', monospace;
      font-size: .85rem;
      line-height: 1.8;
    }
    .line-prompt { color: #7c3aed; }
    .line-cmd    { color: #e2e8f0; }
    .line-out    { color: #3fb950; }
    .line-comment{ color: #484f58; }

    /* ── api demo ── */
    .api-section { max-width: 1100px; margin: 0 auto; padding: 0 24px 72px; }
    .api-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
    @media (max-width: 640px) { .api-grid { grid-template-columns: 1fr; } }

    .api-card {
      background: #111827;
      border: 1px solid #1e293b;
      border-radius: 12px;
      overflow: hidden;
    }
    .api-header {
      display: flex;
      align-items: center;
      gap: 10px;
      padding: 14px 18px;
      border-bottom: 1px solid #1e293b;
    }
    .method {
      font-size: .72rem;
      font-weight: 700;
      padding: 3px 8px;
      border-radius: 5px;
      letter-spacing: .05em;
    }
    .get  { background: #05966922; color: #34d399; border: 1px solid #05966944; }
    .post { background: #2563eb22; color: #60a5fa; border: 1px solid #2563eb44; }
    .api-path { font-family: 'SF Mono', monospace; font-size: .85rem; color: #cbd5e1; }
    .api-body { padding: 16px 18px; font-family: 'SF Mono', monospace; font-size: .8rem; color: #64748b; line-height: 1.6; }
    .api-body a { color: #a78bfa; text-decoration: none; }
    .api-body a:hover { text-decoration: underline; }

    /* ── live ping ── */
    #ping-result {
      margin-top: 12px;
      padding: 12px 16px;
      border-radius: 8px;
      background: #0d1117;
      border: 1px solid #1e293b;
      font-family: monospace;
      font-size: .85rem;
      color: #3fb950;
      min-height: 44px;
      transition: opacity .2s;
    }

    /* ── footer ── */
    footer {
      border-top: 1px solid #1e293b;
      padding: 32px 24px;
      text-align: center;
      color: #334155;
      font-size: .85rem;
    }
    footer span { color: #7c3aed; }
  </style>
</head>
<body>

<!-- hero -->
<section class="hero">
  <div class="badge">⚡ Powered by QuickJS</div>
  <h1>QJS Runtime</h1>
  <p>A lightweight JavaScript runtime built on <strong style="color:#a78bfa">QuickJS</strong>.
     Fast, embeddable, zero dependencies — 965 KB binary.</p>
  <div class="btn-row">
    <a href="/api/ping" class="btn btn-primary">Try API →</a>
    <a href="/api/info" class="btn btn-ghost">Runtime Info</a>
  </div>
</section>

<!-- stats -->
<div class="stats">
  <div class="stat"><div class="stat-num">965K</div><div class="stat-label">Binary Size</div></div>
  <div class="stat"><div class="stat-num">ES2020</div><div class="stat-label">JS Version</div></div>
  <div class="stat"><div class="stat-num">0</div><div class="stat-label">Dependencies</div></div>
  <div class="stat"><div class="stat-num">64K</div><div class="stat-label">Engine LoC</div></div>
</div>

<!-- features -->
<section class="section">
  <h2 class="section-title">Everything you need</h2>
  <p class="section-sub">A full runtime built on top of Bellard's QuickJS engine.</p>
  <div class="grid">
    <div class="card">
      <div class="card-icon">⚙️</div>
      <h3>QuickJS Engine</h3>
      <p>Full ES2020 — async/await, generators, BigInt, optional chaining. Bytecode VM with GC.</p>
    </div>
    <div class="card">
      <div class="card-icon">🌐</div>
      <h3>HTTP Server</h3>
      <p>Non-blocking TCP via POSIX sockets integrated with QuickJS's event loop via <code>os.setReadHandler</code>.</p>
    </div>
    <div class="card">
      <div class="card-icon">📁</div>
      <h3>File System</h3>
      <p>readFileSync, writeFileSync, existsSync, statSync, mkdirSync, readdirSync and more.</p>
    </div>
    <div class="card">
      <div class="card-icon">🔄</div>
      <h3>Event Loop</h3>
      <p>Promises drain before timers. Single-threaded, non-blocking — same model as Node.js.</p>
    </div>
    <div class="card">
      <div class="card-icon">📦</div>
      <h3>CommonJS Modules</h3>
      <p>require('fs'), require('path'), require('http') — familiar module system out of the box.</p>
    </div>
    <div class="card">
      <div class="card-icon">🖥️</div>
      <h3>REPL</h3>
      <p>Interactive shell with expression printing. Run <code>qjs</code> with no arguments to start.</p>
    </div>
  </div>
</section>

<!-- terminal -->
<div class="terminal-wrap">
  <div class="terminal">
    <div class="terminal-bar">
      <div class="dot dot-red"></div>
      <div class="dot dot-yellow"></div>
      <div class="dot dot-green"></div>
      <span class="terminal-title">zsh — qjs</span>
    </div>
    <div class="terminal-body">
      <div><span class="line-prompt">$ </span><span class="line-cmd">qjs examples/hello.js</span></div>
      <div><span class="line-out">Hello, World!</span></div>
      <div><span class="line-out">Welcome to QJS Runtime!</span></div>
      <br>
      <div><span class="line-prompt">$ </span><span class="line-cmd">qjs examples/webpage.js</span></div>
      <div><span class="line-out">Server running at http://localhost:3000/</span></div>
      <div><span class="line-out">GET /</span></div>
      <div><span class="line-out">GET /api/ping</span></div>
      <br>
      <div><span class="line-prompt">$ </span><span class="line-cmd">qjs</span></div>
      <div><span class="line-comment"># REPL mode</span></div>
      <div><span class="line-prompt">qjs[1]&gt; </span><span class="line-cmd">[1,2,3].map(x => x ** 2)</span></div>
      <div><span class="line-out">&lt;= [ 1, 4, 9 ]</span></div>
    </div>
  </div>
</div>

<!-- api endpoints -->
<section class="api-section">
  <h2 class="section-title" style="padding-top:72px; margin-bottom:8px">Live API</h2>
  <p class="section-sub">This server is running right now. Click any endpoint.</p>

  <div class="api-grid">
    <div class="api-card">
      <div class="api-header">
        <span class="method get">GET</span>
        <a href="/api/ping" class="api-path">/api/ping</a>
      </div>
      <div class="api-body">Returns <code>{ "pong": true, "uptime": ... }</code></div>
    </div>
    <div class="api-card">
      <div class="api-header">
        <span class="method get">GET</span>
        <a href="/api/info" class="api-path">/api/info</a>
      </div>
      <div class="api-body">Runtime version, platform, memory usage</div>
    </div>
    <div class="api-card">
      <div class="api-header">
        <span class="method get">GET</span>
        <a href="/api/time" class="api-path">/api/time</a>
      </div>
      <div class="api-body">Current timestamp and ISO date string</div>
    </div>
    <div class="api-card">
      <div class="api-header">
        <span class="method post">POST</span>
        <span class="api-path">/api/echo</span>
      </div>
      <div class="api-body">Echoes back method, headers, and body</div>
    </div>
  </div>

  <!-- live ping widget -->
  <div style="max-width:480px; margin: 40px auto 0;">
    <button class="btn btn-primary" style="width:100%" onclick="doPing()">
      ⚡ Ping the server
    </button>
    <div id="ping-result">Click the button to ping...</div>
  </div>
</section>

<footer>
  Built with <span>♥</span> using <span>QJS Runtime</span> &amp; <span>QuickJS</span> by Fabrice Bellard
</footer>

<script>
  const startTime = Date.now();

  async function doPing() {
    const el = document.getElementById('ping-result');
    el.style.opacity = '0.4';
    el.textContent = 'pinging...';
    try {
      const t0 = performance.now();
      const r  = await fetch('/api/ping');
      const ms = (performance.now() - t0).toFixed(1);
      const j  = await r.json();
      el.style.opacity = '1';
      el.textContent = JSON.stringify({ ...j, latency: ms + 'ms' }, null, 2);
    } catch(e) {
      el.style.opacity = '1';
      el.textContent = 'Error: ' + e.message;
    }
  }
</script>
</body>
</html>`;

const startTime = Date.now();

const server = http.createServer((req, res) => {
    console.log(`${req.method} ${req.pathname}`);

    if (req.pathname === '/' && req.method === 'GET') {
        res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
        res.end(HTML);

    } else if (req.pathname === '/api/ping') {
        res.json({ pong: true, uptime: ((Date.now() - startTime) / 1000).toFixed(2) + 's' });

    } else if (req.pathname === '/api/info') {
        res.json({
            runtime: 'QJS Runtime',
            engine:  'QuickJS',
            version: typeof __runtime_version !== 'undefined' ? __runtime_version : '2025-09-13',
            platform: process.platform,
            arch:    'arm64',
            pid:     0,
        });

    } else if (req.pathname === '/api/time') {
        const now = new Date();
        res.json({ timestamp: Date.now(), iso: now.toISOString() });

    } else if (req.pathname === '/api/echo' && req.method === 'POST') {
        res.json({ method: req.method, headers: req.headers, body: req.body });

    } else {
        res.writeHead(404, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Not Found', path: req.pathname }));
    }
});

server.listen(PORT, () => {
    console.log(`\nQJS Runtime — example web page`);
    console.log(`Open http://localhost:${PORT}/ in your browser\n`);
});
